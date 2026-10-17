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

#ifndef Q3MAP_FILES_LOCAL_H
#define Q3MAP_FILES_LOCAL_H

#include "../../common/q_shared.h"
#include <stddef.h>

#define MAX_FILE_HANDLES 64
#define MAX_SERVER_PAKS 4096
#define MAX_ZPATH 256
#define ERR_FATAL 0

typedef int fileHandle_t;
typedef struct unz_s *unzFile;

typedef union {
	FILE *o;
	unzFile z;
} qfile_gut;

typedef struct {
	qfile_gut file;
	qboolean unique;
} qfile_ut;

typedef struct {
	qfile_ut handleFiles;
	qboolean handleSync;
	int baseOffset;
	int fileSize;
	int zipFilePos;
	qboolean zipFile;
	qboolean streamed;
	char name[MAX_ZPATH];
} fileHandleData_t;

typedef char file_handle_size_check[sizeof( fileHandleData_t ) == 288 ? 1 : -1];
typedef char file_handle_zip_offset_check[offsetof( fileHandleData_t, zipFile ) == 24 ? 1 : -1];
typedef char file_handle_name_offset_check[offsetof( fileHandleData_t, name ) == 32 ? 1 : -1];

extern fileHandleData_t fsh[MAX_FILE_HANDLES];

void Com_Printf( const char *format, ... );
void FS_CheckFileSystemStarted( void );
fileHandle_t FS_FOpenFileWrite( const char *filename );
void FS_FCloseFile( fileHandle_t f );
int FS_Write( const void *buffer, int len, fileHandle_t f );
int unzReadCurrentFile( unzFile file, void *buf, unsigned int len );
char **FS_ListFilteredFiles( const char *path, const char *extension, const char *filter, int *numfiles );

int FS_Read( void *buffer, int len, fileHandle_t f );
void FS_WriteFile( const char *qpath, const void *buffer, int size );
char **FS_ListFiles( const char *path, const char *extension, int *numfiles );
void FS_Flush( fileHandle_t f );

#endif
