/*
===========================================================================
q3map (Call of Duty, 2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

/* IBSP59 lump indices (these differ from the Q3 lump indices).
 */
#ifndef COD_BSP_LUMP_INDICES_H
#define COD_BSP_LUMP_INDICES_H
#define COD_BSP_IDENT 0x50534249
#define COD_BSP_VERSION 59
typedef enum {
    COD_LUMP_SHADERS,
    COD_LUMP_LIGHTMAPS,
    COD_LUMP_PLANES,
    COD_LUMP_BRUSHSIDES,
    COD_LUMP_BRUSHES,
    COD_LUMP_FOGS,
    COD_LUMP_TRIANGLE_SOUPS,
    COD_LUMP_DRAWVERTS,
    COD_LUMP_DRAWINDEXES,
    COD_LUMP_CULLGROUPS,
    COD_LUMP_CULLGROUPINDEXES,
    COD_LUMP_PORTALVERTS,
    COD_LUMP_OCCLUDERS,
    COD_LUMP_OCCLUDERPLANES,
    COD_LUMP_OCCLUDEREDGES,
    COD_LUMP_OCCLUDERINDEXES,
    COD_LUMP_AABBTREES,
    COD_LUMP_CELLS,
    COD_LUMP_PORTALS,
    COD_LUMP_LIGHTINDEXES,
    COD_LUMP_NODES,
    COD_LUMP_LEAFS,
    COD_LUMP_LEAFBRUSHES,
    COD_LUMP_LEAFSURFACES,
    COD_LUMP_COLLISION_SURFACES,
    COD_LUMP_COLLISION_VERTICES,
    COD_LUMP_COLLISION_INDICES,
    COD_LUMP_MODELS,
    COD_LUMP_VISIBILITY,
    COD_LUMP_ENTITIES,
    COD_LUMP_LIGHTS
} cod_bsp_lump_index_t;
#endif
