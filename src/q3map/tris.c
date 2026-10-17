/*
===========================================================================
q3map (Call of Duty, 2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

// triangle-soup tessellation and AABB construction
#include "../common/cmdlib.h"
#include "../common/mathlib.h"
#include "../code/qcommon/qfiles.h"
#include "qbsp.h"
#include <float.h>
#include "../universal/com_math.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>


//===========================================================================
// constants and macros
//===========================================================================

#define TRISOUP_AABB_LEAF_LIMIT 16

#define WINDING_INTERSECT_EPSILON 0.001f

#define WINDING_INTERSECT_EPSILON_SQ \
	( WINDING_INTERSECT_EPSILON * WINDING_INTERSECT_EPSILON )

/* Debug switch for the (disabled) loop-counter probe in CollapseCollinearVerts. */
#define TRIS_DEBUG_COLLAPSE 0

#define TRI_SURF_PROPS_HASH_BUCKETS 16384

#define TRIS_COPY4(from, to) \
	{ VectorCopy( (from), (to) ); (to)[3] = (from)[3]; }

#define TRIANGLE_DETERMINANT_EPSILON 0.001f

#define TRI_AABB_LOCAL_ITEMS 32768

#define MAX_NATIVE_TRIANGLE_SOUPS 131072

#define MAX_TRI_SOUP_VERTS 8192

#define MAX_TRI_SOUP_INDICES 49152

#define TRIANGLE_EDGE_HASH_SIZE 4096

#define MAX_TRIANGLE_EDGES 0x80000

/* Geometric and texture continuity between adjacent triangle records. */
#define TRIANGLE_PLANE_EPSILON_SQ ( 0.001f * 0.001f )

#define TRIS_MIN(a,b) ((a) < (b) ? (a) : (b))

#define TRIS_MAX(a,b) ((a) > (b) ? (a) : (b))

#define TRIS_COLLAPSE_COLOR(row, point) \
	RoundToInt( DotProduct( row, point ) + (row)[3] )

#define TRIS_DISCOVERY_CORNER(verts, index, winding, corner) \
    VectorCopy( (verts)[index].xyz, (winding)->p[corner] ); \
    st[corner][0] = (verts)[index].st[0]; \
    st[corner][1] = (verts)[index].st[1]; \
    lightmap[corner][0] = (verts)[index].lightmap[0]; \
    lightmap[corner][1] = (verts)[index].lightmap[1]; \
    colors[corner][0] = (verts)[index].color[0]; \
    colors[corner][1] = (verts)[index].color[1]; \
    colors[corner][2] = (verts)[index].color[2]; \
    colors[corner][3] = (verts)[index].color[3]

#define MAX_TJUNC_POINTS	0x40000

#define MAX_TJUNC_EDGE_LINES	0x10000

#define MAX_POINTS_ON_CONCAVE_WINDING	0x4000

#define TJUNC_HASH_AXIS_SIZE	256

#define TJUNC_HASH_AXES	3

#define TJUNC_OCTREE_MIN_EXTENT 8.0f

#define TJUNC_OCTREE_MAX_SURFACES 256

#define TJUNC_SKIP_SURFACE_FLAGS ( SURF_SKY | SURF_NODRAW )

#define TJUNC_BOUNDS_INTERSECT( minsA, maxsA, minsB, maxsB ) \
	( !((minsA)[0] > (maxsB)[0]) && \
	  !((minsA)[1] > (maxsB)[1]) && \
	  !((minsA)[2] > (maxsB)[2]) && \
	  !((maxsA)[0] < (minsB)[0]) && \
	  !((maxsA)[1] < (minsB)[1]) && \
	  !((maxsA)[2] < (minsB)[2]) )

#define TRIS_MERGECONCAVE_COLLINEAR_VERTEX_EPSILON tjuncTolerance

#define MAX_THIN_WINDING_POINTS 16384

#define MAX_WINDINGS_PER_CONCAVE_GROUP 16384

#define MERGE_BOUNDS_EPSILON 0.001f

/* The cell merger owns its winding repair and shader-dependent geometry. */
#define MAX_WINDINGS_PER_CELL 0x20000

#define MAX_WINDINGS_PER_GROUP 0x2000


//===========================================================================
// records
//===========================================================================

/* the shader record used by the triangle code */
typedef shaderInfo_t triShaderInfo_t;

typedef struct triSurfProps_s {
	triShaderInfo_t *shaderInfo;
	int lightStyle;
	float smoothAngle;
	vec4_t plane;
	float texVecs[2][4];
	float lmapVecs[2][4];
	float colorVecs[4][4];
	struct triSurfProps_s *hashNext;
} triSurfProps_t;

typedef struct triSurf_s {
	winding_t *winding;
	winding_t *originalWinding;
	triSurfProps_t *props;
	vec3_t mins;
	vec3_t maxs;
	qboolean marked;
	struct triSurf_s *next;
} triSurf_t;

typedef struct tessSplitPoint_s {
	int edgeIdx;
	float position;
	int vertIdx;
} tessSplitPoint_t;


//===========================================================================
// declarations
//===========================================================================


typedef DiskGfxAabbTree triSoupAabbNode_t;


extern triSoupAabbNode_t daabbtrees[];

extern int numaabbtreenodes;

static qboolean AabbPartitionItems( int itemCount, vec3_t *itemMins,
	vec3_t *itemMaxs, int *indices, int *frontCount, int *backStart );

extern int PointsNearLine( float *pointA, float *pointB, float *lineEnd,
	float tolerance );


extern vec_t VectorDistanceSquared( const vec3_t pointA,
	const vec3_t pointB );


extern shaderInfo_t shaderInfo[];

extern int VectorCompareEpsilon( const vec3_t a, const vec3_t b );

extern void VectorNormalizeFast( vec3_t v );

typedef struct triGroupRecord_s {
	int drawOrder;
	int cullGroupIdx;
	triShaderInfo_t *shaderInfo;
	int lightStyle;
	int vertIdx[3];
	int edgeIdx[3];
	qboolean removed;
	float texVecs[2][4];
	float lmapVecs[2][4];
	float colorVecs[4][4];
	int groupId;
} triGroupRecord_t;


typedef cod_bsp_tri_soup_t triSoup_t;

typedef DiskGfxCell triCell_t;

typedef DiskGfxCullGroup triCullGroup_t;


extern triSoup_t triangleSurfaces[];

extern drawVert_t triangleSoupData[];

extern float *g_aabbSortMin;

extern float *g_aabbSortMax;

extern float *g_aabbSortEqual;


extern unsigned char cellCullGroupBits[1048576];


extern triCullGroup_t cullGroups[];

extern triCell_t dcells[];

extern int numTriangleSurfaces;

typedef struct triSubgroup_s {
	int firstTri, triCount;
	vec3_t mins, maxs;
} triSubgroup_t;

typedef struct triIntermediateVert_s {
	vec3_t xyz;
	vec3_t originalPos;
	float st[2];
	float lightmap[2];
	vec3_t normal;
	byte color[4];
	vec3_t smoothNormal;
	float smoothAngle;
	int hashNext;
} triIntermediateVert_t;


extern int numDrawVerts;


extern unsigned short drawIndexes[];

extern int numDrawIndexes;

extern int RoundToInt( float value );


extern int numCells;

extern int numCullGroups;

typedef struct triangleEdge_s {
	int		vertex[2];
	int		triangle[2];
	float	lengthSquared;
	byte	compatible;
	byte	nonmanifold;
	byte	removed;
	int		hashNext;
} triangleEdge_t;


extern float VectorNormalize2( const vec3_t vector, vec3_t result );





static qboolean CollectCollapseTriangles( int representativeVertex,
	int destinationVertex, int edgeIndex, int firstTriangle,
	int secondTriangle, int *affectedTriangles, int *affectedCount,
	int *canTrySecondDirection );

extern void TesselateWinding( triSurf_t *surface, int cellIndex,
	int cullGroupIndex );



extern float brushSmoothAngle, curveSmoothAngle, terrainSmoothAngle;


extern void EmitCellPortalsAndOccluders( entity_t *entity, node_t *headnode );




extern float VectorNormalize2D( float v[2] );

typedef struct tjuncPoint_s {
	float			intercept;
	vec3_t			xyz;
	struct tjuncPoint_s	*prev;
	struct tjuncPoint_s	*next;
} tjuncPoint_t;

typedef struct tjuncEdgeLine_s {
	vec3_t			normal1;
	float			dist1;
	vec3_t			normal2;
	float			dist2;
	vec3_t			direction;
	vec3_t			origin;
	tjuncPoint_t		sentinel;
} tjuncEdgeLine_t;

typedef struct tjuncHashNode_s {
	tjuncEdgeLine_t		*edgeLine;
	struct tjuncHashNode_s	*next;
} tjuncHashNode_t;


/* Fixed edge-line, point and spatial-hash storage for one T-junction pass. */


/* Each hash slot is a dummy node; only its next link is used as the head. */

float tjuncTolerance = 0.25f;
float brushSmoothAngle = 1.0f;
float curveSmoothAngle = 0.01f;
float terrainSmoothAngle = 0.01f;

typedef struct tjuncCellBounds_s {
	vec3_t mins;
	vec3_t maxs;
} tjuncCellBounds_t;


/* Parallel cell bounds and triangle-surface head arrays. */



extern void ExpandBounds( const vec3_t mins, const vec3_t maxs,
	vec3_t outMins, vec3_t outMaxs );




//===========================================================================
// storage
//===========================================================================
// the explicit zero initialisers keep these in definition order
static triSurf_t *triSurfCellHeads[MAX_MAP_CELL_LISTS] = {0};
static int numTriIntermediateVerts = 0;
static int c_thinWindingInputPoints = 0;
static int triangleVertexHash[16384] = {0};
static tree_t *trisCurrentTree = 0;
static int numTriangles = 0;
static int numSimplifiedTriangles = 0;
static triSurfProps_t *triSurfPropsHash[TRI_SURF_PROPS_HASH_BUCKETS] = {0};
static int triangleEdgeHash[TRIANGLE_EDGE_HASH_SIZE] = {0};
static int c_addedThinWindingPoints = 0;
static tjuncHashNode_t tjuncHash[TJUNC_HASH_AXES][TJUNC_HASH_AXIS_SIZE][TJUNC_HASH_AXIS_SIZE] = {0};
static tjuncPoint_t tjuncPoints[MAX_TJUNC_POINTS] = {0};
static int triangleVertexRemap[0x180000] = {0};
static int numTriSplitPoints = 0;
static int numTJuncEdgeLines = 0;
static int numTextureDirectionMismatches = 0;
static triGroupRecord_t triRecords[0x80000] = {0};
static int numTJuncPoints = 0;
static tjuncEdgeLine_t tjuncEdgeLines[MAX_TJUNC_EDGE_LINES] = {0};
static triIntermediateVert_t triIntermediateVerts[0x180000] = {0};
static int numDegenerateTriEdges = 0;
static int numOriginalDrawVerts = 0;
static int numTriangleEdges = 0;
static triangleEdge_t triangleEdges[MAX_MAP_TRIANGLE_EDGES] = {0};
static tjuncCellBounds_t tjuncCellBounds[MAX_MAP_CELL_LISTS] = {0};

void VectorNormalizeFast( vec3_t v );


//===========================================================================
// forward declarations
//===========================================================================

static void GetProjectionAxes( const vec3_t normal, int *axisU, int *axisV );

static int TrisGetCellListCount( void );

static void PrependTriSurfToCell( winding_t *winding,
	triSurfProps_t *props, int cell );

void FilterTriSurfIntoTree( winding_t *winding, triSurfProps_t *props,
	node_t *node );

int TessSplitPointCompare( tessSplitPoint_t *splitA, tessSplitPoint_t *splitB );

static void TrisInsertSplitPoints( winding_t **winding, const vec3_t normal, winding_t **originalWinding );

void TrisRemoveDegenerateEdges( winding_t *winding,
    winding_t *originalWinding );

void TesselateRemoveVertex( winding_t *winding, winding_t *originalWinding,
	int previous, int vertex, int next, const vec3_t normal );

static qboolean TriSideCheck( const double side0[3], const double side1[3],
							 const double side2[3], float x, float y );

static qboolean TrisCheckBothEdges( const double side0[3], const double side1[3],
							   const double side2[3], float ax, float ay,
							   float bx, float by );

static qboolean TrisVerticesCoincident2D( int a, int axisX,
										winding_t *winding, int axisY,
										int b, int c, int d );

static void SetupTriSide( const vec3_t a, const vec3_t b, int axisX,
						  int axisY, double side[3] );

static double TrisLargestAngleCosine( const double side0[3],
									 const double side1[3], const double side2[3] );

static int TriSurfHashIndex( shaderInfo_t *shader, int lightStyle );

void FreeTriSurfProps( void );

static __inline float TrisAffineValue( const vec4_t row, const vec3_t point );

static triSurfProps_t *FindOrCreateTriSurfProps( winding_t *winding,
	const vec4_t plane, shaderInfo_t *shader, int lightStyle, float smoothAngle,
	vec4_t lmapS, vec4_t lmapT, vec4_t texS, vec4_t texT,
	vec4_t colorR, vec4_t colorG, vec4_t colorB, vec4_t colorA );

int FindTriSurfCell( winding_t *winding, triSurfProps_t *props,
	node_t *node );

static void DeriveTextureVectors( float *planeNormal, float *triPositions,
	float *texCoords, float *texVec0, float *texVec1,
	float *lmapCoords, float *lmapVec0, float *lmapVec1,
	byte *colors, float *colorVec0, float *colorVec1,
	float *colorVec2, float *colorVec3 );

static void SelectSurfaceTextureTriangle( const drawVert_t *verts, const vec3_t normal, int numVerts, float dist, vec3_t positions[3], vec2_t texCoords[3], vec2_t lightmapCoords[3] );

static qboolean SurfacesCoplanar( winding_t *windingA, const vec3_t normalA,
	const vec3_t normalB, float distA, winding_t *windingB, float distB );

static qboolean TexVecsMatch( const vec4_t a, const vec4_t b, int scale );

void TJunc_Unused0( void );

void TJunc_Unused1( void );

static void TJunc_ResetCounters( void );

static void TJunc_FreeHash( void );

static void TJunc_InsertPoint( vec3_t vertex, tjuncEdgeLine_t *edgeLine );

static tjuncHashNode_t *TJunc_HashForEdge( int axis, vec3_t vertex );

static void TJunc_AddEdgeLine( vec3_t vertA, vec3_t vertB, int axis );

static qboolean TJunc_EdgeLineMatches( tjuncEdgeLine_t *edgeLine, vec3_t vertA, vec3_t vertB );

static tjuncEdgeLine_t *TJunc_FindEdge( int axis, vec3_t vertA,
	vec3_t vertB );

static int TJunc_ClassifyEdgeAxis( vec3_t vertA, vec3_t vertB );

static void TJunc_AddEdge( vec3_t vertA, vec3_t vertB );

static winding_t *TJunc_FixWinding( winding_t *w );

void FixTriSurfaceTJunctions( triSurf_t *surfaces, int count );

int TriSurfPointerCompare( const void *a, const void *b );

static qboolean TrisFindCoincidentRun( winding_t *w, int *startA,
	int *startB, int *endA, int *endB );

static int WindingIndexInArc( int index, int start, int end );

winding_t *AllocWindingForArc( winding_t *src, int start, int end );

static qboolean WindingEdgesIntersect( float *p1, float *p2, float *p3,
	float *p4, int axisU, int axisV );

static qboolean WindingHasCrossingEdge( winding_t *w,
	vec3_t point, vec3_t origin, int axisU, int axisV );

static qboolean TrisBridgeCutsWindingWedge( winding_t *w, const vec3_t point,
	const vec3_t origin, const vec3_t normal,
	const vec3_t neighbor );

qboolean TrisReconnectWindingArcs( winding_t *w, float *normal,
	int startB, int startA, int endA, int endB, int *newA, int *newB );

void TrisRepairTouchingWinding( winding_t *w, float *normal );

static qboolean CanRemoveVert( int vertIdx, winding_t *w, int anchorIdx,
	int nextIdx, int axisU, int axisV );

static qboolean WindingPointsNear( float *pointA, float *pointB,
	float epsilon );

static void CollapseCollinearVerts( float *normal, winding_t *w );

static winding_t *SubdivideThinWinding( winding_t *w );

static qboolean BoundsCoincideOnAxis( const triSurf_t *a,
	const triSurf_t *b, int axis, float gridSize );

static int CheckWindingSeparation( winding_t *w, winding_t *testW, vec3_t normal );

static int CheckWindingSeparationBoth( winding_t *w1, winding_t *w2, vec3_t normal );

int AddWindingWithoutOverlap_r( triSurf_t *surfs, int start, int count,
							  int limit, winding_t *w, triSurfProps_t *props );

int CompareSurfaces( const void *surfA, const void *surfB );

static int MergeConcaveWindingGroup( triSurf_t **surfaces, int groupCount,
    int tailCount, int freeSlots );

int MergeVisGroupCell( int cellIndex );

void MergeVisGroupCells( void );

static void RemoveDegenerateEdges( winding_t *w );

static int TJunc_MarkIntersecting( const vec3_t mins, const vec3_t maxs,
	int limit );

void TJunc_ProcessBounds( const vec3_t mins, const vec3_t maxs );

void TJunc_FixAll( void );

static void TrisGenerateVerticesFromWinding( triSurf_t *surface );

void TrisGenerateAllVertices( void );

static int HashVertexGridCell( int x, int y, int z );

static int HashVertexPosition( const vec3_t position );

void BuildTriangleVertexRemap( void );

void ApplyMergeMap( void );

static void MergeTriangleVertexRepresentatives( int first, int second );

void TrisSnapNearbyVertices( void );

void RebuildTriangleWindings( void );

static qboolean TrianglePairCanShareEdge( int triangleA, int triangleB );

static int HashTriangleEdge( int vertexA, int vertexB );

static int FindOrCreateTriangleEdge( int triangle, int startVertex,
	int endVertex );

void BuildTriangleEdges( void );

static qboolean TriangleCanSimplify( int firstTriangle,
	int secondTriangle );

static int FindOrCreateDrawVert( float *st, byte *color, int oldVertex,
	int destinationVertex, triGroupRecord_t *record, float *lightmap );

void ApplyTriangleCollapse( int representativeVertex,
	int destinationVertex, int *affectedTriangles, int affectedCount );

static qboolean CheckCollapseAngles( int destinationVertex,
	int representativeVertex, const int *affectedTriangles, int affectedCount );

static void RetireTriangleEdge( int representativeVertex, int destinationVertex,
	int edgeIndex, int triangle );

static qboolean TryCollapseTriangleEdge( int representativeVertex,
	int destinationVertex, int edgeIndex );

static void SimplifyTriangleEdges( void );

void SimplifyTriangleMesh( void );

void SmoothTriangleNormals( void );

int CompareTriRecordHeaders( const void *a, const void *b );

static int TriRecordsShareEdge( const triGroupRecord_t *lhs,
	const triGroupRecord_t *rhs );

static int CompareTrisByGroupId( const void *a, const void *b );

int GroupAdjacentTris( triGroupRecord_t *records, int numTris );

int GroupTriSurfsIntoSubgroups( triGroupRecord_t *records,
	int numTris, int groupCount );

static int GroupAndMergeTriSubgroups( triGroupRecord_t *records, int numTris );

void FilterTriWindingIntoCells( winding_t *winding,
	const vec4_t plane, triGroupRecord_t *record, node_t *node );

static void CopyTriDrawVert( drawVert_t *destination,
	const triIntermediateVert_t *source );

static qboolean CanMergeVerts( drawVert_t *compare, drawVert_t *current,
	triGroupRecord_t *compareTri, triGroupRecord_t *currentTri, int lightStyle );

static int EmitDrawVert( triSoup_t *soup, triShaderInfo_t *material,
	int lightStyle, const triIntermediateVert_t *source );

static qboolean MatchVertWithUVOffset( triShaderInfo_t *shader, drawVert_t *vertexA,
	drawVert_t *vertexB, int *uOffset, int lightStyle, int *vOffset );

static qboolean CheckTriangleUVCompatibility( unsigned short *indices,
	int neighborTriangle, drawVert_t *verts, triShaderInfo_t *shader,
	int lightStyle, int triangle, int *uOffset, int *vOffset );

static void BuildTriangleNeighbors( drawVert_t *verts, unsigned short *indices,
	int numVerts, int numIndices, short *neighbors );

static void AdjustTriangleUVs_r( drawVert_t *verts, unsigned short *indices,
	triShaderInfo_t *shader, int lightStyle, short *neighbors, int triangle,
	byte *visited );

static void StitchTriangleUVs( unsigned short *indices, int numIndices,
	drawVert_t *verts, int numVerts, triShaderInfo_t *shader, int lightStyle );

static void CenterTextureCoordinates( drawVert_t *verts, int numVerts );

static int MergeDuplicateVerts( drawVert_t *verts, int numVerts,
	unsigned short *indices, int numIndices, triGroupRecord_t *records,
	int lightStyle );

static void MapDrawVertexIndices( unsigned short *remap, int numVerts,
	unsigned short *indices, int numIndices );

void ReorderDrawVertexIndices( drawVert_t *verts, int numVerts,
	unsigned short *indices, int numIndices );

static int PrepareTriSoupVertices( drawVert_t *verts, int numVerts,
	unsigned short *indices, int numIndices, triGroupRecord_t *records,
	int lightStyle );

static qboolean TriEdgesShareVerts( triSoup_t *soup, int earlierTriangle,
	int currentTriangle );

int CountTriGroups( triSoup_t *soup, unsigned short *groupIds );

qboolean GroupTriangles( triSoup_t *soup );

static qboolean SplitSoupOnAxis( triSoup_t *soup, int axis, float splitValue );

void PartitionTriSoupRecursive( triSoup_t *soup, qboolean tryGroupSplit );

void EmitTriangleSoup( triGroupRecord_t *records, int numTris, tree_t *tree );

static void AabbBuildSubtree( triSoupAabbNode_t *parent,
	vec3_t *itemMins, vec3_t *itemMaxs, int *indices,
	int itemOffset, int itemCount );

void AabbBuildTree_r( triSoupAabbNode_t *node, vec3_t *itemMins,
	vec3_t *itemMaxs, int *indices );

static void BuildTriSoupAABBTree( triSoupAabbNode_t *root );

void BuildTriangleSoups( tree_t *tree );

static void TriangulateUnsplittableWinding( triSurfProps_t *props,
    winding_t *winding, tree_t *tree, int cullGroupIndex );

static void Tris_EmitWindingAsDrawSurf( tree_t *tree, winding_t *winding, mapDrawSurface_t *surface, vec4_t plane, float smoothAngle,
    vec4_t texS, vec4_t texT, vec4_t lmapS, vec4_t lmapT,
    vec4_t colorR, vec4_t colorG, vec4_t colorB, vec4_t colorA );

static void Tris_FindPatchWindings( mapDrawSurface_t *surface, tree_t *tree );

static void Tris_FindIndexedMeshWindings( mapDrawSurface_t *surface, tree_t *tree );

static void Tris_FindBrushWindings( mapDrawSurface_t *surface, tree_t *tree );

static void Tris_FindWindings( entity_t *entity, tree_t *tree );

int Tris_TriangulateWindings( void );

static void TriangulateEntity( entity_t *entity, tree_t *tree );

void EmitDrawSurfaces( entity_t *entity, tree_t *tree );




/*
=============
GetProjectionAxes

Dominant normal projection with the ordinary double result of fabs.
=============
*/
static void GetProjectionAxes( const vec3_t normal, int *axisU, int *axisV ) {
	double x, y, z;

	z = fabs( normal[2] );
	x = fabs( normal[0] );
	if ( z >= x && z >= fabs( normal[1] ) ) {
		if ( normal[2] > 0 ) {
			*axisU = 0;
			*axisV = 1;
		} else {
			*axisU = 1;
			*axisV = 0;
		}
	} else if ( ( y = fabs( normal[1] ) ) >= x && y >= z ) {
		if ( normal[1] > 0 ) {
			*axisU = 2;
			*axisV = 0;
		} else {
			*axisU = 0;
			*axisV = 2;
		}
	} else {
		if ( normal[0] > 0 ) {
			*axisU = 1;
			*axisV = 2;
		} else {
			*axisU = 2;
			*axisV = 1;
		}
	}
}


/*
=============
TrisGetCellListCount
=============
*/
static int TrisGetCellListCount( void ) {
	if ( entity_num > 0 ) return 1;
	return numCells + numCullGroups;
}


/*
=============
PrependTriSurfToCell
=============
*/
static void PrependTriSurfToCell( winding_t *winding,
	triSurfProps_t *props, int cell ) {
	triSurf_t *surface;
	if ( cell < 0 ) return;
	surface = malloc( sizeof( *surface ) );
	surface->winding = winding;
	surface->props = props;
	surface->next = triSurfCellHeads[cell];
	triSurfCellHeads[cell] = surface;
}


/*
=============
FilterTriSurfIntoTree
=============
*/
void FilterTriSurfIntoTree( winding_t *winding, triSurfProps_t *props,
	node_t *node ) {
	plane_t *plane;
	int side;
	winding_t *back;
	vec3_t normal;

	if ( node->bspCell != -2 ) {
		PrependTriSurfToCell( winding, props, node->bspCell );
		return;
	}
	plane = &mapplanes[node->planenum];
	side = WindingOnPlaneSide( winding, plane->normal, plane->dist );
	if ( side == SIDE_ON ) {
		if ( DotProduct( plane->normal, props->plane ) > 0 )
			goto front;
		goto back;
	}
	if ( side == SIDE_FRONT ) {
front:
		FilterTriSurfIntoTree( winding, props, node->children[0] );
		return;
	}
	if ( side == SIDE_BACK ) {
back:
		FilterTriSurfIntoTree( winding, props, node->children[1] );
		return;
	}
	back = CopyWinding( winding );
	normal[0] = -plane->normal[0];
	normal[1] = -plane->normal[1];
	normal[2] = -plane->normal[2];
	ChopWindingInPlace( &back, normal, -plane->dist, 0.1f );
	ChopWindingInPlace( &winding, plane->normal, plane->dist, 0.1f );
	FilterTriSurfIntoTree( winding, props, node->children[0] );
	FilterTriSurfIntoTree( back, props, node->children[1] );
}


#include "../common/q_round.h"


/*
=============
TessSplitPointCompare

Orders split candidates by winding edge and position along the edge.
Equal and unordered positions deliberately return +1.
=============
*/
int TessSplitPointCompare( tessSplitPoint_t *splitA, tessSplitPoint_t *splitB ) {
	if ( splitA->edgeIdx != splitB->edgeIdx ) {
		return splitA->edgeIdx - splitB->edgeIdx;
	}
	if ( splitA->position < splitB->position ) {
		return -1;
	}
	return 1;
}


/*
=============
TrisInsertSplitPoints

Insert existing winding vertices lying in the interior of projected edges.
=============
*/
static void TrisInsertSplitPoints( winding_t **winding, const vec3_t normal, winding_t **originalWinding ) {
    tessSplitPoint_t splits[16384];
    int axisU, axisV, vertex, previous, offset, scan, count, nextSplit, out;
    float edge[2], length, alongStart, acrossStart;
    float across, position;
    winding_t *expanded, *originalExpanded;

    GetProjectionAxes( normal, &axisU, &axisV );
    count = 0;
    previous = (*winding)->numpoints - 1;
    for ( vertex = 0; vertex < (*winding)->numpoints; vertex++ ) {
        edge[0] = (*winding)->p[vertex][axisU] - (*winding)->p[previous][axisU];
        edge[1] = (*winding)->p[vertex][axisV] - (*winding)->p[previous][axisV];
        length = VectorNormalize2D( edge );
        alongStart = edge[0] * (*winding)->p[previous][axisU]
            + edge[1] * (*winding)->p[previous][axisV];
        acrossStart = edge[1] * (*winding)->p[previous][axisU]
            - edge[0] * (*winding)->p[previous][axisV];
        for ( offset = 2; offset < (*winding)->numpoints - 2; offset++ ) {
            scan = ( offset + vertex ) % (*winding)->numpoints;
            across = edge[1] * (*winding)->p[scan][axisU]
                - edge[0] * (*winding)->p[scan][axisV] - acrossStart;
            if ( across * across >= 0.00390625f ) continue;
            position = edge[1] * (*winding)->p[scan][axisV]
                + edge[0] * (*winding)->p[scan][axisU] - alongStart;
            if ( position <= 0.0625f || position >= length - 0.0625f ) continue;
            {
                tessSplitPoint_t *split = &splits[count];
                split->edgeIdx = previous;
                split->vertIdx = scan;
                split->position = position;
                count++;
            }
        }
        previous = vertex;
    }
    if ( !count ) return;
    numTriSplitPoints += count;
    if ( count > 1 ) qsort( splits, count, sizeof( splits[0] ), TessSplitPointCompare );
    expanded = AllocWinding( count + (*winding)->numpoints );
    expanded->numpoints = count + (*winding)->numpoints;
    originalExpanded = AllocWinding( count + (*originalWinding)->numpoints );
    originalExpanded->numpoints = count + (*originalWinding)->numpoints;
    nextSplit = 0;
    for ( vertex = 0; vertex < (*winding)->numpoints; ) {
        out = nextSplit + vertex;
        VectorCopy( (*winding)->p[vertex], expanded->p[out] );
        VectorCopy( (*originalWinding)->p[vertex], originalExpanded->p[out] );
        vertex++;
        while ( nextSplit < count && splits[nextSplit].edgeIdx == vertex - 1 ) {
            VectorCopy( (*winding)->p[splits[nextSplit].vertIdx], expanded->p[vertex + nextSplit] );
            VectorCopy( (*originalWinding)->p[splits[nextSplit].vertIdx], originalExpanded->p[vertex + nextSplit] );
            nextSplit++;
        }
    }
    FreeWinding( *winding );
    FreeWinding( *originalWinding );
    *winding = expanded;
    *originalWinding = originalExpanded;
}


/*
=============
TrisRemoveDegenerateEdges

Remove a two-edge spike, restarting after each compaction.
=============
*/
void TrisRemoveDegenerateEdges( winding_t *winding,
    winding_t *originalWinding ) {
    int previous, beforePrevious, vertex;
restart:
    previous = winding->numpoints - 1;
    beforePrevious = winding->numpoints - 2;
    for ( vertex = 0; vertex < winding->numpoints; vertex++ ) {
        if ( winding->p[beforePrevious][0] == winding->p[vertex][0]
            && winding->p[beforePrevious][1] == winding->p[vertex][1]
            && winding->p[beforePrevious][2] == winding->p[vertex][2] ) {
            winding->numpoints -= 2;
            numDegenerateTriEdges++;
            if ( winding->numpoints < 3 ) {
                numDegenerateTriEdges++;
                return;
            }
            if ( previous == winding->numpoints || beforePrevious == winding->numpoints )
                goto restart;
            if ( previous == 0 ) {
                memmove( winding->p[0], winding->p[2],
                    winding->numpoints * sizeof( vec3_t ) );
            } else {
                memmove( winding->p[beforePrevious], winding->p[vertex],
                    ( winding->numpoints - vertex + 2 ) * sizeof( vec3_t ) );
                memmove( originalWinding->p[beforePrevious], originalWinding->p[vertex],
                    ( originalWinding->numpoints - vertex + 2 ) * sizeof( vec3_t ) );
            }
            goto restart;
        }
        beforePrevious = previous;
        previous = vertex;
    }
}


/*
=============
TesselateRemoveVertex

Remove the emitted ear from both current and original point arrays. The full
caller supplies previous, next and normal even though this operation needs
only the selected vertex and the two windings.
=============
*/
void TesselateRemoveVertex( winding_t *winding, winding_t *originalWinding,
	int previous, int vertex, int next, const vec3_t normal ) {
	winding->numpoints--;
	originalWinding->numpoints--;
	if ( vertex != winding->numpoints ) {
		memmove( &winding->p[vertex], &winding->p[vertex + 1],
			( winding->numpoints - vertex ) * sizeof( vec3_t ) );
		memmove( &originalWinding->p[vertex], &originalWinding->p[vertex + 1],
			( originalWinding->numpoints - vertex ) * sizeof( vec3_t ) );
	}
}


/*
=============
TriSideCheck
=============
*/
static qboolean TriSideCheck( const double side0[3], const double side1[3],
							 const double side2[3], float x, float y ) {
	if ( y * side0[0] - x * side0[1] < side0[2] )
		return qtrue;
	if ( y * side1[0] - x * side1[1] < side1[2] )
		return qtrue;
	if ( y * side2[0] - x * side2[1] >= side2[2] )
		return qtrue;
	return qfalse;
}


/*
=============
TrisCheckBothEdges
=============
*/
static qboolean TrisCheckBothEdges( const double side0[3], const double side1[3],
							   const double side2[3], float ax, float ay,
							   float bx, float by ) {
	if ( !TriSideCheck( side0, side1, side2, ax, ay ) )
		return qfalse;
	return TriSideCheck( side0, side1, side2, bx, by ) != qfalse;
}


/*
=============
TrisVerticesCoincident2D
=============
*/
static qboolean TrisVerticesCoincident2D( int a, int axisX,
										winding_t *winding, int axisY,
										int b, int c, int d ) {
	return winding->p[a][axisX] == winding->p[b][axisX]
		&& winding->p[a][axisY] == winding->p[b][axisY]
		&& winding->p[c][axisX] == winding->p[d][axisX]
		&& winding->p[c][axisY] == winding->p[d][axisY];
}


/*
=============
SetupTriSide
=============
*/
static void SetupTriSide( const vec3_t a, const vec3_t b, int axisX,
						  int axisY, double side[3] ) {
	double deltaY;
	side[0] = b[axisX] - a[axisX];
	deltaY = b[axisY] - a[axisY];
	side[1] = deltaY;
	side[2] = a[axisY] * side[0] - a[axisX] * deltaY;
}


/*
=============
TrisLargestAngleCosine
=============
*/
static double TrisLargestAngleCosine( const double side0[3],
									 const double side1[3], const double side2[3] ) {
	double lengthsSquared[3];
	double numerator, denominator;
	int adjacent0, adjacent1;
	int shortest;
	lengthsSquared[0] = side0[0] * side0[0] + side0[1] * side0[1];
	shortest = 0;
	lengthsSquared[1] = side1[0] * side1[0] + side1[1] * side1[1];
	lengthsSquared[2] = side2[0] * side2[0] + side2[1] * side2[1];
	if ( lengthsSquared[0] > lengthsSquared[1] )
		shortest = 1;
	if ( lengthsSquared[shortest] > lengthsSquared[2] )
		shortest = 2;
	numerator = lengthsSquared[adjacent0 = ( ~shortest ) & 1];
	numerator += lengthsSquared[adjacent1 = ( ~shortest ) & 2];
	numerator -= lengthsSquared[shortest];
	numerator *= 0.5;
	denominator = lengthsSquared[adjacent1];
	denominator *= lengthsSquared[adjacent0];
	denominator = sqrt( denominator );
	return numerator / denominator;
}


/*
=============
TriSurfHashIndex
=============
*/
static int TriSurfHashIndex( shaderInfo_t *shader, int lightStyle ) {
	return (((shader - shaderInfo) & 255) << 6) | (lightStyle & 63);
}


/*
=============
FreeTriSurfProps

Release the property cache after the triangle emission pipeline.
=============
*/
void FreeTriSurfProps( void ) {
	triSurfProps_t *props, *next;
	int i;

	for ( i = 0; i < TRI_SURF_PROPS_HASH_BUCKETS; i++ ) {
		for ( props = triSurfPropsHash[i]; props; props = next ) {
			next = props->hashNext;
			free( props );
		}
		triSurfPropsHash[i] = NULL;
	}
}


/*
=============
TrisAffineValue

Evaluate an affine row in the surface point coordinate system.
=============
*/
static __inline float TrisAffineValue( const vec4_t row, const vec3_t point ) {
	float value;
	value = row[2] * point[2];
	value += row[1] * point[1];
	value += row[0] * point[0];
	return value + row[3];
}


/*
=============
FindOrCreateTriSurfProps
=============
*/
static triSurfProps_t *FindOrCreateTriSurfProps( winding_t *winding,
	const vec4_t plane, shaderInfo_t *shader, int lightStyle, float smoothAngle,
	vec4_t lmapS, vec4_t lmapT, vec4_t texS, vec4_t texT,
	vec4_t colorR, vec4_t colorG, vec4_t colorB, vec4_t colorA ) {
	triSurfProps_t *props;
	int hash, i, count;
	float s, t;
	vec4_t colorDelta;
	vec_t *point;

	hash = TriSurfHashIndex( shader, lightStyle );
	if ( !shader->clampTexture ) {
		texS[3] -= FloorToInt( texS[3] );
		texT[3] -= FloorToInt( texT[3] );
	}
	for ( props = triSurfPropsHash[hash]; props; props = props->hashNext ) {
		if ( props->shaderInfo != shader || props->lightStyle != lightStyle
			|| props->smoothAngle != smoothAngle
			|| DotProduct( plane, props->plane ) < 0.999f
			|| fabs( props->plane[3] - plane[3] ) > 0.001f )
			continue;
		for ( i = 0, count = winding->numpoints; i < count; i++ ) {
			float planeDistance;
			point = winding->p[i];
			planeDistance = props->plane[2] * winding->p[i][2];
			planeDistance += winding->p[i][1] * props->plane[1];
			planeDistance += winding->p[i][0] * props->plane[0];
			if ( fabs( planeDistance - props->plane[3] )
				> 0.001f ) break;
			s = ( ( DotProduct( props->texVecs[0], point ) + props->texVecs[0][3] )
				- ( DotProduct( texS, point ) + texS[3] ) ) * shader->width;
			t = ( ( DotProduct( props->texVecs[1], point ) + props->texVecs[1][3] )
				- ( DotProduct( texT, point ) + texT[3] ) ) * shader->height;
			if ( t*t + s*s > 0.25f ) break;
			if ( lightStyle >= 0 ) {
				s = ( ( DotProduct( props->lmapVecs[0], point ) + props->lmapVecs[0][3] )
					- ( DotProduct( lmapS, point ) + lmapS[3] ) ) * 512.0f;
				t = ( ( DotProduct( props->lmapVecs[1], point ) + props->lmapVecs[1][3] )
					- ( DotProduct( lmapT, point ) + lmapT[3] ) ) * 512.0f;
				if ( t*t + s*s > 0.25f ) break;
			}
			colorDelta[0] = ( DotProduct( props->colorVecs[0], winding->p[i] ) + props->colorVecs[0][3] )
				- ( DotProduct( colorR, winding->p[i] ) + colorR[3] );
			colorDelta[1] = ( DotProduct( props->colorVecs[1], winding->p[i] ) + props->colorVecs[1][3] )
				- ( DotProduct( colorG, winding->p[i] ) + colorG[3] );
			colorDelta[2] = ( DotProduct( props->colorVecs[2], winding->p[i] ) + props->colorVecs[2][3] )
				- ( DotProduct( colorB, winding->p[i] ) + colorB[3] );
			colorDelta[3] = ( DotProduct( props->colorVecs[3], winding->p[i] ) + props->colorVecs[3][3] )
				- ( DotProduct( colorA, winding->p[i] ) + colorA[3] );
			if ( colorDelta[3]*colorDelta[3] + colorDelta[2]*colorDelta[2]
				+ colorDelta[1]*colorDelta[1] + colorDelta[0]*colorDelta[0] > 0.25f ) break;
		}
		if ( i == count ) return props;
	}
	props = malloc( sizeof( *props ) );
	props->shaderInfo = shader;
	props->lightStyle = lightStyle;
	props->smoothAngle = smoothAngle;
	TRIS_COPY4( plane, props->plane );
	TRIS_COPY4( texS, props->texVecs[0] );
	TRIS_COPY4( texT, props->texVecs[1] );
	TRIS_COPY4( lmapS, props->lmapVecs[0] );
	TRIS_COPY4( lmapT, props->lmapVecs[1] );
	TRIS_COPY4( colorR, props->colorVecs[0] );
	TRIS_COPY4( colorG, props->colorVecs[1] );
	TRIS_COPY4( colorB, props->colorVecs[2] );
	TRIS_COPY4( colorA, props->colorVecs[3] );
	props->hashNext = triSurfPropsHash[hash];
	triSurfPropsHash[hash] = props;
	return props;
}


/*
=============
FindTriSurfCell
=============
*/
int FindTriSurfCell( winding_t *winding, triSurfProps_t *props,
	node_t *node ) {
	plane_t *plane;
	int side, frontCell, backCell;
	winding_t *clipped;
	vec3_t normal;

	while ( node->bspCell == -2 ) {
		plane = &mapplanes[node->planenum];
		side = WindingOnPlaneSide( winding, plane->normal, plane->dist );
		if ( side == SIDE_ON ) {
			if ( DotProduct( plane->normal, props->plane ) > 0 )
				goto front;
			goto back;
		}
		if ( side == SIDE_FRONT ) {
front:
			node = node->children[0];
		} else if ( side == SIDE_BACK ) {
back:
			node = node->children[1];
		} else {
			goto split;
		}
	}
	return node->bspCell;
split:
	clipped = CopyWinding( winding );
	ChopWindingInPlace( &clipped, plane->normal, plane->dist, 0.1f );
	frontCell = FindTriSurfCell( clipped, props, node->children[0] );
	FreeWinding( clipped );
	if ( frontCell == -2 ) return -2;
	else {
		clipped = CopyWinding( winding );
		normal[0] = -plane->normal[0];
		normal[1] = -plane->normal[1];
		normal[2] = -plane->normal[2];
		ChopWindingInPlace( &clipped, normal, -plane->dist, 0.1f );
		backCell = FindTriSurfCell( clipped, props, node->children[1] );
		FreeWinding( clipped );
		if ( frontCell != backCell && backCell != -1 ) {
			if ( frontCell != -1 ) return -2;
			return backCell;
		}
		return frontCell;
	}
}


/*
=============
DeriveTextureVectors
=============
*/
static void DeriveTextureVectors( float *planeNormal, float *triPositions,
	float *texCoords, float *texVec0, float *texVec1,
	float *lmapCoords, float *lmapVec0, float *lmapVec1,
	byte *colors, float *colorVec0, float *colorVec1,
	float *colorVec2, float *colorVec3 ) {
	const double epsilon = TRIANGLE_DETERMINANT_EPSILON;
	double x0, x1, x2, y0, y1, y2;
	double dx02, dx21, dx10, dy01, dy12, dy20;
	double area01, area20, area12;
	double determinant;
	double inverseDeterminant;
	vec3_t centroid;
	/* the original reloads these values into locals before each use; kept so the build matches the original binary */
	float texS2;
	double texT1;
	double texT2;
	double lmapS0;
	double lmapS1;
	float lmapT1;
	float lmapT2;
	double colorG1;
	float colorG2;
	double colorA1;
	float colorA2;
	int drop;
	int axis1;
	int axis2;

	drop = 0;
	if ( fabs( planeNormal[1] ) > fabs( planeNormal[0] ) ) drop = 1;
	if ( fabs( planeNormal[2] ) > fabs( planeNormal[drop] ) ) {
		drop = 2;
	}
	axis1 = ( ~drop ) & 1;
	axis2 = ( ~drop ) & 2;
	x0 = triPositions[axis1];
	x1 = triPositions[axis1 + 3];
	x2 = triPositions[axis1 + 6];
	y0 = triPositions[axis2];
	y1 = triPositions[axis2 + 3];
	y2 = triPositions[axis2 + 6];
	dx02 = x0 - x2;
	dx21 = x2 - x1;
	dx10 = x1 - x0;
	dy01 = y0 - y1;
	dy12 = y1 - y2;
	dy20 = y2 - y0;
	area01 = x0 * y1 - y0 * x1;
	area12 = y2 * x1 - y1 * x2;
	area20 = y0 * x2 - y2 * x0;
	determinant = dy20 * x1 + dy01 * x2 + dy12 * x0;
	if ( determinant * determinant < epsilon * epsilon ) {
		VectorAdd( triPositions, (triPositions + 3), centroid );
		VectorAdd( centroid, (triPositions + 6), centroid );
		VectorScale( centroid, (1.0f / 3.0f), centroid );
		Error( "Degenerate triangle (ie, point or line) given to DeriveTextureVectors; "
			"ref pos = (%.0f %.0f %.0f)\n"
			"This is sometimes caused by improper collapsing of vertices on the boundary "
			"between terrains.  It may also be caused by a bad brush.  Sometimes using "
			"the clipping tool can cause this; hiding all brushes in the area, selecting "
			"complete tall around the point, and pressing 'delete' may fix the problem.\n",
			centroid[0], centroid[1], centroid[2] );
	}
	inverseDeterminant = 1.0 / determinant;
	if ( texCoords ) {
		texS2 = texCoords[4];
		texVec0[axis1] = ( texCoords[0] * dy12 + texCoords[2] * dy20 + texS2 * dy01 ) * inverseDeterminant;
		texS2 = texCoords[4];
		texVec0[axis2] = ( texCoords[0] * dx21 + texCoords[2] * dx02 + texS2 * dx10 ) * inverseDeterminant;
		texVec0[drop] = 0.0f;
		texS2 = texCoords[4];
		texVec0[3] = ( texCoords[0] * area12 + texCoords[2] * area20 + texS2 * area01 ) * inverseDeterminant;
		texT1 = texCoords[3];
		texT2 = texCoords[5];
		texVec1[axis1] = ( texCoords[1] * dy12 + texT1 * dy20 + texT2 * dy01 ) * inverseDeterminant;
		texT1 = texCoords[3];
		texT2 = texCoords[5];
		texVec1[axis2] = ( texCoords[1] * dx21 + texT1 * dx02 + texT2 * dx10 ) * inverseDeterminant;
		texVec1[drop] = 0.0f;
		texT1 = texCoords[3];
		texT2 = texCoords[5];
		texVec1[3] = ( texCoords[1] * area12 + texT1 * area20 + texT2 * area01 ) * inverseDeterminant;
	}
	if ( lmapCoords ) {
		lmapS0 = lmapCoords[0];
		lmapS1 = lmapCoords[2];
		lmapVec0[axis1] = ( lmapS0 * dy12 + lmapS1 * dy20 + lmapCoords[4] * dy01 ) * inverseDeterminant;
		lmapS1 = lmapCoords[2];
		lmapVec0[axis2] = ( lmapCoords[0] * dx21 + lmapS1 * dx02 + lmapCoords[4] * dx10 ) * inverseDeterminant;
		lmapVec0[drop] = 0.0f;
		lmapS0 = lmapCoords[0];
		lmapS1 = lmapCoords[2];
		lmapVec0[3] = ( lmapS0 * area12 + lmapS1 * area20 + lmapCoords[4] * area01 ) * inverseDeterminant;
		lmapT1 = lmapCoords[3];
		lmapT2 = lmapCoords[5];
		lmapVec1[axis1] = ( lmapCoords[1] * dy12 + lmapT1 * dy20 + lmapT2 * dy01 ) * inverseDeterminant;
		lmapT1 = lmapCoords[3];
		lmapT2 = lmapCoords[5];
		lmapVec1[axis2] = ( lmapCoords[1] * dx21 + lmapT1 * dx02 + lmapT2 * dx10 ) * inverseDeterminant;
		lmapVec1[drop] = 0.0f;
		lmapT1 = lmapCoords[3];
		lmapT2 = lmapCoords[5];
		lmapVec1[3] = ( lmapCoords[1] * area12 + lmapT1 * area20 + lmapT2 * area01 ) * inverseDeterminant;
	}
	if ( colors ) {
		colorVec0[axis1] = ( colors[0] * dy12 + colors[4] * dy20 + colors[8] * dy01 ) * inverseDeterminant;
		colorVec0[axis2] = ( colors[0] * dx21 + colors[4] * dx02 + colors[8] * dx10 ) * inverseDeterminant;
		colorVec0[drop] = 0.0f;
		colorVec0[3] = ( colors[0] * area12 + colors[4] * area20 + colors[8] * area01 ) * inverseDeterminant;
		colorG1 = colors[5];
		colorG2 = colors[9];
		colorVec1[axis1] = ( colors[1] * dy12 + colorG1 * dy20 + colorG2 * dy01 ) * inverseDeterminant;
		colorG1 = colors[5];
		colorG2 = colors[9];
		colorVec1[axis2] = ( colors[1] * dx21 + colorG1 * dx02 + colorG2 * dx10 ) * inverseDeterminant;
		colorVec1[drop] = 0.0f;
		colorG1 = colors[5];
		colorG2 = colors[9];
		colorVec1[3] = ( colors[1] * area12 + colorG1 * area20 + colorG2 * area01 ) * inverseDeterminant;
		colorVec2[axis1] = ( colors[2] * dy12 + colors[6] * dy20 + colors[10] * dy01 ) * inverseDeterminant;
		colorVec2[axis2] = ( colors[2] * dx21 + colors[6] * dx02 + colors[10] * dx10 ) * inverseDeterminant;
		colorVec2[drop] = 0.0f;
		colorVec2[3] = ( colors[2] * area12 + colors[6] * area20 + colors[10] * area01 ) * inverseDeterminant;
		colorA1 = colors[7];
		colorA2 = colors[11];
		colorVec3[axis1] = ( colors[3] * dy12 + colorA1 * dy20 + colorA2 * dy01 ) * inverseDeterminant;
		colorA1 = colors[7];
		colorA2 = colors[11];
		colorVec3[axis2] = ( colors[3] * dx21 + colorA1 * dx02 + colorA2 * dx10 ) * inverseDeterminant;
		colorVec3[drop] = 0.0f;
		colorA1 = colors[7];
		colorA2 = colors[11];
		colorVec3[3] = ( colors[3] * area12 + colorA1 * area20 + colorA2 * area01 ) * inverseDeterminant;
	}
}


/*
=============
SelectSurfaceTextureTriangle
=============
*/
static void SelectSurfaceTextureTriangle( const drawVert_t *verts, const vec3_t normal, int numVerts, float dist, vec3_t positions[3], vec2_t texCoords[3], vec2_t lightmapCoords[3] ) {
	int i, j, k, selectedI, selectedJ, selectedK;
	float bestScore, score;
	vec3_t edgeA, edgeB, cross;
	float offset;
	selectedI = 0;
	selectedJ = 0;
	selectedK = 0;
	bestScore = 0;
	for ( k = 2; k < numVerts; k++ ) {
		for ( j = 1; j < k; j++ ) {
			VectorSubtract( verts[k].xyz, verts[j].xyz, edgeA );
			for ( i = 0; i < j; i++ ) {
				VectorSubtract( verts[i].xyz, verts[j].xyz, edgeB );
				CrossProduct( edgeB, edgeA, cross );
				score = cross[0] * normal[0] + ( cross[1] * normal[1] + cross[2] * normal[2] );
				if ( score > bestScore ) {
					bestScore = score;
					selectedI = i;
					selectedJ = j;
					selectedK = k;
				}
			}
		}
	}

	offset = dist - DotProduct( normal, verts[selectedI].xyz );
	positions[0][0] = verts[selectedI].xyz[0] + offset * normal[0];
	positions[0][1] = verts[selectedI].xyz[1] + offset * normal[1];
	positions[0][2] = verts[selectedI].xyz[2] + offset * normal[2];
	VectorCopy( verts[selectedI].xyz, positions[0] );
	texCoords[0][0] = verts[selectedI].st[0];
	texCoords[0][1] = verts[selectedI].st[1];
	lightmapCoords[0][0] = verts[selectedI].lightmap[0];
	lightmapCoords[0][1] = verts[selectedI].lightmap[1];
	offset = dist - DotProduct( normal, verts[selectedJ].xyz );
	positions[1][0] = verts[selectedJ].xyz[0] + offset * normal[0];
	positions[1][1] = verts[selectedJ].xyz[1] + offset * normal[1];
	positions[1][2] = verts[selectedJ].xyz[2] + offset * normal[2];
	VectorCopy( verts[selectedJ].xyz, positions[1] );
	texCoords[1][0] = verts[selectedJ].st[0];
	texCoords[1][1] = verts[selectedJ].st[1];
	lightmapCoords[1][0] = verts[selectedJ].lightmap[0];
	lightmapCoords[1][1] = verts[selectedJ].lightmap[1];
	offset = dist - DotProduct( normal, verts[selectedK].xyz );
	positions[2][0] = verts[selectedK].xyz[0] + offset * normal[0];
	positions[2][1] = verts[selectedK].xyz[1] + offset * normal[1];
	positions[2][2] = verts[selectedK].xyz[2] + offset * normal[2];
	VectorCopy( verts[selectedK].xyz, positions[2] );
	texCoords[2][0] = verts[selectedK].st[0];
	texCoords[2][1] = verts[selectedK].st[1];
	lightmapCoords[2][0] = verts[selectedK].lightmap[0];
	lightmapCoords[2][1] = verts[selectedK].lightmap[1];
}


/*
=============
SurfacesCoplanar

Test each winding against the other surface plane.
=============
*/
static qboolean SurfacesCoplanar( winding_t *windingA, const vec3_t normalA,
	const vec3_t normalB, float distA, winding_t *windingB, float distB ) {
	int i;
	double normalDot;
	normalDot = normalA[1] * normalB[1] + normalA[2] * normalB[2] + normalA[0] * normalB[0];
	if ( (float)normalDot < 0.999f ) return qfalse;
	if ( fabs( distA - distB ) > 0.001f ) return qfalse;
	for ( i = 0; i < windingA->numpoints; i++ ) {
		const float *point = windingA->p[i];
		double value = point[0] * normalB[0];
		value += point[2] * normalB[2];
		{
			double y = point[1];
			y *= normalB[1];
			value += y;
		}
		if ( (float)( value - distB ) > 0.001f ) return qfalse;
	}
	for ( i = 0; i < windingB->numpoints; i++ ) {
		const float *point = windingB->p[i];
		double value = point[0] * normalA[0];
		value += point[2] * normalA[2];
		{
			double y = point[1];
			y *= normalA[1];
			value += y;
		}
		if ( (float)( value - distA ) > 0.001f ) return qfalse;
	}
	return qtrue;
}

/*
=============
TexVecsMatch

Compare affine coefficients in pixel units, retaining the one-sided test.
=============
*/
static qboolean TexVecsMatch( const vec4_t a, const vec4_t b, int scale ) {
	int i;
	for ( i = 0; i < 4; i++ ) {
		if ( ( a[i] - b[i] ) * scale > 0.001f ) return qfalse;
	}
	return qtrue;
}


/*
=============
TJunc_Unused0

Empty hook functions; nothing calls them (probably compiled-out debug hooks).
=============
*/
void TJunc_Unused0( void ) {
}


/*
=============
TJunc_Unused1
=============
*/
void TJunc_Unused1( void ) {
}


/*
=============
TJunc_ResetCounters

Reset the edge-line and point cursors; the hash buckets are released
separately.
=============
*/
static void TJunc_ResetCounters( void ) {
	numTJuncEdgeLines = 0;
	numTJuncPoints = 0;
}


/*
=============
TJunc_FreeHash

Releases the per-bucket edge-line reference chains after a complete
T-junction pass.  The edge lines and points themselves live in fixed arrays;
only these eight-byte hash references are heap allocated.
=============
*/
static void TJunc_FreeHash( void ) {
	int axis;
	int row;
	int column;

	for ( axis = 0; axis < TJUNC_HASH_AXES; axis++ ) {
		for ( row = 0; row < TJUNC_HASH_AXIS_SIZE; row++ ) {
			for ( column = 0; column < TJUNC_HASH_AXIS_SIZE; column++ ) {
				tjuncHashNode_t *node;

				node = tjuncHash[axis][row][column].next;
				while ( node ) {
					tjuncHashNode_t *next;

					next = node->next;
					free( node );
					node = next;
				}
				tjuncHash[axis][row][column].next = NULL;
			}
		}
	}
}


/*
=============
TJunc_InsertPoint
=============
*/
static void TJunc_InsertPoint( vec3_t vertex, tjuncEdgeLine_t *edgeLine ) {
	tjuncPoint_t *point;
	tjuncPoint_t *scan;
	float toleranceSquared;

	if ( numTJuncPoints == MAX_TJUNC_POINTS ) {
		Error( "MAX_TJUNC_POINTS" );
	}

	point = &tjuncPoints[numTJuncPoints];
	memset( point, 0, sizeof( *point ) );
	numTJuncPoints++;
	point->intercept = edgeLine->direction[2] * vertex[2]
		+ edgeLine->direction[0] * vertex[0]
		+ edgeLine->direction[1] * vertex[1];
	VectorCopy( vertex, point->xyz );

	for ( scan = edgeLine->sentinel.next;
		scan != &edgeLine->sentinel; scan = scan->next ) {
		float delta;

		delta = scan->intercept - point->intercept;
		toleranceSquared = tjuncTolerance * tjuncTolerance;
		if ( delta * delta < toleranceSquared ) {
			numTJuncPoints--;
			return;
		}
		if ( point->intercept < scan->intercept ) {
			break;
		}
	}

	point->prev = scan->prev;
	point->next = scan;
	scan->prev->next = point;
	scan->prev = point;
}


/*
=============
TJunc_HashForEdge
=============
*/
static tjuncHashNode_t *TJunc_HashForEdge( int axis, vec3_t vertex ) {
	int axis1;
	int axis2;
	int s;
	int t;
	float scaledS, scaledT;
	tree_t *tree;

	axis1 = ( ~axis ) & 1;
	axis2 = ( ~axis ) & 2;
	tree = trisCurrentTree;

	scaledS = ( vertex[axis1] - tree->mins[axis1] ) /
		( tree->maxs[axis1] - tree->mins[axis1]
			+ 1.0f ) *
		TJUNC_HASH_AXIS_SIZE;
	s = FloorToInt( scaledS );
	if ( s < 0 ) {
		s = 0;
	}
	else if ( s > TJUNC_HASH_AXIS_SIZE - 1 ) {
		s = TJUNC_HASH_AXIS_SIZE - 1;
	}

	scaledT = ( vertex[axis2] - tree->mins[axis2] ) /
		( tree->maxs[axis2] - tree->mins[axis2]
			+ 1.0f ) *
		TJUNC_HASH_AXIS_SIZE;
	t = FloorToInt( scaledT );
	if ( t < 0 ) {
		t = 0;
	}
	else if ( t > TJUNC_HASH_AXIS_SIZE - 1 ) {
		t = TJUNC_HASH_AXIS_SIZE - 1;
	}

	return &tjuncHash[axis][t][s];
}


/*
=============
TJunc_AddEdgeLine
=============
*/
static void TJunc_AddEdgeLine( vec3_t vertA, vec3_t vertB, int axis ) {
	tjuncEdgeLine_t *edgeLine;

	if ( numTJuncEdgeLines == MAX_TJUNC_EDGE_LINES ) {
		Error( "MAX_EDGE_LINES" );
	}

	edgeLine = &tjuncEdgeLines[numTJuncEdgeLines];
	VectorSubtract( vertB, vertA, edgeLine->direction );
	if ( VectorNormalize( edgeLine->direction ) == 0.0f ) {
		return;
	}
	VectorCopy( vertA, edgeLine->origin );
	MakeNormalVectors( edgeLine->direction, edgeLine->normal1,
		edgeLine->normal2 );
	edgeLine->dist1 = edgeLine->normal1[1] * vertA[1] + edgeLine->normal1[2] * vertA[2] + edgeLine->normal1[0] * vertA[0];
	edgeLine->dist2 = edgeLine->normal2[1] * vertA[1] + edgeLine->normal2[2] * vertA[2] + edgeLine->normal2[0] * vertA[0];
	edgeLine->sentinel.prev = &edgeLine->sentinel;
	edgeLine->sentinel.next = &edgeLine->sentinel;

	TJunc_InsertPoint( vertA, edgeLine );
	TJunc_InsertPoint( vertB, edgeLine );
	numTJuncEdgeLines++;

	if ( axis >= 0 ) {
		tjuncHashNode_t *bucket;
		tjuncHashNode_t *node;

		bucket = TJunc_HashForEdge( axis, vertA );
		node = malloc( sizeof( *node ) );
		node->edgeLine = edgeLine;
		node->next = bucket->next;
		bucket->next = node;
	}
}


/*
=============
TJunc_EdgeLineMatches
=============
*/
static qboolean TJunc_EdgeLineMatches( tjuncEdgeLine_t *edgeLine, vec3_t vertA, vec3_t vertB ) {
    if ( fabs( DotProduct( vertA, edgeLine->normal1 ) - edgeLine->dist1 ) > tjuncTolerance ) return qfalse;
    if ( fabs( DotProduct( vertA, edgeLine->normal2 ) - edgeLine->dist2 ) > tjuncTolerance ) return qfalse;
    if ( fabs( DotProduct( vertB, edgeLine->normal1 ) - edgeLine->dist1 ) > tjuncTolerance ) return qfalse;
    if ( fabs( DotProduct( vertB, edgeLine->normal2 ) - edgeLine->dist2 ) > tjuncTolerance ) return qfalse;
    return qtrue;
}


/*
=============
TJunc_FindEdge
=============
*/
static tjuncEdgeLine_t *TJunc_FindEdge( int axis, vec3_t vertA,
	vec3_t vertB ) {
	int edgeIndex;
	tjuncHashNode_t *node;

	if ( axis < 0 ) {
		for ( edgeIndex = 0; edgeIndex < numTJuncEdgeLines; edgeIndex++ ) {
			if ( TJunc_EdgeLineMatches( &tjuncEdgeLines[edgeIndex], vertA, vertB ) ) {
				return &tjuncEdgeLines[edgeIndex];
			}
		}
		return NULL;
	}
	else {
		for ( node = TJunc_HashForEdge( axis, vertA )->next; node;
			node = node->next ) {
			if ( TJunc_EdgeLineMatches( node->edgeLine, vertA, vertB ) ) {
				return node->edgeLine;
			}
		}
		return NULL;
	}
}


/*
=============
TJunc_ClassifyEdgeAxis
=============
*/
static int TJunc_ClassifyEdgeAxis( vec3_t vertA, vec3_t vertB ) {
	float threshold;
	vec3_t delta;
	int flags;

	VectorSubtract( vertA, vertB, delta );
	threshold = tjuncTolerance * tjuncTolerance * 0.25f;

	flags = ( delta[0] * delta[0] > threshold ? 1 : 0 );
	flags |= ( delta[1] * delta[1] > threshold ? 2 : 0 );
	flags = ( delta[2] * delta[2] > threshold ? 4 : 0 ) | flags;

	switch ( flags ) {
	case 1: return 0;
	case 2: return 1;
	case 4: return 2;
	default: return -1;
	}
}


/*
=============
TJunc_AddEdge

Adds one winding edge to the CoD triangle-surface T-junction database.
Existing lines receive both endpoints; otherwise a new line is created.
=============
*/
static void TJunc_AddEdge( vec3_t vertA, vec3_t vertB ) {
	int axis;
	tjuncEdgeLine_t *edgeLine;

	axis = TJunc_ClassifyEdgeAxis( vertA, vertB );
	edgeLine = TJunc_FindEdge( axis, vertA, vertB );
	if ( !edgeLine ) {
		TJunc_AddEdgeLine( vertA, vertB, axis );
		return;
	}
	TJunc_InsertPoint( vertA, edgeLine );
	TJunc_InsertPoint( vertB, edgeLine );
}


/*
=============
TJunc_FixWinding
=============
*/
static winding_t *TJunc_FixWinding( winding_t *w ) {
	vec3_t points[MAX_POINTS_ON_CONCAVE_WINDING];
	float *current;
	int outCount;
	int edgeIndex;

	outCount = 0;
	edgeIndex = 0;
	if ( w->numpoints > 0 ) {
		current = w->p[0];
		do {
			float *next;
			int axis;
			tjuncEdgeLine_t *edgeLine;

			if ( outCount == MAX_POINTS_ON_CONCAVE_WINDING ) {
				Error( "MAX_POINTS_ON_CONCAVE_WINDING" );
			}
			VectorCopy( current, points[outCount] );
			next = w->p[( edgeIndex + 1 ) % w->numpoints];
			outCount++;

			axis = TJunc_ClassifyEdgeAxis( current, next );
			edgeLine = TJunc_FindEdge( axis, current, next );
			if ( edgeLine ) {
				float currentIntercept;
				float nextIntercept;
                double intercept;
				tjuncPoint_t *sentinel;
				tjuncPoint_t *point;
                intercept = edgeLine->direction[2] * current[2];
                intercept += current[0] * edgeLine->direction[0];
                currentIntercept = intercept + current[1] * edgeLine->direction[1];
                nextIntercept = DotProduct( next, edgeLine->direction );
                point = currentIntercept < nextIntercept ? edgeLine->sentinel.next : edgeLine->sentinel.prev;
                sentinel = &edgeLine->sentinel;
                while ( point != sentinel ) {
                    if ( currentIntercept < nextIntercept ) {
                        if ( nextIntercept - tjuncTolerance < point->intercept ) break;
                    } else if ( tjuncTolerance + nextIntercept > point->intercept ) break;
                    if ( ( currentIntercept < nextIntercept && tjuncTolerance + currentIntercept < point->intercept )
                        || ( currentIntercept > nextIntercept && currentIntercept - tjuncTolerance > point->intercept ) ) {
                        if ( outCount == MAX_POINTS_ON_CONCAVE_WINDING ) Error( "MAX_POINTS_ON_CONCAVE_WINDING" );
                        VectorCopy( point->xyz, points[outCount] );
                        outCount++;
                    }
                    point = currentIntercept < nextIntercept ? point->next : point->prev;
                }
			}

			edgeIndex++;
			current += 3;
		} while ( edgeIndex < w->numpoints );
	}

	if ( outCount > w->numpoints ) {
		winding_t *expanded;

		FreeWinding( w );
		expanded = AllocWinding( outCount );
		expanded->numpoints = outCount;
		memcpy( expanded->p, points, outCount * sizeof( vec3_t ) );
		return expanded;
	}
	return w;
}


/*
=============
FixTriSurfaceTJunctions

Repair the complete temporary surface array used by concave merging.
=============
*/
void FixTriSurfaceTJunctions( triSurf_t *surfaces, int count ) {
	int surfaceIndex;
	int vertex;
	int numPoints;
	winding_t *w;

	TJunc_ResetCounters();
	for ( surfaceIndex = 0; surfaceIndex < count; surfaceIndex++ ) {
		w = surfaces[surfaceIndex].winding;
		numPoints = w->numpoints;
		for ( vertex = 0; vertex < numPoints; vertex++ ) {
			TJunc_AddEdge( w->p[vertex], w->p[( vertex + 1 ) % numPoints] );
		}
	}
	for ( surfaceIndex = 0; surfaceIndex < count; surfaceIndex++ ) {
		surfaces[surfaceIndex].winding = TJunc_FixWinding( surfaces[surfaceIndex].winding );
	}
	TJunc_FreeHash();
}


/*
=============
TriSurfPointerCompare
=============
*/
int TriSurfPointerCompare( const void *a, const void *b ) {
	triSurfProps_t *propsA = ( *( const triSurf_t * const * ) a )->props;
	triSurfProps_t *propsB = ( *( const triSurf_t * const * ) b )->props;

	if ( propsA != propsB ) {
		return propsA - propsB;
	}
	return 0;
}


/*
=============
TrisFindCoincidentRun
=============
*/
static qboolean TrisFindCoincidentRun( winding_t *w, int *startA,
	int *startB, int *endA, int *endB ) {
	int i, span, other, next, previous, forward;
	float *point;
	for ( i = 0; i < w->numpoints; i++ ) {
		for ( span = 3; span < w->numpoints - 3; span++ ) {
			point = w->p[i];
			other = ( i + span ) % w->numpoints;
			if ( !VectorCompareEpsilon( point, w->p[other] ) ) continue;
			next = ( i + 1 ) % w->numpoints;
			previous = ( w->numpoints + other - 1 ) % w->numpoints;
			if ( !VectorCompareEpsilon( w->p[next], w->p[previous] ) ) continue;
			*startA = i;
			*startB = other;
			if ( !i ) {
				i = w->numpoints - 1;
				forward = ( other + 1 ) % w->numpoints;
				while ( VectorCompareEpsilon( w->p[i], w->p[forward] ) ) {
					*startA = i;
					*startB = forward;
					i--;
					forward = ( forward + 1 ) % w->numpoints;
				}
			}
			do {
				*endA = next;
				*endB = previous;
				next = ( next + 1 ) % w->numpoints;
				previous = ( w->numpoints + previous - 1 ) % w->numpoints;
			} while ( VectorCompareEpsilon( w->p[next], w->p[previous] ) );
			return qtrue;
		}
	}
	return qfalse;
}


/*
=============
WindingIndexInArc

Tests membership in the half-open cyclic interval [start, end).
=============
*/
static int WindingIndexInArc( int index, int start, int end ) {
	if ( start < end ) {
		return index >= start && index < end;
	}
	return index >= start || index < end;
}


/*
=============
AllocWindingForArc

Allocates and copies the cyclic arc beginning at start and ending immediately
before end.
=============
*/
winding_t *AllocWindingForArc( winding_t *src, int start, int end ) {
	int i;
	int count;
	winding_t *dst;

	count = end - start;
	if ( count < 0 ) {
		count += src->numpoints;
	}

	dst = AllocWinding( count );
	dst->numpoints = 0;

	for ( i = 0; i < src->numpoints; i++ ) {
		if ( WindingIndexInArc( i, start, end ) ) {
			VectorCopy( src->p[i], dst->p[dst->numpoints] );
			dst->numpoints++;
		}
	}
	return dst;
}


/*
=============
WindingEdgesIntersect

Tests two winding edges in the projection selected by axisU and axisV.
=============
*/
static qboolean WindingEdgesIntersect( float *p1, float *p2, float *p3,
	float *p4, int axisU, int axisV ) {
	vec3_t edgeDir;
	float sideP4;
	float sideP3;
	float sideP2;
	float sideP1;

	VectorSubtract( p2, p1, edgeDir );
	sideP3 = ( p3[axisU] - p1[axisU] ) * edgeDir[axisV]
		- ( p3[axisV] - p1[axisV] ) * edgeDir[axisU];
	sideP4 = ( p4[axisU] - p1[axisU] ) * edgeDir[axisV]
		- ( p4[axisV] - p1[axisV] ) * edgeDir[axisU];

	if ( sideP3 <= -WINDING_INTERSECT_EPSILON
		&& sideP4 <= -WINDING_INTERSECT_EPSILON ) {
		return qfalse;
	}
	if ( sideP3 >= WINDING_INTERSECT_EPSILON
		&& sideP4 >= WINDING_INTERSECT_EPSILON ) {
		return qfalse;
	}

	VectorSubtract( p4, p3, edgeDir );
	sideP1 = ( p1[axisU] - p3[axisU] ) * edgeDir[axisV]
		- ( p1[axisV] - p3[axisV] ) * edgeDir[axisU];
	sideP2 = ( p2[axisU] - p3[axisU] ) * edgeDir[axisV]
		- ( p2[axisV] - p3[axisV] ) * edgeDir[axisU];

	if ( sideP1 <= -WINDING_INTERSECT_EPSILON
		&& sideP2 <= -WINDING_INTERSECT_EPSILON ) {
		return qfalse;
	}
	if ( sideP1 >= WINDING_INTERSECT_EPSILON
		&& sideP2 >= WINDING_INTERSECT_EPSILON ) {
		return qfalse;
	}
	if ( VectorCompareEpsilon( p1, p3 ) ) {
		return qfalse;
	}
	if ( VectorCompareEpsilon( p1, p4 ) ) {
		return qfalse;
	}
	if ( VectorCompareEpsilon( p2, p3 ) ) {
		return qfalse;
	}
	if ( VectorCompareEpsilon( p2, p4 ) ) {
		return qfalse;
	}
	if ( sideP3 * sideP3 < WINDING_INTERSECT_EPSILON_SQ
		&& sideP4 * sideP4 < WINDING_INTERSECT_EPSILON_SQ
		&& sideP1 * sideP1 < WINDING_INTERSECT_EPSILON_SQ
		&& sideP2 * sideP2 < WINDING_INTERSECT_EPSILON_SQ ) {
		return qfalse;
	}

	return qtrue;
}


/*
=============
WindingHasCrossingEdge
=============
*/
static qboolean WindingHasCrossingEdge( winding_t *w,
	vec3_t point, vec3_t origin, int axisU, int axisV ) {
	int i, previous;
	for ( i = 0, previous = w->numpoints - 1; i < w->numpoints;
		previous = i, i++ ) {
		if ( WindingEdgesIntersect( w->p[previous], w->p[i],
			(float *)point, (float *)origin, axisU, axisV ) ) return qtrue;
	}
	return qfalse;
}


/*
=============
TrisBridgeCutsWindingWedge
=============
*/
static qboolean TrisBridgeCutsWindingWedge( winding_t *w, const vec3_t point,
	const vec3_t origin, const vec3_t normal,
	const vec3_t neighbor ) {
	vec3_t edges[2];
	vec4_t planes[2];
	int i, previous, beforePrevious;
	int planeIndex;
	qboolean convex;
	vec2_t distances;

	VectorSubtract( point, origin, edges[0] );
	VectorSubtract( origin, neighbor, edges[1] );
	for ( planeIndex = 0; planeIndex < 2; planeIndex++ ) {
		CrossProduct( edges[planeIndex], normal, planes[planeIndex] );
		VectorNormalizeFast( planes[planeIndex] );
		planes[planeIndex][3] = planes[planeIndex][1] * origin[1] + planes[planeIndex][2] * origin[2];
		planes[planeIndex][3] += planes[planeIndex][0] * origin[0];
		planes[planeIndex][3] = planes[planeIndex][3] - 0.001f;
	}
	convex = DotProduct( planes[0], neighbor ) > planes[0][3];
	i = 0;
	if ( w->numpoints <= 0 ) return qfalse;
	previous = w->numpoints - 1;
	beforePrevious = w->numpoints - 2;
	for ( ;; ) {
		if ( !VectorCompareEpsilon( w->p[previous], origin ) ) goto next;
		distances[0] = DotProduct( planes[0], w->p[beforePrevious] ) - planes[0][3];
		distances[1] = DotProduct( planes[1], w->p[beforePrevious] ) - planes[1][3];
		if ( convex ) {
			if ( distances[0] < 0 || distances[1] < 0 ) return qtrue;
		} else {
			if ( distances[0] < 0 && distances[1] < 0 ) return qtrue;
		}
		distances[0] = DotProduct( planes[0], w->p[i] ) - planes[0][3];
		distances[1] = DotProduct( planes[1], w->p[i] ) - planes[1][3];
		if ( convex ) {
			if ( distances[0] < 0 || distances[1] < 0 ) return qtrue;
		} else {
			if ( distances[0] < 0 && distances[1] < 0 ) return qtrue;
		}
next:
		beforePrevious = previous;
		previous = i;
		i++;
		if ( i >= w->numpoints ) break;
	}
	return qfalse;
}


/*
=============
TrisReconnectWindingArcs
=============
*/
qboolean TrisReconnectWindingArcs( winding_t *w, float *normal,
	int startB, int startA, int endA, int endB, int *newA, int *newB ) {
	winding_t *a, *b;
	int i, j, axis, axisU, axisV;
	int beforeA, afterA, beforeB, afterB;
	float bestDistance, distance;
	vec_t *pointA, *pointB;
	vec3_t delta;
	qboolean found;

	axis = 0;
	if ( fabs( normal[1] ) > fabs( normal[0] ) ) axis = 1;
	if ( fabs( normal[2] ) > fabs( normal[axis] ) ) axis = 2;
	axisV = (~axis) & 2;
	axisU = (~axis) & 1;
	a = AllocWindingForArc( w, endA, endB );
	b = AllocWindingForArc( w, startB, startA );
	found = qfalse;
	bestDistance = FLT_MAX;
	for ( i = 0; i < a->numpoints; i++ ) {
		beforeA = (a->numpoints + i - 1) % a->numpoints;
		pointA = a->p[i];
		afterA = (i + 1) % a->numpoints;
		if ( PointsNearLine( a->p[beforeA], pointA, a->p[afterA], 0.0625f ) ) continue;
		for ( j = 0; j < b->numpoints; j++ ) {
			pointB = b->p[j];
			VectorSubtract( pointA, pointB, delta );
			distance = DotProduct( delta, delta );
			if ( distance >= bestDistance ) continue;
			beforeB = (b->numpoints + j - 1) % b->numpoints;
			afterB = (j + 1) % b->numpoints;
			if ( PointsNearLine( b->p[beforeB], pointB, b->p[afterB], 0.0625f )
				|| WindingHasCrossingEdge( a, pointA, pointB, axisV, axisU )
				|| WindingHasCrossingEdge( b, pointA, pointB, axisV, axisU ) ) continue;
			if ( TrisBridgeCutsWindingWedge( a, pointB, pointA, normal, a->p[afterA] )
				|| TrisBridgeCutsWindingWedge( a, a->p[beforeA], pointA, normal, pointB )
				|| TrisBridgeCutsWindingWedge( b, b->p[beforeB], pointB, normal, pointA )
				|| TrisBridgeCutsWindingWedge( b, pointA, pointB, normal, b->p[afterB] ) ) continue;
			bestDistance = distance;
			*newA = i;
			*newB = j;
			found = qtrue;
		}
	}
	if ( found ) {
		for ( i = 1; i <= a->numpoints; i++ ) {
			int sourceIndex = (i + *newA) % a->numpoints;
			VectorCopy( a->p[sourceIndex], w->p[i-1] );
		}
		for ( i = 0; i <= b->numpoints; i++ ) {
			int sourceIndex = (i + *newB) % b->numpoints;
			VectorCopy( b->p[sourceIndex], w->p[a->numpoints+i] );
		}
		w->numpoints = a->numpoints + b->numpoints + 2;
		VectorCopy( a->p[*newA], w->p[w->numpoints-1] );
	}
	FreeWinding( b );
	FreeWinding( a );
	return found;
}


/*
=============
TrisRepairTouchingWinding
=============
*/
void TrisRepairTouchingWinding( winding_t *w, float *normal ) {
	int startA, startB, endA, endB, newA, newB;
	int pass, count, lengthA, lengthB, temporary;
	if ( w->numpoints <= 8 ) return;
	pass = 0;
	while ( TrisFindCoincidentRun( w, &startA, &startB, &endA, &endB ) ) {
		if ( pass >= 32 ) break;
		count = endA - startA;
		if ( count < 0 ) count += w->numpoints;
		if ( pass && count == 1 && startB == w->numpoints - 1 ) break;
		if ( startA == endA || startA == endB || startB == endA || startB == endB ) break;
		lengthA = endB - endA;
		if ( lengthA < 0 ) lengthA += w->numpoints;
		lengthB = startA - startB;
		if ( lengthB < 0 ) lengthB += w->numpoints;
		if ( lengthA > lengthB ) {
			temporary = startA; startA = endB; endB = temporary;
			temporary = startB; startB = endA; endA = temporary;
		}
		if ( !TrisReconnectWindingArcs( w, normal, startB, startA, endA, endB, &newA, &newB ) ) break;
		pass++;
	}
}


/*
=============
CanRemoveVert

Tests whether replacing a winding chain with the anchor-to-vertex shortcut
would introduce a crossing edge.  The nextIdx argument is overwritten before
it is used.
=============
*/
static qboolean CanRemoveVert( int vertIdx, winding_t *w, int anchorIdx,
	int nextIdx, int axisU, int axisV ) {
	int testIdx;

	nextIdx = ( vertIdx + 1 ) % w->numpoints;
	testIdx = ( vertIdx + 2 ) % w->numpoints;

	while ( testIdx != anchorIdx ) {
		if ( WindingEdgesIntersect( w->p[anchorIdx], w->p[vertIdx],
			w->p[nextIdx], w->p[testIdx], axisU, axisV ) ) {
			return qfalse;
		}
		nextIdx = testIdx;
		testIdx = ( testIdx + 1 ) % w->numpoints;
	}

	return qtrue;
}


/*
=============
WindingPointsNear

Squared-distance test used by CollapseCollinearVerts.
=============
*/
static qboolean WindingPointsNear( float *pointA, float *pointB,
	float epsilon ) {
	return VectorDistanceSquared( pointA, pointB ) < epsilon * epsilon;
}


/*
=============
CollapseCollinearVerts

Removes a middle winding point only when it is collinear/near and the shortcut
passes CanRemoveVert.
=============
*/
static void CollapseCollinearVerts( float *normal, winding_t *w ) {
	int anchorIdx;
	int followingIdx;
	int endpointIdx;
	int dominantAxis;
	int axisU;
	int axisV;
	int nextIdx;
	int testIdx;
	float *anchor;

	dominantAxis = 0;
	if ( fabs( normal[1] ) > fabs( normal[0] ) ) dominantAxis = 1;
	if ( fabs( normal[2] ) > fabs( normal[dominantAxis] ) ) {
		dominantAxis = 2;
	}
	axisU = ( ~dominantAxis ) & 2;
	axisV = ( ~dominantAxis ) & 1;

	anchorIdx = 0;
	if ( anchorIdx >= w->numpoints ) return;
	endpointIdx = 2;
	followingIdx = 1;
	anchor = w->p[0];
	while ( anchorIdx < w->numpoints ) {
		nextIdx = followingIdx % w->numpoints;
		testIdx = endpointIdx % w->numpoints;

		if (  ( PointsNearLine( anchor, w->p[nextIdx], w->p[testIdx],
				TRIS_MERGECONCAVE_COLLINEAR_VERTEX_EPSILON )
				|| WindingPointsNear( w->p[nextIdx], w->p[testIdx],
					TRIS_MERGECONCAVE_COLLINEAR_VERTEX_EPSILON ) )
			&& CanRemoveVert( testIdx, w, anchorIdx, nextIdx, axisU, axisV )  ) {
			if ( testIdx ) {
				memmove( w->p[nextIdx], w->p[testIdx],
					sizeof( vec3_t ) * ( w->numpoints - testIdx ) );
			}
			w->numpoints--;
		} else {
			anchorIdx++;
			followingIdx++;
			endpointIdx++;
			anchor += 3;
		}
	}
	/* disabled debug check, as in the original build; it keeps these counters in memory */
	if ( TRIS_DEBUG_COLLAPSE ) {
		memmove( &anchorIdx, &endpointIdx, sizeof( anchorIdx ) );
	}
}


/*
=============
SubdivideThinWinding
=============
*/
static winding_t *SubdivideThinWinding( winding_t *w ) {
	int vertIdx;
	int prevIdx;
	int posOut;
	int subCount;
	int subIdx;
	float minAdjacentLength;
	vec3_t delta;
	float edgeLengths[MAX_THIN_WINDING_POINTS + 1];
	vec3_t pointsOut[MAX_THIN_WINDING_POINTS];

	prevIdx = w->numpoints - 1;
	for ( vertIdx = 0; vertIdx < w->numpoints; ++vertIdx ) {
		VectorSubtract( w->p[vertIdx], w->p[prevIdx], delta );
		edgeLengths[vertIdx] = DotProduct( delta, delta );
		prevIdx = vertIdx;
	}

	posOut = 0;
	edgeLengths[w->numpoints] = edgeLengths[0];
	c_thinWindingInputPoints += w->numpoints;
	prevIdx = w->numpoints - 1;

	for ( vertIdx = 0; vertIdx < w->numpoints; ++vertIdx ) {
		minAdjacentLength = edgeLengths[prevIdx] < edgeLengths[vertIdx + 1]
			? edgeLengths[prevIdx]
			: edgeLengths[vertIdx + 1];

		if ( edgeLengths[vertIdx] > 16384.0f * minAdjacentLength ) {
			subCount = CeilToInt( sqrt( edgeLengths[vertIdx] /
				(16384.0f * minAdjacentLength) ) );
			if ( w->numpoints - vertIdx + subCount + posOut > MAX_THIN_WINDING_POINTS ) {
				return w;
			}

			++subCount;
			for ( subIdx = 1; subIdx < subCount; ++subIdx ) {
				pointsOut[posOut][0] =
					(subIdx * w->p[vertIdx][0] +
					 (subCount - subIdx) * w->p[prevIdx][0]) / subCount;
				pointsOut[posOut][1] =
					(subIdx * w->p[vertIdx][1] +
					 (subCount - subIdx) * w->p[prevIdx][1]) / subCount;
				pointsOut[posOut][2] =
					(subIdx * w->p[vertIdx][2] +
					 (subCount - subIdx) * w->p[prevIdx][2]) / subCount;
				++posOut;
			}
		}

		VectorCopy( w->p[vertIdx], pointsOut[posOut] );
		++posOut;
		prevIdx = vertIdx;
	}

	if ( posOut != w->numpoints ) {
		c_addedThinWindingPoints += posOut - w->numpoints;
		FreeWinding( w );
		w = AllocWinding( posOut );
		w->numpoints = posOut;
		memcpy( w->p, pointsOut, posOut * sizeof(vec3_t) );
	}
	return w;
}


/*
=============
BoundsCoincideOnAxis

Compare the grid cells occupied by both ends of one bounds axis.
=============
*/
static qboolean BoundsCoincideOnAxis( const triSurf_t *a,
	const triSurf_t *b, int axis, float gridSize ) {
	float inverseSize;
	inverseSize = 1.0f / gridSize;
	if ( FloorToInt( ( a->mins[axis] + 0.002f ) * inverseSize ) !=
		FloorToInt( ( b->mins[axis] + 0.002f ) * inverseSize ) )
		return qfalse;
	return CeilToInt( ( a->maxs[axis] - 0.002f ) * inverseSize ) ==
		CeilToInt( ( b->maxs[axis] - 0.002f ) * inverseSize );
}


/*
=============
CheckWindingSeparation

Test the separating planes formed by the edges of one winding.
=============
*/
static int CheckWindingSeparation( winding_t *w, winding_t *testW, vec3_t normal ) {
    int i, next;
    vec3_t edgeDir, clipNormal;
    float clipDist;
    double dot;
    float *current;
    if ( w->numpoints > 0 ) {
        i = 1;
        /* the original walks a float pointer through the winding points; plain indexing behaves the same */
        current = &w->p[0][1];
        do {
            next = i % w->numpoints;
            edgeDir[0] = w->p[next][0] - current[-1];
            edgeDir[1] = w->p[next][1] - current[0];
            edgeDir[2] = w->p[next][2] - current[1];
            CrossProduct( normal, edgeDir, clipNormal );
            VectorNormalizeFast( clipNormal );
            dot = clipNormal[1] * current[0];
            dot += clipNormal[2] * current[1];
            clipDist = dot + clipNormal[0] * current[-1];
            if ( WindingOnPlaneSide( testW, clipNormal, clipDist ) == SIDE_FRONT ) return 1;
            i++;
            current += 3;
        } while ( i - 1 < w->numpoints );
    }
    return 0;
}


/*
=============
CheckWindingSeparationBoth
=============
*/
static int CheckWindingSeparationBoth( winding_t *w1, winding_t *w2, vec3_t normal ) {
	if ( !CheckWindingSeparation( w1, w2, normal )
		&& !CheckWindingSeparation( w2, w1, normal ) ) {
		return 0;
	}
	return 1;
}


/*
=============
AddWindingWithoutOverlap_r

Keep only the portions not already covered by the preceding windings.
=============
*/
int AddWindingWithoutOverlap_r( triSurf_t *surfs, int start, int count,
							  int limit, winding_t *w, triSurfProps_t *props ) {
	int i, e;
	size_t currentVertex;
	triSurf_t *surf;
	winding_t *front, *back;
	vec3_t edgeDir, clipNormal;
	float clipDist;

	for ( i = start; i < limit; i++ ) {
		surf = &surfs[i];
		if ( !CheckWindingSeparationBoth( surf->winding, w, surf->props->plane )
			&& surf->winding->numpoints > 0 ) {
			currentVertex = 0;
			e = 1;
			do {
				VectorSubtract( surf->winding->p[e % surf->winding->numpoints], surf->winding->p[currentVertex], edgeDir );
				CrossProduct( surf->props->plane, edgeDir, clipNormal );
				VectorNormalize( clipNormal );
				clipDist = DotProduct( clipNormal, surfs[i].winding->p[currentVertex] );
				ClipWindingEpsilon( w, clipNormal, clipDist, ON_EPSILON, &front, &back, qfalse );
				if ( front ) {
					if ( back ) {
						FreeWinding( w );
						count = AddWindingWithoutOverlap_r( surfs, start + 1, count, limit, front, props );
						w = back;
					} else {
						FreeWinding( w );
						w = front;
						break;
					}
				} else if ( back ) {
					FreeWinding( w );
					w = back;
					if ( e == surf->winding->numpoints ) {
						FreeWinding( w );
						return count;
					}
				} else {
					Error( "code bug: AddWindingWithoutOverlap_r: winding disappeared\n" );
				}
				e++;
				currentVertex++;
			} while ( e - 1 < surf->winding->numpoints );
		}
	}
	if ( count == MAX_WINDINGS_PER_CONCAVE_GROUP ) {
		Error( "MAX_WINDINGS_PER_CONCAVE_GROUP (%i) exceeded\n", MAX_WINDINGS_PER_CONCAVE_GROUP );
	}
	ClearBounds( surfs[count].mins, surfs[count].maxs );
	for ( i = 0; i < w->numpoints; i++ ) {
		AddPointToBounds( w->p[i], surfs[count].mins, surfs[count].maxs );
	}
	surfs[count].props = props;
	surfs[count].winding = w;
	surfs[count].mins[0] -= MERGE_BOUNDS_EPSILON;
	surfs[count].mins[1] -= MERGE_BOUNDS_EPSILON;
	surfs[count].mins[2] -= MERGE_BOUNDS_EPSILON;
	surfs[count].maxs[0] += MERGE_BOUNDS_EPSILON;
	surfs[count].maxs[1] += MERGE_BOUNDS_EPSILON;
	surfs[count].maxs[2] += MERGE_BOUNDS_EPSILON;
	return count + 1;
}


/*
=============
CompareSurfaces

Orders the temporary concave-winding surfaces by their minimum bounds.
=============
*/
int CompareSurfaces( const void *surfA, const void *surfB ) {
	const triSurf_t *a;
	const triSurf_t *b;

	a = (const triSurf_t *)surfA;
	b = (const triSurf_t *)surfB;

	if ( a->mins[0] < b->mins[0] ) {
		return -1;
	}
	if ( a->mins[0] > b->mins[0] ) {
		return 1;
	}
	if ( a->mins[1] < b->mins[1] ) {
		return -1;
	}
	if ( a->mins[1] > b->mins[1] ) {
		return 1;
	}
	if ( a->mins[2] < b->mins[2] ) {
		return -1;
	}
	if ( a->mins[2] > b->mins[2] ) {
		return 1;
	}
	return 0;
}


/*
=============
MergeConcaveWindingGroup

Join reverse shared arcs after clipping away overlapping area.
=============
*/
static int MergeConcaveWindingGroup( triSurf_t **surfaces, int groupCount,
    int tailCount, int freeSlots ) {
    triSurf_t work[16384], swap;
    triSurf_t *a, *b;
    winding_t *wa, *wb, *joined;
    int count, i, j, av, bv, shared, shortest;
    int pointIndex;
    int scanA, scanB, k, out, changed, openArc, before, after;
    float gridSize = 0;

    count = 0;
    for ( k = 0; k < groupCount; k++ )
        count = AddWindingWithoutOverlap_r( work, 0, count, count,
            surfaces[k]->winding, surfaces[k]->props );
    qsort( work, count, sizeof( work[0] ), CompareSurfaces );
    FixTriSurfaceTJunctions( work, count );
    do {
        changed = 0;
        i = 0;
        for ( ; i < count; i++ ) {
restart:
            a = &work[i];
            wa = a->winding;
            for ( j = i + 1; j < count; j++ ) {
                b = &work[j];
                wb = b->winding;
                if ( a->mins[0] >= b->maxs[0] || a->maxs[0] <= b->mins[0]
                    || a->mins[1] >= b->maxs[1] || a->maxs[1] <= b->mins[1]
                    || a->mins[2] >= b->maxs[2] || a->maxs[2] <= b->mins[2] )
                    continue;
                gridSize = a->props->shaderInfo->subdivisions;
                if ( gridSize != 0.0f &&
                    ( !BoundsCoincideOnAxis( a, b, 0, gridSize )
                      || !BoundsCoincideOnAxis( a, b, 1, gridSize )
                      || !BoundsCoincideOnAxis( a, b, 2, gridSize ) ) )
                    continue;
                for ( av = 0; av < wa->numpoints; av++ ) {
                    for ( bv = 0; bv < wb->numpoints; bv++ ) {
                        if ( !WindingPointsNear( wa->p[av], wb->p[bv], tjuncTolerance ) )
                            continue;
                        shortest = wa->numpoints;
                        if ( shortest >= wb->numpoints ) shortest = wb->numpoints;
                        shared = 1;
                        if ( av == 0 ) {
                            do {
                                scanA = ( wa->numpoints - shared ) % wa->numpoints;
                                scanB = ( shared + bv ) % wb->numpoints;
                                if ( !WindingPointsNear( wa->p[scanA], wb->p[scanB], tjuncTolerance ) )
                                    break;
                                shared++;
                            } while ( shared < shortest );
                            if ( shared > 1 ) {
                                bv = scanB;
                                av = scanA;
                                if ( shared < shortest ) {
                                    av++;
                                    if ( !bv ) bv = wb->numpoints;
                                    bv--;
                                }
                            }
                        }
                        for ( ; shared < shortest; shared++ ) {
                            scanA = ( av + shared ) % wa->numpoints;
                            scanB = ( bv + wb->numpoints - shared ) % wb->numpoints;
                            if ( !WindingPointsNear( wa->p[scanA], wb->p[scanB], tjuncTolerance ) )
                                break;
                        }
                        if ( shared == 1 ) continue;
                        if ( shared == wa->numpoints ) {
                            swap = *a;
                            work[i] = *b;
                            work[j] = swap;
                            a = &work[i];
                            b = &work[j];
                            joined = wa; wa = wb; wb = joined;
                        }
                        if ( shared == wb->numpoints ) {
                            openArc = 0;
                            if ( !WindingPointsNear( wa->p[av], wa->p[( shared + av ) % wa->numpoints], tjuncTolerance ) ) {
                                k = av + 1;
                                openArc = 1;
                                if ( k == wa->numpoints ) k = 0;
                                shared -= 2;
                            } else k = av;
                            if ( shared + k >= wa->numpoints ) {
                                if ( shared + k > wa->numpoints ) {
                                out = ( shared + k ) % wa->numpoints;
                                memmove( wa->p[0], wa->p[out], ( wa->numpoints - out ) * sizeof( vec3_t ) );
                                k = 0;
                                }
                            } else {
                                memmove( wa->p[k], wa->p[shared + k],
                                    ( wa->numpoints - shared - k ) * sizeof( vec3_t ) );
                            }
                            wa->numpoints -= shared;
                            if ( !openArc ) {
                                before = ( wa->numpoints + k - 1 ) % wa->numpoints;
                                after = ( k + 1 ) % wa->numpoints;
                                while ( WindingPointsNear( wa->p[before], wa->p[after], tjuncTolerance ) ) {
                                    if ( !after ) k = 0;
                                    else if ( k + 2 == wa->numpoints ) k = before;
                                    else {
                                        memmove( wa->p[k], wa->p[k + 2],
                                            ( wa->numpoints - k + 2 ) * sizeof( vec3_t ) );
                                        if ( k < before ) k = before - 2;
                                        else k = before;
                                    }
                                    wa->numpoints -= 2;
                                    before = ( wa->numpoints + k - 1 ) % wa->numpoints;
                                    after = ( k + 1 ) % wa->numpoints;
                                }
                            }
                            FreeWinding( wb );
                            count--;
                            memmove( &work[j], &work[j + 1], ( count - j ) * sizeof( work[0] ) );
                            changed = 1;
                            goto restart;
                        } else {
                            joined = AllocWinding( wb->numpoints - 2 * shared + wa->numpoints + 2 );
                            out = 0;
                            for ( k = shared - 1; k < wa->numpoints; k++ ) {
                                pointIndex = ( av + k ) % wa->numpoints;
                                VectorCopy( wa->p[pointIndex], joined->p[out] );
                                out++;
                            }
                            for ( k = 0; k <= wb->numpoints - shared; k++, out++ ) {
                                pointIndex = ( bv + k ) % wb->numpoints;
                                VectorCopy( wb->p[pointIndex], joined->p[out] );
                            }
                            joined->numpoints = out;
                            work[i].mins[0] = TRIS_MIN( a->mins[0], b->mins[0] );
                            work[i].mins[1] = TRIS_MIN( a->mins[1], b->mins[1] );
                            work[i].mins[2] = TRIS_MIN( a->mins[2], b->mins[2] );
                            work[i].maxs[0] = TRIS_MAX( a->maxs[0], b->maxs[0] );
                            work[i].maxs[1] = TRIS_MAX( a->maxs[1], b->maxs[1] );
                            work[i].maxs[2] = TRIS_MAX( a->maxs[2], b->maxs[2] );
                            FreeWinding( wa );
                            FreeWinding( wb );
                            work[i].winding = joined;
                            count--;
                            memmove( &work[j], &work[j + 1], ( count - j ) * sizeof( work[0] ) );
                            changed = 1;
                            goto restart;
                        }
                    }
                }
            }
        }
    } while ( changed );
    for ( i = 0; i < groupCount; i++ ) free( surfaces[i] );
    if ( count != groupCount ) {
        if ( count - groupCount > freeSlots ) Error( "MAX_WINDINGS_PER_CELL exceeded\n" );
        memmove( &surfaces[count], &surfaces[groupCount], tailCount * sizeof( surfaces[0] ) );
    }
    for ( i = 0; i < count; i++ ) {
        surfaces[i] = malloc( sizeof( work[0] ) );
        *surfaces[i] = work[i];
    }
    return count;
}


/*
=============
MergeVisGroupCell
=============
*/
int MergeVisGroupCell( int cellIndex ) {
	triSurf_t *surfaces[MAX_WINDINGS_PER_CELL];
	triSurf_t *surface;
	int surfaceCount;
	int groupStart;
	int groupEnd;
	int mergedCount;
	int surfaceIndex;

	surfaceCount = 0;
	for ( surface = triSurfCellHeads[cellIndex]; surface;
		surface = surface->next ) {
		if ( surfaceCount == MAX_WINDINGS_PER_CELL ) {
			Error( "MAX_WINDINGS_PER_CELL" );
		}
		surfaces[surfaceCount++] = surface;
	}

	qsort( surfaces, surfaceCount, sizeof( surfaces[0] ),
		TriSurfPointerCompare );

	groupStart = 0;
	while ( groupStart < surfaceCount ) {
		groupEnd = groupStart + 1;
		while ( groupEnd < surfaceCount
			&& surfaces[groupEnd]->props == surfaces[groupStart]->props
			&& groupEnd - groupStart < MAX_WINDINGS_PER_GROUP ) {
			++groupEnd;
		}

		if ( groupEnd - groupStart != 1 ) {
			mergedCount = MergeConcaveWindingGroup(
				&surfaces[groupStart], groupEnd - groupStart,
				surfaceCount - groupEnd,
				MAX_WINDINGS_PER_CELL - surfaceCount );
			if ( groupStart + mergedCount != groupEnd ) {
				surfaceCount += groupStart + mergedCount - groupEnd;
				groupEnd = groupStart + mergedCount;
			}
		}
		groupStart = groupEnd;
	}

	triSurfCellHeads[cellIndex] = NULL;
	for ( surfaceIndex = 0; surfaceIndex < surfaceCount; ++surfaceIndex ) {
		TrisRepairTouchingWinding( surfaces[surfaceIndex]->winding, surfaces[surfaceIndex]->props->plane );
		CollapseCollinearVerts( surfaces[surfaceIndex]->props->plane, surfaces[surfaceIndex]->winding );
		if ( surfaces[surfaceIndex]->winding->numpoints < 3 ) {
			FreeWinding( surfaces[surfaceIndex]->winding );
		} else {
			surfaces[surfaceIndex]->winding = SubdivideThinWinding( surfaces[surfaceIndex]->winding );
			surfaces[surfaceIndex]->next = triSurfCellHeads[cellIndex];
			triSurfCellHeads[cellIndex] = surfaces[surfaceIndex];
		}
	}

	return surfaceCount;
}


/*
=============
MergeVisGroupCells
=============
*/
void MergeVisGroupCells( void ) {
	int cell;
	int cellCount;

	cellCount = TrisGetCellListCount();
	for ( cell = 0; cell < cellCount; cell++ ) {
		MergeVisGroupCell( cell );
	}
}


/*
=============
RemoveDegenerateEdges

Removes zero-length edges from a cyclic winding.  A duplicate final/first
pair backs the cursor up so the newly adjacent predecessor is checked again.
=============
*/
static void RemoveDegenerateEdges( winding_t *w ) {
	int i;
	int next;

	i = 0;
	while ( i < w->numpoints ) {
		next = ( i + 1 ) % w->numpoints;
		if ( VectorCompareEpsilon( w->p[i], w->p[next] ) ) {
			if ( !next ) {
				i--;
			}
			else {
				memmove( w->p[i], w->p[next],
					( w->numpoints - next ) * sizeof( vec3_t ) );
			}
			w->numpoints--;
		}
		else {
			i++;
		}
	}
}


/*
=============
TJunc_MarkIntersecting

Mark surfaces intersecting a cell region, subject to shader exclusions.
=============
*/
static int TJunc_MarkIntersecting( const vec3_t mins, const vec3_t maxs,
	int limit ) {
	int cell;
	int markedCount;
	int cellCount;

	cellCount = TrisGetCellListCount();
	markedCount = 0;
	TJunc_ResetCounters();
	for ( cell = 0; cell < cellCount; cell++ ) {
		triSurf_t *surface;

		if ( !TJUNC_BOUNDS_INTERSECT( tjuncCellBounds[cell].mins,
			tjuncCellBounds[cell].maxs, mins, maxs ) ) {
			continue;
		}
		for ( surface = triSurfCellHeads[cell]; surface;
			surface = surface->next ) {
			shaderInfo_t *shader;

			surface->marked = qfalse;
			shader = surface->props->shaderInfo;
			if ( shader->notjunc ||
				( shader->surfaceFlags & TJUNC_SKIP_SURFACE_FLAGS ) ) {
				continue;
			}
			if ( !TJUNC_BOUNDS_INTERSECT( surface->mins, surface->maxs,
				mins, maxs ) ) {
				continue;
			}
			surface->marked = qtrue;
			markedCount++;
			if ( markedCount > limit ) {
				return markedCount;
			}
		}
	}
	return markedCount;
}


/*
=============
TJunc_ProcessBounds

Subdivide dense regions; otherwise insert and repair winding edges.
=============
*/
void TJunc_ProcessBounds( const vec3_t mins, const vec3_t maxs ) {
	int count;
	int limit;
	int axis;
	int cellCount;
	int cell;
	vec3_t midpoint;

	cellCount = TrisGetCellListCount();
    {
        VectorSubtract( maxs, mins, midpoint );
        limit = midpoint[0] >= TJUNC_OCTREE_MIN_EXTENT || midpoint[1] >= TJUNC_OCTREE_MIN_EXTENT
            || midpoint[2] >= TJUNC_OCTREE_MIN_EXTENT ? TJUNC_OCTREE_MAX_SURFACES : INT_MAX;
    }
	TJunc_ResetCounters();
	count = TJunc_MarkIntersecting( mins, maxs, limit );
	if ( count <= 1 ) {
		return;
	}

	if ( count > limit ) {
		int octant;

        VectorAdd( mins, maxs, midpoint );
        VectorScale( midpoint, 0.5f, midpoint );
		for ( octant = 0; octant < 8; octant++ ) {
			vec3_t childMaxs;
			vec3_t childMins;

            if ( octant & 1 ) { childMaxs[0] = midpoint[0]; childMins[0] = mins[0]; }
            else { childMaxs[0] = maxs[0]; childMins[0] = midpoint[0]; }
            if ( octant & 2 ) { childMaxs[1] = midpoint[1]; childMins[1] = mins[1]; }
            else { childMaxs[1] = maxs[1]; childMins[1] = midpoint[1]; }
            if ( octant & 4 ) { childMins[2] = mins[2]; childMaxs[2] = midpoint[2]; }
            else { childMins[2] = midpoint[2]; childMaxs[2] = maxs[2]; }
			TJunc_ProcessBounds( childMins, childMaxs );
		}
		return;
	}

	for ( cell = 0; cell < cellCount; cell++ ) {
		triSurf_t *surface;

		if ( !TJUNC_BOUNDS_INTERSECT( tjuncCellBounds[cell].mins,
			tjuncCellBounds[cell].maxs, mins, maxs ) ) {
			continue;
		}
		for ( surface = triSurfCellHeads[cell]; surface;
			surface = surface->next ) {
			int vertex;

			if ( !surface->marked ) {
				continue;
			}
			for ( vertex = 0; vertex < surface->winding->numpoints;
				vertex++ ) {
				TJunc_AddEdge( surface->winding->p[vertex],
					surface->winding->p[( vertex + 1 ) %
						surface->winding->numpoints] );
			}
		}
	}

	for ( cell = 0; cell < cellCount; cell++ ) {
		triSurf_t *surface;

		if ( !TJUNC_BOUNDS_INTERSECT( tjuncCellBounds[cell].mins,
			tjuncCellBounds[cell].maxs, mins, maxs ) ) {
			continue;
		}
		for ( surface = triSurfCellHeads[cell]; surface;
			surface = surface->next ) {
			winding_t *fixed;

			if ( !surface->marked ) {
				continue;
			}
			fixed = TJunc_FixWinding( surface->winding );
			surface->winding = fixed;
			RemoveDegenerateEdges( fixed );
		}
	}
	TJunc_FreeHash();
}


/*
=============
TJunc_FixAll

Prepare per-cell and global surface bounds, then process the tree.
=============
*/
void TJunc_FixAll( void ) {
	vec3_t mins;
	vec3_t maxs;
	int cellCount;
	int cell;

	cellCount = TrisGetCellListCount();
	ClearBounds( mins, maxs );
	for ( cell = 0; cell < cellCount; cell++ ) {
		triSurf_t *surface;

		ClearBounds( tjuncCellBounds[cell].mins,
			tjuncCellBounds[cell].maxs );
		for ( surface = triSurfCellHeads[cell]; surface;
			surface = surface->next ) {
			shaderInfo_t *shader;
			int vertex;
			int axis;

			shader = surface->props->shaderInfo;
			if ( shader->notjunc ||
				( shader->surfaceFlags & TJUNC_SKIP_SURFACE_FLAGS ) ) {
				continue;
			}
			ClearBounds( surface->mins, surface->maxs );
			for ( vertex = 0; vertex < surface->winding->numpoints;
				vertex++ ) {
				AddPointToBounds( surface->winding->p[vertex],
					surface->mins, surface->maxs );
			}
			for ( axis = 0; axis < 3; axis++ ) {
				surface->mins[axis] -= 2.0f * tjuncTolerance;
				surface->maxs[axis] += 2.0f * tjuncTolerance;
			}
			ExpandBounds( surface->mins, surface->maxs,
				tjuncCellBounds[cell].mins,
				tjuncCellBounds[cell].maxs );
		}
		ExpandBounds( tjuncCellBounds[cell].mins,
			tjuncCellBounds[cell].maxs, mins, maxs );
	}

	tjuncTolerance = 0.3125f;
	TJunc_ProcessBounds( mins, maxs );
	tjuncTolerance = 0.25f;
}


/*
=============
TrisGenerateVerticesFromWinding

Generate the working vertices before coordinate snapping.
=============
*/
static void TrisGenerateVerticesFromWinding( triSurf_t *surface ) {
	triIntermediateVert_t *vertex;
	int i;

	for ( i = 0; i < surface->winding->numpoints; i++ ) {
		vertex = &triIntermediateVerts[numTriIntermediateVerts++];
		VectorCopy( surface->winding->p[i], vertex->xyz );
		VectorCopy( surface->winding->p[i], vertex->originalPos );
		vertex->st[0] = DotProduct( vertex->xyz, surface->props->texVecs[0] ) +
			surface->props->texVecs[0][3];
		vertex->st[1] = DotProduct( vertex->xyz, surface->props->texVecs[1] ) +
			surface->props->texVecs[1][3];
		vertex->lightmap[0] = DotProduct( vertex->xyz, surface->props->lmapVecs[0] ) +
			surface->props->lmapVecs[0][3];
		vertex->lightmap[1] = DotProduct( vertex->xyz, surface->props->lmapVecs[1] ) +
			surface->props->lmapVecs[1][3];
		VectorCopy( surface->props->plane, vertex->normal );
		if ( vertex->lightmap[0] < 0.0f ) vertex->lightmap[0] = 0.0f;
		else if ( vertex->lightmap[0] > 1.0f ) vertex->lightmap[0] = 1.0f;
		if ( vertex->lightmap[1] < 0.0f ) vertex->lightmap[1] = 0.0f;
		else if ( vertex->lightmap[1] > 1.0f ) vertex->lightmap[1] = 1.0f;
	}
}


/*
=============
TrisGenerateAllVertices

Visit every surface in the current cell/cull-group lists.
=============
*/
void TrisGenerateAllVertices( void ) {
	triSurf_t *surface;
	int i, cellCount;

	cellCount = TrisGetCellListCount();
	for ( i = 0; i < cellCount; i++ ) {
		for ( surface = triSurfCellHeads[i]; surface; surface = surface->next ) {
			TrisGenerateVerticesFromWinding( surface );
		}
	}
}


/*
=============
HashVertexGridCell
=============
*/
static int HashVertexGridCell( int x, int y, int z ) {
	return ( 7731 * x - 7737 * y - 911 * z ) & 16383;
}


/*
=============
HashVertexPosition
=============
*/
static int HashVertexPosition( const vec3_t position ) {
	int x, y, z;
	x = RoundToInt( position[0] );
	y = RoundToInt( position[1] );
	z = RoundToInt( position[2] );
	return HashVertexGridCell( x, y, z );
}


/*
=============
BuildTriangleVertexRemap
=============
*/
void BuildTriangleVertexRemap( void ) {
	int i, x, y, z, other, hash;
	int cell[3];
	float bestDistance;
	double dx, dy, dz, distance;

	memset( triangleVertexHash, -1, sizeof( triangleVertexHash ) );
	for ( i = 0; i < numTriIntermediateVerts; i++ ) {
		hash = HashVertexPosition( triIntermediateVerts[i].xyz );
		triIntermediateVerts[i].hashNext = triangleVertexHash[hash];
		triangleVertexHash[hash] = i;
		triangleVertexRemap[i] = i;
		bestDistance = 0.0625f;
		cell[0] = FloorToInt( triIntermediateVerts[i].xyz[0] );
		cell[1] = FloorToInt( triIntermediateVerts[i].xyz[1] );
		cell[2] = FloorToInt( triIntermediateVerts[i].xyz[2] );
		for ( z = 0; z <= 1; z++ ) {
			for ( y = 0; y <= 1; y++ ) {
				for ( x = 0; x <= 1; x++ ) {
					hash = HashVertexGridCell( cell[0] + x, cell[1] + y, cell[2] + z );
					for ( other = triangleVertexHash[hash]; other >= 0;
						other = triIntermediateVerts[other].hashNext ) {
						if ( i == other ) continue;
						dx = triIntermediateVerts[other].xyz[0] - triIntermediateVerts[i].xyz[0];
						dy = triIntermediateVerts[other].xyz[1] - triIntermediateVerts[i].xyz[1];
						dz = triIntermediateVerts[other].xyz[2] - triIntermediateVerts[i].xyz[2];
						distance = dz * dz + dy * dy + dx * dx;
						if ( bestDistance >= distance ) {
							bestDistance = distance;
							triangleVertexRemap[i] = triangleVertexRemap[other];
						}
					}
				}
			}
		}
	}
}


/*
=============
ApplyMergeMap

Average each merged vertex cluster, snap its representative to the quarter
unit grid, and use that position for every member of the cluster.
=============
*/
void ApplyMergeMap( void ) {
	vec3_t *positions;
	int *counts;
	int i;

	positions = malloc( numTriIntermediateVerts * sizeof( *positions ) );
	memset( positions, 0, numTriIntermediateVerts * sizeof( *positions ) );
	counts = malloc( numTriIntermediateVerts * sizeof( *counts ) );
	memset( counts, 0, numTriIntermediateVerts * sizeof( *counts ) );
	for ( i = numOriginalDrawVerts; i < numTriIntermediateVerts; i++ ) {
		VectorAdd( triIntermediateVerts[i].xyz,
			positions[triangleVertexRemap[i]], positions[triangleVertexRemap[i]] );
		counts[triangleVertexRemap[i]]++;
	}
	for ( i = numOriginalDrawVerts; i < numTriIntermediateVerts; i++ ) {
		if ( triangleVertexRemap[i] == i ) {
			VectorScale( positions[i], ( 1.0f / counts[i] ), positions[i] );
			positions[i][0] = RoundToInt( positions[i][0] * 4.0f ) * 0.25f;
			positions[i][1] = RoundToInt( positions[i][1] * 4.0f ) * 0.25f;
			positions[i][2] = RoundToInt( positions[i][2] * 4.0f ) * 0.25f;
		}
		VectorCopy( positions[triangleVertexRemap[i]], triIntermediateVerts[i].xyz );
	}
	free( counts );
	free( positions );
}


/*
=============
MergeTriangleVertexRepresentatives
=============
*/
static void MergeTriangleVertexRepresentatives( int first, int second ) {
	int i, temporary;
	if ( second < first ) {
		temporary = first;
		first = second;
		second = temporary;
	}
	for ( i = second; i < numTriIntermediateVerts; i++ ) {
		if ( triangleVertexRemap[i] == second ) {
			triangleVertexRemap[i] = first;
		}
	}
}


/*
=============
TrisSnapNearbyVertices
=============
*/
void TrisSnapNearbyVertices( void ) {
	int cell, cellCount, firstVertex, i, j, axisU, axisV;
	float dx, dy;
	triSurf_t *surface;
	cellCount = TrisGetCellListCount();
	firstVertex = numOriginalDrawVerts;
	for ( cell = 0; cell < cellCount; cell++ ) {
		for ( surface = triSurfCellHeads[cell]; surface; surface = surface->next ) {
			GetProjectionAxes( surface->props->plane, &axisU, &axisV );
			for ( i = 0; i < surface->winding->numpoints; i++ ) {
				for ( j = i + 1; j < surface->winding->numpoints; j++ ) {
					if ( triangleVertexRemap[firstVertex + i] ==
						triangleVertexRemap[firstVertex + j] ) { continue; }
					dx = triIntermediateVerts[firstVertex + i].xyz[axisU] -
						triIntermediateVerts[firstVertex + j].xyz[axisU];
					dy = triIntermediateVerts[firstVertex + i].xyz[axisV] -
						triIntermediateVerts[firstVertex + j].xyz[axisV];
					if ( dx * dx + dy * dy < 0.1250625f ) {
						MergeTriangleVertexRepresentatives(
							triangleVertexRemap[firstVertex + i],
							triangleVertexRemap[firstVertex + j] );
					}
				}
			}
			firstVertex += surface->winding->numpoints;
		}
	}
}


/*
=============
RebuildTriangleWindings

Rebuild windings after representative vertices have been merged.
=============
*/
void RebuildTriangleWindings( void ) {
	triSurf_t *surface, **link;
	int cell, cellCount, vertexIndex, i, next, count;

	cellCount = TrisGetCellListCount();
	vertexIndex = numOriginalDrawVerts;
	for ( cell = 0; cell < cellCount; cell++ ) {
		link = &triSurfCellHeads[cell];
		while ( ( surface = *link ) != NULL ) {
			surface->originalWinding = AllocWinding( surface->winding->numpoints );
			count = 0;
			for ( i = 0; i < surface->winding->numpoints; i++ ) {
				next = ( i + 1 ) % surface->winding->numpoints;
				if ( triangleVertexRemap[vertexIndex + i] !=
					triangleVertexRemap[vertexIndex + next] ) {
					VectorCopy( triIntermediateVerts[vertexIndex + i].xyz,
						surface->winding->p[count] );
					VectorCopy( triIntermediateVerts[vertexIndex + i].originalPos,
						surface->originalWinding->p[count] );
					count++;
				}
			}
			vertexIndex += surface->winding->numpoints;
			if ( count < 3 ) {
				*link = surface->next;
				FreeWinding( surface->originalWinding );
				FreeWinding( surface->winding );
				free( surface );
			} else {
				surface->winding->numpoints = count;
				surface->originalWinding->numpoints = count;
				link = &surface->next;
			}
		}
	}
}


/*
=============
TrianglePairCanShareEdge
=============
*/
static qboolean TrianglePairCanShareEdge( int triangleA, int triangleB ) {
	const triGroupRecord_t *a = &triRecords[triangleA];
	const triGroupRecord_t *b = &triRecords[triangleB];
	qboolean differ;

	differ = a->shaderInfo != b->shaderInfo || a->drawOrder != b->drawOrder
		|| a->cullGroupIdx != b->cullGroupIdx || a->lightStyle != b->lightStyle;
	if ( differ ) return qfalse;
	{
	int i, shared;
	shared = 0;
	for ( i = 0; i < 3; i++ ) {
		const triangleEdge_t *edge = &triangleEdges[a->edgeIdx[i]];
		if ( edge->triangle[0] + edge->triangle[1] == triangleA + triangleB ) {
			shared++;
		}
	}
	if ( shared != 1 ) {
		return qfalse;
	}
	{
	vec4_t plane;
	vec3_t edge0, edge1, normal;
	const triGroupRecord_t *projection;
	int i, j, vertex;
	float distance, s, t, ds, dt;
	vec4_t colorError;
	float textureWidth, textureHeight;
	PlaneFromPoints( plane,
		triIntermediateVerts[a->vertIdx[0]].xyz,
		triIntermediateVerts[a->vertIdx[1]].xyz,
		triIntermediateVerts[a->vertIdx[2]].xyz );
	i = 0;
	while ( i < 3 ) {
		distance = DotProduct( plane, triIntermediateVerts[b->vertIdx[i]].xyz ) - plane[3];
		if ( distance * distance > TRIANGLE_PLANE_EPSILON_SQ ) {
			return qfalse;
		}
		i++;
	}
	VectorSubtract( triIntermediateVerts[b->vertIdx[1]].xyz,
		triIntermediateVerts[b->vertIdx[0]].xyz, edge0 );
	VectorSubtract( triIntermediateVerts[b->vertIdx[2]].xyz,
		triIntermediateVerts[b->vertIdx[0]].xyz, edge1 );
	CrossProduct( edge1, edge0, normal );
	if ( DotProduct( plane, normal ) <= 0 ) {
		return qfalse;
	}
	textureWidth = a->shaderInfo->width;
	textureHeight = a->shaderInfo->height;
	projection = b;
	for ( i = 0; i < 3; i++ ) {
		vertex = a->vertIdx[i];
		s = triIntermediateVerts[vertex].xyz[2];
		s *= projection->texVecs[0][2];
		s += projection->texVecs[0][1] * triIntermediateVerts[vertex].xyz[1];
		s += projection->texVecs[0][0] * triIntermediateVerts[vertex].xyz[0];
		s += projection->texVecs[0][3];
		s -= triIntermediateVerts[vertex].st[0];
		s *= textureWidth;
		t = projection->texVecs[1][0] * triIntermediateVerts[vertex].xyz[0] + triIntermediateVerts[vertex].xyz[1] * projection->texVecs[1][1] + projection->texVecs[1][2] * triIntermediateVerts[vertex].xyz[2];
		t += projection->texVecs[1][3];
		t -= triIntermediateVerts[vertex].st[1];
		t *= textureHeight;
		if ( t * t + s * s > 0.25f ) {
			return qfalse;
		}
		if ( a->lightStyle >= 0 ) {
		ds = b->lmapVecs[0][2] * triIntermediateVerts[vertex].xyz[2] + b->lmapVecs[0][0] * triIntermediateVerts[vertex].xyz[0];
		ds += b->lmapVecs[0][1] * triIntermediateVerts[vertex].xyz[1];
		ds += b->lmapVecs[0][3];
		ds -= triIntermediateVerts[vertex].lightmap[0];
		/* the original computes this in double precision; kept so the build matches the original binary */
		dt = (double)triIntermediateVerts[vertex].xyz[2] * b->lmapVecs[1][2];
		ds *= 512.0f;
		dt += b->lmapVecs[1][0] * triIntermediateVerts[vertex].xyz[0];
		dt += b->lmapVecs[1][1] * triIntermediateVerts[vertex].xyz[1];
		dt += b->lmapVecs[1][3];
		dt -= triIntermediateVerts[vertex].lightmap[1];
		dt *= 512.0f;
			if ( dt * dt + ds * ds > 0.25f ) {
				return qfalse;
			}
		}
		for ( j = 0; j < 4; j++ ) {
			float error;
			error = b->colorVecs[j][2] * triIntermediateVerts[vertex].xyz[2];
			error += b->colorVecs[j][0] * triIntermediateVerts[vertex].xyz[0];
			error += b->colorVecs[j][1] * triIntermediateVerts[vertex].xyz[1];
			error += b->colorVecs[j][3];
			error -= triIntermediateVerts[vertex].color[j];
			colorError[j] = error;
		}
		if ( colorError[0]*colorError[0] + colorError[1]*colorError[1] + colorError[2]*colorError[2] + colorError[3]*colorError[3] > 0.25f ) {
			return qfalse;
		}
	}
	return qtrue;
	}
	}
}


/*
=============
HashTriangleEdge

Maps an unordered pair of triangle vertex indices into the 4096 edge-chain
buckets used by the simplification pass.
=============
*/
static int HashTriangleEdge( int vertexA, int vertexB ) {
	return ( ( vertexA + 1 ) * ( vertexB + 1 ) ) & 0xFFF;
}


/*
=============
FindOrCreateTriangleEdge

Returns the chained edge for a directed triangle side, or creates it.  The
compatibility, nonmanifold and removal flags are set by the constructor
and simplification consumers.
=============
*/
static int FindOrCreateTriangleEdge( int triangle, int startVertex,
	int endVertex ) {
	int hash;
	int edgeIndex;

	hash = HashTriangleEdge( startVertex, endVertex );
	for ( edgeIndex = triangleEdgeHash[hash]; edgeIndex >= 0;
		edgeIndex = triangleEdges[edgeIndex].hashNext ) {

		if ( triangleEdges[edgeIndex].vertex[0] == endVertex
			&& triangleEdges[edgeIndex].vertex[1] == startVertex ) {
			if ( triangleEdges[edgeIndex].triangle[1] >= 0 ) {
				triangleEdges[edgeIndex].nonmanifold = 1;
				return edgeIndex;
			}
			triangleEdges[edgeIndex].triangle[1] = triangle;
			triangleEdges[edgeIndex].compatible = TrianglePairCanShareEdge(
				triangleEdges[edgeIndex].triangle[0], triangle );
			return edgeIndex;
		}

		if ( triangleEdges[edgeIndex].vertex[0] == startVertex
			&& triangleEdges[edgeIndex].vertex[1] == endVertex ) {
			triangleEdges[edgeIndex].nonmanifold = 1;
			return edgeIndex;
		}
	}

	if ( numTriangleEdges == MAX_TRIANGLE_EDGES ) {
		Error( "Max triangle edges (%i) exceeded\n", MAX_TRIANGLE_EDGES );
	}

	edgeIndex = numTriangleEdges++;
	triangleEdges[edgeIndex].vertex[0] = startVertex;
	triangleEdges[edgeIndex].vertex[1] = endVertex;
	triangleEdges[edgeIndex].triangle[0] = triangle;
	triangleEdges[edgeIndex].triangle[1] = -1;
	triangleEdges[edgeIndex].lengthSquared = VectorDistanceSquared(
		triIntermediateVerts[startVertex].xyz,
		triIntermediateVerts[endVertex].xyz );
	triangleEdges[edgeIndex].compatible = 0;
	triangleEdges[edgeIndex].nonmanifold = 0;
	triangleEdges[edgeIndex].removed = 0;
	triangleEdges[edgeIndex].hashNext = triangleEdgeHash[hash];
	triangleEdgeHash[hash] = edgeIndex;

	return edgeIndex;
}


/*
=============
BuildTriangleEdges

Builds the three directed edge references for every triangle record.
=============
*/
void BuildTriangleEdges( void ) {
	triGroupRecord_t *record;
	int triangle;

	numTriangleEdges = 0;
	memset( triangleEdgeHash, 0xFF, sizeof( triangleEdgeHash ) );

	for ( triangle = 0; triangle < numTriangles; triangle++ ) {
		record = &triRecords[triangle];

		record->edgeIdx[0] = FindOrCreateTriangleEdge(
			triangle, triangleVertexRemap[record->vertIdx[1]],
			triangleVertexRemap[record->vertIdx[2]] );
		record->edgeIdx[1] = FindOrCreateTriangleEdge(
			triangle, triangleVertexRemap[record->vertIdx[2]],
			triangleVertexRemap[record->vertIdx[0]] );
		record->edgeIdx[2] = FindOrCreateTriangleEdge(
			triangle, triangleVertexRemap[record->vertIdx[0]],
			triangleVertexRemap[record->vertIdx[1]] );
	}
}


/*
=============
TriangleCanSimplify

Edges of subdivided (tessellated) shaders are never collapsed.
=============
*/
static qboolean TriangleCanSimplify( int firstTriangle,
	int secondTriangle ) {
	if ( firstTriangle >= 0 &&
		triRecords[firstTriangle].shaderInfo->subdivisions != 0.0f ) {
		return qfalse;
	}
	if ( secondTriangle >= 0 &&
		triRecords[secondTriangle].shaderInfo->subdivisions != 0.0f ) {
		return qfalse;
	}
	return qtrue;
}


/*
=============
CollectCollapseTriangles
=============
*/
static qboolean CollectCollapseTriangles( int representativeVertex,
	int destinationVertex, int edgeIndex, int firstTriangle,
	int secondTriangle, int *affectedTriangles, int *affectedCount,
	int *canTrySecondDirection ) {
	int triangle, incomingEdge, edge, selectedEdge, crossedIncompatible;
	int v0, v1, v2;
	const int *edgeTriangles;
	vec4_t newPlane, oldPlane;

	if ( firstTriangle < 0 ) {
		*canTrySecondDirection = 0;
		return qtrue;
	}
	*canTrySecondDirection = 1;
	incomingEdge = edgeIndex;
	crossedIncompatible = 0;
	triangle = firstTriangle;
	for ( ;; ) {
		edge = triRecords[triangle].edgeIdx[0];
		if ( edge != incomingEdge
			&& (triangleVertexRemap[triangleEdges[edge].vertex[0]] == representativeVertex
				|| triangleVertexRemap[triangleEdges[edge].vertex[1]] == representativeVertex) ) {
			selectedEdge = edge;
		} else {
			edge = triRecords[triangle].edgeIdx[1];
			if ( edge != incomingEdge
				&& (triangleVertexRemap[triangleEdges[edge].vertex[0]] == representativeVertex
					|| triangleVertexRemap[triangleEdges[edge].vertex[1]] == representativeVertex) ) {
				selectedEdge = edge;
			} else {
				selectedEdge = triRecords[triangle].edgeIdx[2];
			}
		}
		edgeTriangles = triangleEdges[selectedEdge].triangle;
		if ( triangleEdges[selectedEdge].nonmanifold ) return qfalse;
		if ( !triangleEdges[selectedEdge].compatible && selectedEdge != edgeIndex ) {
			if ( triangleEdges[edgeIndex].compatible ) return qfalse;
			{
				int other;
				other = triangleEdges[selectedEdge].vertex[triangleVertexRemap[triangleEdges[selectedEdge].vertex[0]] == representativeVertex];
				if ( !PointsNearLine( triIntermediateVerts[representativeVertex].xyz,
						triIntermediateVerts[destinationVertex].xyz,
						triIntermediateVerts[other].xyz,
						0.0625f ) ) return qfalse;
			}
			crossedIncompatible = 1;
		}
		if ( triangle != firstTriangle ) {
			v0 = triRecords[triangle].vertIdx[0];
			v1 = triRecords[triangle].vertIdx[1];
			v2 = triRecords[triangle].vertIdx[2];
			PlaneFromPoints( oldPlane, triIntermediateVerts[v0].xyz,
				triIntermediateVerts[v1].xyz, triIntermediateVerts[v2].xyz );
			if ( triangleVertexRemap[v0] == representativeVertex ) v0 = destinationVertex;
			else if ( triangleVertexRemap[v1] == representativeVertex ) v1 = destinationVertex;
			else v2 = destinationVertex;
			if ( !PlaneFromPoints( newPlane, triIntermediateVerts[v0].xyz,
				triIntermediateVerts[v1].xyz, triIntermediateVerts[v2].xyz )
				|| DotProduct( oldPlane, newPlane ) <= 0.0f ) return qfalse;
		}
		triangle = triangleEdges[selectedEdge].triangle[
			triangle == edgeTriangles[0]];
		if ( triangle == -1 || triangle == secondTriangle ) break;
		if ( triangle == firstTriangle ) return qfalse;
		incomingEdge = selectedEdge;
		affectedTriangles[*affectedCount] = triangle;
		++*affectedCount;
	}
	if ( !triangleEdges[edgeIndex].compatible && !crossedIncompatible ) return qfalse;
	if ( triangle != secondTriangle ) *canTrySecondDirection = 0;
	return qtrue;
}


/*
=============
FindOrCreateDrawVert
=============
*/
static int FindOrCreateDrawVert( float *st, byte *color, int oldVertex,
	int destinationVertex, triGroupRecord_t *record, float *lightmap ) {
	int hash, vertex, result;
	float delta[2];
	hash = HashVertexPosition( triIntermediateVerts[destinationVertex].xyz );
	for ( vertex = triangleVertexHash[hash]; vertex >= 0;
		vertex = triIntermediateVerts[vertex].hashNext ) {
		if ( triangleVertexRemap[vertex] != destinationVertex
			|| DotProduct( triIntermediateVerts[oldVertex].normal,
				triIntermediateVerts[vertex].normal ) < 0.999f ) continue;
		delta[0] = triIntermediateVerts[vertex].st[0] - st[0];
		delta[1] = triIntermediateVerts[vertex].st[1] - st[1];
		delta[0] *= record->shaderInfo->width;
		delta[1] *= record->shaderInfo->height;
		if ( delta[1]*delta[1] + delta[0]*delta[0] > 0.25f ) continue;
		if ( record->lightStyle >= 0 ) {
			delta[0] = triIntermediateVerts[vertex].lightmap[0] - lightmap[0];
			delta[1] = triIntermediateVerts[vertex].lightmap[1] - lightmap[1];
			delta[0] *= 512.0f;
			delta[1] *= 512.0f;
			if ( delta[1]*delta[1] + delta[0]*delta[0] > 0.25f ) continue;
		}
		if ( triIntermediateVerts[vertex].color[0] == color[0]
			&& triIntermediateVerts[vertex].color[1] == color[1]
			&& triIntermediateVerts[vertex].color[2] == color[2]
			&& triIntermediateVerts[vertex].color[3] == color[3] ) return vertex;
	}
	if ( numTriIntermediateVerts >= 0x180000 ) Error( "MAX_MAP_VERTEXES (%i) exceeded\n", 0x180000 );
	result = numTriIntermediateVerts++;
	VectorCopy( triIntermediateVerts[destinationVertex].xyz, triIntermediateVerts[result].xyz );
	VectorCopy( triIntermediateVerts[oldVertex].normal, triIntermediateVerts[result].normal );
	triIntermediateVerts[result].st[0] = st[0];
	triIntermediateVerts[result].st[1] = st[1];
	triIntermediateVerts[result].lightmap[0] = lightmap[0];
	triIntermediateVerts[result].lightmap[1] = lightmap[1];
	triIntermediateVerts[result].color[0] = color[0];
	triIntermediateVerts[result].color[1] = color[1];
	triIntermediateVerts[result].color[2] = color[2];
	triIntermediateVerts[result].color[3] = color[3];
	triIntermediateVerts[result].hashNext = triangleVertexHash[hash];
	triangleVertexHash[hash] = result;
	triangleVertexRemap[result] = destinationVertex;
	return result;
}


/*
=============
ApplyTriangleCollapse
=============
*/
void ApplyTriangleCollapse( int representativeVertex,
	int destinationVertex, int *affectedTriangles, int affectedCount ) {
	int i, j;
	triGroupRecord_t *record;
	vec2_t st, lightmap;
	byte color[4];
	for ( i = 0; i < affectedCount; i++ ) {
		record = &triRecords[affectedTriangles[i]];
		st[0] = record->texVecs[0][1] * triIntermediateVerts[destinationVertex].xyz[1]
			+ record->texVecs[0][0] * triIntermediateVerts[destinationVertex].xyz[0]
			+ record->texVecs[0][2] * triIntermediateVerts[destinationVertex].xyz[2]
			+ record->texVecs[0][3];
		st[1] = record->texVecs[1][1] * triIntermediateVerts[destinationVertex].xyz[1]
			+ record->texVecs[1][0] * triIntermediateVerts[destinationVertex].xyz[0]
			+ record->texVecs[1][2] * triIntermediateVerts[destinationVertex].xyz[2]
			+ record->texVecs[1][3];
		lightmap[0] = record->lmapVecs[0][1] * triIntermediateVerts[destinationVertex].xyz[1]
			+ record->lmapVecs[0][0] * triIntermediateVerts[destinationVertex].xyz[0]
			+ record->lmapVecs[0][2] * triIntermediateVerts[destinationVertex].xyz[2]
			+ record->lmapVecs[0][3];
		lightmap[1] = record->lmapVecs[1][1] * triIntermediateVerts[destinationVertex].xyz[1]
			+ record->lmapVecs[1][0] * triIntermediateVerts[destinationVertex].xyz[0]
			+ record->lmapVecs[1][2] * triIntermediateVerts[destinationVertex].xyz[2]
			+ record->lmapVecs[1][3];
		color[0] = TRIS_MAX( 0, TRIS_MIN( 255, TRIS_COLLAPSE_COLOR(record->colorVecs[0], triIntermediateVerts[destinationVertex].xyz) ) );
		color[1] = TRIS_MAX( 0, TRIS_MIN( 255, TRIS_COLLAPSE_COLOR(record->colorVecs[1], triIntermediateVerts[destinationVertex].xyz) ) );
		color[2] = TRIS_MAX( 0, TRIS_MIN( 255, TRIS_COLLAPSE_COLOR(record->colorVecs[2], triIntermediateVerts[destinationVertex].xyz) ) );
		color[3] = TRIS_MAX( 0, TRIS_MIN( 255, TRIS_COLLAPSE_COLOR(record->colorVecs[3], triIntermediateVerts[destinationVertex].xyz) ) );
		for ( j = 0; j < 3; j++ ) {
			if ( triangleVertexRemap[record->vertIdx[j]] == representativeVertex ) {
				record->vertIdx[j] = FindOrCreateDrawVert( st, color,
					record->vertIdx[j], destinationVertex, record, lightmap );
			} else if ( triangleVertexRemap[triangleEdges[record->edgeIdx[j]].vertex[0]] == representativeVertex ) {
				int other = triangleEdges[record->edgeIdx[j]].vertex[1];
				triangleEdges[record->edgeIdx[j]].lengthSquared = VectorDistanceSquared(
					triIntermediateVerts[destinationVertex].xyz,
					triIntermediateVerts[other].xyz );
				triangleEdges[record->edgeIdx[j]].vertex[0] = destinationVertex;
			} else if ( triangleVertexRemap[triangleEdges[record->edgeIdx[j]].vertex[1]] == representativeVertex ) {
				int other = triangleEdges[record->edgeIdx[j]].vertex[0];
				triangleEdges[record->edgeIdx[j]].lengthSquared = VectorDistanceSquared(
					triIntermediateVerts[destinationVertex].xyz,
					triIntermediateVerts[other].xyz );
				triangleEdges[record->edgeIdx[j]].vertex[1] = destinationVertex;
			}
		}
	}
}


/*
=============
CheckCollapseAngles
=============
*/
static qboolean CheckCollapseAngles( int destinationVertex,
	int representativeVertex, const int *affectedTriangles, int affectedCount ) {
	int i, vertex;
	vec3_t point[3];
	float side[3];
	float root[3];
	const double threshold = 256.0 / sqrt( 16385.0 );

	for ( i = 0; i < affectedCount; i++ ) {
		const triGroupRecord_t *record = &triRecords[affectedTriangles[i]];
		vertex = record->vertIdx[0];
		if ( triangleVertexRemap[vertex] == representativeVertex ) {
			vertex = destinationVertex;
		}
		VectorCopy( triIntermediateVerts[vertex].xyz, point[0] );
		vertex = record->vertIdx[1];
		if ( triangleVertexRemap[vertex] == representativeVertex ) {
			vertex = destinationVertex;
		}
		VectorCopy( triIntermediateVerts[vertex].xyz, point[1] );
		vertex = record->vertIdx[2];
		if ( triangleVertexRemap[vertex] == representativeVertex ) {
			VectorCopy( triIntermediateVerts[destinationVertex].xyz, point[2] );
		} else {
			VectorCopy( triIntermediateVerts[vertex].xyz, point[2] );
		}
		side[0] = VectorDistanceSquared( point[1], point[2] );
		side[1] = VectorDistanceSquared( point[2], point[0] );
		side[2] = VectorDistanceSquared( point[0], point[1] );
		root[0] = sqrt( side[0] );
		root[1] = sqrt( side[1] );
		root[2] = sqrt( side[2] );
		if ( side[1] + side[0] - side[2] > root[1] * root[0] * threshold
			|| side[2] + side[1] - side[0] > root[2] * root[1] * threshold
			|| side[2] + side[0] - side[1] > root[2] * root[0] * threshold ) {
			return qfalse;
		}
	}
	return qtrue;
}


/*
=============
RetireTriangleEdge

Retire one triangle and transfer the surviving edge adjacency. The
destination vertex argument is unused.
=============
*/
static void RetireTriangleEdge( int representativeVertex, int destinationVertex,
	int edgeIndex, int triangle ) {
	int keptEdge;
	int retiredEdge;
	int otherTriangle;
	int corner;

	triRecords[triangle].removed = 1;
	numSimplifiedTriangles++;
	keptEdge = -1;
	retiredEdge = -1;
	for ( corner = 0; corner < 3; corner++ ) {
		if ( triangleVertexRemap[triRecords[triangle].vertIdx[corner]] == representativeVertex ) {
			keptEdge = triRecords[triangle].edgeIdx[corner];
		} else if ( triRecords[triangle].edgeIdx[corner] != edgeIndex ) {
			retiredEdge = triRecords[triangle].edgeIdx[corner];
		}
	}
	triangleEdges[retiredEdge].removed = 1;
	otherTriangle = triangleEdges[retiredEdge].triangle[
		triangleEdges[retiredEdge].triangle[0] == triangle];
	if ( otherTriangle >= 0 ) {
		if ( triRecords[otherTriangle].edgeIdx[0] == retiredEdge ) {
			triRecords[otherTriangle].edgeIdx[0] = keptEdge;
		} else if ( triRecords[otherTriangle].edgeIdx[1] == retiredEdge ) {
			triRecords[otherTriangle].edgeIdx[1] = keptEdge;
		} else {
			triRecords[otherTriangle].edgeIdx[2] = keptEdge;
		}
	}
	triangleEdges[keptEdge].triangle[
		triangleEdges[keptEdge].triangle[1] == triangle] = otherTriangle;
}


/*
=============
TryCollapseTriangleEdge
=============
*/
static qboolean TryCollapseTriangleEdge( int representativeVertex,
	int destinationVertex, int edgeIndex ) {
	int affectedTriangles[65536];
	int affectedCount = 0;
	int canTrySecondDirection;

	if ( !CollectCollapseTriangles( representativeVertex, destinationVertex,
		edgeIndex, triangleEdges[edgeIndex].triangle[0], triangleEdges[edgeIndex].triangle[1],
		affectedTriangles, &affectedCount, &canTrySecondDirection ) ) {
		return qfalse;
	}
	if ( !canTrySecondDirection &&
		!CollectCollapseTriangles( representativeVertex, destinationVertex,
			edgeIndex, triangleEdges[edgeIndex].triangle[1], triangleEdges[edgeIndex].triangle[0],
			affectedTriangles, &affectedCount, &canTrySecondDirection ) ) {
		return qfalse;
	}
	if ( !CheckCollapseAngles( destinationVertex, representativeVertex,
		affectedTriangles, affectedCount ) ) {
		return qfalse;
	}
	RetireTriangleEdge( representativeVertex, destinationVertex, edgeIndex, triangleEdges[edgeIndex].triangle[0] );
	if ( triangleEdges[edgeIndex].triangle[1] >= 0 ) {
		RetireTriangleEdge( representativeVertex, destinationVertex, edgeIndex, triangleEdges[edgeIndex].triangle[1] );
	}
	triangleEdges[edgeIndex].removed = 1;
	ApplyTriangleCollapse( representativeVertex, destinationVertex,
		affectedTriangles, affectedCount );
	return qtrue;
}


/*
=============
SimplifyTriangleEdges

One pass over the edge array; failure in one direction tries the reverse.
=============
*/
static void SimplifyTriangleEdges( void ) {
	int edgeIndex;
	triangleEdge_t *edge;

	for ( edgeIndex = 0; edgeIndex < numTriangleEdges; edgeIndex++ ) {
		edge = &triangleEdges[edgeIndex];
		if ( edge->removed || edge->nonmanifold ) {
			continue;
		}
		if ( !TriangleCanSimplify( edge->triangle[0], edge->triangle[1] ) ) {
			continue;
		}
		if ( !TryCollapseTriangleEdge( edge->vertex[0], edge->vertex[1],
			edgeIndex ) ) {
			TryCollapseTriangleEdge( edge->vertex[1], edge->vertex[0],
				edgeIndex );
		}
	}
}


/*
=============
SimplifyTriangleMesh

Build representative vertices and triangle edges, then make two edge-collapse
passes. The reported fraction uses the original triangle count.
=============
*/
void SimplifyTriangleMesh( void ) {
	BuildTriangleVertexRemap();
	BuildTriangleEdges();
	numSimplifiedTriangles = 0;
	SimplifyTriangleEdges();
	SimplifyTriangleEdges();
	if ( !entity_num ) {
		printf( "%i tris simplified away (%.1f%%)\n",
			numSimplifiedTriangles,
			100.0f * numSimplifiedTriangles / numTriangles );
	}
}


/*
=============
SmoothTriangleNormals
=============
*/
void SmoothTriangleNormals( void ) {
	int originalCount, original, i, other, hash;
	vec3_t normal;
	float threshold;
	float dot;
	originalCount = numOriginalDrawVerts;
	for ( original = 0; original < originalCount; original++ ) {
		VectorCopy( triIntermediateVerts[original].normal, triIntermediateVerts[original].smoothNormal );
	}
	for ( i = originalCount; i < numTriIntermediateVerts; i++ ) {
		if ( triIntermediateVerts[i].smoothAngle >= 0.999f ) {
			VectorCopy( triIntermediateVerts[i].normal, triIntermediateVerts[i].smoothNormal );
		} else {
			VectorClear( normal );
			hash = HashVertexPosition( triIntermediateVerts[i].xyz );
			for ( other = triangleVertexHash[hash]; other >= 0;
				other = triIntermediateVerts[other].hashNext ) {
				threshold = triIntermediateVerts[other].smoothAngle > triIntermediateVerts[i].smoothAngle
					? triIntermediateVerts[other].smoothAngle : triIntermediateVerts[i].smoothAngle;
				dot = triIntermediateVerts[other].normal[0] * triIntermediateVerts[i].normal[0];
				dot += triIntermediateVerts[i].normal[1] * triIntermediateVerts[other].normal[1];
				/* the original computes this in double precision; kept so the build matches the original binary */
				dot += (double)triIntermediateVerts[other].normal[2] * triIntermediateVerts[i].normal[2];
				if ( dot >= threshold && VectorCompareEpsilon( triIntermediateVerts[other].xyz,
					triIntermediateVerts[i].xyz ) ) {
					VectorAdd( normal, triIntermediateVerts[other].normal, normal );
				}
			}
			VectorNormalize2( normal, triIntermediateVerts[i].smoothNormal );
		}
	}
}


/*
=============
CompareTriRecordHeaders

Orders triangle records by removal status and their batching header fields.
=============
*/
int CompareTriRecordHeaders( const void *a, const void *b ) {
	const triGroupRecord_t *triA = a;
	const triGroupRecord_t *triB = b;
	int difference;

	difference = triA->removed - triB->removed;
	if ( difference ) {
		return difference;
	}
	difference = triA->drawOrder - triB->drawOrder;
	if ( difference ) {
		return difference;
	}
	difference = triA->cullGroupIdx - triB->cullGroupIdx;
	if ( difference ) {
		return difference;
	}
	difference = (const char *)triA->shaderInfo - (const char *)triB->shaderInfo;
	if ( difference ) {
		return difference;
	}
	return triA->lightStyle - triB->lightStyle;
}


/*
=============
TriRecordsShareEdge

Tests whether two triangle records refer to any common cached edge.
=============
*/
static int TriRecordsShareEdge( const triGroupRecord_t *lhs,
	const triGroupRecord_t *rhs ) {
	if ( lhs->edgeIdx[0] == rhs->edgeIdx[0] ) {
		return 1;
	}
	if ( lhs->edgeIdx[0] == rhs->edgeIdx[1] ) {
		return 1;
	}
	if ( lhs->edgeIdx[0] == rhs->edgeIdx[2] ) {
		return 1;
	}
	if ( lhs->edgeIdx[1] == rhs->edgeIdx[0] ) {
		return 1;
	}
	if ( lhs->edgeIdx[1] == rhs->edgeIdx[1] ) {
		return 1;
	}
	if ( lhs->edgeIdx[1] == rhs->edgeIdx[2] ) {
		return 1;
	}
	if ( lhs->edgeIdx[2] == rhs->edgeIdx[0] ) {
		return 1;
	}
	if ( lhs->edgeIdx[2] == rhs->edgeIdx[1] ) {
		return 1;
	}
	return lhs->edgeIdx[2] == rhs->edgeIdx[2];
}


/*
=============
CompareTrisByGroupId

Orders triangle records by the connected-component identifier.
=============
*/
static int CompareTrisByGroupId( const void *a, const void *b ) {
	const triGroupRecord_t *tri0;
	const triGroupRecord_t *tri1;

	tri0 = (const triGroupRecord_t *) a;
	tri1 = (const triGroupRecord_t *) b;
	return tri0->groupId - tri1->groupId;
}


/*
=============
GroupAdjacentTris

Assigns connected-component identifiers to triangles sharing cached edges,
merges components bridged by a later triangle, and groups the result by ID.
=============
*/
int GroupAdjacentTris( triGroupRecord_t *records, int numTris ) {
	int i;
	int j;
	int k;
	int nextGroupId;
	int groupCount;

	nextGroupId = 1;
	groupCount = 0;

	for ( i = 0; i < numTris; i++ ) {
		records[i].groupId = 0;

		for ( j = 0; j < i; j++ ) {
			if ( records[i].groupId == records[j].groupId ) {
				continue;
			}
			if ( !TriRecordsShareEdge( &records[i], &records[j] ) ) {
				continue;
			}

			if ( records[i].groupId ) {
				for ( k = 0; k < i; k++ ) {
					if ( records[k].groupId == records[i].groupId ) {
						records[k].groupId = records[j].groupId;
					}
				}
				groupCount--;
			}

			records[i].groupId = records[j].groupId;
		}

		if ( !records[i].groupId ) {
			records[i].groupId = nextGroupId++;
			groupCount++;
		}
	}

	if ( groupCount > 1 ) {
		qsort( records, numTris, sizeof( *records ), CompareTrisByGroupId );
	}

	return groupCount;
}


/*
=============
GroupTriSurfsIntoSubgroups

Rebuild bounds after each merge, keeping records sorted by group.
=============
*/
int GroupTriSurfsIntoSubgroups( triGroupRecord_t *records,
	int numTris, int groupCount ) {
	triSubgroup_t groups[65536];
	vec3_t mins, maxs;
	int end, group, i, j, k;

	if ( groupCount > 65536 ) Error( "MAX_TRI_SUBGROUPS" );
	for ( ;; ) {
		group = 0;
		for ( i = 0; i < numTris; i = end ) {
			ClearBounds( groups[group].mins, groups[group].maxs );
			groups[group].firstTri = i;
			for ( end = i + 1; end < numTris; end++ ) {
				if ( records[end].groupId != records[i].groupId ) break;
				records[end].groupId = group;
				AddPointToBounds( triIntermediateVerts[records[end].vertIdx[0]].xyz,
					groups[group].mins, groups[group].maxs );
				AddPointToBounds( triIntermediateVerts[records[end].vertIdx[1]].xyz,
					groups[group].mins, groups[group].maxs );
				AddPointToBounds( triIntermediateVerts[records[end].vertIdx[2]].xyz,
					groups[group].mins, groups[group].maxs );
			}
			records[i].groupId = group;
			AddPointToBounds( triIntermediateVerts[records[i].vertIdx[0]].xyz,
				groups[group].mins, groups[group].maxs );
			AddPointToBounds( triIntermediateVerts[records[i].vertIdx[1]].xyz,
				groups[group].mins, groups[group].maxs );
			AddPointToBounds( triIntermediateVerts[records[i].vertIdx[2]].xyz,
				groups[group].mins, groups[group].maxs );
			groups[group].triCount = end - i;
			group++;
		}
		for ( i = groupCount - 1; i >= 1; i-- ) {
			for ( j = i - 1; j >= 0; j-- ) {
				if ( groups[j].triCount + groups[i].triCount > 1024 ) continue;
				for ( k = 0; k < 3; k++ ) {
					mins[k] = groups[i].mins[k] < groups[j].mins[k]
						? groups[i].mins[k] : groups[j].mins[k];
					maxs[k] = groups[i].maxs[k] > groups[j].maxs[k]
						? groups[i].maxs[k] : groups[j].maxs[k];
					if ( maxs[k] - mins[k] > groups[i].maxs[k] - groups[i].mins[k]
						+ groups[j].maxs[k] - groups[j].mins[k] + 128.0f ) break;
				}
				if ( k < 3 ) continue;
				for ( k = 0; k < groups[i].triCount; k++ ) {
					records[groups[i].firstTri + k].groupId = j;
				}
				if ( --groupCount == 1 ) return 1;
				qsort( records, numTris, sizeof(*records), CompareTrisByGroupId );
				goto rebuildSubgroups;
			}
		}
		return groupCount;
rebuildSubgroups:
		;
	}
}


/*
=============
GroupAndMergeTriSubgroups
=============
*/
static int GroupAndMergeTriSubgroups( triGroupRecord_t *records, int numTris ) {
	int groupCount;

	groupCount = GroupAdjacentTris( records, numTris );
	if ( groupCount > 1 ) {
		return GroupTriSurfsIntoSubgroups( records, numTris, groupCount );
	}
	return groupCount;
}


/*
=============
FilterTriWindingIntoCells

Consume a winding while collecting the cells touched by its cull group.
=============
*/
void FilterTriWindingIntoCells( winding_t *winding,
	const vec4_t plane, triGroupRecord_t *record, node_t *node ) {
	plane_t *split;
	winding_t *front, *back;
	int bit;
	float dot;

	if ( !winding ) return;
	if ( node->bspCell != -2 ) {
		if ( node->bspCell >= 0 ) {
			bit = ( node->bspCell << 11 ) + record->cullGroupIdx;
			cellCullGroupBits[bit >> 3] |= 1 << ( bit & 7 );
		}
		FreeWinding( winding );
		return;
	}
	split = &mapplanes[node->planenum];
	dot = split->normal[2] * plane[2] + split->normal[1] * plane[1]
		+ split->normal[0] * plane[0];
	if ( dot > 0.999f && fabs( plane[3] - split->dist ) < 0.001f ) {
		FilterTriWindingIntoCells( winding, plane, record, node->children[0] );
		return;
	}
	if ( dot < -0.999f && fabs( split->dist + plane[3] ) < 0.001f ) {
		FilterTriWindingIntoCells( winding, plane, record, node->children[1] );
		return;
	}
	ClipWindingEpsilon( winding, split->normal, split->dist, 0.1f,
		&front, &back, qfalse );
	FilterTriWindingIntoCells( front, plane, record, node->children[0] );
	FilterTriWindingIntoCells( back, plane, record, node->children[1] );
	FreeWinding( winding );
}


/*
=============
CopyTriDrawVert

Convert the working vertex to the complete BSP drawing vertex.
=============
*/
static void CopyTriDrawVert( drawVert_t *destination,
	const triIntermediateVert_t *source ) {
	VectorCopy( source->xyz, destination->xyz );
	VectorCopy( source->smoothNormal, destination->normal );
	destination->st[0] = source->st[0];
	destination->st[1] = source->st[1];
	destination->lightmap[0] = source->lightmap[0];
	destination->lightmap[1] = source->lightmap[1];
	destination->color[0] = source->color[0];
	destination->color[1] = source->color[1];
	destination->color[2] = source->color[2];
	destination->color[3] = source->color[3];
}


/*
=============
CanMergeVerts
=============
*/
static qboolean CanMergeVerts( drawVert_t *compare, drawVert_t *current,
	triGroupRecord_t *compareTri, triGroupRecord_t *currentTri, int lightStyle ) {
	float delta[2];

	if ( !VectorCompareEpsilon( compare->xyz, current->xyz ) ) return qfalse;
	if ( compare->normal[2] * current->normal[2]
		+ compare->normal[1] * current->normal[1]
		+ compare->normal[0] * current->normal[0] < 0.999f ) return qfalse;
	delta[0] = compare->st[0] - current->st[0];
	delta[1] = compare->st[1] - current->st[1];
	delta[0] *= compareTri->shaderInfo->width;
	delta[1] *= compareTri->shaderInfo->height;
	if ( delta[1] * delta[1] + delta[0] * delta[0] > 0.25f ) return qfalse;
	if ( lightStyle >= 0 ) {
		delta[0] = compare->lightmap[0] - current->lightmap[0];
		delta[1] = compare->lightmap[1] - current->lightmap[1];
		delta[0] *= 512.0f;
		delta[1] *= 512.0f;
		if ( delta[1] * delta[1] + delta[0] * delta[0] > 0.25f ) return qfalse;
	}
	if ( compare->color[0] != current->color[0]
		|| compare->color[1] != current->color[1]
		|| compare->color[2] != current->color[2]
		|| compare->color[3] != current->color[3] ) return qfalse;
	if ( compareTri != currentTri
		&& ( compareTri->texVecs[0][2] * currentTri->texVecs[0][2]
			+ compareTri->texVecs[0][1] * currentTri->texVecs[0][1]
			+ compareTri->texVecs[0][0] * currentTri->texVecs[0][0] <= 0.0f
		|| compareTri->texVecs[1][2] * currentTri->texVecs[1][2]
			+ compareTri->texVecs[1][1] * currentTri->texVecs[1][1]
			+ compareTri->texVecs[1][0] * currentTri->texVecs[1][0] <= 0.0f ) ) {
		numTextureDirectionMismatches++;
		return qfalse;
	}
	return qtrue;
}


/*
=============
EmitDrawVert
=============
*/
static int EmitDrawVert( triSoup_t *soup, triShaderInfo_t *material,
	int lightStyle, const triIntermediateVert_t *source ) {
	drawVert_t vertex;

	CopyTriDrawVert( &vertex, source );
	if ( numDrawVerts == 524288 ) {
		Error( "MAX_MAP_DRAW_VERTS" );
	}
	soup->numVerts++;
	numDrawVerts++;
	memcpy( &triangleSoupData[soup->firstVert + soup->numVerts - 1],
		&vertex, sizeof(vertex) );
	return soup->numVerts - 1;
}


/*
=============
MatchVertWithUVOffset
=============
*/
static qboolean MatchVertWithUVOffset( triShaderInfo_t *shader, drawVert_t *vertexA,
	drawVert_t *vertexB, int *uOffset, int lightStyle, int *vOffset ) {
	vec2_t delta;
	if ( !VectorCompareEpsilon( vertexA->xyz, vertexB->xyz ) ) return qfalse;
	if ( vertexA->normal[2] * vertexB->normal[2]
		+ vertexA->normal[1] * vertexB->normal[1]
		+ vertexA->normal[0] * vertexB->normal[0] < 0.999f ) return qfalse;
	if ( lightStyle >= 0 ) {
		delta[0] = vertexB->lightmap[0] - vertexA->lightmap[0];
		delta[1] = vertexB->lightmap[1] - vertexA->lightmap[1];
		delta[0] *= 512.0f;
		delta[1] *= 512.0f;
		if ( delta[1] * delta[1] + delta[0] * delta[0] > 0.25f ) return qfalse;
	}
	if ( vertexA->color[0] != vertexB->color[0]
		|| vertexA->color[1] != vertexB->color[1]
		|| vertexA->color[2] != vertexB->color[2]
		|| vertexA->color[3] != vertexB->color[3] ) return qfalse;
	*uOffset = RoundToInt( vertexB->st[0] - vertexA->st[0] );
	*vOffset = RoundToInt( vertexB->st[1] - vertexA->st[1] );
	delta[0] = vertexB->st[0] - vertexA->st[0];
	delta[1] = vertexB->st[1] - vertexA->st[1];
	delta[0] -= *uOffset;
	delta[1] -= *vOffset;
	delta[0] *= shader->width;
	delta[1] *= shader->height;
	if ( delta[1] * delta[1] + delta[0] * delta[0] > 0.25f ) return qfalse;
	return qtrue;
}


/*
=============
CheckTriangleUVCompatibility
=============
*/
static qboolean CheckTriangleUVCompatibility( unsigned short *indices,
	int neighborTriangle, drawVert_t *verts, triShaderInfo_t *shader,
	int lightStyle, int triangle, int *uOffset, int *vOffset ) {
	int i, j, u, v;
	qboolean found = qfalse;
	drawVert_t *current, *neighbor;
	for ( i = 0; i < 3; i++ ) {
		current = &verts[indices[triangle + i]];
		for ( j = 0; j < 3; j++ ) {
			neighbor = &verts[indices[neighborTriangle + j]];
			if ( current->xyz[0] != neighbor->xyz[0]
				|| current->xyz[1] != neighbor->xyz[1]
				|| current->xyz[2] != neighbor->xyz[2] ) continue;
			if ( !MatchVertWithUVOffset( shader, current, neighbor, &u, lightStyle, &v ) ) {
				return qfalse;
			}
			if ( found ) {
				if ( *uOffset != u || *vOffset != v ) return qfalse;
			} else {
				*uOffset = u;
				*vOffset = v;
				found = qtrue;
			}
		}
	}
	return qtrue;
}


/*
=============
BuildTriangleNeighbors
=============
*/
static void BuildTriangleNeighbors( drawVert_t *verts, unsigned short *indices,
	int numVerts, int numIndices, short *neighbors ) {
	int triangle, earlier, i, j, previousI, previousJ;
	memset( neighbors, -1, sizeof( *neighbors ) * numIndices );
	for ( triangle = 3; triangle < numIndices; triangle += 3 ) {
		for ( earlier = 0; earlier < triangle; earlier += 3 ) {
			for ( previousI = 2, i = 0; i < 3; previousI = i, i++ ) {
				for ( j = 0, previousJ = 2; j < 3; previousJ = j, j++ ) {
					if ( verts[indices[triangle + previousI]].xyz[0] ==
							verts[indices[earlier + j]].xyz[0]
						&& verts[indices[triangle + previousI]].xyz[1] ==
							verts[indices[earlier + j]].xyz[1]
						&& verts[indices[triangle + previousI]].xyz[2] ==
							verts[indices[earlier + j]].xyz[2]
						&& verts[indices[triangle + i]].xyz[0] ==
							verts[indices[earlier + previousJ]].xyz[0]
						&& verts[indices[triangle + i]].xyz[1] ==
							verts[indices[earlier + previousJ]].xyz[1]
						&& verts[indices[triangle + i]].xyz[2] ==
							verts[indices[earlier + previousJ]].xyz[2] ) {
						neighbors[triangle + previousI] = earlier;
						neighbors[earlier + previousJ] = triangle;
						goto nextPair;
					}
				}
			}
nextPair:
			;
		}
	}
}


/*
=============
AdjustTriangleUVs_r
=============
*/
static void AdjustTriangleUVs_r( drawVert_t *verts, unsigned short *indices,
	triShaderInfo_t *shader, int lightStyle, short *neighbors, int triangle,
	byte *visited ) {
	int i, other, u, v;
	double du, dv;
	visited[triangle] = 1;
	for ( i = 0; i < 3; i++ ) {
		other = neighbors[triangle + i];
		if ( other < 0 || visited[other] ) continue;
		if ( !CheckTriangleUVCompatibility( indices, other, verts, shader,
			lightStyle, triangle, &u, &v ) ) continue;
		du = u;
		dv = v;
		verts[indices[other]].st[0] -= du;
		verts[indices[other]].st[1] -= dv;
		verts[indices[other + 1]].st[0] -= du;
		verts[indices[other + 1]].st[1] -= dv;
		verts[indices[other + 2]].st[0] -= du;
		verts[indices[other + 2]].st[1] -= dv;
		AdjustTriangleUVs_r( verts, indices, shader, lightStyle, neighbors, other, visited );
	}
}


/*
=============
StitchTriangleUVs
=============
*/
static void StitchTriangleUVs( unsigned short *indices, int numIndices,
	drawVert_t *verts, int numVerts, triShaderInfo_t *shader, int lightStyle ) {
	short *neighbors;
	byte *visited;
	int i;
	neighbors = malloc( sizeof( *neighbors ) * numIndices );
	visited = malloc( numIndices );
	BuildTriangleNeighbors( verts, indices, numVerts, numIndices, neighbors );
	memset( visited, 0, numIndices );
	for ( i = 0; i < numIndices; i += 3 ) {
		if ( !visited[i] ) {
			AdjustTriangleUVs_r( verts, indices, shader, lightStyle, neighbors, i, visited );
		}
	}
	free( visited );
	free( neighbors );
}


/*
=============
CenterTextureCoordinates
=============
*/
static void CenterTextureCoordinates( drawVert_t *verts, int numVerts ) {
	vec2_t mins, maxs;
	int i;

	mins[0] = maxs[0] = verts[0].st[0];
	mins[1] = maxs[1] = verts[0].st[1];
	for ( i = 1; i < numVerts; i++ ) {
		if ( mins[0] > verts[i].st[0] ) mins[0] = verts[i].st[0];
		else if ( maxs[0] < verts[i].st[0] ) maxs[0] = verts[i].st[0];
		if ( mins[1] > verts[i].st[1] ) mins[1] = verts[i].st[1];
		else if ( maxs[1] < verts[i].st[1] ) maxs[1] = verts[i].st[1];
	}
	mins[0] = FloorToInt( ( mins[0] + maxs[0] ) * 0.5f );
	mins[1] = FloorToInt( ( mins[1] + maxs[1] ) * 0.5f );
	for ( i = 0; i < numVerts; i++ ) {
		verts[i].st[0] -= mins[0];
		verts[i].st[1] -= mins[1];
	}
}


/*
=============
MergeDuplicateVerts
=============
*/
static int MergeDuplicateVerts( drawVert_t *verts, int numVerts,
	unsigned short *indices, int numIndices, triGroupRecord_t *records,
	int lightStyle ) {
	int vertexTriangle[131072];
	int i, j, k, triangle;
	drawVert_t *current;

	if ( (unsigned)numVerts > 131072 ) {
		Error( "MergeDuplicateVerts: %i vertices > %i\n", numVerts, 131072 );
	}
	for ( i = 0, triangle = 0; i < numVerts; i += 3, triangle++ ) {
		vertexTriangle[i] = triangle;
		vertexTriangle[i + 1] = triangle;
		vertexTriangle[i + 2] = triangle;
	}
	i = 1;
	if ( numVerts > i ) {
		current = verts + i;
		do {
			for ( j = 0; j < i; j++ ) {
				if ( !CanMergeVerts( &verts[j], current,
					&records[vertexTriangle[j]], &records[vertexTriangle[i]],
					lightStyle ) ) continue;
				numVerts--;
				*current = verts[numVerts];
				vertexTriangle[i] = vertexTriangle[numVerts];
				for ( k = 0; k < numIndices; k++ ) {
					if ( indices[k] == i ) indices[k] = j;
					else if ( indices[k] == numVerts ) indices[k] = i;
				}
				/* Recheck the vertex moved into this slot. */
				goto nextVertex;
			}
			i++;
			current++;
nextVertex:
			;
		} while ( i < numVerts );
	}
	return numVerts;
}


/*
=============
MapDrawVertexIndices

Assign vertex slots by first appearance in the triangle index stream.
=============
*/
static void MapDrawVertexIndices( unsigned short *remap, int numVerts,
	unsigned short *indices, int numIndices ) {
	int i;
	int nextIndex;

	nextIndex = 1;
	memset( remap, 0, sizeof(*remap) * numVerts );
	for ( i = 1; i < numIndices; i++ ) {
		if ( !remap[indices[i]] && indices[i] != indices[0] ) {
			remap[indices[i]] = nextIndex++;
		}
		indices[i] = remap[indices[i]];
	}
	indices[0] = 0;
}


/*
=============
ReorderDrawVertexIndices
=============
*/
void ReorderDrawVertexIndices( drawVert_t *verts, int numVerts,
	unsigned short *indices, int numIndices ) {
	unsigned short *remap;
	drawVert_t *oldVerts;
	int i;

	remap = malloc( sizeof(int) * numVerts );
	oldVerts = malloc( sizeof(*oldVerts) * numVerts );
	memcpy( oldVerts, verts, sizeof(*oldVerts) * numVerts );
	MapDrawVertexIndices( remap, numVerts, indices, numIndices );
	for ( i = 0; i < numVerts; i++ ) {
		memcpy( &verts[remap[i]], &oldVerts[i], sizeof(*oldVerts) );
	}
	free( oldVerts );
	free( remap );
}


/*
=============
PrepareTriSoupVertices
=============
*/
static int PrepareTriSoupVertices( drawVert_t *verts, int numVerts,
	unsigned short *indices, int numIndices, triGroupRecord_t *records,
	int lightStyle ) {
	triShaderInfo_t *shader = records->shaderInfo;
	if ( !shader->clampTexture && !shader->flap ) {
		StitchTriangleUVs( indices, numIndices, verts, numVerts, shader, lightStyle );
		CenterTextureCoordinates( verts, numVerts );
	}
	return MergeDuplicateVerts( verts, numVerts, indices, numIndices, records, lightStyle );
}


/*
=============
TriEdgesShareVerts

Compare oppositely directed geometric edges of two indexed triangles.
=============
*/
static qboolean TriEdgesShareVerts( triSoup_t *soup, int earlierTriangle,
	int currentTriangle ) {
	unsigned short *indices = &drawIndexes[soup->firstIndex];
	drawVert_t *verts = &triangleSoupData[soup->firstVert];
	int i, j, previousCurrent, previousEarlier;

	for ( i = 0, previousCurrent = 2; i < 3; previousCurrent = i++ ) {
		for ( j = 0, previousEarlier = 2; j < 3; previousEarlier = j++ ) {
			if ( verts[indices[currentTriangle + previousCurrent]].xyz[0] ==
					verts[indices[earlierTriangle + j]].xyz[0]
				&& verts[indices[currentTriangle + previousCurrent]].xyz[1] ==
					verts[indices[earlierTriangle + j]].xyz[1]
				&& verts[indices[currentTriangle + previousCurrent]].xyz[2] ==
					verts[indices[earlierTriangle + j]].xyz[2] ) {
				if ( verts[indices[currentTriangle + i]].xyz[0] ==
					verts[indices[earlierTriangle + previousEarlier]].xyz[0]
				&& verts[indices[currentTriangle + i]].xyz[1] ==
					verts[indices[earlierTriangle + previousEarlier]].xyz[1]
				&& verts[indices[currentTriangle + i]].xyz[2] ==
					verts[indices[earlierTriangle + previousEarlier]].xyz[2] )
				return qtrue;
			}
		}
	}
	return qfalse;
}


/*
=============
CountTriGroups

Label the connected components, merging labels when an edge bridges them.
=============
*/
int CountTriGroups( triSoup_t *soup, unsigned short *groupIds ) {
	int current, previous, k;
	int nextGroupId = 1;
	int numGroups = 0;

	for ( current = 0; current < soup->numIndices / 3; current++ ) {
		groupIds[current] = 0;
		for ( previous = 0; previous < current; previous++ ) {
			if ( groupIds[current] != groupIds[previous]
				&& TriEdgesShareVerts( soup, previous * 3, current * 3 ) ) {
				if ( groupIds[current] ) {
					for ( k = 0; k < current; k++ ) {
						if ( groupIds[k] == groupIds[current] )
							groupIds[k] = groupIds[previous];
					}
					numGroups--;
				}
				groupIds[current] = groupIds[previous];
			}
		}
		if ( !groupIds[current] ) {
			groupIds[current] = nextGroupId++;
			numGroups++;
		}
	}
	return numGroups;
}


/*
=============
GroupTriangles

Split a soup into its connected triangle groups.
=============
*/
qboolean GroupTriangles( triSoup_t *soup ) {
	unsigned short *groupIds, *savedIndices;
	drawVert_t *savedVerts;
	short *remap;
	triSoup_t savedSoup, *groupSoup;
	int numGroups, indexCount, i, corner;

	groupIds = malloc( sizeof( *groupIds ) * ( soup->numIndices / 3 ) );
	numGroups = CountTriGroups( soup, groupIds );
	if ( numGroups == 1 ) {
		free( groupIds );
		return qfalse;
	}
	savedVerts = malloc( sizeof( *savedVerts ) * soup->numVerts );
	memcpy( savedVerts, &triangleSoupData[soup->firstVert],
		sizeof( *savedVerts ) * soup->numVerts );
	savedIndices = malloc( sizeof( *savedIndices ) * soup->numIndices );
	memcpy( savedIndices, &drawIndexes[soup->firstIndex],
		sizeof( *savedIndices ) * soup->numIndices );
	remap = malloc( sizeof( *remap ) * soup->numIndices );
	savedSoup = *soup;
	indexCount = soup->numIndices;
	numTriangleSurfaces--;
	for ( ; numGroups; numGroups-- ) {
		if ( numTriangleSurfaces == MAX_NATIVE_TRIANGLE_SOUPS )
			Error( "MAX_MAP_TRIANGLE_SURFS" );
		groupSoup = &triangleSurfaces[numTriangleSurfaces++];
		*groupSoup = savedSoup;
		groupSoup->firstIndex = numDrawIndexes;
		groupSoup->firstVert = numDrawVerts;
		groupSoup->numVerts = groupSoup->numIndices = 0;
		memset( remap, -1, sizeof( *remap ) * indexCount );
		while ( !groupSoup->numIndices ) {
			for ( i = 0; i < indexCount; i += 3 ) {
				if ( groupIds[savedIndices[i]] != 1 ) continue;
				for ( corner = 0; corner < 3; corner++ ) {
					if ( remap[savedIndices[i]] < 0 ) {
						triangleSoupData[groupSoup->firstVert + groupSoup->numVerts] =
							savedVerts[savedIndices[i]];
						remap[savedIndices[i]] = groupSoup->numVerts++;
					}
					drawIndexes[groupSoup->firstIndex + groupSoup->numIndices++] =
						remap[savedIndices[i]];
				}
			}
		}
		PartitionTriSoupRecursive( groupSoup, qfalse );
	}
	free( savedIndices );
	free( savedVerts );
	free( groupIds );
	return qtrue;
}


/*
=============
SplitSoupOnAxis
=============
*/
static qboolean SplitSoupOnAxis( triSoup_t *soup, int axis, float splitValue ) {
	drawVert_t *vertexBuffers[2];
	unsigned short *indexBuffers[2], *remaps[2];
	byte *belowFlags, *aboveFlags;
	int vertexCounts[2];
	int indexCounts[2];
	int i, j, vertex;
	byte below, above;
	qboolean result;
	triSoup_t *otherSoup;
	drawVert_t *sourceVertex;

	memset( indexCounts, 0, sizeof(indexCounts) );
	memset( vertexCounts, 0, sizeof(vertexCounts) );
	for ( i = 0; i < 2; i++ ) {
		vertexBuffers[i] = malloc( sizeof(drawVert_t) * soup->numVerts );
		indexBuffers[i] = malloc( sizeof(unsigned short) * soup->numIndices );
		remaps[i] = malloc( sizeof(unsigned short) * soup->numIndices );
	}
	belowFlags = malloc( soup->numVerts );
	memset( belowFlags, 0, soup->numVerts );
	aboveFlags = malloc( soup->numVerts );
	memset( aboveFlags, 0, soup->numVerts );
	for ( i = 0; i < soup->numIndices; i += 3 ) {
		below = 0; above = 0;
		for ( j = 0; j < 3; j++ ) {
			vertex = (short)drawIndexes[soup->firstIndex + i + j];
			sourceVertex = &triangleSoupData[soup->firstVert + vertex];
			if ( sourceVertex->xyz[axis] < splitValue - 0.001f ) below = 1;
			else if ( sourceVertex->xyz[axis] > splitValue + 0.001f ) above = 1;
		}
		if ( below == above ) {
			if ( vertexCounts[0] < vertexCounts[1] ) {
				below = 1;
				above = 0;
			}
			else {
				below = 0;
				above = 1;
			}
		}
		vertexCounts[0] += below;
		vertexCounts[1] += above;
		for ( j = 0; j < 3; j++ ) {
			belowFlags[(short)drawIndexes[soup->firstIndex + i + j]] |= below;
			aboveFlags[(short)drawIndexes[soup->firstIndex + i + j]] |= above;
		}
	}
	vertexCounts[0] = 0; vertexCounts[1] = 0;
	for ( i = 0; i < soup->numVerts; i++ ) {
		sourceVertex = &triangleSoupData[soup->firstVert + i];
		if ( belowFlags[i] ) {
			memcpy( &vertexBuffers[0][vertexCounts[0]],
				sourceVertex, sizeof(drawVert_t) );
			remaps[0][i] = vertexCounts[0]++;
		}
		if ( aboveFlags[i] ) {
			memcpy( &vertexBuffers[1][vertexCounts[1]],
				sourceVertex, sizeof(drawVert_t) );
			remaps[1][i] = vertexCounts[1]++;
		}
	}
	result = qfalse;
	if ( vertexCounts[0] < soup->numVerts && vertexCounts[1] < soup->numVerts ) {
		result = qtrue;
		for ( i = 0; i < soup->numIndices; i += 3 ) {
			below = 1; above = 1;
			for ( j = 0; j < 3; j++ ) {
				vertex = (short)drawIndexes[soup->firstIndex + i + j];
				if ( !belowFlags[vertex] ) below = 0;
				if ( !aboveFlags[vertex] ) above = 0;
			}
			if ( below && above && indexCounts[0] >= indexCounts[1] ) above = 0;
			{
				unsigned short *map = remaps[above];
				unsigned short *output = indexBuffers[above];
				int count = indexCounts[above];
				for ( j = 0; j < 3; j++ ) {
					output[count + j] = map[(short)drawIndexes[soup->firstIndex + i + j]];
				}
				indexCounts[above] += 3;
			}
		}
		if ( numTriangleSurfaces == MAX_NATIVE_TRIANGLE_SOUPS ) Error( "MAX_MAP_TRIANGLE_SURFS" );
		otherSoup = &triangleSurfaces[numTriangleSurfaces++];
		*otherSoup = *soup;
		soup->numVerts = vertexCounts[0];
		soup->numIndices = indexCounts[0];
		memcpy( &triangleSoupData[soup->firstVert], vertexBuffers[0],
			sizeof(drawVert_t) * soup->numVerts );
		memcpy( &drawIndexes[soup->firstIndex], indexBuffers[0],
			sizeof(unsigned short) * soup->numIndices );
		PartitionTriSoupRecursive( soup, qfalse );
		otherSoup->numVerts = vertexCounts[1];
		otherSoup->firstVert = numDrawVerts;
		otherSoup->numIndices = indexCounts[1];
		otherSoup->firstIndex = numDrawIndexes;
		memcpy( &triangleSoupData[otherSoup->firstVert], vertexBuffers[1],
			sizeof(drawVert_t) * otherSoup->numVerts );
		memcpy( &drawIndexes[otherSoup->firstIndex], indexBuffers[1],
			sizeof(unsigned short) * otherSoup->numIndices );
		PartitionTriSoupRecursive( otherSoup, qfalse );
	}
	free( aboveFlags );
	free( belowFlags );
	for ( i = 1; i >= 0; i-- ) {
		free( indexBuffers[i] );
		free( vertexBuffers[i] );
	}
	return result;
}


/*
=============
PartitionTriSoupRecursive
=============
*/
void PartitionTriSoupRecursive( triSoup_t *soup, qboolean tryGroupSplit ) {
	vec3_t mins, maxs, extents;
	int i, axis, second, third;
	int result;

	if ( soup->numVerts <= MAX_TRI_SOUP_VERTS && soup->numIndices <= MAX_TRI_SOUP_INDICES ) {
		ReorderDrawVertexIndices( &triangleSoupData[soup->firstVert],
			soup->numVerts, &drawIndexes[soup->firstIndex], soup->numIndices );
		numDrawIndexes += soup->numIndices;
		numDrawVerts = soup->firstVert + soup->numVerts;
		return;
	}
	if ( tryGroupSplit && GroupTriangles( soup ) ) return;
	ClearBounds( mins, maxs );
	for ( i = 0; i < soup->numVerts; i++ ) {
		AddPointToBounds( triangleSoupData[soup->firstVert + i].xyz, mins, maxs );
	}
	VectorSubtract( maxs, mins, extents );
	axis = 0;
	if ( extents[axis] < extents[1] ) axis = 1;
	if ( extents[axis] < extents[2] ) axis = 2;
	result = SplitSoupOnAxis( soup, axis, (mins[axis] + maxs[axis]) * 0.5f );
	if ( result ) return;
	second = (~axis) & 1;
	third = (~axis) & 2;
	if ( extents[second] < extents[third] ) {
		/* Visit the longer of the two remaining axes first. */
		/* the original swaps with xor; a temporary behaves the same */
		second ^= third;
		third ^= second;
		second ^= third;
	}
	result = SplitSoupOnAxis( soup, second, (mins[second] + maxs[second]) * 0.5f );
	if ( result ) return;
	result = SplitSoupOnAxis( soup, third, (mins[third] + maxs[third]) * 0.5f );
	if ( result ) return;
	Error( "couldn't partition large draw surface on any axis" );
}


/*
=============
EmitTriangleSoup
=============
*/
void EmitTriangleSoup( triGroupRecord_t *records, int numTris, tree_t *tree ) {
	triSoup_t *soup;
	triGroupRecord_t *record;
	winding_t *w;
	vec4_t plane;
	int first, end;

	for ( first = 0; first < numTris; first = end ) {
		if ( numTriangleSurfaces == MAX_NATIVE_TRIANGLE_SOUPS ) {
			Error( "MAX_MAP_TRIANGLE_SURFS" );
		}
		soup = &triangleSurfaces[numTriangleSurfaces++];
		soup->material = EmitShader( records[first].shaderInfo->shader,
			records[first].shaderInfo->contents );
		soup->lightStyle = records[first].lightStyle;
		soup->numVerts = 0;
		soup->numIndices = 0;
		soup->firstVert = numDrawVerts;
		soup->firstIndex = numDrawIndexes;
		for ( end = first; end < numTris; end++ ) {
			record = &records[end];
			if ( record->groupId != records[first].groupId ) break;
			drawIndexes[soup->firstIndex + soup->numIndices++] =
				EmitDrawVert( soup, record->shaderInfo, record->lightStyle,
					&triIntermediateVerts[record->vertIdx[0]] );
			drawIndexes[soup->firstIndex + soup->numIndices++] =
				EmitDrawVert( soup, record->shaderInfo, record->lightStyle,
					&triIntermediateVerts[record->vertIdx[1]] );
			drawIndexes[soup->firstIndex + soup->numIndices++] =
				EmitDrawVert( soup, record->shaderInfo, record->lightStyle,
					&triIntermediateVerts[record->vertIdx[2]] );
			if ( !entity_num && records[first].cullGroupIdx >= 0 ) {
				w = AllocWinding( 3 );
				w->numpoints = 3;
				VectorCopy( triIntermediateVerts[record->vertIdx[0]].xyz, w->p[0] );
				VectorCopy( triIntermediateVerts[record->vertIdx[1]].xyz, w->p[1] );
				VectorCopy( triIntermediateVerts[record->vertIdx[2]].xyz, w->p[2] );
				AddPointToBounds( w->p[0], cullGroups[records[first].cullGroupIdx].mins,
					cullGroups[records[first].cullGroupIdx].maxs );
				AddPointToBounds( w->p[1], cullGroups[records[first].cullGroupIdx].mins,
					cullGroups[records[first].cullGroupIdx].maxs );
				AddPointToBounds( w->p[2], cullGroups[records[first].cullGroupIdx].mins,
					cullGroups[records[first].cullGroupIdx].maxs );
				PlaneFromPoints( plane, w->p[0], w->p[1], w->p[2] );
				FilterTriWindingIntoCells( w, plane, record, tree->headnode );
			}
		}
		soup->numVerts = PrepareTriSoupVertices( &triangleSoupData[soup->firstVert], soup->numVerts,
			&drawIndexes[soup->firstIndex], soup->numIndices, &records[first], records[first].lightStyle );
		PartitionTriSoupRecursive( soup, qtrue );
	}
}


//===========================================================================
// triangle-soup AABB tree
//===========================================================================

float *g_aabbSortMin, *g_aabbSortMax, *g_aabbSortEqual;



/*
=============
AabbFloatCompare

Floating split-event ordering, including unordered values returning zero.
=============
*/
int AabbFloatCompare( const void *a, const void *b ) {
	float difference;

	difference = *(const float *)a - *(const float *)b;
	if ( difference < 0.0f ) {
		return -1;
	}
	if ( difference > 0.0f ) {
		return 1;
	}
	return 0;
}

/*
=============
AabbFindBestSplitPlane
=============
*/
static qboolean AabbFindBestSplitPlane( vec3_t *itemMins, vec3_t *itemMaxs,
	int *indices, int itemCount, int *splitAxis, float *splitPosition ) {
	vec3_t maxBounds;
	vec3_t minBounds;
	int bonuses[3];
	int shortestAxis, axis, i, item, planeCount, intervalCount;
	int minIndex, maxIndex, planeIndex;
	int front, back, crossing, onPlane, previousPlanes, starts;
	int score, bestScore;
	float position, nextPosition;

	ClearBounds( minBounds, maxBounds );
	for ( i = 0; i < itemCount; i++ ) {
		item = indices[i];
		ExpandBounds( itemMins[item], itemMaxs[item], minBounds, maxBounds );
	}
	shortestAxis = 0;
	if ( maxBounds[1] - minBounds[1] < maxBounds[0] - minBounds[0] ) shortestAxis = 1;
	if ( maxBounds[2] - minBounds[2] < maxBounds[shortestAxis] - minBounds[shortestAxis] ) shortestAxis = 2;
	for ( i = 0; i < 3; i++ ) {
		bonuses[i] = CeilToInt( ( maxBounds[i] - minBounds[i] + 1.0f ) * 10.0f /
			( maxBounds[shortestAxis] - minBounds[shortestAxis] + 1.0f ) );
	}
	bestScore = INT_MIN;
	for ( axis = 0; axis < 3; axis++ ) {
		planeCount = intervalCount = 0;
		for ( i = 0; i < itemCount; i++ ) {
			if ( itemMins[indices[i]][axis] == itemMaxs[indices[i]][axis] ) {
				g_aabbSortEqual[planeCount++] = itemMins[indices[i]][axis];
			} else {
				g_aabbSortMin[intervalCount] = itemMins[indices[i]][axis];
				g_aabbSortMax[intervalCount++] = itemMaxs[indices[i]][axis];
			}
		}
		qsort( g_aabbSortMin, intervalCount, sizeof(float), AabbFloatCompare );
		qsort( g_aabbSortMax, intervalCount, sizeof(float), AabbFloatCompare );
		qsort( g_aabbSortEqual, planeCount, sizeof(float), AabbFloatCompare );
		front = 0;
		back = itemCount;
		crossing = 0;
		minIndex = 0;
		maxIndex = 0;
		planeIndex = 0;
		previousPlanes = 0;
		onPlane = 0;
		starts = 0;
		nextPosition = g_aabbSortMin[0] < g_aabbSortEqual[0] ? g_aabbSortMin[0] : g_aabbSortEqual[0];
		while ( nextPosition < FLT_MAX ) {
			position = nextPosition;
			nextPosition = FLT_MAX;
			crossing += starts;
			back -= starts;
			starts = 0;
			while ( minIndex < intervalCount && g_aabbSortMin[minIndex] == position ) {
				starts++;
				minIndex++;
			}
			if ( minIndex < intervalCount && g_aabbSortMin[minIndex] < nextPosition )
				nextPosition = g_aabbSortMin[minIndex];
			while ( maxIndex < intervalCount && g_aabbSortMax[maxIndex] == position ) {
				front++;
				crossing--;
				maxIndex++;
			}
			if ( maxIndex < intervalCount && g_aabbSortMax[maxIndex] < nextPosition )
				nextPosition = g_aabbSortMax[maxIndex];
			front += previousPlanes;
			onPlane -= previousPlanes;
			previousPlanes = 0;
			while ( planeIndex < planeCount && g_aabbSortEqual[planeIndex] == position ) {
				previousPlanes++;
				planeIndex++;
			}
			back -= previousPlanes;
			onPlane += previousPlanes;
			if ( planeIndex < planeCount && g_aabbSortEqual[planeIndex] < nextPosition )
				nextPosition = g_aabbSortEqual[planeIndex];
			if ( front > 1 && back > 1 ) {
				score = itemCount + bonuses[axis] - 4 * crossing - onPlane - abs( front - back );
				if ( !onPlane && !crossing && !starts )
					score += RoundToInt( nextPosition - position );
				if ( score > bestScore ) {
					bestScore = score;
					*splitAxis = axis;
					if ( !onPlane && !crossing && !starts )
						*splitPosition = ( position + nextPosition ) * 0.5f;
					else
						*splitPosition = position;
				}
			}
		}
	}
	return bestScore != INT_MIN;
}

/*
=============
AabbBoundsVolumeGrowth

Volume added when an item's bounds are included in a partition.
=============
*/
static float AabbBoundsVolumeGrowth( const vec3_t mins, const vec3_t maxs,
	const vec3_t itemMins, const vec3_t itemMaxs ) {
	vec3_t newMins;
	vec3_t newMaxs;
	double oldVolume;

	VectorCopy( mins, newMins );
	VectorCopy( maxs, newMaxs );
	ExpandBounds( itemMins, itemMaxs, newMins, newMaxs );
	oldVolume = ( maxs[0] - mins[0] ) * ( maxs[2] - mins[2] );
	oldVolume *= maxs[1] - mins[1];
	return ( newMaxs[0] - newMins[0] ) * ( newMaxs[1] - newMins[1] ) *
		( newMaxs[2] - newMins[2] ) -
		oldVolume;
}

/*
=============
AabbPartitionItems
=============
*/
static qboolean AabbPartitionItems( int itemCount, vec3_t *itemMins,
	vec3_t *itemMaxs, int *indices, int *frontCount, int *backStart ) {
	int front, back, scan, item, frontItem, temp;
	vec3_t childBounds[2][2];
	int splitAxis;
	float splitPosition;

	if ( !AabbFindBestSplitPlane( itemMins, itemMaxs, indices, itemCount,
		&splitAxis, &splitPosition ) ) return qfalse;
	ClearBounds( childBounds[0][0], childBounds[0][1] );
	ClearBounds( childBounds[1][0], childBounds[1][1] );
	front = 0;
	back = itemCount - 1;
	if ( back < 0 ) return qfalse;

	for ( ;; ) {
		while ( front <= back && ( itemMaxs[indices[front]][splitAxis] <= splitPosition && itemMins[indices[front]][splitAxis] < splitPosition ) ) {
			ExpandBounds( itemMins[indices[front]], itemMaxs[indices[front]], childBounds[0][0], childBounds[0][1] );
			front++;
		}
		while ( front <= back && ( itemMins[indices[back]][splitAxis] >= splitPosition && itemMaxs[indices[back]][splitAxis] > splitPosition ) ) {
			ExpandBounds( itemMins[indices[back]], itemMaxs[indices[back]], childBounds[1][0], childBounds[1][1] );
			back--;
		}
		if ( front > back ) break;
		frontItem = indices[front];
		if ( !( itemMins[frontItem][splitAxis] >= splitPosition && itemMaxs[frontItem][splitAxis] > splitPosition ) && !( itemMaxs[indices[back]][splitAxis] <= splitPosition && itemMins[indices[back]][splitAxis] < splitPosition ) ) {
			for ( scan = front; scan < back; scan++ ) {
				item = indices[scan];
				if ( itemMins[item][splitAxis] >= splitPosition && itemMaxs[item][splitAxis] > splitPosition ) {
					temp = indices[scan];
					indices[scan] = indices[back];
					indices[back] = temp;
					break;
				}
				if ( itemMaxs[item][splitAxis] <= splitPosition && itemMins[item][splitAxis] < splitPosition ) {
					temp = indices[scan];
					indices[scan] = frontItem;
					indices[front] = temp;
					break;
				}
			}
			if ( scan == back ) break;
			continue;
		}
		indices[front] = indices[back];
		indices[back] = frontItem;
	}

	if ( front <= back && ( front < 4 || back - front + 1 < 4 || itemCount - back - 1 < 4 ) ) {
		while ( front <= back ) {
			while ( front <= back && AabbBoundsVolumeGrowth( childBounds[0][0], childBounds[0][1], itemMins[indices[front]], itemMaxs[indices[front]] ) <= AabbBoundsVolumeGrowth( childBounds[1][0], childBounds[1][1], itemMins[indices[front]], itemMaxs[indices[front]] ) ) {
				ExpandBounds( itemMins[indices[front]], itemMaxs[indices[front]], childBounds[0][0], childBounds[0][1] );
				front++;
			}
			while ( front <= back && AabbBoundsVolumeGrowth( childBounds[1][0], childBounds[1][1], itemMins[indices[back]], itemMaxs[indices[back]] ) <= AabbBoundsVolumeGrowth( childBounds[0][0], childBounds[0][1], itemMins[indices[back]], itemMaxs[indices[back]] ) ) {
				ExpandBounds( itemMins[indices[back]], itemMaxs[indices[back]], childBounds[1][0], childBounds[1][1] );
				back--;
			}
			if ( front < back ) {
				temp = indices[front];
				indices[front] = indices[back];
				indices[back] = temp;
				front++;
				back--;
			} else if ( front == back ) {
				if ( 2 * front < itemCount ) front++;
				else back--;
			}
		}
	}
	if ( !front || front == itemCount ) return qfalse;
	*frontCount = front;
	*backStart = back + 1;
	return qtrue;
}

/*
=============
AabbBuildSubtree

Appends the front, optional middle, and back child nodes for one tri-soup
range.  Unsplittable ranges and ranges of at most sixteen soups become a
single leaf.
=============
*/
static void AabbBuildSubtree( triSoupAabbNode_t *parent,
	vec3_t *itemMins, vec3_t *itemMaxs, int *indices,
	int itemOffset, int itemCount ) {
	int frontCount;
	int backStart;
	triSoupAabbNode_t *node;

	if ( itemCount > TRISOUP_AABB_LEAF_LIMIT
		&& AabbPartitionItems( itemCount, itemMins, itemMaxs,
			indices + itemOffset, &frontCount, &backStart ) ) {
		node = &daabbtrees[numaabbtreenodes++];
		node->firstSurface = parent->firstSurface + itemOffset;
		node->surfaceCount = frontCount;

		if ( frontCount < backStart ) {
			node = &daabbtrees[numaabbtreenodes++];
			node->firstSurface = parent->firstSurface + itemOffset + frontCount;
			node->surfaceCount = backStart - frontCount;
		}

		node = &daabbtrees[numaabbtreenodes++];
		node->firstSurface = parent->firstSurface + itemOffset + backStart;
		node->surfaceCount = itemCount - backStart;
		return;
	}

	node = &daabbtrees[numaabbtreenodes++];
	node->firstSurface = parent->firstSurface + itemOffset;
	node->surfaceCount = itemCount;
}


/*
=============
AabbBuildTree_r

Builds the tri-soup AABB tree recursively.  The firstSurface values also provide
relative offsets into the reordered index array for recursive descendants.
=============
*/
void AabbBuildTree_r( triSoupAabbNode_t *node, vec3_t *itemMins,
	vec3_t *itemMaxs, int *indices ) {
	int i;
	triSoupAabbNode_t *children;

	{
		int frontCount;
		int backStart;

		node->childCount = 0;
		if ( node->surfaceCount <= TRISOUP_AABB_LEAF_LIMIT ) {
			return;
		}
		if ( !AabbPartitionItems( node->surfaceCount, itemMins, itemMaxs,
			indices, &frontCount, &backStart ) ) {
			return;
		}

		children = &daabbtrees[numaabbtreenodes];
		node->childCount = numaabbtreenodes;

		AabbBuildSubtree( node, itemMins, itemMaxs, indices, 0, frontCount );
		if ( frontCount < backStart ) {
			AabbBuildSubtree( node, itemMins, itemMaxs, indices,
				frontCount, backStart - frontCount );
		}
		AabbBuildSubtree( node, itemMins, itemMaxs, indices,
			backStart, node->surfaceCount - backStart );
	}

	node->childCount = numaabbtreenodes - node->childCount;
	for ( i = 0; i < node->childCount; i++ ) {
		AabbBuildTree_r( &children[i], itemMins, itemMaxs,
			indices + children[i].firstSurface - node->firstSurface );
	}
}


/*
=============
BuildTriSoupAABBTree
=============
*/
static void BuildTriSoupAABBTree( triSoupAabbNode_t *root ) {
	struct {
		int indices[TRI_AABB_LOCAL_ITEMS];
		float sortMins[TRI_AABB_LOCAL_ITEMS];
		float sortMaxs[TRI_AABB_LOCAL_ITEMS];
		float sortEqual[TRI_AABB_LOCAL_ITEMS];
		vec3_t mins[TRI_AABB_LOCAL_ITEMS];
		vec3_t maxs[TRI_AABB_LOCAL_ITEMS];
	} local;
	int *indices;
	vec3_t *bounds[2];
	triSoup_t *soup;
	triSoup_t *originalSoups;
	int i;
	int j;

	if ( root->surfaceCount <= 16 ) {
		root->childCount = 0;
		return;
	}
	if ( (unsigned)root->surfaceCount <= TRI_AABB_LOCAL_ITEMS ) {
		indices = local.indices;
		bounds[0] = local.mins;
		bounds[1] = local.maxs;
		g_aabbSortMin = local.sortMins;
		g_aabbSortMax = local.sortMaxs;
		g_aabbSortEqual = local.sortEqual;
	} else {
		indices = malloc( root->surfaceCount * sizeof(*indices) );
		bounds[0] = malloc( root->surfaceCount * sizeof(*bounds[0]) );
		bounds[1] = malloc( root->surfaceCount * sizeof(*bounds[1]) );
		g_aabbSortMin = malloc( root->surfaceCount * sizeof(*g_aabbSortMin) );
		g_aabbSortMax = malloc( root->surfaceCount * sizeof(*g_aabbSortMax) );
		g_aabbSortEqual = malloc( root->surfaceCount * sizeof(*g_aabbSortEqual) );
	}
	for ( i = 0; i < root->surfaceCount; i++ ) {
		indices[i] = i;
		soup = &triangleSurfaces[root->firstSurface + i];
		ClearBounds( bounds[0][i], bounds[1][i] );
		for ( j = 0; j < soup->numVerts; j++ ) {
			AddPointToBounds( triangleSoupData[soup->firstVert + j].xyz,
				bounds[0][i], bounds[1][i] );
		}
	}
	AabbBuildTree_r( root, bounds[0], bounds[1], indices );
	soup = &triangleSurfaces[root->firstSurface];
	originalSoups = malloc( root->surfaceCount * sizeof(*originalSoups) );
	memcpy( originalSoups, soup, root->surfaceCount * sizeof(*soup) );
	for ( i = 0; i < root->surfaceCount; i++ ) {
		soup[i] = originalSoups[indices[i]];
	}
	free( originalSoups );
	if ( bounds[0] != local.mins ) {
		free( indices );
		free( bounds[0] );
		free( bounds[1] );
		free( g_aabbSortMin );
		free( g_aabbSortMax );
		free( g_aabbSortEqual );
	}
}


/*
=============
BuildTriangleSoups
=============
*/
void BuildTriangleSoups( tree_t *tree ) {
	int first;
	int end;
	int currentCell;
	int currentCullGroup;
	triSoupAabbNode_t *root;
	triCullGroup_t *cullGroup;
	int i;

	if ( !entity_num ) {
		memset( cellCullGroupBits, 0, sizeof(cellCullGroupBits) );
		for ( i = 0; i < numCullGroups; i++ ) {
			ClearBounds( cullGroups[i].mins, cullGroups[i].maxs );
		}
	}
	qsort( triRecords, numTriangles, sizeof(*triRecords), CompareTriRecordHeaders );
	currentCell = -1;
	currentCullGroup = -1;
	for ( first = 0; first < numTriangles; first = end ) {
		if ( triRecords[first].removed ) {
			break;
		}
		for ( end = first + 1; end < numTriangles && end - first < 32768; end++ ) {
			if ( triRecords[end].drawOrder != triRecords[first].drawOrder ||
				triRecords[end].cullGroupIdx != triRecords[first].cullGroupIdx ||
				triRecords[end].shaderInfo != triRecords[first].shaderInfo ||
				triRecords[end].lightStyle != triRecords[first].lightStyle ) {
				break;
			}
		}
		if ( !entity_num ) {
			if ( triRecords[first].drawOrder >= 0 ) {
				if ( currentCell != triRecords[first].drawOrder ) {
					currentCell = triRecords[first].drawOrder;
					currentCullGroup = -1;
					if ( numaabbtreenodes > 0 ) {
						BuildTriSoupAABBTree( &daabbtrees[numaabbtreenodes - 1] );
					}
					dcells[currentCell].aabbTreeIndex = numaabbtreenodes;
					daabbtrees[numaabbtreenodes].firstSurface = numTriangleSurfaces;
					daabbtrees[numaabbtreenodes].surfaceCount = 0;
					daabbtrees[numaabbtreenodes].childCount = 0;
					numaabbtreenodes++;
				}
			} else if ( currentCullGroup != triRecords[first].cullGroupIdx ) {
				currentCullGroup = triRecords[first].cullGroupIdx;
				currentCell = -1;
				cullGroups[currentCullGroup].firstSurface = numTriangleSurfaces;
				cullGroups[currentCullGroup].surfaceCount = 0;
			}
		}
		GroupAndMergeTriSubgroups( &triRecords[first], end - first );
		EmitTriangleSoup( &triRecords[first], end - first, tree );
		if ( !entity_num ) {
			if ( triRecords[first].drawOrder >= 0 ) {
				root = &daabbtrees[dcells[triRecords[first].drawOrder].aabbTreeIndex];
				root->surfaceCount = numTriangleSurfaces - root->firstSurface;
			} else {
				cullGroup = &cullGroups[triRecords[first].cullGroupIdx];
				cullGroup->surfaceCount = numTriangleSurfaces - cullGroup->firstSurface;
			}
		}
	}
	if ( currentCell >= 0 && numaabbtreenodes > 0 ) {
		BuildTriSoupAABBTree( &daabbtrees[numaabbtreenodes - 1] );
	}
}


#define TRIS_AFFINE_COMPONENTS( row, point ) \
    ( (point)[2] * (row)[2] + (point)[1] * (row)[1] + (point)[0] * (row)[0] + (row)[3] )

/*
=============
EmitTriRecord
=============
*/
static void EmitTriRecord( winding_t *winding, int first, int second, int third, winding_t *originalWinding, int cellIndex,
    int cullGroupIndex, triSurfProps_t *props ) {
    vec4_t plane, texS, texT;
    triIntermediateVert_t *vertices, *vertex;
    int i;
    float value;
    int alpha;
    if ( !PlaneFromPoints( plane, winding->p[first], winding->p[second], winding->p[third] ) ) return;
    if ( numTriangles >= 524288 ) Error( "MAX_MAP_TRIANGLES (%i) exceeded\n", 524288 );
    if ( numTriIntermediateVerts + 3 > 1572864 ) Error( "MAX_MAP_VERTEXES (%i) exceeded\n", 1572864 );
    TRIS_COPY4( props->texVecs[0], texS );
    TRIS_COPY4( props->texVecs[1], texT );
    if ( !props->shaderInfo->clampTexture ) {
        texS[3] -= FloorToInt( texS[3] );
        texT[3] -= FloorToInt( texT[3] );
    }
    vertices = &triIntermediateVerts[numTriIntermediateVerts];
    VectorCopy( originalWinding->p[first], vertices[0].xyz );
    VectorCopy( originalWinding->p[second], vertices[1].xyz );
    VectorCopy( originalWinding->p[third], vertices[2].xyz );
    for ( i = 0; i < 3; i++ ) {
        vertex = &vertices[i];
        value = texS[2] * vertex->xyz[2];
        value += texS[0] * vertex->xyz[0];
        value += texS[1] * vertex->xyz[1];
        vertex->st[0] = value + texS[3];
        value = texT[2] * vertex->xyz[2];
        value += texT[0] * vertex->xyz[0];
        value += texT[1] * vertex->xyz[1];
        vertex->st[1] = value + texT[3];
        value = props->lmapVecs[0][1] * vertex->xyz[1];
        value += props->lmapVecs[0][2] * vertex->xyz[2];
        /* the original computes this in double precision; kept so the build matches the original binary */
        value += (double)( props->lmapVecs[0][0] * vertex->xyz[0] );
        vertices[i].lightmap[0] = value + props->lmapVecs[0][3];
        value = props->lmapVecs[1][2] * vertex->xyz[2];
        value += props->lmapVecs[1][1] * vertex->xyz[1];
        value += props->lmapVecs[1][0] * vertex->xyz[0];
        vertices[i].lightmap[1] = value + props->lmapVecs[1][3];
        vertices[i].normal[0] = plane[0];
        vertices[i].normal[1] = plane[1];
        vertex->normal[2] = plane[2];
        vertex->smoothAngle = props->smoothAngle;
        vertex->color[0] = TRIS_MAX( 0, TRIS_MIN( 255, RoundToInt( TRIS_AFFINE_COMPONENTS( props->colorVecs[0], vertex->xyz ) ) ) );
        vertex->color[1] = TRIS_MAX( 0, TRIS_MIN( 255, RoundToInt( TRIS_AFFINE_COMPONENTS( props->colorVecs[1], vertex->xyz ) ) ) );
        vertex->color[2] = TRIS_MAX( 0, TRIS_MIN( 255, RoundToInt( TRIS_AFFINE_COMPONENTS( props->colorVecs[2], vertex->xyz ) ) ) );
        alpha = TRIS_MAX( 0, TRIS_MIN( 255, RoundToInt( TRIS_AFFINE_COMPONENTS( props->colorVecs[3], vertex->xyz ) ) ) );
        value = vertices[i].lightmap[0];
        vertex->color[3] = alpha;
        if ( value < 0.0f ) vertices[i].lightmap[0] = 0.0f;
        else if ( vertices[i].lightmap[0] > 1.0f ) vertices[i].lightmap[0] = 1.0f;
        if ( vertices[i].lightmap[1] < 0.0f ) vertices[i].lightmap[1] = 0.0f;
        else if ( vertices[i].lightmap[1] > 1.0f ) vertices[i].lightmap[1] = 1.0f;
    }
    VectorCopy( winding->p[first], vertices[0].xyz );
    VectorCopy( winding->p[second], vertices[1].xyz );
    VectorCopy( winding->p[third], vertices[2].xyz );
    triRecords[numTriangles].drawOrder = cellIndex;
    triRecords[numTriangles].cullGroupIdx = cullGroupIndex;
    triRecords[numTriangles].shaderInfo = props->shaderInfo;
    triRecords[numTriangles].lightStyle = props->lightStyle;
    triRecords[numTriangles].vertIdx[0] = numTriIntermediateVerts;
    triRecords[numTriangles].vertIdx[1] = numTriIntermediateVerts + 1;
    triRecords[numTriangles].vertIdx[2] = numTriIntermediateVerts + 2;
    triRecords[numTriangles].removed = qfalse;
    TRIS_COPY4( texS, triRecords[numTriangles].texVecs[0] );
    TRIS_COPY4( texT, triRecords[numTriangles].texVecs[1] );
    TRIS_COPY4( props->lmapVecs[0], triRecords[numTriangles].lmapVecs[0] );
    TRIS_COPY4( props->lmapVecs[1], triRecords[numTriangles].lmapVecs[1] );
    TRIS_COPY4( props->colorVecs[0], triRecords[numTriangles].colorVecs[0] );
    TRIS_COPY4( props->colorVecs[1], triRecords[numTriangles].colorVecs[1] );
    TRIS_COPY4( props->colorVecs[2], triRecords[numTriangles].colorVecs[2] );
    TRIS_COPY4( props->colorVecs[3], triRecords[numTriangles].colorVecs[3] );
    numTriangles++;
    numTriIntermediateVerts += 3;
}

#undef TRIS_AFFINE_COMPONENTS

/*
=============
TesselateWinding
=============
*/
void TesselateWinding( triSurf_t *surface, int cellIndex, int cullGroupIndex ) {
    winding_t *winding, *originalWinding;
    const float *normal;
    double bestCosine, sideAC[3], sideCB[3], sideBA[3], cosine;
    qboolean valid;
    int removePrevious;
    int beforePrevious, afterNext;
    int pass, scanPrevious, firstScan, scanNext, scan;
    /* previous is initialised here as in the original; kept so the build matches the original binary */
    int axisX, bestEar, count, ear, axisY, previous = 0, next;

    winding = surface->winding;
    originalWinding = surface->originalWinding;
    normal = surface->props->plane;
    if ( winding->numpoints == 3 ) {
        EmitTriRecord( winding, 0, 1, 2, originalWinding,
            cellIndex, cullGroupIndex, surface->props );
        return;
    }
    TrisInsertSplitPoints( &winding, normal, &originalWinding );
    TrisRemoveDegenerateEdges( winding, originalWinding );
    if ( winding->numpoints < 3 )
        return;
    surface->originalWinding = originalWinding;
    surface->winding = winding;
    GetProjectionAxes( normal, &axisX, &axisY );
    while ( winding->numpoints > 3 ) {
        bestEar = -1;
        count = winding->numpoints;
        bestCosine = FLT_MAX;
        for ( pass = 0; bestEar == -1; ++pass ) {
            if ( pass > 1 ) break;
            ear = 0;
            if ( ear < count ) {
                do {
                    next = ( ear + 1 ) % count;
                    previous = ( ear + winding->numpoints - 1 ) % winding->numpoints;
                    SetupTriSide( winding->p[previous], winding->p[next], axisX, axisY, sideBA );
                    if ( !( winding->p[ear][axisY] * sideBA[0]
                    - winding->p[ear][axisX] * sideBA[1] <= sideBA[2] ) ) {
                        SetupTriSide( winding->p[next], winding->p[ear], axisX, axisY, sideAC );
                        SetupTriSide( winding->p[ear], winding->p[previous], axisX, axisY, sideCB );
                        cosine = TrisLargestAngleCosine( sideBA, sideAC, sideCB );
                        if ( !( cosine >= bestCosine ) ) {
                            firstScan = ( next + 1 ) % count;
                            scanPrevious = next, scan = ( next + 1 ) % count, scanNext = ( next + 2 ) % count;
                            valid = qtrue;
                            while ( scan != previous ) {
                                if ( !( sideBA[0] * winding->p[scan][axisY]
                                - winding->p[scan][axisX] * sideBA[1] < sideBA[2]
                                || winding->p[scan][axisY] * sideAC[0]
                                - winding->p[scan][axisX] * sideAC[1] < sideAC[2]
                                || winding->p[scan][axisY] * sideCB[0]
                                - sideCB[1] * winding->p[scan][axisX] < sideCB[2] ) ) {
                                    if ( winding->p[scan][axisX] == winding->p[previous][axisX] && winding->p[scan][axisY] == winding->p[previous][axisY] ) {
                                        if ( !TrisCheckBothEdges( sideBA, sideCB, sideAC, winding->p[scanPrevious][axisX], winding->p[scanPrevious][axisY], winding->p[scanNext][axisX], winding->p[scanNext][axisY] ) ) valid = qfalse;
                                        else if ( !pass ) {
                                            if ( TrisVerticesCoincident2D( scanPrevious, axisX, winding, axisY, ear, scanNext, ( count + previous - 1 ) % count ) ) valid = qfalse;
                                        }
                                    } else if ( winding->p[scan][axisX] == winding->p[ear][axisX] && winding->p[scan][axisY] == winding->p[ear][axisY] ) {
                                        if ( !TrisCheckBothEdges( sideCB, sideAC, sideBA, winding->p[scanPrevious][axisX], winding->p[scanPrevious][axisY], winding->p[scanNext][axisX], winding->p[scanNext][axisY] ) ) valid = qfalse;
                                        else if ( !pass ) {
                                            if ( TrisVerticesCoincident2D( scanPrevious, axisX, winding, axisY, next, scanNext, previous ) ) valid = qfalse;
                                        }
                                    } else if ( winding->p[scan][axisX] == winding->p[next][axisX] && winding->p[scan][axisY] == winding->p[next][axisY] ) {
                                        if ( !TrisCheckBothEdges( sideAC, sideBA, sideCB, winding->p[scanPrevious][axisX], winding->p[scanPrevious][axisY], winding->p[scanNext][axisX], winding->p[scanNext][axisY] ) ) valid = qfalse;
                                        else if ( !pass ) {
                                            if ( TrisVerticesCoincident2D( scanPrevious, axisX, winding, axisY, firstScan, scanNext, ear ) ) valid = qfalse;
                                        }
                                    } else { valid = qfalse; }
                                    if ( !valid )
                                        break;
                                }
                                scanPrevious = scan, scan = scanNext, scanNext = ( scanNext + 1 ) % count;
                            }
                            if ( valid ) {
                                bestCosine = cosine;
                                bestEar = ear;
                            }
                        }
                    }
                    ear++;
                } while ( ear < count );
            }
        }
        if ( bestEar < 0 )
            Error( "code bug: triangulation failed!\n" );
        next = ( bestEar + 1 ) % winding->numpoints;
        removePrevious = ( winding->numpoints + bestEar - 1 ) % winding->numpoints;
        EmitTriRecord( winding, removePrevious, bestEar, next, originalWinding,
            cellIndex, cullGroupIndex, surface->props );
        TesselateRemoveVertex( winding, originalWinding, removePrevious, bestEar, next,
            normal );
        if ( bestEar != winding->numpoints ) {
            next--;
            if ( bestEar == 0 )
                removePrevious--;
        }
        beforePrevious = ( winding->numpoints + removePrevious - 1 ) % winding->numpoints;
        afterNext = ( next + 1 ) % winding->numpoints;
        if ( ( winding->p[removePrevious][0] == winding->p[afterNext][0]
        && winding->p[removePrevious][1] == winding->p[afterNext][1]
        && winding->p[removePrevious][2] == winding->p[afterNext][2] )
        || ( winding->p[beforePrevious][0] == winding->p[next][0]
        && winding->p[beforePrevious][1] == winding->p[next][1]
        && winding->p[beforePrevious][2] == winding->p[next][2] ) )
            TrisRemoveDegenerateEdges( winding, originalWinding );
    }
    if ( winding->numpoints == 3 )
        EmitTriRecord( winding, 0, 1, 2, originalWinding,
    cellIndex, cullGroupIndex, surface->props );
}

/*
=============
TriangulateUnsplittableWinding
=============
*/
static void TriangulateUnsplittableWinding( triSurfProps_t *props,
    winding_t *winding, tree_t *tree, int cullGroupIndex ) {
    int cellIndex, i;
    vec3_t center;
    float scale;
    triSurf_t surface;
    if ( !entity_num ) {
        if ( cullGroupIndex >= 0 ) {
            cellIndex = -2;
        } else {
        cellIndex = FindTriSurfCell( winding, props, tree->headnode );
        if ( cellIndex == -2 ) {
            VectorClear( center );
            for ( i = 0; i < winding->numpoints; i++ ) {
                VectorAdd( center, winding->p[i], center );
            }
            scale = 1.0f / winding->numpoints;
            VectorScale( center, scale, center );
            Error( "unsplittable surface crosses a cell boundary (ref pos = %g %g %g, shader = %s)\n",
                center[0], center[1], center[2], props->shaderInfo->shader );
        }
    }
    } else {
        cellIndex = 0;
    }
    surface.winding = CopyWinding( winding );
    surface.originalWinding = winding;
    surface.props = props;
    ClearBounds( surface.mins, surface.maxs );
    for ( i = 0; i < winding->numpoints; i++ ) {
        AddPointToBounds( winding->p[i], surface.mins, surface.maxs );
    }
    surface.marked = qfalse;
    surface.next = NULL;
    TesselateWinding( &surface, cellIndex, cullGroupIndex );
}


/*
=============
Tris_EmitWindingAsDrawSurf
=============
*/
static void Tris_EmitWindingAsDrawSurf( tree_t *tree, winding_t *winding, mapDrawSurface_t *surface, vec4_t plane, float smoothAngle,
    vec4_t texS, vec4_t texT, vec4_t lmapS, vec4_t lmapT,
    vec4_t colorR, vec4_t colorG, vec4_t colorB, vec4_t colorA ) {
    triSurfProps_t *props;
    props = FindOrCreateTriSurfProps( winding, plane, surface->shaderInfo,
        surface->lightmapNum, smoothAngle, lmapS, lmapT, texS, texT,
        colorR, colorG, colorB, colorA );
    if ( surface->shaderInfo->autosprite ) {
        TriangulateUnsplittableWinding( props, winding, tree, surface->cullgroup );
    } else if ( surface->cullgroup >= 0 ) {
        PrependTriSurfToCell( winding, props, numCells + surface->cullgroup );
    } else if ( entity_num > 0 ) {
        PrependTriSurfToCell( winding, props, 0 );
    } else {
        FilterTriSurfIntoTree( winding, props, tree->headnode );
    }
}


/*
=============
Tris_FindPatchWindings
=============
*/
static void Tris_FindPatchWindings( mapDrawSurface_t *surface, tree_t *tree ) {
    mesh_t base, *subdivided, *mesh;
    int x, y, i, first, second, third;
    winding_t *windings[2];
    qboolean valid[2];
    byte colors[3][4];
    vec2_t st[3], lightmap[3];
    vec4_t plane[2], texS[2], texT[2], lmapS[2], lmapT[2];
    vec4_t colorR[2], colorG[2], colorB[2], colorA[2];
    base.width = surface->patchWidth;
    base.height = surface->patchHeight;
    base.verts = surface->verts;
    subdivided = SubdivideMesh( base, surface->patchSubdivisions, 999.0f, NULL, NULL );
    PutMeshOnCurve( *subdivided );
    mesh = RemoveLinearMeshColumnsRows( subdivided, NULL, NULL );
    FreeMesh( subdivided );
    for ( x = 0; x < mesh->width - 1; x++ ) {
        for ( y = 0; y < mesh->height - 1; y++ ) {
            windings[0] = AllocWinding( 4 );
            windings[0]->numpoints = 3;
            first = (y + 1) * mesh->width + x;
            second = first + 1;
            third = y * mesh->width + x + 1;
            TRIS_DISCOVERY_CORNER( mesh->verts, first, windings[0], 0 );
            TRIS_DISCOVERY_CORNER( mesh->verts, second, windings[0], 1 );
            TRIS_DISCOVERY_CORNER( mesh->verts, third, windings[0], 2 );
            valid[0] = PlaneFromPoints( plane[0], windings[0]->p[0], windings[0]->p[1], windings[0]->p[2] );
            if ( valid[0] ) {
                DeriveTextureVectors( plane[0], windings[0]->p[0], st[0], texS[0], texT[0],
                    lightmap[0], lmapS[0], lmapT[0], colors[0], colorR[0], colorG[0], colorB[0], colorA[0] );
            } else { FreeWinding( windings[0] ); }
            windings[1] = AllocWinding( 3 );
            windings[1]->numpoints = 3;
            first = y * mesh->width + x + 1;
            second = y * mesh->width + x;
            third = (y + 1) * mesh->width + x;
            TRIS_DISCOVERY_CORNER( mesh->verts, first, windings[1], 0 );
            TRIS_DISCOVERY_CORNER( mesh->verts, second, windings[1], 1 );
            TRIS_DISCOVERY_CORNER( mesh->verts, third, windings[1], 2 );
            valid[1] = PlaneFromPoints( plane[1], windings[1]->p[0], windings[1]->p[1], windings[1]->p[2] );
            if ( valid[1] ) {
                DeriveTextureVectors( plane[1], windings[1]->p[0], st[0], texS[1], texT[1],
                    lightmap[0], lmapS[1], lmapT[1], colors[0], colorR[1], colorG[1], colorB[1], colorA[1] );
            } else { FreeWinding( windings[1] ); }
            if ( valid[0] && valid[1] &&
                SurfacesCoplanar( windings[0], plane[0], plane[1], plane[0][3], windings[1], plane[1][3] ) &&
                TexVecsMatch( texS[0], texS[1], surface->shaderInfo->width ) &&
                TexVecsMatch( texT[0], texT[1], surface->shaderInfo->height ) &&
                TexVecsMatch( lmapS[0], lmapS[1], 512 ) &&
                TexVecsMatch( lmapT[0], lmapT[1], 512 ) ) {
                VectorCopy( windings[1]->p[1], windings[0]->p[3] );
                windings[0]->numpoints = 4;
                FreeWinding( windings[1] );
                valid[1] = qfalse;
            }
            for ( i = 0; i < 2; i++ ) {
                if ( valid[i] ) {
                    Tris_EmitWindingAsDrawSurf( tree, windings[i], surface, plane[i], curveSmoothAngle,
                        texS[i], texT[i], lmapS[i], lmapT[i], colorR[i], colorG[i], colorB[i], colorA[i] );
                }
            }
        }
    }
    free( mesh );
}


/*
=============
Tris_FindIndexedMeshWindings
=============
*/
static void Tris_FindIndexedMeshWindings( mapDrawSurface_t *surface, tree_t *tree ) {
    int i, j, index;
    winding_t *winding;
    byte colors[3][4];
    vec2_t st[3], lightmap[3];
    vec4_t plane, texS, texT, lmapS, lmapT, colorR, colorG, colorB, colorA;
    for ( i = 0; i < surface->numIndexes; i += 3 ) {
        winding = AllocWinding( 3 );
        winding->numpoints = 3;
        for ( j = 0; j < 3; j++ ) {
            index = surface->indexes[i + j];
            TRIS_DISCOVERY_CORNER( surface->verts, index, winding, j );
        }
        PlaneFromPoints( plane, winding->p[0], winding->p[1], winding->p[2] );
        DeriveTextureVectors( plane, winding->p[0], st[0], texS, texT,
            lightmap[0], lmapS, lmapT, colors[0], colorR, colorG, colorB, colorA );
        Tris_EmitWindingAsDrawSurf( tree, winding, surface, plane, terrainSmoothAngle,
            texS, texT, lmapS, lmapT, colorR, colorG, colorB, colorA );
    }
}


/*
=============
Tris_FindBrushWindings
=============
*/
static void Tris_FindBrushWindings( mapDrawSurface_t *surface, tree_t *tree ) {
    winding_t *winding;
    int i;
    float scale;
    int planeNum;
    vec4_t plane, texS, texT, lmapS, lmapT, colorR, colorG, colorB, colorA;
    vec3_t positions[3];
    vec2_t st[3], lightmap[3];
    winding = AllocWinding( surface->numVerts );
    winding->numpoints = surface->numVerts;
    for ( i = 0; i < surface->numVerts; i++ ) {
        VectorCopy( surface->verts[i].xyz, winding->p[i] );
    }
    planeNum = surface->side->planenum;
    VectorCopy( mapplanes[planeNum].normal, plane );
    plane[3] = mapplanes[planeNum].dist;
    scale = 1.0f / surface->shaderInfo->width;
    VectorScale( surface->side->vecs[0], scale, texS );
    texS[3] = scale * surface->side->vecs[0][3];
    texS[3] -= RoundToInt( DotProduct( texS, surface->verts[0].xyz ) + texS[3] - surface->verts[0].st[0] );
    scale = 1.0f / surface->shaderInfo->height;
    VectorScale( surface->side->vecs[1], scale, texT );
    texT[3] = scale * surface->side->vecs[1][3];
    texT[3] -= RoundToInt( DotProduct( texT, surface->verts[0].xyz ) + texT[3] - surface->verts[0].st[1] );
    if ( surface->lightmapNum >= 0 ) {
        SelectSurfaceTextureTriangle( surface->verts, plane, surface->numVerts, plane[3], positions, st, lightmap );
        DeriveTextureVectors( plane, positions[0], NULL, NULL, NULL,
            lightmap[0], lmapS, lmapT, NULL, NULL, NULL, NULL, NULL );
    } else {
        memset( lmapS, 0, sizeof(lmapS) );
        memset( lmapT, 0, sizeof(lmapT) );
    }
    colorR[0] = 0; colorR[1] = 0; colorR[2] = 0; colorR[3] = 255.0f;
    colorG[0] = 0; colorG[1] = 0; colorG[2] = 0; colorG[3] = 255.0f;
    colorB[0] = 0; colorB[1] = 0; colorB[2] = 0; colorB[3] = 255.0f;
    colorA[0] = 0; colorA[1] = 0; colorA[2] = 0; colorA[3] = 255.0f;
    Tris_EmitWindingAsDrawSurf( tree, winding, surface, plane, brushSmoothAngle,
        texS, texT, lmapS, lmapT, colorR, colorG, colorB, colorA );
}


/*
=============
Tris_FindWindings
=============
*/
static void Tris_FindWindings( entity_t *entity, tree_t *tree ) {
    int i;
    mapDrawSurface_t *surface;
    memset( triSurfCellHeads, 0, TrisGetCellListCount() * sizeof(triSurfCellHeads[0]) );
    numTriangles = 0;
    numTriIntermediateVerts = 0;
    for ( i = entity->firstDrawSurf; i < numMapDrawSurfs; i++ ) {
        surface = &mapDrawSurfs[i];
        if ( !surface->numVerts && !surface->flareSurface ) continue;
        if ( surface->miscModel ) continue;
        if ( surface->patch ) {
            Tris_FindPatchWindings( surface, tree );
        } else if ( surface->triangleSoup ) {
            Tris_FindIndexedMeshWindings( surface, tree );
        } else if ( !surface->flareSurface ) {
            Tris_FindBrushWindings( surface, tree );
        }
    }
    numOriginalDrawVerts = numTriIntermediateVerts;
}


/*
=============
Tris_TriangulateWindings

Tessellate each owned surface, then release the finalized winding lists.
=============
*/
int Tris_TriangulateWindings( void ) {
	triSurf_t *surface;
	triSurf_t *next;
	int cell;
	int cellCount;
	int cellIndex;
	int cullGroupIndex;

	cellCount = TrisGetCellListCount();
	for ( cell = 0; cell < cellCount; cell++ ) {
		if ( cell < numCells ) {
			cellIndex = cell;
			cullGroupIndex = -1;
		} else {
			cellIndex = -2;
			cullGroupIndex = cell - numCells;
		}
		for ( surface = triSurfCellHeads[cell]; surface; surface = next ) {
			TesselateWinding( surface, cellIndex, cullGroupIndex );
			next = surface->next;
			FreeWinding( surface->winding );
			FreeWinding( surface->originalWinding );
			free( surface );
		}
		triSurfCellHeads[cell] = NULL;
	}
	return cellCount;
}


/*
=============
TriangulateEntity

Run each triangle stage; world geometry additionally reports progress.
=============
*/
static void TriangulateEntity( entity_t *entity, tree_t *tree ) {
	if ( !entity_num ) {
		printf( "finding triangle windings...\n" );
	}
	Tris_FindWindings( entity, tree );
	if ( !entity_num ) {
		printf( "merging into concave windings...\n" );
	}
	MergeVisGroupCells();
	if ( !entity_num ) {
		printf( "added %i points for thin polygons (%.1f%% more points)\n",
			c_addedThinWindingPoints,
			100.0f * c_addedThinWindingPoints / c_thinWindingInputPoints );
		printf( "fixing t-junctions...\n" );
	}
	TJunc_FixAll();
	if ( !entity_num ) {
		printf( "finding index mapping and snapping vertices...\n" );
	}
	TrisGenerateAllVertices();
	BuildTriangleVertexRemap();
	TrisSnapNearbyVertices();
	ApplyMergeMap();
	RebuildTriangleWindings();
	if ( !entity_num ) {
		printf( "triangulating all windings...\n" );
	}
	Tris_TriangulateWindings();
	if ( !entity_num ) {
		printf( "%i self-tjunctions fixed\n%i degenerate tris removed\n",
			numTriSplitPoints, numDegenerateTriEdges );
	}
}


/*
=============
EmitDrawSurfaces
=============
*/
void EmitDrawSurfaces( entity_t *entity, tree_t *tree ) {
    if ( entity->firstDrawSurf == numMapDrawSurfs ) return;
    trisCurrentTree = tree;
    TriangulateEntity( entity, tree );
    if ( !entity_num ) printf( "simplifying triangles...\n" );
    SimplifyTriangleMesh();
    if ( !entity_num ) printf( "emitting triangles...\n" );
    SmoothTriangleNormals();
    BuildTriangleSoups( tree );
    if ( !entity_num ) {
        printf( "%i vertices couldn't be merged because the textures point different ways\n", numTextureDirectionMismatches );
        EmitCellPortalsAndOccluders( entity, tree->headnode );
    }
    FreeTriSurfProps();
    trisCurrentTree = NULL;
}


