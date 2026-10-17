/*
===========================================================================
q3map (Call of Duty, 2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

/* CoD IBSP59 records as stored on disk.
 * Float disk coordinates do not depend on DOUBLEVEC_T.
 * The DiskGfx* type names and their field names are the CoD2 spellings (from the CoD2 Mac build's
 * debug info); the layouts are the ones this tool writes.
 */
#ifndef COD_BSP_DISK_RECORDS_H
#define COD_BSP_DISK_RECORDS_H
typedef dbrush_t cod_bsp_brush_t;
typedef float cod_bsp_vertex_t[3];
typedef struct {
    unsigned short material;
    short lightStyle;
    int firstVert;
    short numVerts;
    short numIndices;
    int firstIndex;
} cod_bsp_tri_soup_t;
typedef struct {
    float mins[3];
    float maxs[3];
    int firstSurface;
    int surfaceCount;
} DiskGfxCullGroup;
typedef struct {
    int firstSurface;
    int surfaceCount;
    int childCount;
} DiskGfxAabbTree;
typedef struct {
    float mins[3];
    float maxs[3];
    int aabbTreeIndex;
    int firstPortal;
    int portalCount;
    int firstCullGroup;
    int cullGroupCount;
    int firstOccluder;
    int occluderCount;
} DiskGfxCell;
typedef struct {
    int planeIndex;
    int cellIndex;
    int firstPortalVertex;
    int portalVertexCount;
} DiskGfxPortal;
typedef struct {
    int firstPlane;
    unsigned short planeCount;
    unsigned short edgeCount;
    int firstEdge;
    int firstPortalVertex;
    unsigned short portalVertexCount;
} DiskGfxOccluder;
typedef struct {
    unsigned char plane[2];
    unsigned char vertex[2];
} cod_bsp_occluder_edge_t;
typedef struct {
    unsigned short material;
    unsigned char isTerrain;
    union {
        struct {
            unsigned short width;
            unsigned short height;
            int subdivisions;
            int firstVertex;
        } patch;
        struct {
            unsigned short numVerts;
            unsigned short numIndices;
            int firstVertex;
            int firstIndex;
        } terrain;
    } geometry;
} cod_bsp_collision_surface_t;
#endif
