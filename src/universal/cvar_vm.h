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

/* VM-facing cvar snapshot layout from Quake III q_shared.h. */
#ifndef Q3MAP_CVAR_VM_H
#define Q3MAP_CVAR_VM_H

#include "cvar.h"

#define MAX_CVARS 1024
#define MAX_CVAR_VALUE_STRING 256

typedef int cvarHandle_t;
typedef struct {
	cvarHandle_t handle;
	int modificationCount;
	float value;
	int integer;
	char string[MAX_CVAR_VALUE_STRING];
} vmCvar_t;

void Cvar_Shutdown( void );
void Cvar_SetValue( const char *var_name, float value );
void Cvar_Reset( const char *var_name );
void Cvar_Update( vmCvar_t *vmCvar );

extern int Com_sprintf( char *dest, int size, const char *format, ... );
extern void Q_strncpyz( char *dest, const char *src, int destsize );

#endif
