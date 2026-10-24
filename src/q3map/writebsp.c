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
#include "plane_remap.h"

#define MAX_SWITCHED_LIGHTS 32

float VectorDistance( const vec3_t a, const vec3_t b );
int Q_strcasecmp( const char *s1, const char *s2 );

void EmitLeaf( node_t *node );

/* Forward declarations. */
void EmitLeaf( node_t *node );
void SetCoronaCutoffs( void );
int	EmitShader( const char *shader, int contents );
void SetModelNumbers (void);
void SetLightStyles (void);
void BeginBSPFile( void );
void EndBSPFile( void );
void EmitBrushes ( bspbrush_t *brushes );
void BeginModel( void );
void EndModel( node_t *headnode );
void RemapNodePlanes( const int *planeMap );
void RemapBrushPlanes( const int *planeMap );
void EmitPlanes( void );
int EmitDrawNode_r( node_t *node );

/*
============
EmitShader
============
*/
int	EmitShader( const char *shader, int contents ) {
	int				i;
	shaderInfo_t	*si;

	if ( !shader ) {
		shader = "noshader";
	}

	for ( i = 0 ; i < numShaders ; i++ ) {
		if ( !Q_stricmp( shader, dshaders[i].shader ) &&
			contents == dshaders[i].contentFlags ) {
			return i;
		}
	}

	if ( i == MAX_MAP_SHADERS ) {
		Error( "MAX_MAP_SHADERS" );
	}
	numShaders++;
	strcpy( dshaders[i].shader, shader );

	si = ShaderInfoForShader( shader );
	dshaders[i].surfaceFlags = si->surfaceFlags;
	dshaders[i].contentFlags = contents;
	/* empty check left over from the original (probably a removed debug test); kept as is */
	if ( strstr( shader, "caulk" ) ) {
	}

	return i;
}

/*
==================
EmitLeaf
==================
*/
void EmitLeaf( node_t *node ) {
	dleaf_t *leaf;
	bspbrush_t *b;
	drawSurfRef_t *ref;
	if ( numleafs >= MAX_MAP_LEAFS ) Error("MAX_MAP_LEAFS");
	leaf = &dleafs[numleafs];
	numleafs++;
	leaf->cluster = node->cluster;
	leaf->area = node->area;
	leaf->cellIndex = node->bspCell;
	leaf->firstLeafBrush = numleafbrushes;
	for ( b = node->brushlist; b; b = b->next ) {
		if ( numleafbrushes >= MAX_MAP_LEAFBRUSHES ) Error("MAX_MAP_LEAFBRUSHES");
		dleafbrushes[numleafbrushes] = b->original->outputNumber;
		numleafbrushes++;
	}
	leaf->numLeafBrushes = numleafbrushes - leaf->firstLeafBrush;
	leaf->firstLeafSurface = 0;
	leaf->numLeafSurfaces = 0;
	if ( !node->opaque ) {
		leaf->firstLeafSurface = numleafsurfaces;
		for ( ref = node->collisionSurfReferences; ref; ref = ref->nextRef ) {
			if ( numleafsurfaces >= MAX_MAP_LEAFFACES ) Error("MAX_MAP_LEAFFACES");
			dleafsurfaces[numleafsurfaces] = ref->outputNumber;
			numleafsurfaces++;
		}
		leaf->numLeafSurfaces = numleafsurfaces - leaf->firstLeafSurface;
	}
}

/*
============
EmitDrawNode_r
============
*/
int EmitDrawNode_r( node_t *node ) {
	dnode_t *n;
	int i;

	if ( node->planenum == PLANENUM_LEAF ) {
		EmitLeaf( node );
		return -numleafs;
	}
	if ( numnodes == MAX_MAP_NODES ) {
		Error( "MAX_MAP_NODES" );
	}
	n = &dnodes[numnodes];
	numnodes++;
	VectorCopy( node->mins, n->mins );
	VectorCopy( node->maxs, n->maxs );
	if ( node->planenum & 1 ) {
		Error( "WriteDrawNodes_r: odd planenum" );
	}
	n->planeNum = node->planenum;
	mapplanes[n->planeNum].used = qtrue;
	for ( i = 0; i < 2; i++ ) {
		if ( node->children[i]->planenum == PLANENUM_LEAF ) {
			n->children[i] = -( numleafs + 1 );
			EmitLeaf( node->children[i] );
		} else {
			n->children[i] = numnodes;
			EmitDrawNode_r( node->children[i] );
		}
	}
	return n - dnodes;
}

/*
=============
RemapNodePlanes
=============
*/
void RemapNodePlanes( const int *planeMap ) {
	int i;

	for ( i = 0; i < numnodes; i++ ) {
		if ( dnodes[i].planeNum >= 0 ) {
			dnodes[i].planeNum = planeMap[dnodes[i].planeNum];
		}
	}
}

/*
============
SetModelNumbers
============
*/
void SetModelNumbers (void)
{
	int		i;
	int		models;
	char	value[10];

	models = 1;
	for ( i=1 ; i<num_entities ; i++ ) {
		if ( entities[i].brushes || entities[i].patches ) {
			sprintf ( value, "*%i", models );
			models++;
			SetKeyValue (&entities[i], "model", value);
		}
	}

}

/*
============
SetLightStyles
============
*/
void SetLightStyles (void)
{
	int		stylenum;
	const char	*t;
	entity_t	*e;
	int		i, j;
	char	value[10];
	char	lighttargets[MAX_SWITCHED_LIGHTS][64];


	// any light that is controlled (has a targetname)
	// must have a unique style number generated for it

	stylenum = 0;
	for (i=1 ; i<num_entities ; i++)
	{
		e = &entities[i];

		t = ValueForKey (e, "classname");
		if (Q_strncasecmp (t, "light", 5))
			continue;
		t = ValueForKey (e, "targetname");
		if (!t[0])
			continue;

		// find this targetname
		for (j=0 ; j<stylenum ; j++)
			if (!strcmp (lighttargets[j], t))
				break;
		if (j == stylenum)
		{
			if (stylenum == MAX_SWITCHED_LIGHTS)
				Error ("stylenum == MAX_SWITCHED_LIGHTS");
			strcpy (lighttargets[j], t);
			stylenum++;
		}
		sprintf (value, "%i", 32 + j);
		SetKeyValue (e, "style", value);
	}

}

/*
=============
SetCoronaCutoffs
=============
*/
void SetCoronaCutoffs( void ) {
    int i, j;
    entity_t *ent, *ent2;
    const char *value;
    i = 0;
    if (i + 1 >= num_entities) return;
    for (ent = &entities[1]; i + 1 < num_entities; i++, ent++) {
        value = ValueForKey(ent, "classname");
        if (Q_strcasecmp(value, "corona")) continue;
        value = ValueForKey(ent, "zcutoff");
        if (value && value[0]) continue;
        for (j = 1; j < num_entities; j++) {
            ent2 = &entities[j];
            value = ValueForKey(ent2, "classname");
            if (Q_strcasecmp(value, "misc_model") || VectorDistance(ent->origin, ent2->origin) > 8) continue;
            value = ValueForKey(ent2, "model");
            if (!Q_strcasecmp(value, "xmodel/hangingutillight") || !Q_strcasecmp(value, "xmodel/light_ind_ceiling2on")) {
                SetKeyValue(ent, "zcutoff", "-0.15");
                SetKeyValue(ent, "zfadeout", "-0.25");
            }
            break;
        }
    }
}

/*
==================
BeginBSPFile
==================
*/
void BeginBSPFile( void ) {
	// these values may actually be initialized
	// if the file existed when loaded, so clear them explicitly
	nummodels = 0;
	numnodes = 0;
	numbrushsides = 0;
	numleafsurfaces = 0;
	numleafbrushes = 0;

	// leave leaf 0 as an error, because leafs are referenced as
	// negative number nodes
	numleafs = 1;
}

/*
=============
RemapBrushPlanes
=============
*/
void RemapBrushPlanes( const int *planeMap ) {
	int i, j;
	dbrushside_t *side;

	side = dbrushsides;
	for ( i = 0; i < numbrushes; i++ ) {
		side += 6;
		for ( j = 6; j < dbrushes[i].numSides; j++, side++ ) {
			side->plane.planeNum = planeMap[side->plane.planeNum];
		}
	}
}

/*
============
EmitBrushes
============
*/
void EmitBrushes ( bspbrush_t *brushes ) {
	int				j;
	int				firstSide;
	int				sideNum;
	dbrush_t		*db;
	bspbrush_t		*b;
	dbrushside_t	*cp;
	plane_t			*plane;

	for ( b = brushes ; b ; b = b->next ) {
		if ( b->noBspOutput || b->original->noBspOutput ) {
			continue;
		}

		if ( numbrushes == MAX_MAP_BRUSHES ) {
			Error( "MAX_MAP_BRUSHES" );
		}
		b->outputNumber = numbrushes;
		db = &dbrushes[numbrushes];
		numbrushes++;

		db->shaderNum = EmitShader( b->contentShader->shader, b->contents );
		db->numSides = 0;
		firstSide = numbrushsides;

		for ( j = 0 ; j < b->numsides ; j++ ) {
			if ( b->sides[j].backSide ) {
				continue;
			}
			if ( numbrushsides == MAX_MAP_BRUSHSIDES ) {
				Error( "MAX_MAP_BRUSHSIDES " );
			}

			sideNum = numbrushsides - firstSide;
			plane = &mapplanes[b->sides[j].planenum];
			cp = &dbrushsides[numbrushsides];
			db->numSides++;
			numbrushsides++;

			if ( sideNum < 6 ) {
				if ( sideNum & 1 ) {
					cp->plane.planeDist = plane->dist;
				}
				else {
					cp->plane.planeDist = -plane->dist;
				}
			}
			else {
				plane->used = qtrue;
				cp->plane.planeNum = b->sides[j].planenum;
			}

			cp->shaderNum = EmitShader( b->sides[j].shaderInfo->shader, b->contents );
		}
	}
}

/*
==================
BeginModel
==================
*/
void BeginModel( void ) {
	dmodel_t	*mod;
	bspbrush_t	*b;
	entity_t	*e;
	vec3_t		mins, maxs;
	parseMesh_t	*p;
	int			i;

	if ( nummodels == MAX_MAP_MODELS ) {
		Error( "MAX_MAP_MODELS" );
	}
	mod = &dmodels[nummodels];

	//
	// bound the brushes
	//
	e = &entities[entity_num];

	ClearBounds (mins, maxs);
	for ( b = e->brushes ; b ; b = b->next ) {
		if ( !b->numsides ) {
			continue;	// not a real brush (origin brush, etc)
		}
		AddPointToBounds (b->mins, mins, maxs);
		AddPointToBounds (b->maxs, mins, maxs);
	}

	for ( p = e->patches ; p ; p = p->next ) {
		for ( i = 0 ; i < p->mesh.width * p->mesh.height ; i++ ) {
			AddPointToBounds( p->mesh.verts[i].xyz, mins, maxs );
		}
	}

	VectorCopy (mins, mod->mins);
	VectorCopy (maxs, mod->maxs);

	mod->firstSurface = numTriangleSurfaces;
	mod->firstLeafSurface = numleafsurfaces;
	mod->firstBrush = numbrushes;

	EmitBrushes( e->brushes );
}

/*
==================
EndModel
==================
*/
void EndModel( node_t *headnode ) {
	dmodel_t	*mod;

	qprintf ("--- EndModel ---\n");

	mod = &dmodels[nummodels];
	EmitDrawNode_r (headnode);
	mod->numSurfaces = numTriangleSurfaces - mod->firstSurface;
	mod->numLeafSurfaces = numleafsurfaces - mod->firstLeafSurface;
	mod->numBrushes = numbrushes - mod->firstBrush;

	nummodels++;
}

/*
============
EmitPlanes

There is no oportunity to discard planes, because all of the original
brushes will be saved in the map.
============
*/
void EmitPlanes( void ) {
	int i;
	int *planeMap;
	dplane_t *dp;
	plane_t *mp;

	planeMap = malloc( nummapplanes * sizeof( *planeMap ) );
	mp = mapplanes;
	for ( i = 0; i < nummapplanes; i++, mp++ ) {
		if ( !mapplanes[i].used ) {
			planeMap[i] = -1;
			continue;
		}
		planeMap[i] = numplanes;
		dp = &dplanes[numplanes];
		VectorCopy( mp->normal, dp->normal );
		dp->dist = mp->dist;
		numplanes++;
	}

	RemapNodePlanes( planeMap );
	RemapBrushPlanes( planeMap );
	RemapPortalPlanes( planeMap );
	RemapOccluderPlanes( planeMap );
	free( planeMap );
}

/*
============
EndBSPFile
============
*/
void EndBSPFile( void ) {
	char	path[1024];

	EmitPlanes ();
	UnparseEntities ();

	// write the map
	sprintf (path, "%s.bsp", source);
	_printf ("Writing %s\n", path);
	WriteBSPFile (path);
}

