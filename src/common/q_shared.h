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

#ifndef Q3MAP_Q_SHARED_H
#define Q3MAP_Q_SHARED_H

#include "cmdlib.h"
#include "mathlib.h"

/* Quake shared-code byte representation; passed and returned by value. */
typedef struct {
	byte b0;
	byte b1;
	byte b2;
	byte b3;
	byte b4;
	byte b5;
	byte b6;
	byte b7;
} qint64;

void Com_BitSet( int array[], int bitNum );
qint64 Long64Swap( qint64 ll );
int Q_islower( int c );
int Q_stricmpn( const char *s1, const char *s2, int n );
char Q_CleanCharacter( char c );
float *tv( float x, float y, float z );

#define MAX_INFO_STRING 1024
#define ERR_DROP 1

void Com_Error( int code, const char *format, ... );
void Com_Printf( const char *format, ... );
int Com_sprintf( char *dest, int size, const char *format, ... );
void Info_RemoveKey( char *s, const char *key );
void Info_SetValueForKey( char *s, const char *key, const char *value );

#endif
