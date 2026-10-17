/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "../common/cmdlib.h"
#include "../common/mathlib.h"
#include "../common/scriplib.h"
#include "../common/polylib.h"
#include "../common/imagelib.h"
#include "../common/threads.h"
#include "../common/bspfile.h"
#include "shaders.h"
#include "mesh.h"


#define	MAX_PATCH_SIZE	32

#define	CLIP_EPSILON		0.1
#define	PLANENUM_LEAF		-1

#define	HINT_PRIORITY		1000

typedef struct parseMesh_s {
	struct parseMesh_s	*next;
	mesh_t			mesh;
	shaderInfo_t	*shaderInfo;
	qboolean	isTerrain;
	int			contents;
	int			brushnum;
	int			entitynum;
	int			cullgroup;

	qboolean	grouped;			// used during shared edge grouping
	struct parseMesh_s *groupChain;
} parseMesh_t;

typedef struct bspface_s {
	struct bspface_s	*next;
	int					planenum;
	int					priority;	// added to value calculation
	qboolean			checked;
	qboolean			hint;
	winding_t			*w;
} bspface_t;

typedef struct plane_s {
	vec3_t	normal;
	vec_t	dist;
	int		type;
	qboolean	used;
	struct plane_s	*hash_chain;
} plane_t;

typedef struct side_s {
	int			planenum;

	float		texMat[2][3];	// brush primitive texture matrix
	// for old brush coordinates mode
	float		vecs[2][4];		// texture coordinate mapping

	winding_t	*winding;
	winding_t	*visibleHull;	// convex hull of all visible fragments

	struct shaderInfo_s	*shaderInfo;

	int			contents;		// from shaderInfo
	int			surfaceFlags;	// from shaderInfo
	int			value;			// from shaderInfo
	int			lightmapSampleSize;	// per-side override; zero uses shader/default

	qboolean	visible;		// choose visble planes first
	qboolean	bevel;			// don't ever use for bsp splitting, and don't bother
								// making windings for it
	qboolean	backSide;		// generated side for a q3map_backShader

	qboolean	portalIgnored;		// unmatched or same-cell portal side
	int			portalCellCount[2];	// emitted links for each orientation
	struct bspbrush_s *portalBrush;	// owning brush for diagnostics
} side_t;


#define	MAX_BRUSH_SIDES		1024

typedef struct bspbrush_s {
	struct bspbrush_s	*next;

	int			entitynum;			// editor numbering
	int			brushnum;			// editor numbering

	struct shaderInfo_s	*contentShader;

	int			contents;
	qboolean	detail;
	qboolean	opaque;
	qboolean	noBspOutput;		// excluded from the BSP brush lump
	qboolean	isOccluder;
	int			cullgroup;
	int			outputNumber;		// set when the brush is written to the file list

	int			portalareas[2];

	struct bspbrush_s	*original;	// chopped up brushes will reference the originals

	vec3_t		mins, maxs;
	int			numsides;
	side_t		sides[6];			// variably sized
} bspbrush_t;



typedef struct drawsurf_s {
	shaderInfo_t	*shaderInfo;
	int			contents;
	int			lightmapSampleSize;

	bspbrush_t	*mapBrush;
	side_t		*side;
	struct drawsurf_s	*nextOnShader;

	int			fogNum;
	int			lightmapNum;
	int			lightmapX, lightmapY;
	int			lightmapWidth, lightmapHeight;

	int			lightmapGroupNum;
	struct drawsurf_s	*nextOnLightmapGroup;
	qboolean	lightmapGroupVisited;
	vec3_t		mins, maxs;

	int			numVerts;
	drawVert_t	*verts;
	int			numIndexes;
	int			*indexes;

	int			cullgroup;
	int			lightmapPlaneNum;
	vec3_t		lightmapOrigin;
	vec3_t		lightmapVecs[3];

	qboolean	patch;
	qboolean	isTerrain;
	qboolean	planar;
	int			patchWidth;
	int			patchHeight;
	int			patchSubdivisions;

	qboolean	triangleSoup;
	qboolean	miscModel;
	qboolean	flareSurface;
} mapDrawSurface_t;

typedef struct drawSurfRef_s {
	struct drawSurfRef_s	*nextRef;
	int						outputNumber;
} drawSurfRef_t;

typedef struct node_s {
	// both leafs and nodes
	int				planenum;	// -1 = leaf node
	struct node_s	*parent;
	vec3_t			mins, maxs;	// valid after portalization
	bspbrush_t		*volume;	// one for each leaf/node

	// nodes only
	side_t			*side;		// the side that created the node
	struct node_s	*children[2];
	qboolean		hint;
	int				tinyportals;
	vec3_t			referencepoint;
	int				pad;

	// leafs only
	qboolean		opaque;		// view can never be inside
	qboolean		areaportal;
	int				cluster;	// for portalfile writing
	int				area;		// for areaportals
	bspbrush_t		*brushlist;	// fragments of all brushes in this leaf
	drawSurfRef_t	*drawSurfReferences;	// references to patches pushed down
	drawSurfRef_t	*collisionSurfReferences;	// emitted collision surface references

	int				occupied;	// 1 or greater can reach entity
	entity_t		*occupant;	// for leak file testing
	int				bspCell;		// -2 = unassigned, -1 = opaque; InitBspTreeNodes

	struct portal_s	*portals;	// also on nodes during construction
} node_t;

typedef struct portal_s {
	plane_t		plane;
	node_t		*onnode;		// NULL = outside box
	node_t		*nodes[2];		// [0] = front side of plane
	struct portal_s	*next[2];
	winding_t	*winding;

	qboolean	sidefound;		// false if ->side hasn't been checked
	qboolean	hint;
	side_t		*side;			// NULL = non-visible
	bspbrush_t	*brush;			// brush owning the visible side
} portal_t;

typedef struct {
	node_t		*headnode;
	node_t		outside_node;
	vec3_t		mins, maxs;
} tree_t;

extern	int			entity_num;


extern	qboolean	noprune;
extern	qboolean	nodetail;
extern	qboolean	fulldetail;
extern	qboolean	nowater;
extern	qboolean	noCurveBrushes;
extern	qboolean	fakemap;
extern	qboolean	coplanar;
extern	qboolean	nofog;
extern	qboolean	testExpand;
extern	qboolean	showseams;

extern	vec_t		microvolume;

extern	char		outbase[32];
extern	char		source[1024];

extern int		samplesize;			//sample size in units
extern int		novertexlighting;
extern int		nogridlighting;

//=============================================================================

// brush.c

int	CountBrushList (bspbrush_t *brushes);
bspbrush_t *AllocBrush (int numsides);
void FreeBrush (bspbrush_t *brushes);
void FreeBrushList (bspbrush_t *brushes);
bspbrush_t *CopyBrush (bspbrush_t *brush);
void DrawBrushList (bspbrush_t *brush);
void WriteBrushList (char *name, bspbrush_t *brush, qboolean onlyvis);
void PrintBrush (bspbrush_t *brush);
qboolean BoundBrush (bspbrush_t *brush);
qboolean CreateBrushWindings (bspbrush_t *brush);
bspbrush_t	*BrushFromBounds (vec3_t mins, vec3_t maxs);
vec_t BrushVolume (bspbrush_t *brush);
void WriteBspBrushMap (char *name, bspbrush_t *list);

void FilterDetailBrushesIntoTree( entity_t *e, tree_t *tree );
void FilterStructuralBrushesIntoTree( entity_t *e, tree_t *tree );

//=============================================================================

// map.c

extern	int			entitySourceBrushes;

// mapplanes[ num^1 ] will always be the mirror or mapplanes[ num ]
// nummapplanes will always be even
extern	plane_t		mapplanes[MAX_MAP_PLANES];
extern	int			nummapplanes;

extern	vec3_t		map_mins, map_maxs;

extern	char		mapIndexedShaders[MAX_MAP_BRUSHSIDES][MAX_QPATH];
extern	int			numMapIndexedShaders;

extern	entity_t	*mapent;

#define		MAX_BUILD_SIDES		300
extern	bspbrush_t	*buildBrush;


void 		LoadMapFile (char *filename);
int			FindFloatPlane (vec3_t normal, vec_t dist);
int			PlaneTypeForNormal (vec3_t normal);
bspbrush_t	*FinishBrush( void );
mapDrawSurface_t	*AllocDrawSurf( void );
mapDrawSurface_t	*DrawSurfaceForSide( bspbrush_t *b, side_t *s, winding_t *w, qboolean globalTexture );

//=============================================================================

//=============================================================================

// draw.c

extern	vec3_t		draw_mins, draw_maxs;
extern	qboolean	drawflag;

void Draw_ClearWindow (void);
void DrawWinding (winding_t *w);

void GLS_BeginScene (void);
void GLS_Winding (winding_t *w, int code);
void GLS_EndScene (void);

//=============================================================================

// csg

bspbrush_t *MakeBspBrushList ( bspbrush_t *brushes,	vec3_t clipmins, vec3_t clipmaxs);

//=============================================================================

// brushbsp

#define	PSIDE_FRONT			1
#define	PSIDE_BACK			2
#define	PSIDE_BOTH			(PSIDE_FRONT|PSIDE_BACK)
#define	PSIDE_FACING		4

typedef struct cplane_s {
	vec3_t normal;
	float dist;
	byte type;
	byte signbits;
} cplane_t;

void SetPlaneSignbits( cplane_t *plane );
int BoxOnPlaneSide (vec3_t mins, vec3_t maxs, cplane_t *plane);
qboolean WindingIsTiny (winding_t *w);

void SplitBrush (bspbrush_t *brush, int planenum,
	bspbrush_t **front, bspbrush_t **back);

tree_t *AllocTree (void);
node_t *AllocNode (void);

tree_t *BrushBSP (bspbrush_t *brushlist, vec3_t mins, vec3_t maxs);

//=============================================================================

// portals.c

void MakeHeadnodePortals (tree_t *tree);
void MakeNodePortal (node_t *node);
void SplitNodePortals (node_t *node);

qboolean	Portal_Passable(portal_t *p);

qboolean FloodEntities (tree_t *tree);
void FillOutside (node_t *headnode);
void FloodAreas (tree_t *tree);
bspface_t *VisibleFaces(entity_t *e, tree_t *tree);
void FreePortal (portal_t *p);

void MakeTreePortals (tree_t *tree);

//=============================================================================

// glfile.c

void OutputWinding( winding_t *w, FILE *glview );
void WriteGLView( tree_t *tree, char *source );

//=============================================================================

// leakfile.c

void LeakFile( tree_t *tree );

//=============================================================================

// prtfile.c

void NumberClusters( tree_t *tree );
void WritePortalFile( tree_t *tree );

//=============================================================================

// writebsp.c

void SetModelNumbers (void);
void SetLightStyles (void);

int	EmitShader( const char *shader, int contents );
void EmitPlanes( void );
int EmitDrawNode_r( node_t *node );

void BeginBSPFile (void);
void EndBSPFile (void);

void BeginModel (void);
void EndModel( node_t *headnode );


//=============================================================================

// tree.c

void FreeTree (tree_t *tree);
void FreeTree_r (node_t *node);
void PrintTree_r (node_t *node, int depth);
void FreeTreePortals_r (node_t *node);

//=============================================================================

// patch.c

extern	int			numMapPatches;

mapDrawSurface_t	*DrawSurfaceForMesh( mesh_t *m, qboolean isTerrain, int cullgroup );
void ParsePatch( char **data_p, qboolean terrain );
mesh_t *SubdivideMesh( mesh_t in, float maxError, float minLength, int *originalWidths, int *originalHeights );
void PatchMapDrawSurfs( entity_t *e );

//=============================================================================

// lightmap.c

void AllocateLightmaps( entity_t *e );

//=============================================================================

// tjunction.c

void FixTJunctions( entity_t *e );


//=============================================================================

// fog.c

void FogDrawSurfs( void );
winding_t	*WindingFromDrawSurf( mapDrawSurface_t *ds );

//=============================================================================

// facebsp.c

bspface_t	*BspFaceForPortal( portal_t *p );
bspface_t	*MakeStructuralBspFaceList( bspbrush_t *list );
bspface_t	*MakeVisibleBspFaceList( bspbrush_t *list );
tree_t *FaceBSP( bspface_t *list );

//=============================================================================

// misc_model.c

extern	int		c_triangleModels;
extern	int		c_triangleSurfaces;
extern	int		c_triangleVertexes;
extern	int		c_triangleIndexes;

void AddTriangleModels( tree_t *tree );

//=============================================================================

// surface.c

extern	mapDrawSurface_t	mapDrawSurfs[MAX_MAP_DRAW_SURFS];
extern	int			numMapDrawSurfs;

mapDrawSurface_t	*AllocDrawSurf( void );
void	MergeSides( entity_t *e, tree_t *tree );
void	SubdivideDrawSurfs( entity_t *e, tree_t *tree );
void	MakeDrawSurfaces( bspbrush_t *b );
void	ClipSidesIntoTree( entity_t *e, tree_t *tree );
void FilterDrawsurfsIntoTree( entity_t *e, tree_t *tree );

//==============================================================================

// brush_primit.c

#define BPRIMIT_UNDEFINED 0
#define BPRIMIT_OLDBRUSHES 1
#define BPRIMIT_NEWBRUSHES 2
extern	int	g_bBrushPrimit;

void ComputeAxisBase( vec3_t normal, vec3_t texX, vec3_t texY);
