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

#include "cmdlib.h"
#include "mathlib.h"
#include "bspfile.h"
#include "universal/q_parse.h"

/* BSP lump storage. */
byte lightBytes[MAX_MAP_LIGHTING];
DiskGfxPortal dportals[MAX_MAP_BSP_PORTALS];
cod_bsp_vertex_t dportalVerts[MAX_MAP_PORTAL_VERTS];
int cullGroupIndexes[MAX_MAP_CULLGROUP_INDEXES];
bspLight_t dlights[MAX_MAP_LIGHTS];
cod_bsp_tri_soup_t triangleSurfaces[MAX_MAP_TRISOUPS];
unsigned short collisionIndexes[MAX_MAP_COLLISION_INDEXES];
DiskGfxCullGroup cullGroups[MAX_MAP_CULLGROUPS];
drawVert_t triangleSoupData[MAX_MAP_DRAW_VERTS];
DiskGfxCell dcells[MAX_MAP_CELLS];
cod_bsp_occluder_edge_t bspOccluderEdges[MAX_MAP_OCCLUDER_EDGES];
unsigned short bspLightIndexes[MAX_MAP_LIGHT_INDEXES];
int occluderPlane[MAX_MAP_OCCLUDER_PLANES];
DiskGfxAabbTree daabbtrees[MAX_MAP_AABBTREES];
unsigned short drawIndexes[MAX_MAP_DRAW_INDEXES];
DiskGfxOccluder doccluders[MAX_MAP_OCCLUDERS];
int numnodes;
int numCollisionIndexes;
dmodel_t dmodels[MAX_MAP_MODELS];
int numleafsurfaces;
dleaf_t dleafs[MAX_MAP_LEAFS];
dplane_t dplanes[MAX_MAP_PLANES];
int numaabbtreenodes;
int numShaders;
dshader_t dshaders[MAX_MAP_SHADERS];
int numCells;
int nummodels;
dbrushside_t dbrushsides[MAX_MAP_BRUSHSIDES];
int numLightBytes;
int numOccluders;
int numleafbrushes;
int numVisBytes;
int numplanes;
int numleafs;
int numPortals;
int numLights;
byte visBytes[MAX_MAP_VISIBILITY];
int numCullGroupIndexes;
int numCullGroups;
int numPatchCollides;
cod_bsp_collision_surface_t patchCollides[0x2000];
int numCollisionVerts;
cod_bsp_vertex_t collisionVerts[0x20000];
int numOccluderPlanes;
int numLightIndexes;
int numPortalVerts;
int numDrawIndexes;
int numDrawVerts;
unsigned short occluderIndexes[0xC000];
int numOccluderIndexes;
int numOccluderEdges;
int dleafsurfaces[MAX_MAP_LEAFFACES];
dfog_t dfogs[MAX_MAP_FOGS];
int numTriangleSurfaces;
int numbrushes;
int numFogs;
int dleafbrushes[MAX_MAP_LEAFBRUSHES];
dnode_t dnodes[MAX_MAP_NODES];
int numbrushsides;
int entdatasize;
dbrush_t dbrushes[MAX_MAP_BRUSHES];
char dEntstring[MAX_MAP_ENTSTRING];

/* CoD1 on-disk BSP records; distinct from the retained Quake III records. */
int num_entities;
entity_t entities[MAX_MAP_ENTITIES];

/* Serialized CoD1 BSP element widths, not in-memory record declarations. */
enum {
    COD_BSP_MODEL_BYTES = 48,
    COD_BSP_LEAF_BYTES = 36,
    COD_BSP_PATCH_COLLISION_BYTES = 16,
    COD_BSP_COLLISION_VERTEX_BYTES = 12,
    COD_BSP_COLLISION_INDEX_BYTES = 2,
    COD_BSP_DRAW_INDEX_BYTES = 2,
    COD_BSP_LIGHT_BYTES = 72,
    COD_BSP_LIGHT_INDEX_BYTES = 2,
    COD_BSP_PORTAL_VERTEX_BYTES = 12,
    COD_BSP_OCCLUDER_BYTES = 20,
    COD_BSP_OCCLUDER_PLANE_BYTES = 4,
    COD_BSP_OCCLUDER_EDGE_BYTES = 4,
    COD_BSP_OCCLUDER_INDEX_BYTES = 2,
    COD_BSP_AABB_TREE_BYTES = 12,
    COD_BSP_CELL_BYTES = 52,
    COD_BSP_PORTAL_BYTES = 16,
    COD_BSP_CULL_GROUP_BYTES = 32,
    COD_BSP_CULL_GROUP_INDEX_BYTES = 4,
    COD_BSP_LIGHTMAP_WIDTH = 512,
    COD_BSP_LIGHTMAP_HEIGHT = 512
};

#include <windows.h>


/*
=============
SwapBlock

If all values are 32 bits, this can be used to swap everything
=============
*/
void SwapBlock(int *block, int sizeOfBlock)
{
    int i;

    sizeOfBlock >>= 2;
    for (i = 0; i < sizeOfBlock; i++)
        block[i] = LittleLong(block[i]);
}

/*
=============
SwapBSPBrush
=============
*/
void SwapBSPBrush(cod_bsp_brush_t *brush)
{
    brush->numSides = LittleShort(brush->numSides);
    brush->shaderNum = LittleShort(brush->shaderNum);
}

/*
=============
SwapTriangleSurface
=============
*/
void SwapTriangleSurface(cod_bsp_tri_soup_t *surface)
{
    surface->material = LittleShort(surface->material);
    surface->lightStyle = LittleShort(surface->lightStyle);
    surface->firstVert = LittleLong(surface->firstVert);
    surface->numVerts = LittleShort(surface->numVerts);
    surface->numIndices = LittleShort(surface->numIndices);
    surface->firstIndex = LittleLong(surface->firstIndex);
}

/*
=============
SwapBSPFile

Byte swaps all data in a bsp file.
=============
*/
void SwapBSPFile( void ) {
	int i;
	byte swaptest[2] = { 1, 0 };

	if ( *(short *)swaptest == 1 ) {
		return;
	}

	if ( (unsigned)nummodels > MAX_MAP_MODELS ) {
		Error( "nummodels > %i\n", MAX_MAP_MODELS );
	}
	SwapBlock( (int *)dmodels, nummodels * sizeof( dmodels[0] ) );

	if ( (unsigned)numShaders > MAX_MAP_SHADERS ) {
		Error( "numShaders > %i\n", MAX_MAP_SHADERS );
	}
	for ( i = 0; i < numShaders; i++ ) {
		dshaders[i].contentFlags = LittleLong( dshaders[i].contentFlags );
		dshaders[i].surfaceFlags = LittleLong( dshaders[i].surfaceFlags );
	}

	if ( (unsigned)numplanes > MAX_MAP_PLANES ) {
		Error( "numplanes > %i\n", MAX_MAP_PLANES );
	}
	SwapBlock( (int *)dplanes, numplanes * sizeof( dplanes[0] ) );

	if ( (unsigned)numnodes > MAX_MAP_NODES ) {
		Error( "numnodes > %i\n", MAX_MAP_NODES );
	}
	SwapBlock( (int *)dnodes, numnodes * sizeof( dnodes[0] ) );

	if ( (unsigned)numleafs > MAX_MAP_LEAFS ) {
		Error( "numleafs > %i\n", MAX_MAP_LEAFS );
	}
	SwapBlock( (int *)dleafs, numleafs * sizeof( dleafs[0] ) );

	if ( (unsigned)numleafbrushes > MAX_MAP_LEAFBRUSHES ) {
		Error( "numleafbrushes > %i\n", MAX_MAP_LEAFBRUSHES );
	}
	SwapBlock( dleafbrushes, numleafbrushes * sizeof( dleafbrushes[0] ) );

	if ( (unsigned)numbrushes > MAX_MAP_BRUSHES ) {
		Error( "numbrushes > %i\n", MAX_MAP_BRUSHES );
	}
	for ( i = 0; i < numbrushes; i++ ) {
		SwapBSPBrush( &dbrushes[i] );
	}

	if ( (unsigned)numbrushsides > MAX_MAP_BRUSHSIDES ) {
		Error( "numbrushsides > %i\n", MAX_MAP_BRUSHSIDES );
	}
	SwapBlock( (int *)dbrushsides, numbrushsides * sizeof( dbrushsides[0] ) );

	((int *)visBytes)[0] = LittleLong( ((int *)visBytes)[0] );
	((int *)visBytes)[1] = LittleLong( ((int *)visBytes)[1] );

	if ( (unsigned)numDrawVerts > MAX_MAP_DRAW_VERTS ) {
		Error( "numDrawVerts > %i\n", MAX_MAP_DRAW_VERTS );
	}
	for ( i = 0; i < numDrawVerts; i++ ) {
		triangleSoupData[i].lightmap[0] = LittleFloat( triangleSoupData[i].lightmap[0] );
		triangleSoupData[i].lightmap[1] = LittleFloat( triangleSoupData[i].lightmap[1] );
		triangleSoupData[i].st[0] = LittleFloat( triangleSoupData[i].st[0] );
		triangleSoupData[i].st[1] = LittleFloat( triangleSoupData[i].st[1] );
		triangleSoupData[i].xyz[0] = LittleFloat( triangleSoupData[i].xyz[0] );
		triangleSoupData[i].xyz[1] = LittleFloat( triangleSoupData[i].xyz[1] );
		triangleSoupData[i].xyz[2] = LittleFloat( triangleSoupData[i].xyz[2] );
		triangleSoupData[i].normal[0] = LittleFloat( triangleSoupData[i].normal[0] );
		triangleSoupData[i].normal[1] = LittleFloat( triangleSoupData[i].normal[1] );
		triangleSoupData[i].normal[2] = LittleFloat( triangleSoupData[i].normal[2] );
	}

	if ( (unsigned)numDrawIndexes > MAX_MAP_DRAW_INDEXES ) {
		Error( "numDrawIndexes > %i\n", MAX_MAP_DRAW_INDEXES );
	}
	for ( i = 0; i < numDrawIndexes; i++ ) {
		drawIndexes[i] = LittleShort( drawIndexes[i] );
	}

	if ( (unsigned)numTriangleSurfaces > COD_BSP_MAX_TRIANGLE_SURFACES ) {
		Error( "numTriangleSurfaces > %i\n", COD_BSP_MAX_TRIANGLE_SURFACES );
	}
	for ( i = 0; i < numTriangleSurfaces; i++ ) {
		SwapTriangleSurface( &triangleSurfaces[i] );
	}

	if ( (unsigned)numFogs > MAX_MAP_FOGS ) {
		Error( "numFogs > %i\n", MAX_MAP_FOGS );
	}
	for ( i = 0; i < numFogs; i++ ) {
		dfogs[i].brushNum = LittleLong( dfogs[i].brushNum );
		dfogs[i].visibleSide = LittleLong( dfogs[i].visibleSide );
	}

	if ( (unsigned)numLights > COD_BSP_MAX_LIGHTS ) {
		Error( "numLights > %i\n", COD_BSP_MAX_LIGHTS );
	}
	SwapBlock( (int *)dlights, numLights * sizeof( dlights[0] ) );

	if ( (unsigned)numLightIndexes > COD_BSP_MAX_LIGHT_INDEXES ) {
		Error( "numLightIndexes > %i\n", COD_BSP_MAX_LIGHT_INDEXES );
	}
	for ( i = 0; i < numLightIndexes; i++ ) {
		bspLightIndexes[i] = LittleShort( bspLightIndexes[i] );
	}
}

/*
=============
CopyLump
=============
*/
int CopyLump(dheader_t *header, int lump, void *dest, int size)
{
    int length, ofs;

    length = header->lumps[lump].filelen;
    ofs = header->lumps[lump].fileofs;
    if (length % size)
        Error("LoadBSPFile: odd lump size");
    memcpy(dest, (byte *)header + ofs, length);
    return length / size;
}

/*
=============
LoadBSPFile
=============
*/
void LoadBSPFile( const char *filename ) {
	dheader_t *header;

	if ( LoadFile( filename, (void **)&header ) < 0 ) {
		Error( "Could not load file '%s'\n", filename );
	}
	SwapBlock( (int *)header, sizeof( *header ) );
	if ( header->ident != COD_BSP_IDENT ) {
		Error( "%s is not a IBSP file", filename );
	}
	if ( header->version != COD_BSP_VERSION ) {
		Error( "%s is version %i, not %i", filename, header->version,
			COD_BSP_VERSION );
	}

	numShaders = CopyLump( header, COD_LUMP_SHADERS, dshaders, sizeof( dshaders[0] ) );
	numLightBytes = CopyLump( header, COD_LUMP_LIGHTMAPS, lightBytes, sizeof( lightBytes[0] ) );
	numplanes = CopyLump( header, COD_LUMP_PLANES, dplanes, sizeof( dplanes[0] ) );
	numbrushsides = CopyLump( header, COD_LUMP_BRUSHSIDES, dbrushsides, sizeof( dbrushsides[0] ) );
	numbrushes = CopyLump( header, COD_LUMP_BRUSHES, dbrushes, sizeof( dbrushes[0] ) );
	numFogs = CopyLump( header, COD_LUMP_FOGS, dfogs, sizeof( dfogs[0] ) );
	numTriangleSurfaces = CopyLump( header, COD_LUMP_TRIANGLE_SOUPS, triangleSurfaces, sizeof( triangleSurfaces[0] ) );
	numDrawVerts = CopyLump( header, COD_LUMP_DRAWVERTS, triangleSoupData, sizeof( triangleSoupData[0] ) );
	numDrawIndexes = CopyLump( header, COD_LUMP_DRAWINDEXES, drawIndexes, sizeof( drawIndexes[0] ) );
	numCullGroups = CopyLump( header, COD_LUMP_CULLGROUPS, cullGroups, sizeof( cullGroups[0] ) );
	numCullGroupIndexes = CopyLump( header, COD_LUMP_CULLGROUPINDEXES, cullGroupIndexes, sizeof( cullGroupIndexes[0] ) );
	numPortalVerts = CopyLump( header, COD_LUMP_PORTALVERTS, dportalVerts, sizeof( dportalVerts[0] ) );
	numOccluders = CopyLump( header, COD_LUMP_OCCLUDERS, doccluders, sizeof( doccluders[0] ) );
	numOccluderPlanes = CopyLump( header, COD_LUMP_OCCLUDERPLANES, occluderPlane, sizeof( occluderPlane[0] ) );
	numOccluderEdges = CopyLump( header, COD_LUMP_OCCLUDEREDGES, bspOccluderEdges, sizeof( bspOccluderEdges[0] ) );
	numOccluderIndexes = CopyLump( header, COD_LUMP_OCCLUDERINDEXES, occluderIndexes, sizeof( occluderIndexes[0] ) );
	numaabbtreenodes = CopyLump( header, COD_LUMP_AABBTREES, daabbtrees, sizeof( daabbtrees[0] ) );
	numCells = CopyLump( header, COD_LUMP_CELLS, dcells, sizeof( dcells[0] ) );
	numPortals = CopyLump( header, COD_LUMP_PORTALS, dportals, sizeof( dportals[0] ) );
	numLightIndexes = CopyLump( header, COD_LUMP_LIGHTINDEXES, bspLightIndexes, sizeof( bspLightIndexes[0] ) );
	numnodes = CopyLump( header, COD_LUMP_NODES, dnodes, sizeof( dnodes[0] ) );
	numleafs = CopyLump( header, COD_LUMP_LEAFS, dleafs, sizeof( dleafs[0] ) );
	numleafbrushes = CopyLump( header, COD_LUMP_LEAFBRUSHES, dleafbrushes, sizeof( dleafbrushes[0] ) );
	numleafsurfaces = CopyLump( header, COD_LUMP_LEAFSURFACES, dleafsurfaces, sizeof( dleafsurfaces[0] ) );
	numPatchCollides = CopyLump( header, COD_LUMP_COLLISION_SURFACES, patchCollides, sizeof( patchCollides[0] ) );
	numCollisionVerts = CopyLump( header, COD_LUMP_COLLISION_VERTICES, collisionVerts, sizeof( collisionVerts[0] ) );
	numCollisionIndexes = CopyLump( header, COD_LUMP_COLLISION_INDICES, collisionIndexes, sizeof( collisionIndexes[0] ) );
	nummodels = CopyLump( header, COD_LUMP_MODELS, dmodels, sizeof( dmodels[0] ) );
	numVisBytes = CopyLump( header, COD_LUMP_VISIBILITY, visBytes, sizeof( visBytes[0] ) );
	entdatasize = CopyLump( header, COD_LUMP_ENTITIES, dEntstring, sizeof( dEntstring[0] ) );
	numLights = CopyLump( header, COD_LUMP_LIGHTS, dlights, sizeof( dlights[0] ) );

	free( header );
	SwapBSPFile();
}

/*
=============
AddLump
=============
*/
void AddLump(FILE *bspfile, dheader_t *header, int lumpnum,
             const void *data, int len)
{
    lump_t *lump;

    lump = &header->lumps[lumpnum];
    lump->fileofs = LittleLong(ftell(bspfile));
    lump->filelen = LittleLong(len);
    SafeWrite(bspfile, data, (len + 3) & ~3);
}

/*
=============
WriteBSPFile

Swaps the bsp file in place, so it should not be referenced again
=============
*/
void WriteBSPFile( const char *filename ) {
	dheader_t outheader, *header;
	FILE *bspfile;
	DWORD attributes;
	char text[MAX_OS_PATH];

	header = &outheader;
	memset( header, 0, sizeof( *header ) );
	SwapBSPFile();

	bspfile = fopen( filename, "wb" );
	if ( !bspfile ) {
		attributes = GetFileAttributesA( filename );
		if ( attributes == INVALID_FILE_ATTRIBUTES ||
			!( attributes & FILE_ATTRIBUTE_READONLY ) ) {
			Error( "could not open '%s' for writing\n", filename );
		}
		sprintf( text, "could not open '%s' for writing; replace read-only file?", filename );
		if ( MessageBoxA( NULL, text, "OUTPUT FILE IS READ ONLY",
			MB_OKCANCEL | MB_ICONQUESTION ) != IDOK ) {
			return;
		}
		SetFileAttributesA( filename, attributes & ~FILE_ATTRIBUTE_READONLY );
		bspfile = fopen( filename, "wb" );
		if ( !bspfile ) {
			Error( "could not open '%s' for writing\n", filename );
		}
	}

	header->ident = LittleLong( COD_BSP_IDENT );
	header->version = LittleLong( COD_BSP_VERSION );
	SafeWrite( bspfile, header, sizeof( *header ) );

	AddLump( bspfile, header, COD_LUMP_SHADERS, dshaders,
		numShaders * sizeof( dshaders[0] ) );
	AddLump( bspfile, header, COD_LUMP_LIGHTMAPS, lightBytes, numLightBytes );
	AddLump( bspfile, header, COD_LUMP_PLANES, dplanes,
		numplanes * sizeof( dplanes[0] ) );
	AddLump( bspfile, header, COD_LUMP_BRUSHSIDES, dbrushsides,
		numbrushsides * sizeof( dbrushsides[0] ) );
	AddLump( bspfile, header, COD_LUMP_BRUSHES, dbrushes,
		numbrushes * sizeof( dbrushes[0] ) );
	AddLump( bspfile, header, COD_LUMP_FOGS, dfogs,
		numFogs * sizeof( dfogs[0] ) );
	AddLump( bspfile, header, COD_LUMP_TRIANGLE_SOUPS, triangleSurfaces,
		numTriangleSurfaces * sizeof( triangleSurfaces[0] ) );
	AddLump( bspfile, header, COD_LUMP_DRAWVERTS, triangleSoupData,
		numDrawVerts * sizeof( triangleSoupData[0] ) );
	AddLump( bspfile, header, COD_LUMP_DRAWINDEXES, drawIndexes,
		numDrawIndexes * sizeof( drawIndexes[0] ) );
	AddLump( bspfile, header, COD_LUMP_CULLGROUPS, cullGroups,
		numCullGroups * sizeof( cullGroups[0] ) );
	AddLump( bspfile, header, COD_LUMP_CULLGROUPINDEXES, cullGroupIndexes,
		numCullGroupIndexes * sizeof( cullGroupIndexes[0] ) );
	AddLump( bspfile, header, COD_LUMP_PORTALVERTS, dportalVerts,
		numPortalVerts * sizeof( dportalVerts[0] ) );
	AddLump( bspfile, header, COD_LUMP_OCCLUDERS, doccluders,
		numOccluders * sizeof( doccluders[0] ) );
	AddLump( bspfile, header, COD_LUMP_OCCLUDERPLANES, occluderPlane,
		numOccluderPlanes * sizeof( occluderPlane[0] ) );
	AddLump( bspfile, header, COD_LUMP_OCCLUDEREDGES, bspOccluderEdges,
		numOccluderEdges * sizeof( bspOccluderEdges[0] ) );
	AddLump( bspfile, header, COD_LUMP_OCCLUDERINDEXES, occluderIndexes,
		numOccluderIndexes * sizeof( occluderIndexes[0] ) );
	AddLump( bspfile, header, COD_LUMP_AABBTREES, daabbtrees,
		numaabbtreenodes * sizeof( daabbtrees[0] ) );
	AddLump( bspfile, header, COD_LUMP_CELLS, dcells,
		numCells * sizeof( dcells[0] ) );
	AddLump( bspfile, header, COD_LUMP_PORTALS, dportals,
		numPortals * sizeof( dportals[0] ) );
	AddLump( bspfile, header, COD_LUMP_LIGHTINDEXES, bspLightIndexes,
		numLightIndexes * sizeof( bspLightIndexes[0] ) );
	AddLump( bspfile, header, COD_LUMP_NODES, dnodes,
		numnodes * sizeof( dnodes[0] ) );
	AddLump( bspfile, header, COD_LUMP_LEAFS, dleafs,
		numleafs * sizeof( dleafs[0] ) );
	AddLump( bspfile, header, COD_LUMP_LEAFBRUSHES, dleafbrushes,
		numleafbrushes * sizeof( dleafbrushes[0] ) );
	AddLump( bspfile, header, COD_LUMP_LEAFSURFACES, dleafsurfaces,
		numleafsurfaces * sizeof( dleafsurfaces[0] ) );
	AddLump( bspfile, header, COD_LUMP_COLLISION_SURFACES, patchCollides,
		numPatchCollides * sizeof( patchCollides[0] ) );
	AddLump( bspfile, header, COD_LUMP_COLLISION_VERTICES, collisionVerts,
		numCollisionVerts * sizeof( collisionVerts[0] ) );
	AddLump( bspfile, header, COD_LUMP_COLLISION_INDICES, collisionIndexes,
		numCollisionIndexes * sizeof( collisionIndexes[0] ) );
	AddLump( bspfile, header, COD_LUMP_MODELS, dmodels,
		nummodels * sizeof( dmodels[0] ) );
	AddLump( bspfile, header, COD_LUMP_VISIBILITY, visBytes, numVisBytes );
	AddLump( bspfile, header, COD_LUMP_ENTITIES, dEntstring, entdatasize );
	AddLump( bspfile, header, COD_LUMP_LIGHTS, dlights,
		numLights * sizeof( dlights[0] ) );

	fseek( bspfile, 0, SEEK_SET );
	SafeWrite( bspfile, header, sizeof( *header ) );
	fclose( bspfile );
}

/*
=============
PrintBSPLumpSize
=============
*/
void PrintBSPLumpSize(const char *name, int count, int size, float pctMultiplier)
{
    char buffer[16];

    buffer[0] = 0;
    if (count >= 0)
        itoa(count, buffer, 10);
    printf("%6s %-16s %8i B %6.0f KB %5.1f%%\n", buffer, name, size,
           size / 1024.0f, pctMultiplier * size);
}

/*
=============
StripTrailing
=============
*/
void StripTrailing(char *e)
{
    char *s;

    s = e + strlen(e) - 1;
    while (s >= e && *s <= 32)
    {
        *s = 0;
        s--;
    }
}

/*
=================
ParseEpair
=================
*/
epair_t *ParseEpair(char *key, char **data_p)
{
    epair_t *ep;
    char *value;

    ep = malloc(sizeof(*ep));
    memset(ep, 0, sizeof(*ep));
    if (strlen(key) >= MAX_KEY - 1)
        Error("ParseEpair: token too long");
    if (key[strlen(key) - 2] == '\\')
        Error("ParseEpair: key '%s' ends with a '\\'\n", key);
    if (strchr(key, '\n') || strchr(key, '\r'))
        Error("ParseEpair: key '%s' contains a newline character\n", key);
    if (strchr(key, '"'))
        Error("ParseEpair: key '%s' contains a \" character, will cause parsing errors\n", key);
    ep->key = copystring(key);

    value = Com_ParseOnLine(data_p);
    if (strlen(value) >= MAX_VALUE - 1)
        Error("ParseEpair: token too long");
    if (value[strlen(value) - 2] == '\\')
        Error("ParseEpair: value '%s' ends with a '\\'\n", value);
    if (strchr(value, '\n') || strchr(value, '\r'))
        Error("ParseEpair: value '%s' contains a newline character (use of '\\' at end of value?)\n", value);
    if (strchr(value, '"'))
        Error("ParseEpair: value '%s' contains a \" character, will cause parsing errors\n", value);
    ep->value = copystring(value);
    StripTrailing(ep->key);
    StripTrailing(ep->value);
    return ep;
}

/*
================
ParseEntity
================
*/
int ParseEntity(char **data_p)
{
    char *token;
    epair_t *ep;
    entity_t *mapent;

    token = Com_Parse(data_p);
    if (!token[0])
        return 0;
    if (strcmp(token, "{"))
        Error("ParseEntity: { not found");
    if (num_entities == MAX_MAP_ENTITIES)
        Error("num_entities == MAX_MAP_ENTITIES");

    mapent = &entities[num_entities];
    num_entities++;

    while (1)
    {
        token = Com_Parse(data_p);
        if (!token[0])
            Error("ParseEntity: EOF without closing brace");
        if (!strcmp(token, "}"))
            break;
        ep = ParseEpair(token, data_p);
        ep->next = mapent->epairs;
        mapent->epairs = ep;
    }
    return 1;
}

/*
================
ParseEntities

Parses the dentdata string into entities
================
*/
void ParseEntities(void)
{
    char *data;

    num_entities = 0;
    Com_BeginParseSession("LUMP_ENTITIES");
    data = dEntstring;
    while (ParseEntity(&data))
        ;
}

/*
================
UnparseEntities

Generates the dentdata string from all the entities
This allows the utilities to add or remove key/value pairs
to the data created by the map editor.
================
*/
void UnparseEntities(void)
{
    char *buf, *end;
    epair_t *ep;
    char line[2048];
    int i;
    char key[1024], value[1024];

    buf = dEntstring;
    end = buf;
    *end = 0;
    for (i = 0; i < num_entities; i++)
    {
        ep = entities[i].epairs;
        if (!ep)
            continue;
        strcat(end, "{\n");
        end += 2;
        for (ep = entities[i].epairs; ep; ep = ep->next)
        {
            strcpy(key, ep->key);
            StripTrailing(key);
            strcpy(value, ep->value);
            StripTrailing(value);
            sprintf(line, "\"%s\" \"%s\"\n", key, value);
            strcat(end, line);
            end += strlen(line);
        }
        strcat(end, "}\n");
        end += 2;
        if (end > buf + MAX_MAP_ENTSTRING)
            Error("Entity text too long");
    }
    entdatasize = end - buf + 1;
}

/*
=============
UnparseEntitiesWithOrigins
=============
*/
void UnparseEntitiesWithOrigins(void)
{
    char *buf, *end;
    epair_t *ep;
    char line[2048];
    int i;
    char key[1024], value[1024];
    char origin[32], search[256], originCopy[256];
    int length = 0;
    char *found;
    int j;
    epair_t *originPair;

    for (i = 0; i < num_entities; i++) {
        ep = entities[i].epairs;
        if (!ep) continue;
        for (; ep; ep = ep->next) {
            if (!strstr(ep->key, "model") || !strstr(ep->value, "*")) continue;
            strcpy(key, ep->key);
            StripTrailing(key);
            strcpy(value, ep->value);
            StripTrailing(value);
            sprintf(search, "\"%s\" \"%s\"\n\"origin\" ", key, value);
            found = strstr(dEntstring, search);
            if (found) break;
        }
        if (!ep) continue;
        length = strlen(search) + 1;
        memset(origin, 0, sizeof(origin));
        if (found[length] != '"') {
            j = 0;
            do {
                origin[j] = found[length + j];
                j++;
                originCopy[j] = found[length + j];
            } while (found[length + j] != '"');
        }
        originPair = malloc(sizeof(*originPair));
        originPair->next = entities[i].epairs->next;
        entities[i].epairs->next = originPair;
        originPair->key = copystring("origin");
        originPair->value = copystring(origin);
    }

    buf = dEntstring;
    end = buf;
    *end = 0;
    for (i = 0; i < num_entities; i++)
    {
        ep = entities[i].epairs;
        if (!ep)
            continue;
        strcat(end, "{\n");
        end += 2;
        for (ep = entities[i].epairs; ep; ep = ep->next)
        {
            strcpy(key, ep->key);
            StripTrailing(key);
            strcpy(value, ep->value);
            StripTrailing(value);
            sprintf(line, "\"%s\" \"%s\"\n", key, value);
            strcat(end, line);
            end += strlen(line);
        }
        strcat(end, "}\n");
        end += 2;
        if (end > buf + MAX_MAP_ENTSTRING)
            Error("Entity text too long");
    }
    entdatasize = end - buf + 1;
}

/*
=============
PrintEntity
=============
*/
void PrintEntity( const entity_t *ent ) {
	epair_t *ep;

	printf( "------- entity %p -------\n", ent );
	for ( ep = ent->epairs; ep; ep = ep->next ) {
		printf( "%s = %s\n", ep->key, ep->value );
	}
}

/*
=============
SetKeyValue
=============
*/
void SetKeyValue(entity_t *ent, const char *key, const char *value)
{
    epair_t *ep;

    for (ep = ent->epairs; ep; ep = ep->next)
    {
        if (!strcmp(ep->key, key))
        {
            free(ep->value);
            ep->value = copystring(value);
            return;
        }
    }
    ep = malloc(sizeof(*ep));
    ep->next = ent->epairs;
    ent->epairs = ep;
    ep->key = copystring(key);
    ep->value = copystring(value);
}

/*
=============
ValueForKey
=============
*/
const char *ValueForKey(const entity_t *ent, const char *key)
{
    epair_t *ep;

    for (ep = ent->epairs; ep; ep = ep->next)
    {
        if (!strcmp(ep->key, key))
            return ep->value;
    }
    return "";
}

/*
=============
FloatForKey
=============
*/
vec_t FloatForKey(const entity_t *ent, const char *key)
{
    const char *k;

    k = ValueForKey(ent, key);
    return atof(k);
}

/*
=============
GetVectorForKey
=============
*/
void GetVectorForKey(const entity_t *ent, const char *key, vec3_t vec)
{
    const char *k;
    double v1, v2, v3;
    int count;

    k = ValueForKey(ent, key);
    v1 = v2 = v3 = 0;
    count = sscanf(k, "%lf %lf %lf", &v1, &v2, &v3);
    if (count == 1 || count == 2)
        printf("WARNING: entity %i: key '%s' with value '%s', when it should be a vector; treating as '%g %g %g'\n",
               ent - entities, key, k, v1, v2, v3);
    vec[0] = v1;
    vec[1] = v2;
    vec[2] = v3;
}

/*
=============
PrintBSPFileSizes

Dumps info about current file
=============
*/
void PrintBSPFileSizes(int fileSize)
{
    float pct;

    if (!num_entities)
        ParseEntities();
    pct = 100.0f / fileSize;

    PrintBSPLumpSize("models", nummodels, nummodels * COD_BSP_MODEL_BYTES, pct);
    PrintBSPLumpSize("shaders", numShaders, numShaders * sizeof(dshader_t), pct);
    PrintBSPLumpSize("brushes", numbrushes, numbrushes * sizeof(cod_bsp_brush_t), pct);
    PrintBSPLumpSize("brushsides", numbrushsides, numbrushsides * sizeof(dbrushside_t), pct);
    PrintBSPLumpSize("fogs", numFogs, numFogs * sizeof(dfog_t), pct);
    PrintBSPLumpSize("planes", numplanes, numplanes * sizeof(dplane_t), pct);
    PrintBSPLumpSize("entdata", num_entities, entdatasize, pct);
    printf("\n");

    PrintBSPLumpSize("nodes", numnodes, numnodes * sizeof(dnode_t), pct);
    PrintBSPLumpSize("leafs", numleafs, numleafs * COD_BSP_LEAF_BYTES, pct);
    PrintBSPLumpSize("leafbrushes", numleafbrushes, numleafbrushes * sizeof(int), pct);
    PrintBSPLumpSize("leafsurfaces", numleafsurfaces, numleafsurfaces * sizeof(int), pct);
    PrintBSPLumpSize("patchcollision", numPatchCollides, numPatchCollides * COD_BSP_PATCH_COLLISION_BYTES, pct);
    PrintBSPLumpSize("collisionverts", numCollisionVerts, numCollisionVerts * COD_BSP_COLLISION_VERTEX_BYTES, pct);
    PrintBSPLumpSize("collisionindexes", numCollisionIndexes, numCollisionIndexes * COD_BSP_COLLISION_INDEX_BYTES, pct);
    PrintBSPLumpSize("drawverts", numDrawVerts, numDrawVerts * sizeof(drawVert_t), pct);
    PrintBSPLumpSize("drawindexes", numDrawIndexes, numDrawIndexes * COD_BSP_DRAW_INDEX_BYTES, pct);
    PrintBSPLumpSize("trianglesoups", numTriangleSurfaces, numTriangleSurfaces * sizeof(cod_bsp_tri_soup_t), pct);
    PrintBSPLumpSize("lightmaps", numLightBytes / (COD_BSP_LIGHTMAP_WIDTH * COD_BSP_LIGHTMAP_HEIGHT * 3), numLightBytes, pct);
    PrintBSPLumpSize("lights", numLights, numLights * COD_BSP_LIGHT_BYTES, pct);
    PrintBSPLumpSize("lightindexes", numLightIndexes, numLightIndexes * COD_BSP_LIGHT_INDEX_BYTES, pct);
    PrintBSPLumpSize("visibility", -1, numVisBytes, pct);
    PrintBSPLumpSize("portalverts", numPortalVerts, numPortalVerts * COD_BSP_PORTAL_VERTEX_BYTES, pct);
    PrintBSPLumpSize("occluders", numOccluders, numOccluders * COD_BSP_OCCLUDER_BYTES, pct);
    PrintBSPLumpSize("occluderplanes", numOccluderPlanes, numOccluderPlanes * COD_BSP_OCCLUDER_PLANE_BYTES, pct);
    PrintBSPLumpSize("occluderedges", numOccluderEdges, numOccluderEdges * COD_BSP_OCCLUDER_EDGE_BYTES, pct);
    PrintBSPLumpSize("occluderindexes", numOccluderIndexes, numOccluderIndexes * COD_BSP_OCCLUDER_INDEX_BYTES, pct);
    PrintBSPLumpSize("aabbtrees", numaabbtreenodes, numaabbtreenodes * COD_BSP_AABB_TREE_BYTES, pct);
    PrintBSPLumpSize("cells", numCells, numCells * COD_BSP_CELL_BYTES, pct);
    PrintBSPLumpSize("portals", numPortals, numPortals * COD_BSP_PORTAL_BYTES, pct);
    PrintBSPLumpSize("cullgroups", numCullGroups, numCullGroups * COD_BSP_CULL_GROUP_BYTES, pct);
    PrintBSPLumpSize("cullgroupindexes", numCullGroupIndexes, numCullGroupIndexes * COD_BSP_CULL_GROUP_INDEX_BYTES, pct);
}

