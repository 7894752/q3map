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

#include "qbsp.h"
#include "../universal/com_math.h"

#include <float.h>

enum { LIGHTMAP_POLYGON_POINTS = 1024, LIGHTMAP_POLYGON_WORK_POINTS = 1028 };
typedef float lightmapPoint2_t[2];
typedef lightmapPoint2_t lightmapPolygon2_t[LIGHTMAP_POLYGON_WORK_POINTS];
typedef struct {
	int count;
	int groups[2];
} lightmapGroupCell_t;


/*
=============
LightmapPolygonAreaAndCentroid
=============
*/
static double LightmapPolygonAreaAndCentroid( float *points, int numPoints, float center[ 2 ] ) {
	double area;
	double centerX;
	double centerY;
	double scale;
	int i;
	int last;
	float edge;
	float sumX, sumY;

	last = numPoints - 1;
	area = ( points[ 2 * ( last - 1 ) + 1 ] - points[ 1 ] ) * points[ 2 * last ]
		+ ( points[ 2 * last + 1 ] - points[ 3 ] ) * points[ 0 ];
	centerX = ( points[ 2 * last ] + points[ 0 ] + points[ 2 ] )
		* ( points[ 2 ] - points[ 2 * last ] ) * points[ 1 ]
		+ ( points[ 2 * ( last - 1 ) ] + points[ 2 * last ] + points[ 0 ] )
		* ( points[ 0 ] - points[ 2 * ( last - 1 ) ] ) * points[ 2 * last + 1 ];
	centerY = ( points[ 3 ] + points[ 2 * last + 1 ] + points[ 1 ] )
		* points[ 0 ] * ( points[ 2 * last + 1 ] - points[ 3 ] )
		+ ( points[ 2 * ( last - 1 ) + 1 ] + points[ 2 * last + 1 ] + points[ 1 ] )
		* points[ 2 * last ] * ( points[ 2 * ( last - 1 ) + 1 ] - points[ 1 ] );

	if ( last > 1 ) {
		i = last - 1;
		do {
			edge = points[ 1 ] - points[ 5 ];
			points += 2;
			area += points[ 0 ] * edge;
			sumX = points[ -2 ] + points[ 2 ];
			sumX += points[ 0 ];
			sumY = points[ -1 ];
			sumY += points[ 3 ];
			sumY += points[ 1 ];
			centerX += sumX
				* ( points[ 2 ] - points[ -2 ] ) * points[ 1 ];
			centerY += sumY
				* points[ 0 ] * edge;
		} while ( --i );
	}

	scale = 1.0 / ( area * 3.0 );
	center[ 0 ] = ( scale * centerX );
	center[ 1 ] = ( scale * centerY );
	return area;
}

/*
=============
ClipLightmapPolygon
=============
*/
static void ClipLightmapPolygon( float *points, int numPoints, int axis, float dist,
	float *frontPoints, int *frontCount, float *backPoints, int *backCount ) {
	int sides[ MAX_POINTS_ON_WINDING ];
	int numFront, numBack;
	float frac;
	float mid[ 2 ];
	float frontDist, backDist;
	int i;
	int previous;
	int j;

	frontDist = dist + 0.001f;
	backDist = dist - 0.001f;
	*frontCount = 0;
	*backCount = 0;
	numFront = 0;
	numBack = 0;
	for ( i = 0; i < numPoints; i++ ) {
		sides[ i ] = SIDE_ON;
		if ( points[ 2 * i + axis ] > frontDist ) {
			sides[ i ] = SIDE_FRONT;
			numFront++;
		}
		else if ( points[ 2 * i + axis ] < backDist ) {
			sides[ i ] = SIDE_BACK;
			numBack++;
		}
	}

	if ( !numBack ) {
		memcpy( frontPoints, points, 2 * sizeof( float ) * numPoints );
		*frontCount = numPoints;
		return;
	}
	if ( !numFront ) {
		memcpy( backPoints, points, 2 * sizeof( float ) * numPoints );
		*backCount = numPoints;
		return;
	}

	previous = numPoints - 1;
	for ( j = 0; j < numPoints; previous = j, j++ ) {
		if ( sides[ previous ] == SIDE_ON ) {
			frontPoints[ 2 * *frontCount ] = points[ 2 * previous ];
			frontPoints[ 2 * *frontCount + 1 ] = points[ 2 * previous + 1 ];
			( *frontCount )++;
			backPoints[ 2 * *backCount ] = points[ 2 * previous ];
			backPoints[ 2 * *backCount + 1 ] = points[ 2 * previous + 1 ];
			( *backCount )++;
			continue;
		}
		else if ( sides[ previous ] == SIDE_FRONT ) {
			frontPoints[ 2 * *frontCount ] = points[ 2 * previous ];
			frontPoints[ 2 * *frontCount + 1 ] = points[ 2 * previous + 1 ];
			( *frontCount )++;
		}
		else if ( sides[ previous ] == SIDE_BACK ) {
			backPoints[ 2 * *backCount ] = points[ 2 * previous ];
			backPoints[ 2 * *backCount + 1 ] = points[ 2 * previous + 1 ];
			( *backCount )++;
		}

		if ( sides[ j ] != SIDE_ON && sides[ j ] != sides[ previous ] ) {
			frac = ( points[ 2 * previous + axis ] - dist )
				/ ( points[ 2 * previous + axis ] - points[ 2 * j + axis ] );
			mid[ 1 - axis ] = points[ 2 * previous + 1 - axis ]
				+ frac * ( points[ 2 * j + 1 - axis ]
				- points[ 2 * previous + 1 - axis ] );
			mid[ axis ] = dist;
			frontPoints[ 2 * *frontCount ] = mid[ 0 ];
			frontPoints[ 2 * *frontCount + 1 ] = mid[ 1 ];
			( *frontCount )++;
			backPoints[ 2 * *backCount ] = mid[ 0 ];
			backPoints[ 2 * *backCount + 1 ] = mid[ 1 ];
			( *backCount )++;
		}
	}

	if ( *frontCount > MAX_POINTS_ON_WINDING || *backCount > MAX_POINTS_ON_WINDING ) {
		Error( "MAX_POINTS_ON_WINDING exceeded on 2d poly\n" );
	}
}

/*
=============
ScanConvertLightmapPolygon
=============
*/
static void ScanConvertLightmapPolygon( int numPoints,
	float *points, int width, int height,
	float xOrigin, float yOrigin, float xStep, float yStep,
	void ( *callback )( int x, int y ) ) {
	float center[ 2 ];
	float x, y;
	int remainingPoly, rowPoly, cellPoly, sparePoly;
	int remainingCount, rowCount, cellCount, nextCount;
	int temp;
	int row, column;

	remainingPoly = 0;
	remainingCount = numPoints;
	rowPoly = 1;
	cellPoly = 2;
	sparePoly = 3;
	y = yOrigin;
	for ( row = 0; row < height; row++ ) {
		if ( row == height - 1 ) {
			rowPoly = remainingPoly;
			rowCount = remainingCount;
		}
		else {
			y += yStep;
			ClipLightmapPolygon( &points[ remainingPoly * ( LIGHTMAP_POLYGON_WORK_POINTS * 2 ) ], remainingCount, 1, y,
				&points[ sparePoly * ( LIGHTMAP_POLYGON_WORK_POINTS * 2 ) ], &nextCount, &points[ rowPoly * ( LIGHTMAP_POLYGON_WORK_POINTS * 2 ) ], &rowCount );
			temp = remainingPoly;
			remainingPoly = sparePoly;
			sparePoly = temp;
			remainingCount = nextCount;
		}

		x = xOrigin;
		for ( column = 0; column < width && rowCount; column++ ) {
			if ( column == width - 1 ) {
				temp = rowPoly;
				rowPoly = cellPoly;
				cellPoly = temp;
				cellCount = rowCount;
			}
			else {
				x += xStep;
				ClipLightmapPolygon( &points[ rowPoly * ( LIGHTMAP_POLYGON_WORK_POINTS * 2 ) ], rowCount, 0, x,
					&points[ sparePoly * ( LIGHTMAP_POLYGON_WORK_POINTS * 2 ) ], &nextCount, &points[ cellPoly * ( LIGHTMAP_POLYGON_WORK_POINTS * 2 ) ], &cellCount );
				temp = rowPoly;
				rowPoly = sparePoly;
				sparePoly = temp;
				rowCount = nextCount;
			}

			if ( cellCount >= 3 &&
				(float) fabs( LightmapPolygonAreaAndCentroid( &points[ cellPoly * ( LIGHTMAP_POLYGON_WORK_POINTS * 2 ) ], cellCount, center ) ) > 0.0f ) {
				callback( column, row );
			}
		}
	}
}



enum { LIGHTMAP_BLOCK_SIZE = 512, MAX_LIGHTMAP_STORAGE = 0x1000000 };

/* Skyline allocation storage retained from the Q3 lightmap allocator. */
int allocated[LIGHTMAP_BLOCK_SIZE];

typedef struct lightmapRef_s {
	struct lightmapRef_s *next;
	int lightmapNum;
} lightmapRef_t;

typedef struct lightmapBlock_s {
	int lightmapNum;
	int x, y;
	int width, height;
	struct lightmapBlock_s *next, *prev;
} lightmapBlock_t;

typedef struct {
	mapDrawSurface_t *surfaces;
	int numSurfaces;
	int maxArea;
	lightmapRef_t *lightmaps;
} lightmapMaterial_t;

lightmapMaterial_t *currentMaterialSet;
int samplesize = 16;
int numLightmaps = 1;
float lightmapEfficiency = 0;
qboolean colorLightmaps = 0;
static lightmapBlock_t *lightmapBlocks = NULL;
float lightmapNormalDotThreshold = 0.9f;
const int lightmapBorder = 2;
extern int c_exactLightmap;

/*
=============
AllocLightmapBlock
=============
*/
static lightmapBlock_t *AllocLightmapBlock( void ) {
	return malloc( sizeof( lightmapBlock_t ) );
}

/*
=============
FreeLightmapBlock
=============
*/
static void FreeLightmapBlock( lightmapBlock_t *block ) {
	if ( block->prev ) {
		block->prev->next = block->next;
	}
	if ( block->next ) {
		block->next->prev = block->prev;
	}
	if ( lightmapBlocks == block ) {
		lightmapBlocks = block->next;
	}
	free( block );
}

/*
=============
MergeLightmapBlock
=============
*/
static void MergeLightmapBlock( lightmapBlock_t *block ) {
	lightmapBlock_t *other;

restart:
	for ( other = lightmapBlocks ; other ; other = other->next ) {
		if ( block->lightmapNum != other->lightmapNum || block == other ) {
			continue;
		}
		if ( block->x == other->x ) {
			if ( block->width != other->width ) {
				continue;
			}
			if ( block->y == other->y + other->height ) {
				other->height += block->height;
				FreeLightmapBlock( block );
				block = other;
				goto restart;
			}
			if ( block->y + block->height == other->y ) {
				block->height += other->height;
				FreeLightmapBlock( other );
				goto restart;
			}
		}
		else if ( block->y == other->y && block->height == other->height ) {
			if ( block->x == other->x + other->width ) {
				other->width += block->width;
				FreeLightmapBlock( block );
				block = other;
				goto restart;
			}
			if ( block->x + block->width == other->x ) {
				block->width += other->width;
				FreeLightmapBlock( other );
				goto restart;
			}
		}
	}
}

/*
=============
SplitLightmapBlock
=============
*/
static void SplitLightmapBlock( lightmapBlock_t *block, int w, int h ) {
	lightmapBlock_t *newBlock;

	if ( w == block->width ) {
		block->y += h;
		block->height -= h;
		if ( block->height <= 2 ) {
			FreeLightmapBlock( block );
			return;
		}
	}
	else if ( h == block->height ) {
		block->x += w;
		block->width -= w;
		if ( block->width <= 2 ) {
			FreeLightmapBlock( block );
			return;
		}
	}
	else {
		if ( block->width * h < block->height * w ) {
			block->y += h;
			block->height -= h;
			if ( block->width - w <= 2 ) {
				goto merge;
			}
			newBlock = AllocLightmapBlock();
			newBlock->x = block->x + w;
			newBlock->y = block->y - h;
			newBlock->width = block->width - w;
			newBlock->height = h;
		}
		else {
			block->x += w;
			block->width -= w;
			if ( block->height - h <= 2 ) {
				goto merge;
			}
			newBlock = AllocLightmapBlock();
			newBlock->x = block->x - w;
			newBlock->y = block->y + h;
			newBlock->width = w;
			newBlock->height = block->height - h;
		}
		newBlock->lightmapNum = block->lightmapNum;
		newBlock->prev = block;
		newBlock->next = block->next;
		if ( block->next ) {
			block->next->prev = newBlock;
		}
		block->next = newBlock;
		MergeLightmapBlock( newBlock );
	}
merge:
	MergeLightmapBlock( block );
}

/*
=============
ColorLightmapBlock
=============
*/
static void ColorLightmapBlock( int lightmapNum, int x, int y, int w, int h ) {
	vec3_t color;
	float scale, value;
	byte rgb[3];
	byte *dest;
	int i, j;

	color[0] = rand() * ( 1.0f / 32767.0f );
	color[1] = rand() * ( 1.0f / 32767.0f );
	color[2] = rand() * ( 1.0f / 32767.0f );
	scale = 63.0f / ( color[0] > ( color[1] > color[2] ? color[1] : color[2] )
		? color[0] : ( color[1] > color[2] ? color[1] : color[2] ) );
	value = color[0] * scale;
	rgb[0] = 63.0f < value ? 63.0f : value;
	value = color[1] * scale;
	rgb[1] = 63.0f < value ? 63.0f : value;
	value = color[2] * scale;
	rgb[2] = 63.0f < value ? 63.0f : value;
	for ( i = y ; i < y + h ; i++ ) {
		for ( j = x ; j < x + w ; j++ ) {
			lightBytes[( ( lightmapNum * LIGHTMAP_BLOCK_SIZE + i ) * LIGHTMAP_BLOCK_SIZE + j ) * 3] = rgb[0];
			lightBytes[( ( lightmapNum * LIGHTMAP_BLOCK_SIZE + i ) * LIGHTMAP_BLOCK_SIZE + j ) * 3 + 1] = rgb[1];
			lightBytes[( ( lightmapNum * LIGHTMAP_BLOCK_SIZE + i ) * LIGHTMAP_BLOCK_SIZE + j ) * 3 + 2] = rgb[2];
		}
	}
}

/*
=============
AllocBestFitBlock
=============
*/
qboolean AllocBestFitBlock( int w, int h, int *lightmapNum, int *x,
	int *y, qboolean *rotated, lightmapRef_t *allowed ) {
	lightmapBlock_t *block, *best;
	lightmapRef_t *ref;
	int minSide, remainder, area, dx, dy, temp, rotation;
	qboolean bestRotated, found;

	minSide = 0;
	remainder = 0;
	area = 0;
	bestRotated = qfalse;
	best = NULL;
	found = qfalse;
	for ( rotation = 0 ; rotation < 2 ; rotation++ ) {
		for ( block = lightmapBlocks ; block ; block = block->next ) {
			if ( allowed ) {
				for ( ref = allowed ; ref ; ref = ref->next ) {
					if ( block->lightmapNum == allowed->lightmapNum ) {
						break;
					}
				}
				if ( !ref ) {
					continue;
				}
			}
			if ( block->width == w && block->height == h ) {
				found = qtrue;
				break;
			}
			if ( block->width == w && block->height > h ) {
				if ( !best || minSide > 0 || remainder < block->height - h ) {
					best = block;
					remainder = block->height - h;
					minSide = 0;
					bestRotated = rotation == 1;
				}
			}
			else if ( block->height == h && block->width > w ) {
				if ( !best || minSide > 0 || remainder < block->width - w ) {
					best = block;
					remainder = block->width - w;
					minSide = 0;
					bestRotated = rotation == 1;
				}
			}
			else if ( block->width > w && block->height > h ) {
				dx = block->width - w;
				dy = block->height - h;
				temp = block->height * dx < block->width * dy ? block->height * dx : block->width * dy;
				if ( !best || ( minSide > 0 && area > temp ) ) {
					best = block;
					minSide = dx < dy ? dx : dy;
					remainder = dx + dy - minSide;
					area = temp;
					bestRotated = rotation == 1;
				}
			}
		}
		if ( found ) {
			*lightmapNum = block->lightmapNum;
			*x = block->x;
			*y = block->y;
			*rotated = rotation == 1;
			if ( colorLightmaps ) {
				ColorLightmapBlock( block->lightmapNum, block->x, block->y, w, h );
			}
			FreeLightmapBlock( block );
			return qtrue;
		}
		if ( w == h ) {
			break;
		}
		temp = h;
		h = w;
		w = temp;
	}
	if ( best ) {
		*lightmapNum = best->lightmapNum;
		*x = best->x;
		*y = best->y;
		*rotated = bestRotated;
		if ( bestRotated ) {
			temp = h;
			h = w;
			w = temp;
		}
		if ( colorLightmaps ) {
			ColorLightmapBlock( best->lightmapNum, best->x, best->y, w, h );
		}
		SplitLightmapBlock( best, w, h );
		return qtrue;
	}
	return qfalse;
}

/*
=============
PrepareNewLightmap
=============
*/
void PrepareNewLightmap( void ) {
	lightmapBlock_t *block;

	block = AllocLightmapBlock();
	block->lightmapNum = numLightmaps;
	block->width = LIGHTMAP_BLOCK_SIZE;
	block->height = LIGHTMAP_BLOCK_SIZE;
	block->x = 0;
	block->y = 0;
	block->next = lightmapBlocks;
	block->prev = NULL;
	if ( lightmapBlocks ) {
		lightmapBlocks->prev = block;
	}
	lightmapBlocks = block;
	numLightmaps++;
	if ( numLightmaps * LIGHTMAP_BLOCK_SIZE * LIGHTMAP_BLOCK_SIZE * 3 > MAX_LIGHTMAP_STORAGE ) {
		Error( "MAX_MAP_LIGHTING (%g MB) exceeded... more than %i lightmaps used\n",
			MAX_LIGHTMAP_STORAGE / ( 1024.0 * 1024.0 ), numLightmaps - 1 );
	}
	if ( colorLightmaps ) {
		numLightBytes = numLightmaps * LIGHTMAP_BLOCK_SIZE * LIGHTMAP_BLOCK_SIZE * 3;
	}
}

/*
=============
AllocLightmapRectangle
=============
*/
static qboolean AllocLightmapRectangle( int w, int h, int *lightmapNum, int *x,
	int *y, qboolean *rotated ) {
	if ( !AllocBestFitBlock( w, h, lightmapNum, x, y, rotated,
		currentMaterialSet->lightmaps ) ) {
		if ( !currentMaterialSet->lightmaps ||
			!AllocBestFitBlock( w, h, lightmapNum, x, y, rotated, NULL ) ) {
			PrepareNewLightmap();
			if ( !AllocBestFitBlock( w, h, lightmapNum, x, y, rotated, NULL ) ) {
				return qfalse;
			}
		}
	}
	return qtrue;
}

/*
=============
AddLightmapToCurrentMaterial
=============
*/
static void AddLightmapToCurrentMaterial( int lightmapNum ) {
	lightmapRef_t *ref;
	for ( ref = currentMaterialSet->lightmaps; ref; ref = ref->next ) {
		if ( ref->lightmapNum == lightmapNum ) return;
	}
	ref = malloc( sizeof( *ref ) );
	ref->lightmapNum = lightmapNum;
	ref->next = currentMaterialSet->lightmaps;
	currentMaterialSet->lightmaps = ref;
}

/*
=============
CountAndFreeMaterialLightmaps
=============
*/
static int CountAndFreeMaterialLightmaps( void ) {
	int count = 0;
	lightmapRef_t *ref, *next;
	while ( currentMaterialSet->lightmaps ) {
		ref = currentMaterialSet->lightmaps;
		next = ref->next;
		free( ref );
		currentMaterialSet->lightmaps = next;
		count++;
	}
	return count;
}

enum { LIGHTMAP_GROUP_BATCH_LIMIT = 1024 };

/*
=============
SurfaceLightmapSampleSize
=============
*/
static int SurfaceLightmapSampleSize( mapDrawSurface_t *ds ) {
	if ( ds->lightmapSampleSize ) return ds->lightmapSampleSize;
	if ( ds->shaderInfo->lightmapSampleSize ) return ds->shaderInfo->lightmapSampleSize;
	return samplesize;
}

float lightmapGridMins[2];
int lightmapCellsGroup;
int lightmapGridWidth;
int bspLightmapGridHeight;
lightmapGroupCell_t *lightmapGroupGrid;

/*
=============
RoundLightmapCoordinate
=============
*/
void RoundLightmapCoordinate( int sampleSize, float *mins, float *maxs, float *size ) {
	float invSampleSize;
	float sampleScale;

	sampleScale = sampleSize;
	invSampleSize = 1.0f / sampleScale;
	*mins = floor( *mins * invSampleSize - 0.5f );
	*maxs = ceil( *maxs * invSampleSize + 0.5f );
	*size = *maxs - *mins + 1.0f;
	*mins *= sampleScale;
	*maxs *= sampleScale;
}

/*
=============
RoundLightmapBounds
=============
*/
void RoundLightmapBounds( int sampleSize, vec3_t mins, vec3_t maxs, vec3_t size ) {
	RoundLightmapCoordinate( sampleSize, &mins[0], &maxs[0], &size[0] );
	RoundLightmapCoordinate( sampleSize, &mins[1], &maxs[1], &size[1] );
	RoundLightmapCoordinate( sampleSize, &mins[2], &maxs[2], &size[2] );
}

/*
=============
FitLightmapCoordinate
=============
*/
int FitLightmapCoordinate( int maxSize, int sampleSize, float *mins,
	float *maxs, float *size ) {
	float originalMins;
	float originalMaxs;

	originalMins = *mins;
	originalMaxs = *maxs;
	RoundLightmapCoordinate( sampleSize, mins, maxs, size );
	while ( !( *size < maxSize ) ) {
		*mins = originalMins;
		*maxs = originalMaxs;
		sampleSize++;
		RoundLightmapCoordinate( sampleSize, mins, maxs, size );
	}
	return sampleSize;
}

/*
=============
LightmapSurfaceBounds
=============
*/
static void LightmapSurfaceBounds( mapDrawSurface_t *ds, vec3_t mins,
	vec3_t maxs, vec3_t size ) {
	int i;
	ClearBounds( mins, maxs );
	for ( i = 0; i < ds->numVerts; i++ ) {
		AddPointToBounds( ds->verts[i].xyz, mins, maxs );
	}
	RoundLightmapBounds( SurfaceLightmapSampleSize( ds ), mins, maxs, size );
}

/*
=============
LightmapGroupBounds
=============
*/
static void LightmapGroupBounds( mapDrawSurface_t *ds, vec3_t mins,
	vec3_t maxs, vec3_t size ) {
	int i;
	mapDrawSurface_t *first;
	ClearBounds( mins, maxs );
	first = ds;
	do {
		for ( i = 0; i < ds->numVerts; i++ ) {
			AddPointToBounds( ds->verts[i].xyz, mins, maxs );
		}
		ds = ds->nextOnLightmapGroup;
	} while ( ds != first );
	RoundLightmapBounds( SurfaceLightmapSampleSize( ds ), mins, maxs, size );
}

/*
=============
ComputePatchLightmapSize
=============
*/
static void ComputePatchLightmapSize( mapDrawSurface_t *ds, int *width, int *height ) {
	mesh_t mesh, *subdivided, *linear, *lightmapMesh;
	int widthtable[MAX_EXPANDED_AXIS], heighttable[MAX_EXPANDED_AXIS];
	int sampleSize;

	mesh.width = ds->patchWidth;
	mesh.height = ds->patchHeight;
	mesh.verts = ds->verts;
	if ( ds->isTerrain ) {
		linear = RemoveLinearMeshColumnsRows( &mesh, NULL, NULL );
	}
	else {
		subdivided = SubdivideMesh( mesh, 8, 999, NULL, NULL );
		PutMeshOnCurve( *subdivided );
		linear = RemoveLinearMeshColumnsRows( subdivided, NULL, NULL );
		FreeMesh( subdivided );
	}
	sampleSize = SurfaceLightmapSampleSize( ds );
	lightmapMesh = SubdivideMeshQuads( linear, sampleSize,
		MAX_EXPANDED_AXIS, heighttable, widthtable, NULL, NULL );
	*width = lightmapMesh->width;
	*height = lightmapMesh->height;
}

/* Triangle-soup projection axis from the accumulated oriented area. */
/*
=============
LightmapAxisForSurface
=============
*/
static int LightmapAxisForSurface( mapDrawSurface_t *ds ) {
	int i, axis;
	vec3_t edge1, edge2, normal, total;

	VectorClear( total );
	for ( i = 0; i < ds->numIndexes; i += 3 ) {
		VectorSubtract( ds->verts[ds->indexes[i+2]].xyz,
			ds->verts[ds->indexes[i]].xyz, edge1 );
		VectorSubtract( ds->verts[ds->indexes[i+1]].xyz,
			ds->verts[ds->indexes[i]].xyz, edge2 );
		CrossProduct( edge1, edge2, normal );
		VectorAdd( total, normal, total );
	}
	axis = 0;
	if ( fabs( total[1] ) > fabs( total[0] ) ) axis = 1;
	if ( fabs( total[2] ) > fabs( total[axis] ) ) return 2;
	return axis;
}

/*
=============
AllocateLightmapForTriangleSoup
=============
*/
void AllocateLightmapForTriangleSoup( mapDrawSurface_t *ds ) {
	vec3_t mins, maxs, bounds[2], size, scale;
	mapDrawSurface_t *group;
	drawVert_t *verts;
	int i, sAxis, tAxis, sampleSize, sampleSizeS, sampleSizeT;
	int w, h, x, y, lightmapSAxis, lightmapTAxis, temp;
	float t;
	qboolean rotated;
	sampleSize = SurfaceLightmapSampleSize( ds );
	ClearBounds( bounds[0], bounds[1] );
	group = ds;
	do {
		for ( i = 0; i < group->numVerts; i++ ) {
			AddPointToBounds( group->verts[i].xyz, bounds[0], bounds[1] );
		}
		group = group->nextOnLightmapGroup;
	} while ( group != ds );
	sAxis = LightmapAxisForSurface( ds );
	VectorCopy( bounds[0], mins );
	VectorCopy( bounds[1], maxs );
	RoundLightmapBounds( sampleSize, mins, maxs, size );
	if ( sAxis == 0 ) { sAxis = 1; tAxis = 2; } else if (sAxis == 1) { sAxis = 0; tAxis = 2; } else { sAxis = 0; tAxis = 1; }
	if ( size[sAxis] > LIGHTMAP_BLOCK_SIZE ) {
		mins[sAxis] = bounds[0][sAxis];
		maxs[sAxis] = bounds[1][sAxis];
		sampleSizeS = FitLightmapCoordinate( LIGHTMAP_BLOCK_SIZE, sampleSize,
			&mins[sAxis], &maxs[sAxis], &size[sAxis] );
	}
	else sampleSizeS = sampleSize;
	if ( size[tAxis] > LIGHTMAP_BLOCK_SIZE ) {
		mins[tAxis] = bounds[0][tAxis];
		maxs[tAxis] = bounds[1][tAxis];
		sampleSizeT = FitLightmapCoordinate( LIGHTMAP_BLOCK_SIZE, sampleSize,
			&mins[tAxis], &maxs[tAxis], &size[tAxis] );
	}
	else sampleSizeT = sampleSize;
	w = size[sAxis]; h = size[tAxis];
	c_exactLightmap += w * h;
	scale[sAxis] = 1.0f / sampleSizeS;
	scale[tAxis] = 1.0f / sampleSizeT;
	if ( !AllocLightmapRectangle( w, h, &ds->lightmapNum, &x, &y, &rotated ) ) {
		Error( "Entity %i, brush %i: Lightmap allocation failed", ds->mapBrush->entitynum, ds->mapBrush->brushnum );
	}
	group = ds;
	do {
		group->lightmapWidth = w; group->lightmapHeight = h;
		group->lightmapX = x; group->lightmapY = y;
		group->lightmapNum = ds->lightmapNum;
		lightmapSAxis = 0; lightmapTAxis = 1;
		if ( rotated ) {
			group->lightmapWidth = h;
			group->lightmapHeight = w;
			lightmapSAxis = 1;
			lightmapTAxis = 0;
			temp = x; x = y; y = temp;
		}
		verts = group->verts;
		for ( i = 0; i < group->numVerts; i++ ) {
			t = ( verts[i].xyz[tAxis] - mins[tAxis] ) * scale[tAxis] + y;
			verts[i].lightmap[lightmapSAxis] = (( verts[i].xyz[sAxis] - mins[sAxis] ) * scale[sAxis] + x + 0.5f) / LIGHTMAP_BLOCK_SIZE;
			verts[i].lightmap[lightmapTAxis] = (t + 0.5f) / LIGHTMAP_BLOCK_SIZE;
		}
		group = group->nextOnLightmapGroup;
	} while ( group != ds );
}

/*
=============
ProjectSurfaceLightmap
=============
*/
static void ProjectSurfaceLightmap( vec3_t normal, mapDrawSurface_t *ds, int sampleSize, mesh_t *freeMesh ) {
	vec3_t mins, maxs, size, delta, absNormal, vecs[2];
	drawVert_t *verts;
	int i, w, h, x, y, axis, sAxis, tAxis;
	qboolean rotated;
	float scale, sampleScale;
	double s;
	LightmapSurfaceBounds( ds, mins, maxs, size );
	absNormal[0] = fabs( normal[0] );
	absNormal[1] = fabs( normal[1] );
	absNormal[2] = fabs( normal[2] );
	memset( vecs, 0, sizeof( vecs ) );
	if ( absNormal[0] >= absNormal[1] && absNormal[0] >= absNormal[2] ) {
		w = size[1]; h = size[2]; axis = 0;
		sampleScale = sampleSize;
		vecs[0][1] = 1.0f / sampleScale;
		vecs[1][2] = vecs[0][1];
	}
	else if ( absNormal[1] >= absNormal[0] && absNormal[1] >= absNormal[2] ) {
		w = size[0]; h = size[2]; axis = 1;
		sampleScale = sampleSize;
		vecs[0][0] = 1.0f / sampleScale;
		vecs[1][2] = vecs[0][0];
	}
	else {
		w = size[0]; h = size[1]; axis = 2;
		sampleScale = sampleSize;
		vecs[0][0] = 1.0f / sampleScale;
		vecs[1][1] = vecs[0][0];
	}
	if ( !normal[axis] ) Error( "Chose a 0 valued axis" );
	if ( w > LIGHTMAP_BLOCK_SIZE ) {
		scale = 1.0f / (float) ceil( ( sampleSize * LIGHTMAP_BLOCK_SIZE ) / w );
		VectorScale( vecs[0], ( sampleScale * scale ), vecs[0] );
		w = ceil( w * sampleSize * scale );
	}
	if ( h > LIGHTMAP_BLOCK_SIZE ) {
		scale = 1.0f / (float) ceil( ( sampleSize * LIGHTMAP_BLOCK_SIZE ) / h );
		VectorScale( vecs[1], ( sampleScale * scale ), vecs[1] );
		h = ceil( h * sampleSize * scale );
	}
	c_exactLightmap += w * h;
	if ( !AllocLightmapRectangle( w, h, &ds->lightmapNum, &x, &y, &rotated ) ) {
		Error( "Entity %i, brush %i: Lightmap allocation failed", ds->mapBrush->entitynum, ds->mapBrush->brushnum );
	}
	ds->lightmapWidth = w; ds->lightmapHeight = h;
	ds->lightmapX = x; ds->lightmapY = y;
	sAxis = 0; tAxis = 1;
	if ( rotated ) {
		sAxis = 1; tAxis = 0;
		ds->lightmapWidth = h; ds->lightmapHeight = w;
	}
	verts = ds->verts;
	for ( i = 0; i < ds->numVerts; i++ ) {
		VectorSubtract( verts[i].xyz, mins, delta );
		s = DotProduct( delta, vecs[sAxis] ) + x + 0.5f;
		verts[i].lightmap[0] = (float)s / LIGHTMAP_BLOCK_SIZE;
		verts[i].lightmap[1] = ( DotProduct( delta, vecs[tAxis] ) + y + 0.5f ) / LIGHTMAP_BLOCK_SIZE;
	}
}

/*
===================
AllocateLightmapForPatch
===================
*/
void AllocateLightmapForPatch( mapDrawSurface_t *ds ) {
	int i, j, k, w, h, sampleSize, sAxis, tAxis;
	float s, t;
	/* anonymous structs as in the original; plain locals behave the same */
	struct { int row, column; } lightmapOrigin;
	qboolean rotated;
	drawVert_t *verts;
	struct { mesh_t mesh; vec4_t plane; } projection;
	mesh_t *subdivided, *linear, *lightmapMesh;
	int originalSize[2][MAX_EXPANDED_AXIS];
	int subdivisionSize[2][MAX_EXPANDED_AXIS];

	verts = ds->verts;
	if ( ds->isTerrain ) return;
	projection.mesh.width = ds->patchWidth;
	projection.mesh.height = ds->patchHeight;
	projection.mesh.verts = verts;
	subdivided = SubdivideMesh( projection.mesh, 4, 999, originalSize[0], originalSize[1] );
	PutMeshOnCurve( *subdivided );
	linear = RemoveLinearMeshColumnsRows( subdivided, originalSize[0], originalSize[1] );
	FreeMesh( subdivided );
	sampleSize = SurfaceLightmapSampleSize( ds );
	lightmapMesh = SubdivideMeshQuads( linear, sampleSize, MAX_EXPANDED_AXIS,
		subdivisionSize[1], subdivisionSize[0], originalSize[0], originalSize[1] );
	if ( GuessMeshPlane( lightmapMesh, projection.plane ) ) {
		ProjectSurfaceLightmap( projection.plane, ds, sampleSize, lightmapMesh );
		return;
	}
	w = lightmapMesh->width;
	h = lightmapMesh->height;
	FreeMesh( lightmapMesh );
	c_exactLightmap += w * h;
	if ( !AllocLightmapRectangle( w, h, &ds->lightmapNum, &lightmapOrigin.column, &lightmapOrigin.row, &rotated ) ) {
		Error( "Entity %i, brush %i: Lightmap allocation failed",
			ds->mapBrush->entitynum, ds->mapBrush->brushnum );
	}
	ds->lightmapWidth = w;
	ds->lightmapHeight = h;
	ds->lightmapX = lightmapOrigin.column;
	ds->lightmapY = lightmapOrigin.row;
	sAxis = 0;
	tAxis = 1;
	if ( rotated ) {
		sAxis = 1;
		ds->lightmapWidth = h;
		ds->lightmapHeight = w;
		tAxis = 0;
		k = lightmapOrigin.column;
		lightmapOrigin.column = lightmapOrigin.row;
		lightmapOrigin.row = k;
	}
	for ( i = 0; i < ds->patchWidth; i++ ) {
		for ( k = 0; k < w; k++ ) {
			if ( originalSize[0][k] >= i ) break;
		}
		if ( k >= w ) k = w - 1;
		s = lightmapOrigin.column + k;
		for ( j = 0; j < ds->patchHeight; j++ ) {
			for ( k = 0; k < h; k++ ) {
				if ( originalSize[1][k] >= j ) break;
			}
			if ( k >= h ) k = h - 1;
			t = lightmapOrigin.row + k;
			verts[i + j * ds->patchWidth].lightmap[sAxis] =
				( s + 0.5f ) * ( 1.0f / LIGHTMAP_BLOCK_SIZE );
			verts[i + j * ds->patchWidth].lightmap[tAxis] =
				( t + 0.5f ) * ( 1.0f / LIGHTMAP_BLOCK_SIZE );
		}
	}
}

/* Allocate one projected rectangle for a circular compatible surface group. */
/*
=============
AllocateSharedLightmap
=============
*/
void AllocateSharedLightmap( mapDrawSurface_t *ds ) {
	vec3_t mins, maxs, size, delta, origin, normalized;
	vec3_t vecs[2], scaled;
	mapDrawSurface_t *group;
	plane_t *plane;
	int i, j, w, h, sampleSize, axis, x, y, sAxis, tAxis;
	qboolean rotated;
	int sampleExtent;
	float sampleScale, scale, d, len;

	sampleSize = SurfaceLightmapSampleSize( ds );
	LightmapGroupBounds( ds, mins, maxs, size );
	memset( vecs, 0, sizeof( vecs ) );
	axis = ds->lightmapNum + 4;
	if ( axis == 0 ) {
		w = size[1];
		h = size[2];
		sampleScale = sampleSize;
		scale = 1.0f / sampleScale;
		vecs[0][1] = scale;
		vecs[1][2] = scale;
	}
	else {
		if ( axis == 1 ) {
			w = size[0];
			h = size[2];
			sampleScale = sampleSize;
			scale = 1.0f / sampleScale;
			vecs[0][0] = scale;
			vecs[1][2] = scale;
		}
		else {
			w = size[0];
			h = size[1];
			sampleScale = sampleSize;
			scale = 1.0f / sampleScale;
			vecs[0][0] = scale;
			vecs[1][1] = scale;
		}
	}
	if ( w > LIGHTMAP_BLOCK_SIZE ) {
		sampleExtent = sampleSize * LIGHTMAP_BLOCK_SIZE;
		scale = 1.0f / (float) ceil( sampleExtent / w );
		VectorScale( vecs[0], ( sampleScale * scale ), vecs[0] );
		w = ceil( w * sampleSize * scale );
	}
	if ( h > LIGHTMAP_BLOCK_SIZE ) {
		scale = 1.0f / (float) ceil( ( sampleSize * LIGHTMAP_BLOCK_SIZE ) / h );
		VectorScale( vecs[1], ( sampleScale * scale ), vecs[1] );
		h = ceil( h * sampleSize * scale );
	}
	c_exactLightmap += w * h;
	if ( !AllocLightmapRectangle( w, h, &ds->lightmapNum, &x, &y, &rotated ) ) {
		Error( "Entity %i, brush %i: Lightmap allocation failed",
			ds->mapBrush->entitynum, ds->mapBrush->brushnum );
	}
	group = ds;
	do {
		group->lightmapNum = ds->lightmapNum;
		group->lightmapWidth = w;
		group->lightmapHeight = h;
		group->lightmapX = x;
		group->lightmapY = y;
		sAxis = 0;
		tAxis = 1;
		if ( rotated ) {
			sAxis = 1;
			tAxis = 0;
			group->lightmapWidth = h;
			group->lightmapHeight = w;
		}
		for ( i = 0; i < group->numVerts; i++ ) {
			VectorSubtract( group->verts[i].xyz, mins, delta );
			group->verts[i].lightmap[0] =
				( DotProduct( delta, vecs[sAxis] ) + x + 0.5f ) / LIGHTMAP_BLOCK_SIZE;
			group->verts[i].lightmap[1] =
				( DotProduct( delta, vecs[tAxis] ) + y + 0.5f ) / LIGHTMAP_BLOCK_SIZE;
		}
		if ( !ds->triangleSoup ) {
			if ( group->patch ) plane = &mapplanes[group->lightmapPlaneNum];
			else plane = &mapplanes[group->side->planenum];
			d = mins[0] * plane->normal[0] + mins[2] * plane->normal[2] + mins[1] * plane->normal[1];
			d -= plane->dist;
			d /= plane->normal[axis];
			VectorCopy( mins, origin );
			origin[axis] -= d;
			VectorCopy( origin, group->lightmapOrigin );
			VectorCopy( vecs[sAxis], group->lightmapVecs[0] );
			VectorCopy( vecs[tAxis], group->lightmapVecs[1] );
			VectorCopy( plane->normal, group->lightmapVecs[2] );
			for ( j = 0; j < 2; j++ ) {
				len = VectorNormalize2( group->lightmapVecs[j], normalized );
				group->lightmapVecs[j][0] = scaled[0] = ( 1.0f / len ) * normalized[0];
				group->lightmapVecs[j][1] = scaled[1] = ( 1.0f / len ) * normalized[1];
				group->lightmapVecs[j][2] = scaled[2] = ( 1.0f / len ) * normalized[2];
				d = DotProduct( scaled, plane->normal ) / plane->normal[axis];
				group->lightmapVecs[j][axis] -= d;
			}
		}
		group = group->nextOnLightmapGroup;
	} while ( group != ds );
}


/*
===================
AllocateLightmapForSurface
===================
*/
void AllocateLightmapForSurface( mapDrawSurface_t *ds ) {
	vec3_t mins, maxs, size, delta, planeNormal, origin;
	vec3_t vecs[2];
	plane_t *plane;
	/* the original gathers the t axis components in a struct; kept so the build matches the original binary */
	struct { const float *z, *y, *x; } tProjection;
	drawVert_t *verts;
	int i, w, h, x, y, sampleSize, axis, sAxis, tAxis;
	qboolean rotated;
	float d, len, scale, sampleScale, s, t;
	if ( ds->patch ) { AllocateLightmapForPatch( ds ); return; }
	if ( ds->nextOnLightmapGroup != ds ) { AllocateSharedLightmap( ds ); return; }
	if ( ds->triangleSoup ) { AllocateLightmapForTriangleSoup( ds ); return; }
	sampleSize = SurfaceLightmapSampleSize( ds );
	plane = &mapplanes[ds->side->planenum];
	LightmapSurfaceBounds( ds, mins, maxs, size );
	memset( vecs, 0, sizeof( vecs ) );
	planeNormal[0] = fabs( plane->normal[0] );
	planeNormal[1] = fabs( plane->normal[1] );
	planeNormal[2] = fabs( plane->normal[2] );
	if ( planeNormal[0] >= planeNormal[1] && planeNormal[0] >= planeNormal[2] ) {
		w = size[1]; h = size[2]; axis = 0;
		sampleScale = sampleSize;
		vecs[0][1] = 1.0f / sampleScale;
		vecs[1][2] = vecs[0][1];
	}
	else if ( planeNormal[1] >= planeNormal[0] && planeNormal[1] >= planeNormal[2] ) {
		w = size[0]; h = size[2]; axis = 1;
		sampleScale = sampleSize;
		vecs[0][0] = 1.0f / sampleScale;
		vecs[1][2] = vecs[0][0];
	}
	else {
		w = size[0]; h = size[1]; axis = 2;
		sampleScale = sampleSize;
		vecs[0][0] = 1.0f / sampleScale;
		vecs[1][1] = vecs[0][0];
	}
	if ( !plane->normal[axis] ) Error( "Chose a 0 valued axis" );
	if ( w > LIGHTMAP_BLOCK_SIZE ) {
		scale = 1.0f / (float) ceil( ( sampleSize * LIGHTMAP_BLOCK_SIZE ) / w );
		VectorScale( vecs[0], (sampleScale * scale), vecs[0] );
		w = ceil( w * sampleSize * scale );
	}
	if ( h > LIGHTMAP_BLOCK_SIZE ) {
		scale = 1.0f / (float) ceil( ( sampleSize * LIGHTMAP_BLOCK_SIZE ) / h );
		VectorScale( vecs[1], (sampleScale * scale), vecs[1] );
		h = ceil( h * sampleSize * scale );
	}
	c_exactLightmap += w * h;
	if ( !AllocLightmapRectangle( w, h, &ds->lightmapNum, &x, &y, &rotated ) ) {
		Error( "Entity %i, brush %i: Lightmap allocation failed", ds->mapBrush->entitynum, ds->mapBrush->brushnum );
	}
	ds->lightmapWidth = w; ds->lightmapHeight = h;
	ds->lightmapX = x; ds->lightmapY = y;
	sAxis = 0; tAxis = 1;
	if ( rotated ) {
		sAxis = 1; tAxis = 0;
		ds->lightmapWidth = h; ds->lightmapHeight = w;
	}
	verts = ds->verts;
	for ( i = 0; i < ds->numVerts; i++ ) {
		tProjection.z = &vecs[tAxis][2];
		tProjection.y = &vecs[tAxis][1];
		tProjection.x = &vecs[tAxis][0];
		delta[0] = verts[i].xyz[0] - mins[0];
		delta[1] = verts[i].xyz[1] - mins[1];
		delta[2] = verts[i].xyz[2] - mins[2];
		s = DotProduct( delta, vecs[sAxis] ) + x + 0.5f;
		t = delta[0] * *tProjection.x;
		t += delta[2] * *tProjection.z;
		t += delta[1] * *tProjection.y;
		t += y;
		t += 0.5f;
		verts[i].lightmap[0] = s / LIGHTMAP_BLOCK_SIZE;
		verts[i].lightmap[1] = t / LIGHTMAP_BLOCK_SIZE;
	}
	d = ( DotProduct( mins, plane->normal ) - plane->dist ) / plane->normal[axis];
	VectorCopy( mins, origin );
	origin[axis] -= d;
	for ( i = 0; i < 2; i++ ) {
		vec3_t normalized;
		len = VectorNormalize2( vecs[i], normalized );
		VectorScale( normalized, ( 1.0f / len ), vecs[i] );
		d = DotProduct( plane->normal, vecs[i] ) / plane->normal[axis];
		vecs[i][axis] -= d;
	}
	VectorCopy( origin, ds->lightmapOrigin );
	VectorCopy( vecs[sAxis], ds->lightmapVecs[0] );
	VectorCopy( vecs[tAxis], ds->lightmapVecs[1] );
	VectorCopy( plane->normal, ds->lightmapVecs[2] );
}

/*
=============
LightmapGroupSurfacesShareVertex
=============
*/
qboolean LightmapGroupSurfacesShareVertex( mapDrawSurface_t *a, mapDrawSurface_t *b ) {
	drawVert_t patchVertsA[1024], patchVertsB[1024];
	drawVert_t *vertsA, *vertsB;
	int countA, countB, i, j;

	if ( a->patch ) {
		vertsA = patchVertsA;
		countA = 0;
		for ( i = 0 ; i < a->patchHeight ; i += 2 ) {
			for ( j = 0 ; j < a->patchWidth ; j += 2 ) {
				patchVertsA[countA++] = a->verts[i * a->patchWidth + j];
			}
		}
	}
	else {
		vertsA = a->verts;
		countA = a->numVerts;
	}
	if ( b->patch ) {
		vertsB = patchVertsB;
		countB = 0;
		for ( i = 0 ; i < b->patchHeight ; i += 2 ) {
			for ( j = 0 ; j < b->patchWidth ; j += 2 ) {
				patchVertsB[countB++] = b->verts[i * b->patchWidth + j];
			}
		}
	}
	else {
		vertsB = b->verts;
		countB = b->numVerts;
	}
	for ( i = 0 ; i < countA ; i++ ) {
		for ( j = 0 ; j < countB ; j++ ) {
			if ( VectorCompareEpsilon( vertsA[i].xyz, vertsB[j].xyz ) ) {
				return qtrue;
			}
		}
	}
	return qfalse;
}



/*
=============
LightmapGroupsCompatible
=============
*/
static qboolean LightmapGroupsCompatible( mapDrawSurface_t *ds, mapDrawSurface_t *group ) {
	vec3_t groupMins, groupMaxs, dsMins, dsMaxs, size;
	int groupPlane, dsPlane;
	int sampleSize, groupSampleSize, i;
	float extent;
	if ( ds->lightmapNum != group->lightmapNum ) return qfalse;
	if ( !ds->triangleSoup ) {
		dsPlane = ds->patch ? ds->lightmapPlaneNum : ds->side->planenum;
		groupPlane = group->patch ? group->lightmapPlaneNum : group->side->planenum;
		if ( DotProduct( mapplanes[dsPlane].normal, mapplanes[groupPlane].normal ) < lightmapNormalDotThreshold ) return qfalse;
	}
	if ( SurfaceLightmapSampleSize( ds ) != SurfaceLightmapSampleSize( group ) ) return qfalse;
	if ( !LightmapGroupSurfacesShareVertex( ds, group ) ) return qfalse;
	sampleSize = SurfaceLightmapSampleSize( ds );
	LightmapGroupBounds( ds, dsMins, dsMaxs, size );
	LightmapGroupBounds( group, groupMins, groupMaxs, size );
	for ( i = 0; i < 3; i++ ) {
		extent = ( dsMaxs[i] > groupMaxs[i] ? dsMaxs[i] : groupMaxs[i] )
			- ( dsMins[i] < groupMins[i] ? dsMins[i] : groupMins[i] );
		if ( extent > ( LIGHTMAP_BLOCK_SIZE - 1 ) * sampleSize ) return qfalse;
	}
	return qtrue;
}

/*
=============
SetupLightmapGroupSurfaceBounds
=============
*/
static void SetupLightmapGroupSurfaceBounds( mapDrawSurface_t *ds ) {
	int i;
	const float epsilon = EQUAL_EPSILON;
	ClearBounds( ds->mins, ds->maxs );
	for ( i = 0; i < ds->numVerts; i++ ) AddPointToBounds( ds->verts[i].xyz, ds->mins, ds->maxs );
	ds->mins[0] -= epsilon;
	ds->mins[1] -= epsilon;
	ds->mins[2] -= epsilon;
	ds->maxs[0] += epsilon;
	ds->maxs[1] += epsilon;
	ds->maxs[2] += epsilon;
}

/*
=============
AddSurfaceToLightmapGroups
=============
*/
static void AddSurfaceToLightmapGroups( mapDrawSurface_t *ds, entity_t *e ) {
	mapDrawSurface_t *group, *tail;
	int i, surfaceNum;
	SetupLightmapGroupSurfaceBounds( ds );
	i = e->firstDrawSurf;
	surfaceNum = ds - mapDrawSurfs;
	for ( ; i < surfaceNum; i++ ) {
		group = &mapDrawSurfs[i];
		if ( !group->numVerts || group->miscModel ) continue;
		if ( group->patch && ( group->isTerrain || !group->planar ) ) continue;
		if ( ds->triangleSoup != group->triangleSoup ) continue;
		if ( ds->mins[0] >= group->maxs[0] || ds->maxs[0] <= group->mins[0]
			|| ds->mins[1] >= group->maxs[1] || ds->maxs[1] <= group->mins[1]
			|| ds->mins[2] >= group->maxs[2] || ds->maxs[2] <= group->mins[2] ) continue;
		if ( !LightmapGroupsCompatible( ds, group ) ) continue;
		tail = ds->nextOnLightmapGroup;
		while ( tail->nextOnLightmapGroup != ds ) {
			tail = tail->nextOnLightmapGroup;
		}
		tail->nextOnLightmapGroup = group;
		tail = group;
		while ( tail->nextOnLightmapGroup != group ) {
			tail = tail->nextOnLightmapGroup;
		}
		tail->nextOnLightmapGroup = ds;
	}
}

/*
=============
BuildLightmapGroups
=============
*/
static void BuildLightmapGroups( entity_t *e ) {
	int i, axis, groupNum;
	mapDrawSurface_t *ds, *groupSurface;
	plane_t *plane;
	for ( i = e->firstDrawSurf; i < numMapDrawSurfs; i++ ) {
		ds = &mapDrawSurfs[i];
		ds->nextOnLightmapGroup = ds;
		ds->lightmapGroupNum = -1;
		if ( !ds->numVerts || ds->miscModel ) continue;
		if ( ds->shaderInfo->surfaceFlags & SURF_NOLIGHTMAP ) continue;
		if ( ds->patch && ( ds->isTerrain || !ds->planar ) ) continue;
		if ( ds->triangleSoup ) axis = LightmapAxisForSurface( ds );
		else {
			plane = &mapplanes[ds->patch ? ds->lightmapPlaneNum : ds->side->planenum];
			axis = 0;
			if ( fabs( plane->normal[0] ) < fabs( plane->normal[1] ) ) axis = 1;
			if ( fabs( plane->normal[axis] ) < fabs( plane->normal[2] ) ) axis = 2;
		}
		ds->lightmapNum = axis - 4;
		AddSurfaceToLightmapGroups( ds, e );
	}
	groupNum = 0;
	for ( i = e->firstDrawSurf; i < numMapDrawSurfs; i++ ) {
		ds = &mapDrawSurfs[i];
		if ( !ds->numVerts || ds->miscModel || ds->lightmapGroupNum >= 0 ) continue;
		ds->lightmapGroupNum = groupNum++;
		if ( !ds->patch ) {
			for ( groupSurface = ds->nextOnLightmapGroup; groupSurface != ds; groupSurface = groupSurface->nextOnLightmapGroup ) {
				groupSurface->lightmapGroupNum = ds->lightmapGroupNum;
			}
		}
	}
}

lightmapMaterial_t materialGroupSortArray[MAX_MAP_SHADERS];
int numSortShader;
int c_exactLightmap;

/*
=============
AddSurfaceToOverlapGrid
=============
*/
static void AddSurfaceToOverlapGrid( int column, int row ) {
	int x, y, i;
	int endX, endY;
	lightmapGroupCell_t *cell;
	float pos[2];

	pos[0] = column * 0.5f + lightmapGridMins[0];
	pos[1] = row * 0.5f + lightmapGridMins[1];
	column = floor( pos[0] - 0.5f );
	row = floor( pos[1] - 0.5f );
	endX = column + 1;
	endY = row + 1;
	for ( y = row; y <= endY; y++ ) {
		for ( x = column; x <= endX; x++ ) {
			cell = &lightmapGroupGrid[y * lightmapGridWidth + x];
			for ( i = 0; i < cell->count; i++ ) {
				if ( cell->groups[i] == lightmapCellsGroup ) break;
			}
			if ( i == cell->count ) {
				if ( (unsigned int) cell->count < 2 ) cell->groups[cell->count] = lightmapCellsGroup;
				cell->count++;
			}
		}
	}
}

/*
=============
ScanLightmapSurface
=============
*/
static void ScanLightmapSurface( vec3_t origin, mapDrawSurface_t *ds, int group,
	int xAxis, int yAxis, int sampleSize ) {
	float polygons[4 * LIGHTMAP_POLYGON_WORK_POINTS * 2];
	float x, y;
	float mins[2], maxs[2];
	float scale;
	int i, width, height;

	scale = 1.0f / sampleSize;
	mins[0] = FLT_MAX;
	mins[1] = FLT_MAX;
	maxs[0] = -FLT_MAX;
	maxs[1] = -FLT_MAX;
	for ( i = 0; i < ds->numVerts; i++ ) {
		x = ( ds->verts[i].xyz[xAxis] - origin[xAxis] ) * scale;
		polygons[i * 2] = x;
		if ( mins[0] > x ) mins[0] = x;
		if ( maxs[0] < x ) maxs[0] = x;
		y = ( ds->verts[i].xyz[yAxis] - origin[yAxis] ) * scale;
		polygons[i * 2 + 1] = y;
		if ( mins[1] > y ) mins[1] = y;
		if ( maxs[1] < y ) maxs[1] = y;
	}
	lightmapCellsGroup = group;
	lightmapGridMins[0] = (float) floor( mins[0] + mins[0] ) * 0.5f;
	lightmapGridMins[1] = (float) floor( mins[1] + mins[1] ) * 0.5f;
	width = ceil( ( maxs[0] - lightmapGridMins[0] ) * 2 );
	height = ceil( ( maxs[1] - lightmapGridMins[1] ) * 2 );
	ScanConvertLightmapPolygon( ds->numVerts, polygons, width, height,
		lightmapGridMins[0], lightmapGridMins[1], 0.5f, 0.5f, AddSurfaceToOverlapGrid );
}

/*
=============
LightmapSurfaceBoundsSeparate
=============
*/
static qboolean LightmapSurfaceBoundsSeparate( mapDrawSurface_t *a,
	mapDrawSurface_t *b, int sAxis, int tAxis ) {
	vec3_t minsA, maxsA, size, minsB, maxsB;
	LightmapSurfaceBounds( a, minsA, maxsA, size );
	LightmapSurfaceBounds( b, minsB, maxsB, size );
	return minsA[sAxis] > maxsB[sAxis] ||
		minsB[sAxis] > maxsA[sAxis] ||
		minsA[tAxis] > maxsB[tAxis] ||
		minsB[tAxis] > maxsA[tAxis];
}

/*
=============
LightmapGroupsShareVertex
=============
*/
static qboolean LightmapGroupsShareVertex( mapDrawSurface_t *a, mapDrawSurface_t *b ) {
	mapDrawSurface_t *first = a, *second;
	do {
		second = b;
		do {
			if ( LightmapGroupSurfacesShareVertex( first, second ) ) return qtrue;
			second = second->nextOnLightmapGroup;
		} while ( second != b );
		first = first->nextOnLightmapGroup;
	} while ( first != a );
	return qfalse;
}

/*
=============
MergeCompatibleLightmapGroups
=============
*/
static qboolean MergeCompatibleLightmapGroups( mapDrawSurface_t *a,
	mapDrawSurface_t *b, byte *compatible, int rowBytes ) {
	mapDrawSurface_t *first = b, *second, *lastFirst, *lastSecond;
	int id;
	do {
		id = first->lightmapGroupNum;
		second = a;
		do {
			if ( !( compatible[second->lightmapGroupNum * rowBytes + ( id >> 3 )] & ( 1 << ( id & 7 ) ) ) ) {
				return qfalse;
			}
			lastSecond = second;
			second = second->nextOnLightmapGroup;
		} while ( second != a );
		lastFirst = first;
		first = first->nextOnLightmapGroup;
	} while ( first != b );
	if ( !LightmapGroupsShareVertex( b, a ) ) return qfalse;
	lastFirst->nextOnLightmapGroup = second;
	lastSecond->nextOnLightmapGroup = first;
	return qtrue;
}

/*
=============
MergeOneCompatibleLightmapGroup
=============
*/
static qboolean MergeOneCompatibleLightmapGroup( int *count, mapDrawSurface_t **groups,
	byte *compatible, int rowBytes ) {
	int first, second;
	for ( first = 0; first < *count; first++ ) {
		for ( second = first + 1; second < *count; second++ ) {
			if ( MergeCompatibleLightmapGroups( groups[second], groups[first], compatible, rowBytes ) ) {
				(*count)--;
				groups[second] = groups[*count];
				return qtrue;
			}
		}
	}
	return qfalse;
}

/*
=============
AttachIsolatedLightmapGroups
=============
*/
static void AttachIsolatedLightmapGroups( mapDrawSurface_t **isolated, int numIsolated,
	mapDrawSurface_t **groups, int numGroups, int xAxis, int yAxis ) {
	int attached = 0, i, best;
	float bestCost;
	float cost, delta;
	vec3_t mins, maxs, groupMins, groupMaxs, size;
	mapDrawSurface_t *tail, *head;
	if ( numIsolated <= 0 ) return;

	while ( attached < numIsolated ) {
		LightmapSurfaceBounds( isolated[attached], mins, maxs, size );
		best = -1;
		bestCost = FLT_MAX;
		for ( i = 0; i < numGroups; i++ ) {
			if ( !LightmapGroupsShareVertex( isolated[attached], groups[i] ) ) continue;
			cost = 0.0f;
			LightmapGroupBounds( groups[i], groupMins, groupMaxs, size );
			delta = maxs[xAxis] - groupMaxs[xAxis];
			if ( delta > 0.0f ) cost = delta;
			delta = groupMins[xAxis] - mins[xAxis];
			if ( delta > 0.0f ) cost += delta;
			delta = maxs[yAxis] - groupMaxs[yAxis];
			if ( delta > 0.0f ) cost += delta;
			delta = groupMins[yAxis] - mins[yAxis];
			if ( delta > 0.0f ) cost += delta;
			if ( bestCost > cost ) {
				bestCost = cost;
				best = i;
				if ( cost == 0.0f ) break;
			}
		}
		if ( best < 0 ) {
			head = isolated[attached];
			memmove( &isolated[attached], &isolated[attached + 1], ( numIsolated - attached - 1 ) * sizeof( isolated[0] ) );
			isolated[numIsolated - 1] = head;
			continue;
		}
		tail = groups[best];
		while ( tail->nextOnLightmapGroup != groups[best] ) {
			tail = tail->nextOnLightmapGroup;
		}
		tail->nextOnLightmapGroup = isolated[attached];
		(isolated[attached])->nextOnLightmapGroup = groups[best];
		attached++;
	}
}

/*
=============
MergeCompatibleSurfaceBatch
=============
*/
static void MergeCompatibleSurfaceBatch( mapDrawSurface_t **surfaces, int count,
	int xAxis, int yAxis, byte *compatible, int rowBytes ) {
	mapDrawSurface_t *groups[LIGHTMAP_GROUP_BATCH_LIMIT];
	mapDrawSurface_t *isolated[LIGHTMAP_GROUP_BATCH_LIMIT];
	int i, j, hasNeighbor, numGroups, numIsolated;

	for ( i = 0; i < count; i++ ) {
		hasNeighbor = 0;
		for ( j = 0; j < count; j++ ) {
			if ( hasNeighbor ) break;
			if ( i != j ) hasNeighbor = LightmapGroupSurfacesShareVertex( surfaces[i], surfaces[j] );
		}
	}
	numGroups = 0;
	numIsolated = 0;
	for ( i = 0; i < count; i++ ) {
		surfaces[i]->nextOnLightmapGroup = surfaces[i];
		surfaces[i]->lightmapGroupNum = i;
		for ( j = 0; j < count; j++ ) {
			if ( !( compatible[j * rowBytes + (i >> 3)] & ( 1 << (i & 7) ) ) ) goto incompatible;
		}
		isolated[numIsolated++] = surfaces[i];
		continue;
		incompatible:
		groups[numGroups++] = surfaces[i];
	}
	while ( MergeOneCompatibleLightmapGroup( &numGroups, groups, compatible, rowBytes ) ) {}
	AttachIsolatedLightmapGroups( isolated, numIsolated, groups, numGroups, xAxis, yAxis );
}

/*
=============
BuildLightmapCompatibilityGrid
=============
*/
static void BuildLightmapCompatibilityGrid( mapDrawSurface_t **surfaces, int count,
	int xAxis, int yAxis, byte *compatible, int rowBytes ) {
	byte overlap[LIGHTMAP_GROUP_BATCH_LIMIT * LIGHTMAP_GROUP_BATCH_LIMIT / 8];
	vec3_t mins, maxs, size;
	int sampleSize, cells, i, a, b, j;
	lightmapGroupCell_t *cell;

	memset( overlap, 0, count * rowBytes );
	sampleSize = SurfaceLightmapSampleSize( surfaces[0] );
	LightmapGroupBounds( surfaces[0], mins, maxs, size );
	lightmapGridWidth = size[xAxis];
	bspLightmapGridHeight = size[yAxis];
	cells = lightmapGridWidth * bspLightmapGridHeight;
	lightmapGroupGrid = malloc( cells * sizeof( *lightmapGroupGrid ) );
	memset( lightmapGroupGrid, 0, cells * sizeof( *lightmapGroupGrid ) );
	for ( i = 0; i < count; i++ ) {
		ScanLightmapSurface( mins, surfaces[i], i, xAxis, yAxis, sampleSize );
	}
	cells = lightmapGridWidth * bspLightmapGridHeight;
	for ( i = 0; i < cells; i++ ) {
		cell = &lightmapGroupGrid[i];
		if ( cell->count != 2 ) continue;
		a = cell->groups[0];
		b = cell->groups[1];
		if ( !( compatible[a * rowBytes + ( b >> 3 )] & ( 1 << ( b & 7 ) ) ) ) {
			overlap[a * rowBytes + ( b >> 3 )] |= ( 1 << ( b & 7 ) );
			overlap[b * rowBytes + ( a >> 3 )] |= ( 1 << ( a & 7 ) );
		}
	}
	free( lightmapGroupGrid );
	for ( i = 0; i < count; i++ ) {
		for ( j = i + 1; j < count; j++ ) {
			if ( !( overlap[i * rowBytes + ( j >> 3 )] & ( 1 << ( j & 7 ) ) ) ) {
				compatible[i * rowBytes + ( j >> 3 )] |= ( 1 << ( j & 7 ) );
				compatible[j * rowBytes + ( i >> 3 )] |= ( 1 << ( i & 7 ) );
			}
		}
	}
	for ( i = 0; i < count; i++ ) {
		for ( j = i + 1; j < count; j++ ) {
			if ( !( compatible[i * rowBytes + ( j >> 3 )] & ( 1 << ( j & 7 ) ) ) ) {
				MergeCompatibleSurfaceBatch( surfaces, count, xAxis, yAxis, compatible, rowBytes );
				return;
			}
		}
	}
}

/*
=============
SplitInvalidLightmapGroup
=============
*/
void SplitInvalidLightmapGroup( mapDrawSurface_t **surfaces, int count ) {
	byte compatible[LIGHTMAP_GROUP_BATCH_LIMIT * LIGHTMAP_GROUP_BATCH_LIMIT / 8];
	plane_t *plane;
	qboolean invalid;
	int dominantAxis, sAxis, tAxis, rowBytes, i, j;
	if ( surfaces[0]->triangleSoup ) return;
	if ( surfaces[0]->patch ) plane = &mapplanes[surfaces[0]->lightmapPlaneNum];
	else plane = &mapplanes[surfaces[0]->side->planenum];
	dominantAxis = 0;
	if ( fabs( plane->normal[0] ) < fabs( plane->normal[1] ) ) dominantAxis = 1;
	if ( fabs( plane->normal[dominantAxis] ) < fabs( plane->normal[2] ) ) dominantAxis = 2;
	sAxis = ( ~dominantAxis ) & 1;
	tAxis = ( ~dominantAxis ) & 2;
	rowBytes = ( count + 7 ) >> 3;
	memset( compatible, 0, count * rowBytes );
	invalid = qfalse;
	for ( i = 0; i < count; i++ ) {
		compatible[i * rowBytes + ( i >> 3 )] |= 1 << ( i & 7 );
		for ( j = i + 1; j < count; j++ ) {
			if ( LightmapGroupSurfacesShareVertex( surfaces[i], surfaces[j] ) ) {
				compatible[i * rowBytes + ( j >> 3 )] |= 1 << ( j & 7 );
				compatible[j * rowBytes + ( i >> 3 )] |= 1 << ( i & 7 );
			}
			else if ( LightmapSurfaceBoundsSeparate( surfaces[i], surfaces[j], sAxis, tAxis ) ) {
				compatible[i * rowBytes + ( j >> 3 )] |= 1 << ( j & 7 );
				compatible[j * rowBytes + ( i >> 3 )] |= 1 << ( i & 7 );
			}
			else invalid = qtrue;
		}
	}
	if ( invalid ) BuildLightmapCompatibilityGrid( surfaces, count, sAxis, tAxis, compatible, rowBytes );
}

qboolean GuessMeshPlane( mesh_t *mesh, vec4_t plane );

/*
=============
SplitInvalidLightmapGroups
=============
*/
static void SplitInvalidLightmapGroups( entity_t *entity ) {
	mapDrawSurface_t *surfaces[LIGHTMAP_GROUP_BATCH_LIMIT];
	mapDrawSurface_t *ds, *groupSurface;
	int i, count;
	for ( i = entity->firstDrawSurf; i < numMapDrawSurfs; i++ ) {
		mapDrawSurfs[i].lightmapGroupVisited = qfalse;
	}
	for ( i = entity->firstDrawSurf; i < numMapDrawSurfs; i++ ) {
		ds = &mapDrawSurfs[i];
		groupSurface = ds;
		if ( ds->lightmapGroupVisited ) continue;
		count = 0;
		do {
			surfaces[count] = groupSurface;
			groupSurface->lightmapGroupVisited = qtrue;
			groupSurface = groupSurface->nextOnLightmapGroup;
			count++;
		} while ( groupSurface != surfaces[0] && count < sizeof( surfaces ) / sizeof( surfaces[0] ) );
		if ( count >= 3 ) {
			if ( groupSurface != surfaces[0] ) {
				qprintf( "WARNING: SplitInvalidLightmapGroups: MAX_DRAWSURFS_PER_GROUP (%i) exceeded\n", LIGHTMAP_GROUP_BATCH_LIMIT );
			}
			else SplitInvalidLightmapGroup( surfaces, count );
		}
	}
}

/*
=============
FinalizeLightmapVectors
=============
*/
static void FinalizeLightmapVectors( entity_t *e ) {
	int i, axis;
	mapDrawSurface_t *ds;
	plane_t *plane;
	vec3_t mins, maxs, size;

	for ( i = e->firstDrawSurf; i < numMapDrawSurfs; i++ ) {
		ds = &mapDrawSurfs[i];
		if ( !ds->numVerts || ds->miscModel ) continue;
		if ( ds->patch && !ds->planar ) {
			ComputePatchLightmapSize( ds, &ds->lightmapWidth, &ds->lightmapHeight );
			continue;
		}
		if ( ds->nextOnLightmapGroup != ds ) {
			LightmapGroupBounds( ds, mins, maxs, size );
			axis = ds->lightmapNum + 4;
		}
		else {
			LightmapSurfaceBounds( ds, mins, maxs, size );
			if ( ds->triangleSoup ) {
				axis = LightmapAxisForSurface( ds );
			}
			else {
				plane = &mapplanes[ds->lightmapPlaneNum];
				axis = 0;
				if ( fabs( plane->normal[0] ) < fabs( plane->normal[1] ) ) axis = 1;
				if ( fabs( plane->normal[axis] ) < fabs( plane->normal[2] ) ) axis = 2;
			}
		}
		if ( axis == 0 ) {
			ds->lightmapWidth = size[1];
			ds->lightmapHeight = size[2];
		}
		else if ( axis == 1 ) {
			ds->lightmapWidth = size[0];
			ds->lightmapHeight = size[2];
		}
		else {
			ds->lightmapWidth = size[0];
			ds->lightmapHeight = size[1];
		}
	}
}

/*
=============
CompareLightmapSurfaces
=============
*/
static int CompareLightmapSurfaces( mapDrawSurface_t *a, mapDrawSurface_t *b ) {
	return b->lightmapWidth * b->lightmapHeight - a->lightmapWidth * a->lightmapHeight;
}

/*
=============
MergeSortLightmapSurfaces
=============
*/
mapDrawSurface_t *MergeSortLightmapSurfaces( mapDrawSurface_t *head, int count ) {
	int leftCount, rightCount, i;
	mapDrawSurface_t *left, *right, *split;
	mapDrawSurface_t **tail;

	if ( count == 1 ) {
		return head;
	}
	leftCount = count / 2;
	split = head;
	for ( i = 0 ; i < leftCount ; i++ ) {
		split = split->nextOnShader;
	}
	rightCount = count - leftCount;
	left = MergeSortLightmapSurfaces( head, leftCount );
	right = MergeSortLightmapSurfaces( split, rightCount );
	tail = &head;
	while ( leftCount || rightCount ) {
		if ( !leftCount || ( rightCount && CompareLightmapSurfaces( left, right ) > 0 ) ) {
			*tail = right;
			right = right->nextOnShader;
			rightCount--;
		}
		else {
			*tail = left;
			left = left->nextOnShader;
			leftCount--;
		}
		tail = &( *tail )->nextOnShader;
	}
	*tail = NULL;
	return head;
}

/*
=============
CompareMaterialGroupsByArea
=============
*/
static int CompareMaterialGroupsByArea( const void *va, const void *vb ) {
	const lightmapMaterial_t *a = va, *b = vb;
	int difference = b->maxArea - a->maxArea;
	if ( !difference ) return b->numSurfaces - a->numSurfaces;
	return difference;
}

/*
===================
AllocateLightmaps
===================
*/
void AllocateLightmaps( entity_t *e ) {
	int i, j, area, lightmapNum, actualShaders, lastLightmapArea;
	lightmapMaterial_t *material;
	mapDrawSurface_t *ds;
	shaderInfo_t *si;
	lightmapBlock_t *block;

	qprintf( "--- AllocateLightmaps ---\n" );
	numSortShader = 0;
	for ( i = e->firstDrawSurf; i < numMapDrawSurfs; i++ ) {
		ds = &mapDrawSurfs[i];
		ds->lightmapNum = -1;
		ds->nextOnLightmapGroup = ds;
		if ( !ds->numVerts || ds->miscModel ) continue;
		if ( !ds->patch && !ds->triangleSoup ) {
			VectorCopy( mapplanes[ds->side->planenum].normal, ds->lightmapVecs[2] );
		}
		for ( j = 0; j < numSortShader; j++ ) {
			if ( ds->shaderInfo == materialGroupSortArray[j].surfaces->shaderInfo ) {
				ds->nextOnShader = materialGroupSortArray[j].surfaces;
				materialGroupSortArray[j].surfaces = ds;
				materialGroupSortArray[j].numSurfaces++;
				break;
			}
		}
		if ( j == numSortShader ) {
			if ( numSortShader >= MAX_MAP_SHADERS ) Error( "MAX_MAP_SHADERS" );
			materialGroupSortArray[j].surfaces = ds;
			materialGroupSortArray[j].numSurfaces = 1;
			materialGroupSortArray[j].lightmaps = NULL;
			numSortShader++;
		}
	}
	if ( e == entities ) printf( "%5i unique shaders\n", numSortShader );
	BuildLightmapGroups( e );
	SplitInvalidLightmapGroups( e );
	if ( !lightmapBlocks ) numLightmaps = 0;
	FinalizeLightmapVectors( e );
	for ( i = 0; i < numSortShader; i++ ) {
		materialGroupSortArray[i].surfaces = MergeSortLightmapSurfaces(
			materialGroupSortArray[i].surfaces, materialGroupSortArray[i].numSurfaces );
		materialGroupSortArray[i].maxArea = 0;
		for ( ds = materialGroupSortArray[i].surfaces; ds; ds = ds->nextOnShader ) {
			area = ds->lightmapWidth * ds->lightmapHeight;
			if ( materialGroupSortArray[i].maxArea < area ) materialGroupSortArray[i].maxArea = area;
		}
	}
	qsort( materialGroupSortArray, numSortShader, sizeof( materialGroupSortArray[0] ), CompareMaterialGroupsByArea );
	actualShaders = 0;
	for ( i = 0; i < numSortShader; i++ ) {
		material = &materialGroupSortArray[i];
		si = material->surfaces->shaderInfo;
		currentMaterialSet = material;
		for ( ds = material->surfaces; ds; ds = ds->nextOnShader ) {
			lightmapNum = ds->lightmapNum;
			if ( lightmapNum < 0 ) {
				if ( si->surfaceFlags & SURF_NOLIGHTMAP ) { ds->lightmapNum = -1; continue; }
				if ( si->surfaceFlags & SURF_POINTLIGHT ) { ds->lightmapNum = -3; continue; }
				AllocateLightmapForSurface( ds );
				lightmapNum = ds->lightmapNum;
			}
			AddLightmapToCurrentMaterial( lightmapNum );
		}
		actualShaders += CountAndFreeMaterialLightmaps();
	}
	if ( e == entities ) printf( "%5i actual shaders\n", actualShaders );
	lastLightmapArea = 0;
	for ( block = lightmapBlocks; block; block = block->next ) {
		if ( block->lightmapNum == numLightmaps - 1 ) lastLightmapArea += block->width * block->height;
	}
	if ( numLightmaps > 1 ) {
		lightmapEfficiency = ( c_exactLightmap + lastLightmapArea - LIGHTMAP_BLOCK_SIZE * LIGHTMAP_BLOCK_SIZE )
			* ( 100.0f / ( LIGHTMAP_BLOCK_SIZE * LIGHTMAP_BLOCK_SIZE ) ) / ( numLightmaps - 1 );
	}
	else if ( numLightmaps == 1 ) {
		lightmapEfficiency = c_exactLightmap * ( 100.0f / ( LIGHTMAP_BLOCK_SIZE * LIGHTMAP_BLOCK_SIZE ) );
	}
	qprintf( "%7i exact lightmap texels\n", c_exactLightmap );
	qprintf( "%7i block lightmap texels\n", numLightmaps * LIGHTMAP_BLOCK_SIZE * LIGHTMAP_BLOCK_SIZE );
	qprintf( "%.2f%% lightmap efficiency\n", lightmapEfficiency );
}
