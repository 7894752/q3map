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

#include "../common/q_shared.h"
#include <io.h>
#include <direct.h>

#define MAX_OSPATH 256
typedef int fileHandle_t;

extern int FS_Read( void *buffer, int len, fileHandle_t f );
extern int FS_Seek( fileHandle_t f, int offset, int origin );
int Com_FilterPath( char *filter, char *name, int casesensitive );
char *CopyStringInternal( const char *in );
void *Z_MallocInternal( int size );
void Z_FreeInternal( void *block );
#include <string.h>
#define MAX_FOUND_FILES     0x1000

/*
==============
Sys_Mkdir
==============
*/
void Sys_Mkdir( const char *path ) {
	_mkdir( path );
}

/*
==============
Sys_Cwd
==============
*/
char *Sys_Cwd( void ) {
	static char cwd[MAX_OSPATH];

	_getcwd( cwd, sizeof( cwd ) - 1 );
	cwd[MAX_OSPATH - 1] = 0;

	return cwd;
}

/*
==============
Sys_DefaultCDPath
==============
*/
char *Sys_DefaultCDPath( void ) {
	return "";
}

/*
==============
Sys_DefaultBasePath
==============
*/
char *Sys_DefaultBasePath( void ) {
	return Sys_Cwd();
}

/*
=============
Sys_DefaultHomePath
=============
*/
char *Sys_DefaultHomePath( void ) {
	return NULL;
}

/*
=============
Sys_DefaultInstallPath
=============
*/
char *Sys_DefaultInstallPath( void ) {
	return Sys_Cwd();
}

/*
=============
Sys_ListFilteredFiles
=============
*/
void Sys_ListFilteredFiles( const char *basedir, char *subdirs,
                                     char *filter, char **list, int *numfiles ) {
	char search[MAX_OSPATH], filename[MAX_OSPATH];
	struct _finddata_t findinfo;
	long findhandle;

	if ( *numfiles >= MAX_FOUND_FILES - 1 ) {
		return;
	}

	if ( strlen( subdirs ) ) {
		Com_sprintf( search, sizeof( search ), "%s\\%s\\*", basedir, subdirs );
	} else {
		Com_sprintf( search, sizeof( search ), "%s\\*", basedir );
	}

	findhandle = _findfirst( search, &findinfo );
	if ( findhandle == -1 ) {
		return;
	}

	do {
		if ( ( findinfo.attrib & _A_SUBDIR )
		     && ( !Q_stricmp( findinfo.name, "." )
		       || !Q_stricmp( findinfo.name, ".." )
		       || !Q_stricmp( findinfo.name, "CVS" ) ) ) {
			continue;
		}

		if ( *numfiles >= MAX_FOUND_FILES - 1 ) {
			break;
		}

		if ( subdirs ) {
			Com_sprintf( filename, sizeof( filename ), "%s\\%s", subdirs, findinfo.name );
		} else {
			Com_sprintf( filename, sizeof( filename ), "%s", findinfo.name );
		}

		if ( !Com_FilterPath( filter, filename, qfalse ) ) {
			continue;
		}
		list[*numfiles] = CopyStringInternal( filename );
		( *numfiles )++;
	} while ( _findnext( findhandle, &findinfo ) != -1 );

	_findclose( findhandle );
}

/* Enumerate files or subdirectories, with an optional filename filter. */
/*
=============
Sys_ListFiles
=============
*/
char **Sys_ListFiles( const char *directory, const char *extension,
                      char *filter, int *numfiles, int wantsubs ) {
	char search[MAX_OSPATH];
	int nfiles;
	char **listCopy;
	char *list[MAX_FOUND_FILES];
	struct _finddata_t findinfo;
	long findhandle;
	int flag;
	int i;

	if ( filter ) {
		nfiles = 0;
		Sys_ListFilteredFiles( directory, "", filter, list, &nfiles );

		list[nfiles] = NULL;
		*numfiles = nfiles;

		if ( !nfiles ) {
			return NULL;
		}

		listCopy = Z_MallocInternal( ( nfiles + 1 ) * sizeof( *listCopy ) );
		for ( i = 0 ; i < nfiles ; i++ ) {
			listCopy[i] = list[i];
		}
		listCopy[i] = NULL;

		return listCopy;
	}

	if ( !extension ) {
		extension = "";
	}

	if ( extension[0] == '/' && extension[1] == 0 ) {
		extension = "";
		flag = 0;
	} else {
		flag = _A_SUBDIR;
	}

	Com_sprintf( search, sizeof( search ), "%s\\*%s", directory, extension );

	nfiles = 0;

	findhandle = _findfirst( search, &findinfo );
	if ( findhandle == -1 ) {
		*numfiles = 0;
		return NULL;
	}

	do {
		if ( ( !wantsubs && flag ^ ( findinfo.attrib & _A_SUBDIR ) ) || ( wantsubs && findinfo.attrib & _A_SUBDIR ) ) {
			if ( ( findinfo.attrib & _A_SUBDIR )
				&& ( !Q_stricmp( findinfo.name, "." )
					|| !Q_stricmp( findinfo.name, ".." )
					|| !Q_stricmp( findinfo.name, "CVS" ) ) ) {
				continue;
			}
			list[nfiles] = CopyStringInternal( findinfo.name );
			nfiles++;
			if ( nfiles == MAX_FOUND_FILES - 1 ) {
				break;
			}
		}
	} while ( _findnext( findhandle, &findinfo ) != -1 );

	list[nfiles] = NULL;
	_findclose( findhandle );
	*numfiles = nfiles;

	if ( !nfiles ) {
		return NULL;
	}

	listCopy = Z_MallocInternal( ( nfiles + 1 ) * sizeof( *listCopy ) );
	for ( i = 0 ; i < nfiles ; i++ ) {
		listCopy[i] = list[i];
	}
	listCopy[i] = NULL;

	return listCopy;
}

/* Release every owned filename and the pointer list. */
/*
=============
Sys_FreeFileList
=============
*/
void Sys_FreeFileList( char **list ) {
	int i;

	if ( !list ) {
		return;
	}

	for ( i = 0 ; list[i] ; i++ ) {
		Z_FreeInternal( list[i] );
	}

	Z_FreeInternal( list );
}

/* Ignore the special directory entries when testing for usable contents. */
/*
=============
Sys_DirectoryHasContents
=============
*/
int Sys_DirectoryHasContents( const char *path ) {
	struct _finddata_t findinfo;
	long findhandle;
	char search[MAX_OSPATH];

	Com_sprintf( search, sizeof( search ), "%s\\*", path );
	findhandle = _findfirst( search, &findinfo );
	if ( findhandle == -1 ) {
		return 0;
	}

	do {
		if ( !( findinfo.attrib & _A_SUBDIR )
			|| ( Q_stricmp( findinfo.name, "." )
				&& Q_stricmp( findinfo.name, ".." )
				&& Q_stricmp( findinfo.name, "CVS" ) ) ) {
			return 1;
		}
	} while ( _findnext( findhandle, &findinfo ) != -1 );
	return 0;
}

/*
===============
Sys_InitStreamThread

================
*/
void Sys_InitStreamThread( void ) {
}

/*
===============
Sys_ShutdownStreamThread

================
*/
void Sys_ShutdownStreamThread( void ) {
}

/*
===============
Sys_BeginStreamedFile

================
*/
void Sys_BeginStreamedFile( int f, int readAhead ) {
}

/*
===============
Sys_EndStreamedFile

================
*/
void Sys_EndStreamedFile( int f ) {
}

/*
===============
Sys_StreamedRead

================
*/
int Sys_StreamedRead( void *buffer, int size, int count, fileHandle_t f ) {
	return FS_Read( buffer, size * count, f );
}

/*
===============
Sys_StreamSeek

================
*/
void Sys_StreamSeek( fileHandle_t f, int offset, int origin ) {
	FS_Seek( f, offset, origin );
}

/*
=============
Sys_InitializeCriticalSection
=============
*/
void *Sys_InitializeCriticalSection( void ) {
	return (void *)-1;
}

/*
=============
Sys_EnterCriticalSection
=============
*/
void Sys_EnterCriticalSection( void *ptr ) {
}

/*
=============
Sys_LeaveCriticalSection
=============
*/
void Sys_LeaveCriticalSection( void *ptr ) {
}
