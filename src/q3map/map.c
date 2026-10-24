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
#include "../universal/com_math.h"
#include "../universal/q_parse.h"

int ParseVehicleCollMap(char **parsePtr, const char *vehicleName);
void Com_BeginParseSession(const char *filename);
void Com_EndParseSession(void);

#define MODEL_SCALE_EPSILON 0.001f
void LoadModelCollMap(char *collmapPath, const char *modelName, vec3_t origin, vec3_t angles, vec3_t scale);

#define PLANE_HASHES 1024
#define NORMAL_EPSILON 0.0000001
#define DIST_EPSILON 0.001

plane_t *planehash[PLANE_HASHES];
plane_t mapplanes[MAX_MAP_PLANES];
int nummapplanes;

#define TEXTURE_SCALE_EPSILON 0.000001f

void TextureAxisFromPlane( plane_t *pln, vec3_t xv, vec3_t yv );

vec3_t baseaxis[18] =
{
	{0,0,1}, {1,0,0}, {0,-1,0},
	{0,0,-1}, {1,0,0}, {0,-1,0},
	{1,0,0}, {0,1,0}, {0,0,-1},
	{-1,0,0}, {0,1,0}, {0,0,-1},
	{0,1,0}, {1,0,0}, {0,0,-1},
	{0,-1,0}, {1,0,0}, {0,0,-1}
};

vec3_t map_mins, map_maxs;
int numMapPatches;
int c_boxbevels, c_edgebevels, c_areaportals, c_detail, c_structural;
bspbrush_t *buildBrush;

void Com_BeginParseSession( const char *filename );
void Com_EndParseSession( void );
int FS_ReadFile( const char *qpath, void **buffer );
qboolean ParseMapEntity( char **parsePtr );
qboolean ParseCollMapEntity( char **parsePtr, vec3_t origin, vec3_t angles, vec3_t scale );
void ProcessMapModels( void );
void ProcessVehicleCollisionMaps( void );
void TestExpandBrushes( void );

int entitySourceBrushes;
int currentEntNum;
entity_t *mapent;

#define CONTENTS_CLIP_MASK (CONTENTS_MISSILECLIP | CONTENTS_VEHICLECLIP | CONTENTS_AI_NOSIGHT | CONTENTS_CLIPSHOT | CONTENTS_PLAYERCLIP | CONTENTS_MONSTERCLIP)

char *Com_Parse( char **data_p );
int Com_GetCurrentParseLine( void );
void Com_UngetToken( void );
void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up );
void MatrixInverse( const float in[3][3], float out[3][3] );
void MatrixTranspose( const float in[3][3], float out[3][3] );
void ParseBrush( char **data_p, float transformMtx[3][3], vec3_t origin );
void ParseTerrain( char **data_p );
void MoveBrushesToWorld( entity_t *ent, int cullGroup );

extern int numCullGroups;

void ParsePatch( char **data_p, qboolean terrain );
void AdjustBrushesForOrigin( entity_t *ent );
void SetTerrainTextures( void );

char mapIndexedShaders[MAX_MAP_BRUSHSIDES][MAX_QPATH];
int numMapIndexedShaders;

void VectorRotate3x3( vec3_t v, float r[3][3], vec3_t d );
int MapPlaneFromPoints( vec3_t p0, vec3_t p1, vec3_t p2 );
void QuakeTextureVecs( plane_t *plane, vec_t shift[2], vec_t rotate,
					   vec_t scale[2], float vecs[2][4] );

void SetBrushContents( bspbrush_t *b );

/*
================
PlaneEqual
================
*/
qboolean PlaneEqual( plane_t *p, vec3_t normal, vec_t dist )
{
	if ( fabs(p->normal[0] - normal[0]) < NORMAL_EPSILON
		&& fabs(p->normal[1] - normal[1]) < NORMAL_EPSILON
		&& fabs(p->normal[2] - normal[2]) < NORMAL_EPSILON
		&& fabs(p->dist - dist) < DIST_EPSILON )
		return qtrue;
	return qfalse;
}

/*
================
AddPlaneToHash
================
*/
void	AddPlaneToHash (plane_t *p)
{
	int		hash;

	hash = (int)fabs(p->dist) / 8;
	hash &= (PLANE_HASHES-1);

	p->hash_chain = planehash[hash];
	planehash[hash] = p;
}

#define MapVectorLength(v) ( (vec_t)sqrt( (v)[0] * (v)[0] + (v)[1] * (v)[1] + (v)[2] * (v)[2] ) )

/*
================
CreateNewFloatPlane
================
*/
int CreateNewFloatPlane (vec3_t normal, vec_t dist)
{
	plane_t	*p, temp;
	int		type;

	if (MapVectorLength(normal) < 0.5)
	{
		_printf( "FloatPlane: bad normal\n");
		return -1;
	}

	// create a new plane
	if (nummapplanes+2 > MAX_MAP_PLANES)
		Error ("MAX_MAP_PLANES");

	p = &mapplanes[nummapplanes];
	VectorCopy (normal, p->normal);
	p->dist = dist;

	// Classify axial normals before storing the plane pair.
	if (p->normal[0] == 1.0 || p->normal[0] == -1.0)
		type = PLANE_X;
	else if (p->normal[1] == 1.0 || p->normal[1] == -1.0)
		type = PLANE_Y;
	else if (p->normal[2] == 1.0 || p->normal[2] == -1.0)
		type = PLANE_Z;
	else
		type = PLANE_NON_AXIAL;
	p->type = (p+1)->type = type;

	VectorSubtract (vec3_origin, normal, (p+1)->normal);
	(p+1)->dist = -dist;

	nummapplanes += 2;

	// Always put axial planes facing positive first.
	if (p->type < PLANE_NON_AXIAL)
	{
		if (p->normal[0] < 0 || p->normal[1] < 0 || p->normal[2] < 0)
		{
			// flip order
			temp = *p;
			*p = *(p+1);
			*(p+1) = temp;

			AddPlaneToHash (p);
			AddPlaneToHash (p+1);
			return nummapplanes - 1;
		}
	}

	AddPlaneToHash (p);
	AddPlaneToHash (p+1);
	return nummapplanes - 2;
}

/*
==============
SnapVector
==============
*/
void	SnapVector (vec3_t normal)
{
	int		i;

	for (i=0 ; i<3 ; i++)
	{
		if ( fabs(normal[i] - 1) < NORMAL_EPSILON )
		{
			VectorClear (normal);
			normal[i] = 1;
			break;
		}
		if ( fabs(normal[i] - -1) < NORMAL_EPSILON )
		{
			VectorClear (normal);
			normal[i] = -1;
			break;
		}
	}
}

/*
==============
SnapPlane
==============
*/
void SnapPlane( vec3_t normal, vec_t *dist )
{
	SnapVector( normal );
	if ( fabs(*dist - Q_rint(*dist)) < DIST_EPSILON )
		*dist = Q_rint( *dist );
}

/*
=============
FindFloatPlane

=============
*/
int FindFloatPlane( vec3_t normal, vec_t dist )
{
	int i, hash, h;
	plane_t *p;

	SnapPlane( normal, &dist );
	hash = (int)fabs(dist) / 8;
	hash &= PLANE_HASHES - 1;
	for ( i = -1 ; i <= 1 ; i++ )
	{
		h = (hash + i) & (PLANE_HASHES - 1);
		for ( p = planehash[h] ; p ; p = p->hash_chain )
			if ( PlaneEqual( p, normal, dist ) )
				return p - mapplanes;
	}
	return CreateNewFloatPlane( normal, dist );
}

/*
================
MapPlaneFromPoints
================
*/
int MapPlaneFromPoints( vec3_t p0, vec3_t p1, vec3_t p2 )
{
	vec3_t t1, t2, normal;
	vec_t dist;

	VectorSubtract( p0, p1, t1 );
	VectorSubtract( p2, p1, t2 );
	CrossProduct( t1, t2, normal );
	VectorNormalize( normal );
	dist = DotProduct( p0, normal );
	return FindFloatPlane( normal, dist );
}

/*
===========
SetBrushContents

The contents on all sides of a brush should be the same
Sets contentsShader, contents, opaque, and detail
===========
*/
void SetBrushContents( bspbrush_t *b ) {
	int contents, c2;
	side_t *s;
	int i;
	qboolean mixed;
	int allFlags;
	s = &b->sides[0];
	contents = s->contents;
	b->contentShader = s->shaderInfo;
	mixed = qfalse;
	allFlags = s->surfaceFlags;
	b->isOccluder = qfalse;
	for ( i = 1; i < b->numsides; i++, s++ ) {
		s = &b->sides[i];
		if ( !s->shaderInfo ) continue;
		c2 = s->contents;
		if ( c2 != contents ) mixed = qtrue;
		allFlags |= s->surfaceFlags;
		b->isOccluder |= s->shaderInfo->isOccluder;
	}
	if ( mixed ) qprintf("Entity %i, Brush %i: mixed face contents\n", b->entitynum, b->brushnum);
	if ( (contents & CONTENTS_DETAIL) && (contents & CONTENTS_STRUCTURAL) ) {
		_printf("Entity %i, Brush %i: mixed CONTENTS_DETAIL and CONTENTS_STRUCTURAL\n", num_entities-1, entitySourceBrushes);
		contents &= ~CONTENTS_DETAIL;
	}
	if ( fulldetail ) contents &= ~CONTENTS_DETAIL;
	if ( (contents & CONTENTS_TRANSLUCENT) && !(contents & CONTENTS_STRUCTURAL) && !(allFlags & SURF_HINT) ) contents |= CONTENTS_DETAIL;
	if ( contents & CONTENTS_DETAIL ) {
		c_detail++;
		b->detail = qtrue;
	} else {
		c_structural++;
		b->detail = qfalse;
	}
	if ( contents & CONTENTS_TRANSLUCENT ) {
		b->opaque = qfalse;
	} else {
		b->opaque = qtrue;
	}
	b->noBspOutput = qfalse;
	/* Ignore lighting/footstep modifiers when recognizing a hidden portal. */
	if ( contents & CONTENTS_NONCOLLIDING ) {
		b->noBspOutput = qtrue;
	} else if ( !b->opaque ) {
		if ( ( allFlags & ~( SURF_NODAMAGE | SURF_NOIMPACT | SURF_NOMARKS | SURF_HINT | SURF_NOLIGHTMAP | SURF_POINTLIGHT | SURF_NOSTEPS ) ) == ( SURF_PORTAL | SURF_NONSOLID | SURF_NODRAW ) || b->isOccluder ) b->noBspOutput = qtrue;
	}
	if ( contents & CONTENTS_AREAPORTAL ) c_areaportals++;
	b->contents = contents;
}

/*
=================
AddBrushBevels

Adds any additional planes necessary to allow the brush being
built to be expanded against axial bounding boxes
=================
*/
void AddBrushBevels( void ) {
	int		axis, dir;
	int		i, order;
	int		sideIndex;
	side_t	sidetemp;
	side_t	*s;
	vec3_t	normal;
	float	dist;
	shaderInfo_t *bevelShader;


	//
	// add the axial planes
	//
	order = 0;
	for (axis=0 ; axis <3 ; axis++)
	{
		for (dir=-1 ; dir <= 1 ; dir+=2, order++)
		{
			// see if the plane is allready present
			for ( i=0, s=buildBrush->sides ; i < buildBrush->numsides ; i++,s++ ) {
				if (mapplanes[s->planenum].normal[axis] == dir)
					break;
			}

			if (i == buildBrush->numsides )
			{	// add a new side
				if ( buildBrush->numsides == MAX_BUILD_SIDES ) {
					Error( "MAX_BUILD_SIDES" );
				}
				memset( s, 0, sizeof( *s ) );
				VectorClear (normal);
				normal[axis] = dir;
				if (dir == 1)
					dist = buildBrush->maxs[axis];
				else
					dist = -buildBrush->mins[axis];
				s->planenum = FindFloatPlane (normal, dist);
                {
                    int bestSide;
                    float bestDot, dot;
                    bevelShader = NULL;
                    bestSide = -1;
                    bestDot = -1;
                    for (sideIndex = 0; sideIndex < buildBrush->numsides; sideIndex++) {
                        if (!buildBrush->sides[sideIndex].shaderInfo) continue;
                        if (!strcmp("1", ValueForKey(mapent, "terrain")) &&
                            (buildBrush->sides[sideIndex].shaderInfo->surfaceFlags & SURF_NODRAW) &&
                            !strstr(buildBrush->sides[sideIndex].shaderInfo->shader, "terrain")) continue;
                        dot = normal[0] * mapplanes[buildBrush->sides[sideIndex].planenum].normal[0];
                        dot += normal[1] * mapplanes[buildBrush->sides[sideIndex].planenum].normal[1];
                        dot += normal[2] * mapplanes[buildBrush->sides[sideIndex].planenum].normal[2];
                        if (dot > bestDot) {
                            bestDot = dot;
                            bestSide = sideIndex;
                        }
                    }
                    if (bestSide >= 0) bevelShader = buildBrush->sides[bestSide].shaderInfo;
                    buildBrush->numsides++;
                    s->contents = buildBrush->sides[0].contents;
                    if (bevelShader) s->shaderInfo = bevelShader;
                }
				s->bevel = qtrue;
				c_boxbevels++;
			}

			// if the plane is not in it canonical order, swap it
			if (i != order)
			{
				sidetemp = buildBrush->sides[order];
				buildBrush->sides[order] = buildBrush->sides[i];
				buildBrush->sides[i] = sidetemp;
			}
		}
	}

	//
	// add the edge bevels
	//
	if ( buildBrush->numsides == 6 ) {
		return;		// pure axial
  } else {
	  int			j, k, l;
	  float		d;
	  winding_t	*w, *w2;
	  side_t		*s2;
	  vec3_t		vec, vec2;

	  // test the non-axial plane edges
	  // this code tends to cause some problems...
	  for (i=6 ; i<buildBrush->numsides ; i++)
	  {
		  s = buildBrush->sides + i;
		  w = s->winding;
		  if (!w)
			  continue;
		  for (j=0 ; j<w->numpoints ; j++)
		  {
			  k = (j+1)%w->numpoints;
			  VectorSubtract (w->p[j], w->p[k], vec);
			  if (VectorNormalize (vec) < 0.5)
				  continue;
			  SnapVector (vec);
			  for (k=0 ; k<3 ; k++)
				  if ( vec[k] == -1 || vec[k] == 1)
					  break;	// axial
			  if (k != 3)
				  continue;	// only test non-axial edges

			  // try the six possible slanted axials from this edge
			  for (axis=0 ; axis <3 ; axis++)
			  {
				  for (dir=-1 ; dir <= 1 ; dir+=2)
				  {
					  // construct a plane
					  VectorClear (vec2);
					  vec2[axis] = dir;
					  CrossProduct (vec, vec2, normal);
					  if (VectorNormalize (normal) < 0.5)
						  continue;
					  dist = DotProduct (w->p[j], normal);
                      SnapPlane(normal, &dist);

					  // if all the points on all the sides are
					  // behind this plane, it is a proper edge bevel
					  for (k=0 ; k < buildBrush->numsides ; k++)
					  {
						  // if this plane has allready been used, skip it
						  if (PlaneEqual (&mapplanes[buildBrush->sides[k].planenum]
							  , normal, dist) )
							  break;

						  w2 = buildBrush->sides[k].winding;
						  if (!w2)
							  continue;
						  for (l=0 ; l<w2->numpoints ; l++)
						  {
							  d = DotProduct (w2->p[l], normal) - dist;
							  if (d > 0.1)
								  break;	// point in front
						  }
						  if (l != w2->numpoints)
							  break;
					  }

					  if (k != buildBrush->numsides)
						  continue;	// wasn't part of the outer hull
					  // add this plane
					  if ( buildBrush->numsides == MAX_BUILD_SIDES ) {
						  Error( "MAX_BUILD_SIDES" );
					  }

					  s2 = &buildBrush->sides[buildBrush->numsides];
                      {
                          int bestSide;
                          float bestDot, dot;
                          bevelShader = NULL;
                          bestSide = -1;
                          bestDot = -1;
                          for (sideIndex = 0; sideIndex < buildBrush->numsides; sideIndex++) {
                              if (!buildBrush->sides[sideIndex].shaderInfo) continue;
                              if (!strcmp("1", ValueForKey(mapent, "terrain")) &&
                                  (buildBrush->sides[sideIndex].shaderInfo->surfaceFlags & SURF_NODRAW) &&
                                  !strstr(buildBrush->sides[sideIndex].shaderInfo->shader, "terrain")) continue;
                              dot = DotProduct(normal, mapplanes[buildBrush->sides[sideIndex].planenum].normal);
                              if (dot > bestDot) {
                                  bestDot = dot;
                                  bestSide = sideIndex;
                              }
                          }
                          if (bestSide >= 0) bevelShader = buildBrush->sides[bestSide].shaderInfo;
                          buildBrush->numsides++;
                          memset( s2, 0, sizeof( *s2 ) );
                          s2->planenum = FindFloatPlane (normal, dist);
                          s2->contents = buildBrush->sides[0].contents;
                          s2->bevel = qtrue;
                          if (bevelShader) s2->shaderInfo = bevelShader;
                      }

					  c_edgebevels++;
				  }
			  }
		  }
	  }
  }
}

/*
===============
AddBackSides

fog volumes need to have inside faces created
===============
*/
void AddBackSides( void ) {
}

/*
===============
FinishBrush

Produces a final brush based on the buildBrush->sides array
and links it to the current entity
===============
*/
bspbrush_t *FinishBrush( void ) {
	bspbrush_t	*b;

	// create windings for sides and bounds for brush
	if ( !CreateBrushWindings( buildBrush ) ) {
		// don't keep this brush
		return NULL;
	}

	// brushes that will not be visible at all are forced to be detail
	if ( buildBrush->contents & CONTENTS_CLIP_MASK )
	{
		buildBrush->detail = qtrue;
		c_detail++;
	}

	//
	// origin brushes are removed, but they set
	// the rotation origin for the rest of the brushes
	// in the entity.  After the entire entity is parsed,
	// the planenums and texinfos will be adjusted for
	// the origin brush
	//
	if ( buildBrush->contents & CONTENTS_ORIGIN )
	{
		char	string[32];
		vec3_t	origin;

		if (num_entities == 1) {
			_printf ("Entity %i, Brush %i: origin brushes not allowed in world\n"
				,  num_entities - 1, entitySourceBrushes);
			return NULL;
		}

		VectorAdd (buildBrush->mins, buildBrush->maxs, origin);
		VectorScale (origin, 0.5f, origin);

		sprintf (string, "%i %i %i", (int)origin[0], (int)origin[1], (int)origin[2]);
		SetKeyValue (&entities[num_entities - 1], "origin", string);

		VectorCopy (origin, entities[num_entities - 1].origin);

		// don't keep this brush
		return NULL;
	}

	if ( buildBrush->contents & CONTENTS_AREAPORTAL ) {
		if (num_entities != 1) {
			_printf ("Entity %i, Brush %i: areaportals only allowed in world\n"
				,  num_entities - 1, entitySourceBrushes);
			return NULL;
		}
	}

	AddBrushBevels ();

	// keep it
	b = CopyBrush( buildBrush );

	b->entitynum = currentEntNum-1;
	b->brushnum = entitySourceBrushes;

	b->original = b;

	b->next = mapent->brushes;
	mapent->brushes = b;

	return b;
}

/*
=============
TextureAxisFromPlane
=============
*/
void TextureAxisFromPlane( plane_t *pln, vec3_t xv, vec3_t yv )
{
	int bestaxis, i;
	vec_t dot, best;

	best = 0;
	bestaxis = 0;
	for ( i = 0 ; i < 6 ; i++ )
	{
		dot = DotProduct( pln->normal, baseaxis[i*3] );
		if ( dot > best )
		{
			best = dot;
			bestaxis = i;
		}
	}
	VectorCopy( baseaxis[bestaxis*3+1], xv );
	VectorCopy( baseaxis[bestaxis*3+2], yv );
}

/*
=================
QuakeTextureVecs

Creates world-to-texture mapping vecs for crappy quake plane arrangements
=================
*/
void QuakeTextureVecs( plane_t *plane, vec_t shift[2], vec_t rotate,
	vec_t scale[2], float mappingVecs[2][4] )
{
	vec3_t vecs[2];
	int sv, tv, i, j;
	vec_t ang, sinv, cosv, ns, nt;

	TextureAxisFromPlane( plane, vecs[0], vecs[1] );
	if ( fabs(scale[0]) < TEXTURE_SCALE_EPSILON )
		scale[0] = 1;
	if ( fabs(scale[1]) < TEXTURE_SCALE_EPSILON )
		scale[1] = 1;
	if ( rotate == 0 )
		{ sinv = 0; cosv = 1; }
	else if ( rotate == 90 )
		{ sinv = 1; cosv = 0; }
	else if ( rotate == 180 )
		{ sinv = 0; cosv = -1; }
	else if ( rotate == 270 )
		{ sinv = -1; cosv = 0; }
	else
	{
		ang = rotate * (vec_t)(Q_PI / 180.0);
		sinv = sin( ang );
		cosv = cos( ang );
	}
	if ( vecs[0][0] )
		sv = 0;
	else if ( vecs[0][1] )
		sv = 1;
	else
		sv = 2;
	if ( vecs[1][0] )
		tv = 0;
	else if ( vecs[1][1] )
		tv = 1;
	else
		tv = 2;
	for ( i = 0 ; i < 2 ; i++ )
	{
		ns = cosv * vecs[i][sv] - sinv * vecs[i][tv];
		nt = sinv * vecs[i][sv] + cosv * vecs[i][tv];
		vecs[i][sv] = ns;
		vecs[i][tv] = nt;
	}
	for ( i = 0 ; i < 2 ; i++ )
		for ( j = 0 ; j < 3 ; j++ )
			mappingVecs[i][j] = vecs[i][j] / scale[i];
	mappingVecs[0][3] = shift[0];
	mappingVecs[1][3] = shift[1];
}

/*
=================
ParseRawBrush

Just parses the sides into buildBrush->sides[], nothing else.
no validation, back plane removal, etc.

Timo - 08/26/99
added brush epairs parsing ( ignoring actually )
Timo - 08/04/99
added exclusive brush primitive parsing
Timo - 08/08/99
support for old brush format back in
NOTE : it would be "cleaner" to have seperate functions to parse between old and new brushes
=================
*/
void ParseRawBrush( char **data_p, float transformMtx[3][3], vec3_t origin ) {
	side_t		*side;
	float		planeDist;
	double		normalLength;
	vec3_t		transformedNormal;
	vec_t		rotate;
	vec_t		shift[2];
	vec_t		scale[2];
	vec3_t		planepts[3];
	vec4_t		plane;
	char		name[MAX_QPATH];
	char		shader[MAX_QPATH];
	int			planenum;
	shaderInfo_t	*si;
	int			flags;
	const char	*parseToken;

	buildBrush->numsides = 0;
	buildBrush->detail = qfalse;

	if ( g_bBrushPrimit == BPRIMIT_NEWBRUSHES ) {
		Com_MatchToken( data_p, "{", qfalse );
	}

	for ( parseToken = Com_Parse( data_p ); parseToken[0]; parseToken = Com_Parse( data_p ) ) {
		if ( !strcmp( parseToken, "}" ) ) {
			break;
		}

		if ( g_bBrushPrimit == BPRIMIT_NEWBRUSHES ) {
			while ( strcmp( parseToken, "(" ) ) {
				Com_ParseOnLine( data_p );
				parseToken = Com_Parse( data_p );
			}
		}
		Com_UngetToken();

		if ( buildBrush->numsides == MAX_BUILD_SIDES ) {
			Error( "MAX_BUILD_SIDES" );
		}

		side = &buildBrush->sides[ buildBrush->numsides ];
		memset( side, 0, sizeof( *side ) );
		buildBrush->numsides++;

		Com_Parse1DMatrix( data_p, 3, planepts[0] );
		Com_Parse1DMatrix( data_p, 3, planepts[1] );
		Com_Parse1DMatrix( data_p, 3, planepts[2] );

		if ( g_bBrushPrimit == BPRIMIT_NEWBRUSHES ) {
			Com_Parse2DMatrix( data_p, 2, 3, side->texMat[0] );
		}

		strcpy( name, Com_ParseOnLine( data_p ) );

		if ( numMapIndexedShaders == MAX_MAP_BRUSHSIDES ) {
			Error( "MAX_MAP_BRUSHSIDES" );
		}
		strcpy( mapIndexedShaders[numMapIndexedShaders], name );
		numMapIndexedShaders++;

		if ( g_bBrushPrimit == BPRIMIT_OLDBRUSHES ) {
			shift[0] = atoi( Com_ParseOnLine( data_p ) );
			shift[1] = atoi( Com_ParseOnLine( data_p ) );
			rotate = atoi( Com_ParseOnLine( data_p ) );
			scale[0] = atof( Com_ParseOnLine( data_p ) );
			scale[1] = atof( Com_ParseOnLine( data_p ) );
		}

		sprintf( shader, "textures/%s", name );
		si = ShaderInfoForShader( shader );
		side->shaderInfo = si;
		side->surfaceFlags = si->surfaceFlags;
		side->value = si->value;
		side->contents = si->contents;

		parseToken = Com_ParseOnLine( data_p );
		if ( parseToken ) {
			flags = atoi( parseToken );
			if ( flags & CONTENTS_DETAIL ) {
				side->contents |= CONTENTS_DETAIL;
			}
			if ( flags & CONTENTS_NONCOLLIDING ) {
				side->contents |= CONTENTS_NONCOLLIDING;
			}
			if ( flags & ( CONTENTS_CLIPSHOT | CONTENTS_MISSILECLIP ) ) {
				side->contents &= ~( CONTENTS_DETAIL | CONTENTS_SOLID );
			}
			if ( flags & CONTENTS_CLIPSHOT ) {
				side->contents |= CONTENTS_CLIPSHOT;
			}
			if ( flags & CONTENTS_MISSILECLIP ) {
				side->contents |= CONTENTS_MISSILECLIP;
			}

			Com_ParseOnLine( data_p );
			Com_ParseOnLine( data_p );
			side->lightmapSampleSize = atoi( Com_ParseOnLine( data_p ) );
		}

		if ( transformMtx ) {
			PlaneFromPoints( plane, planepts[0], planepts[1], planepts[2] );
			VectorRotate3x3( plane, transformMtx, transformedNormal );
			normalLength = VectorNormalize( transformedNormal );
			planeDist = DotProduct( transformedNormal, origin ) + plane[3] / normalLength;
			planenum = FindFloatPlane( transformedNormal, planeDist );
		} else {
			planenum = MapPlaneFromPoints( planepts[0], planepts[1], planepts[2] );
		}
		side->planenum = planenum;

		if ( g_bBrushPrimit == BPRIMIT_OLDBRUSHES ) {
			QuakeTextureVecs( &mapplanes[planenum], shift, rotate, scale, side->vecs );
		}
	}

	if ( g_bBrushPrimit == BPRIMIT_NEWBRUSHES ) {
		Com_UngetToken();
		Com_MatchToken( data_p, "}", qfalse );
		Com_MatchToken( data_p, "}", qfalse );
	}
}

/*
=================
RemoveDuplicateBrushPlanes

Returns false if the brush has a mirrored set of planes,
meaning it encloses no volume.
Also removes planes without any normal
=================
*/
qboolean RemoveDuplicateBrushPlanes( bspbrush_t * b ) {
	int			i, j, k;
	side_t		*sides;

	sides = b->sides;

	for ( i = 1 ; i < b->numsides ; i++ ) {

		// check for a degenerate plane
		if ( sides[i].planenum == -1) {
			_printf ("Entity %i, Brush %i: degenerate plane\n"
				, b->entitynum, b->brushnum);
			// remove it
			for ( k = i + 1 ; k < b->numsides ; k++ ) {
				sides[k-1] = sides[k];
			}
			b->numsides--;
			i--;
			continue;
		}

		// check for duplication and mirroring
		for ( j = 0 ; j < i ; j++ ) {
			if ( sides[i].planenum == sides[j].planenum ) {
				_printf ("Entity %i, Brush %i: duplicate plane\n"
					, b->entitynum, b->brushnum);
				// remove the second duplicate
				for ( k = i + 1 ; k < b->numsides ; k++ ) {
					sides[k-1] = sides[k];
				}
				b->numsides--;
				i--;
				break;
			}

			if ( sides[i].planenum == (sides[j].planenum ^ 1) ) {
				// mirror plane, brush is invalid
				_printf ("Entity %i, Brush %i: mirrored plane\n"
					, b->entitynum, b->brushnum);
				return qfalse;
			}
		}
	}
	return qtrue;
}

/*
=================
ParseBrush

  qboolean parameter to true -> parse new brush primitive format ( else use old format )
=================
*/
void ParseBrush( char **data_p, float transformMtx[3][3], vec3_t origin ) {
	bspbrush_t	*b;

	ParseRawBrush( data_p, transformMtx, origin );

	buildBrush->portalareas[0] = -1;
	buildBrush->portalareas[1] = -1;
	buildBrush->entitynum = currentEntNum - 1;
	buildBrush->brushnum = entitySourceBrushes;

	if ( !RemoveDuplicateBrushPlanes( buildBrush ) ) {
		return;
	}

	SetBrushContents( buildBrush );

	if ( nodetail && ( buildBrush->contents & CONTENTS_DETAIL ) ) {
		FreeBrush( buildBrush );
		return;
	}

	if ( nowater && ( buildBrush->contents & CONTENTS_WATER ) ) {
		FreeBrush( buildBrush );
		return;
	}

	b = FinishBrush();
	if ( !b ) {
		return;
	}
}

/*
================
MoveBrushesToWorld

Takes all of the brushes from the current entity and
adds them to the world's brush list.

Used by func_group
================
*/
void MoveBrushesToWorld(entity_t *ent, int cullGroup)
{
	bspbrush_t *b, *next;
	parseMesh_t *pm;

	for (b = ent->brushes; b; b = next)
	{
		next = b->next;
		b->cullgroup = cullGroup;
		b->next = entities[0].brushes;
		entities[0].brushes = b;
	}
	ent->brushes = NULL;
	if (ent->patches)
	{
		for (pm = ent->patches; pm->next; pm = pm->next)
			pm->cullgroup = cullGroup;
		pm->cullgroup = cullGroup;
		pm->next = entities[0].patches;
		entities[0].patches = ent->patches;
		ent->patches = NULL;
	}
}

/*
================
AdjustBrushesForOrigin
================
*/
void AdjustBrushesForOrigin(entity_t *ent)
{
	bspbrush_t *b;
	int i;
	side_t *s;
	vec_t newdist;
	parseMesh_t *p;

	for (b = ent->brushes; b; b = b->next)
	{
		for (i = 0; i < b->numsides; i++)
		{
			s = &b->sides[i];
			newdist = mapplanes[s->planenum].dist -
				DotProduct(mapplanes[s->planenum].normal, ent->origin);
			s->planenum = FindFloatPlane(mapplanes[s->planenum].normal, newdist);
			s->vecs[0][3] += DotProduct(s->vecs[0], ent->origin);
			s->vecs[1][3] += DotProduct(s->vecs[1], ent->origin);
		}
		CreateBrushWindings(b);
	}
	for (p = ent->patches; p; p = p->next)
		for (i = 0; i < p->mesh.width * p->mesh.height; i++)
			VectorSubtract(p->mesh.verts[i].xyz, ent->origin, p->mesh.verts[i].xyz);
}

/*
================
ParseMapEntity
================
*/
qboolean ParseMapEntity( char **data_p ) {
	epair_t		*e;
	char		*token;

	token = Com_Parse( data_p );
	if ( !token[0] ) {
		return qfalse;
	}

	if ( strcmp( token, "{" ) ) {
		Error( "ParseEntity: { not found, found %s on line %d - last entity was at: <%4.2f, %4.2f, %4.2f>...",
			token, Com_GetCurrentParseLine(), entities[num_entities].origin[0], entities[num_entities].origin[1], entities[num_entities].origin[2] );
	}

	if ( num_entities == MAX_MAP_ENTITIES ) {
		Error( "num_entities == MAX_MAP_ENTITIES" );
	}

	entitySourceBrushes = 0;
	mapent = &entities[num_entities];
	currentEntNum++;
	num_entities++;
	memset( mapent, 0, sizeof( *mapent ) );

	do {
		token = Com_Parse( data_p );
		if ( !token[0] ) {
			Error( "ParseEntity: EOF without closing brace" );
		}
		if ( !strcmp( token, "}" ) ) {
			break;
		}

		if ( !strcmp( token, "{" ) ) {
			token = Com_Parse( data_p );
			if ( !token[0] ) {
				break;
			}

			if ( !strcmp( token, "patchDef5" ) ) {
				numMapPatches++;
				ParsePatch( data_p, qfalse );
			} else if ( !strcmp( token, "patchTerrainDef3" ) ) {
				numMapPatches++;
				ParsePatch( data_p, qtrue );
			} else if ( !strcmp( token, "terrainDef" ) ) {
				ParseTerrain( data_p );
			} else {
				if ( !strcmp( token, "brushDef" ) ) {
					if ( g_bBrushPrimit == BPRIMIT_OLDBRUSHES ) {
						Error( "old brush format not allowed in new brush format map" );
					}
					g_bBrushPrimit = BPRIMIT_NEWBRUSHES;
				} else {
					if ( g_bBrushPrimit == BPRIMIT_NEWBRUSHES ) {
						Error( "new brush format not allowed in old brush format map" );
					}
					g_bBrushPrimit = BPRIMIT_OLDBRUSHES;
					Com_UngetToken();
				}
				ParseBrush( data_p, NULL, NULL );
			}
			entitySourceBrushes++;
		} else {
			e = ParseEpair( token, data_p );
			e->next = mapent->epairs;
			mapent->epairs = e;
		}
	} while ( 1 );

	GetVectorForKey( mapent, "origin", mapent->origin );

	if ( mapent->origin[0] || mapent->origin[1] || mapent->origin[2] ) {
		AdjustBrushesForOrigin( mapent );
	}

	if ( !strcmp( "group_info", ValueForKey( mapent, "classname" ) ) ) {
		num_entities--;
		return qtrue;
	}

	if ( !strcmp( "func_group", ValueForKey( mapent, "classname" ) ) ) {
		if ( !strcmp( "1", ValueForKey( mapent, "terrain" ) ) ) {
			SetTerrainTextures();
		}
		MoveBrushesToWorld( mapent, -1 );
		num_entities--;
		return qtrue;
	}

	if ( !strcmp( "func_cullgroup", ValueForKey( mapent, "classname" ) ) ) {
		if ( numCullGroups >= MAX_MAP_CULLGROUPS ) {
			Error( "MAX_MAP_CULLGROUPS (%i) exceeded\n", MAX_MAP_CULLGROUPS );
		}
		MoveBrushesToWorld( mapent, numCullGroups );
		num_entities--;
		numCullGroups++;
	}

	return qtrue;
}

/*
=============
ParseCollMapEntity
=============
*/
qboolean ParseCollMapEntity( char **data_p, vec3_t origin, vec3_t angles, vec3_t scale ) {
	epair_t		*e;
	char		*token;
	vec3_t		axis[3];
	float		inverse[3][3];

	token = Com_Parse( data_p );
	if ( !token[0] ) {
		return qfalse;
	}

	if ( strcmp( token, "{" ) ) {
		Error( "ParseCollMapEntity: { not found, found %s on line %d", token, Com_GetCurrentParseLine() );
	}

	if ( num_entities == MAX_MAP_ENTITIES ) {
		Error( "num_entities == MAX_MAP_ENTITIES" );
	}

	entitySourceBrushes = 0;
	mapent = &entities[num_entities];
	memset( mapent, 0, sizeof( *mapent ) );

	AngleVectors( angles, axis[0], axis[1], axis[2] );
	VectorScale( axis[0], scale[0], axis[0] );
	VectorScale( axis[1], scale[1], axis[1] );
	VectorScale( axis[2], scale[2], axis[2] );
	VectorNegate( axis[1] );
	MatrixInverse( axis, inverse );
	MatrixTranspose( inverse, axis );

	do {
		token = Com_Parse( data_p );
		if ( !token[0] ) {
			Error( "ParseEntity: EOF without closing brace" );
		}
		if ( !strcmp( token, "}" ) ) {
			break;
		}

		if ( !strcmp( token, "{" ) ) {
			token = Com_Parse( data_p );
			if ( !token[0] ) {
				break;
			}

			if ( !strcmp( token, "patchDef4" ) ) {
				numMapPatches++;
				Error( "cannot have curves in collision maps" );
			} else if ( !strcmp( token, "patchTerrainDef2" ) ) {
				numMapPatches++;
				Error( "cannot have patch terrain in collision maps" );
			} else if ( !strcmp( token, "terrainDef" ) ) {
				ParseTerrain( data_p );
				Error( "cannot have brush terrain in collision maps" );
			} else {
				if ( !strcmp( token, "brushDef" ) ) {
					if ( g_bBrushPrimit == BPRIMIT_OLDBRUSHES ) {
						Error( "old brush format not allowed in new brush format map" );
					}
					g_bBrushPrimit = BPRIMIT_NEWBRUSHES;
					ParseBrush( data_p, axis, origin );
				} else {
					if ( g_bBrushPrimit == BPRIMIT_NEWBRUSHES ) {
						Error( "new brush format not allowed in old brush format map" );
					}
					g_bBrushPrimit = BPRIMIT_OLDBRUSHES;
					Com_UngetToken();
					ParseBrush( data_p, axis, origin );
				}
			}
			entitySourceBrushes++;
		} else {
			e = ParseEpair( token, data_p );
			e->next = mapent->epairs;
			mapent->epairs = e;
		}
	} while ( 1 );

	MoveBrushesToWorld( mapent, -1 );
	return qtrue;
}

/*
=============
LoadModelCollMap
=============
*/
void LoadModelCollMap( char *collmapPath, const char *modelName, vec3_t origin, vec3_t angles, vec3_t scale ) {
	void	*fileData;
	char	*parsePtr;

	if ( FS_ReadFile( collmapPath, &fileData ) < 0 ) {
		_printf( "WARNING: No collision map file for misc_model \"%s\"\n", modelName );
		return;
	}

	Com_BeginParseSession( collmapPath );
	parsePtr = fileData;
	while ( ParseCollMapEntity( &parsePtr, origin, angles, scale ) ) {
	}
	Com_EndParseSession();
}

/*
=============
ProcessMapModels
=============
*/
void ProcessMapModels(void)
{
	int entity_num, i;
	entity_t *entity;
	const char *model;
	char name[MAX_OS_PATH], collmapPath[MAX_OS_PATH];
	vec3_t origin, angles, scale, scaleVec;
	vec_t modelscale;

	for (entity_num = 1; entity_num < num_entities; entity_num++)
	{
		entity = &entities[entity_num];
		if (Q_stricmp("misc_model", ValueForKey(entity, "classname")))
			continue;
		model = ValueForKey(entity, "model");
		if (!model[0])
			continue;
		ExtractFileName(model, name);
		sprintf(collmapPath, "collmaps/%s.map", name);
		GetVectorForKey(entity, "origin", origin);
		GetVectorForKey(entity, "angles", angles);
		scale[0] = scale[1] = scale[2] = 1;
		modelscale = FloatForKey(entity, "modelscale");
		if (modelscale > 0)
			scale[0] = scale[1] = scale[2] = modelscale;
		GetVectorForKey(entity, "modelscale_vec", scaleVec);
		if ((vec_t)sqrt(DotProduct(scaleVec, scaleVec)) > 0)
			VectorCopy(scaleVec, scale);
		for (i = 0; i < 3; i++)
			if (scale[i] * scale[i] < MODEL_SCALE_EPSILON * MODEL_SCALE_EPSILON)
				Error("entity %i: near-zero scale on at least one axis (scale = %g %g %g)\n", entity_num, scale[0], scale[1], scale[2]);
		LoadModelCollMap(collmapPath, model, origin, angles, scale);
	}
}

/*
=============
ParseVehicleCollMap
=============
*/
int ParseVehicleCollMap( char **parsePtr, const char *vehicleName ) {
	char	*token;

	token = Com_Parse( parsePtr );
	if ( !token[0] ) {
		return 0;
	}
	if ( strcmp( token, "{" ) ) {
		Error( "ParseVehicleCollMap: { not found, found %s on line %d", token, Com_GetCurrentParseLine() );
	}

	if ( num_entities == MAX_MAP_ENTITIES ) {
		Error( "num_entities == MAX_MAP_ENTITIES" );
	}

	entitySourceBrushes = 0;
	mapent = &entities[num_entities];
	memset( mapent, 0, sizeof( *mapent ) );
	currentEntNum++;
	num_entities++;

	do {
		token = Com_Parse( parsePtr );
		if ( !token[0] ) {
			Error( "ParseVehicleCollMap: EOF without closing brace" );
		}
		if ( !strcmp( token, "}" ) ) {
			break;
		}

		if ( !strcmp( token, "{" ) ) {
			token = Com_Parse( parsePtr );
			if ( !token[0] ) {
				break;
			}

			if ( !strcmp( token, "patchDef4" ) ) {
				Error( "cannot have curves in collision maps" );
			} else if ( !strcmp( token, "patchTerrainDef2" ) ) {
				Error( "cannot have patch terrain in collision maps" );
			} else if ( !strcmp( token, "terrainDef" ) ) {
				Error( "cannot have brush terrain in collision maps" );
			} else {
				if ( !strcmp( token, "brushDef" ) ) {
					if ( g_bBrushPrimit == BPRIMIT_OLDBRUSHES ) {
						Error( "old brush format not allowed in new brush format map" );
					}
					g_bBrushPrimit = BPRIMIT_NEWBRUSHES;
				} else {
					if ( g_bBrushPrimit == BPRIMIT_NEWBRUSHES ) {
						Error( "new brush format not allowed in old brush format map" );
					}
					g_bBrushPrimit = BPRIMIT_OLDBRUSHES;
					Com_UngetToken();
				}
				ParseBrush( parsePtr, NULL, NULL );
			}
			entitySourceBrushes++;
		} else {
			ParseEpair( token, parsePtr );
		}
	} while ( 1 );

	SetKeyValue( &entities[num_entities - 1], "targetname", vehicleName );
	SetKeyValue( &entities[num_entities - 1], "classname", "script_vehicle_collmap" );
	return 1;
}

/*
=============
ProcessVehicleCollisionMaps
=============
*/
void ProcessVehicleCollisionMaps(void)
{
	int entity_num, i;
	entity_t *entity;
	const char *model;
	char name[MAX_OS_PATH], collmapPath[MAX_OS_PATH];
	void *fileData;
	char *parsePtr;

	for (entity_num = 1; entity_num < num_entities; entity_num++)
	{
		entity = &entities[entity_num];
		if (Q_stricmp("script_vehicle", ValueForKey(entity, "classname")))
			continue;
		model = ValueForKey(entity, "model");
		if (!model[0])
			continue;
		ExtractFileName(model, name);
		sprintf(collmapPath, "%s%s.map", ExpandGamePath("collmaps/"), name);
		for (i = 0; i < num_entities; i++)
		{
			if (entity_num == i)
				continue;
			if (!Q_stricmp("script_vehicle_collmap", ValueForKey(&entities[i], "classname")) &&
				!Q_stricmp(model, ValueForKey(&entities[i], "targetname")))
				break;
		}
		if (i != num_entities)
			continue;
		if (LoadFile(collmapPath, &fileData) < 0)
		{
			_printf("WARNING: No collision map file for script_vehicle \"%s\"\n", name);
			continue;
		}
		Com_BeginParseSession(collmapPath);
		parsePtr = fileData;
		ParseVehicleCollMap(&parsePtr, model);
		Com_EndParseSession();
	}
}

/*
================
TestExpandBrushes

Expands all the brush planes and saves a new map out to
allow visual inspection of the clipping bevels
================
*/
void TestExpandBrushes( void ) {
	side_t	*s;
	int		i, j;
	bspbrush_t	*brush, *list, *copy;
	vec_t	dist;
	plane_t		*plane;

	list = NULL;

	for ( brush = entities[0].brushes ; brush ; brush = brush->next ) {
		copy = CopyBrush( brush );
		copy->next = list;
		list = copy;

		// expand all the planes
		for ( i=0 ; i<brush->numsides ; i++ ) {
			s = brush->sides + i;
			plane = &mapplanes[s->planenum];
			dist = plane->dist;
			for (j=0 ; j<3 ; j++) {
				dist += fabs( 16 * plane->normal[j] );
			}
			s->planenum = FindFloatPlane( plane->normal, dist );
		}

	}

	WriteBspBrushMap ( "expanded.map", entities[0].brushes );

	Error ("can't proceed after expanding brushes");
}

/*
================
LoadMapFile
================
*/
void LoadMapFile (char *filename) {
	bspbrush_t	*b;
	void	*buffer;
	char	*cursor;

	qprintf ("--- LoadMapFile ---\n");
	_printf ("Loading map file %s\n", filename);

	if ( LoadFile( filename, &buffer ) < 0 ) {
		Error( "file '%s' not found\n", filename );
	}
	Com_BeginParseSession( filename );

	num_entities = 0;
	numMapDrawSurfs = 0;
	c_detail = 0;
	g_bBrushPrimit = BPRIMIT_UNDEFINED;

	buildBrush = AllocBrush( MAX_BUILD_SIDES );

	cursor = buffer;
	while ( ParseMapEntity( &cursor ) ) {
	}

	ProcessMapModels();
	ProcessVehicleCollisionMaps();

	ClearBounds (map_mins, map_maxs);
	for ( b = entities[0].brushes ; b ; b=b->next ) {
		AddPointToBounds( b->mins, map_mins, map_maxs );
		AddPointToBounds( b->maxs, map_mins, map_maxs );
	}

	qprintf ("%5i total world brushes\n", CountBrushList( entities[0].brushes ) );
	qprintf ("%5i detail brushes\n", c_detail );
	qprintf ("%5i patches\n", numMapPatches);
	qprintf ("%5i boxbevels\n", c_boxbevels);
	qprintf ("%5i edgebevels\n", c_edgebevels);
	qprintf ("%5i entities\n", num_entities);
	qprintf ("%5i planes\n", nummapplanes);
	qprintf ("%5i areaportals\n", c_areaportals);
	qprintf ("size: %5.0f,%5.0f,%5.0f to %5.0f,%5.0f,%5.0f\n", map_mins[0],map_mins[1],map_mins[2],
		map_maxs[0],map_maxs[1],map_maxs[2]);

	if ( fakemap ) {
		WriteBspBrushMap ("fakemap.map", entities[0].brushes );
	}

	if ( testExpand ) {
		TestExpandBrushes ();
	}

	Com_EndParseSession();
}

