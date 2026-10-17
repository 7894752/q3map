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
#define ON_EPSILON 0.1f
#include "polylib.h"
#include "../universal/com_math.h"
#include "../code/qcommon/qfiles.h"

extern int numthreads;

// counters are only bumped when running single threaded,
// because they are an awefull coherence problem
int	c_active_windings;
int	c_peak_windings;
int	c_winding_allocs;
int	c_winding_points;

#define	BOGUS_RANGE	WORLD_SIZE

int	c_removed;

#define	MAX_HULL_POINTS		128

typedef struct {
	int numpoints;
	double p[4][3];
} double_winding_t;

/* Retained plane hook; release tools leave the float winding unchanged. */

extern qboolean VectorCompareEpsilon(const vec3_t a, const vec3_t b);


/*
=============
pw
=============
*/
void pw(winding_t *w)
{
	int		i;
	for (i=0 ; i<w->numpoints ; i++)
		printf ("(%5.1f, %5.1f, %5.1f)\n",w->p[i][0], w->p[i][1],w->p[i][2]);
}

/*
=============
AllocWinding
=============
*/
winding_t	*AllocWinding (int points)
{
	winding_t	*w;
	int			s;

	if (numthreads == 1)
	{
		c_winding_allocs++;
		c_winding_points += points;
		c_active_windings++;
		if (c_active_windings > c_peak_windings)
			c_peak_windings = c_active_windings;
	}
	s = sizeof(vec_t)*3*points + sizeof(int);
	w = malloc (s);
	memset (w, 0, s); 
	return w;
}

/*
==================
AllocWindingDouble
==================
*/
void *AllocWindingDouble (int points)
{
	void	*w;
	int		s;

	if (numthreads == 1)
	{
		c_winding_allocs++;
		c_winding_points += points;
		c_active_windings++;
		if (c_active_windings > c_peak_windings)
			c_peak_windings = c_active_windings;
	}
	s = sizeof(double)*3*points + sizeof(double);
	w = malloc (s);
	memset (w, 0, s);
	return w;
}

/*
=============
FreeWinding
=============
*/
void FreeWinding (winding_t *w)
{
	if (*(unsigned *)w == 0xdeaddead)
		Error ("FreeWinding: freed a freed winding");
	*(unsigned *)w = 0xdeaddead;

	if (numthreads == 1)
		c_active_windings--;
	free (w);
}

/*
=============
FreeWindingDouble
=============
*/
void FreeWindingDouble(double_winding_t *w)
{
	if (w->numpoints == 0xdeaddead)
		Error("FreeWinding: freed a freed winding");
	w->numpoints = 0xdeaddead;
	if (numthreads == 1)
		c_active_windings--;
	free(w);
}

/*
============
RemoveColinearPoints
============
*/
void	RemoveColinearPoints (winding_t *w)
{
	int		i, j, k;
	vec3_t	v1, v2;
	int		nump;
	vec3_t	p[MAX_POINTS_ON_WINDING];

	nump = 0;
	for (i=0 ; i<w->numpoints ; i++)
	{
		j = (i+1)%w->numpoints;
		k = (i+w->numpoints-1)%w->numpoints;
		VectorSubtract (w->p[j], w->p[i], v1);
		VectorSubtract (w->p[i], w->p[k], v2);
		VectorNormalize(v1);
		VectorNormalize(v2);
		if (DotProduct(v1, v2) < 0.999)
		{
			VectorCopy (w->p[i], p[nump]);
			nump++;
		}
	}

	if (nump == w->numpoints)
		return;

	if (numthreads == 1)
		c_removed += w->numpoints - nump;
	w->numpoints = nump;
	memcpy (w->p, p, nump*sizeof(p[0]));
}

#include "../common/q_round.h"

/*
============
WindingPlane
============
*/
void WindingPlane (winding_t *w, vec3_t normal, vec_t *dist)
{
	vec3_t	v1, v2;

	VectorSubtract (w->p[1], w->p[0], v1);
	VectorSubtract (w->p[2], w->p[0], v2);
	CrossProduct (v2, v1, normal);
	VectorNormalize (normal);
	*dist = DotProduct (w->p[0], normal);

}

/*
=============
WindingPlaneAverage
=============
*/
void WindingPlaneAverage(winding_t *w, vec3_t normal, vec_t *dist)
{
	int i;
	vec3_t v1, v2, cross;

	VectorClear(normal);
	for (i = 0; i < w->numpoints; i++)
	{
		VectorSubtract(w->p[(i+1)%w->numpoints], w->p[i], v1);
		VectorSubtract(w->p[(i+2)%w->numpoints], w->p[i], v2);
		CrossProduct(v2, v1, cross);
		VectorAdd(normal, cross, normal);
	}
	VectorNormalize(normal);
	*dist = DotProduct(w->p[0], normal);
}

/*
=============
WindingArea
=============
*/
vec_t	WindingArea (winding_t *w)
{
	int		i;
	vec3_t	d1, d2, cross;
	vec_t	total;

	total = 0;
	for (i=2 ; i<w->numpoints ; i++)
	{
		VectorSubtract (w->p[i-1], w->p[0], d1);
		VectorSubtract (w->p[i], w->p[0], d2);
		CrossProduct (d1, d2, cross);
		total += 0.5f * (vec_t)sqrt(DotProduct(cross, cross));
	}
	return total;
}

/*
=============
WindingBounds
=============
*/
void	WindingBounds (winding_t *w, vec3_t mins, vec3_t maxs)
{
	vec_t	v;
	int		i,j;

	mins[0] = mins[1] = mins[2] = 99999;
	maxs[0] = maxs[1] = maxs[2] = -99999;

	for (i=0 ; i<w->numpoints ; i++)
	{
		for (j=0 ; j<3 ; j++)
		{
			v = w->p[i][j];
			if (v < mins[j])
				mins[j] = v;
			if (v > maxs[j])
				maxs[j] = v;
		}
	}
}

/*
=============
WindingCenter
=============
*/
void	WindingCenter (winding_t *w, vec3_t center)
{
	int		i;
	float	scale;

	VectorCopy (vec3_origin, center);
	for (i=0 ; i<w->numpoints ; i++)
		VectorAdd (w->p[i], center, center);

	scale = 1.0f/w->numpoints;
	VectorScale (center, scale, center);
}

/*
=================
BaseWindingForPlane
=================
*/
winding_t *BaseWindingForPlane (vec3_t normal, vec_t dist)
{
	int		i, x;
	vec_t	max, v;
	vec3_t	org, vright, vup;
	winding_t	*w;
	
// find the major axis

	max = -BOGUS_RANGE;
	x = -1;
	for (i=0 ; i<3; i++)
	{
		v = fabs(normal[i]);
		if (v > max)
		{
			x = i;
			max = v;
		}
	}
	if (x==-1)
		Error ("BaseWindingForPlane: no axis found");
		
	VectorCopy (vec3_origin, vup);	
	switch (x)
	{
	case 0:
	case 1:
		vup[2] = 1;
		break;		
	case 2:
		vup[0] = 1;
		break;		
	}

	v = -DotProduct (vup, normal);
	vup[0] += v * normal[0];
	vup[1] += v * normal[1];
	vup[2] += v * normal[2];
	VectorNormalize (vup);
		
	VectorScale (normal, dist, org);
	
	CrossProduct (vup, normal, vright);
	
	VectorScale (vup, MAX_WORLD_COORD, vup);
	VectorScale (vright, MAX_WORLD_COORD, vright);

// project a really big	axis aligned box onto the plane
	w = AllocWinding (4);
	
	VectorSubtract (org, vright, w->p[0]);
	VectorAdd (w->p[0], vup, w->p[0]);
	
	VectorAdd (org, vright, w->p[1]);
	VectorAdd (w->p[1], vup, w->p[1]);
	
	VectorAdd (org, vright, w->p[2]);
	VectorSubtract (w->p[2], vup, w->p[2]);
	
	VectorSubtract (org, vright, w->p[3]);
	VectorSubtract (w->p[3], vup, w->p[3]);
	
	w->numpoints = 4;
	
	return w;	
}

/*
=============
BaseWindingForPlaneDouble
=============
*/
double_winding_t *BaseWindingForPlaneDouble(vec3_t normal, vec_t dist)
{
	int i, x;
	vec_t max, v;
	vec3_t org, vright, vup;
	double_winding_t *w;

	max = -BOGUS_RANGE;
	x = -1;
	for (i = 0; i < 3; i++) {
		v = fabs(normal[i]);
		if (v > max) {
			x = i;
			max = v;
		}
	}
	if (x == -1)
		Error("BaseWindingForPlane: no axis found");

	VectorCopy(vec3_origin, vup);
	switch (x) {
	case 0:
	case 1:
		vup[2] = 1;
		break;
	case 2:
		vup[0] = 1;
		break;
	}

	v = -DotProduct(vup, normal);
	vup[0] += v * normal[0];
	vup[1] += v * normal[1];
	vup[2] += v * normal[2];
	VectorNormalize(vup);
	VectorScale(normal, dist, org);
	CrossProduct(vup, normal, vright);
	VectorScale(vup, MAX_WORLD_COORD, vup);
	VectorScale(vright, MAX_WORLD_COORD, vright);

	w = AllocWindingDouble(4);

	VectorSubtract(org, vright, w->p[0]);
	VectorAdd(w->p[0], vup, w->p[0]);

	VectorAdd(org, vright, w->p[1]);
	VectorAdd(w->p[1], vup, w->p[1]);

	VectorAdd(org, vright, w->p[2]);
	VectorSubtract(w->p[2], vup, w->p[2]);

	VectorSubtract(org, vright, w->p[3]);
	VectorSubtract(w->p[3], vup, w->p[3]);
	w->numpoints = 4;
	return w;
}

/*
=============
WindingFromDouble
=============
*/
winding_t *WindingFromDouble(const double_winding_t *in)
{
	winding_t *out;
	int i, num;

	out = AllocWinding(in->numpoints);
	num = 0;
	for (i = 0; i < in->numpoints; i++) {
		out->p[num][0] = in->p[i][0];
		out->p[num][1] = in->p[i][1];
		out->p[num][2] = in->p[i][2];
		if (num && VectorCompareEpsilon(out->p[num], out->p[num - 1]))
			continue;
		num++;
	}
	while (num > 0 && VectorCompareEpsilon(out->p[num - 1], out->p[0]))
		num--;
	if (num < 3) {
		FreeWinding(out);
		return NULL;
	}
	out->numpoints = num;
	return out;
}

/*
==================
CopyWinding
==================
*/
winding_t	*CopyWinding (winding_t *w)
{
	int			size;
	winding_t	*c;

	c = AllocWinding (w->numpoints);
	size = (int)((winding_t *)0)->p[w->numpoints];
	memcpy (c, w, size);
	return c;
}

/*
==================
ReverseWinding
==================
*/
winding_t	*ReverseWinding (winding_t *w)
{
	int			i;
	winding_t	*c;

	c = AllocWinding (w->numpoints);
	for (i=0 ; i<w->numpoints ; i++)
	{
		VectorCopy (w->p[w->numpoints-1-i], c->p[i]);
	}
	c->numpoints = w->numpoints;
	return c;
}

/*
=============
ClipWindingEpsilon
=============
*/
void	ClipWindingEpsilon (winding_t *in, vec3_t normal, vec_t dist, 
				vec_t epsilon, winding_t **front, winding_t **back, qboolean keepOn)
{
	vec_t	dists[MAX_POINTS_ON_WINDING+4];
	int		sides[MAX_POINTS_ON_WINDING+4];
	int		counts[3];
	vec_t	dot;
	int		i, j;
	vec_t	*p1, *p2;
	vec3_t	mid;
	winding_t	*f, *b;
	int		maxpts;
	
	counts[0] = counts[1] = counts[2] = 0;

// determine sides for each point
	for (i=0 ; i<in->numpoints ; i++)
	{
		dot = DotProduct (in->p[i], normal);
		dot -= dist;
		dists[i] = dot;
		if (dot > epsilon)
			sides[i] = SIDE_FRONT;
		else if (dot < -epsilon)
			sides[i] = SIDE_BACK;
		else
		{
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];
	
	*front = *back = NULL;

	if (!counts[0])
	{
		*back = CopyWinding (in);
		if (keepOn && !counts[1])
			*front = CopyWinding (in);
		return;
	}
	if (!counts[1])
	{
		*front = CopyWinding (in);
		return;
	}

	maxpts = in->numpoints+4;	// cant use counts[0]+2 because
								// of fp grouping errors

	*front = f = AllocWinding (maxpts);
	*back = b = AllocWinding (maxpts);
		
	for (i=0 ; i<in->numpoints ; i++)
	{
		p1 = in->p[i];
		
		if (sides[i] == SIDE_ON)
		{
			VectorCopy (p1, f->p[f->numpoints]);
			f->numpoints++;
			VectorCopy (p1, b->p[b->numpoints]);
			b->numpoints++;
			continue;
		}
	
		if (sides[i] == SIDE_FRONT)
		{
			VectorCopy (p1, f->p[f->numpoints]);
			f->numpoints++;
		}
		if (sides[i] == SIDE_BACK)
		{
			VectorCopy (p1, b->p[b->numpoints]);
			b->numpoints++;
		}

		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;
			
	// generate a split point
		p2 = in->p[(i+1)%in->numpoints];
		
		for (j=0 ; j<3 ; j++)
		{	// avoid round off error when possible
			if (normal[j] == 1)
				mid[j] = dist;
			else if (normal[j] == -1)
				mid[j] = -dist;
			else
				mid[j] = (p2[j]*dists[i] - p1[j]*dists[i+1]) / (dists[i]-dists[i+1]);
		}
			
		VectorCopy (mid, f->p[f->numpoints]);
		f->numpoints++;
		VectorCopy (mid, b->p[b->numpoints]);
		b->numpoints++;
	}
	
	if (f->numpoints > maxpts || b->numpoints > maxpts)
		Error ("ClipWinding: points exceeded estimate");
	if (f->numpoints > MAX_POINTS_ON_WINDING || b->numpoints > MAX_POINTS_ON_WINDING)
		Error ("ClipWinding: MAX_POINTS_ON_WINDING");
}

/*
=============
ChopWindingInPlace
=============
*/
void ChopWindingInPlace (winding_t **inout, vec3_t normal, vec_t dist, vec_t epsilon)
{
	winding_t	*in;
	vec_t	dists[MAX_POINTS_ON_WINDING+4];
	int		sides[MAX_POINTS_ON_WINDING+4];
	int		counts[3];
	vec_t	dot;
	int		i, j;
	vec_t	*p1, *p2;
	vec3_t	mid;
	winding_t	*f;
	int		maxpts;

	in = *inout;
	counts[0] = counts[1] = counts[2] = 0;

// determine sides for each point
	for (i=0 ; i<in->numpoints ; i++)
	{
		dot = DotProduct (in->p[i], normal);
		dot -= dist;
		dists[i] = dot;
		if (dot > epsilon)
			sides[i] = SIDE_FRONT;
		else if (dot < -epsilon)
			sides[i] = SIDE_BACK;
		else
		{
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];
	
	if (!counts[0])
	{
		FreeWinding (in);
		*inout = NULL;
		return;
	}
	if (!counts[1])
		return;		// inout stays the same

	maxpts = in->numpoints+4;	// cant use counts[0]+2 because
								// of fp grouping errors

	f = AllocWinding (maxpts);
		
	for (i=0 ; i<in->numpoints ; i++)
	{
		p1 = in->p[i];
		
		if (sides[i] == SIDE_ON)
		{
			VectorCopy (p1, f->p[f->numpoints]);
			f->numpoints++;
			continue;
		}
	
		if (sides[i] == SIDE_FRONT)
		{
			VectorCopy (p1, f->p[f->numpoints]);
			f->numpoints++;
		}

		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;
			
	// generate a split point
		p2 = in->p[(i+1)%in->numpoints];
		
		for (j=0 ; j<3 ; j++)
		{	// avoid round off error when possible
			if (normal[j] == 1)
				mid[j] = dist;
			else if (normal[j] == -1)
				mid[j] = -dist;
			else
				mid[j] = (p2[j]*dists[i] - p1[j]*dists[i+1]) / (dists[i]-dists[i+1]);
		}
			
		VectorCopy (mid, f->p[f->numpoints]);
		f->numpoints++;
	}
	
	if (f->numpoints > maxpts)
		Error ("ClipWinding: points exceeded estimate");
	if (f->numpoints > MAX_POINTS_ON_WINDING)
		Error ("ClipWinding: MAX_POINTS_ON_WINDING");

	FreeWinding (in);
	*inout = f;
}

/*
=============
ChopWindingInPlaceDouble
=============
*/
void ChopWindingInPlaceDouble(double_winding_t **inout, vec3_t normal, vec_t dist, vec_t epsilon)
{
	double_winding_t	*in;
	double	dists[MAX_POINTS_ON_WINDING+4];
	int		sides[MAX_POINTS_ON_WINDING+4];
	int		counts[3];
	double	dot;
	int		i, j;
	double	*p1, *p2;
	double	mid[3];
	double_winding_t	*f;
	int		maxpts;

	in = *inout;
	counts[0] = counts[1] = counts[2] = 0;

	for (i=0 ; i<in->numpoints ; i++)
	{
		dot = in->p[i][0] * normal[0] + in->p[i][1] * normal[1] + in->p[i][2] * normal[2];
		dot -= dist;
		dists[i] = dot;
		if (dot > epsilon)
			sides[i] = SIDE_FRONT;
		else if (dot < -epsilon)
			sides[i] = SIDE_BACK;
		else
			sides[i] = SIDE_ON;
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	if (!counts[0])
	{
		FreeWindingDouble(in);
		*inout = NULL;
		return;
	}
	if (!counts[1])
		return;

	maxpts = in->numpoints + 4;
	f = AllocWindingDouble(maxpts);

	for (i=0 ; i<in->numpoints ; i++)
	{
		p1 = in->p[i];

		if (sides[i] == SIDE_ON)
		{
			f->p[f->numpoints][0] = p1[0];
			f->p[f->numpoints][1] = p1[1];
			f->p[f->numpoints][2] = p1[2];
			f->numpoints++;
			continue;
		}

		if (sides[i] == SIDE_FRONT)
		{
			f->p[f->numpoints][0] = p1[0];
			f->p[f->numpoints][1] = p1[1];
			f->p[f->numpoints][2] = p1[2];
			f->numpoints++;
		}

		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		p2 = in->p[(i+1)%in->numpoints];

		for (j=0 ; j<3 ; j++)
		{
			if (normal[j] == 1)
				mid[j] = dist;
			else if (normal[j] == -1)
				mid[j] = -dist;
			else
				mid[j] = (p2[j]*dists[i] - p1[j]*dists[i+1]) / (dists[i]-dists[i+1]);
		}

		f->p[f->numpoints][0] = mid[0];
		f->p[f->numpoints][1] = mid[1];
		f->p[f->numpoints][2] = mid[2];
		f->numpoints++;
	}

	if (f->numpoints > maxpts)
		Error("ClipWinding: points exceeded estimate");
	if (f->numpoints > MAX_POINTS_ON_WINDING)
		Error("ClipWinding: MAX_POINTS_ON_WINDING");

	FreeWindingDouble(in);
	*inout = f;
}

/*
=================
ChopWinding

Returns the fragment of in that is on the front side
of the cliping plane.  The original is freed.
=================
*/
winding_t	*ChopWinding (winding_t *in, vec3_t normal, vec_t dist)
{
	winding_t	*f, *b;

	ClipWindingEpsilon (in, normal, dist, ON_EPSILON, &f, &b, qfalse);
	FreeWinding (in);
	if (b)
		FreeWinding (b);
	return f;
}

/*
=================
CheckWinding

=================
*/
void CheckWinding (winding_t *w)
{
	int		i, j;
	vec_t	*p1, *p2;
	vec_t	d, edgedist;
	vec3_t	dir, edgenormal, facenormal;
	vec_t	area;
	vec_t	facedist;

	if (w->numpoints < 3)
		Error ("CheckWinding: %i points",w->numpoints);
	
	area = WindingArea(w);
	if (area < 1)
		Error ("CheckWinding: %f area", area);

	WindingPlane (w, facenormal, &facedist);
	
	for (i=0 ; i<w->numpoints ; i++)
	{
		p1 = w->p[i];

		for (j=0 ; j<3 ; j++)
			if (p1[j] > MAX_WORLD_COORD || p1[j] < MIN_WORLD_COORD)
				Error ("CheckFace: BUGUS_RANGE: %f",p1[j]);

		j = i+1 == w->numpoints ? 0 : i+1;
		
	// check the point is on the face plane
		d = facenormal[0] * p1[0] + facenormal[1] * p1[1] + facenormal[2] * p1[2] - facedist;
		if (d < -ON_EPSILON || d > ON_EPSILON)
			Error ("CheckWinding: point off plane");
	
	// check the edge isnt degenerate
		p2 = w->p[j];
		VectorSubtract (p2, p1, dir);
		
		if ((float)sqrt(DotProduct(dir, dir)) < ON_EPSILON)
			Error ("CheckWinding: degenerate edge");
			
		CrossProduct (facenormal, dir, edgenormal);
		VectorNormalize (edgenormal);
		edgedist = edgenormal[0] * p1[0] + edgenormal[1] * p1[1] + edgenormal[2] * p1[2];
		edgedist += ON_EPSILON;
		
	// all other points must be on front side
		for (j=0 ; j<w->numpoints ; j++)
		{
			if (j == i)
				continue;
			d = DotProduct (w->p[j], edgenormal);
			if (d > edgedist)
				Error ("CheckWinding: non-convex");
		}
	}
}

/*
============
WindingOnPlaneSide
============
*/
int		WindingOnPlaneSide (winding_t *w, vec3_t normal, vec_t dist)
{
	qboolean	front, back;
	int			i;
	vec_t		d;

	front = qfalse;
	back = qfalse;
	for (i=0 ; i<w->numpoints ; i++)
	{
		d = DotProduct (w->p[i], normal) - dist;
		if (d < -ON_EPSILON)
		{
			if (front)
				return SIDE_CROSS;
			back = qtrue;
			continue;
		}
		if (d > ON_EPSILON)
		{
			if (back)
				return SIDE_CROSS;
			front = qtrue;
			continue;
		}
	}

	if (back)
		return SIDE_BACK;
	if (front)
		return SIDE_FRONT;
	return SIDE_ON;
}

/*
=================
AddWindingToConvexHull

Both w and *hull are on the same plane
=================
*/
void	AddWindingToConvexHull( winding_t *w, winding_t **hull, vec3_t normal ) {
	int			i, j, k;
	float		*p, *copy;
	vec3_t		dir;
	float		d;
	int			numHullPoints, numNew;
	vec3_t		hullPoints[MAX_HULL_POINTS];
	vec3_t		newHullPoints[MAX_HULL_POINTS];
	vec3_t		hullDirs[MAX_HULL_POINTS];
	qboolean	hullSide[MAX_HULL_POINTS];
	qboolean	outside;

	if ( !*hull ) {
		*hull = CopyWinding( w );
		return;
	}

	numHullPoints = (*hull)->numpoints;
	memcpy( hullPoints, (*hull)->p, numHullPoints * sizeof(vec3_t) );

	for ( i = 0 ; i < w->numpoints ; i++ ) {
		p = w->p[i];

		// calculate hull side vectors
		for ( j = 0 ; j < numHullPoints ; j++ ) {
			k = ( j + 1 ) % numHullPoints;

			VectorSubtract( hullPoints[k], hullPoints[j], dir );
			VectorNormalize( dir );
			CrossProduct( normal, dir, hullDirs[j] );
		}

		outside = qfalse;
		for ( j = 0 ; j < numHullPoints ; j++ ) {
			VectorSubtract( p, hullPoints[j], dir );
			d = DotProduct( dir, hullDirs[j] );
			if ( d >= ON_EPSILON ) {
				outside = qtrue;
			}
			if ( d >= -ON_EPSILON ) {
				hullSide[j] = qtrue;
			} else {
				hullSide[j] = qfalse;
			}
		}

		// if the point is effectively inside, do nothing
		if ( !outside ) {
			continue;
		}

		// find the back side to front side transition
		for ( j = 0 ; j < numHullPoints ; j++ ) {
			if ( !hullSide[ j % numHullPoints ] && hullSide[ (j + 1) % numHullPoints ] ) {
				break;
			}
		}
		if ( j == numHullPoints ) {
			continue;
		}

		// insert the point here
		VectorCopy( p, newHullPoints[0] );
		numNew = 1;

		// copy over all points that aren't double fronts
		j = (j+1)%numHullPoints;
		for ( k = 0 ; k < numHullPoints ; k++ ) {
			if ( hullSide[ (j+k) % numHullPoints ] && hullSide[ (j+k+1) % numHullPoints ] ) {
				continue;
			}
			copy = hullPoints[ (j+k+1) % numHullPoints ];
			VectorCopy( copy, newHullPoints[numNew] );
			numNew++;
		}

		numHullPoints = numNew;
		memcpy( hullPoints, newHullPoints, numHullPoints * sizeof(vec3_t) );
	}

	FreeWinding( *hull );
	*hull = AllocWinding( numHullPoints );
	(*hull)->numpoints = numHullPoints;
	memcpy( (*hull)->p, hullPoints, numHullPoints * sizeof(vec3_t) );
}

/*
=============
SnapWindingToPlaneDouble
=============
*/
void SnapWindingToPlaneDouble( double_winding_t *w, vec3_t normal, vec_t dist ) {
	int i, j;
	double point[3], rounded, oldDist, newDist, distance;
	for ( i = 0; i < w->numpoints; i++ ) {
		oldDist = normal[0] * w->p[i][0] + normal[2] * w->p[i][2] +
			normal[1] * w->p[i][1] - dist;
		for ( j = 0; j < 3; j++ ) {
			rounded = RoundToInt( w->p[i][j] );
			if ( fabs( rounded - w->p[i][j] ) <= 10.0 * 0.001f )
				point[j] = rounded;
			else
				point[j] = w->p[i][j];
		}
		newDist = normal[1] * point[1] + normal[2] * point[2] + normal[0] * point[0] - dist;
		distance = oldDist;
		if ( fabs( newDist ) <= fabs( oldDist ) ) {
			VectorCopy( point, w->p[i] );
			distance = newDist;
		}
		if ( distance > 0.001f ) {
			w->p[i][0] -= normal[0] * distance;
			w->p[i][1] -= normal[1] * distance;
			w->p[i][2] -= normal[2] * distance;
		}
	}
}

/*
=============
CheckWindingPlane
=============
*/
void CheckWindingPlane( winding_t *w, vec3_t normal, vec_t dist ) {
}

/*
=============
PointsNearLine
=============
*/
int PointsNearLine( vec3_t pointA, vec3_t pointB, vec3_t lineEnd, vec_t tolerance ) {
	vec3_t delta, direction, projected;
	vec_t projection, distance;

	VectorSubtract( pointB, pointA, delta );
	VectorSubtract( lineEnd, pointA, direction );
	if ( VectorNormalize( direction ) == 0 ) {
		return 0;
	}
	projection = DotProduct( direction, delta );
	VectorScale( direction, projection, projected );
	VectorSubtract( delta, projected, delta );
	distance = sqrt( DotProduct( delta, delta ) );
	if ( distance < tolerance ) {
		return 1;
	}
	return 0;
}

