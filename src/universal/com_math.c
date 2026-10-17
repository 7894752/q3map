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

#include "code/game/q_math.h"
#include "universal/com_math.h"

typedef struct cplane_s {
	vec3_t normal;
	float dist;
	byte type;
	byte signbits;
} cplane_t;


#define ERR_DROP 1
void Com_Error( int code, const char *fmt, ... );
#ifndef M_PI
#define M_PI 3.14159265358979323846f /* matches value in gcc v2 math.h */
#endif
#undef DEG2RAD
#undef RAD2DEG
#define DEG2RAD( a ) ( ( (a) * M_PI ) / 180.0F )
#define RAD2DEG( a ) ( ( (a) * 180.0f ) / M_PI )
#define VectorMA( v, s, b, o )     ((o)[0]=(v)[0]+(b)[0]*(s),      (o)[1]=(v)[1]+(b)[1]*(s),      (o)[2]=(v)[2]+(b)[2]*(s))


#ifdef _WIN32
#pragma optimize("p", on)
#endif

const vec3_t vec3_origin = {0, 0, 0};
typedef vec_t vec5_t[5];
#define NUMVERTEXNORMALS 162

vec3_t bytedirs[NUMVERTEXNORMALS] =
{
	{  -0.525731f,   0.000000f,   0.850651f },
	{  -0.442863f,   0.238856f,   0.864188f },
	{  -0.295242f,   0.000000f,   0.955423f },
	{  -0.309017f,   0.500000f,   0.809017f },
	{  -0.162460f,   0.262866f,   0.951056f },
	{   0.000000f,   0.000000f,   1.000000f },
	{   0.000000f,   0.850651f,   0.525731f },
	{  -0.147621f,   0.716567f,   0.681718f },
	{   0.147621f,   0.716567f,   0.681718f },
	{   0.000000f,   0.525731f,   0.850651f },
	{   0.309017f,   0.500000f,   0.809017f },
	{   0.525731f,   0.000000f,   0.850651f },
	{   0.295242f,   0.000000f,   0.955423f },
	{   0.442863f,   0.238856f,   0.864188f },
	{   0.162460f,   0.262866f,   0.951056f },
	{  -0.681718f,   0.147621f,   0.716567f },
	{  -0.809017f,   0.309017f,   0.500000f },
	{  -0.587785f,   0.425325f,   0.688191f },
	{  -0.850651f,   0.525731f,   0.000000f },
	{  -0.864188f,   0.442863f,   0.238856f },
	{  -0.716567f,   0.681718f,   0.147621f },
	{  -0.688191f,   0.587785f,   0.425325f },
	{  -0.500000f,   0.809017f,   0.309017f },
	{  -0.238856f,   0.864188f,   0.442863f },
	{  -0.425325f,   0.688191f,   0.587785f },
	{  -0.716567f,   0.681718f,  -0.147621f },
	{  -0.500000f,   0.809017f,  -0.309017f },
	{  -0.525731f,   0.850651f,   0.000000f },
	{   0.000000f,   0.850651f,  -0.525731f },
	{  -0.238856f,   0.864188f,  -0.442863f },
	{   0.000000f,   0.955423f,  -0.295242f },
	{  -0.262866f,   0.951056f,  -0.162460f },
	{   0.000000f,   1.000000f,   0.000000f },
	{   0.000000f,   0.955423f,   0.295242f },
	{  -0.262866f,   0.951056f,   0.162460f },
	{   0.238856f,   0.864188f,   0.442863f },
	{   0.262866f,   0.951056f,   0.162460f },
	{   0.500000f,   0.809017f,   0.309017f },
	{   0.238856f,   0.864188f,  -0.442863f },
	{   0.262866f,   0.951056f,  -0.162460f },
	{   0.500000f,   0.809017f,  -0.309017f },
	{   0.850651f,   0.525731f,   0.000000f },
	{   0.716567f,   0.681718f,   0.147621f },
	{   0.716567f,   0.681718f,  -0.147621f },
	{   0.525731f,   0.850651f,   0.000000f },
	{   0.425325f,   0.688191f,   0.587785f },
	{   0.864188f,   0.442863f,   0.238856f },
	{   0.688191f,   0.587785f,   0.425325f },
	{   0.809017f,   0.309017f,   0.500000f },
	{   0.681718f,   0.147621f,   0.716567f },
	{   0.587785f,   0.425325f,   0.688191f },
	{   0.955423f,   0.295242f,   0.000000f },
	{   1.000000f,   0.000000f,   0.000000f },
	{   0.951056f,   0.162460f,   0.262866f },
	{   0.850651f,  -0.525731f,   0.000000f },
	{   0.955423f,  -0.295242f,   0.000000f },
	{   0.864188f,  -0.442863f,   0.238856f },
	{   0.951056f,  -0.162460f,   0.262866f },
	{   0.809017f,  -0.309017f,   0.500000f },
	{   0.681718f,  -0.147621f,   0.716567f },
	{   0.850651f,   0.000000f,   0.525731f },
	{   0.864188f,   0.442863f,  -0.238856f },
	{   0.809017f,   0.309017f,  -0.500000f },
	{   0.951056f,   0.162460f,  -0.262866f },
	{   0.525731f,   0.000000f,  -0.850651f },
	{   0.681718f,   0.147621f,  -0.716567f },
	{   0.681718f,  -0.147621f,  -0.716567f },
	{   0.850651f,   0.000000f,  -0.525731f },
	{   0.809017f,  -0.309017f,  -0.500000f },
	{   0.864188f,  -0.442863f,  -0.238856f },
	{   0.951056f,  -0.162460f,  -0.262866f },
	{   0.147621f,   0.716567f,  -0.681718f },
	{   0.309017f,   0.500000f,  -0.809017f },
	{   0.425325f,   0.688191f,  -0.587785f },
	{   0.442863f,   0.238856f,  -0.864188f },
	{   0.587785f,   0.425325f,  -0.688191f },
	{   0.688191f,   0.587785f,  -0.425325f },
	{  -0.147621f,   0.716567f,  -0.681718f },
	{  -0.309017f,   0.500000f,  -0.809017f },
	{   0.000000f,   0.525731f,  -0.850651f },
	{  -0.525731f,   0.000000f,  -0.850651f },
	{  -0.442863f,   0.238856f,  -0.864188f },
	{  -0.295242f,   0.000000f,  -0.955423f },
	{  -0.162460f,   0.262866f,  -0.951056f },
	{   0.000000f,   0.000000f,  -1.000000f },
	{   0.295242f,   0.000000f,  -0.955423f },
	{   0.162460f,   0.262866f,  -0.951056f },
	{  -0.442863f,  -0.238856f,  -0.864188f },
	{  -0.309017f,  -0.500000f,  -0.809017f },
	{  -0.162460f,  -0.262866f,  -0.951056f },
	{   0.000000f,  -0.850651f,  -0.525731f },
	{  -0.147621f,  -0.716567f,  -0.681718f },
	{   0.147621f,  -0.716567f,  -0.681718f },
	{   0.000000f,  -0.525731f,  -0.850651f },
	{   0.309017f,  -0.500000f,  -0.809017f },
	{   0.442863f,  -0.238856f,  -0.864188f },
	{   0.162460f,  -0.262866f,  -0.951056f },
	{   0.238856f,  -0.864188f,  -0.442863f },
	{   0.500000f,  -0.809017f,  -0.309017f },
	{   0.425325f,  -0.688191f,  -0.587785f },
	{   0.716567f,  -0.681718f,  -0.147621f },
	{   0.688191f,  -0.587785f,  -0.425325f },
	{   0.587785f,  -0.425325f,  -0.688191f },
	{   0.000000f,  -0.955423f,  -0.295242f },
	{   0.000000f,  -1.000000f,   0.000000f },
	{   0.262866f,  -0.951056f,  -0.162460f },
	{   0.000000f,  -0.850651f,   0.525731f },
	{   0.000000f,  -0.955423f,   0.295242f },
	{   0.238856f,  -0.864188f,   0.442863f },
	{   0.262866f,  -0.951056f,   0.162460f },
	{   0.500000f,  -0.809017f,   0.309017f },
	{   0.716567f,  -0.681718f,   0.147621f },
	{   0.525731f,  -0.850651f,   0.000000f },
	{  -0.238856f,  -0.864188f,  -0.442863f },
	{  -0.500000f,  -0.809017f,  -0.309017f },
	{  -0.262866f,  -0.951056f,  -0.162460f },
	{  -0.850651f,  -0.525731f,   0.000000f },
	{  -0.716567f,  -0.681718f,  -0.147621f },
	{  -0.716567f,  -0.681718f,   0.147621f },
	{  -0.525731f,  -0.850651f,   0.000000f },
	{  -0.500000f,  -0.809017f,   0.309017f },
	{  -0.238856f,  -0.864188f,   0.442863f },
	{  -0.262866f,  -0.951056f,   0.162460f },
	{  -0.864188f,  -0.442863f,   0.238856f },
	{  -0.809017f,  -0.309017f,   0.500000f },
	{  -0.688191f,  -0.587785f,   0.425325f },
	{  -0.681718f,  -0.147621f,   0.716567f },
	{  -0.442863f,  -0.238856f,   0.864188f },
	{  -0.587785f,  -0.425325f,   0.688191f },
	{  -0.309017f,  -0.500000f,   0.809017f },
	{  -0.147621f,  -0.716567f,   0.681718f },
	{  -0.425325f,  -0.688191f,   0.587785f },
	{  -0.162460f,  -0.262866f,   0.951056f },
	{   0.442863f,  -0.238856f,   0.864188f },
	{   0.162460f,  -0.262866f,   0.951056f },
	{   0.309017f,  -0.500000f,   0.809017f },
	{   0.147621f,  -0.716567f,   0.681718f },
	{   0.000000f,  -0.525731f,   0.850651f },
	{   0.425325f,  -0.688191f,   0.587785f },
	{   0.587785f,  -0.425325f,   0.688191f },
	{   0.688191f,  -0.587785f,   0.425325f },
	{  -0.955423f,   0.295242f,   0.000000f },
	{  -0.951056f,   0.162460f,   0.262866f },
	{  -1.000000f,   0.000000f,   0.000000f },
	{  -0.850651f,   0.000000f,   0.525731f },
	{  -0.955423f,  -0.295242f,   0.000000f },
	{  -0.951056f,  -0.162460f,   0.262866f },
	{  -0.864188f,   0.442863f,  -0.238856f },
	{  -0.951056f,   0.162460f,  -0.262866f },
	{  -0.809017f,   0.309017f,  -0.500000f },
	{  -0.864188f,  -0.442863f,  -0.238856f },
	{  -0.951056f,  -0.162460f,  -0.262866f },
	{  -0.809017f,  -0.309017f,  -0.500000f },
	{  -0.681718f,   0.147621f,  -0.716567f },
	{  -0.681718f,  -0.147621f,  -0.716567f },
	{  -0.850651f,   0.000000f,  -0.525731f },
	{  -0.688191f,   0.587785f,  -0.425325f },
	{  -0.587785f,   0.425325f,  -0.688191f },
	{  -0.425325f,   0.688191f,  -0.587785f },
	{  -0.425325f,  -0.688191f,  -0.587785f },
	{  -0.587785f,  -0.425325f,  -0.688191f },
	{  -0.688191f,  -0.587785f,  -0.425325f },
};
int com_randSeed = 0x89ABCDEF;

int Q_rand( int *seed );
float Q_random( int *seed );
float AngleNormalize360( float angle );
float AngleNormalize180( float angle );
void RotateAroundDirection( vec3_t axis[3], float yaw );
float NormalizeColor( const vec3_t in, vec3_t out );
float LerpAngle( float from, float to, float frac );
float AngleSubtract( float a1, float a2 );
void AnglesSubtract( const vec3_t v1, const vec3_t v2, vec3_t v3 );
float AngleNormalize360Accurate( float angle );
float AngleNormalize180Accurate( float angle );
float Q_crandom( int *seed );
int Q_log2( int val );
float Q_acos( float c );
signed char ClampChar( int i );
signed short ClampShort( int i );
float RadiusFromBounds( const vec3_t mins, const vec3_t maxs );
void NormalToLatLong( const vec3_t normal, byte bytes[2] );
void MakeNormalVectors( const vec3_t forward, vec3_t right, vec3_t up );
void Vec10Copy( vec_t *in, vec_t *out );
void VectorRotate3x3( vec3_t v, float r[3][3], vec3_t d );
float _VectorLength( const vec3_t v );
vec_t Q_rint (vec_t in);
void CrossProduct( const vec3_t v1, const vec3_t v2, vec3_t cross );
vec_t _DotProduct (vec3_t v1, vec3_t v2);
void _VectorSubtract (vec3_t va, vec3_t vb, vec3_t out);
void _VectorAdd (vec3_t va, vec3_t vb, vec3_t out);
void _VectorCopy (vec3_t in, vec3_t out);
void _VectorScale (vec3_t v, vec_t scale, vec3_t out);
vec_t ColorNormalize( const vec3_t in, vec3_t out );
void VectorInverse (vec3_t v);
void ClearBounds (vec3_t mins, vec3_t maxs);
void AddPointToBounds( const vec3_t v, vec3_t mins, vec3_t maxs );
void ExpandBounds( const vec3_t mins, const vec3_t maxs, vec3_t outMins, vec3_t outMaxs );
void MatrixMultiply(float in1[3][3], float in2[3][3], float out[3][3]);
void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal );
void PerpendicularVector( vec3_t dst, const vec3_t src );
void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point,
							 float degrees );
void _Vector5Add (vec5_t va, vec5_t vb, vec5_t out);
void _Vector5Scale (vec5_t v, vec_t scale, vec5_t out);
__inline void SinCos( float radians, float *sinOut, float *cosOut );
__inline void SinCos_double( double radians, double *sinOut, double *cosOut );
qboolean VectorCompareEpsilon( const vec3_t p1, const vec3_t p2 );
void YawVectors( float yaw, vec3_t forward, vec3_t right );
void RotateVector2D( vec3_t v, float degrees );
void VectorPolar( vec3_t out, float radius, float angle );
void RollToQuaternion( float degrees, vec4_t q );
void gunrandom( float *outX, float *outY );
void MatrixMultiplyEquals( const float left[3][3], float inout[3][3] );
float vectopitch( const vec3_t vec );
float vectosignedpitch( const vec3_t vec );
void vectosignedangles( const vec3_t value, vec3_t angles );
void vectoangles( const vec3_t value, vec3_t angles );
float VectorNormalize2D( float v[2] );
float VectorNormalize4D( vec4_t v );
float VectorNormalize2( const vec3_t v, vec3_t out );
float RoundToPrecision( float value, int places );
float QuatEigenTrace( const vec4_t q );
void QuatInverse( const vec4_t in, vec4_t out );
void ConvertQuatToMat( float quatAndMatrix[9] );
void MatrixTransposeTransformVector( const vec3_t in, const float m[4][3], vec3_t out );
void MatrixTranspose( const float in[3][3], float out[3][3] );
void MatrixInverseOrthogonal43( const float in[4][3], float out[4][3] );
vec_t VectorNormalize( vec3_t v );
void VectorRotateAnglesAroundPoint( const vec3_t point, const vec3_t angles,
							   const vec3_t pivot, vec3_t out );
qboolean PlaneFromPoints( vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c );
float VectorDistance( const vec3_t p1, const vec3_t p2 );
float VectorDistanceSquared( const vec3_t p1, const vec3_t p2 );
float VectorDistance2D( const vec3_t p1, const vec3_t p2 );
float VectorDistanceSquared2D( const vec3_t p1, const vec3_t p2 );
void MatrixMultiply34( const float left[3][4], const float right[3][4], float out[3][4] );
void MatrixMultiply43( const float left[4][3], const float right[4][3], float out[4][3] );
void DObjSkelMatrixMultiply43( const float left[4][4], const float right[4][3], float out[4][3] );
void DObjSkel2MatrixMultiply43( const float left[4][4], const float right[4][3], float out[4][4] );
void MatrixTransformVector33( const vec3_t in, const float m[3][3], vec3_t out );
void MatrixTransformVector43( const vec3_t in, const float m[4][3], vec3_t out );
void MatrixTransformVector44( const vec3_t in, const float m[4][4], vec3_t out );
void VectorSnap( vec3_t v );
void NormalFromPoints( const vec3_t a, const vec3_t origin,
						 const vec3_t b, vec3_t normal );
void ProjectPointOntoVector( const vec3_t point, const vec3_t origin,
							 const vec3_t linePoint, vec3_t out );
void AxisClear( vec3_t axis[3] );
float QuatRatioEigenTrace( const vec4_t a, const vec4_t b );
float AngleEigenTrace( float degrees );
byte DirToByte( vec3_t dir );
void ByteToDir( int b, vec3_t dir );
void SetPlaneSignbits( cplane_t *out );
void AnglesToAxis( const vec3_t angles, vec3_t axis[3] );
void YawToAxis( float yaw, vec3_t axis[3] );
float PitchForYawOnNormal( float yaw, const vec3_t normal );
void Com_SRand( int seed );
float Com_RandFloatRange( float lo, float hi );
int Com_RandIntRange( int lo, int hi );
void Vector4Scale( const vec4_t in, float scale, vec4_t out );
void _VectorMA( const vec3_t a, float scale, const vec3_t b, vec3_t out );
float VectorMax( const vec3_t v );
void QuatMultiply( const vec4_t a, const vec4_t b, vec4_t out );
float *MatrixInverse44( const float in[16], float out[16] );
void MatrixTransformVector43Equals( vec3_t v, const float matrix[4][3] );
void _Vector53Copy( const vec3_t in, vec3_t out );
void VectorRotate( vec3_t in, vec3_t matrix[3], vec3_t out );
void AxisCopy( vec3_t in[3], vec3_t out[3] );
float AngleDelta( float angle1, float angle2 );
float vectoyaw( const vec3_t vec );
float vectosignedyaw( const vec3_t vec );
unsigned ColorBytes3( float r, float g, float b );
unsigned ColorBytes4( float r, float g, float b, float a );
float RotationToYaw( const float v[2] );
void VectorNormalizeFast( vec3_t v );
void Perp2D( const vec3_t in, vec3_t out );
void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up );
void MatrixInverse( const float in[9], float out[9] );
void AxisToAngles( const vec3_t axis[3], vec3_t angles );
void Axis4ToAngles( const float matrix[3][4], vec3_t angles );
void AxisToSignedAngles( const vec3_t axis[3], vec3_t angles );
void VectorRotateAngles( const vec3_t in, const vec3_t angles, vec3_t out );
int BoxOnPlaneSide( vec3_t emins, vec3_t emaxs, struct cplane_s *p );

/*
=============
Q_rand
=============
*/
int Q_rand( int *seed ) {
	*seed = 69069 * *seed + 1;
	return *seed;
}

/*
=============
Q_random
=============
*/
float Q_random( int *seed ) {
	return ( Q_rand( seed ) & 0xffff ) / (float)0x10000;
}

/*
=============
Q_crandom
=============
*/
float Q_crandom( int *seed ) {
	return 2.0f * ( Q_random( seed ) - 0.5f );
}

/*
=============
Q_log2
=============
*/
int Q_log2( int val ) {
	int answer;

	answer = 0;
	while ( ( val >>= 1 ) != 0 ) {
		answer++;
	}
	return answer;
}

/*
=====================
Q_acos

the msvc acos doesn't always return a value between -PI and PI:

int i;
i = 1065353246;
acos(*(float*) &i) == -1.#IND0

	This should go in q_math but it is too late to add new traps
	to game and ui
=====================
*/
float Q_acos( float c ) {
	float angle;

	angle = acos( c );
	if ( angle > M_PI ) {
		return M_PI;
	}
	if ( angle < -M_PI ) {
		return M_PI;
	}
	return angle;
}

/*
=============
ClampChar
=============
*/
signed char ClampChar( int i ) {
	if ( i < -128 ) {
		return -128;
	}
	if ( i > 127 ) {
		return 127;
	}
	return i;
}

#pragma optimize("", on)
/* compiled without the float-consistency option, as in the original */
/*
=============
__fsqrt
=============
*/
__inline float __fsqrt( float number ) {
	long i;
	float x2, y;

	x2 = number * 0.5f;
	y = number;
	i = *(long *)&y;
	i = 0x5f3759df - ( i >> 1 );
	y = *(float *)&i;
	y = y * ( 1.5f - ( x2 * y * y ) );

	return y;
}
#pragma optimize("p", on)

/*
=============
ClampShort
=============
*/
signed short ClampShort( int i ) {
	if ( i < -32768 ) {
		return -32768;
	}
	if ( i > 32767 ) {
		return 32767;
	}
	return i;
}

/*
=============
DirToByte
=============
*/
byte DirToByte( vec3_t dir ) {
	byte i, best;
	float d, bestd;

	if ( !dir ) {
		return 0;
	}

	bestd = 0;
	best = 0;
	for ( i = 0 ; i < NUMVERTEXNORMALS ; i++ ) {
		d = DotProduct( dir, bytedirs[i] );
		if ( d > bestd ) {
			bestd = d;
			best = i;
		}
	}

	return best;
}

/*
=============
ByteToDir
=============
*/
void ByteToDir( int b, vec3_t dir ) {
	if ( b < 0 || b >= NUMVERTEXNORMALS ) {
		VectorCopy( vec3_origin, dir );
		return;
	}
	VectorCopy( bytedirs[b], dir );
}

/*
=============
_DotProduct
=============
*/
vec_t _DotProduct (vec3_t v1, vec3_t v2)
{
	return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}

/*
=============
_VectorSubtract
=============
*/
void _VectorSubtract (vec3_t va, vec3_t vb, vec3_t out)
{
	out[0] = va[0]-vb[0];
	out[1] = va[1]-vb[1];
	out[2] = va[2]-vb[2];
}

/*
=============
_VectorAdd
=============
*/
void _VectorAdd (vec3_t va, vec3_t vb, vec3_t out)
{
	out[0] = va[0]+vb[0];
	out[1] = va[1]+vb[1];
	out[2] = va[2]+vb[2];
}

/*
=============
SinCos
=============
*/
__inline void SinCos( float radians, float *sinOut, float *cosOut ) {
	__asm {
		push eax
		push ecx
		fld radians
		fsincos
		mov ecx, cosOut
		mov eax, sinOut
		fstp dword ptr [ecx]
		fstp dword ptr [eax]
		pop ecx
		pop eax
	}
}

/*
=============
_VectorCopy
=============
*/
void _VectorCopy (vec3_t in, vec3_t out)
{
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

/*
=============
SinCos_double
=============
*/
__inline void SinCos_double( double radians, double *sinOut, double *cosOut ) {
	__asm {
		push eax
		push ecx
		fld radians
		fsincos
		mov ecx, cosOut
		mov eax, sinOut
		fstp qword ptr [ecx]
		fstp qword ptr [eax]
		pop ecx
		pop eax
	}
}

/*
=============
_VectorScale
=============
*/
void _VectorScale (vec3_t v, vec_t scale, vec3_t out)
{
	out[0] = v[0] * scale;
	out[1] = v[1] * scale;
	out[2] = v[2] * scale;
}

/*
=============
_VectorMA
=============
*/
void _VectorMA( const vec3_t a, float scale, const vec3_t b, vec3_t out ) {
	out[0] = a[0] + scale * b[0];
	out[1] = a[1] + scale * b[1];
	out[2] = a[2] + scale * b[2];
}

/*
=============
VectorCompareEpsilon
=============
*/
qboolean VectorCompareEpsilon( const vec3_t p1, const vec3_t p2 ) {
	int i;
	for ( i = 0; i < 3; i++ ) {
		if ( ( p1[i] - p2[i] ) * ( p1[i] - p2[i] ) > 0.001f * 0.001f ) return qfalse;
	}
	return qtrue;
}

/*
=============
_VectorLength
=============
*/
float _VectorLength( const vec3_t v ) {
	return (float)sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
}

/*
=============
VectorDistance
=============
*/
float VectorDistance( const vec3_t p1, const vec3_t p2 ) {
	vec3_t delta;

	VectorSubtract( p2, p1, delta );
	return (float)sqrt( delta[2] * delta[2] + delta[1] * delta[1] + delta[0] * delta[0] );
}

/*
=============
VectorDistanceSquared
=============
*/
float VectorDistanceSquared( const vec3_t p1, const vec3_t p2 ) {
	vec3_t delta;

	VectorSubtract( p2, p1, delta );
	return delta[2] * delta[2] + delta[1] * delta[1] + delta[0] * delta[0];
}

/*
=============
VectorDistance2D
=============
*/
float VectorDistance2D( const vec3_t p1, const vec3_t p2 ) {
	float delta[2];

	delta[0] = p2[0] - p1[0];
	delta[1] = p2[1] - p1[1];
	return (float)sqrt( delta[1] * delta[1] + delta[0] * delta[0] );
}

/*
=============
VectorDistanceSquared2D
=============
*/
float VectorDistanceSquared2D( const vec3_t p1, const vec3_t p2 ) {
	float delta[2];

	delta[0] = p2[0] - p1[0];
	delta[1] = p2[1] - p1[1];
	return delta[1] * delta[1] + delta[0] * delta[0];
}

/*
=============
CrossProduct
=============
*/
void CrossProduct( const vec3_t v1, const vec3_t v2, vec3_t cross ) {
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}

/*
=============
Perp2D
=============
*/
void Perp2D( const vec3_t in, vec3_t out ) {
	out[0] = in[1];
	out[1] = -in[0];
	out[2] = 0;
}

/*
=============
VectorNormalize
=============
*/
vec_t VectorNormalize( vec3_t v ) {
	float length, ilength;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	length = (float)sqrt( length );

	if ( length ) {
		ilength = 1.0f / length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}
	return length;
}

/*
=============
VectorNormalize2D
=============
*/
float VectorNormalize2D( float v[2] ) {
	float length, ilength;

	length = v[0] * v[0] + v[1] * v[1];
	length = (float)sqrt( length );
	if ( length != 0.0f ) {
		ilength = 1.0f / length;
		v[0] *= ilength;
		v[1] *= ilength;
	}
	return length;
}

/*
=============
VectorNormalize4D
=============
*/
float VectorNormalize4D( vec4_t v ) {
	float length, ilength;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3];
	length = (float)sqrt( length );
	if ( length != 0.0f ) {
		ilength = 1.0f / length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
		v[3] *= ilength;
	}
	return length;
}

/*
=============
VectorNormalizeFast
=============
*/
void VectorNormalizeFast( vec3_t v ) {
	float ilength;

	ilength = __fsqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
	v[0] *= ilength;
	v[1] *= ilength;
	v[2] *= ilength;
}

/*
=============
VectorNormalize2
=============
*/
float VectorNormalize2( const vec3_t v, vec3_t out ) {
	float length, ilength;

	length = DotProduct( v, v );
	length = (float)sqrt( length );
	if ( length != 0.0f ) {
		ilength = 1.0f / length;
		VectorScale( v, ilength, out );
	} else {
		VectorClear( out );
	}
	return length;
}

/*
=============
VectorInverse
=============
*/
void VectorInverse (vec3_t v)
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

/*
=============
Vector4Scale
=============
*/
void Vector4Scale( const vec4_t in, float scale, vec4_t out ) {
	out[0] = in[0] * scale;
	out[1] = in[1] * scale;
	out[2] = in[2] * scale;
	out[3] = in[3] * scale;
}

/*
=============
VectorMax
=============
*/
float VectorMax( const vec3_t v ) {
	float m;

	m = ( v[0] >= v[1] ) ? v[0] : v[1];
	return ( m >= v[2] ) ? m : v[2];
}

/*
=============
VectorRotate
=============
*/
void VectorRotate( vec3_t in, vec3_t matrix[3], vec3_t out ) {
	out[0] = DotProduct( in, matrix[0] );
	out[1] = DotProduct( in, matrix[1] );
	out[2] = DotProduct( in, matrix[2] );
}

/*
================
MakeNormalVectors

Given a normalized forward vector, create two
other perpendicular vectors
================
*/
void MakeNormalVectors( const vec3_t forward, vec3_t right, vec3_t up ) {
	float d;

	right[1] = -forward[0];
	right[2] = forward[1];
	right[0] = forward[2];
	d = DotProduct( right, forward );
	VectorMA( right, -d, forward, right );
	VectorNormalize( right );
	CrossProduct( right, forward, up );
}

/*
=============
vectoyaw
=============
*/
float vectoyaw( const vec3_t vec ) {
	float yaw;

	if ( vec[1] == 0.0f && vec[0] == 0.0f ) {
		yaw = 0.0f;
	} else {
		yaw = (float)( 180.0 * atan2( vec[1], vec[0] ) / M_PI );
		if ( yaw < 0.0f ) {
			yaw += 360.0f;
		}
	}
	return yaw;
}

/*
=============
vectosignedyaw
=============
*/
float vectosignedyaw( const vec3_t vec ) {
	float yaw;

	if ( vec[1] == 0.0f && vec[0] == 0.0f ) {
		yaw = 0.0f;
	} else {
		yaw = (float)( 180.0 * atan2( vec[1], vec[0] ) / M_PI );
	}
	return yaw;
}

/*
=============
vectopitch
=============
*/
float vectopitch( const vec3_t vec ) {
	float forward, pitch;

	if ( vec[1] == 0.0f && vec[0] == 0.0f ) {
		if ( vec[2] > 0.0f ) {
			pitch = 270.0f;
		} else {
			pitch = 90.0f;
		}
	} else {
		forward = (float)sqrt( vec[0] * vec[0] + vec[1] * vec[1] );
		pitch = (float)( -180.0 * atan2( vec[2], forward ) / M_PI );
		if ( pitch < 0.0f ) {
			pitch += 360.0f;
		}
	}
	return pitch;
}

/*
=============
vectosignedpitch
=============
*/
float vectosignedpitch( const vec3_t vec ) {
	float forward, pitch;

	if ( vec[1] == 0.0f && vec[0] == 0.0f ) {
		if ( vec[2] > 0.0f ) {
			pitch = -90.0f;
		} else {
			pitch = 90.0f;
		}
	} else {
		forward = (float)sqrt( vec[0] * vec[0] + vec[1] * vec[1] );
		pitch = (float)( -180.0 * atan2( vec[2], forward ) / M_PI );
	}
	return pitch;
}

/*
=============
vectoangles
=============
*/
void vectoangles( const vec3_t value, vec3_t angles ) {
	float forward, yaw, pitch;

	if ( value[1] == 0.0f && value[0] == 0.0f ) {
		yaw = 0.0f;
		pitch = ( value[2] > 0.0f ) ? 270.0f : 90.0f;
	} else {
		yaw = (float)( 180.0 * atan2( value[1], value[0] ) / M_PI );
		if ( yaw < 0.0f ) {
			yaw += 360.0f;
		}
		forward = (float)sqrt( value[0] * value[0] + value[1] * value[1] );
		pitch = (float)( -180.0 * atan2( value[2], forward ) / M_PI );
		if ( pitch < 0.0f ) {
			pitch += 360.0f;
		}
	}
	angles[0] = pitch;
	angles[1] = yaw;
	angles[2] = 0.0f;
}

/*
=============
vectosignedangles
=============
*/
void vectosignedangles( const vec3_t value, vec3_t angles ) {
	float forward, yaw, pitch;

	if ( value[1] == 0.0f && value[0] == 0.0f ) {
		yaw = 0.0f;
		pitch = ( value[2] > 0.0f ) ? -90.0f : 90.0f;
	} else {
		yaw = (float)( 180.0 * atan2( value[1], value[0] ) / M_PI );
		forward = (float)sqrt( value[0] * value[0] + value[1] * value[1] );
		pitch = (float)( -180.0 * atan2( value[2], forward ) / M_PI );
	}
	angles[0] = pitch;
	angles[1] = yaw;
	angles[2] = 0.0f;
}

/*
=============
AngleVectors
=============
*/
void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up ) {
	float angle;
	float sr, sp, sy, cr, cp, cy;

	angle = angles[1] * ( M_PI * 2 / 360 );
	SinCos( angle, &sy, &cy );

	angle = angles[0] * ( M_PI * 2 / 360 );
	SinCos( angle, &sp, &cp );

	if ( forward ) {
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;
	}

	if ( !right && !up ) {
		return;
	}

	angle = angles[2] * ( M_PI * 2 / 360 );
	SinCos( angle, &sr, &cr );

	if ( right ) {
		right[0] = ( -1 * sr * sp * cy + -1 * cr * -sy );
		right[1] = ( -1 * sr * sp * sy + -1 * cr * cy );
		right[2] = -1 * sr * cp;
	}

	if ( up ) {
		up[0] = ( cr * sp * cy + -sr * -sy );
		up[1] = ( cr * sp * sy + -sr * cy );
		up[2] = cr * cp;
	}
}

/*
=============
YawVectors
=============
*/
void YawVectors( float yaw, vec3_t forward, vec3_t right ) {
	float angle, s, c;
	angle = yaw * ( M_PI * 2 / 360 );
	SinCos( angle, &s, &c );
	if ( forward ) {
		forward[0] = c;
		forward[1] = s;
		forward[2] = 0;
	}
	if ( right ) {
		right[0] = s;
		right[1] = -c;
		right[2] = 0;
	}
}

/*
=============
NormalFromPoints
=============
*/
void NormalFromPoints( const vec3_t a, const vec3_t origin,
						 const vec3_t b, vec3_t normal ) {
	vec3_t v1, v2;

	VectorSubtract( a, origin, v1 );
	VectorNormalize( v1 );
	VectorSubtract( a, b, v2 );
	VectorNormalize( v2 );
	CrossProduct( v1, v2, normal );
	VectorNormalize( normal );
}

/*
================
ProjectPointOntoVector
================
*/
void ProjectPointOntoVector( const vec3_t point, const vec3_t origin,
							 const vec3_t linePoint, vec3_t out ) {
	vec3_t rel, dir;
	float d;

	VectorSubtract( point, origin, rel );
	VectorSubtract( linePoint, origin, dir );
	VectorNormalize( dir );
	d = DotProduct( dir, rel );
	VectorMA( origin, d, dir, out );
}

/*
================
MatrixMultiply
================
*/
void MatrixMultiply(float in1[3][3], float in2[3][3], float out[3][3]) {
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
				in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
				in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
				in1[0][2] * in2[2][2];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
				in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
				in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
				in1[1][2] * in2[2][2];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
				in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
				in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
				in1[2][2] * in2[2][2];
}

/*
=============
MatrixMultiplyEquals
=============
*/
void MatrixMultiplyEquals( const float left[3][3], float inout[3][3] ) {
	vec3_t rows[2];

	rows[0][0] = left[0][0] * inout[0][0] + left[0][1] * inout[1][0] +
		left[0][2] * inout[2][0];
	rows[0][1] = left[0][0] * inout[0][1] + left[0][1] * inout[1][1] +
		left[0][2] * inout[2][1];
	rows[0][2] = left[0][0] * inout[0][2] + left[0][1] * inout[1][2] +
		left[0][2] * inout[2][2];
	rows[1][0] = left[1][0] * inout[0][0] + left[1][1] * inout[1][0] +
		left[1][2] * inout[2][0];
	rows[1][1] = left[1][0] * inout[0][1] + left[1][1] * inout[1][1] +
		left[1][2] * inout[2][1];
	rows[1][2] = left[1][0] * inout[0][2] + left[1][1] * inout[1][2] +
		left[1][2] * inout[2][2];
	inout[2][0] = left[2][0] * inout[0][0] + left[2][1] * inout[1][0] +
		left[2][2] * inout[2][0];
	inout[2][1] = left[2][0] * inout[0][1] + left[2][1] * inout[1][1] +
		left[2][2] * inout[2][1];
	inout[2][2] = left[2][0] * inout[0][2] + left[2][1] * inout[1][2] +
		left[2][2] * inout[2][2];
	VectorCopy( rows[0], inout[0] );
	VectorCopy( rows[1], inout[1] );
}

/*
=============
MatrixMultiply34
=============
*/
void MatrixMultiply34( const float left[3][4], const float right[3][4], float out[3][4] ) {
	out[0][0] = left[0][0] * right[0][0] + left[0][1] * right[1][0] +
		left[0][2] * right[2][0];
	out[0][1] = left[0][0] * right[0][1] + left[0][1] * right[1][1] +
		left[0][2] * right[2][1];
	out[0][2] = left[0][0] * right[0][2] + left[0][1] * right[1][2] +
		left[0][2] * right[2][2];
	out[0][3] = left[0][0] * right[0][3] + left[0][1] * right[1][3] +
		left[0][2] * right[2][3] + left[0][3];
	out[1][0] = left[1][0] * right[0][0] + left[1][1] * right[1][0] +
		left[1][2] * right[2][0];
	out[1][1] = left[1][0] * right[0][1] + left[1][1] * right[1][1] +
		left[1][2] * right[2][1];
	out[1][2] = left[1][0] * right[0][2] + left[1][1] * right[1][2] +
		left[1][2] * right[2][2];
	out[1][3] = left[1][0] * right[0][3] + left[1][1] * right[1][3] +
		left[1][2] * right[2][3] + left[1][3];
	out[2][0] = left[2][0] * right[0][0] + left[2][1] * right[1][0] +
		left[2][2] * right[2][0];
	out[2][1] = left[2][0] * right[0][1] + left[2][1] * right[1][1] +
		left[2][2] * right[2][1];
	out[2][2] = left[2][0] * right[0][2] + left[2][1] * right[1][2] +
		left[2][2] * right[2][2];
	out[2][3] = left[2][0] * right[0][3] + left[2][1] * right[1][3] +
		left[2][2] * right[2][3] + left[2][3];
}

/*
=============
MatrixMultiply43
=============
*/
void MatrixMultiply43( const float left[4][3], const float right[4][3], float out[4][3] ) {
	out[0][0] = left[0][0] * right[0][0] + left[0][1] * right[1][0] +
		left[0][2] * right[2][0];
	out[1][0] = left[1][0] * right[0][0] + left[1][1] * right[1][0] +
		left[1][2] * right[2][0];
	out[2][0] = left[2][0] * right[0][0] + left[2][1] * right[1][0] +
		left[2][2] * right[2][0];
	out[0][1] = left[0][0] * right[0][1] + left[0][1] * right[1][1] +
		left[0][2] * right[2][1];
	out[1][1] = left[1][0] * right[0][1] + left[1][1] * right[1][1] +
		left[1][2] * right[2][1];
	out[2][1] = left[2][0] * right[0][1] + left[2][1] * right[1][1] +
		left[2][2] * right[2][1];
	out[0][2] = left[0][0] * right[0][2] + left[0][1] * right[1][2] +
		left[0][2] * right[2][2];
	out[1][2] = left[1][0] * right[0][2] + left[1][1] * right[1][2] +
		left[1][2] * right[2][2];
	out[2][2] = left[2][0] * right[0][2] + left[2][1] * right[1][2] +
		left[2][2] * right[2][2];
	out[3][0] = left[3][0] * right[0][0] + left[3][1] * right[1][0] +
		left[3][2] * right[2][0] + right[3][0];
	out[3][1] = left[3][0] * right[0][1] + left[3][1] * right[1][1] +
		left[3][2] * right[2][1] + right[3][1];
	out[3][2] = left[3][0] * right[0][2] + left[3][1] * right[1][2] +
		left[3][2] * right[2][2] + right[3][2];
}

/*
=============
DObjSkelMatrixMultiply43
=============
*/
void DObjSkelMatrixMultiply43( const float left[4][4], const float right[4][3], float out[4][3] ) {
	out[0][0] = left[0][0] * right[0][0] + left[0][1] * right[1][0] +
		left[0][2] * right[2][0];
	out[1][0] = left[1][0] * right[0][0] + left[1][1] * right[1][0] +
		left[1][2] * right[2][0];
	out[2][0] = left[2][0] * right[0][0] + left[2][1] * right[1][0] +
		left[2][2] * right[2][0];
	out[0][1] = left[0][0] * right[0][1] + left[0][1] * right[1][1] +
		left[0][2] * right[2][1];
	out[1][1] = left[1][0] * right[0][1] + left[1][1] * right[1][1] +
		left[1][2] * right[2][1];
	out[2][1] = left[2][0] * right[0][1] + left[2][1] * right[1][1] +
		left[2][2] * right[2][1];
	out[0][2] = left[0][0] * right[0][2] + left[0][1] * right[1][2] +
		left[0][2] * right[2][2];
	out[1][2] = left[1][0] * right[0][2] + left[1][1] * right[1][2] +
		left[1][2] * right[2][2];
	out[2][2] = left[2][0] * right[0][2] + left[2][1] * right[1][2] +
		left[2][2] * right[2][2];
	out[3][0] = left[3][0] * right[0][0] + left[3][1] * right[1][0] +
		left[3][2] * right[2][0] + right[3][0];
	out[3][1] = left[3][0] * right[0][1] + left[3][1] * right[1][1] +
		left[3][2] * right[2][1] + right[3][1];
	out[3][2] = left[3][0] * right[0][2] + left[3][1] * right[1][2] +
		left[3][2] * right[2][2] + right[3][2];
}

/*
=============
DObjSkel2MatrixMultiply43
=============
*/
void DObjSkel2MatrixMultiply43( const float left[4][4], const float right[4][3], float out[4][4] ) {
	out[0][0] = left[0][0] * right[0][0] + left[0][1] * right[1][0] +
		left[0][2] * right[2][0];
	out[1][0] = left[1][0] * right[0][0] + left[1][1] * right[1][0] +
		left[1][2] * right[2][0];
	out[2][0] = left[2][0] * right[0][0] + left[2][1] * right[1][0] +
		left[2][2] * right[2][0];
	out[0][1] = left[0][0] * right[0][1] + left[0][1] * right[1][1] +
		left[0][2] * right[2][1];
	out[1][1] = left[1][0] * right[0][1] + left[1][1] * right[1][1] +
		left[1][2] * right[2][1];
	out[2][1] = left[2][0] * right[0][1] + left[2][1] * right[1][1] +
		left[2][2] * right[2][1];
	out[0][2] = left[0][0] * right[0][2] + left[0][1] * right[1][2] +
		left[0][2] * right[2][2];
	out[1][2] = left[1][0] * right[0][2] + left[1][1] * right[1][2] +
		left[1][2] * right[2][2];
	out[2][2] = left[2][0] * right[0][2] + left[2][1] * right[1][2] +
		left[2][2] * right[2][2];
	out[0][3] = 0.0f;
	out[1][3] = 0.0f;
	out[2][3] = 0.0f;
	out[3][0] = left[3][0] * right[0][0] + left[3][1] * right[1][0] +
		left[3][2] * right[2][0] + right[3][0];
	out[3][1] = left[3][0] * right[0][1] + left[3][1] * right[1][1] +
		left[3][2] * right[2][1] + right[3][1];
	out[3][2] = left[3][0] * right[0][2] + left[3][1] * right[1][2] +
		left[3][2] * right[2][2] + right[3][2];
	out[3][3] = 1.0f;
}

/*
=============
MatrixTranspose
=============
*/
void MatrixTranspose( const float in[3][3], float out[3][3] ) {
	out[0][0] = in[0][0];
	out[0][1] = in[1][0];
	out[0][2] = in[2][0];
	out[1][0] = in[0][1];
	out[1][1] = in[1][1];
	out[1][2] = in[2][1];
	out[2][0] = in[0][2];
	out[2][1] = in[1][2];
	out[2][2] = in[2][2];
}

/*
=============
MatrixInverse
=============
*/
void MatrixInverse( const float in[9], float out[9] ) {
	float determinant, inverseDeterminant;

	determinant = ( in[4] * in[8] - in[7] * in[5] ) * in[0]
		- ( in[1] * in[8] - in[7] * in[2] ) * in[3]
		+ ( in[1] * in[5] - in[2] * in[4] ) * in[6];
	inverseDeterminant = 1.0f / determinant;

	out[0] =  ( in[4] * in[8] - in[7] * in[5] ) * inverseDeterminant;
	out[1] = -( in[1] * in[8] - in[7] * in[2] ) * inverseDeterminant;
	out[2] =  ( in[1] * in[5] - in[2] * in[4] ) * inverseDeterminant;
	out[3] = -( in[3] * in[8] - in[6] * in[5] ) * inverseDeterminant;
	out[4] =  ( in[0] * in[8] - in[2] * in[6] ) * inverseDeterminant;
	out[5] = -( in[0] * in[5] - in[2] * in[3] ) * inverseDeterminant;
	out[6] =  ( in[7] * in[3] - in[6] * in[4] ) * inverseDeterminant;
	out[7] = -( in[7] * in[0] - in[1] * in[6] ) * inverseDeterminant;
	out[8] =  ( in[4] * in[0] - in[1] * in[3] ) * inverseDeterminant;
}

/*
=============
MatrixInverse44
=============
*/
float *MatrixInverse44( const float in[16], float out[16] ) {
	float pair[12];
	float transposed[16];
	float determinant;
	int i;

	for ( i = 0; i < 4; ++i ) {
		transposed[i] = in[i * 4];
		transposed[i + 4] = in[i * 4 + 1];
		transposed[i + 8] = in[i * 4 + 2];
		transposed[i + 12] = in[i * 4 + 3];
	}

	pair[0] = transposed[10] * transposed[15];
	pair[1] = transposed[11] * transposed[14];
	pair[2] = transposed[9] * transposed[15];
	pair[3] = transposed[11] * transposed[13];
	pair[4] = transposed[9] * transposed[14];
	pair[5] = transposed[10] * transposed[13];
	pair[6] = transposed[8] * transposed[15];
	pair[7] = transposed[11] * transposed[12];
	pair[8] = transposed[8] * transposed[14];
	pair[9] = transposed[10] * transposed[12];
	pair[10] = transposed[8] * transposed[13];
	pair[11] = transposed[9] * transposed[12];

	out[0] = pair[0] * transposed[5] + pair[3] * transposed[6] + pair[4] * transposed[7];
	out[0] -= pair[1] * transposed[5] + pair[2] * transposed[6] + pair[5] * transposed[7];
	out[1] = pair[1] * transposed[4] + pair[6] * transposed[6] + pair[9] * transposed[7];
	out[1] -= pair[0] * transposed[4] + pair[7] * transposed[6] + pair[8] * transposed[7];
	out[2] = pair[2] * transposed[4] + pair[7] * transposed[5] + pair[10] * transposed[7];
	out[2] -= pair[3] * transposed[4] + pair[6] * transposed[5] + pair[11] * transposed[7];
	out[3] = pair[5] * transposed[4] + pair[8] * transposed[5] + pair[11] * transposed[6];
	out[3] -= pair[4] * transposed[4] + pair[9] * transposed[5] + pair[10] * transposed[6];
	out[4] = pair[1] * transposed[1] + pair[2] * transposed[2] + pair[5] * transposed[3];
	out[4] -= pair[0] * transposed[1] + pair[3] * transposed[2] + pair[4] * transposed[3];
	out[5] = pair[0] * transposed[0] + pair[7] * transposed[2] + pair[8] * transposed[3];
	out[5] -= pair[1] * transposed[0] + pair[6] * transposed[2] + pair[9] * transposed[3];
	out[6] = pair[3] * transposed[0] + pair[6] * transposed[1] + pair[11] * transposed[3];
	out[6] -= pair[2] * transposed[0] + pair[7] * transposed[1] + pair[10] * transposed[3];
	out[7] = pair[4] * transposed[0] + pair[9] * transposed[1] + pair[10] * transposed[2];
	out[7] -= pair[5] * transposed[0] + pair[8] * transposed[1] + pair[11] * transposed[2];

	pair[0] = transposed[2] * transposed[7];
	pair[1] = transposed[3] * transposed[6];
	pair[2] = transposed[1] * transposed[7];
	pair[3] = transposed[3] * transposed[5];
	pair[4] = transposed[1] * transposed[6];
	pair[5] = transposed[2] * transposed[5];
	pair[6] = transposed[0] * transposed[7];
	pair[7] = transposed[3] * transposed[4];
	pair[8] = transposed[0] * transposed[6];
	pair[9] = transposed[2] * transposed[4];
	pair[10] = transposed[0] * transposed[5];
	pair[11] = transposed[1] * transposed[4];

	out[8] = pair[0] * transposed[13] + pair[3] * transposed[14] + pair[4] * transposed[15];
	out[8] -= pair[1] * transposed[13] + pair[2] * transposed[14] + pair[5] * transposed[15];
	out[9] = pair[1] * transposed[12] + pair[6] * transposed[14] + pair[9] * transposed[15];
	out[9] -= pair[0] * transposed[12] + pair[7] * transposed[14] + pair[8] * transposed[15];
	out[10] = pair[2] * transposed[12] + pair[7] * transposed[13] + pair[10] * transposed[15];
	out[10] -= pair[3] * transposed[12] + pair[6] * transposed[13] + pair[11] * transposed[15];
	out[11] = pair[5] * transposed[12] + pair[8] * transposed[13] + pair[11] * transposed[14];
	out[11] -= pair[4] * transposed[12] + pair[9] * transposed[13] + pair[10] * transposed[14];
	out[12] = pair[2] * transposed[10] + pair[5] * transposed[11] + pair[1] * transposed[9];
	out[12] -= pair[4] * transposed[11] + pair[0] * transposed[9] + pair[3] * transposed[10];
	out[13] = pair[8] * transposed[11] + pair[0] * transposed[8] + pair[7] * transposed[10];
	out[13] -= pair[6] * transposed[10] + pair[9] * transposed[11] + pair[1] * transposed[8];
	out[14] = pair[6] * transposed[9] + pair[11] * transposed[11] + pair[3] * transposed[8];
	out[14] -= pair[10] * transposed[11] + pair[2] * transposed[8] + pair[7] * transposed[9];
	out[15] = pair[10] * transposed[10] + pair[4] * transposed[8] + pair[9] * transposed[9];
	out[15] -= pair[8] * transposed[9] + pair[11] * transposed[10] + pair[5] * transposed[8];

	determinant = transposed[0] * out[0] + transposed[1] * out[1]
		+ transposed[2] * out[2] + transposed[3] * out[3];
	determinant = 1.0f / determinant;
	for ( i = 0; i < 16; ++i ) {
		out[i] *= determinant;
	}
	return out;
}

/*
=============
VectorRotate3x3
=============
*/
void VectorRotate3x3( vec3_t v, float r[3][3], vec3_t d )
{
	d[0] = v[0] * r[0][0] + v[1] * r[1][0] + v[2] * r[2][0];
	d[1] = v[0] * r[0][1] + v[1] * r[1][1] + v[2] * r[2][1];
	d[2] = v[0] * r[0][2] + v[1] * r[1][2] + v[2] * r[2][2];
}

/*
=============
MatrixTransformVector33
=============
*/
void MatrixTransformVector33( const vec3_t in, const float m[3][3], vec3_t out ) {
	out[0] = in[0] * m[0][0] + in[1] * m[0][1] + in[2] * m[0][2];
	out[1] = in[0] * m[1][0] + in[1] * m[1][1] + in[2] * m[1][2];
	out[2] = in[0] * m[2][0] + in[1] * m[2][1] + in[2] * m[2][2];
}

/*
=============
MatrixTransformVector43
=============
*/
void MatrixTransformVector43( const vec3_t in, const float m[4][3], vec3_t out ) {
	out[0] = in[0] * m[0][0] + in[1] * m[1][0] + in[2] * m[2][0] + m[3][0];
	out[1] = in[0] * m[0][1] + in[1] * m[1][1] + in[2] * m[2][1] + m[3][1];
	out[2] = in[0] * m[0][2] + in[1] * m[1][2] + in[2] * m[2][2] + m[3][2];
}

/*
=============
MatrixTransformVector44
=============
*/
void MatrixTransformVector44( const vec3_t in, const float m[4][4], vec3_t out ) {
	out[0] = in[0] * m[0][0] + in[1] * m[1][0] + in[2] * m[2][0] + m[3][0];
	out[1] = in[0] * m[0][1] + in[1] * m[1][1] + in[2] * m[2][1] + m[3][1];
	out[2] = in[0] * m[0][2] + in[1] * m[1][2] + in[2] * m[2][2] + m[3][2];
}

/*
=============
MatrixTransposeTransformVector
=============
*/
void MatrixTransposeTransformVector( const vec3_t in, const float m[4][3], vec3_t out ) {
	vec3_t local;

	VectorSubtract( in, m[3], local );
	out[0] = DotProduct( local, m[0] );
	out[1] = DotProduct( local, m[1] );
	out[2] = DotProduct( local, m[2] );
}

/*
=============
MatrixTransformVector43Equals
=============
*/
void MatrixTransformVector43Equals( vec3_t v, const float matrix[4][3] ) {
	vec2_t transformed;

	transformed[0] = v[0] * matrix[0][0] + v[1] * matrix[1][0] +
		v[2] * matrix[2][0] + matrix[3][0];
	transformed[1] = v[0] * matrix[0][1] + v[1] * matrix[1][1] +
		v[2] * matrix[2][1] + matrix[3][1];
	v[2] = v[0] * matrix[0][2] + v[1] * matrix[1][2] +
		v[2] * matrix[2][2] + matrix[3][2];
	v[0] = transformed[0];
	v[1] = transformed[1];
}

/*
=============
RotateVector2D
=============
*/
void RotateVector2D( vec3_t v, float degrees ) {
	float angle, s, c, x;
	angle = degrees * ( M_PI * 2 / 360 );
	SinCos( angle, &s, &c );
	x = c * v[0] - s * v[1];
	v[1] = s * v[0] + c * v[1];
	v[0] = x;
}

/*
=============
QuatMultiply
=============
*/
void QuatMultiply( const vec4_t a, const vec4_t b, vec4_t out ) {
	out[0] = a[0] * b[3] + a[3] * b[0] + a[2] * b[1] - a[1] * b[2];
	out[1] = a[1] * b[3] - a[2] * b[0] + a[3] * b[1] + a[0] * b[2];
	out[2] = a[2] * b[3] + a[1] * b[0] - a[0] * b[1] + a[3] * b[2];
	out[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
}

/*
=============
QuatInverse
=============
*/
void QuatInverse( const vec4_t in, vec4_t out ) {
	out[0] = -in[0];
	out[1] = -in[1];
	out[2] = -in[2];
	out[3] = in[3];
}

/*
=============
ConvertQuatToMat
=============
*/
void ConvertQuatToMat( float quatAndMatrix[9] ) {
	float xx, yy, zz, len2, s;
	float xs, ys;
	float xy, xz, xw, yz, yw, zw;

	xx = quatAndMatrix[0] * quatAndMatrix[0];
	yy = quatAndMatrix[1] * quatAndMatrix[1];
	zz = quatAndMatrix[2] * quatAndMatrix[2];
	len2 = quatAndMatrix[3] * quatAndMatrix[3] + zz + yy + xx;

	if ( len2 != 0.0f ) {
		s = 2.0f / len2;

		xx *= s;
		yy *= s;
		zz *= s;

		xs = s * quatAndMatrix[0];
		xy = xs * quatAndMatrix[1];
		xz = xs * quatAndMatrix[2];
		xw = xs * quatAndMatrix[3];

		ys = s * quatAndMatrix[1];
		yz = ys * quatAndMatrix[2];
		yw = ys * quatAndMatrix[3];

		zw = quatAndMatrix[3] * quatAndMatrix[2] * s;

		quatAndMatrix[0] = 1.0f - ( zz + yy );
		quatAndMatrix[1] = zw + xy;
		quatAndMatrix[2] = xz - yw;
		quatAndMatrix[3] = xy - zw;
		quatAndMatrix[4] = 1.0f - ( zz + xx );
		quatAndMatrix[5] = yz + xw;
		quatAndMatrix[6] = yw + xz;
		quatAndMatrix[7] = yz - xw;
		quatAndMatrix[8] = 1.0f - ( yy + xx );
	} else {
		quatAndMatrix[0] = 1.0f;
		quatAndMatrix[1] = 0.0f;
		quatAndMatrix[2] = 0.0f;
		quatAndMatrix[3] = 0.0f;
		quatAndMatrix[4] = 1.0f;
		quatAndMatrix[5] = 0.0f;
		quatAndMatrix[6] = 0.0f;
		quatAndMatrix[7] = 0.0f;
		quatAndMatrix[8] = 1.0f;
	}
}

/*
=============
QuatEigenTrace
=============
*/
float QuatEigenTrace( const vec4_t q ) {
	float xx, yy, zz, lengthSquared;

	xx = q[0] * q[0];
	yy = q[1] * q[1];
	zz = q[2] * q[2];
	lengthSquared = q[3] * q[3] + zz + yy + xx;
	if ( lengthSquared != 0.0f ) {
		lengthSquared = 1.0f / lengthSquared;
		xx *= lengthSquared;
		yy *= lengthSquared;
		zz *= lengthSquared;
		return zz + yy + xx;
	}
	return 0.0f;
}

/*
=============
AngleEigenTrace
=============
*/
float AngleEigenTrace( float degrees ) {
	double s = sin( degrees * ( M_PI / 180.0f ) );

	return (float)( s * s );
}

/*
=============
QuatRatioEigenTrace
=============
*/
float QuatRatioEigenTrace( const vec4_t a, const vec4_t b ) {
	vec4_t conj, delta;

	QuatInverse( b, conj );
	QuatMultiply( a, conj, delta );
	return QuatEigenTrace( delta );
}

/*
=============
RotationToYaw
=============
*/
float RotationToYaw( const float v[2] ) {
	float xx, scale;

	xx = v[0] * v[0];
	scale = xx + v[1] * v[1];
	scale = 2.0f / scale;

	return (float)( atan2( v[0] * v[1] * scale, 1.0f - xx * scale ) *
		( 180.0f / M_PI ) );
}

/*
=============
PitchToQuaternion
=============
*/
void PitchToQuaternion( float degrees, vec4_t q ) {
	degrees = degrees * ( M_PI / 360.0f );
	q[0] = 0.0f;
	q[2] = 0.0f;
	SinCos( degrees, &q[1], &q[3] );
}

/*
=============
YawToQuaternion
=============
*/
void YawToQuaternion( float degrees, vec4_t q ) {
	degrees = degrees * ( M_PI / 360.0f );
	q[0] = 0.0f;
	q[1] = 0.0f;
	SinCos( degrees, &q[2], &q[3] );
}

/*
=============
RollToQuaternion
=============
*/
void RollToQuaternion( float degrees, vec4_t q ) {
	degrees = degrees * ( M_PI / 360.0f );
	q[1] = 0.0f;
	q[2] = 0.0f;
	SinCos( degrees, &q[0], &q[3] );
}

/*
=============
ColorBytes3
=============
*/
unsigned ColorBytes3( float r, float g, float b ) {
	unsigned i;

	( (byte *)&i )[0] = r * 255;
	( (byte *)&i )[1] = g * 255;
	( (byte *)&i )[2] = b * 255;
	( (byte *)&i )[3] = 0xff;

	return i;
}

/*
=============
ColorBytes4
=============
*/
unsigned ColorBytes4( float r, float g, float b, float a ) {
	unsigned i;

	( (byte *)&i )[0] = r * 255;
	( (byte *)&i )[1] = g * 255;
	( (byte *)&i )[2] = b * 255;
	( (byte *)&i )[3] = a * 255;

	return i;
}

/*
=============
NormalizeColor
=============
*/
float NormalizeColor( const vec3_t in, vec3_t out ) {
	float max;

	max = in[0];
	if ( in[1] > max ) {
		max = in[1];
	}
	if ( in[2] > max ) {
		max = in[2];
	}
	if ( !max ) {
		VectorClear( out );
	} else {
		out[0] = in[0] / max;
		out[1] = in[1] / max;
		out[2] = in[2] / max;
	}
	return max;
}

/*
=============
AngleMod
=============
*/
float AngleMod( float a ) {
	return ( 360.0f / 65536 ) * ( (int)( a * ( 65536 / 360.0f ) ) & 65535 );
}

/*
===============
LerpAngle

===============
*/
float LerpAngle( float from, float to, float frac ) {
	float a;

	a = to;
	if ( to - from > 180 ) {
		a -= 360;
	}
	if ( a - from < -180 ) {
		a += 360;
	}
	from = from + frac * ( a - from );
	return from;
}

/*
=================
AngleSubtract

Always returns a value from -180 to 180
=================
*/
float AngleSubtract( float a1, float a2 ) {
	float a;

	a = a1 - a2;
	while ( a > 180 ) {
		a -= 360;
	}
	while ( a < -180 ) {
		a += 360;
	}
	return a;
}

/*
=============
AnglesSubtract
=============
*/
void AnglesSubtract( const vec3_t v1, const vec3_t v2, vec3_t v3 ) {
	v3[0] = AngleSubtract( v1[0], v2[0] );
	v3[1] = AngleSubtract( v1[1], v2[1] );
	v3[2] = AngleSubtract( v1[2], v2[2] );
}

/*
=================
AngleNormalize360

returns angle normalized to the range [0 <= angle < 360]
=================
*/
float AngleNormalize360( float angle ) {
	return ( 360.0f / 65536 ) * ( (int)( angle * ( 65536 / 360.0f ) ) & 65535 );
}

/*
=================
AngleNormalize180

returns angle normalized to the range [-180 < angle <= 180]
=================
*/
float AngleNormalize180( float angle ) {
	float normalized;

	normalized = AngleNormalize360( angle );
	if ( normalized > 180.0f ) {
		normalized -= 360.0f;
	}
	return normalized;
}

/*
=============
AngleNormalize360Accurate
=============
*/
float AngleNormalize360Accurate( float angle ) {
	if ( angle < 0.0f ) {
		do {
			angle += 360.0f;
		} while ( angle < 0.0f );
	} else {
		while ( angle >= 360.0f ) {
			angle -= 360.0f;
		}
	}
	return angle;
}

/*
=============
AngleNormalize180Accurate
=============
*/
float AngleNormalize180Accurate( float angle ) {
	if ( angle <= -180.0f ) {
		do {
			angle += 360.0f;
		} while ( angle <= -180.0f );
	} else {
		while ( angle > 180.0f ) {
			angle -= 360.0f;
		}
	}
	return angle;
}

/*
=================
AngleDelta

returns the normalized delta from angle1 to angle2
=================
*/
float AngleDelta( float angle1, float angle2 ) {
	return AngleNormalize180( angle1 - angle2 );
}

/*
=================
RadiusFromBounds
=================
*/
float RadiusFromBounds( const vec3_t mins, const vec3_t maxs ) {
	int i;
	vec3_t corner;
	float a, b;

	for ( i = 0; i < 3; i++ ) {
		a = fabs( mins[i] );
		b = fabs( maxs[i] );
		corner[i] = a > b ? a : b;
	}
	return sqrt( DotProduct( corner, corner ) );
}

/*
=============
ClearBounds
=============
*/
void ClearBounds (vec3_t mins, vec3_t maxs)
{
	mins[0] = mins[1] = mins[2] = 262144;
	maxs[0] = maxs[1] = maxs[2] = -262144;
}

/*
=============
AddPointToBounds
=============
*/
void AddPointToBounds( const vec3_t v, vec3_t mins, vec3_t maxs ) {
	if ( v[0] < mins[0] ) {
		mins[0] = v[0];
	}
	if ( v[0] > maxs[0] ) {
		maxs[0] = v[0];
	}
	if ( v[1] < mins[1] ) {
		mins[1] = v[1];
	}
	if ( v[1] > maxs[1] ) {
		maxs[1] = v[1];
	}
	if ( v[2] < mins[2] ) {
		mins[2] = v[2];
	}
	if ( v[2] > maxs[2] ) {
		maxs[2] = v[2];
	}
}

/*
=============
ExpandBounds
=============
*/
void ExpandBounds( const vec3_t mins, const vec3_t maxs, vec3_t outMins, vec3_t outMaxs ) {
	int i;

	for ( i = 0; i < 3; i++ ) {
		if ( outMins[i] > mins[i] ) {
			outMins[i] = mins[i];
		}
		if ( outMaxs[i] < maxs[i] ) {
			outMaxs[i] = maxs[i];
		}
	}
}

/*
=============
AxisClear
=============
*/
void AxisClear( vec3_t axis[3] ) {
	axis[0][0] = 1;
	axis[0][1] = 0;
	axis[0][2] = 0;
	axis[1][0] = 0;
	axis[1][1] = 1;
	axis[1][2] = 0;
	axis[2][0] = 0;
	axis[2][1] = 0;
	axis[2][2] = 1;
}

/*
=============
AxisCopy
=============
*/
void AxisCopy( vec3_t in[3], vec3_t out[3] ) {
	VectorCopy( in[0], out[0] );
	VectorCopy( in[1], out[1] );
	VectorCopy( in[2], out[2] );
}

/*
=================
AnglesToAxis
=================
*/
void AnglesToAxis( const vec3_t angles, vec3_t axis[3] ) {
	vec3_t right;

	AngleVectors( angles, axis[0], right, axis[2] );
	VectorSubtract( vec3_origin, right, axis[1] );
}

/*
=============
YawToAxis
=============
*/
void YawToAxis( float yaw, vec3_t axis[3] ) {
	vec3_t right;

	YawVectors( yaw, axis[0], right );
	axis[2][0] = 0;
	axis[2][1] = 0;
	axis[2][2] = 1;
	VectorSubtract( vec3_origin, right, axis[1] );
}

/*
=================
AxisToAngles

  Used to convert the MD3 tag axis to MDC tag angles, which are much smaller

  This doesn't have to be fast, since it's only used for conversion in utils, try to avoid
  using this during gameplay
=================
*/
void AxisToAngles( const vec3_t axis[3], vec3_t angles ) {
	vec3_t right;
	float angle, s, c, t, roll;

	// first get the pitch and yaw from the forward vector
	vectoangles( axis[0], angles );

	// now get the roll from the right vector, reverse-rotated by yaw then pitch
	VectorCopy( axis[1], right );

	angle = ( -angles[1] ) * M_PI / 180.0f;
	SinCos( angle, &s, &c );
	t        = c * right[0] - s * right[1];
	right[1] = c * right[1] + s * right[0];

	angle = ( -angles[0] ) * M_PI / 180.0f;
	SinCos( angle, &s, &c );
	right[0] = t * c + s * right[2];
	right[2] = c * right[2] - t * s;

	roll = vectosignedpitch( right );
	if ( right[1] < 0.0f ) {
		angles[2] = ( roll < 0.0f ? 180.0f : -180.0f ) + roll;
	} else {
		angles[2] = -roll;
	}
}

/*
=============
Axis4ToAngles
=============
*/
void Axis4ToAngles( const float matrix[3][4], vec3_t angles ) {
	vec3_t right;
	float angle, s, c, t, roll;

	// first get the pitch and yaw from the forward vector
	vectoangles( matrix[0], angles );

	// now get the roll from the right vector, reverse-rotated by yaw then pitch
	VectorCopy( matrix[1], right );

	angle = ( -angles[1] ) * M_PI / 180.0f;
	SinCos( angle, &s, &c );
	t        = c * right[0] - s * right[1];
	right[1] = c * right[1] + s * right[0];

	angle = ( -angles[0] ) * M_PI / 180.0f;
	SinCos( angle, &s, &c );
	right[0] = t * c + s * right[2];
	right[2] = c * right[2] - t * s;

	roll = vectosignedpitch( right );
	if ( right[1] < 0.0f ) {
		angles[2] = ( roll < 0.0f ? 180.0f : -180.0f ) + roll;
	} else {
		angles[2] = -roll;
	}
}

/*
=============
AxisToSignedAngles
=============
*/
void AxisToSignedAngles( const vec3_t axis[3], vec3_t angles ) {
	vec3_t right;
	float angle, s, c, t, roll;

	// first get the pitch and yaw from the forward vector
	vectosignedangles( axis[0], angles );

	// now get the roll from the right vector, reverse-rotated by yaw then pitch
	VectorCopy( axis[1], right );

	angle = ( -angles[1] ) * M_PI / 180.0f;
	SinCos( angle, &s, &c );
	t        = c * right[0] - s * right[1];
	right[1] = c * right[1] + s * right[0];

	angle = ( -angles[0] ) * M_PI / 180.0f;
	SinCos( angle, &s, &c );
	right[0] = t * c + s * right[2];
	right[2] = c * right[2] - t * s;

	roll = vectosignedpitch( right );
	if ( right[1] < 0.0f ) {
		angles[2] = ( roll < 0.0f ? 180.0f : -180.0f ) + roll;
	} else {
		angles[2] = -roll;
	}
}

/*
=====================
PlaneFromPoints

Returns false if the triangle is degenrate.
The normal will point out of the clock for clockwise ordered points
=====================
*/
qboolean PlaneFromPoints( vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c ) {
	vec3_t d1, d2;

	VectorSubtract( b, a, d1 );
	VectorSubtract( c, a, d2 );
	CrossProduct( d2, d1, plane );
	if ( VectorNormalize( plane ) == 0 ) {
		return qfalse;
	}

	plane[3] = DotProduct( a, plane );
	return qtrue;
}

/*
=============
ProjectPointOnPlane
=============
*/
void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal )
{
	float d;
	vec3_t n;
	float inv_denom;

	inv_denom = DotProduct( normal, normal );
	inv_denom = 1.0F / inv_denom;

	d = DotProduct( normal, p ) * inv_denom;

	n[0] = normal[0] * inv_denom;
	n[1] = normal[1] * inv_denom;
	n[2] = normal[2] * inv_denom;

	dst[0] = p[0] - d * n[0];
	dst[1] = p[1] - d * n[1];
	dst[2] = p[2] - d * n[2];
}

/*
=================
SetPlaneSignbits
=================
*/
void SetPlaneSignbits( cplane_t *out ) {
	byte bits;
	int j;

	bits = 0;
	for ( j = 0; j < 3; j++ ) {
		if ( out->normal[j] < 0 ) {
			bits |= 1 << j;
		}
	}
	out->signbits = bits;
}

#pragma warning( disable: 4035 )

/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2

// this is the slow, general version
int BoxOnPlaneSide2 (vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	int		i;
	float	dist1, dist2;
	int		sides;
	vec3_t	corners[2];

	for (i=0 ; i<3 ; i++)
	{
		if (p->normal[i] < 0)
		{
			corners[0][i] = emins[i];
			corners[1][i] = emaxs[i];
		}
		else
		{
			corners[1][i] = emins[i];
			corners[0][i] = emaxs[i];
		}
	}
	dist1 = DotProduct (p->normal, corners[0]) - p->dist;
	dist2 = DotProduct (p->normal, corners[1]) - p->dist;
	sides = 0;
	if (dist1 >= 0)
		sides = 1;
	if (dist2 < 0)
		sides |= 2;

	return sides;
}

==================
*/
__declspec( naked ) int BoxOnPlaneSide( vec3_t emins, vec3_t emaxs, struct cplane_s *p )
{
	static int bops_initialized;
	static int Ljmptab[8];

	__asm {

		push ebx
			
		cmp bops_initialized, 1
		je  initialized
		mov bops_initialized, 1
		
		mov Ljmptab[0*4], offset Lcase0
		mov Ljmptab[1*4], offset Lcase1
		mov Ljmptab[2*4], offset Lcase2
		mov Ljmptab[3*4], offset Lcase3
		mov Ljmptab[4*4], offset Lcase4
		mov Ljmptab[5*4], offset Lcase5
		mov Ljmptab[6*4], offset Lcase6
		mov Ljmptab[7*4], offset Lcase7
			
initialized:

		mov edx,dword ptr[4+12+esp]
		mov ecx,dword ptr[4+4+esp]
		xor eax,eax
		mov ebx,dword ptr[4+8+esp]
		mov al,byte ptr[17+edx]
		cmp al,8
		jae Lerror
		fld dword ptr[0+edx]
		fld st(0)
		jmp dword ptr[Ljmptab+eax*4]
Lcase0:
		fmul dword ptr[ebx]
		fld dword ptr[0+4+edx]
		fxch st(2)
		fmul dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[4+ebx]
		fld dword ptr[0+8+edx]
		fxch st(2)
		fmul dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase1:
		fmul dword ptr[ecx]
		fld dword ptr[0+4+edx]
		fxch st(2)
		fmul dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[4+ebx]
		fld dword ptr[0+8+edx]
		fxch st(2)
		fmul dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase2:
		fmul dword ptr[ebx]
		fld dword ptr[0+4+edx]
		fxch st(2)
		fmul dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[4+ecx]
		fld dword ptr[0+8+edx]
		fxch st(2)
		fmul dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase3:
		fmul dword ptr[ecx]
		fld dword ptr[0+4+edx]
		fxch st(2)
		fmul dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[4+ecx]
		fld dword ptr[0+8+edx]
		fxch st(2)
		fmul dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase4:
		fmul dword ptr[ebx]
		fld dword ptr[0+4+edx]
		fxch st(2)
		fmul dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[4+ebx]
		fld dword ptr[0+8+edx]
		fxch st(2)
		fmul dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase5:
		fmul dword ptr[ecx]
		fld dword ptr[0+4+edx]
		fxch st(2)
		fmul dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[4+ebx]
		fld dword ptr[0+8+edx]
		fxch st(2)
		fmul dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase6:
		fmul dword ptr[ebx]
		fld dword ptr[0+4+edx]
		fxch st(2)
		fmul dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[4+ecx]
		fld dword ptr[0+8+edx]
		fxch st(2)
		fmul dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase7:
		fmul dword ptr[ecx]
		fld dword ptr[0+4+edx]
		fxch st(2)
		fmul dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[4+ecx]
		fld dword ptr[0+8+edx]
		fxch st(2)
		fmul dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
LSetSides:
		faddp st(2),st(0)
		fcomp dword ptr[12+edx]
		xor ecx,ecx
		fnstsw ax
		fcomp dword ptr[12+edx]
		and ah,1
		xor ah,1
		add cl,ah
		fnstsw ax
		and ah,1
		add ah,ah
		add cl,ah
		pop ebx
		mov eax,ecx
		ret
Lerror:
		int 3
	}
	Com_Error( ERR_DROP, "\x15" "BoxOnPlaneSide: invalid signbits for plane" );
	__asm int 3
}
#pragma warning( default: 4035 )

/*
=============
NormalToLatLong
=============
*/
void NormalToLatLong( const vec3_t normal, byte bytes[2] ) {
	// check for singularities
	if ( normal[0] == 0 && normal[1] == 0 ) {
		if ( normal[2] > 0 ) {
			bytes[0] = 0;
			bytes[1] = 0;		// lat = 0, long = 0
		} else {
			bytes[0] = 128;
			bytes[1] = 0;		// lat = 0, long = 128
		}
	} else {
		byte a, b;

		a = (int)( RAD2DEG( atan2( normal[1], normal[0] ) ) * (255.0f / 360.0f ) );

		b = (int)( RAD2DEG( acos( normal[2] ) ) * ( 255.0f / 360.0f ) );

		bytes[0] = b;	// longitude
		bytes[1] = a;	// lattitude
	}
}

/*
=============
Vec10Copy
=============
*/
void Vec10Copy( vec_t *in, vec_t *out ) {
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
	out[3] = in[3];
	out[4] = in[4];
	out[5] = in[5];
	out[6] = in[6];
	out[7] = in[7];
	out[8] = in[8];
	out[9] = in[9];
}

/*
=============
Q_rint
=============
*/
vec_t Q_rint (vec_t in)
{
	return floor (in + 0.5f);
}

/*
=============
ColorNormalize
=============
*/
vec_t ColorNormalize( const vec3_t in, vec3_t out ) {
	float	max, scale;

	max = in[0];
	if (in[1] > max)
		max = in[1];
	if (in[2] > max)
		max = in[2];

	if (max == 0) {
		out[0] = out[1] = out[2] = 1.0;
		return 0;
	}

	scale = 1.0f / max;

	VectorScale (in, scale, out);

	return max;
}

/*
=============
VectorRotateAngles
=============
*/
void VectorRotateAngles( const vec3_t in, const vec3_t angles, vec3_t out ) {
	vec3_t v, r;
	int pair[3][2];
	double s, c;
	int i;
	float *rj, *rk;

	VectorCopy( in, v );
	VectorCopy( v, r );
	pair[0][0] = 1; pair[0][1] = 2;
	pair[1][0] = 2; pair[1][1] = 0;
	pair[2][0] = 0; pair[2][1] = 1;

	for ( i = 0 ; i < 3 ; i++ ) {
		if ( angles[i] != 0.0f ) {
			SinCos_double( (angles[i] * M_PI / 180.0), &s, &c );

			/* the original uses component pointers and redundant parentheses here; plain indexing behaves the same */
			rj = r + pair[i][0]; rk = r + pair[i][1];
			*rj = v[pair[i][0]] * c - v[pair[i][1]] * s;
			*rk = v[pair[i][1]] * c + v[pair[i][0]] * s;
		}
		VectorCopy( r, v );
	}

	VectorCopy( r, out );
}

/*
=============
VectorRotateAnglesAroundPoint
=============
*/
void VectorRotateAnglesAroundPoint( const vec3_t point, const vec3_t angles,
							   const vec3_t pivot, vec3_t out ) {
	vec3_t rel, res;

	VectorSubtract( point, pivot, rel );
	VectorRotateAngles( rel, angles, res );
	VectorAdd( res, pivot, out );
}

/*
=============
VectorPolar
=============
*/
void VectorPolar( vec3_t out, float radius, float angle ) {
	float s1, c1, s2, c2;
	SinCos( angle, &s1, &c1 );
	SinCos( angle, &s2, &c2 );
	out[0] = c2 * c1 * radius;
	out[1] = c2 * s1 * radius;
	out[2] = s2 * radius;
}

/*
=============
VectorSnap
=============
*/
void VectorSnap( vec3_t v ) {
	v[0] = floor( v[0] + 0.5f );
	v[1] = floor( v[1] + 0.5f );
	v[2] = floor( v[2] + 0.5f );
}

/*
=============
_Vector5Add
=============
*/
void _Vector5Add (vec5_t va, vec5_t vb, vec5_t out)
{
	out[0] = va[0]+vb[0];
	out[1] = va[1]+vb[1];
	out[2] = va[2]+vb[2];
	out[3] = va[3]+vb[3];
	out[4] = va[4]+vb[4];
}

/*
=============
_Vector5Scale
=============
*/
void _Vector5Scale (vec5_t v, vec_t scale, vec5_t out)
{
	out[0] = v[0] * scale;
	out[1] = v[1] * scale;
	out[2] = v[2] * scale;
	out[3] = v[3] * scale;
	out[4] = v[4] * scale;
}

/*
=============
_Vector53Copy
=============
*/
void _Vector53Copy( const vec3_t in, vec3_t out ) {
	VectorCopy( in, out );
}

/*
=============
RoundToPrecision
=============
*/
float RoundToPrecision( float value, int places ) {
	double scaled, integral, frac;

	scaled = pow( 10.0, places ) * value;
	frac = modf( scaled, &integral );
	if ( frac >= 0.5 ) {
		integral += 1.0;
	} else if ( frac <= -0.5 ) {
		integral -= 1.0;
	}
	return (float)( pow( 0.1, places ) * integral );
}

/*
=============
PitchForYawOnNormal
=============
*/
float PitchForYawOnNormal( float yaw, const vec3_t normal ) {
	vec3_t forward, projected;

	YawVectors( yaw, forward, NULL );
	ProjectPointOnPlane( projected, forward, normal );
	return vectopitch( projected );
}

/*
=============
Com_SRand
=============
*/
void Com_SRand( int seed ) {
	com_randSeed = seed;
}

/*
=============
Com_RandFloatRange
=============
*/
float Com_RandFloatRange( float lo, float hi ) {
	float value;

	com_randSeed = 214013 * com_randSeed + 2531011;
	value = (unsigned)com_randSeed >> 17;
	value = ( hi - lo ) * value / 32768.0f + lo;
	return value;
}

/*
=============
Com_RandIntRange
=============
*/
int Com_RandIntRange( int lo, int hi ) {
	com_randSeed = 214013 * com_randSeed + 2531011;
	return ( (int)( (unsigned)com_randSeed >> 17 ) * ( hi - lo ) >> 15 ) + lo;
}

/*
=============
gunrandom
=============
*/
void gunrandom( float *outX, float *outY ) {
	float degrees, radius, s, c;
	degrees = (float)rand() / 32768.0f * 360.0f;
	radius = (float)rand() / 32768.0f;
	SinCos( degrees * M_PI / 180.0f, &s, &c );
	*outX = c * radius;
	*outY = s * radius;
}

/*
=============
PerpendicularVector
=============
*/
void PerpendicularVector( vec3_t dst, const vec3_t src )
{
	int	pos;
	int i;
	float minelem = 1.0F;
	vec3_t tempvec;

	/*
	** find the smallest magnitude axially aligned vector
	*/
	for ( pos = 0, i = 0; i < 3; i++ )
	{
		if ( fabs( src[i] ) < minelem )
		{
			pos = i;
			minelem = fabs( src[i] );
		}
	}
	tempvec[0] = tempvec[1] = tempvec[2] = 0.0F;
	tempvec[pos] = 1.0F;

	/*
	** project the point onto the plane defined by src
	*/
	ProjectPointOnPlane( dst, tempvec, src );

	/*
	** normalize the result
	*/
	VectorNormalize( dst );
}

/*
=============
MatrixInverseOrthogonal43
=============
*/
void MatrixInverseOrthogonal43( const float in[4][3], float out[4][3] ) {
	vec3_t translation;

	MatrixTranspose( in, out );
	VectorSubtract( vec3_origin, in[3], translation );
	VectorRotate3x3( translation, out, out[3] );
}

/*
===============
RotatePointAroundVector

This is not implemented very well...
===============
*/
void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point,
							 float degrees ) {
	float	m[3][3];
	float	im[3][3];
	float	zrot[3][3];
	float	tmpmat[3][3];
	float	rot[3][3];
	int	i;
	vec3_t vr, vup, vf;
	float	rad;

	vf[0] = dir[0];
	vf[1] = dir[1];
	vf[2] = dir[2];

	PerpendicularVector( vr, dir );
	CrossProduct( vr, vf, vup );

	m[0][0] = vr[0];
	m[1][0] = vr[1];
	m[2][0] = vr[2];

	m[0][1] = vup[0];
	m[1][1] = vup[1];
	m[2][1] = vup[2];

	m[0][2] = vf[0];
	m[1][2] = vf[1];
	m[2][2] = vf[2];

	memcpy( im, m, sizeof( im ) );

	im[0][1] = m[1][0];
	im[0][2] = m[2][0];
	im[1][0] = m[0][1];
	im[1][2] = m[2][1];
	im[2][0] = m[0][2];
	im[2][1] = m[1][2];

	memset( zrot, 0, sizeof( zrot ) );
	zrot[0][0] = zrot[1][1] = zrot[2][2] = 1.0F;

	rad = DEG2RAD( degrees );
	SinCos( rad, &zrot[0][1], &zrot[0][0] );
	zrot[1][0] = -zrot[0][1];
	zrot[1][1] = zrot[0][0];

	MatrixMultiply( m, zrot, tmpmat );
	MatrixMultiply( tmpmat, im, rot );

	for ( i = 0; i < 3; i++ ) {
		dst[i] = rot[i][0] * point[0] + rot[i][1] * point[1] + rot[i][2] * point[2];
	}
}

/*
===============
RotateAroundDirection
===============
*/
void RotateAroundDirection( vec3_t axis[3], float yaw ) {
	PerpendicularVector( axis[1], axis[0] );

	if ( yaw ) {
		vec3_t temp;

		VectorCopy( axis[1], temp );
		RotatePointAroundVector( axis[1], axis[0], temp, yaw );
	}

	CrossProduct( axis[0], axis[1], axis[2] );
}
