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

#ifdef _TTIMOBUILD
#include "qfiles.h"
#include "surfaceflags.h"
#else
#include "../code/qcommon/qfiles.h"
#include "../code/game/surfaceflags.h"
#endif

#include "cod_bsp_load_storage.h"

extern	int			nummodels;
extern	dmodel_t	dmodels[MAX_MAP_MODELS];

extern	int			numShaders;
extern	dshader_t	dshaders[MAX_MAP_SHADERS];

extern	int			entdatasize;
extern	char		dEntstring[MAX_MAP_ENTSTRING];

extern	int			numleafs;
extern	dleaf_t		dleafs[MAX_MAP_LEAFS];

extern	int			numplanes;
extern	dplane_t	dplanes[MAX_MAP_PLANES];

extern	int			numnodes;
extern	dnode_t		dnodes[MAX_MAP_NODES];

extern	int			numleafsurfaces;
extern	int			dleafsurfaces[MAX_MAP_LEAFFACES];

extern	int			numleafbrushes;
extern	int			dleafbrushes[MAX_MAP_LEAFBRUSHES];

extern	int			numbrushes;
extern	dbrush_t	dbrushes[MAX_MAP_BRUSHES];

extern	int			numbrushsides;
extern	dbrushside_t	dbrushsides[MAX_MAP_BRUSHSIDES];

extern	int			numLightBytes;
extern	byte		lightBytes[];

extern	int			numGridPoints;
extern	byte		gridData[MAX_MAP_LIGHTGRID];

extern	int			numVisBytes;
extern	byte		visBytes[MAX_MAP_VISIBILITY];

extern	int			numDrawVerts;

extern	int			numDrawIndexes;

extern	int			numTriangleSurfaces;

extern	int			numFogs;
extern	dfog_t		dfogs[MAX_MAP_FOGS];

void	LoadBSPFile( const char *filename );
void	WriteBSPFile( const char *filename );
void PrintBSPFileSizes( int fileSize );

//===============


typedef struct epair_s {
	struct epair_s	*next;
	char	*key;
	char	*value;
} epair_t;

typedef struct {
	vec3_t		origin;
	struct bspbrush_s	*brushes;
	struct parseMesh_s	*patches;
	struct terrainSurface_s *terrainSurfaces;	// intermediate terrain meshes, emitted by SubdivideTerrain
	int			firstDrawSurf;
	epair_t		*epairs;
} entity_t;

extern	int			num_entities;
extern	entity_t	entities[MAX_MAP_ENTITIES];

void	ParseEntities( void );
void	UnparseEntities( void );
void UnparseEntitiesWithOrigins( void );

void 	SetKeyValue( entity_t *ent, const char *key, const char *value );
const char 	*ValueForKey( const entity_t *ent, const char *key );
// will return "" if not present

vec_t	FloatForKey( const entity_t *ent, const char *key );
void 	GetVectorForKey( const entity_t *ent, const char *key, vec3_t vec );


// Cursor-based CoD parser, distinct from the legacy global-token parser.
epair_t *ParseEpair( char *key, char **data_p );

void	PrintEntity( const entity_t *ent );

