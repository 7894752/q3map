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
#include "../universal/q_parse.h"
#include "../universal/com_math.h"
#include "terrain_surface.h"

#define MAX_PATCH_TRIANGULATION_AXIS 512
#define PATCH_ST_EPSILON ( 0.01f * 0.001f )



typedef struct {
	parseMesh_t *patch;
	int projectionAxis;
} terrainPatchEntry_t;

#define MAX_TERRAIN_GROUP_TRIANGLES 16384
#define MAX_TERRAIN_GROUP_VERTS 4096
#define MAX_TERRAIN_PATCHES 1024


/*
=============
PrintCtrl
=============
*/
void PrintCtrl( vec3_t ctrl[9] ) {
	int		i, j;

	for ( i = 0 ; i < 3 ; i++ ) {
		for ( j = 0 ; j < 3 ; j++ ) {
			_printf("(%5.2f %5.2f %5.2f) ", ctrl[i*3+j][0], ctrl[i*3+j][1], ctrl[i*3+j][2] );
		}
		_printf("\n");
	}
}

static void RecenterPatchTexCoords(mapDrawSurface_t *ds);

char *Com_Parse(char **data_p);
void Com_UngetToken(void);
extern int currentEntNum;

/*
=============
AllocStackOrHeap
=============
*/
static void *AllocStackOrHeap( int count, int capacity, void *buffer, int elementSize ) {
	if ( count <= capacity )
		return buffer;
	return malloc( count * elementSize );
}

/* Same-game squared-component proximity, distinct from tools VectorCompare. */
int VectorCompareEpsilon( const vec3_t a, const vec3_t b );

/*
=============
FreeStackOrHeap
=============
*/
static void FreeStackOrHeap( void *buffer, void *stackBuffer ) {
	if ( buffer != stackBuffer )
		free( buffer );
}


/*
================
DrawSurfaceForMesh
================
*/
mapDrawSurface_t	*DrawSurfaceForMesh( mesh_t *m, qboolean isTerrain, int cullgroup ) {
	mapDrawSurface_t	*ds;
	int				i, j;
	mesh_t			*copy;
	vec4_t			plane;
	qboolean		planar;

	// to make valid normals for patches with degenerate edges,
	// we need to make a copy of the mesh and put the aproximating
	// points onto the curve
	copy = CopyMesh( m );
	PutMeshOnCurve( *copy );
	MakeMeshNormals( *copy );
	for ( j = 0 ; j < m->width ; j++ ) {
		for ( i = 0 ; i < m->height ; i++ ) {
			VectorCopy( copy->verts[i*m->width+j].normal, m->verts[i*m->width+j].normal );
		}
	}
	planar = GuessMeshPlane( copy, plane );
	FreeMesh( copy );

	ds = AllocDrawSurf();
	ds->mapBrush = NULL;
	ds->side = NULL;
	if ( planar ) {
		ds->lightmapPlaneNum = FindFloatPlane( plane, plane[3] );
	}

	ds->isTerrain = isTerrain;
	ds->patch = qtrue;
	ds->planar = planar;
	ds->patchSubdivisions = m->patchSubdivisions;
	ds->patchWidth = m->width;
	ds->patchHeight = m->height;
	ds->numVerts = ds->patchWidth * ds->patchHeight;
	ds->verts = malloc( ds->numVerts * sizeof( *ds->verts ) );
	memcpy( ds->verts, m->verts, ds->numVerts * sizeof( *ds->verts ) );

	ds->lightmapNum = -1;
	ds->fogNum = -1;
	ds->cullgroup = cullgroup;
	ds->lightmapSampleSize = m->lightmapSampleSize;

	return ds;
}

#include "../common/q_round.h"

/*
=================
ParsePatch

Creates a mapDrawSurface_t from the patch text
=================
*/
void ParsePatch(char **data_p, qboolean terrain)
{
	int i, j, flags;
	parseMesh_t *pm;
	char texture[MAX_QPATH], shader[MAX_QPATH];
	mesh_t m;
	float info[10];
	drawVert_t *verts, *v;
	char *parseToken;

	Com_MatchToken(data_p, "{", qfalse);
	strcpy(texture, Com_Parse(data_p));
	if (numMapIndexedShaders == MAX_MAP_BRUSHSIDES)
		Error("MAX_MAP_BRUSHSIDES");
	strcpy(mapIndexedShaders[numMapIndexedShaders], texture);
	numMapIndexedShaders++;
	Com_MatchToken(data_p, "(", qfalse);
	m.width = atoi(Com_Parse(data_p));
	m.height = atoi(Com_Parse(data_p));
	flags = atoi(Com_Parse(data_p));
	Com_Parse(data_p);
	Com_Parse(data_p);
	m.lightmapSampleSize = atoi(Com_Parse(data_p));
	m.patchSubdivisions = atoi(Com_Parse(data_p));
	Com_MatchToken(data_p, ")", qfalse);
	m.verts = verts = malloc(m.width * m.height * sizeof(*m.verts));
	m.terrainFlags = malloc(m.width * m.height * sizeof(*m.terrainFlags));
	if (m.width < 0 || m.width > MAX_PATCH_SIZE || m.height < 0 || m.height > MAX_PATCH_SIZE)
		Error("ParsePatch: bad size");
	Com_MatchToken(data_p, "(", qfalse);
	for (j = 0; j < m.width; j++)
	{
		Com_MatchToken(data_p, "(", qfalse);
		for (i = 0; i < m.height; i++)
		{
			Com_Parse1DMatrix(data_p, 10, info);
			v = &verts[i * m.width + j];
			VectorCopy(info, v->xyz);
			v->st[0] = info[3];
			v->st[1] = info[4];
			v->color[0] = info[5];
			v->color[1] = info[6];
			v->color[2] = info[7];
			v->color[3] = info[8];
			m.terrainFlags[i * m.width + j] = info[9];
		}
		Com_MatchToken(data_p, ")", qfalse);
	}
	Com_MatchToken(data_p, ")", qfalse);
	parseToken = Com_Parse(data_p);
	if (g_bBrushPrimit != BPRIMIT_OLDBRUSHES && strcmp(parseToken, "}"))
		ParseEpair(parseToken, data_p);
	else
		Com_UngetToken();
	Com_MatchToken(data_p, "}", qfalse);
	Com_MatchToken(data_p, "}", qfalse);
	if (noCurveBrushes)
		return;
	pm = malloc(sizeof(*pm));
	memset(pm, 0, sizeof(*pm));
	pm->cullgroup = -1;
	sprintf(shader, "textures/%s", texture);
	pm->shaderInfo = ShaderInfoForShader(shader);
	pm->mesh = m;
	pm->isTerrain = terrain;
	pm->entitynum = currentEntNum - 1;
	pm->brushnum = entitySourceBrushes;
	pm->contents = pm->shaderInfo->contents;
	if (flags & (CONTENTS_CLIPSHOT | CONTENTS_MISSILECLIP))
		pm->contents &= ~(CONTENTS_DETAIL | CONTENTS_SOLID);
	if (flags & CONTENTS_CLIPSHOT)
		pm->contents |= CONTENTS_CLIPSHOT;
	if (flags & CONTENTS_MISSILECLIP)
		pm->contents |= CONTENTS_MISSILECLIP;
	if (flags & CONTENTS_NONCOLLIDING)
		pm->contents |= CONTENTS_NONCOLLIDING;
	pm->next = mapent->patches;
	mapent->patches = pm;
}

qboolean GuessMeshPlane( mesh_t *in, vec4_t plane );

/*
=============
GrowGroup_r
=============
*/
void GrowGroup_r( int patchNum, int patchCount, const byte *bordering, byte *group ) {
	int		i;
	const byte *row;

	if ( group[patchNum] ) {
		return;
	}
	group[patchNum] = 1;
	row = bordering + patchNum * patchCount;
	for ( i = 0 ; i < patchCount ; i++ ) {
		if ( row[i] ) {
			GrowGroup_r( i, patchCount, bordering, group );
		}
	}
}

/*
=============
FindPatchDrawVert
=============
*/
static int FindPatchDrawVert( const drawVert_t *vertex, drawVert_t *verts,
							 int *vertexCount ) {
	int i;

	for ( i = 0; i < *vertexCount; i++ ) {
		if ( !VectorCompareEpsilon( verts[i].xyz, vertex->xyz ) )
			continue;
		if ( fabs( verts[i].st[0] - vertex->st[0] ) > PATCH_ST_EPSILON )
			continue;
		if ( fabs( verts[i].st[1] - vertex->st[1] ) > PATCH_ST_EPSILON )
			continue;
		return i;
	}
	( *vertexCount )++;
	verts[i] = *vertex;
	return i;
}

/*
=============
AddPatchTriangle
=============
*/
static void AddPatchTriangle( const drawVert_t *verts, unsigned short *indices,
							 int *triangleCount, unsigned short a,
							 unsigned short b, unsigned short c ) {
	int triangleBase;

	if ( a == b || b == c || c == a )
		return;
	if ( verts[a].xyz[0] == verts[b].xyz[0] && verts[a].xyz[1] == verts[b].xyz[1] && verts[a].xyz[2] == verts[b].xyz[2] )
		return;
	if ( verts[b].xyz[0] == verts[c].xyz[0] && verts[b].xyz[1] == verts[c].xyz[1] && verts[b].xyz[2] == verts[c].xyz[2] )
		return;
	if ( verts[c].xyz[0] == verts[a].xyz[0] && verts[c].xyz[1] == verts[a].xyz[1] && verts[c].xyz[2] == verts[a].xyz[2] )
		return;
	triangleBase = *triangleCount * 3;
	indices[triangleBase] = a;
	indices[triangleBase + 1] = b;
	indices[triangleBase + 2] = c;
	( *triangleCount )++;
}

vec_t VectorNormalize2(const vec3_t in, vec3_t out);

/*
=============
TriangulatePatch
=============
*/
void TriangulatePatch( parseMesh_t **patchEntry, int *triangleCount,
					  int *vertexCount, drawVert_t *verts, unsigned short *indices ) {
	unsigned short vertexMap[MAX_PATCH_TRIANGULATION_AXIS * MAX_PATCH_TRIANGULATION_AXIS];
	int i, j, k, count, nextRow;

	*triangleCount = 0;
	*vertexCount = 0;
	count = ( *patchEntry )->mesh.width * ( *patchEntry )->mesh.height;
	for ( i = 0; i < count; i++ )
		vertexMap[i] = FindPatchDrawVert( &( *patchEntry )->mesh.verts[i], verts, vertexCount );

	for ( i = 0; i < ( *patchEntry )->mesh.height - 1; i++ ) {
		for ( j = 0; j < ( *patchEntry )->mesh.width - 1; j++ ) {
			k = i * ( *patchEntry )->mesh.width + j;
			nextRow = k + ( *patchEntry )->mesh.width;
			if ( ( *patchEntry )->mesh.terrainFlags[k] & 1 ) {
				AddPatchTriangle( verts, indices, triangleCount, vertexMap[k], vertexMap[nextRow + 1], vertexMap[k + 1] );
				AddPatchTriangle( verts, indices, triangleCount, vertexMap[nextRow + 1], vertexMap[k], vertexMap[nextRow] );
			} else {
				AddPatchTriangle( verts, indices, triangleCount, vertexMap[k], vertexMap[nextRow], vertexMap[k + 1] );
				AddPatchTriangle( verts, indices, triangleCount, vertexMap[k + 1], vertexMap[nextRow], vertexMap[nextRow + 1] );
			}
		}
	}
}

#undef PATCH_ST_EPSILON
#undef MAX_PATCH_TRIANGULATION_AXIS
/*
=============
PatchGroupCompare
=============
*/
int PatchGroupCompare( const void *a, const void *b ) {
	const terrainPatchEntry_t *first = a, *second = b;
	if (first->patch->contents != second->patch->contents)
		return first->patch->contents - second->patch->contents;
	if (first->patch->shaderInfo != second->patch->shaderInfo)
		return first->patch->shaderInfo - second->patch->shaderInfo;
	if (first->patch->mesh.lightmapSampleSize != second->patch->mesh.lightmapSampleSize)
		return first->patch->mesh.lightmapSampleSize - second->patch->mesh.lightmapSampleSize;
	if (first->patch->cullgroup != second->patch->cullgroup)
		return first->patch->cullgroup - second->patch->cullgroup;
	return first->projectionAxis - second->projectionAxis;
}

/*
=============
MarkPatchNormalAxes
=============
*/
static void MarkPatchNormalAxes(const vec3_t normal, int axes[3])
{
	vec3_t normalized;

	VectorNormalize2(normal, normalized);
	if (normalized[2] * normalized[2] >= 0.933f)
		axes[2] = 1;
	else if (normalized[0] * normalized[0] >= 0.933f)
		axes[0] = 1;
	else if (normalized[1] * normalized[1] >= 0.933f)
		axes[1] = 1;
}

/*
=============
GuessPatchProjectionAxis
=============
*/
int GuessPatchProjectionAxis(parseMesh_t *patch)
{
	int x, y, base, right, below, belowRight, axis, numAxes;
	int axes[3];
	vec3_t total, normal, edgeA, edgeB;
	drawVert_t *firstOrigin, *firstSecond, *firstThird, *secondOrigin, *secondSecond, *secondThird;

	axes[0] = axes[1] = axes[2] = 0;
	VectorClear(total);
	for (y = 0; y < patch->mesh.height - 1; y++) {
		for (x = 0; x < patch->mesh.width - 1; x++) {
			base = y * patch->mesh.width + x;
			right = base + 1;
			below = base + patch->mesh.width;
			belowRight = below + 1;
			if (patch->mesh.terrainFlags[base] & 1) {
				firstOrigin = &patch->mesh.verts[base];
				firstSecond = &patch->mesh.verts[right];
				firstThird = &patch->mesh.verts[belowRight];
				secondOrigin = &patch->mesh.verts[base];
				secondThird = &patch->mesh.verts[below];
				secondSecond = &patch->mesh.verts[belowRight];
			} else {
				firstOrigin = &patch->mesh.verts[base];
				firstSecond = &patch->mesh.verts[right];
				firstThird = &patch->mesh.verts[below];
				secondOrigin = &patch->mesh.verts[right];
				secondThird = &patch->mesh.verts[below];
				secondSecond = &patch->mesh.verts[belowRight];
			}
			VectorSubtract(firstSecond->xyz, firstOrigin->xyz, edgeA);
			VectorSubtract(firstThird->xyz, firstOrigin->xyz, edgeB);
			CrossProduct(edgeA, edgeB, normal);
			VectorAdd(normal, total, total);
			MarkPatchNormalAxes(normal, axes);
			VectorSubtract(secondSecond->xyz, secondOrigin->xyz, edgeA);
			VectorSubtract(secondThird->xyz, secondOrigin->xyz, edgeB);
			CrossProduct(edgeA, edgeB, normal);
			VectorAdd(normal, total, total);
			MarkPatchNormalAxes(normal, axes);
		}
	}
	numAxes = axes[0] + axes[1] + axes[2];
	if (numAxes > 1)
		printf("WARNING: entity %i brush %i: tris with 2+ major axes; bad lighting likely\n", patch->entitynum, patch->brushnum);
	VectorNormalize(total);
	if (total[2] > 0.55f || total[2] < -0.55f)
		axis = 2;
	else
		axis = fabs(total[0]) < fabs(total[1]) ? 1 : 0;
	if (numAxes == 1 && !axes[axis]) {
		printf("WARNING: entity %i brush %i: tris with major axis not matching major axis for whole mesh; bad lighting likely\n", patch->entitynum, patch->brushnum);
		axis = axes[0] ? 0 : (axes[1] ? 1 : 2);
	}
	return axis;
}

void RoundLightmapBounds(int sampleSize, vec3_t mins, vec3_t maxs, vec3_t size);
void WriteTerrainSurfaceToPolyFile(terrainSurface_t *terrain);

/*
=============
RecenterPatchTexCoords
=============
*/
static void RecenterPatchTexCoords(mapDrawSurface_t *ds) {
	float mins[2], maxs[2];
	int i;
	if (!ds || !ds->shaderInfo)
		return;
	if (!ds->shaderInfo->clampTexture && !ds->shaderInfo->flap)
		return;
	memcpy(mins, ds->verts[0].st, sizeof(mins));
	maxs[0] = ds->verts[0].st[0];
	maxs[1] = ds->verts[0].st[1];
	for (i = 1; i < ds->numVerts; i++) {
		if (mins[0] > ds->verts[i].st[0])
			mins[0] = ds->verts[i].st[0];
		else if (maxs[0] < ds->verts[i].st[0])
			maxs[0] = ds->verts[i].st[0];
		if (mins[1] > ds->verts[i].st[1])
			mins[1] = ds->verts[i].st[1];
		else if (maxs[1] < ds->verts[i].st[1])
			maxs[1] = ds->verts[i].st[1];
	}
	maxs[0] += mins[0];
	mins[0] = FloorToInt(maxs[0] * 0.5f);
	{
		/* the original computes this in double precision; kept so the build matches the original binary */
		double sum = maxs[1];
		sum += mins[1];
		mins[1] = FloorToInt((float)sum * 0.5f);
	}
	for (i = 0; i < ds->numVerts; i++) {
		ds->verts[i].st[0] -= mins[0];
		ds->verts[i].st[1] -= mins[1];
	}
}

void TriangulatePatch( parseMesh_t **patchEntry, int *triangleCount,
	int *vertexCount, drawVert_t *verts, unsigned short *indices );
int GuessPatchProjectionAxis( parseMesh_t *patch );

/*
=============
DrawSurfaceForTerrain
=============
*/
static mapDrawSurface_t *DrawSurfaceForTerrain(terrainSurface_t *terrain) {
	mapDrawSurface_t *ds;
	int i;
	ds = AllocDrawSurf();
	ds->shaderInfo = terrain->shaderInfo;
	ds->contents = terrain->contents;
	ds->lightmapSampleSize = terrain->lightmapSampleSize;
	ds->mapBrush = NULL;
	ds->side = NULL;
	ds->triangleSoup = qtrue;
	ds->numIndexes = terrain->numIndexes;
	ds->indexes = malloc(ds->numIndexes * sizeof(*ds->indexes));
	for (i = 0; i < ds->numIndexes; i++)
		ds->indexes[i] = terrain->indexes[i];
	ds->numVerts = terrain->numVerts;
	ds->verts = malloc(ds->numVerts * sizeof(*ds->verts));
	memcpy(ds->verts, terrain->verts, ds->numVerts * sizeof(*ds->verts));
	RecenterPatchTexCoords(ds);
	ds->lightmapNum = -1;
	ds->fogNum = -1;
	ds->cullgroup = terrain->cullgroup;
	return ds;
}
/*
=============
SubdivideTerrain
=============
*/
void SubdivideTerrain(entity_t *entity, drawVert_t *verts, int vertexCount,
	unsigned short *indices, int triangleCount, shaderInfo_t *shader,
	int contents, int sampleSize, int cullgroup, int projectionAxis) {
	vec3_t mins, maxs, size;
	unsigned short splitVertexCount[32], splitTriangleCount[32];
	drawVert_t *splitVerts[32];
	unsigned short *splitIndices[32], *vertexMap[32];
	int i, axis, otherAxis, splitCount, split;
	int splitAxis, nonempty;
	unsigned short a, b, c, *map, *out;
	float splitScale;
	terrainSurface_t *terrain;

	ClearBounds(mins, maxs);
	for (i = 0; i < vertexCount; i++)
		AddPointToBounds(verts[i].xyz, mins, maxs);
	RoundLightmapBounds(sampleSize, mins, maxs, size);
	switch (projectionAxis) {
	case 0:
		axis = 1;
		otherAxis = 2;
		break;
	case 1:
		axis = 0;
		otherAxis = 2;
		break;
	default:
		axis = 0;
		otherAxis = 1;
		break;
	}
	/* split along the larger lightmap dimension */
	splitCount = 0;
	if ( size[axis] >= size[otherAxis] ) {
		if ( size[axis] > 512.0f ) {
			splitAxis = axis;
			splitCount = (int)ceil( size[axis] / 472 );
		}
	} else if ( size[otherAxis] > 512.0f ) {
		splitAxis = otherAxis;
		splitCount = (int)ceil( size[otherAxis] / 472.0 );
	}
	if (splitCount) {
		axis = splitAxis;
		splitScale = splitCount / (maxs[axis] - mins[axis]);
		if ((unsigned)splitCount > 32)
			Error("terrain mesh exceeded maximum lightmap splits (%i > %i)\n", splitCount, 32);
		for (i = 0; i < splitCount; i++) {
			splitVertexCount[i] = 0;
			splitTriangleCount[i] = 0;
			splitIndices[i] = malloc(3 * triangleCount * sizeof(*indices));
			splitVerts[i] = malloc(vertexCount * sizeof(*verts));
			vertexMap[i] = malloc(vertexCount * sizeof(*vertexMap[i]));
			memset(vertexMap[i], 0xff, vertexCount * sizeof(*vertexMap[i]));
		}
		nonempty = 0;
		for (i = 0; i < triangleCount; i++) {
			a = indices[i * 3];
			b = indices[i * 3 + 1];
			c = indices[i * 3 + 2];
			split = floor(((verts[a].xyz[splitAxis] + verts[b].xyz[splitAxis] + verts[c].xyz[splitAxis]) / 3.0f - mins[splitAxis]) * splitScale);
			map = vertexMap[split];
			if (map[a] == 0xffff) {
				map[a] = splitVertexCount[split];
				splitVerts[split][splitVertexCount[split]] = verts[a];
				splitVertexCount[split]++;
			}
			if (map[b] == 0xffff) {
				map[b] = splitVertexCount[split];
				splitVerts[split][splitVertexCount[split]] = verts[b];
				splitVertexCount[split]++;
			}
			if (map[c] == 0xffff) {
				map[c] = splitVertexCount[split];
				splitVerts[split][splitVertexCount[split]] = verts[c];
				splitVertexCount[split]++;
			}
			out = &splitIndices[split][3 * splitTriangleCount[split]];
			out[0] = map[a]; out[1] = map[b]; out[2] = map[c];
			if (!splitTriangleCount[split]) nonempty++;
			splitTriangleCount[split]++;
		}
		for (i = 0; i < splitCount; i++) {
			if (nonempty > 1)
				SubdivideTerrain(entity, splitVerts[i], splitVertexCount[i], splitIndices[i],
					splitTriangleCount[i], shader, contents, sampleSize, cullgroup, projectionAxis);
			free(splitIndices[i]);
			free(splitVerts[i]);
			free(vertexMap[i]);
		}
		if (nonempty > 1)
			return;
	}
	terrain = malloc(sizeof(*terrain));
	terrain->next = entity->terrainSurfaces;
	entity->terrainSurfaces = terrain;
	terrain->numVerts = vertexCount;
	terrain->shaderInfo = shader;
	terrain->contents = contents;
	terrain->lightmapSampleSize = sampleSize;
	terrain->cullgroup = cullgroup;
	terrain->verts = malloc(vertexCount * sizeof(*terrain->verts));
	memcpy(terrain->verts, verts, terrain->numVerts * sizeof(*terrain->verts));
	terrain->numIndexes = 3 * triangleCount;
	terrain->indexes = malloc(terrain->numIndexes * sizeof(*terrain->indexes));
	memcpy(terrain->indexes, indices, terrain->numIndexes * sizeof(*terrain->indexes));
	if (terrain->shaderInfo->surfaceFlags & SURF_NODRAW) {
		if (terrain->shaderInfo->surfaceFlags & SURF_CASTSHADOW)
			WriteTerrainSurfaceToPolyFile(terrain);
	} else {
		DrawSurfaceForTerrain(terrain);
	}
}

/*
=============
EmitTerrainPatchGroup
=============
*/
void EmitTerrainPatchGroup( entity_t *entity, terrainPatchEntry_t *patches, int patchCount ) {
	struct {
		unsigned short indexBuffer[MAX_TERRAIN_GROUP_TRIANGLES];
		unsigned short triangleBuffer[MAX_TERRAIN_GROUP_TRIANGLES];
		drawVert_t vertexBuffer[MAX_TERRAIN_GROUP_VERTS];
	} scratch;
	unsigned short *indices, *triangles;
	drawVert_t *verts;
	int triangleCount, vertexCount, i, width, height;

	triangleCount = 0;
	vertexCount = 0;
		for ( i = 0; i < patchCount; i++ ) {
		height = patches[i].patch->mesh.height;
		width = patches[i].patch->mesh.width;
		triangleCount += (width - 1) * (2 * height - 2);
		vertexCount += width * height;
	}

	/* the result is never read; the call is kept as in the original */
	triangles = AllocStackOrHeap( triangleCount, MAX_TERRAIN_GROUP_TRIANGLES,
		scratch.triangleBuffer, sizeof( unsigned short ) );
	indices = AllocStackOrHeap( 3 * triangleCount, MAX_TERRAIN_GROUP_TRIANGLES,
		scratch.indexBuffer, sizeof( unsigned short ) );
	verts = AllocStackOrHeap( vertexCount, MAX_TERRAIN_GROUP_VERTS,
		scratch.vertexBuffer, sizeof( drawVert_t ) );
	for ( i = 0; i < patchCount; i++ ) {
		TriangulatePatch( &patches[i].patch, &triangleCount, &vertexCount, verts, indices );
		SubdivideTerrain( entity, verts, vertexCount, indices, triangleCount,
			patches[i].patch->shaderInfo, patches[i].patch->contents,
			patches[i].patch->mesh.lightmapSampleSize, patches[i].patch->cullgroup,
			patches[i].projectionAxis );
	}
	FreeStackOrHeap( indices, scratch.indexBuffer );
	FreeStackOrHeap( verts, scratch.vertexBuffer );
}

/*
=============
ProcessTerrainPatches
=============
*/
void ProcessTerrainPatches( entity_t *entity ) {
	terrainPatchEntry_t stackPatches[MAX_TERRAIN_PATCHES], *patches;
	parseMesh_t *patch;
	int patchCount, i, groupStart, groupSize;

	patchCount = 0;
	for ( patch = entity->patches; patch; patch = patch->next ) {
		if ( patch->isTerrain )
			patchCount++;
	}
	if ( !patchCount )
		return;
	patches = AllocStackOrHeap( patchCount, MAX_TERRAIN_PATCHES,
		stackPatches, sizeof( stackPatches[0] ) );
	patchCount = 0;
	for ( patch = entity->patches; patch; patch = patch->next ) {
		if ( patch->isTerrain ) {
			patches[patchCount].patch = patch;
			patches[patchCount].projectionAxis = GuessPatchProjectionAxis( patch );
			patchCount++;
		}
	}
	qsort( patches, patchCount, sizeof( patches[0] ), PatchGroupCompare );
	groupStart = 0;
	groupSize = 1;
	for ( i = 1; i < patchCount; i++ ) {
		if ( patches[i - 1].patch->contents == patches[i].patch->contents &&
			patches[i - 1].patch->shaderInfo == patches[i].patch->shaderInfo &&
			patches[i - 1].patch->mesh.lightmapSampleSize == patches[i].patch->mesh.lightmapSampleSize &&
			patches[i - 1].patch->cullgroup == patches[i].patch->cullgroup &&
			patches[i - 1].projectionAxis == patches[i].projectionAxis ) {
			groupSize++;
		} else {
			EmitTerrainPatchGroup( entity, &patches[groupStart], groupSize );
			groupStart = i;
			groupSize = 1;
		}
	}
	EmitTerrainPatchGroup( entity, &patches[groupStart], groupSize );
	FreeStackOrHeap( patches, stackPatches );
}


/*
=====================
PatchMapDrawSurfs

Any patches that share an edge need to choose their
level of detail as a unit, otherwise the edges would
pull apart.
=====================
*/
void PatchMapDrawSurfs( entity_t *e ) {
	parseMesh_t			*pm;
	parseMesh_t			*check, *scan;
	mapDrawSurface_t	*ds;
	int					patchCount, groupCount;
	int					i, j, k, l, c1, c2;
	drawVert_t			*v1, *v2;
	vec3_t				bounds[2];
	byte				*bordering;
	parseMesh_t			*meshes[MAX_MAP_DRAW_SURFS];
	qboolean			grouped[MAX_MAP_DRAW_SURFS];
	byte				group[MAX_MAP_DRAW_SURFS];

	qprintf( "----- PatchMapDrawSurfs -----\n" );

	patchCount = 0;
	for ( pm = e->patches ; pm ; pm = pm->next  ) {
		if (pm->isTerrain)
			continue;
		meshes[patchCount] = pm;
		patchCount++;
	}

	if ( !patchCount ) {
		return;
	}
	bordering = malloc( patchCount * patchCount );
	memset( bordering, 0, patchCount * patchCount );

	// build the bordering matrix
	for ( k = 0 ; k < patchCount ; k++ ) {
		bordering[k*patchCount+k] = 1;

		for ( l = k+1 ; l < patchCount ; l++ ) {
			check = meshes[k];
			scan = meshes[l];
			c1 = scan->mesh.width * scan->mesh.height;
			v1 = scan->mesh.verts;

			for ( i = 0 ; i < c1 ; i++, v1++ ) {
				c2 = check->mesh.width * check->mesh.height;
				v2 = check->mesh.verts;
				for ( j = 0 ; j < c2 ; j++, v2++ ) {
					if ( fabs( v1->xyz[0] - v2->xyz[0] ) < 1.0
						&& fabs( v1->xyz[1] - v2->xyz[1] ) < 1.0
						&& fabs( v1->xyz[2] - v2->xyz[2] ) < 1.0 ) {
						break;
					}
				}
				if ( j != c2 ) {
					break;
				}
			}
			if ( i != c1 ) {
				// we have a connection
				bordering[k*patchCount+l] =
				bordering[l*patchCount+k] = 1;
			} else {
				// no connection
				bordering[k*patchCount+l] =
				bordering[l*patchCount+k] = 0;
			}

		}
	}

	// build groups
	memset( grouped, 0, sizeof(grouped) );
	groupCount = 0;
	for ( i = 0 ; i < patchCount ; i++ ) {
		if ( !grouped[i] ) {
			groupCount++;
		}

		// recursively find all patches that belong in the same group
		memset( group, 0, patchCount );
		GrowGroup_r( i, patchCount, bordering, group );

		// bound them
		ClearBounds( bounds[0], bounds[1] );
		for ( j = 0 ; j < patchCount ; j++ ) {
			if ( group[j] ) {
				grouped[j] = qtrue;
				scan = meshes[j];
				c1 = scan->mesh.width * scan->mesh.height;
				v1 = scan->mesh.verts;
				for ( k = 0 ; k < c1 ; k++, v1++ ) {
					AddPointToBounds( v1->xyz, bounds[0], bounds[1] );
				}
			}
		}

		// create drawsurf
		scan = meshes[i];
		scan->grouped = qtrue;
		ds = DrawSurfaceForMesh( &scan->mesh, scan->isTerrain, scan->cullgroup );
		ds->shaderInfo = scan->shaderInfo;
		ds->contents = scan->contents;
		RecenterPatchTexCoords(ds);
		VectorCopy( bounds[0], ds->lightmapVecs[0] );
		VectorCopy( bounds[1], ds->lightmapVecs[1] );
	}

	qprintf( "%5i patches\n", patchCount );
	qprintf( "%5i patch LOD groups\n", groupCount );
}
