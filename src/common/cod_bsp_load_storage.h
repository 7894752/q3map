/*
===========================================================================
q3map (Call of Duty, 2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

/* Declarations of the BSP lump storage arrays defined in bspfile.c. */
#ifndef COD_BSP_LOAD_STORAGE_H
#define COD_BSP_LOAD_STORAGE_H
#include "cod_bsp_disk_records.h"
#include "bsp_light_disk.h"
#include "cod_bsp_lump_indices.h"
enum {
    COD_BSP_MAX_TRIANGLE_SURFACES = 131072,
    COD_BSP_MAX_LIGHTS = 2048,
    COD_BSP_MAX_LIGHT_INDEXES = 65536
};
extern int numShaders;
extern dshader_t dshaders[];
extern int numLightBytes;
extern byte lightBytes[];
extern int numplanes;
extern dplane_t dplanes[];
extern int numbrushsides;
extern dbrushside_t dbrushsides[];
extern int numbrushes;
extern dbrush_t dbrushes[];
extern int numFogs;
extern dfog_t dfogs[];
extern int numTriangleSurfaces;
extern cod_bsp_tri_soup_t triangleSurfaces[];
extern int numDrawVerts;
extern drawVert_t triangleSoupData[];
extern int numDrawIndexes;
extern unsigned short drawIndexes[];
extern int numCullGroups;
extern DiskGfxCullGroup cullGroups[];
extern int numCullGroupIndexes;
extern int cullGroupIndexes[];
extern int numPortalVerts;
extern cod_bsp_vertex_t dportalVerts[];
extern int numOccluders;
extern DiskGfxOccluder doccluders[];
extern int numOccluderPlanes;
extern int occluderPlane[];
extern int numOccluderEdges;
extern cod_bsp_occluder_edge_t bspOccluderEdges[];
extern int numOccluderIndexes;
extern unsigned short occluderIndexes[];
extern int numaabbtreenodes;
extern DiskGfxAabbTree daabbtrees[];
extern int numCells;
extern DiskGfxCell dcells[];
extern int numPortals;
extern DiskGfxPortal dportals[];
extern int numLightIndexes;
extern unsigned short bspLightIndexes[];
extern int numnodes;
extern dnode_t dnodes[];
extern int numleafs;
extern dleaf_t dleafs[];
extern int numleafbrushes;
extern int dleafbrushes[];
extern int numleafsurfaces;
extern int dleafsurfaces[];
extern int numPatchCollides;
extern cod_bsp_collision_surface_t patchCollides[];
extern int numCollisionVerts;
extern cod_bsp_vertex_t collisionVerts[];
extern int numCollisionIndexes;
extern unsigned short collisionIndexes[];
extern int nummodels;
extern dmodel_t dmodels[];
extern int numLights;
extern bspLight_t dlights[];
extern int numVisBytes;
extern byte visBytes[];
extern int entdatasize;
extern char dEntstring[];
void SwapBSPFile( void );
#endif
