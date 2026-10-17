/*
===========================================================================
q3map (Call of Duty, 2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

#ifndef Q3MAP_TERRAIN_SURFACE_H
#define Q3MAP_TERRAIN_SURFACE_H

/* Intermediate terrain mesh, before conversion to a map draw surface. */
typedef struct terrainSurface_s {
	struct terrainSurface_s *next;
	shaderInfo_t *shaderInfo;
	int contents;
	int lightmapSampleSize;
	int cullgroup;
	int numIndexes;
	unsigned short *indexes;
	int numVerts;
	drawVert_t *verts;
} terrainSurface_t;

#endif
