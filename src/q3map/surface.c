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

#include "qbsp.h"
#include "terrain_surface.h"

/* Floor to int through the x87 (biased fistp). */

mapDrawSurface_t mapDrawSurfs[MAX_MAP_DRAW_SURFS];
int numMapDrawSurfs;

FILE *debugPolyFile = 0;

typedef struct {
	int planenum;
	shaderInfo_t *shaderInfo;
	int count;
} sideRef_t;

#define MAX_SIDE_REFS MAX_MAP_PLANES

sideRef_t sideRefs[MAX_SIDE_REFS];
int numSideRefs;

/* Serialized collision records shared by patch and terrain branches. */
typedef cod_bsp_collision_surface_t collisionSurface_t;

#define MAX_MAP_PATCHCOLLIDES 0x2000
#define MAX_MAP_COLLISIONVERTS 0x20000

extern int numPatchCollides;
extern collisionSurface_t patchCollides[MAX_MAP_PATCHCOLLIDES];
extern int numCollisionVerts;
extern vec3_t collisionVerts[MAX_MAP_COLLISIONVERTS];

extern int numCollisionIndexes;
extern unsigned short collisionIndexes[];

int FilterMapDrawSurfIntoTree_r( winding_t *w, mapDrawSurface_t *ds, node_t *node );
int FilterMapDrawSurfIntoTree( vec3_t point, mapDrawSurface_t *ds, node_t *node );

extern void CheckWindingPlane( winding_t *w, vec3_t normal, vec_t dist );

/* Forward declarations. */
mapDrawSurface_t *AllocDrawSurf( void );
mapDrawSurface_t *DrawSurfaceForSide(bspbrush_t *b, side_t *s, winding_t *w, qboolean globalTexture);
void WriteDebugSideToFile(side_t *side, winding_t *w);
FILE *OpenDebugFile( void );
void AddSideRef( side_t *side );
void CloseDebugFile( void );
void EmitPatchCollide( mapDrawSurface_t *ds );
void EmitTerrainCollide( mapDrawSurface_t *ds );
void ClipSideIntoTree_r( winding_t *w, side_t *side, node_t *node );
int FilterSideIntoTree_r(winding_t *w, side_t *side, mapDrawSurface_t *ds, node_t *node);
int FilterTriangleSoupIntoTree( mapDrawSurface_t *ds, tree_t *tree );
int FilterPatchSurfIntoTree( mapDrawSurface_t *ds, tree_t *tree );
void FilterCollisionSurfsIntoTree( entity_t *entity, tree_t *tree );
void SubdivideDrawSurf( mapDrawSurface_t *ds, winding_t *w, float subdivisions );
void SubdivideDrawSurfs(entity_t *e, tree_t *tree);
void ClipSidesIntoTree(entity_t *e, tree_t *tree);
void MergeSides( entity_t *e, tree_t *tree );
void WriteDebugSurfaceToFile( terrainSurface_t *terrain );

#include "../common/q_round.h"

/*
=================
AllocDrawSurf
=================
*/
mapDrawSurface_t *AllocDrawSurf( void ) {
	mapDrawSurface_t *ds;

	if ( numMapDrawSurfs >= MAX_MAP_DRAW_SURFS ) {
		Error( "MAX_MAP_DRAW_SURFS" );
	}
	ds = &mapDrawSurfs[numMapDrawSurfs];
	numMapDrawSurfs++;
	return ds;
}

/*
=============
OpenDebugFile
=============
*/
FILE *OpenDebugFile( void ) {
	char filename[128];

	sprintf( filename, "%s.poly", source );
	debugPolyFile = fopen( filename, "wt" );
	return debugPolyFile;
}

/*
=============
CloseDebugFile
=============
*/
void CloseDebugFile( void ) {
	if ( debugPolyFile ) {
		fclose( debugPolyFile );
		debugPolyFile = NULL;
	}
}

/*
=============
WriteDebugSideToFile
=============
*/
void WriteDebugSideToFile(side_t *side, winding_t *w) {
	shaderInfo_t *si;
	int i;
	if (!debugPolyFile)
		return;
	si = side->shaderInfo;
	fprintf(debugPolyFile, "%s\n", si->shader);
	fprintf(debugPolyFile, "%i\n", w->numpoints);
	for (i = 0; i < w->numpoints; i++) {
		double s, t;
		s = (DotProduct(w->p[i], side->vecs[0]) + side->vecs[0][3]) / si->width;
		t = (DotProduct(w->p[i], side->vecs[1]) + side->vecs[1][3]) / si->height;
		fprintf(debugPolyFile, "( %g %g %g %g %g )\n", w->p[i][0], w->p[i][1], w->p[i][2], s, t);
	}
}

/*
=============
WriteDebugSurfaceToFile
=============
*/
void WriteDebugSurfaceToFile( terrainSurface_t *terrain ) {
	const char *shader;
	int vertexIndex;
	int i;

	if ( !debugPolyFile ) {
		return;
	}

	shader = terrain->shaderInfo->shader;
	for ( i = 0 ; i < terrain->numIndexes ; i++ ) {
		if ( i % 3 == 0 ) {
			fprintf( debugPolyFile, "%s\n", shader );
			fprintf( debugPolyFile, "3\n" );
		}

		vertexIndex = terrain->indexes[i];
		fprintf( debugPolyFile, "( %g %g %g %g %g )\n",
			terrain->verts[vertexIndex].xyz[0], terrain->verts[vertexIndex].xyz[1],
			terrain->verts[vertexIndex].xyz[2], terrain->verts[vertexIndex].st[0],
			terrain->verts[vertexIndex].st[1] );
	}
}

/*
=============
AddSideRef
=============
*/
void AddSideRef( side_t *side ) {
	int i;

	for ( i = 0; i < numSideRefs; i++ ) {
		if ( side->planenum == sideRefs[i].planenum
			&& side->shaderInfo == sideRefs[i].shaderInfo ) {
			sideRefs[i].count++;
			return;
		}
	}
	if ( numSideRefs == MAX_SIDE_REFS ) {
		Error( "MAX_SIDE_REFS" );
	}
	sideRefs[i].planenum = side->planenum;
	sideRefs[i].shaderInfo = side->shaderInfo;
	sideRefs[i].count++;
	numSideRefs++;
}

/*
=====================
MergeSides

=====================
*/
void MergeSides( entity_t *e, tree_t *tree ) {
	int i;

	qprintf( "----- MergeSides -----\n" );
	for ( i = e->firstDrawSurf; i < numMapDrawSurfs; i++ ) {
		/* Original Q3 AddSideRef call is commented out in this loop. */
	}
	qprintf( "%5i siderefs\n", numSideRefs );
}

/*
====================
ClipSideIntoTree_r

Adds non-opaque leaf fragments to the convex hull
====================
*/
void ClipSideIntoTree_r( winding_t *w, side_t *side, node_t *node ) {
	plane_t *plane;
	winding_t *front, *back;
	if ( !w )
		return;
	if ( node->planenum != PLANENUM_LEAF ) {
		if ( side->planenum == node->planenum ) {
			ClipSideIntoTree_r( w, side, node->children[0] );
			return;
		}
		if ( side->planenum == (node->planenum ^ 1) ) {
			ClipSideIntoTree_r( w, side, node->children[1] );
			return;
		}
		plane = &mapplanes[node->planenum];
		ClipWindingEpsilon( w, plane->normal, plane->dist, 0.25f, &front, &back, qtrue );
		FreeWinding( w );
		ClipSideIntoTree_r( front, side, node->children[0] );
		ClipSideIntoTree_r( back, side, node->children[1] );
		return;
	}
	if ( !node->opaque )
		AddWindingToConvexHull( w, &side->visibleHull, mapplanes[side->planenum].normal );
	FreeWinding( w );
}

/*
=============
FilterMapDrawSurfIntoTree
=============
*/
int FilterMapDrawSurfIntoTree( vec3_t point, mapDrawSurface_t *ds, node_t *node ) {
	drawSurfRef_t *dsr;
	float d;
	plane_t *plane;
	int c;
	if ( node->planenum != PLANENUM_LEAF ) {
		plane = &mapplanes[node->planenum];
		d = DotProduct(point, plane->normal) - plane->dist;
		c = 0;
		if ( d >= -0.1f )
			c += FilterMapDrawSurfIntoTree( point, ds, node->children[0] );
		if ( d <= 0.1f )
			c += FilterMapDrawSurfIntoTree( point, ds, node->children[1] );
		return c;
	}
	if ( node->opaque )
		return 0;
	for ( dsr = node->collisionSurfReferences; dsr; dsr = dsr->nextRef ) {
		if ( dsr->outputNumber == numPatchCollides )
			return 0;
	}
	dsr = malloc(sizeof(*dsr));
	dsr->outputNumber = numPatchCollides;
	dsr->nextRef = node->collisionSurfReferences;
	node->collisionSurfReferences = dsr;
	return 1;
}

/*
=============
FilterMapDrawSurfIntoTree_r
=============
*/
int FilterMapDrawSurfIntoTree_r( winding_t *w, mapDrawSurface_t *ds, node_t *node ) {
	drawSurfRef_t *dsr;
	winding_t *front, *back;
	plane_t *plane;
	int total;
	if ( node->planenum != PLANENUM_LEAF ) {
		plane = &mapplanes[node->planenum];
		ClipWindingEpsilon( w, plane->normal, plane->dist, 0.1f, &front, &back, qtrue );
		total = 0;
		if ( front )
			total += FilterMapDrawSurfIntoTree_r( front, ds, node->children[0] );
		if ( back )
			total += FilterMapDrawSurfIntoTree_r( back, ds, node->children[1] );
		FreeWinding( w );
		return total;
	}
	if ( node->opaque )
		return 0;
	for ( dsr = node->collisionSurfReferences; dsr; dsr = dsr->nextRef ) {
		if ( dsr->outputNumber == numPatchCollides )
			return 0;
	}
	dsr = malloc(sizeof(*dsr));
	dsr->outputNumber = numPatchCollides;
	dsr->nextRef = node->collisionSurfReferences;
	node->collisionSurfReferences = dsr;
	return 1;
}

/*
====================
FilterSideIntoTree_r

Place a reference to the given drawsurf in every leaf it contacts
====================
*/
int FilterSideIntoTree_r(winding_t *w, side_t *side, mapDrawSurface_t *ds, node_t *node) {
	plane_t *plane;
	winding_t *front, *back;
	drawSurfRef_t *dsr;
	int total;
	if (!w) return 0;
	if (node->planenum != PLANENUM_LEAF) {
		if (side->planenum == node->planenum)
			return FilterSideIntoTree_r(w, side, ds, node->children[0]);
		if (side->planenum == (node->planenum ^ 1))
			return FilterSideIntoTree_r(w, side, ds, node->children[1]);
		plane = &mapplanes[node->planenum];
		ClipWindingEpsilon(w, plane->normal, plane->dist, 0.1f, &front, &back, qfalse);
		total = FilterSideIntoTree_r(front, side, ds, node->children[0]);
		total += FilterSideIntoTree_r(back, side, ds, node->children[1]);
		FreeWinding(w);
		return total;
	}
	if (node->opaque) return 0;
	dsr = malloc(sizeof(*dsr));
	dsr->outputNumber = numPatchCollides;
	dsr->nextRef = node->collisionSurfReferences;
	node->collisionSurfReferences = dsr;
	FreeWinding(w);
	return 1;
}

/*
=====================
FilterPatchSurfIntoTree
=====================
*/
int FilterPatchSurfIntoTree( mapDrawSurface_t *ds, tree_t *tree ) {
	int i, j, refs;
	mesh_t baseMesh, *subdividedMesh;
	winding_t *w;

	baseMesh.width = ds->patchWidth;
	baseMesh.height = ds->patchHeight;
	baseMesh.verts = ds->verts;
	subdividedMesh = SubdivideMesh( baseMesh, ds->patchSubdivisions, 999.0f, NULL, NULL );
	refs = 0;
	for ( i = 0; i < subdividedMesh->width - 1; i++ ) {
		for ( j = 0; j < subdividedMesh->height - 1; j++ ) {
			w = AllocWinding( 3 );
			VectorCopy( subdividedMesh->verts[j * subdividedMesh->width + i].xyz, w->p[0] );
			VectorCopy( subdividedMesh->verts[j * subdividedMesh->width + i + 1].xyz, w->p[1] );
			VectorCopy( subdividedMesh->verts[(j + 1) * subdividedMesh->width + i].xyz, w->p[2] );
			w->numpoints = 3;
			refs += FilterMapDrawSurfIntoTree_r( w, ds, tree->headnode );
			w = AllocWinding( 3 );
			VectorCopy( subdividedMesh->verts[j * subdividedMesh->width + i + 1].xyz, w->p[0] );
			VectorCopy( subdividedMesh->verts[(j + 1) * subdividedMesh->width + i + 1].xyz, w->p[1] );
			VectorCopy( subdividedMesh->verts[(j + 1) * subdividedMesh->width + i].xyz, w->p[2] );
			w->numpoints = 3;
			refs += FilterMapDrawSurfIntoTree_r( w, ds, tree->headnode );
		}
	}
	for ( i = 0; i < subdividedMesh->width * subdividedMesh->height; i++ ) {
		refs += FilterMapDrawSurfIntoTree( subdividedMesh->verts[i].xyz, ds, tree->headnode );
	}
	free( subdividedMesh );
	return refs;
}

/*
=============
FilterTriangleSoupIntoTree
=============
*/
int FilterTriangleSoupIntoTree( mapDrawSurface_t *ds, tree_t *tree ) {
	int i, refs;
	winding_t *w;

	refs = 0;
	for ( i = 0; i < ds->numIndexes; i += 3 ) {
		w = AllocWinding( 3 );
		w->numpoints = 3;
		VectorCopy( ds->verts[ds->indexes[i]].xyz, w->p[0] );
		VectorCopy( ds->verts[ds->indexes[i + 1]].xyz, w->p[1] );
		VectorCopy( ds->verts[ds->indexes[i + 2]].xyz, w->p[2] );
		refs += FilterMapDrawSurfIntoTree_r( w, ds, tree->headnode );
	}
	return refs;
}

/*
=============
EmitPatchCollide
=============
*/
void EmitPatchCollide( mapDrawSurface_t *ds ) {
	collisionSurface_t *out;
	int i;

	if ( ds->isTerrain ) {
		return;
	}
	if ( numPatchCollides == MAX_MAP_PATCHCOLLIDES ) {
		Error( "MAX_MAP_PATCHCOLLIDES" );
	}
	out = &patchCollides[numPatchCollides++];
	out->material = EmitShader( ds->shaderInfo->shader, ds->contents );
	out->isTerrain = qfalse;
	out->geometry.patch.width = ds->patchWidth;
	out->geometry.patch.height = ds->patchHeight;
	out->geometry.patch.subdivisions = ds->patchSubdivisions;
	out->geometry.patch.firstVertex = numCollisionVerts;
	for ( i = 0; i < ds->numVerts; i++ ) {
		vec_t *vertex;
		if ( numCollisionVerts == MAX_MAP_COLLISIONVERTS ) {
			Error( "MAX_MAP_COLLISIONVERTS" );
		}
		vertex = collisionVerts[numCollisionVerts++];
		VectorCopy( ds->verts[i].xyz, vertex );
	}
}

/*
=============
EmitTerrainCollide
=============
*/
void EmitTerrainCollide( mapDrawSurface_t *ds ) {
	collisionSurface_t *out;
	int i;

	if ( numPatchCollides == MAX_MAP_PATCHCOLLIDES ) {
		Error( "MAX_MAP_PATCHCOLLIDES" );
	}
	out = &patchCollides[numPatchCollides++];
	out->material = EmitShader( ds->shaderInfo->shader, ds->contents );
	out->isTerrain = qtrue;
	out->geometry.terrain.numVerts = ds->numVerts;
	out->geometry.terrain.numIndices = ds->numIndexes;
	out->geometry.terrain.firstVertex = numCollisionVerts;
	out->geometry.terrain.firstIndex = numCollisionIndexes;
	for ( i = 0; i < ds->numVerts; i++ ) {
		vec_t *vertex;

		if ( numCollisionVerts == MAX_MAP_COLLISIONVERTS ) {
			Error( "MAX_MAP_COLLISIONVERTS" );
		}
		vertex = collisionVerts[numCollisionVerts++];
		VectorCopy( ds->verts[i].xyz, vertex );
	}
	for ( i = 0; i < ds->numIndexes; i++ ) {
		if ( numCollisionIndexes == MAX_MAP_DRAW_INDEXES ) {
			Error( "MAX_MAP_DRAW_INDEXES" );
		}
		collisionIndexes[numCollisionIndexes++] = ds->indexes[i];
	}
}

/*
=============
FilterCollisionSurfsIntoTree
=============
*/
void FilterCollisionSurfsIntoTree( entity_t *entity, tree_t *tree ) {
	int i;
	mapDrawSurface_t *ds;

	qprintf( "----- FilterCollisionSurfsIntoTree -----\n" );
	for ( i = entity->firstDrawSurf; i < numMapDrawSurfs; i++ ) {
		ds = &mapDrawSurfs[i];
		if ( !ds->numVerts || ( ds->contents & CONTENTS_NONCOLLIDING ) ) {
			continue;
		}
		if ( ds->patch ) {
			FilterPatchSurfIntoTree( ds, tree );
			EmitPatchCollide( ds );
		} else if ( ds->triangleSoup ) {
			FilterTriangleSoupIntoTree( ds, tree );
			EmitTerrainCollide( ds );
		}
	}
}

/*
=================
DrawSurfaceForSide
=================
*/
mapDrawSurface_t *DrawSurfaceForSide(bspbrush_t *b, side_t *s, winding_t *w, qboolean globalTexture) {
	mapDrawSurface_t *ds;
	int i, j;
	shaderInfo_t *si;
	drawVert_t *dv;
	float mins[2], maxs[2];
	vec3_t texX, texY;
	vec_t x, y;

	if (w->numpoints > 64)
		Error("DrawSurfaceForSide: w->numpoints = %i", w->numpoints);
	si = s->shaderInfo;
	ds = AllocDrawSurf();
	ds->shaderInfo = si;
	ds->lightmapSampleSize = s->lightmapSampleSize;
	ds->mapBrush = b;
	ds->side = s;
	ds->fogNum = -1;
	ds->cullgroup = b->cullgroup;
	ds->numVerts = w->numpoints;
	ds->verts = malloc(ds->numVerts * sizeof(*ds->verts));
	memset(ds->verts, 0, ds->numVerts * sizeof(*ds->verts));
	mins[0] = mins[1] = 99999;
	maxs[0] = maxs[1] = -99999;
	ComputeAxisBase(mapplanes[s->planenum].normal, texX, texY);
	for (j = 0; j < w->numpoints; j++) {
		dv = ds->verts + j;
		VectorCopy(w->p[j], dv->xyz);
		if (g_bBrushPrimit == BPRIMIT_OLDBRUSHES) {
			dv->st[0] = s->vecs[0][3] + DotProduct(s->vecs[0], dv->xyz);
			dv->st[1] = s->vecs[1][3] + DotProduct(s->vecs[1], dv->xyz);
			dv->st[0] /= si->width;
			dv->st[1] /= si->height;
		} else {
			x = DotProduct(dv->xyz, texX);
			y = DotProduct(dv->xyz, texY);
			dv->st[0] = s->texMat[0][0] * x + s->texMat[0][1] * y + s->texMat[0][2];
			dv->st[1] = s->texMat[1][0] * x + s->texMat[1][1] * y + s->texMat[1][2];
		}
		for (i = 0; i < 2; i++) {
			if (dv->st[i] < mins[i]) mins[i] = dv->st[i];
			if (dv->st[i] > maxs[i]) maxs[i] = dv->st[i];
		}
		VectorCopy(mapplanes[s->planenum].normal, dv->normal);
	}
	if (!globalTexture || si->flap || si->clampTexture) {
		mins[0] = FloorToInt((maxs[0] + mins[0]) * 0.5f);
		mins[1] = FloorToInt((maxs[1] + mins[1]) * 0.5f);
		for (i = 0; i < w->numpoints; i++) {
			dv = ds->verts + i;
			dv->st[0] -= mins[0];
			dv->st[1] -= mins[1];
		}
	}
	return ds;
}

/*
===================
SubdivideDrawSurf
===================
*/
void SubdivideDrawSurf( mapDrawSurface_t *ds, winding_t *w, float subdivisions ) {
	int i, axis;
	vec3_t bounds[2];
	const float epsilon = 0.1f;
	float inverseSubdivisions;
	int subFloor, subCeil;
	winding_t *frontWinding, *backWinding;
	mapDrawSurface_t *newds;

	if ( !w ) {
		return;
	}
	if ( w->numpoints < 3 ) {
		Error( "SubdivideDrawSurf: Bad w->numpoints" );
	}
	ClearBounds( bounds[0], bounds[1] );
	for ( i = 0; i < w->numpoints; i++ ) {
		AddPointToBounds( w->p[i], bounds[0], bounds[1] );
	}
	inverseSubdivisions = 1.0f / subdivisions;
	for ( axis = 0; axis < 3; axis++ ) {
		vec3_t planePoint = { 0, 0, 0 };
		vec3_t planeNormal = { 0, 0, 0 };
		float d;

		subFloor = floor( bounds[0][axis] * inverseSubdivisions ) * subdivisions;
		subCeil = ceil( bounds[1][axis] * inverseSubdivisions ) * subdivisions;
		planePoint[axis] = subFloor + subdivisions;
		planeNormal[axis] = -1;
		d = DotProduct( planePoint, planeNormal );
		if ( subCeil - subFloor > subdivisions ) {
			ClipWindingEpsilon( w, planeNormal, d, epsilon,
				&frontWinding, &backWinding, qfalse );
			if ( !frontWinding ) {
				w = backWinding;
			} else if ( !backWinding ) {
				w = frontWinding;
			} else {
				SubdivideDrawSurf( ds, frontWinding, subdivisions );
				SubdivideDrawSurf( ds, backWinding, subdivisions );
				return;
			}
		}
	}
	CheckWindingPlane( w, mapplanes[ds->side->planenum].normal,
		mapplanes[ds->side->planenum].dist );
	newds = DrawSurfaceForSide( ds->mapBrush, ds->side, w, qtrue );
	newds->fogNum = ds->fogNum;
}

/*
=====================
SubdivideDrawSurfs

Chop up surfaces that have subdivision attributes
=====================
*/
void SubdivideDrawSurfs(entity_t *e, tree_t *tree) {
	int i, numBaseDrawSurfs;
	mapDrawSurface_t *ds;
	winding_t *w;
	float subdivision;
	shaderInfo_t *si;
	qprintf("----- SubdivideDrawSurfs -----\n");
	numBaseDrawSurfs = numMapDrawSurfs;
	for (i = e->firstDrawSurf; i < numBaseDrawSurfs; i++) {
		ds = &mapDrawSurfs[i];
		if (!ds->side) continue;
		si = ds->side->shaderInfo;
		if (!si) continue;
		if (ds->shaderInfo->autosprite || si->autosprite) continue;
		subdivision = si->subdivisions;
		if (!subdivision) continue;
		w = WindingFromDrawSurf(ds);
		ds->numVerts = 0;
		SubdivideDrawSurf(ds, w, subdivision);
	}
}

/*
=====================
ClipSidesIntoTree

Creates side->visibleHull for all visible sides

The drawsurf for a side will consist of the convex hull of
all points in non-opaque clusters, which allows overlaps
to be trimmed off automatically.
=====================
*/
void ClipSidesIntoTree(entity_t *e, tree_t *tree) {
	bspbrush_t *b;
	int i;
	winding_t *w;
	side_t *side;
	shaderInfo_t *si;
	qprintf("----- ClipSidesIntoTree -----\n");
	for (b = e->brushes; b; b = b->next) {
		for (i = 0; i < b->numsides; i++) {
			side = &b->sides[i];
			if (!side->winding) continue;
			w = CopyWinding(side->winding);
			side->visibleHull = NULL;
			ClipSideIntoTree_r(w, side, tree->headnode);
			w = side->visibleHull;
			if (!w) continue;
			si = side->shaderInfo;
			if (!si) continue;
			if (si->autosprite) w = side->winding;
							if (si->surfaceFlags & SURF_NODRAW) {
				if (si->surfaceFlags & SURF_CASTSHADOW) WriteDebugSideToFile(side, w);
				continue;
			}
			if (side->bevel) Error("monkey tried to create draw surface for brush bevel");
			CheckWindingPlane(w, mapplanes[side->planenum].normal, mapplanes[side->planenum].dist);
			DrawSurfaceForSide(b, side, w, side->shaderInfo->globalTexture);
		}
	}
}

