/*
===========================================================================
q3map (Call of Duty, 2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

#ifndef Q3MAP_GAME_Q_MATH_H
#define Q3MAP_GAME_Q_MATH_H

#include "../../common/cmdlib.h"
#include "../../common/mathlib.h"

int Q_rand( int *seed );
float Q_random( int *seed );
float AngleNormalize360( float angle );
float AngleNormalize180( float angle );
void PerpendicularVector( vec3_t dst, const vec3_t src );
void RotateAroundDirection( vec3_t axis[3], float yaw );

#endif
