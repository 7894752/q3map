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

#include "../../common/q_shared.h"
#include <ctype.h>

#define MAX_QPATH 64
#define MAX_TOKEN_CHARS 1024

int com_fullyInitialized;

/*
============
Com_StringContains
============
*/
char *Com_StringContains( char *str1, char *str2, int casesensitive ) {
	int len, i, j;

	len = strlen( str1 ) - strlen( str2 );
	for ( i = 0; i <= len; i++, str1++ ) {
		for ( j = 0; str2[j]; j++ ) {
			if ( casesensitive ) {
				if ( str1[j] != str2[j] ) {
					break;
				}
			} else {
				if ( toupper( str1[j] ) != toupper( str2[j] ) ) {
					break;
				}
			}
		}
		if ( !str2[j] ) {
			return str1;
		}
	}
	return NULL;
}

/*
============
Com_Filter
============
*/
int Com_Filter( char *filter, char *name, int casesensitive ) {
	char buf[MAX_TOKEN_CHARS];
	char *ptr;
	int i, found;

	while ( *filter ) {
		if ( *filter == '*' ) {
			filter++;
			for ( i = 0; *filter; i++ ) {
				if ( *filter == '*' || *filter == '?' ) break;
				buf[i] = *filter;
				filter++;
			}
			buf[i] = '\0';
			if ( strlen( buf ) ) {
				ptr = Com_StringContains( name, buf, casesensitive );
				if ( !ptr ) return qfalse;
				name = ptr + strlen( buf );
			}
		} else if ( *filter == '?' ) {
			filter++;
			name++;
		} else if ( *filter == '[' && *(filter + 1) == '[' ) {
			filter++;
		} else if ( *filter == '[' ) {
			filter++;
			found = qfalse;
			while ( *filter && !found ) {
				if ( *filter == ']' && *(filter + 1) != ']' ) break;
				if ( *(filter + 1) == '-' && *(filter + 2) &&
					( *(filter + 2) != ']' || *(filter + 3) == ']' ) ) {
					if ( casesensitive ) {
						if ( *name >= *filter && *name <= *(filter + 2) ) found = qtrue;
					} else {
						if ( toupper( *name ) >= toupper( *filter ) &&
							toupper( *name ) <= toupper( *(filter + 2) ) ) found = qtrue;
					}
					filter += 3;
				} else {
					if ( casesensitive ) {
						if ( *filter == *name ) found = qtrue;
					} else {
						if ( toupper( *filter ) == toupper( *name ) ) found = qtrue;
					}
					filter++;
				}
			}
			if ( !found ) return qfalse;
			while ( *filter ) {
				if ( *filter == ']' && *(filter + 1) != ']' ) break;
				filter++;
			}
			filter++;
			name++;
		} else {
			if ( casesensitive ) {
				if ( *filter != *name ) return qfalse;
			} else {
				if ( toupper( *filter ) != toupper( *name ) ) return qfalse;
			}
			filter++;
			name++;
		}
	}
	return qtrue;
}

/*
============
Com_FilterPath
============
*/
int Com_FilterPath( char *filter, char *name, int casesensitive ) {
	int i;
	char new_filter[MAX_QPATH];
	char new_name[MAX_QPATH];

	for ( i = 0; i < MAX_QPATH - 1 && filter[i]; i++ ) {
		if ( filter[i] == '\\' || filter[i] == ':' ) {
			new_filter[i] = '/';
		} else {
			new_filter[i] = filter[i];
		}
	}
	new_filter[i] = '\0';
	for ( i = 0; i < MAX_QPATH - 1 && name[i]; i++ ) {
		if ( name[i] == '\\' || name[i] == ':' ) {
			new_name[i] = '/';
		} else {
			new_name[i] = name[i];
		}
	}
	new_name[i] = '\0';
	return Com_Filter( new_filter, new_name, casesensitive );
}

/*
============
Com_HashKey
============
*/
int Com_HashKey( char *string, int maxlen ) {
	int hash, i;

	hash = 0;
	for ( i = 0; i < maxlen && string[i] != '\0'; i++ ) {
		hash += string[i] * (119 + i);
	}
	hash = hash ^ (hash >> 10) ^ (hash >> 20);
	return hash;
}
