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

#include "cvar_vm.h"

void Z_FreeInternal( void *block );

#define FILE_HASH_SIZE 256
#define CVAR_UNSAFE 0x1000
enum { ERR_FATAL, ERR_DROP };

cvar_t *cvar_vars;
cvar_t *cvar_cheats;
int cvar_modifiedFlags;
cvar_t cvar_indexes[MAX_CVARS];
int cvar_numIndexes;
static cvar_t *cvar_hashTable[FILE_HASH_SIZE];

extern void Com_Error( int code, const char *format, ... );
extern void Com_Printf( const char *format, ... );
extern void Com_DPrintf( const char *format, ... );
extern char *va( const char *format, ... );
extern char *CopyStringInternal( const char *string );

enum { CVAR_SYSTEMINFO = 8 };

/*
=======================
generateHashValue
=======================
*/
static long generateHashValue( const char *fname ) {
	int i;
	long hash;
	int letter;

	if ( !fname ) {
		Com_Error( ERR_DROP, "\x15" "null name in generateHashValue" );
	}
	hash = 0;
	i = 0;
	while ( fname[i] != '\0' ) {
		letter = tolower( fname[i] );
		hash += (long)( letter ) * ( i + 119 );
		i++;
	}
	hash &= ( FILE_HASH_SIZE - 1 );
	return hash;
}

/*
============
Cvar_ValidateString
============
*/
static qboolean Cvar_ValidateString( const char *s ) {
	if ( !s ) {
		return qfalse;
	}
	if ( strchr( s, '\\' ) ) {
		return qfalse;
	}
	if ( strchr( s, '\"' ) ) {
		return qfalse;
	}
	if ( strchr( s, ';' ) ) {
		return qfalse;
	}
	return qtrue;
}

/*
============
Cvar_FindVar
============
*/
cvar_t *Cvar_FindVar( const char *var_name ) {
	cvar_t  *var;
	long hash;

	hash = generateHashValue( var_name );

	for ( var = cvar_hashTable[hash] ; var ; var = var->hashNext ) {
		if ( !Q_stricmp( var_name, var->name ) ) {
			return var;
		}
	}

	return NULL;
}

/*
=============
Cvar_Shutdown
=============
*/
void Cvar_Shutdown( void ) {
	unsigned int i;
	cvar_t  *var;

	for ( i = 0 ; i < FILE_HASH_SIZE ; i++ ) {
		for ( var = cvar_hashTable[i] ; var ; var = var->hashNext ) {
			if ( var->latchedString ) {
				Z_FreeInternal( var->latchedString );
				var->latchedString = NULL;
			}
			if ( var->string ) {
				Z_FreeInternal( var->string );
				var->string = NULL;
			}
			if ( var->resetString ) {
				Z_FreeInternal( var->resetString );
				var->resetString = NULL;
			}
			Z_FreeInternal( var->name );
			var->name = NULL;
		}
	}
}

/*
============
Cvar_Set2
============
*/
cvar_t *Cvar_Set2( const char *var_name, const char *value, qboolean force ) {
	cvar_t  *var;

	if ( !Cvar_ValidateString( var_name ) ) {
		Com_Error( ERR_FATAL, va( "invalid cvar name string: %s", var_name ) );
	}

	var = Cvar_FindVar( var_name );
	if ( !var ) {
		if ( !value ) {
			return NULL;
		}
		if ( !force ) {
			return Cvar_Get( var_name, value, CVAR_USER_CREATED );
		} else {
			return Cvar_Get( var_name, value, 0 );
		}
	}

	if ( !value ) {
		value = var->resetString;
	}

	if ( !strcmp( value, var->string ) ) {
		if ( ( var->flags & CVAR_LATCH ) && var->latchedString ) {
			Z_FreeInternal( var->latchedString );
			var->latchedString = NULL;
		}
		return var;
	}

	cvar_modifiedFlags |= var->flags;

	if ( !force ) {
		if ( var->flags & CVAR_ROM ) {
			Com_Printf( "%s is read only.\n", var_name );
			return var;
		}

		if ( var->flags & CVAR_INIT ) {
			Com_Printf( "%s is write protected.\n", var_name );
			return var;
		}

		if ( ( var->flags & CVAR_CHEAT ) && !cvar_cheats->integer ) {
			Com_Printf( "%s is cheat protected.\n", var_name );
			return var;
		}

		if ( var->flags & CVAR_LATCH ) {
			if ( var->latchedString ) {
				if ( !strcmp( value, var->latchedString ) ) {
					return var;
				}
				Z_FreeInternal( var->latchedString );
			} else {
				if ( !strcmp( value, var->string ) ) {
					return var;
				}
			}

			Com_Printf( "%s will be changed upon restarting.\n", var_name );
			var->latchedString = CopyStringInternal( value );
			var->modified = qtrue;
			return var;
		}
	} else {
		if ( var->latchedString ) {
			Z_FreeInternal( var->latchedString );
			var->latchedString = NULL;
		}
	}

	if ( !strcmp( value, var->string ) ) {
		return var;
	}

	var->modified = qtrue;
	var->modificationCount++;

	Z_FreeInternal( var->string );

	var->string = CopyStringInternal( value );
	var->value = atof( var->string );
	var->integer = atoi( var->string );

	return var;
}

/*
============
Cvar_Set
============
*/
void Cvar_Set( const char *var_name, const char *value ) {
	Cvar_Set2( var_name, value, qtrue );
}

/*
============
Cvar_SetLatched
============
*/
void Cvar_SetLatched( const char *var_name, const char *value ) {
	Cvar_Set2( var_name, value, qfalse );
}

/*
============
Cvar_SetValue
============
*/
void Cvar_SetValue( const char *var_name, float value ) {
	char val[32];

	if ( value == (int)value ) {
		Com_sprintf( val, sizeof( val ), "%i",(int)value );
	} else {
		Com_sprintf( val, sizeof( val ), "%f",value );
	}
	Cvar_Set( var_name, val );
}

/*
============
Cvar_Reset
============
*/
void Cvar_Reset( const char *var_name ) {
	Cvar_Set2( var_name, NULL, qfalse );
}

/*
=====================
Cvar_Update

updates an interpreted modules' version of a cvar
=====================
*/
void    Cvar_Update( vmCvar_t *vmCvar ) {
	cvar_t  *cv;

	if ( (unsigned)vmCvar->handle >= cvar_numIndexes ) {
		Com_Error( ERR_DROP, "\x15" "Cvar_Update: handle out of range" );
	}

	cv = cvar_indexes + vmCvar->handle;

	if ( cv->modificationCount == vmCvar->modificationCount ) {
		return;
	}
	if ( !cv->string ) {
		return;
	}
	vmCvar->modificationCount = cv->modificationCount;
	if ( strlen( cv->string ) + 1 > MAX_CVAR_VALUE_STRING ) {
		Com_Error( ERR_DROP, "\x15" "Cvar_Update: src %s length %d exceeds MAX_CVAR_VALUE_STRING",
				   cv->string,
				   strlen( cv->string ), MAX_CVAR_VALUE_STRING );
	}
	Q_strncpyz( vmCvar->string, cv->string, MAX_CVAR_VALUE_STRING );

	vmCvar->value = cv->value;
	vmCvar->integer = cv->integer;
}

/*
============
Cvar_Get

If the variable already exists, the value will not be set unless CVAR_ROM
The flags will be or'ed in if the variable exists.
============
*/
cvar_t *Cvar_Get( const char *var_name, const char *var_value, int flags ) {
	cvar_t  *var;
	long hash;
	cvar_t  **prev;

	if ( !var_name || !var_value ) {
		Com_Error( ERR_FATAL, "\x15" "Cvar_Get: NULL parameter" );
	}

	if ( !Cvar_ValidateString( var_name ) ) {
		Com_Error( ERR_FATAL, va( "invalid cvar name string: %s", var_name ) );
	}

	var = Cvar_FindVar( var_name );
	if ( var ) {
		if ( ( var->flags & ( CVAR_USER_CREATED | CVAR_UNSAFE ) )
			 && !( flags & ( CVAR_USER_CREATED | CVAR_UNSAFE ) )
			 && ( var_value[0] || ( flags & CVAR_CHEAT ) ) ) {
			var->flags &= ~( CVAR_USER_CREATED | CVAR_UNSAFE );
			Z_FreeInternal( var->resetString );
			var->resetString = CopyStringInternal( var_value );
			cvar_modifiedFlags |= flags;
		}

		var->flags |= flags;

		if ( !var->resetString[0] ) {
			Z_FreeInternal( var->resetString );
			var->resetString = CopyStringInternal( var_value );
		} else if ( var_value[0] && strcmp( var->resetString, var_value ) ) {
			Com_DPrintf( "Warning: cvar \"%s\" given initial values: \"%s\" and \"%s\"\n",
						 var_name, var->resetString, var_value );
		}

		if ( var->latchedString ) {
			char *s;

			s = var->latchedString;
			var->latchedString = NULL;
			Cvar_Set2( var_name, s, qtrue );
			Z_FreeInternal( s );
		}

		return var;
	}

	if ( cvar_numIndexes >= MAX_CVARS ) {
		Com_Error( ERR_FATAL, "MAX_CVARS" );
	}
	var = &cvar_indexes[cvar_numIndexes];
	cvar_numIndexes++;

	var->name = CopyStringInternal( var_name );

	var->string = CopyStringInternal( var_value );

	var->modified = qtrue;
	var->modificationCount = 1;
	var->value = atof( var->string );
	var->integer = atoi( var->string );

	var->resetString = CopyStringInternal( var_value );

	prev = &cvar_vars;
	while ( *prev ) {
		if ( stricmp( var->name, ( *prev )->name ) < 0 ) {
			break;
		}
		prev = &( *prev )->next;
	}
	var->next = *prev;
	*prev = var;

	var->flags = flags;

	hash = generateHashValue( var_name );
	var->hashNext = cvar_hashTable[hash];
	cvar_hashTable[hash] = var;

	return var;
}

/*
=============
Cvar_VMSet
=============
*/
void Cvar_VMSet( vmCvar_t *vmCvar, const char *value ) {
	Cvar_Set( cvar_indexes[vmCvar->handle].name, value );
	Cvar_Update( vmCvar );
}

/*
=====================
Cvar_Register

basically a slightly modified Cvar_Get for the interpreted modules
=====================
*/
void Cvar_Register( vmCvar_t *vmCvar, const char *varName,
	const char *defaultValue, int flags ) {
	cvar_t *cv;

	cv = Cvar_Get( varName, defaultValue, flags );
	if ( !vmCvar ) {
		return;
	}
	vmCvar->handle = cv - cvar_indexes;
	vmCvar->modificationCount = -1;
	Cvar_Update( vmCvar );
}

/*
============
Cvar_Init

Reads in all archived cvars
============
*/
void Cvar_Init( void ) {
	cvar_cheats = Cvar_Get( "sv_cheats", "0", CVAR_SYSTEMINFO | CVAR_ROM );
}

