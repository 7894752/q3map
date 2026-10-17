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

#include "files_local.h"
#include "unzip_layout.h"
#include "../../universal/cvar.h"
#include <direct.h>
#include <ctype.h>
#include <windows.h>

#define MAX_OSPATH 256
#define MAX_SEARCH_PATHS 1024
#define MAX_FILEHASH_SIZE 1024
#define MAX_FOUND_FILES 4096

cvar_t *fs_homepath;
cvar_t *fs_debug;
char fs_gamedir[MAX_OSPATH];
cvar_t *fs_ignoreLozalized;
int fs_numServerPaks;
void *Z_MallocInternal( int size );
void Z_FreeInternal( void *ptr );
void Com_Memcpy( void *dest, const void *src, size_t count );
int Q_strncmp( const char *s1, const char *s2, int n );
char *va( const char *format, ... );
void Com_Memset( void *dest, int val, size_t count );
int unzCloseCurrentFile( unzFile file );
int unzClose( unzFile file );

typedef struct fileData_s {
	char *name;
	void *data;
	void ( *clearCallback )( struct fileData_s * );
} fileData_t;

typedef struct fileInPack_s {
	unsigned long zipPosition;
	fileData_t data;
	char *name;
	struct fileInPack_s *next;
} fileInPack_t;

typedef struct {
	char pakFilename[MAX_OSPATH];
	char pakBasename[MAX_OSPATH];
	char pakGamename[MAX_OSPATH];
	unzFile handle;
	int numfiles;
	byte referenced[4];
	int hashSize;
	fileInPack_t **hashTable;
	fileInPack_t *buildBuffer;
} pack_t;

typedef struct fs_dirFile_s {
	fileData_t data;
	struct fs_dirFile_s *next;
} fs_dirFile_t;

typedef struct fs_dirFileList_s {
	char dir[MAX_OSPATH];
	int numFiles;
	int hashSize;
	fs_dirFile_t **hashTable;
	fs_dirFile_t *buildBuffer;
	struct fs_dirFileList_s *next;
} fs_dirFileList_t;

long FS_HashFileName( const char *fname, int hashSize );
qboolean FS_FilenameCompare( const char *s1, const char *s2 );

typedef struct {
	char path[MAX_OSPATH];
	char gamedir[MAX_OSPATH];
} directory_t;

typedef struct searchpath_s {
	struct searchpath_s *next;
	pack_t *pack;
	directory_t *dir;
	qboolean localized;
	int language;
} searchpath_t;

searchpath_t *fs_searchpaths = 0;
fs_dirFileList_t *fs_dirFileLists = 0;
qboolean FS_UseSearchPath( searchpath_t *search );
qboolean FS_PakIsPure( pack_t *pack );

fileHandle_t FS_HandleForFileStreamed( int streamed );

void FS_BuildOSPath( const char *base, const char *game, const char *qpath, char *fs_path );
qboolean FS_CreatePath( char *OSPath );
void Q_strncpyz( char *dest, const char *src, int destsize );
void Sys_Mkdir( const char *path );

fileHandleData_t fsh[MAX_FILE_HANDLES];
int fs_loadStack;

int FS_FOpenFileRead( const char *filename, fileHandle_t *file, qboolean uniqueFILE );
void *Hunk_AllocateTempMemoryInternal( int size );
void Hunk_FreeTempMemory( void *buffer );
FILE *FS_FileForHandle( fileHandle_t f );

extern int com_fullyInitialized;
cvar_t *fs_copyfiles;
cvar_t *fs_cdpath;
cvar_t *fs_basepath;
cvar_t *fs_restrict;
void FS_CopyFile( char *fromOSPath, char *toOSPath );
int fs_loadingModeNum;
int fs_fakeChkSum;
unzFile unzReOpen( const char *path, unzFile file );
int unzSetCurrentFileInfoPosition( unzFile file, unsigned long position );
int unzOpenCurrentFile( unzFile file );
qboolean FS_UseSearchPath( searchpath_t *search );
int FS_filelength( fileHandle_t f );

char *CopyStringInternal( const char *in );
qboolean Com_FilterPath( const char *filter, const char *name, int casesensitive );
int Q_stricmpn( const char *s1, const char *s2, int n );
extern cvar_t *fs_restrict;
char **Sys_ListFiles( const char *directory, const char *extension,
	const char *filter, int *numfiles, int wantsubs );
void Sys_FreeFileList( char **list );

unzFile unzOpen( const char *filename );
int unzGetGlobalInfo( unzFile file, unz_global_info *info );
int unzGoToFirstFile( unzFile file );
int unzGoToNextFile( unzFile file );
int unzGetCurrentFileInfo( unzFile file, unz_file_info *info, char *filename,
	unsigned long filenameSize, void *extra, unsigned long extraSize,
	char *comment, unsigned long commentSize );
int unzGetCurrentFileInfoPosition( unzFile file, unsigned long *position );
int LittleLong( int value );
char *Q_strlwr( char *s );
int fs_packFiles;

char **Sys_ListFiles( const char *directory, const char *extension,
	const char *filter, int *numfiles, int wantsubs );
void Sys_FreeFileList( char **list );

int fs_checksumFeed;
char lastValidBase[MAX_OSPATH];
char lastValidGame[MAX_OSPATH];
extern cvar_t *fs_basepath;
cvar_t *fs_gamedirvar;
void FS_Startup( const char *gameName );
void FS_Shutdown( qboolean closemfp );

typedef enum {
	FS_READ,
	FS_WRITE,
	FS_APPEND,
	FS_APPEND_SYNC
} fsMode_t;

#define FS_UNSET_FILE_LENGTH 6969
long unztell( unzFile file );

#define CVAR_SYSTEMINFO 8
extern cvar_t *fs_copyfiles;
extern cvar_t *fs_cdpath;
cvar_t *fs_basegame;
const char *Sys_DefaultCDPath( void );
const char *Sys_DefaultInstallPath( void );
const char *Sys_DefaultHomePath( void );
qboolean Sys_DirectoryHasContents( const char *path );
void FS_DisplayPath( qboolean showAll );

/* CoD retains the Q3 pure-server arrays as globals even in the tool build. */
int fs_serverPaks[MAX_SERVER_PAKS];
char *fs_serverPakNames[MAX_SERVER_PAKS];
int fs_numServerReferencedPaks;
int fs_serverReferencedPaks[MAX_SERVER_PAKS];
char *fs_serverReferencedPakNames[MAX_SERVER_PAKS];

#define FS_SEEK_CUR 0
#define FS_SEEK_END 1
#define FS_SEEK_SET 2

/*
==============
FS_Initialized
==============
*/
qboolean FS_Initialized( void ) {
	return fs_searchpaths != NULL;
}

/*
=============
FS_CheckFileSystemStarted
=============
*/
void FS_CheckFileSystemStarted( void ) {
	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "\x15" "Filesystem call made without initialization\n" );
	}
}

/*
=================
FS_PakIsPure
=================
*/
qboolean FS_PakIsPure( pack_t *pack ) {
	return qtrue;
}

/*
=================
FS_LoadStack
return load stack
=================
*/
int FS_LoadStack( void ) {
	return fs_loadStack;
}

/*
=============
FS_UseSearchPath
=============
*/
qboolean FS_UseSearchPath( searchpath_t *search ) {
	if ( search->localized && fs_ignoreLozalized->integer ) {
		return qfalse;
	}
	return qtrue;
}

/*
=============
FS_LanguageHasAssets
=============
*/
qboolean FS_LanguageHasAssets( int language ) {
	searchpath_t *sp;

	for ( sp = fs_searchpaths; sp; sp = sp->next ) {
		if ( sp->localized && sp->language == language ) {
			return qtrue;
		}
	}
	return qfalse;
}

/*
=============
FS_HashFileName
=============
*/
long FS_HashFileName( const char *fname, int hashSize ) {
	int i;
	long hash;
	int letter;

	hash = 0;
	i = 0;
	while ( fname[i] != '\0' ) {
		letter = tolower( fname[i] );
		if ( letter == '.' ) {
			break;
		}
		if ( letter == '\\' ) {
			letter = '/';
		}
		hash += letter * ( i + 119 );
		i++;
	}
	hash = hash ^ ( ( hash ^ ( hash >> 10 ) ) >> 10 );
	hash &= hashSize - 1;
	return hash;
}

/*
=============
FS_HandleForFileStreamed
=============
*/
fileHandle_t FS_HandleForFileStreamed( int streamed ) {
	int i;
	int start, count;

	if ( streamed ) {
		start = 51;
		count = 13;
	} else {
		start = 1;
		count = 50;
	}
	for ( i = 0; i < count; i++ ) {
		if ( fsh[start + i].handleFiles.file.o == NULL ) {
			return start + i;
		}
	}
	for ( i = 1; i < MAX_FILE_HANDLES; i++ ) {
		Com_Printf( "FILE %2i: '%s'\n", i, fsh[i].name );
	}
	Com_Error( ERR_DROP, "\x15" "FS_HandleForFile: none free" );
	return -1;
}

/*
=============
FS_FileForHandle
=============
*/
FILE *FS_FileForHandle( fileHandle_t f ) {
	return fsh[f].handleFiles.file.o;
}

/*
=============
FS_ForceFlush
=============
*/
void FS_ForceFlush( fileHandle_t f ) {
	FILE *file;

	file = FS_FileForHandle( f );
	setvbuf( file, NULL, _IONBF, 0 );
}

/*
================
FS_filelength

If this is called on a non-unique FILE (from a pak file),
it will return the size of the pak file, not the expected
size of the file.
================
*/
int FS_filelength( fileHandle_t f ) {
	FILE *h;
	int pos, end;

	FS_CheckFileSystemStarted();
	if ( fsh[f].zipFile == qtrue ) {
		return fsh[f].handleFiles.file.z->cur_file_info.uncompressed_size;
	}
	h = FS_FileForHandle( f );
	pos = ftell( h );
	fseek( h, 0, SEEK_END );
	end = ftell( h );
	fseek( h, pos, SEEK_SET );
	return end;
}

/*
====================
FS_ReplaceSeparators

Fix things up differently for win/unix/mac
====================
*/
static void FS_ReplaceSeparators( char *path ) {
	char *s;

	for ( s = path; *s; s++ ) {
		if ( *s == '/' || *s == '\\' ) {
			*s = '\\';
		}
	}
}

/*
=============
FS_BuildOSPath3
=============
*/
static void FS_BuildOSPath3( const char *base, const char *game, const char *qpath,
	char *fs_path, int allowOverflow ) {
	int baseLen, gameLen, qLen;

	if ( !game || !*game ) {
		game = fs_gamedir;
	}
	baseLen = strlen( base );
	gameLen = strlen( game );
	qLen = strlen( qpath );
	if ( baseLen + gameLen + qLen + 2 >= MAX_OSPATH ) {
		if ( allowOverflow ) {
			*fs_path = 0;
			return;
		}
		Com_Error( ERR_FATAL, "\x15" "FS_BuildOSPath: os path length exceeded\n" );
	}
	memcpy( fs_path, base, baseLen );
	fs_path[baseLen] = '/';
	memcpy( fs_path + baseLen + 1, game, gameLen );
	fs_path[baseLen + gameLen + 1] = '/';
	memcpy( fs_path + baseLen + gameLen + 2, qpath, qLen + 1 );
	FS_ReplaceSeparators( fs_path + baseLen );
}

/*
===================
FS_BuildOSPath

Qpath may have either forward or backwards slashes
===================
*/
void FS_BuildOSPath( const char *base, const char *game, const char *qpath, char *fs_path ) {
	FS_BuildOSPath3( base, game, qpath, fs_path, 0 );
}

/*
============
FS_CreatePath

Creates any directories needed to store the given filename
============
*/
qboolean FS_CreatePath( char *OSPath ) {
	char *ofs;

	if ( strstr( OSPath, ".." ) || strstr( OSPath, "::" ) ) {
		Com_Printf( "WARNING: refusing to create relative path \"%s\"\n", OSPath );
		return qtrue;
	}
	for ( ofs = OSPath + 1; *ofs; ofs++ ) {
		if ( *ofs == '\\' ) {
			*ofs = 0;
			Sys_Mkdir( OSPath );
			*ofs = '\\';
		}
	}
	return qfalse;
}

/*
=================
FS_CopyFile

Copy a fully specified file from one place to another
=================
*/
void FS_CopyFile( char *fromOSPath, char *toOSPath ) {
	FILE *f;
	int len;
	byte *buf;

	if ( strstr( fromOSPath, "journal.dat" ) || strstr( fromOSPath, "journaldata.dat" ) ) {
		return;
	}
	f = fopen( fromOSPath, "rb" );
	if ( !f ) {
		return;
	}
	fseek( f, 0, SEEK_END );
	len = ftell( f );
	fseek( f, 0, SEEK_SET );
	buf = malloc( len );
	if ( fread( buf, 1, len, f ) != len ) {
		Com_Error( ERR_FATAL, "\x15" "Short read in FS_Copyfiles()\n" );
	}
	fclose( f );
	if ( FS_CreatePath( toOSPath ) ) {
		free( buf );
		return;
	}
	f = fopen( toOSPath, "wb" );
	if ( !f ) {
		free( buf );
		return;
	}
	if ( fwrite( buf, 1, len, f ) != len ) {
		Com_Error( ERR_FATAL, "\x15" "Short write in FS_Copyfiles()\n" );
	}
	fclose( f );
	free( buf );
}

/*
=============
FS_CopyFiles
=============
*/
void FS_CopyFiles( const char *fromDir, const char *toDir, const char *ext ) {
	char fromOSPath[MAX_OSPATH];
	char toOSPath[MAX_OSPATH];
	FILE *f;
	int len;
	byte *buf;

	FS_BuildOSPath( fs_homepath->string, fs_gamedir, fromDir, fromOSPath );
	FS_BuildOSPath( fs_homepath->string, fs_gamedir, toDir, toOSPath );
	if ( strstr( fromOSPath, "journal.dat" ) || strstr( fromOSPath, "journaldata.dat" ) ) {
		Com_Printf( "Ignoring journal files\n" );
		return;
	}
	f = fopen( fromOSPath, "rb" );
	if ( !f ) {
		return;
	}
	fseek( f, 0, SEEK_END );
	len = ftell( f );
	fseek( f, 0, SEEK_SET );
	buf = malloc( len );
	if ( fread( buf, 1, len, f ) != len ) {
		Com_Error( ERR_FATAL, "\x15" "Short read in FS_Copyfiles()\n" );
	}
	fclose( f );
	if ( FS_CreatePath( toOSPath ) ) {
		free( buf );
		return;
	}
	f = fopen( toOSPath, "wb" );
	if ( !f ) {
		free( buf );
		return;
	}
	if ( fwrite( buf, 1, len, f ) != len ) {
		Com_Error( ERR_FATAL, "\x15" "Short write in FS_Copyfiles()\n" );
	}
	fclose( f );
	free( buf );
}

/*
===========
FS_Remove

===========
*/
void FS_Remove( const char *osPath ) {
	remove( osPath );
}

/*
================
FS_FileExists

Tests if the file exists in the current gamedir, this DOES NOT
search the paths.  This is to determine if opening a file to write
(which always goes into the current gamedir) will cause any overwrites.
NOTE TTimo: this goes with FS_FOpenFileWrite for opening the file afterwards
================
*/
qboolean FS_FileExists( const char *file ) {
	FILE *f;
	char testpath[MAX_OSPATH];

	FS_BuildOSPath( fs_homepath->string, fs_gamedir, file, testpath );
	f = fopen( testpath, "rb" );
	if ( f ) {
		fclose( f );
		return qtrue;
	}
	return qfalse;
}

/*
===========
FS_Rename

===========
*/
void FS_Rename( const char *from, const char *to ) {
	char fromOSPath[MAX_OSPATH];
	char toOSPath[MAX_OSPATH];

	FS_CheckFileSystemStarted();
	FS_BuildOSPath( fs_homepath->string, fs_gamedir, from, fromOSPath );
	FS_BuildOSPath( fs_homepath->string, fs_gamedir, to, toOSPath );
	if ( fs_debug->integer ) {
		Com_Printf( "FS_Rename: %s --> %s\n", fromOSPath, toOSPath );
	}
	if ( rename( fromOSPath, toOSPath ) ) {
		FS_Remove( toOSPath );
		if ( rename( fromOSPath, toOSPath ) ) {
			FS_CopyFile( fromOSPath, toOSPath );
			FS_Remove( fromOSPath );
		}
	}
}

/*
==============
FS_FCloseFile

If the FILE pointer is an open pak file, leave it open.

For some reason, other dll's can't just cal fclose()
on files returned by FS_FOpenFile...
==============
*/
void FS_FCloseFile( fileHandle_t f ) {
	FS_CheckFileSystemStarted();
	if ( fsh[f].zipFile == qtrue ) {
		unzCloseCurrentFile( fsh[f].handleFiles.file.z );
		if ( fsh[f].handleFiles.unique ) {
			unzClose( fsh[f].handleFiles.file.z );
		}
		Com_Memset( &fsh[f], 0, sizeof( fsh[f] ) );
		return;
	}
	if ( fsh[f].handleFiles.file.o ) {
		fclose( fsh[f].handleFiles.file.o );
	}
	Com_Memset( &fsh[f], 0, sizeof( fsh[f] ) );
}

/*
===========
FS_FOpenFileWrite

===========
*/
fileHandle_t FS_FOpenFileWrite( const char *filename ) {
	fileHandle_t f;
	char ospath[MAX_OSPATH];

	FS_CheckFileSystemStarted();
	f = FS_HandleForFileStreamed( 0 );
	fsh[f].zipFile = qfalse;
	FS_BuildOSPath( fs_homepath->string, fs_gamedir, filename, ospath );
	if ( fs_debug->integer ) {
		Com_Printf( "FS_FOpenFileWrite: %s\n", ospath );
	}
	if ( FS_CreatePath( ospath ) ) {
		return 0;
	}
	fsh[f].handleFiles.file.o = fopen( ospath, "wb" );
	Q_strncpyz( fsh[f].name, filename, sizeof( fsh[f].name ) );
	fsh[f].handleSync = qfalse;
	if ( !fsh[f].handleFiles.file.o ) {
		f = 0;
	}
	return f;
}

/*
=============
FS_FOpenTextFileWrite
=============
*/
fileHandle_t FS_FOpenTextFileWrite( const char *filename ) {
	fileHandle_t f;
	char ospath[MAX_OSPATH];

	FS_CheckFileSystemStarted();
	f = FS_HandleForFileStreamed( 0 );
	fsh[f].zipFile = qfalse;
	FS_BuildOSPath( fs_homepath->string, fs_gamedir, filename, ospath );
	if ( fs_debug->integer ) {
		Com_Printf( "FS_FOpenFileWrite: %s\n", ospath );
	}
	if ( FS_CreatePath( ospath ) ) {
		return 0;
	}
	fsh[f].handleFiles.file.o = fopen( ospath, "wt" );
	Q_strncpyz( fsh[f].name, filename, sizeof( fsh[f].name ) );
	fsh[f].handleSync = qfalse;
	if ( !fsh[f].handleFiles.file.o ) {
		f = 0;
	}
	return f;
}

/*
===========
FS_FOpenFileAppend

===========
*/
fileHandle_t FS_FOpenFileAppend( const char *filename ) {
	fileHandle_t f;
	char ospath[MAX_OSPATH];

	FS_CheckFileSystemStarted();
	f = FS_HandleForFileStreamed( 0 );
	fsh[f].zipFile = qfalse;
	Q_strncpyz( fsh[f].name, filename, sizeof( fsh[f].name ) );
	FS_BuildOSPath( fs_homepath->string, fs_gamedir, filename, ospath );
	if ( fs_debug->integer ) {
		Com_Printf( "FS_FOpenFileAppend: %s\n", ospath );
	}
	if ( FS_CreatePath( ospath ) ) {
		return 0;
	}
	fsh[f].handleFiles.file.o = fopen( ospath, "ab" );
	fsh[f].handleSync = qfalse;
	if ( !fsh[f].handleFiles.file.o ) {
		f = 0;
	}
	return f;
}

/*
===========
FS_FilenameCompare

Ignore case and seprator char distinctions
===========
*/
qboolean FS_FilenameCompare( const char *s1, const char *s2 ) {
	int c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;
		if ( Q_islower( c1 ) ) {
			c1 -= 'a' - 'A';
		}
		if ( Q_islower( c2 ) ) {
			c2 -= 'a' - 'A';
		}
		if ( c1 == '\\' || c1 == ':' ) {
			c1 = '/';
		}
		if ( c2 == '\\' || c2 == ':' ) {
			c2 = '/';
		}
		if ( c1 != c2 ) {
			return -1;
		}
	} while ( c1 );
	return 0;
}

/*
===========
FS_FileCompare

Do a binary check of the two files, return qfalse if they are different, otherwise qtrue
===========
*/
qboolean FS_FileCompare( const char *s1, const char *s2 ) {
	FILE *f1, *f2;
	int len1, len2, pos, i;
	byte *buf1, *buf2, *p1, *p2;

	f1 = fopen( s1, "rb" );
	if ( !f1 ) {
		Com_Error( ERR_FATAL, "\x15" "FS_FileCompare: %s does not exist\n", s1 );
	}
	f2 = fopen( s2, "rb" );
	if ( !f2 ) {
		fclose( f1 );
		return qfalse;
	}
	pos = ftell( f1 );
	fseek( f1, 0, SEEK_END );
	len1 = ftell( f1 );
	fseek( f1, pos, SEEK_SET );
	pos = ftell( f2 );
	fseek( f2, 0, SEEK_END );
	len2 = ftell( f2 );
	fseek( f2, pos, SEEK_SET );
	if ( len1 != len2 ) {
		fclose( f1 );
		fclose( f2 );
		return qfalse;
	}
	buf1 = Z_MallocInternal( len1 );
	if ( fread( buf1, 1, len1, f1 ) != len1 ) {
		Com_Error( ERR_FATAL, "\x15" "Short read in FS_FileCompare()\n" );
	}
	fclose( f1 );
	buf2 = Z_MallocInternal( len2 );
	if ( fread( buf2, 1, len2, f2 ) != len2 ) {
		Com_Error( ERR_FATAL, "\x15" "Short read in FS_FileCompare()\n" );
	}
	fclose( f2 );
	for ( i = 0, p1 = buf1, p2 = buf2; i < len1; i++, p1++, p2++ ) {
		if ( *p1 != *p2 ) {
			free( buf1 );
			free( buf2 );
			return qfalse;
		}
	}
	free( buf1 );
	free( buf2 );
	return qtrue;
}

/*
===========
FS_ShiftedStrStr
===========
*/
char *FS_ShiftedStrStr( const char *string, const char *substring, int shift ) {
	char buf[MAX_OSPATH];
	int i;

	for ( i = 0; substring[i]; i++ ) {
		buf[i] = substring[i] + shift;
	}
	buf[i] = '\0';
	return strstr( string, buf );
}

/*
=============
FS_FOpenFileRead_Internal
=============
*/
static int FS_FOpenFileRead_Internal( const char *qpath, fileHandle_t *file,
	qboolean uniqueFILE, int filter_flag ) {
	searchpath_t *search;
	pack_t *pak;
	fileInPack_t *pakFile;
	directory_t *dir;
	long hash;
	char netpath[MAX_OSPATH];
	char copyPath[MAX_OSPATH];
	int len;
	struct unz_s *zfi;
	FILE *temp;

	hash = 0;
	FS_CheckFileSystemStarted();
	if ( !file ) {
		for ( search = fs_searchpaths; search; search = search->next ) {
			if ( !FS_UseSearchPath( search ) ) {
				continue;
			}
			if ( search->pack ) {
				hash = FS_HashFileName( qpath, search->pack->hashSize );
			}
			if ( search->pack ) {
				pakFile = search->pack->hashTable[hash];
				if ( pakFile ) {
					do {
						if ( !FS_FilenameCompare( pakFile->name, qpath ) ) {
							return qtrue;
						}
						pakFile = pakFile->next;
					} while ( pakFile );
					continue;
				}
			}
			if ( search->dir ) {
				FS_BuildOSPath3( search->dir->path, search->dir->gamedir, qpath, netpath, filter_flag );
				temp = fopen( netpath, "rb" );
				if ( temp ) {
					fclose( temp );
					return qtrue;
				}
			}
		}
		return qfalse;
	}
	while ( *qpath == '/' || *qpath == '\\' ) {
		qpath++;
	}
	if ( strstr( qpath, ".." ) || strstr( qpath, "::" ) ||
		( com_fullyInitialized && strstr( qpath, "codkey" ) ) ) {
		*file = 0;
		return -1;
	}
	*file = FS_HandleForFileStreamed( filter_flag );
	fsh[*file].handleFiles.unique = uniqueFILE;
	for ( search = fs_searchpaths; search; search = search->next ) {
		if ( !FS_UseSearchPath( search ) ) {
			continue;
		}
		if ( search->pack ) {
			hash = FS_HashFileName( qpath, search->pack->hashSize );
		}
		if ( search->pack && search->pack->hashTable[hash] ) {
			if ( !FS_PakIsPure( search->pack ) ) {
				continue;
			}
			pak = search->pack;
			pakFile = pak->hashTable[hash];
			do {
				if ( !FS_FilenameCompare( pakFile->name, qpath ) ) {
					len = strlen( qpath );
					if ( !pak->referenced[0] &&
						Q_stricmp( qpath + len - 7, ".shader" ) &&
						Q_stricmp( qpath + len - 4, ".txt" ) &&
						Q_stricmp( qpath + len - 4, ".cfg" ) &&
						Q_stricmp( qpath + len - 7, ".config" ) &&
						!strstr( qpath, "levelshots" ) &&
						Q_stricmp( qpath + len - 4, ".bot" ) &&
						Q_stricmp( qpath + len - 6, ".arena" ) &&
						Q_stricmp( qpath + len - 5, ".menu" ) ) {
						pak->referenced[0] = 1;
					}
					if ( uniqueFILE ) {
						fsh[*file].handleFiles.file.z = unzReOpen( pak->pakFilename, pak->handle );
						if ( !fsh[*file].handleFiles.file.z ) {
							if ( filter_flag ) {
								FS_FCloseFile( *file );
								*file = 0;
								return -1;
							}
							Com_Error( ERR_FATAL, "\x15" "Couldn't reopen %s", pak->pakFilename );
						}
					} else {
						fsh[*file].handleFiles.file.z = pak->handle;
					}
					Q_strncpyz( fsh[*file].name, qpath, sizeof( fsh[*file].name ) );
					fsh[*file].zipFile = qtrue;
					zfi = fsh[*file].handleFiles.file.z;
					temp = zfi->file;
					unzSetCurrentFileInfoPosition( pak->handle, pakFile->zipPosition );
					Com_Memcpy( zfi, pak->handle, sizeof( *zfi ) );
					zfi->file = temp;
					unzOpenCurrentFile( fsh[*file].handleFiles.file.z );
					fsh[*file].zipFilePos = pakFile->zipPosition;
					if ( fs_debug->integer && !filter_flag ) {
						Com_Printf( "FS_FOpenFileRead: %s (found in '%s')\n", qpath, pak->pakFilename );
					}
					return zfi->cur_file_info.uncompressed_size;
				}
				pakFile = pakFile->next;
			} while ( pakFile );
		} else if ( search->dir ) {
			len = strlen( qpath );
			if ( ( !fs_restrict->integer && !fs_numServerPaks ) ||
				!Q_stricmp( qpath + len - 4, ".cfg" ) ||
				!Q_stricmp( qpath + len - 5, ".menu" ) ||
				!Q_stricmp( qpath + len - 4, ".svg" ) ||
				!Q_stricmp( qpath + len - 5, ".game" ) ||
				!Q_stricmp( qpath + len - 4, ".dat" ) ) {
				dir = search->dir;
				FS_BuildOSPath3( dir->path, dir->gamedir, qpath, netpath, filter_flag );
				fsh[*file].handleFiles.file.o = fopen( netpath, "rb" );
				if ( fsh[*file].handleFiles.file.o ) {
					if ( Q_stricmp( qpath + len - 4, ".cfg" ) &&
						Q_stricmp( qpath + len - 5, ".menu" ) &&
						Q_stricmp( qpath + len - 5, ".game" ) &&
						Q_stricmp( qpath + len - 4, ".dat" ) ) {
						fs_fakeChkSum = rand();
					}
					Q_strncpyz( fsh[*file].name, qpath, sizeof( fsh[*file].name ) );
					fsh[*file].zipFile = qfalse;
					if ( fs_debug->integer && !filter_flag ) {
						Com_Printf( "FS_FOpenFileRead: %s (found in '%s/%s')\n", qpath, dir->path, dir->gamedir );
					}
					if ( fs_copyfiles->integer && !Q_stricmp( dir->path, fs_cdpath->string ) ) {
						FS_BuildOSPath3( fs_basepath->string, dir->gamedir, qpath, copyPath, filter_flag );
						FS_CopyFile( netpath, copyPath );
					}
					return FS_filelength( *file );
				}
			}
		}
	}
	if ( fs_debug->integer && !filter_flag ) {
		Com_Printf( "Can't find %s\n", qpath );
	}
	*file = 0;
	return -1;
}

/*
=============
FS_FOpenFileReadStream
=============
*/
int FS_FOpenFileReadStream( const char *qpath, fileHandle_t *file, qboolean uniqueFILE ) {
	return FS_FOpenFileRead_Internal( qpath, file, uniqueFILE, 1 );
}

/*
===========
FS_FOpenFileRead

Finds the file in the search path.
Returns filesize and an open FILE pointer.
Used for streaming data out of either a
separate file or a ZIP file.
===========
*/
int FS_FOpenFileRead( const char *qpath, fileHandle_t *file, qboolean uniqueFILE ) {
	fs_loadingModeNum = 1;
	return FS_FOpenFileRead_Internal( qpath, file, uniqueFILE, 0 );
}

/*
=============
FS_TouchFile
=============
*/
int FS_TouchFile( const char *qpath ) {
	fileHandle_t f;

	FS_FOpenFileRead( qpath, &f, qfalse );
	if ( !f ) {
		return 0;
	}
	FS_FCloseFile( f );
	return 1;
}

/*
=============
FS_ShortOSFilePath
=============
*/
char *FS_ShortOSFilePath( const char *fullpath ) {
	searchpath_t *search;
	directory_t *dir;
	FILE *file;
	char netpath[MAX_OSPATH];

	for ( search = fs_searchpaths; search; search = search->next ) {
		if ( !FS_UseSearchPath( search ) ) {
			continue;
		}
		dir = search->dir;
		if ( dir ) {
			FS_BuildOSPath( dir->path, dir->gamedir, fullpath, netpath );
			file = fopen( netpath, "rb" );
			if ( file ) {
				fclose( file );
				return va( "%s/%s", dir->gamedir, fullpath );
			}
		}
	}
	return NULL;
}

/*
==============
FS_Delete
TTimo - this was not in the 1.30 filesystem code
using fs_homepath for the file to remove
==============
*/
int FS_Delete( const char *filename ) {
	char osPath[MAX_OSPATH];
	char c;

	FS_CheckFileSystemStarted();
	if ( filename && filename[0] && !Q_strncmp( filename, "save", 4 ) ) {
		c = filename[4];
		if ( c == '/' || c == '\\' ) {
			FS_BuildOSPath( fs_homepath->string, fs_gamedir, filename, osPath );
			if ( remove( osPath ) != -1 ) {
				return 1;
			}
		}
	}
	return 0;
}

/*
=============
FS_MakeReadOnly
=============
*/
qboolean FS_MakeReadOnly( const char *filename, qboolean readOnly ) {
	char osPath[MAX_OSPATH];
	DWORD attr, newAttr;

	FS_CheckFileSystemStarted();
	FS_BuildOSPath( fs_homepath->string, fs_gamedir, filename, osPath );
	attr = GetFileAttributesA( osPath );
	if ( !readOnly ) {
		newAttr = attr & ~FILE_ATTRIBUTE_READONLY;
	} else {
		newAttr = attr | FILE_ATTRIBUTE_READONLY;
	}
	if ( newAttr == attr ) {
		return qfalse;
	}
	SetFileAttributesA( osPath, newAttr );
	return qtrue;
}

/*
=================
FS_Read

Properly handles partial reads
=================
*/
int FS_Read( void *buffer, int len, fileHandle_t f ) {
	int block, remaining;
	int read;
	byte *buf;
	int tries;

	FS_CheckFileSystemStarted();
	if ( !f ) {
		return 0;
	}

	buf = (byte *)buffer;
	if ( fsh[f].zipFile == qfalse ) {
		remaining = len;
		tries = 0;
		while ( remaining ) {
			block = remaining;
			read = fread( buf, 1, block, fsh[f].handleFiles.file.o );
			if ( read == 0 ) {
				if ( tries ) {
					return len - remaining;
				}
				tries = 1;
			} else if ( read == -1 ) {
				if ( f >= 51 && f < MAX_FILE_HANDLES ) {
					return -1;
				}
				Com_Error( ERR_FATAL, "\x15" "FS_Read: -1 bytes read" );
			}
			remaining -= read;
			buf += read;
		}
		return len;
	}
	return unzReadCurrentFile( fsh[f].handleFiles.file.z, buffer, len );
}

/*
=================
FS_Write

Properly handles partial writes
=================
*/
int FS_Write( const void *buffer, int len, fileHandle_t f ) {
	int remaining, written, tries;
	const byte *buf;
	FILE *file;

	FS_CheckFileSystemStarted();
	if ( !f ) {
		return 0;
	}
	file = FS_FileForHandle( f );
	buf = buffer;
	remaining = len;
	tries = 0;
	while ( remaining ) {
		written = fwrite( buf, 1, remaining, file );
		if ( written == 0 ) {
			if ( tries ) {
				Com_Printf( "FS_Write: 0 bytes written\n" );
				return 0;
			}
			tries = 1;
		} else if ( written == -1 ) {
			Com_Printf( "FS_Write: -1 bytes written\n" );
			return 0;
		}
		remaining -= written;
		buf += written;
	}
	if ( fsh[f].handleSync ) {
		fflush( file );
	}
	return len;
}

/*
=============
FS_Printf
=============
*/
void FS_Printf( fileHandle_t f, const char *fmt, ... ) {
	va_list argptr;
	char msg[4096];

	va_start( argptr, fmt );
	vsprintf( msg, fmt, argptr );
	va_end( argptr );

	FS_Write( msg, strlen( msg ), f );
}

/*
=================
FS_Seek

=================
*/
int FS_Seek( fileHandle_t f, int offset, int origin ) {
	byte buffer[65536];
	int remaining, read;
	int pos;
	int fseek_origin;

	FS_CheckFileSystemStarted();

	if ( fsh[f].zipFile == qtrue ) {
		if ( offset == 0 ) {
			if ( origin == FS_SEEK_SET ) {
				unzSetCurrentFileInfoPosition( fsh[f].handleFiles.file.z, fsh[f].zipFilePos );
				return unzOpenCurrentFile( fsh[f].handleFiles.file.z );
			}
			if ( origin == FS_SEEK_CUR ) {
				return 0;
			}
		}

		pos = unztell( fsh[f].handleFiles.file.z );

		if ( origin == FS_SEEK_CUR ) {
			if ( offset < 0 ) {
				unzSetCurrentFileInfoPosition( fsh[f].handleFiles.file.z, fsh[f].zipFilePos );
				unzOpenCurrentFile( fsh[f].handleFiles.file.z );
				remaining = offset + pos;
			} else {
				remaining = offset;
			}
		} else if ( origin == FS_SEEK_END ) {
			if ( offset + FS_filelength( f ) < pos ) {
				unzSetCurrentFileInfoPosition( fsh[f].handleFiles.file.z, fsh[f].zipFilePos );
				unzOpenCurrentFile( fsh[f].handleFiles.file.z );
				remaining = FS_filelength( f ) + offset;
			} else {
				remaining = FS_filelength( f ) + ( offset - pos );
			}
		} else if ( origin == FS_SEEK_SET ) {
			if ( offset < pos ) {
				unzSetCurrentFileInfoPosition( fsh[f].handleFiles.file.z, fsh[f].zipFilePos );
				unzOpenCurrentFile( fsh[f].handleFiles.file.z );
				remaining = offset;
			} else {
				remaining = offset - pos;
			}
		} else {
			return -1;
		}

		while ( remaining ) {
			if ( remaining < (int)sizeof( buffer ) ) {
				read = FS_Read( buffer, remaining, f );
				remaining = 0;
			} else {
				read = FS_Read( buffer, sizeof( buffer ), f );
				remaining -= sizeof( buffer );
			}
			if ( !read ) {
				return -1;
			}
		}
		return 0;
	} else {
		FILE *file = FS_FileForHandle( f );
		switch ( origin ) {
		case FS_SEEK_CUR:
			fseek_origin = SEEK_CUR;
			break;
		case FS_SEEK_END:
			fseek_origin = SEEK_END;
			break;
		case FS_SEEK_SET:
			fseek_origin = SEEK_SET;
			break;
		default:
			return 0;
		}
		return fseek( file, offset, fseek_origin );
	}
}

/*
============
FS_ReadFile

Filename are relative to the quake search path
a null buffer will just return the file length without loading
============
*/
int FS_ReadFile( const char *qpath, void **buffer ) {
	fileHandle_t h;
	byte *buf;
	int len;
	qboolean isConfig;

	FS_CheckFileSystemStarted();
	if ( !qpath || !qpath[0] ) {
		Com_Error( ERR_FATAL, "\x15" "FS_ReadFile with empty name\n" );
	}
	buf = NULL;
	isConfig = ( strstr( qpath, ".cfg" ) != NULL );
	len = FS_FOpenFileRead( qpath, &h, qfalse );
	if ( !h ) {
		if ( buffer ) {
			*buffer = NULL;
		}
		return -1;
	}
	if ( !buffer ) {
		FS_FCloseFile( h );
		return len;
	}

	fs_loadStack++;
	buf = Hunk_AllocateTempMemoryInternal( len + 1 );
	*buffer = buf;
	FS_Read( buf, len, h );
	buf[len] = 0;
	FS_FCloseFile( h );
	return len;
}

/*
=============
FS_ResetFiles
=============
*/
void FS_ResetFiles( void ) {
	fs_loadStack = 0;
}

/*
=============
FS_FreeFile
=============
*/
void FS_FreeFile( void *buffer ) {
	FS_CheckFileSystemStarted();
	if ( !buffer ) {
		Com_Error( ERR_FATAL, "\x15" "FS_FreeFile( NULL )" );
	}
	fs_loadStack--;
	Hunk_FreeTempMemory( buffer );
}

/*
============
FS_WriteFile

Filename are reletive to the quake search path
============
*/
void FS_WriteFile( const char *qpath, const void *buffer, int size ) {
	fileHandle_t f;

	FS_CheckFileSystemStarted();
	if ( !qpath || !buffer ) {
		Com_Error( ERR_FATAL, "\x15" "FS_WriteFile: NULL parameter" );
	}
	f = FS_FOpenFileWrite( qpath );
	if ( !f ) {
		Com_Printf( "Failed to open %s\n", qpath );
		return;
	}
	FS_Write( buffer, size, f );
	FS_FCloseFile( f );
}

/*
=================
FS_LoadZipFile

Creates a new pak_t in the search chain for the contents
of a zip file.
=================
*/
static pack_t *FS_LoadZipFile( char *zipfile, const char *basename ) {
	pack_t *pack;
	unzFile uf;
	unz_global_info gi;
	unz_file_info info;
	int i, len, k, headerLen;
	fileInPack_t *buildBuffer;
	char *namePtr;
	char filename[MAX_ZPATH];
	int *headerLongs;
	long hash;

	headerLen = 0;
	uf = unzOpen( zipfile );
	if ( unzGetGlobalInfo( uf, &gi ) ) {
		return NULL;
	}
	fs_packFiles += gi.number_entry;
	len = 0;
	unzGoToFirstFile( uf );
	for ( i = 0; i < gi.number_entry; i++ ) {
		if ( unzGetCurrentFileInfo( uf, &info, filename, sizeof( filename ), NULL, 0, NULL, 0 ) ) {
			break;
		}
		len += strlen( filename ) + 1;
		unzGoToNextFile( uf );
	}
	buildBuffer = Z_MallocInternal( gi.number_entry * sizeof( *buildBuffer ) + len );
	namePtr = (char *)( buildBuffer + gi.number_entry );
	headerLongs = Z_MallocInternal( gi.number_entry * sizeof( *headerLongs ) );
	for ( i = 1; i <= MAX_FILEHASH_SIZE; i <<= 1 ) {
		if ( i > gi.number_entry ) {
			break;
		}
	}
	pack = Z_MallocInternal( sizeof( *pack ) + i * sizeof( *pack->hashTable ) );
	pack->hashSize = i;
	pack->hashTable = (fileInPack_t **)( (char *)pack + sizeof( *pack ) );
	for ( i = 0; i < pack->hashSize; i++ ) {
		pack->hashTable[i] = NULL;
	}
	Q_strncpyz( pack->pakFilename, zipfile, sizeof( pack->pakFilename ) );
	Q_strncpyz( pack->pakBasename, basename, sizeof( pack->pakBasename ) );
	if ( strlen( pack->pakBasename ) > 4 &&
		!Q_stricmp( pack->pakBasename + strlen( pack->pakBasename ) - 4, ".pk3" ) ) {
		pack->pakBasename[strlen( pack->pakBasename ) - 4] = 0;
	}
	pack->handle = uf;
	pack->numfiles = gi.number_entry;
	unzGoToFirstFile( uf );
	for ( i = 0; i < gi.number_entry; i++ ) {
		if ( unzGetCurrentFileInfo( uf, &info, filename, sizeof( filename ), NULL, 0, NULL, 0 ) ) {
			break;
		}
		if ( info.uncompressed_size > 0 ) {
			headerLongs[headerLen++] = LittleLong( info.crc );
		}
		Q_strlwr( filename );
		hash = FS_HashFileName( filename, pack->hashSize );
		buildBuffer[i].name = namePtr;
		strcpy( namePtr, filename );
		for ( k = strlen( namePtr ) - 1; k >= 0; k-- ) {
			if ( namePtr[k] == '/' || namePtr[k] == '\\' ) {
				break;
			}
		}
		buildBuffer[i].data.name = namePtr + k + 1;
		namePtr += strlen( filename ) + 1;
		unzGetCurrentFileInfoPosition( uf, &buildBuffer[i].zipPosition );
		buildBuffer[i].next = pack->hashTable[hash];
		pack->hashTable[hash] = &buildBuffer[i];
		unzGoToNextFile( uf );
	}
	Z_FreeInternal( headerLongs );
	pack->buildBuffer = buildBuffer;
	return pack;
}

/*
=============
FS_ReturnPath
=============
*/
static int FS_ReturnPath( const char *zname, char *zpath, int *depth ) {
	int len, at, newdep;

	newdep = 0;
	zpath[0] = 0;
	len = 0;
	at = 0;
	while ( zname[at] ) {
		if ( zname[at] == '/' || zname[at] == '\\' ) {
			len = at;
			newdep++;
		}
		at++;
	}
	strcpy( zpath, zname );
	zpath[len] = 0;
	if ( len + 1 == at ) {
		newdep--;
	}
	*depth = newdep;
	return len;
}

/*
==================
FS_AddFileToList
==================
*/
static int FS_AddFileToList( const char *name, char *list[MAX_FOUND_FILES], int nfiles ) {
	int i;

	if ( nfiles == MAX_FOUND_FILES - 1 ) {
		return nfiles;
	}
	for ( i = 0; i < nfiles; i++ ) {
		if ( !Q_stricmp( name, list[i] ) ) {
			return nfiles;
		}
	}
	list[nfiles] = CopyStringInternal( name );
	return nfiles + 1;
}

/*
===============
FS_ListFilteredFiles

Returns a uniqued list of files that match the given criteria
from all search paths
===============
*/
char **FS_ListFilteredFiles( const char *path, const char *extension,
	const char *filter, int *numfiles ) {
	int nfiles, i, pathLength, extensionLength;
	int length, pathDepth, temp;
	char **listCopy;
	char *name;
	char *list[MAX_FOUND_FILES];
	searchpath_t *search;
	pack_t *pak;
	fileInPack_t *buildBuffer;
	qboolean wantSubs;
	char zpath[MAX_ZPATH];
	char dirName[64];
	char netpath[MAX_OSPATH];

	FS_CheckFileSystemStarted();
	if ( !path ) {
		*numfiles = 0;
		return NULL;
	}
	if ( !extension ) {
		extension = "";
	}
	if ( *path == '/' || *path == '\\' ) {
		path++;
	}
	wantSubs = Q_stricmp( extension, "/" ) == 0;
	pathLength = strlen( path );
	if ( pathLength && ( path[pathLength - 1] == '\\' || path[pathLength - 1] == '/' ) ) {
		pathLength--;
	}
	extensionLength = strlen( extension );
	nfiles = 0;
	FS_ReturnPath( path, zpath, &pathDepth );
	if ( *path ) {
		pathDepth++;
	}
	for ( search = fs_searchpaths; search; search = search->next ) {
		if ( !FS_UseSearchPath( search ) ) {
			continue;
		}
		if ( search->pack ) {
			pak = search->pack;
			if ( !FS_PakIsPure( pak ) ) {
				continue;
			}
			buildBuffer = pak->buildBuffer;
			for ( i = 0; i < pak->numfiles; i++ ) {
				int zpathLen, depth;
				name = buildBuffer[i].name;

				if ( filter ) {
					if ( !Com_FilterPath( filter, name, qfalse ) ) {
						continue;
					}
					nfiles = FS_AddFileToList( name, list, nfiles );
				} else {
					zpathLen = FS_ReturnPath( name, zpath, &depth );
					if ( depth != pathDepth || pathLength > zpathLen ) {
						continue;
					}
					if ( pathLength > 0 && name[pathLength] != '/' ) {
						continue;
					}
					if ( Q_stricmpn( name, path, pathLength ) ) {
						continue;
					}
					length = strlen( name );
					if ( length < extensionLength ||
						Q_stricmp( name + length - extensionLength, extension ) ) {
						continue;
					}
					temp = pathLength;
					if ( pathLength ) {
						temp++;
					}
					if ( wantSubs ) {
						strcpy( dirName, name + temp );
						dirName[strlen( dirName ) - 1] = 0;
						nfiles = FS_AddFileToList( dirName, list, nfiles );
					} else {
						nfiles = FS_AddFileToList( name + temp, list, nfiles );
					}
				}
			}
		} else if ( search->dir ) {
			int numSysFiles;
			char **sysFiles;

			if ( ( fs_restrict->integer || fs_numServerPaks ) && Q_stricmp( extension, "svg" ) ) {
				continue;
			}
			FS_BuildOSPath( search->dir->path, search->dir->gamedir, path, netpath );
			sysFiles = Sys_ListFiles( netpath, extension, filter, &numSysFiles, wantSubs );
			for ( i = 0; i < numSysFiles; i++ ) {
				name = sysFiles[i];
				nfiles = FS_AddFileToList( name, list, nfiles );
			}
			Sys_FreeFileList( sysFiles );
		}
	}
	*numfiles = nfiles;
	if ( !nfiles ) {
		return NULL;
	}
	listCopy = Z_MallocInternal( ( nfiles + 1 ) * sizeof( *listCopy ) );
	for ( i = 0; i < nfiles; i++ ) {
		listCopy[i] = list[i];
	}
	listCopy[i] = NULL;
	return listCopy;
}

/*
=================
FS_ListFiles
=================
*/
char **FS_ListFiles( const char *path, const char *extension, int *numfiles ) {
	return FS_ListFilteredFiles( path, extension, NULL, numfiles );
}

/*
=================
FS_FreeFileList
=================
*/
void FS_FreeFileList( char **list ) {
	int i;

	FS_CheckFileSystemStarted();
	if ( !list ) {
		return;
	}
	for ( i = 0; list[i]; i++ ) {
		Z_FreeInternal( list[i] );
	}
	Z_FreeInternal( list );
}

/*
================
FS_GetFileList
================
*/
int FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize ) {
	int nFiles, i, nTotal, nLen;
	char **pFiles;

	*listbuf = 0;
	nFiles = 0;
	nTotal = 0;
	pFiles = FS_ListFiles( path, extension, &nFiles );
	for ( i = 0; i < nFiles; i++ ) {
		nLen = strlen( pFiles[i] ) + 1;
		if ( nTotal + nLen + 1 < bufsize ) {
			strcpy( listbuf, pFiles[i] );
			listbuf += nLen;
			nTotal += nLen;
		} else {
			nFiles = i;
			break;
		}
	}
	FS_FreeFileList( pFiles );
	return nFiles;
}

/*
===========
FS_ConvertPath
===========
*/
void FS_ConvertPath( char *s ) {
	while ( *s ) {
		if ( *s == '\\' || *s == ':' ) {
			*s = '/';
		}
		s++;
	}
}

/*
===========
FS_PathCmp

Ignore case and seprator char distinctions
===========
*/
int FS_PathCmp( const char *s1, const char *s2 ) {
	int c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;
		if ( Q_islower( c1 ) ) {
			c1 -= 'a' - 'A';
		}
		if ( Q_islower( c2 ) ) {
			c2 -= 'a' - 'A';
		}
		if ( c1 == '\\' || c1 == ':' ) {
			c1 = '/';
		}
		if ( c2 == '\\' || c2 == ':' ) {
			c2 = '/';
		}
		if ( c1 < c2 ) {
			return -1;
		}
		if ( c1 > c2 ) {
			return 1;
		}
	} while ( c1 );
	return 0;
}

/*
================
FS_SortFileList
================
*/
void FS_SortFileList( char **filelist, int numfiles ) {
	char **sortedlist;
	int i, j;

	sortedlist = Z_MallocInternal( ( numfiles + 1 ) * sizeof( *sortedlist ) );
	sortedlist[0] = NULL;
	for ( i = 0; i < numfiles; i++ ) {
		for ( j = 0; j < i; j++ ) {
			if ( FS_PathCmp( filelist[i], sortedlist[j] ) < 0 ) {
				break;
			}
		}
		{
			int k;
			for ( k = i; k > j; k-- ) {
				sortedlist[k] = sortedlist[k - 1];
			}
		}
		sortedlist[j] = filelist[i];
	}
	Com_Memcpy( filelist, sortedlist, numfiles * sizeof( *filelist ) );
	Z_FreeInternal( sortedlist );
}

/*
=============
FS_DisplayPath
=============
*/
void FS_DisplayPath( qboolean showAll ) {
	searchpath_t *s;
	int i;

	if ( fs_ignoreLozalized->integer ) {
		Com_Printf( "    localized assets are being ignored\n" );
	}

	Com_Printf( "Current search path:\n" );
	for ( s = fs_searchpaths; s; s = s->next ) {
		if ( showAll && !FS_UseSearchPath( s ) ) {
			continue;
		}

		if ( s->pack ) {
			Com_Printf( "%s (%i files)\n", s->pack->pakFilename, s->pack->numfiles );
			if ( fs_numServerPaks ) {
				if ( !FS_PakIsPure( s->pack ) ) {
					Com_Printf( "    not on the pure list\n" );
				} else {
					Com_Printf( "    on the pure list\n" );
				}
			}
		} else {
			Com_Printf( "%s/%s\n", s->dir->path, s->dir->gamedir );
		}
	}

	Com_Printf( "\nFile Handles:\n" );
	for ( i = 1; i < MAX_FILE_HANDLES; i++ ) {
		if ( fsh[i].handleFiles.file.o ) {
			Com_Printf( "handle %i: %s\n", i, fsh[i].name );
		}
	}
}

/*
============
FS_Path_f

============
*/
void FS_Path_f( void ) {
	FS_DisplayPath( qfalse );
}

/*
=============
FS_FullPath_f
=============
*/
void FS_FullPath_f( void ) {
	FS_DisplayPath( qfalse );
}

/*
=============
PakFileLanguage
=============
*/
static char *PakFileLanguage( const char *pakName ) {
	static char buffers[2][64];
	static int index = 0;
	int i;

	index ^= 1;
	if ( strlen( pakName ) < 10 ) {
		buffers[index][0] = 0;
	} else {
		memset( buffers[index], 0, sizeof( buffers[0] ) );
		for ( i = 10; i < 64; i++ ) {
			if ( !pakName[i] ) {
				break;
			}
			if ( !isalpha( pakName[i] ) ) {
				break;
			}
			buffers[index][i - 10] = pakName[i];
		}
	}
	return buffers[index];
}

/*
=============
paksort
=============
*/
static int paksort( const void *a, const void *b ) {
	const char *const *pa = a;
	const char *const *pb = b;
	const char *aa = *pa;
	const char *bb = *pb;
	char *langA, *langB;

	if ( !Q_strncmp( aa, "          ", 10 ) &&
		!Q_strncmp( bb, "          ", 10 ) ) {
		langA = PakFileLanguage( aa );
		langB = PakFileLanguage( bb );
		if ( !Q_stricmp( langA, "english" ) ) {
			if ( Q_stricmp( langB, "english" ) ) {
				return -1;
			}
		} else if ( !Q_stricmp( langB, "english" ) ) {
			return 1;
		}
	}
	return FS_PathCmp( aa, bb );
}

/*
=============
FS_AddSearchPath
=============
*/
static void FS_AddSearchPath( searchpath_t *sp ) {
	searchpath_t *s;

	if ( sp->localized ) {
		s = fs_searchpaths;
		if ( s ) {
			while ( s->next ) {
				if ( s->next->localized ) {
					break;
				}
				s = s->next;
			}
			sp->next = s->next;
			s->next = sp;
			return;
		}
	}
	sp->next = fs_searchpaths;
	fs_searchpaths = sp;
}

/*
=============
FS_AddPakFilesForGameDirectory
=============
*/
void FS_AddPakFilesForGameDirectory( const char *path, const char *dir,
	qboolean localized, int language ) {
	char pakfile[MAX_OSPATH];
	char **pakfiles;
	char *sortedfiles[MAX_SEARCH_PATHS];
	int numfiles, i;
	searchpath_t *search;
	pack_t *pak;
	qboolean pakLocalized;
	char *lang;

	FS_BuildOSPath( path, dir, "", pakfile );
	pakfile[strlen( pakfile ) - 1] = 0;
	pakfiles = Sys_ListFiles( pakfile, ".pk3", NULL, &numfiles, qfalse );
	if ( numfiles > MAX_SEARCH_PATHS ) {
		Com_Printf( "WARNING: Exceeded max number of pak files in %s/%s (%1/%1)\n",
			path, dir, numfiles, MAX_SEARCH_PATHS );
		numfiles = MAX_SEARCH_PATHS;
	}
	for ( i = 0; i < numfiles; i++ ) {
		sortedfiles[i] = pakfiles[i];
		if ( !Q_strncmp( sortedfiles[i], "sp_", 3 ) ) {
			memcpy( sortedfiles[i], "zz", 2 );
		}
		if ( !Q_strncmp( sortedfiles[i], "localized_", 10 ) ) {
			memcpy( sortedfiles[i], "          ", 10 );
		}
	}
	qsort( sortedfiles, numfiles, sizeof( sortedfiles[0] ), paksort );
	for ( i = 0; i < numfiles; i++ ) {
		if ( !Q_strncmp( sortedfiles[i], "mp_", 3 ) ) {
			continue;
		}
		if ( !Q_strncmp( sortedfiles[i], "zz_", 3 ) ) {
			Q_strncpyz( sortedfiles[i], "sp_", 2 );
			sortedfiles[i][2] = '_';
		}
		if ( !Q_strncmp( sortedfiles[i], "          ", 10 ) ) {
			memcpy( sortedfiles[i], "localized_", 10 );
			pakLocalized = qtrue;
			lang = PakFileLanguage( sortedfiles[i] );
			if ( !lang[0] ) {
				Com_Printf( "WARNING: Localized assets pak file %s/%s/%s has invalid name "
					"(no language specified). Proper naming convention is: "
					"localized_[language]_pak#.pk3\n", path, dir, sortedfiles[i] );
				continue;
			}
			if ( Q_stricmp( lang, "english" ) ) {
				continue;
			}
		} else {
			pakLocalized = qfalse;
		}
		FS_BuildOSPath( path, dir, sortedfiles[i], pakfile );
		pak = FS_LoadZipFile( pakfile, sortedfiles[i] );
		if ( !pak ) {
			continue;
		}
		strcpy( pak->pakGamename, dir );
		search = Z_MallocInternal( sizeof( *search ) );
		search->pack = pak;
		search->localized = pakLocalized;
		search->language = 0;
		FS_AddSearchPath( search );
	}
	Sys_FreeFileList( pakfiles );
}

/*
================
FS_AddGameDirectory

Sets fs_gamedir, adds the directory to the head of the path,
then loads the zip headers
================
*/
static void FS_AddGameDirectory( const char *path, const char *gameName,
	qboolean localized, int language ) {
	searchpath_t *sp;
	searchpath_t *search;
	char folder[64];
	char netpath[MAX_OSPATH];

	if ( localized ) {
		Com_sprintf( folder, sizeof( folder ), "%s_%s", gameName, "english" );
	} else {
		Q_strncpyz( folder, gameName, sizeof( folder ) );
	}
	for ( sp = fs_searchpaths; sp; sp = sp->next ) {
		if ( sp->dir && !Q_stricmp( sp->dir->path, path ) &&
			!Q_stricmp( sp->dir->gamedir, folder ) ) {
			if ( sp->localized != localized ) {
				Com_Printf( "WARNING: game folder %s/%s added as both localized & non-localized. Using folder as %s\n",
					path, folder, sp->localized ? "localized" : "non-localized" );
			}
			if ( sp->localized && sp->language != language ) {
				Com_Printf( "WARNING: game golder %s/%s re-added as localized folder with different language\n" );
			}
			return;
		}
	}
	if ( localized ) {
		FS_BuildOSPath( path, folder, "", netpath );
		netpath[strlen( netpath ) - 1] = 0;
		if ( !Sys_DirectoryHasContents( netpath ) ) {
			return;
		}
	} else {
		Q_strncpyz( fs_gamedir, folder, sizeof( fs_gamedir ) );
	}
	search = Z_MallocInternal( sizeof( *search ) );
	search->dir = Z_MallocInternal( sizeof( *search->dir ) );
	Q_strncpyz( search->dir->path, path, sizeof( search->dir->path ) );
	Q_strncpyz( search->dir->gamedir, folder, sizeof( search->dir->gamedir ) );
	search->localized = localized;
	search->language = language;
	FS_AddSearchPath( search );
	FS_AddPakFilesForGameDirectory( path, folder, localized, language );
}

/*
=============
FS_AddLocalizedGameDirectory
=============
*/
static void FS_AddLocalizedGameDirectory( const char *path, const char *gameName ) {
	FS_AddGameDirectory( path, gameName, qfalse, 0 );
	FS_AddGameDirectory( path, gameName, qtrue, 0 );
}

/*
=============
FS_AddNonPackFileDirectory_Internal
=============
*/
void FS_AddNonPackFileDirectory_Internal( const char *path, const char *gamedir,
	const char *dir, const char *extension ) {
	char folder[MAX_OSPATH];
	char netpath[MAX_OSPATH];
	char **files;
	int numFiles, len, i;
	long hash;
	fs_dirFileList_t *list;
	fs_dirFile_t *buildBuffer;
	fs_dirFile_t *file;
	char *namePtr;

	sprintf( folder, "%s/%s", gamedir, dir );
	FS_BuildOSPath( path, folder, "", netpath );
	netpath[strlen( netpath ) - 1] = 0;
	files = Sys_ListFiles( netpath, extension, NULL, &numFiles, qfalse );
	len = 0;
	for ( i = 0; i < numFiles; i++ ) {
		len += strlen( files[i] ) + 1;
	}
	for ( i = 1; i <= MAX_FILEHASH_SIZE; i <<= 1 ) {
		if ( i > numFiles ) {
			break;
		}
	}
	list = Z_MallocInternal( sizeof( *list ) + i * sizeof( *list->hashTable ) );
	list->hashTable = (fs_dirFile_t **)( (char *)list + sizeof( *list ) );
	list->hashSize = i;
	for ( i = 0; i < list->hashSize; i++ ) {
		list->hashTable[i] = NULL;
	}
	Q_strncpyz( list->dir, dir, sizeof( list->dir ) );
	list->next = fs_dirFileLists;
	fs_dirFileLists = list;
	list->numFiles = numFiles;
	buildBuffer = Z_MallocInternal( numFiles * sizeof( *buildBuffer ) + len );
	file = buildBuffer;
	namePtr = (char *)( buildBuffer + numFiles );
	for ( i = 0; i < numFiles; i++, file++ ) {
		char *filename = files[i];

		Q_strlwr( filename );
		hash = FS_HashFileName( filename, list->hashSize );
		file->data.name = namePtr;
		strcpy( namePtr, filename );
		namePtr += strlen( filename ) + 1;
		file->next = list->hashTable[hash];
		file->data.data = NULL;
		file->data.clearCallback = NULL;
		list->hashTable[hash] = file;
	}
	list->buildBuffer = buildBuffer;
	Sys_FreeFileList( files );
}

/*
=============
FS_AddNonPackFileDirectory
=============
*/
void FS_AddNonPackFileDirectory( const char *dir, const char *extension ) {
	searchpath_t *search;

	for ( search = fs_searchpaths; search; search = search->next ) {
		if ( search->pack ) {
			continue;
		}
		FS_AddNonPackFileDirectory_Internal( search->dir->path, search->dir->gamedir,
			dir, extension );
	}
}

/*
=============
FS_GetDataForFile
=============
*/
fileData_t *FS_GetDataForFile( const char *dir, const char *name, const char *ext ) {
	char filename[1024];
	searchpath_t *search;
	fileInPack_t *pakFile;
	fs_dirFileList_t *list;
	fs_dirFile_t *file;
	long hash;

	sprintf( filename, "%s/%s%s", dir, name, ext );
	for ( search = fs_searchpaths; search; search = search->next ) {
		if ( !FS_UseSearchPath( search ) || !search->pack ) {
			continue;
		}
		hash = FS_HashFileName( filename, search->pack->hashSize );
		for ( pakFile = search->pack->hashTable[hash]; pakFile; pakFile = pakFile->next ) {
			if ( !FS_FilenameCompare( pakFile->name, filename ) ) {
				return &pakFile->data;
			}
		}
	}

	sprintf( filename, "%s%s", name, ext );
	for ( list = fs_dirFileLists; list; list = list->next ) {
		if ( stricmp( list->dir, dir ) ) {
			continue;
		}
		hash = FS_HashFileName( filename, list->hashSize );
		for ( file = list->hashTable[hash]; file; file = file->next ) {
			if ( !FS_FilenameCompare( file->data.name, filename ) ) {
				return &file->data;
			}
		}
	}
	return NULL;
}

/*
=============
FS_ClearDataForFiles
=============
*/
void FS_ClearDataForFiles( void *lowEnd, void *highEnd ) {
	searchpath_t *search;
	fileInPack_t *pakFile;
	fs_dirFileList_t *list;
	fs_dirFile_t *entry;
	int i;

	for ( search = fs_searchpaths; search; search = search->next ) {
		if ( !search->pack ) {
			continue;
		}
		for ( i = 0; i < search->pack->hashSize; i++ ) {
			for ( pakFile = search->pack->hashTable[i]; pakFile; pakFile = pakFile->next ) {
				if ( pakFile->data.data < lowEnd || pakFile->data.data >= highEnd ) {
					continue;
				}
				if ( pakFile->data.clearCallback ) {
					pakFile->data.clearCallback( &pakFile->data );
					pakFile->data.clearCallback = NULL;
				}
				pakFile->data.data = NULL;
			}
		}
	}
	for ( list = fs_dirFileLists; list; list = list->next ) {
		for ( i = 0; i < list->hashSize; i++ ) {
			for ( entry = list->hashTable[i]; entry; entry = entry->next ) {
				if ( entry->data.data < lowEnd || entry->data.data >= highEnd ) {
					continue;
				}
				if ( entry->data.clearCallback ) {
					entry->data.clearCallback( &entry->data );
					entry->data.clearCallback = NULL;
				}
				entry->data.data = NULL;
			}
		}
	}
}

/*
=============
FS_ShutdownSearchPaths
=============
*/
static void FS_ShutdownSearchPaths( searchpath_t *paths ) {
	searchpath_t *p, *next;

	for ( p = paths; p; p = next ) {
		next = p->next;
		if ( p->pack ) {
			unzClose( p->pack->handle );
			Z_FreeInternal( p->pack->buildBuffer );
			Z_FreeInternal( p->pack );
		}
		if ( p->dir ) {
			Z_FreeInternal( p->dir );
		}
		Z_FreeInternal( p );
	}
}

/*
=============
FS_ShutdownFileLists
=============
*/
static void FS_ShutdownFileLists( fs_dirFileList_t *lists ) {
	fs_dirFileList_t *list, *next;

	for ( list = lists; list; list = next ) {
		next = list->next;
		Z_FreeInternal( list->buildBuffer );
		Z_FreeInternal( list );
	}
}

/*
=============
FS_ClearMemory
=============
*/
void FS_ClearMemory( void ) {
}

/*
=============
FS_ShutdownServerPakNames
=============
*/
void FS_ShutdownServerPakNames( void ) {
	int i;

	for ( i = 0; i < fs_numServerPaks; i++ ) {
		if ( fs_serverPakNames[i] ) {
			Z_FreeInternal( fs_serverPakNames[i] );
		}
		fs_serverPakNames[i] = NULL;
	}
	fs_numServerPaks = 0;
}

/*
=============
FS_ShutdownServerReferencedPaks
=============
*/
void FS_ShutdownServerReferencedPaks( void ) {
	int i;

	for ( i = 0; i < fs_numServerReferencedPaks; i++ ) {
		if ( fs_serverReferencedPakNames[i] ) {
			Z_FreeInternal( fs_serverReferencedPakNames[i] );
		}
		fs_serverReferencedPakNames[i] = NULL;
	}
	fs_numServerReferencedPaks = 0;
}

/*
================
FS_Shutdown

Frees all resources and closes all files
================
*/
void FS_Shutdown( qboolean closemfp ) {
	int i;

	for ( i = 1; i < MAX_FILE_HANDLES; i++ ) {
		if ( fsh[i].fileSize ) {
			FS_FCloseFile( i );
		}
	}
	FS_ShutdownSearchPaths( fs_searchpaths );
	FS_ShutdownFileLists( fs_dirFileLists );
	fs_searchpaths = NULL;
	fs_dirFileLists = NULL;
}

/*
================
FS_Startup
================
*/
void FS_Startup( const char *gameName ) {
	const char *homePath;

	Com_Printf( "----- FS_Startup -----\n" );
	fs_debug = Cvar_Get( "fs_debug", "0", 0 );
	fs_copyfiles = Cvar_Get( "fs_copyfiles", "0", CVAR_INIT );
	fs_cdpath = Cvar_Get( "fs_cdpath", Sys_DefaultCDPath(), CVAR_INIT );
	fs_basepath = Cvar_Get( "fs_basepath", Sys_DefaultInstallPath(), CVAR_INIT );
	fs_basegame = Cvar_Get( "fs_basegame", "", CVAR_INIT );
	homePath = Sys_DefaultHomePath();
	if ( !homePath || !*homePath ) {
		homePath = fs_basepath->string;
	}
	fs_homepath = Cvar_Get( "fs_homepath", homePath, CVAR_INIT );
	fs_gamedirvar = Cvar_Get( "fs_game", "", CVAR_INIT | CVAR_SYSTEMINFO );
	fs_restrict = Cvar_Get( "fs_restrict", "", CVAR_INIT );
	fs_ignoreLozalized = Cvar_Get( "fs_ignoreLozalized", "0", CVAR_CHEAT | CVAR_LATCH );
	if ( fs_cdpath->string[0] ) {
		FS_AddLocalizedGameDirectory( fs_cdpath->string, gameName );
	}
	if ( fs_basepath->string[0] ) {
		FS_AddLocalizedGameDirectory( fs_basepath->string, gameName );
	}
	if ( fs_basepath->string[0] && Q_stricmp( fs_homepath->string, fs_basepath->string ) ) {
		FS_AddLocalizedGameDirectory( fs_homepath->string, gameName );
	}
	if ( fs_basegame->string[0] && !Q_stricmp( gameName, "main" ) &&
		Q_stricmp( fs_basegame->string, gameName ) ) {
		if ( fs_cdpath->string[0] ) {
			FS_AddLocalizedGameDirectory( fs_cdpath->string, fs_basegame->string );
		}
		if ( fs_basepath->string[0] ) {
			FS_AddLocalizedGameDirectory( fs_basepath->string, fs_basegame->string );
		}
		if ( fs_homepath->string[0] && Q_stricmp( fs_homepath->string, fs_basepath->string ) ) {
			FS_AddLocalizedGameDirectory( fs_homepath->string, fs_basegame->string );
		}
	}
	if ( fs_gamedirvar->string[0] && !Q_stricmp( gameName, "main" ) &&
		Q_stricmp( fs_gamedirvar->string, gameName ) ) {
		if ( fs_cdpath->string[0] ) {
			FS_AddLocalizedGameDirectory( fs_cdpath->string, fs_gamedirvar->string );
		}
		if ( fs_basepath->string[0] ) {
			FS_AddLocalizedGameDirectory( fs_basepath->string, fs_gamedirvar->string );
		}
		if ( fs_homepath->string[0] && Q_stricmp( fs_homepath->string, fs_basepath->string ) ) {
			FS_AddLocalizedGameDirectory( fs_homepath->string, fs_gamedirvar->string );
		}
	}
	FS_AddNonPackFileDirectory( "xanim", "" );
	FS_AddNonPackFileDirectory( "xmodel", "" );
	FS_AddNonPackFileDirectory( "xmodelparts", "" );
	FS_AddNonPackFileDirectory( "xmodelsurfs", "" );
	FS_AddNonPackFileDirectory( "aitype", "" );
	FS_AddNonPackFileDirectory( "animscripts/traverse", "" );
	FS_AddNonPackFileDirectory( "weapons", "" );
	FS_AddNonPackFileDirectory( "animtrees", ".atr" );
	FS_FullPath_f();
	fs_gamedirvar->modified = qfalse;
	Com_Printf( "----------------------\n" );
	Com_Printf( "%d files in pk3 files\n", fs_packFiles );
}

/*
=====================
FS_ClearPakReferences
=====================
*/
void FS_ClearPakReferences( int flags ) {
	searchpath_t *search;

	for ( search = fs_searchpaths; search; search = search->next ) {
		if ( !search->pack ) {
			continue;
		}
		search->pack->referenced[2] = 0;
		search->pack->referenced[1] = 0;
		if ( !flags ) {
			search->pack->referenced[0] = 0;
			search->pack->referenced[3] = 0;
		}
	}
}

/*
================
FS_InitFilesystem

Called only at inital startup, not when the filesystem
is resetting due to a game change
================
*/
void FS_InitFilesystem( void ) {
	// try to start up normally
	FS_Startup( "main" );

	// if we can't find default.cfg, assume that the paths are
	// busted and error out now, rather than getting an unreadable
	// graphics screen when the font fails to load
	if ( FS_ReadFile( "default.cfg", NULL ) <= 0 ) {
		Com_Error( ERR_FATAL, "Couldn't load %s.  Make sure Call of Duty is run from the correct folder.",
			"default.cfg" );
	}

	Q_strncpyz( lastValidBase, fs_basepath->string, sizeof( lastValidBase ) );
	Q_strncpyz( lastValidGame, fs_gamedirvar->string, sizeof( lastValidGame ) );
}

/*
================
FS_Restart
================
*/
void FS_Restart( int checksumFeed ) {
	FS_Shutdown( qfalse );
	fs_checksumFeed = checksumFeed;
	FS_ClearPakReferences( 0 );
	FS_Startup( "main" );
	if ( FS_ReadFile( "default.cfg", NULL ) <= 0 ) {
		if ( lastValidBase[0] ) {
			Cvar_Set( "fs_basepath", lastValidBase );
			Cvar_Set( "fs_gamedirvar", lastValidGame );
			lastValidBase[0] = 0;
			lastValidGame[0] = 0;
			Cvar_Set( "fs_restrict", "0" );
			FS_Restart( checksumFeed );
			Com_Error( ERR_DROP, "Invalid game folder\n" );
		}
		Com_Error( ERR_FATAL, "Couldn't load %s.  Make sure Call of Duty is run from the correct folder.",
			"default.cfg" );
	}
	Q_strncpyz( lastValidBase, fs_basepath->string, sizeof( lastValidBase ) );
	Q_strncpyz( lastValidGame, fs_gamedirvar->string, sizeof( lastValidGame ) );
}

/*
=================
FS_ConditionalRestart
restart if necessary
=================
*/
qboolean FS_ConditionalRestart( int checksumFeed ) {
	if ( !fs_gamedirvar->modified && checksumFeed == fs_checksumFeed ) {
		return qfalse;
	}
	FS_Restart( checksumFeed );
	return qtrue;
}

/*
=============
FS_FOpenFileByMode
=============
*/
int FS_FOpenFileByMode( const char *qpath, fileHandle_t *f, fsMode_t mode ) {
	int r = FS_UNSET_FILE_LENGTH;
	qboolean sync = qfalse;

	switch ( mode ) {
	case FS_READ:
		r = FS_FOpenFileRead( qpath, f, qtrue );
		break;
	case FS_WRITE:
		*f = FS_FOpenFileWrite( qpath );
		r = 0;
		if ( !*f ) {
			r = -1;
		}
		break;
	case FS_APPEND_SYNC:
		sync = qtrue;
	case FS_APPEND:
		*f = FS_FOpenFileAppend( qpath );
		r = 0;
		if ( !*f ) {
			r = -1;
		}
		break;
	default:
		Com_Error( ERR_FATAL, "\x15" "FSH_FOpenFile: bad mode" );
		break;
	}
	if ( !f ) {
		return r;
	}
	if ( *f ) {
		if ( fsh[*f].zipFile == qtrue ) {
			fsh[*f].baseOffset = unztell( fsh[*f].handleFiles.file.z );
		} else {
			fsh[*f].baseOffset = ftell( fsh[*f].handleFiles.file.o );
		}
		fsh[*f].fileSize = r;
		fsh[*f].streamed = qfalse;
	}
	fsh[*f].handleSync = sync;
	return r;
}

/*
=============
FS_FTell
=============
*/
int FS_FTell( fileHandle_t f ) {
	int pos;

	if ( fsh[f].zipFile == qtrue ) {
		pos = unztell( fsh[f].handleFiles.file.z );
	} else {
		pos = ftell( fsh[f].handleFiles.file.o );
	}
	return pos;
}

/*
=============
FS_Flush
=============
*/
void FS_Flush( fileHandle_t f ) {
	fflush( fsh[f].handleFiles.file.o );
}
