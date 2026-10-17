/*
===========================================================================
q3map (Call of Duty, 2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

#ifndef Q3MAP_COM_MATH_H
#define Q3MAP_COM_MATH_H

#include "../common/cmdlib.h"
#include "../common/mathlib.h"

/* Distinct from tools/mathlib's two-argument, out-of-place operation. */
vec_t VectorNormalize( vec3_t v );
float VectorNormalize2( const vec3_t v, vec3_t out );
qboolean VectorCompareEpsilon( const vec3_t p1, const vec3_t p2 );
void ExpandBounds( const vec3_t mins, const vec3_t maxs,
				   vec3_t outMins, vec3_t outMaxs );
void ProjectPointOntoVector( const vec3_t point, const vec3_t origin,
							 const vec3_t linePoint, vec3_t out );
void VectorRotateAngles( const vec3_t in, const vec3_t angles, vec3_t out );
void VectorRotateAnglesAroundPoint( const vec3_t point, const vec3_t angles,
							   const vec3_t pivot, vec3_t out );

#endif
