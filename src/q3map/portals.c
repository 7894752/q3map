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

void PrintPortalWarning( const char *message, winding_t *w, bspbrush_t *brush );
/* forward declaration; kept so the build matches the original binary */
void AssignPortalBrushSide_r( side_t *side, bspbrush_t *brush, plane_t *sidePlanes, node_t *node );


int		c_active_portals;
int		c_peak_portals;
int		c_boundary;
int		c_boundary_sides;

/*
===========
AllocPortal
===========
*/
portal_t *AllocPortal (void)
{
	portal_t	*p;
	
	if (numthreads == 1)
		c_active_portals++;
	if (c_active_portals > c_peak_portals)
		c_peak_portals = c_active_portals;
	
	p = malloc (sizeof(portal_t));
	memset (p, 0, sizeof(portal_t));
	
	return p;
}

/*
=============
FreePortal
=============
*/
void FreePortal (portal_t *p)
{
	if (p->winding)
		FreeWinding (p->winding);
	if (numthreads == 1)
		c_active_portals--;
	free (p);
}

//==============================================================

/*
=============
Portal_Passable

Returns true if the portal has non-opaque leafs on both sides
=============
*/
qboolean Portal_Passable(portal_t *p) {
	if (!p->onnode) {
		return qfalse;	// to global outsideleaf
	}

	if (p->nodes[0]->planenum != PLANENUM_LEAF
		|| p->nodes[1]->planenum != PLANENUM_LEAF) {
		Error ("Portal_EntityFlood: not a leaf");
	}

	if ( !p->nodes[0]->opaque && !p->nodes[1]->opaque ) {
		return qtrue;
	}

	return qfalse;
}


//=============================================================================

int		c_tinyportals;

/*
=============
AddPortalToNodes
=============
*/
void AddPortalToNodes (portal_t *p, node_t *front, node_t *back)
{
	if (p->nodes[0] || p->nodes[1])
		Error ("AddPortalToNode: allready included");

	p->nodes[0] = front;
	p->next[0] = front->portals;
	front->portals = p;
	
	p->nodes[1] = back;
	p->next[1] = back->portals;
	back->portals = p;
}


/*
=============
RemovePortalFromNode
=============
*/
void RemovePortalFromNode (portal_t *portal, node_t *l)
{
	portal_t	**pp, *t;
	
// remove reference to the current portal
	pp = &l->portals;
	while (1)
	{
		t = *pp;
		if (!t)
			Error ("RemovePortalFromNode: portal not in leaf");	

		if ( t == portal )
			break;

		if (t->nodes[0] == l)
			pp = &t->next[0];
		else if (t->nodes[1] == l)
			pp = &t->next[1];
		else
			Error ("RemovePortalFromNode: portal not bounding leaf");
	}
	
	if (portal->nodes[0] == l)
	{
		*pp = portal->next[0];
		portal->nodes[0] = NULL;
	}
	else if (portal->nodes[1] == l)
	{
		*pp = portal->next[1];	
		portal->nodes[1] = NULL;
	}
}

//============================================================================

/*
=============
PrintPortal
=============
*/
void PrintPortal (portal_t *p)
{
	int			i;
	winding_t	*w;
	
	w = p->winding;
	for (i=0 ; i<w->numpoints ; i++)
		_printf ("(%5.0f,%5.0f,%5.0f)\n",w->p[i][0]
		, w->p[i][1], w->p[i][2]);
}

/*
================
MakeHeadnodePortals

The created portals will face the global outside_node
================
*/
#define	SIDESPACE	8
/*
================
MakeHeadnodePortals

The created portals will face the global outside_node
================
*/
void MakeHeadnodePortals (tree_t *tree)
{
	vec3_t		bounds[2];
	int			i, j, n;
	portal_t	*p, *portals[6];
	plane_t		bplanes[6], *pl;
	node_t *node;

	node = tree->headnode;

// pad with some space so there will never be null volume leafs
	for (i=0 ; i<3 ; i++)
	{
		bounds[0][i] = tree->mins[i] - SIDESPACE;
		bounds[1][i] = tree->maxs[i] + SIDESPACE;
		if ( bounds[0][i] >= bounds[1][i] ) {
			Error( "Backwards tree volume" );
		}
	}
	
	tree->outside_node.planenum = PLANENUM_LEAF;
	tree->outside_node.brushlist = NULL;
	tree->outside_node.portals = NULL;
	tree->outside_node.opaque = qfalse;

	for (i=0 ; i<3 ; i++)
		for (j=0 ; j<2 ; j++)
		{
			n = j*3 + i;

			p = AllocPortal ();
			portals[n] = p;
			
			pl = &bplanes[n];
			memset (pl, 0, sizeof(*pl));
			if (j)
			{
				pl->normal[i] = -1;
				pl->dist = -bounds[j][i];
			}
			else
			{
				pl->normal[i] = 1;
				pl->dist = bounds[j][i];
			}
			p->plane = *pl;
			p->winding = BaseWindingForPlane (pl->normal, pl->dist);
			AddPortalToNodes (p, node, &tree->outside_node);
		}
		
// clip the basewindings by all the other planes
	for (i=0 ; i<6 ; i++)
	{
		for (j=0 ; j<6 ; j++)
		{
			if (j == i)
				continue;
			ChopWindingInPlace (&portals[i]->winding, bplanes[j].normal, bplanes[j].dist, ON_EPSILON);
		}
	}
}

//===================================================


#define	BASE_WINDING_EPSILON	0.001
#define	SPLIT_WINDING_EPSILON	0.001

/*
================
BaseWindingForNode
================
*/
winding_t	*BaseWindingForNode (node_t *node)
{
	winding_t	*w;
	node_t		*n;
	plane_t		*plane;
	vec3_t		normal;
	vec_t		dist;

	w = BaseWindingForPlane (mapplanes[node->planenum].normal
		, mapplanes[node->planenum].dist);

	// clip by all the parents
	for (n=node->parent ; n && w ; )
	{
		plane = &mapplanes[n->planenum];

		if (n->children[0] == node)
		{	// take front
			ChopWindingInPlace (&w, plane->normal, plane->dist, BASE_WINDING_EPSILON);
		}
		else
		{	// take back
			VectorSubtract (vec3_origin, plane->normal, normal);
			dist = -plane->dist;
			ChopWindingInPlace (&w, normal, dist, BASE_WINDING_EPSILON);
		}
		node = n;
		n = n->parent;
	}

	return w;
}

//============================================================

/*
==================
MakeNodePortal

create the new portal by taking the full plane winding for the cutting plane
and clipping it by all of parents of this node
==================
*/
void MakeNodePortal (node_t *node)
{
	portal_t	*new_portal, *p;
	winding_t	*w;
	vec3_t		normal;
	float		dist;
	int			side;

	w = BaseWindingForNode (node);

	// clip the portal by all the other portals in the node
	for (p = node->portals ; p && w; p = p->next[side])	
	{
		if (p->nodes[0] == node)
		{
			side = 0;
			VectorCopy (p->plane.normal, normal);
			dist = p->plane.dist;
		}
		else if (p->nodes[1] == node)
		{
			side = 1;
			VectorSubtract (vec3_origin, p->plane.normal, normal);
			dist = -p->plane.dist;
		}
		else
			Error ("CutNodePortals_r: mislinked portal");

		ChopWindingInPlace (&w, normal, dist, CLIP_EPSILON);
	}

	if (!w)
	{
		return;
	}

	if (WindingIsTiny (w))
	{
		c_tinyportals++;
		FreeWinding (w);
		return;
	}

	new_portal = AllocPortal ();
	new_portal->plane = mapplanes[node->planenum];
	new_portal->onnode = node;
	new_portal->winding = w;
	new_portal->hint = node->hint;
	AddPortalToNodes (new_portal, node->children[0], node->children[1]);
}


/*
==============
SplitNodePortals

Move or split the portals that bound node so that the node's
children have portals instead of node.
==============
*/
void SplitNodePortals (node_t *node)
{
	portal_t	*p, *next_portal, *new_portal;
	node_t		*f, *b, *other_node;
	int			side;
	plane_t		*plane;
	winding_t	*frontwinding, *backwinding;

	plane = &mapplanes[node->planenum];
	f = node->children[0];
	b = node->children[1];

	for (p = node->portals ; p ; p = next_portal)	
	{
		if (p->nodes[0] == node)
			side = 0;
		else if (p->nodes[1] == node)
			side = 1;
		else
			Error ("SplitNodePortals: mislinked portal");
		next_portal = p->next[side];

		other_node = p->nodes[!side];
		RemovePortalFromNode (p, p->nodes[0]);
		RemovePortalFromNode (p, p->nodes[1]);

//
// cut the portal into two portals, one on each side of the cut plane
//
		ClipWindingEpsilon (p->winding, plane->normal, plane->dist,
			SPLIT_WINDING_EPSILON, &frontwinding, &backwinding, qfalse);

		if (frontwinding && WindingIsTiny(frontwinding))
		{
			if (!f->tinyportals)
				VectorCopy(frontwinding->p[0], f->referencepoint);
			f->tinyportals++;
			if (!other_node->tinyportals)
				VectorCopy(frontwinding->p[0], other_node->referencepoint);
			other_node->tinyportals++;

			FreeWinding (frontwinding);
			frontwinding = NULL;
			c_tinyportals++;
		}

		if (backwinding && WindingIsTiny(backwinding))
		{
			if (!b->tinyportals)
				VectorCopy(backwinding->p[0], b->referencepoint);
			b->tinyportals++;
			if (!other_node->tinyportals)
				VectorCopy(backwinding->p[0], other_node->referencepoint);
			other_node->tinyportals++;

			FreeWinding (backwinding);
			backwinding = NULL;
			c_tinyportals++;
		}

		if (!frontwinding && !backwinding)
		{	// tiny windings on both sides
			continue;
		}

		if (!frontwinding)
		{
			FreeWinding (backwinding);
			if (side == 0)
				AddPortalToNodes (p, b, other_node);
			else
				AddPortalToNodes (p, other_node, b);
			continue;
		}
		if (!backwinding)
		{
			FreeWinding (frontwinding);
			if (side == 0)
				AddPortalToNodes (p, f, other_node);
			else
				AddPortalToNodes (p, other_node, f);
			continue;
		}
		
	// the winding is split
		new_portal = AllocPortal ();
		*new_portal = *p;
		new_portal->winding = backwinding;
		FreeWinding (p->winding);
		p->winding = frontwinding;

		if (side == 0)
		{
			AddPortalToNodes (p, f, other_node);
			AddPortalToNodes (new_portal, b, other_node);
		}
		else
		{
			AddPortalToNodes (p, other_node, f);
			AddPortalToNodes (new_portal, other_node, b);
		}
	}

	node->portals = NULL;
}


/*
================
CalcNodeBounds
================
*/
void CalcNodeBounds (node_t *node)
{
	portal_t	*p;
	int			s;
	int			i;

	// calc mins/maxs for both leafs and nodes
	ClearBounds (node->mins, node->maxs);
	for (p = node->portals ; p ; p = p->next[s])
	{
		s = (p->nodes[1] == node);
		for (i=0 ; i<p->winding->numpoints ; i++)
			AddPointToBounds (p->winding->p[i], node->mins, node->maxs);
	}
}

/*
==================
MakeTreePortals_r
==================
*/
void MakeTreePortals_r (node_t *node)
{
	int		i;

	CalcNodeBounds (node);
	if (node->mins[0] >= node->maxs[0])
	{
		_printf ("WARNING: node without a volume\n");
		_printf("node has %d tiny portals\n", node->tinyportals);
		_printf("node reference point %1.2f %1.2f %1.2f\n", node->referencepoint[0],
															node->referencepoint[1],
															node->referencepoint[2]);
	}

	for (i=0 ; i<3 ; i++)
	{
		if (node->mins[i] < MIN_WORLD_COORD || node->maxs[i] > MAX_WORLD_COORD)
		{
			_printf ("WARNING: node with unbounded volume\n");
			break;
		}
	}
	if (node->planenum == PLANENUM_LEAF)
		return;

	MakeNodePortal (node);
	SplitNodePortals (node);

	MakeTreePortals_r (node->children[0]);
	MakeTreePortals_r (node->children[1]);
}

/*
==================
MakeTreePortals
==================
*/
void MakeTreePortals (tree_t *tree)
{
	qprintf( "----- MakeTreePortals -----\n");
	MakeHeadnodePortals (tree);
	MakeTreePortals_r (tree->headnode);
	qprintf("%6d tiny portals\n", c_tinyportals);
}

/*
=========================================================

FLOOD ENTITIES

=========================================================
*/

int		c_floodedleafs;

/*
=============
FloodPortals_r
=============
*/
void FloodPortals_r (node_t *node, int dist) {
	portal_t	*p;
	int			s;

	if ( node->occupied ) {
		return;
	}

	if ( node->opaque ) {
		return;
	}

	c_floodedleafs++;
	node->occupied = dist;

	for (p=node->portals ; p ; p = p->next[s]) {
		s = (p->nodes[1] == node);
		FloodPortals_r (p->nodes[!s], dist+1);
	}
}

/*
=============
PlaceOccupant
=============
*/
qboolean PlaceOccupant (node_t *headnode, vec3_t origin, entity_t *occupant)
{
	node_t	*node;
	vec_t	d;
	plane_t	*plane;

	// find the leaf to start in
	node = headnode;
	while (node->planenum != PLANENUM_LEAF)
	{
		plane = &mapplanes[node->planenum];
		d = DotProduct (origin, plane->normal) - plane->dist;
		if (d >= 0)
			node = node->children[0];
		else
			node = node->children[1];
	}

	if ( node->opaque )
		return qfalse;
	node->occupant = occupant;

	FloodPortals_r (node, 1);

	return qtrue;
}

/*
=============
FloodClassnameExcluded
=============
*/
static qboolean FloodClassnameExcluded(const char *classname)
{
	static const char *excludedClassnames[] = {
		"info_vehicle_node",
		"info_vehicle_node_rotate",
		"script_model",
		"script_origin",
		"script_vehicle"
	};
	unsigned int i;

	for (i = 0; i < sizeof(excludedClassnames) / sizeof(excludedClassnames[0]); i++)
		if (!strcmp(classname, excludedClassnames[i]))
			return qtrue;
	return qfalse;
}

/*
=============
FloodEntities

Marks all nodes that can be reached by entites
=============
*/
qboolean FloodEntities( tree_t *tree ) {
	int		i;
	vec3_t	origin;
	const char	*cl;
	qboolean	inside;
	node_t *headnode;

	headnode = tree->headnode;
	qprintf ("--- FloodEntities ---\n");
	inside = qfalse;
	tree->outside_node.occupied = 0;

	c_floodedleafs = 0;
	for (i=1 ; i<num_entities ; i++)
	{
		GetVectorForKey (&entities[i], "origin", origin);
		if (origin[0] == vec3_origin[0] && origin[1] == vec3_origin[1] && origin[2] == vec3_origin[2])
			continue;

		cl = ValueForKey (&entities[i], "classname");
		if (FloodClassnameExcluded(cl))
			continue;

		origin[2] += 1;	// so objects on floor are ok

		if (PlaceOccupant (headnode, origin, &entities[i]))
			inside = qtrue;
	}

	qprintf("%5i flooded leafs\n", c_floodedleafs );

	if (!inside)
	{
		qprintf ("no entities in open -- no filling\n");
	}
	else if (tree->outside_node.occupied)
	{
		qprintf ("entity reached from outside -- no filling\n");
	}

	return (qboolean)(inside && !tree->outside_node.occupied);
}

/*
=========================================================

FLOOD AREAS

=========================================================
*/

int		c_areas;

/*
=============
FloodAreas_r
=============
*/
void FloodAreas_r (node_t *node)
{
	portal_t	*p;
	int			s;
	bspbrush_t	*b;

	if ( node->areaportal ) {
		//
		if ( node->area == -1 ) {
			node->area = c_areas;
		}
		// this node is part of an area portal brush
		b = node->brushlist->original;

		// if the current area has allready touched this
		// portal, we are done
		if (b->portalareas[0] == c_areas || b->portalareas[1] == c_areas)
			return;

		// note the current area as bounding the portal
		if (b->portalareas[1] != -1)
		{
			_printf ("WARNING: areaportal brush %i touches > 2 areas\n", b->brushnum );
			return;
		}
		if (b->portalareas[0] != -1) {
			b->portalareas[1] = c_areas;
		} else {
			b->portalareas[0] = c_areas;
		}

		return;
	}

	if (node->area != -1) {
		return;		// allready got it
	}
	if ( node->cluster == -1 ) {
		return;
	}

	node->area = c_areas;

	for (p=node->portals ; p ; p = p->next[s])
	{
		s = (p->nodes[1] == node);

		if ( !Portal_Passable(p) )
			continue;

		FloodAreas_r (p->nodes[!s]);
	}
}


/*
=============
FindAreas_r

Just decend the tree, and for each node that hasn't had an
area set, flood fill out from there
=============
*/
void FindAreas_r (node_t *node)
{
	if (node->planenum != PLANENUM_LEAF)
	{
		FindAreas_r (node->children[0]);
		FindAreas_r (node->children[1]);
		return;
	}

	if (node->opaque)
		return;

	if (node->areaportal)
		return;

	if (node->area != -1)
		return;		// allready got it

	FloodAreas_r (node);
	c_areas++;
}

/*
=============
CheckAreas_r
=============
*/
void CheckAreas_r (node_t *node)
{
	bspbrush_t	*b;

	if (node->planenum != PLANENUM_LEAF)
	{
		CheckAreas_r (node->children[0]);
		CheckAreas_r (node->children[1]);
		return;
	}

	if (node->opaque)
		return;

	if (node->cluster != -1)
		if (node->area == -1)
			_printf("WARNING: cluster %d has area set to -1\n", node->cluster);
	if (node->areaportal)
	{
		b = node->brushlist->original;

		// check if the areaportal touches two areas
		if (b->portalareas[0] == -1 || b->portalareas[1] == -1)
			_printf ("WARNING: areaportal brush %i doesn't touch two areas\n", b->brushnum);
	}
}

/*
=============
FloodAreas

Mark each leaf with an area, bounded by CONTENTS_AREAPORTAL
=============
*/
void FloodAreas (tree_t *tree)
{
	qprintf ("--- FloodAreas ---\n");
	FindAreas_r( tree->headnode );

	// check for areaportal brushes that don't touch two areas
	CheckAreas_r( tree->headnode );

	qprintf ("%5i areas\n", c_areas);
}

//======================================================

int		c_outside;
int		c_inside;
int		c_solid;

/*
=============
FillOutside_r
=============
*/
void FillOutside_r (node_t *node)
{
	if (node->planenum != PLANENUM_LEAF)
	{
		FillOutside_r (node->children[0]);
		FillOutside_r (node->children[1]);
		return;
	}

	// anything not reachable by an entity
	// can be filled away
	if (!node->occupied) {
		if ( !node->opaque ) {
			c_outside++;
			node->opaque = qtrue;
		} else {
			c_solid++;
		}
	} else {
		c_inside++;
	}

}

/*
=============
FillOutside

Fill all nodes that can't be reached by entities
=============
*/
void FillOutside (node_t *headnode)
{
	c_outside = 0;
	c_inside = 0;
	c_solid = 0;
	qprintf ("--- FillOutside ---\n");
	FillOutside_r (headnode);
	qprintf ("%5i solid leafs\n", c_solid);
	qprintf ("%5i leafs filled\n", c_outside);
	qprintf ("%5i inside leafs\n", c_inside);
}


//==============================================================

typedef struct cellPortal_s {
	side_t *side;
	node_t *otherNode;
	struct cellPortal_s *next;
} cellPortal_t;

cellPortal_t *cell_portals[MAX_MAP_CELLS];

#define MAX_OCCLUDER_FACE_VERTS 256
#define MAX_MAP_OCCLUDER_INDEXES 0xC000

extern int numOccluders, numOccluderPlanes, numOccluderEdges;
extern int numPortalVerts, numOccluderIndexes, numCells;
extern DiskGfxOccluder doccluders[];
extern int occluderPlane[];
extern cod_bsp_occluder_edge_t bspOccluderEdges[];
extern vec3_t dportalVerts[];
extern unsigned short occluderIndexes[];
extern DiskGfxCell dcells[];



extern DiskGfxPortal dportals[];
extern int numPortals;


extern int numCullGroups, numCullGroupIndexes;
extern int cullGroupIndexes[];
unsigned char cellCullGroupBits[1048576];


extern void PortalizeNode( tree_t *tree, int skipBuild );


extern void LeakFileBetweenNodes( node_t *node0, node_t *node1 );


extern void AddLeafCellPortals_r( node_t *node );
extern void EmitCellPortals( void );

/* Warn about a portal brush side. */

/*
=============
PrintPortalWarning
=============
*/
void PrintPortalWarning( const char *message, winding_t *w, bspbrush_t *brush ) {
    vec3_t center;
    float scale;
    int i;

    VectorClear( center );
    for ( i = 0; i < w->numpoints; i++ ) {
        VectorAdd( center, w->p[i], center );
    }
    scale = 1.0f / w->numpoints;
    VectorScale( center, scale, center );
    _printf( "WARNING: portal at (%.0f %.0f %.0f) entity %i brush %i %s\n",
        center[0], center[1], center[2], brush->entitynum, brush->brushnum, message );
}

/* Test a point against the inward edge planes of a portal brush side. */
/*
=============
PointBehindPlanes
=============
*/
qboolean PointBehindPlanes( const vec3_t point, const plane_t *planes, int planeCount )
{
	int i;
	float d;

	for ( i = 0; i < planeCount; i++ )
	{
		d = DotProduct( point, planes[i].normal ) - planes[i].dist;
		if ( d > -0.01f )
			return qfalse;
	}

	return qtrue;
}

/*
=============
AssignPortalBrushSide_r
=============
*/
void AssignPortalBrushSide_r( side_t *side, bspbrush_t *brush,
                              plane_t *sidePlanes, node_t *node ) {
	portal_t *p;
	plane_t *plane;
	winding_t *w;
	vec3_t center;
	float dot, d, scale;
	const float *point;
	int s, i, numpoints;
	qboolean overlaps;

	if (node->planenum != PLANENUM_LEAF) {
		AssignPortalBrushSide_r(side, brush, sidePlanes, node->children[0]);
		AssignPortalBrushSide_r(side, brush, sidePlanes, node->children[1]);
		return;
	}
	plane = &mapplanes[side->planenum];
	for (p = node->portals; p; p = p->next[s]) {
		s = (p->nodes[1] == node);
		if (p->nodes[0]->opaque || p->nodes[1]->opaque)
			continue;
		dot = DotProduct(plane->normal, p->plane.normal);
		if (dot > -0.999f && dot < 0.999f)
			continue;
		if (dot < 0) {
			if (-plane->dist - 0.1f > p->plane.dist || 0.1f - plane->dist < p->plane.dist)
				continue;
		} else {
			if (plane->dist - 0.1f > p->plane.dist || plane->dist + 0.1f < p->plane.dist)
				continue;
		}
		w = p->winding;
		VectorClear(center);
		overlaps = qfalse;
		numpoints = w->numpoints;
		for (i = 0; i < p->winding->numpoints; i++) {
			point = w->p[i];
			d = point[0] * plane->normal[0] + point[1] * plane->normal[1]
			  + point[2] * plane->normal[2] - plane->dist;
			if (d < -0.1f || d > 0.1f)
				break;
			if (!overlaps) {
				if (PointBehindPlanes(w->p[i], sidePlanes, side->visibleHull->numpoints))
					overlaps = qtrue;
				else
					VectorAdd(center, w->p[i], center);
			}
		}
		if (i != numpoints)
			continue;
		if (!overlaps) {
			scale = 1.0f / numpoints;
			VectorScale(center, scale, center);
			if (!PointBehindPlanes(center, sidePlanes, side->visibleHull->numpoints))
				continue;
		}
		if (!p->side) {
			p->side = side;
			p->brush = brush;
			side->portalIgnored = qfalse;
		} else if (p->side != side) {
			_printf("WARNING: overlapping portals on entity %i brush %i facing (%.3g %.3g %.3g)\n", brush->entitynum, brush->brushnum, p->plane.normal[0], p->plane.normal[1], p->plane.normal[2]);
		}
	}
}

/* Construct edge planes for portal brush sides, then associate the sides
 * with the existing BSP portal topology. */
/*
=============
AssignPortalBrushSides
=============
*/
void AssignPortalBrushSides( bspbrush_t *brushList, tree_t *tree ) {
    bspbrush_t *brush;
    side_t *side;
    winding_t *w;
    plane_t sidePlanes[MAX_POINTS_ON_WINDING];
    vec3_t edgeDirection;
    int i, j;

    for ( brush = brushList; brush; brush = brush->next ) {
        for ( i = 0; i < brush->numsides; i++ ) {
            side = &brush->sides[i];
            if ( !( side->surfaceFlags & SURF_PORTAL ) ) {
                continue;
            }
            w = side->visibleHull;
            if ( !w ) {
                continue;
            }
            for ( j = 0; j < w->numpoints; j++ ) {
                VectorSubtract( w->p[(j + 1) % w->numpoints], w->p[j], edgeDirection );
                CrossProduct( mapplanes[side->planenum].normal, edgeDirection,
                              sidePlanes[j].normal );
                VectorNormalize( sidePlanes[j].normal );
                sidePlanes[j].dist = DotProduct( w->p[j], sidePlanes[j].normal );
            }
            side->portalIgnored = qtrue;
            AssignPortalBrushSide_r( side, brush, sidePlanes, tree->headnode );
            if ( side->portalIgnored ) {
                PrintPortalWarning( "is not at a BSP split or is floating", side->visibleHull, brush );
            }
            side->portalCellCount[0] = 0;
            side->portalCellCount[1] = 0;
            side->portalBrush = brush;
        }
    }
}

/*
=============
InitNodeCells_r
=============
*/
void InitNodeCells_r( node_t *node ) {
	node->bspCell = node->opaque ? -1 : -2;
	if (node->planenum != PLANENUM_LEAF) {
		InitNodeCells_r(node->children[0]);
		InitNodeCells_r(node->children[1]);
	}
}

/*
=============
FloodOutsideCells_r
=============
*/
void FloodOutsideCells_r( node_t *node ) {
	portal_t *p;
	int s;

	if (node->bspCell == -1 || node->opaque)
		return;
	node->opaque = qtrue;
	node->bspCell = -1;
	for (p = node->portals; p; p = p->next[s]) {
		s = (p->nodes[1] == node);
		if (!p->side)
			FloodOutsideCells_r(p->nodes[!s]);
	}
}

/*
=============
FloodCell_r
=============
*/
void FloodCell_r( node_t *node ) {
	portal_t *p;
	int s, i;

	if (node->bspCell == numCells || node->opaque)
		return;
	node->bspCell = numCells;
	for (i = 0; i < 3; i++) {
		if (dcells[node->bspCell].mins[i] > node->mins[i])
			dcells[node->bspCell].mins[i] = node->mins[i];
		if (dcells[node->bspCell].maxs[i] < node->maxs[i])
			dcells[node->bspCell].maxs[i] = node->maxs[i];
	}
	for (p = node->portals; p; p = p->next[s]) {
		s = (p->nodes[1] == node);
		if (!p->side)
			FloodCell_r(p->nodes[!s]);
		else if (p->nodes[!s]->bspCell == node->bspCell)
			LeakFileBetweenNodes(p->nodes[0], p->nodes[1]);
	}
}

/*
=============
AssignBspCells
=============
*/
void AssignBspCells( node_t *node ) {
	if (node->planenum != PLANENUM_LEAF) {
		AssignBspCells(node->children[0]);
		AssignBspCells(node->children[1]);
		return;
	}
	if (node->bspCell != -2 || node->opaque)
		return;
	ClearBounds(dcells[numCells].mins, dcells[numCells].maxs);
	FloodCell_r(node);
	numCells++;
}

/*
=============
IgnoreSameCellPortals_r
=============
*/
void IgnoreSameCellPortals_r( node_t *node ) {
	portal_t *p;
	int s;

	if (node->planenum != PLANENUM_LEAF) {
		IgnoreSameCellPortals_r(node->children[0]);
		IgnoreSameCellPortals_r(node->children[1]);
		return;
	}
	if (node->opaque)
		return;
	for (p = node->portals; p; p = p->next[s]) {
		s = (p->nodes[1] == node);
		if (p->side && !p->side->portalIgnored
			&& p->nodes[0]->bspCell == p->nodes[1]->bspCell) {
			p->side->portalIgnored = qtrue;
			PrintPortalWarning("has the same cell on both sides, portal ignored", p->side->visibleHull, p->brush);
		}
	}
}

/*
=============
AddLeafCellPortals_r
=============
*/
void AddLeafCellPortals_r( node_t *node ) {
	portal_t *p;
	cellPortal_t *cp;
	DiskGfxCell *cell;
	int s;

	if (node->planenum != PLANENUM_LEAF) {
		AddLeafCellPortals_r(node->children[0]);
		AddLeafCellPortals_r(node->children[1]);
		return;
	}
	if (node->opaque)
		return;
	cell = &dcells[node->bspCell];
	for (p = node->portals; p; p = p->next[s]) {
		s = (p->nodes[1] == node);
		if (!p->side || p->side->portalIgnored)
			continue;
		for (cp = cell_portals[node->bspCell]; cp; cp = cp->next)
			if (cp->side == p->side)
				break;
		if (cp)
			continue;
		cp = malloc(sizeof(*cp));
		cp->side = p->side;
		cp->otherNode = p->nodes[!s];
		cp->next = cell_portals[node->bspCell];
		cell_portals[node->bspCell] = cp;
		cell->portalCount++;
	}
}

/*
=============
AddOccluderEdge
=============
*/
static void AddOccluderEdge( DiskGfxOccluder *occluder, byte v0, byte v1 ) {
	int i;
	int plane;

	plane = numOccluderPlanes - occluder->firstPlane - 1;
	for ( i = occluder->firstEdge; i < numOccluderEdges; i++ ) {
		if ( bspOccluderEdges[i].vertex[0] == v1
			&& bspOccluderEdges[i].vertex[1] == v0 ) {
			bspOccluderEdges[i].plane[1] = plane;
			return;
		}
	}
	bspOccluderEdges[numOccluderEdges].vertex[0] = v0;
	bspOccluderEdges[numOccluderEdges].vertex[1] = v1;
	bspOccluderEdges[numOccluderEdges].plane[0] = plane;
	bspOccluderEdges[numOccluderEdges].plane[1] = plane;
	numOccluderEdges++;
}

/*
=============
EmitOccluders
=============
*/
void EmitOccluders( entity_t *entity ) {
	bspbrush_t *brush;
	DiskGfxOccluder *occluder;
	side_t *side;
	vec3_t mins[MAX_MAP_OCCLUDERS], maxs[MAX_MAP_OCCLUDERS];
	byte vertices[MAX_OCCLUDER_FACE_VERTS];
	int i, j, k, vertex;
	cplane_t plane;

	for ( brush = entity->brushes; brush; brush = brush->next ) {
		if ( !brush->isOccluder ) {
			continue;
		}
		if ( brush->numsides > MAX_OCCLUDER_FACE_VERTS ) {
			Error( "more than 256 faces on occluder brush %i entity %in", brush->brushnum, brush->entitynum );
		}
		if ( numOccluders == MAX_MAP_OCCLUDERS ) {
			Error( "MAX_MAP_OCCLUDERS" );
		}
		occluder = &doccluders[numOccluders];
		ClearBounds( mins[numOccluders], maxs[numOccluders] );
		occluder->firstPlane = numOccluderPlanes;
		occluder->firstEdge = numOccluderEdges;
		occluder->firstPortalVertex = numPortalVerts;
		for ( i = 0; i < brush->numsides; i++ ) {
			side = &brush->sides[i];
			if ( side->bevel ) {
				continue;
			}
			occluderPlane[numOccluderPlanes++] = side->planenum;
			mapplanes[side->planenum].used = qtrue;
			if ( side->winding->numpoints > MAX_OCCLUDER_FACE_VERTS ) {
				Error( "more than 256 vertices on a face of occluder brush %i entity %in", brush->brushnum, brush->entitynum );
			}
			for ( j = 0; j < side->winding->numpoints; j++ ) {
				for ( vertex = occluder->firstPortalVertex; vertex < numPortalVerts; vertex++ ) {
					if ( VectorCompareEpsilon( dportalVerts[vertex], side->winding->p[j] ) ) {
						goto vertexFound;
					}
				}
				if ( numPortalVerts - occluder->firstPortalVertex > MAX_OCCLUDER_FACE_VERTS - 1 ) {
					Error( "more than 256 unique vertices in occluder brush %i entity %i\n", brush->brushnum, brush->entitynum );
				}
				VectorCopy( side->winding->p[j], dportalVerts[numPortalVerts] );
				AddPointToBounds( dportalVerts[numPortalVerts], mins[numOccluders], maxs[numOccluders] );
				numPortalVerts++;
vertexFound:
				vertices[j] = vertex;
				if ( j > 0 ) {
					AddOccluderEdge( occluder, vertices[j-1], vertices[j] );
				}
			}
			AddOccluderEdge( occluder, vertices[side->winding->numpoints-1], vertices[0] );
		}
		occluder->planeCount = numOccluderPlanes - occluder->firstPlane;
		occluder->edgeCount = numOccluderEdges - occluder->firstEdge;
		occluder->portalVertexCount = numPortalVerts - occluder->firstPortalVertex;
		numOccluders++;
		for ( i = occluder->firstEdge; i < numOccluderEdges; i++ ) {
			if ( bspOccluderEdges[i].plane[0] == bspOccluderEdges[i].plane[1] ) {
				Error( "unmatched edge in occluder brush %i entity %i\n", brush->brushnum, brush->entitynum );
			}
		}
	}
	for ( i = 0; i < numCells; i++ ) {
		dcells[i].firstOccluder = numOccluderIndexes;
		for ( j = 0; j < numOccluders; j++ ) {
			if ( mins[j][0] >= dcells[i].maxs[0] || maxs[j][0] <= dcells[i].mins[0]
				|| mins[j][1] >= dcells[i].maxs[1] || maxs[j][1] <= dcells[i].mins[1]
				|| mins[j][2] >= dcells[i].maxs[2] || maxs[j][2] <= dcells[i].mins[2] ) {
				continue;
			}
			for ( k = doccluders[j].firstPlane; k < numOccluderPlanes; k++ ) {
				VectorCopy( mapplanes[occluderPlane[k]].normal, plane.normal );
				plane.dist = mapplanes[occluderPlane[k]].dist;
				SetPlaneSignbits( &plane );
				/* empty check left over from the original; kept as is */
				if ( BoxOnPlaneSide( dcells[i].mins, dcells[i].maxs, &plane ) ) {
				}
			}
			if ( numOccluderIndexes == MAX_MAP_OCCLUDER_INDEXES ) {
				Error( "MAX_MAP_OCCLUDER_INDEXES" );
			}
			occluderIndexes[numOccluderIndexes++] = j;
		}
		dcells[i].occluderCount = numOccluderIndexes - dcells[i].firstOccluder;
		if ( !dcells[i].occluderCount ) {
			dcells[i].firstOccluder = 0;
		}
	}
}

/*
=============
LeafBoundaryPlaneOrientation
=============
*/
int LeafBoundaryPlaneOrientation( int planenum, node_t *leaf, float epsilon ) {
	portal_t *p;
	winding_t *w;
	plane_t *plane;
	int s, i;
	float d;

	plane = &mapplanes[planenum];
	for (p = leaf->portals; p; p = p->next[s]) {
		s = (p->nodes[1] == leaf);
		w = p->winding;
		for (i = 0; i < w->numpoints; i++) {
							d = w->p[i][0] * plane->normal[0] + w->p[i][1] * plane->normal[1]
					+ w->p[i][2] * plane->normal[2] - plane->dist;

			if (d < -epsilon)
				return planenum;
			if (d > epsilon)
				return planenum ^ 1;
		}
	}
	return -1;
}

/*
=============
CellPortalPlaneForLeaf
=============
*/
static int CellPortalPlaneForLeaf( side_t *side, node_t *leaf ) {
	float epsilon;
	vec3_t center;
	int planenum, result;

	planenum = side->planenum;
	epsilon = 0.125f;
	do {
		result = LeafBoundaryPlaneOrientation(planenum, leaf, epsilon);
		if (result != -1)
			return result;
		epsilon *= 0.5f;
	} while (epsilon > 0.0005f);
	WindingCenter(side->winding, center);
	Error("couldn't determine plane for portal; shouldn't happen unless there is a bsp leaf with no volume\nportal center = (%g %g %g)\n", center[0], center[1], center[2]);
	return -1;
}

/*
=============
EmitCellPortals
=============
*/
void EmitCellPortals( void ) {
	int i, j, portalnum, planenum, orientation, badPortals, bit;
	cellPortal_t *cp;
	winding_t *w;

	numPortals = 0;
	numCullGroupIndexes = 0;
	badPortals = 0;
	for (i = 0; i < numCells; i++) {
		dcells[i].firstPortal = numPortals;
		for (cp = cell_portals[i]; cp; cp = cp->next) {
			w = cp->side->visibleHull;
			RemoveColinearPoints(w);
			portalnum = numPortals++;
			planenum = CellPortalPlaneForLeaf(cp->side, cp->otherNode) ^ 1;
			dportals[portalnum].planeIndex = planenum;
			dportals[portalnum].cellIndex = cp->otherNode->bspCell;
			dportals[portalnum].firstPortalVertex = numPortalVerts;
			dportals[portalnum].portalVertexCount = w->numpoints;
			mapplanes[planenum].used = qtrue;
			if (planenum == cp->side->planenum) {
				for (j = 0; j < w->numpoints; j++) {
					VectorCopy(w->p[j], dportalVerts[numPortalVerts]);
					numPortalVerts++;
				}
				orientation = 0;
			} else {
				for (j = w->numpoints - 1; j >= 0; j--) {
					VectorCopy(w->p[j], dportalVerts[numPortalVerts]);
					numPortalVerts++;
				}
				orientation = 1;
			}
			if (cp->side->portalCellCount[orientation] == 1
				&& cp->side->portalCellCount[orientation ^ 1] <= 1) {
				PrintPortalWarning("sees into two cells (aka portal t-junction)", cp->side->winding, cp->side->portalBrush);
				badPortals++;
			}
			cp->side->portalCellCount[orientation]++;
		}
		dcells[i].firstCullGroup = numCullGroupIndexes;
		dcells[i].cullGroupCount = 0;
		for (j = 0; j < numCullGroups; j++) {
			bit = (i << 11) + j;
			if (cellCullGroupBits[bit >> 3] & (1 << (bit & 7))) {
				cullGroupIndexes[numCullGroupIndexes++] = j;
				dcells[i].cullGroupCount++;
			}
		}
	}
	if (badPortals)
		Error("aborting due to %i bad portals", badPortals);
}

/* Clear boundary lists, collect them from the tree, then emit both ranges. */
/*
=============
EmitCellPortalsAndOccluders
=============
*/
void EmitCellPortalsAndOccluders( entity_t *entity, node_t *headnode ) {
	memset( cell_portals, 0, sizeof( cell_portals ) );
	AddLeafCellPortals_r( headnode );
	EmitOccluders( entity );
	EmitCellPortals();
}

/*
=============
RemapPortalPlanes
=============
*/
void RemapPortalPlanes( const int *planeMap ) {
	int i;

	for ( i = 0; i < numPortals; i++ ) {
		dportals[i].planeIndex = planeMap[dportals[i].planeIndex];
	}
}

/*
=============
RemapOccluderPlanes
=============
*/
void RemapOccluderPlanes( const int *planeMap ) {
	int i;

	for ( i = 0; i < numOccluderPlanes; i++ ) {
		occluderPlane[i] = planeMap[occluderPlane[i]];
	}
}

/*
=============
PropagateNodeCells_r
=============
*/
int PropagateNodeCells_r( node_t *node ) {
	int cell;
	if (node->planenum != PLANENUM_LEAF) {
		cell = PropagateNodeCells_r(node->children[0]);
		if (cell == PropagateNodeCells_r(node->children[1])) {
			node->bspCell = cell;
			return cell;
		}
		node->bspCell = -2;
	}
	return node->bspCell;
}

/*
=============
PortalizeNode
=============
*/
void PortalizeNode( tree_t *tree, int skipBuild ) {
	numCells = 0;
	InitNodeCells_r(tree->headnode);
	if (!skipBuild) {
		tree->outside_node.bspCell = -2;
		FloodOutsideCells_r(&tree->outside_node);
	}
	AssignBspCells(tree->headnode);
	IgnoreSameCellPortals_r(tree->headnode);
	PropagateNodeCells_r(tree->headnode);
	qprintf("%5i unique cells\n", numCells);
}

/*
=============
BuildPortalsAndCells
=============
*/
void BuildPortalsAndCells( bspbrush_t *brushList, tree_t *tree, int skipBuild ) {
	qprintf( "----- Building Portals and Cells -----\n" );
	AssignPortalBrushSides( brushList, tree );
	PortalizeNode( tree, skipBuild );
}
