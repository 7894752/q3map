/*
===========================================================================
q3map (Call of Duty, 2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

#ifndef Q3MAP_Q_SHARED_VECTORS_H
#define Q3MAP_Q_SHARED_VECTORS_H

#include "q_shared.h"

#define VectorSet( v, x, y, z ) ( (v)[0] = (x), (v)[1] = (y), (v)[2] = (z) )

float GetLeanFraction( float f );
void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up );
void AddLeanToPosition( float *pos, float yaw, float leanFrac, float leanDist, float scale );

#endif
