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

#ifndef Q3MAP_CVAR_H
#define Q3MAP_CVAR_H

#include "../common/cmdlib.h"

#define CVAR_INIT         16
#define CVAR_LATCH        32
#define CVAR_ROM          64
#define CVAR_USER_CREATED 128
#define CVAR_CHEAT        512

typedef struct cvar_s {
	char *name;
	char *string;
	char *resetString;
	char *latchedString;
	int flags;
	qboolean modified;
	int modificationCount;
	float value;
	int integer;
	struct cvar_s *next;
	struct cvar_s *hashNext;
} cvar_t;

cvar_t *Cvar_FindVar( const char *var_name );
cvar_t *Cvar_Get( const char *var_name, const char *var_value, int flags );
cvar_t *Cvar_Set2( const char *var_name, const char *value, qboolean force );
void Cvar_Set( const char *var_name, const char *value );

#endif
