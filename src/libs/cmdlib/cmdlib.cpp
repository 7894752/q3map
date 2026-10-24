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
//
// start of shared cmdlib stuff
// 

#include "cmdlib.h"
#include "windows.h"
#ifdef WIN32
#include <direct.h>
#endif

#define PATHSEPERATOR   '/'

// rad additions
// 11.29.99
PFN_ERR *g_pfnError = NULL;
PFN_PRINTF *g_pfnPrintf = NULL;
PFN_ERR_NUM *g_pfnErrorNum = NULL;
PFN_PRINTF_NUM *g_pfnPrintfNum = NULL;

// rad end

#define MEM_BLOCKSIZE 4096

typedef enum { qfalse, qtrue } qboolean;
extern "C" {
int Q_strncasecmp(const char *, const char *, int);
double I_FloatTime(void);
void SetQdirFromPath(const char *path, const char *baseDir);
char *ExpandArg(const char *path);
char *ExpandGamePath(const char *path);
char *ExpandPath(const char *path);
void FS_InitToolFilesystem( const char *basepath, const char *game, const char *basegame );
void qprintf( const char *format, ... );
void _printf( const char *format, ... );
void    StripExtension (char *path);

qboolean verbose = qfalse;

char qdir[1024];
char gamedir[1024];
char writedir[1024];
char com_token[1024];

void Swap_Init( void );
void Cvar_Init( void );
void Cvar_Set( const char *var_name, const char *value );
void FS_InitFilesystem( void );

#ifdef WIN32
HWND hwndOut = NULL;
qboolean lookedForServer = qfalse;
UINT wm_BroadcastCommand = -1;
#endif

}
/*
=================
Error

For abnormal program terminations in windowed apps
=================
*/
void Error(const char *pFormat, ...)
{
  if (g_pfnError)
  {
    va_list arg_ptr;
    va_start(arg_ptr, pFormat);
    g_pfnError(pFormat, arg_ptr);
    va_end(arg_ptr);
  }
}

/*
=============
Printf
=============
*/
void Printf(const char *pFormat, ...)
{
  if (g_pfnPrintf)
  {
    va_list arg_ptr;
    va_start(arg_ptr, pFormat);
    g_pfnPrintf(pFormat, arg_ptr);
    va_end(arg_ptr);
  }
}

/*
=============
ErrorNum
=============
*/
void ErrorNum(int nErr, const char *pFormat, ...)
{
  if (g_pfnErrorNum)
  {
    va_list arg_ptr;
    va_start(arg_ptr, pFormat);
    g_pfnErrorNum(nErr, pFormat, arg_ptr);
    va_end(arg_ptr);
  }
}

/*
=============
PrintfNum
=============
*/
void PrintfNum(int nErr, const char *pFormat, ...)
{
  if (g_pfnPrintfNum)
  {
    va_list arg_ptr;
    va_start(arg_ptr, pFormat);
    g_pfnPrintfNum(nErr, pFormat, arg_ptr);
    va_end(arg_ptr);
  }
}

/*
=============
SetErrorHandler
=============
*/
extern "C" void SetErrorHandler(PFN_ERR pe)
{
  g_pfnError = pe;
}

/*
=============
SetPrintfHandler
=============
*/
void SetPrintfHandler(PFN_PRINTF pe)
{
  g_pfnPrintf = pe;
}

/*
=============
SetErrorHandlerNum
=============
*/
void SetErrorHandlerNum(PFN_ERR_NUM pe)
{
  g_pfnErrorNum = pe;
}

/*
=============
SetPrintfHandler
=============
*/
void SetPrintfHandler(PFN_PRINTF_NUM pe)
{
  g_pfnPrintfNum = pe;
}

/*
=============
qblockmalloc
=============
*/
void* qblockmalloc(size_t nSize)
{
	void *b;
  // round up to threshold
  int nAllocSize = nSize % MEM_BLOCKSIZE;
  if ( nAllocSize > 0)
  {
    nSize += MEM_BLOCKSIZE - nAllocSize;
  }
	b = malloc(nSize + 1);
	memset (b, 0, nSize);
	return b;
}

/*
=============
qmalloc
=============
*/
void* qmalloc (size_t nSize)
{
	void *b;
	b = malloc(nSize + 1);
	memset (b, 0, nSize);
	return b;
}

/*
================
Q_filelength
================
*/
int Q_filelength (FILE *f)
{
	int		pos;
	int		end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

// FIXME: need error handler
/*
=============
SafeOpenWrite
=============
*/
FILE *SafeOpenWrite (const char *filename)
{
	FILE	*f;

	f = fopen(filename, "wb");

	if (!f)
  {
		Error ("Error opening %s: %s",filename,strerror(errno));
  }

	return f;
}

/*
=============
SafeOpenRead
=============
*/
FILE *SafeOpenRead (const char *filename)
{
	FILE	*f;

	f = fopen(filename, "rb");

	if (!f)
  {
		Error ("Error opening %s: %s",filename,strerror(errno));
  }

	return f;
}

/*
=============
SafeRead
=============
*/
void SafeRead (FILE *f, void *buffer, int count)
{
	if ( (int)fread (buffer, 1, count, f) != count)
		Error ("File read failure");
}

/*
=============
SafeWrite
=============
*/
void SafeWrite (FILE *f, const void *buffer, int count)
{
	if ( (int)fwrite (buffer, 1, count, f) != count)
		Error ("File read failure");
}

/*
==============
LoadFile
==============
*/
int LoadFile (const char *filename, void **bufferptr)
{
	FILE	*f;
	int    length;
	void    *buffer;

  *bufferptr = NULL;
  
  if (filename == NULL || strlen(filename) == 0)
  {
    return -1;
  }

	f = fopen (filename, "rb");
	if (!f)
	{
		return -1;
	}
	length = Q_filelength (f);
	buffer = qblockmalloc (length+1);
	((char *)buffer)[length] = 0;
	SafeRead (f, buffer, length);
	fclose (f);

	*bufferptr = buffer;
	return length;
}

/*
==============
LoadFileNoCrash

returns -1 length if not present
==============
*/
int    LoadFileNoCrash (const char *filename, void **bufferptr)
{
	FILE	*f;
	int    length;
	void    *buffer;

	f = fopen (filename, "rb");
	if (!f)
		return -1;
	length = Q_filelength (f);
	buffer = qmalloc (length+1);
	((char *)buffer)[length] = 0;
	SafeRead (f, buffer, length);
	fclose (f);

	*bufferptr = buffer;
	return length;
}

/*
==============
SaveFile
==============
*/
void    SaveFile (const char *filename, void *buffer, int count)
{
	FILE	*f;

	f = SafeOpenWrite (filename);
	SafeWrite (f, buffer, count);
	fclose (f);
}

/*
=============
DefaultExtension
=============
*/
void DefaultExtension (char *path, char *extension)
{
	char    *src;
//
// if path doesn't have a .EXT, append extension
// (extension should include the .)
//
	src = path + strlen(path) - 1;

	while (*src != PATHSEPERATOR && src != path)
	{
		if (*src == '.')
			return;                 // it has an extension
		src--;
	}

	strcat (path, extension);
}

/*
=============
DefaultPath
=============
*/
void DefaultPath (char *path, char *basepath)
{
	char    temp[128];

	if (path[0] == PATHSEPERATOR)
		return;                   // absolute path location
	strcpy (temp,path);
	strcpy (path,basepath);
	strcat (path,temp);
}

/*
=============
StripFilename
=============
*/
void    StripFilename (char *path)
{
	int             length;

	length = strlen(path)-1;
	while (length > 0 && path[length] != PATHSEPERATOR)
		length--;
	path[length] = 0;
}

/*
=============
StripExtension
=============
*/
void    StripExtension (char *path)
{
	int             length;

	length = strlen(path)-1;
	while (length > 0 && path[length] != '.')
	{
		length--;
		if (path[length] == '/')
			return;		// no extension
	}
	if (length)
		path[length] = 0;
}

/*
==================
Extract file parts
==================
*/
/*
=============
ExtractFilePath
=============
*/
void ExtractFilePath (const char *path, char *dest)
{
	const char *src;

	src = path + strlen(path) - 1;

//
// back up until a \ or the start
//
	while (src != path && *(src-1) != PATHSEPERATOR)
		src--;

	memcpy (dest, path, src-path);
	dest[src-path] = 0;
}

/*
=============
ExtractFileName
=============
*/
void ExtractFileName (const char *path, char *dest)
{
	const char *src;

	src = path + strlen(path) - 1;

//
// back up until a \ or the start
//
	while (src != path && *(src-1) != '/' 
		 && *(src-1) != '\\' )
		src--;

	while (*src)
	{
		*dest++ = *src++;
	}
	*dest = 0;
}

/*
=============
ExtractFileBase
=============
*/
void ExtractFileBase (const char *path, char *dest)
{
	const char *src;

	src = path + strlen(path) - 1;

//
// back up until a \ or the start
//
	while (src != path && *(src-1) != '/' 
		 && *(src-1) != '\\' )
		src--;

	while (*src && *src != '.')
	{
		*dest++ = *src++;
	}
	*dest = 0;
}

/*
=============
ExtractFileExtension
=============
*/
void ExtractFileExtension (const char *path, char *dest)
{
	const char *src;

	src = path + strlen(path) - 1;

//
// back up until a . or the start
//
	while (src != path && *(src-1) != '.')
		src--;
	if (src == path)
	{
		*dest = 0;	// no extension
		return;
	}

	strcpy (dest,src);
}

/*
=============
ConvertDOSToUnixName
=============
*/
void ConvertDOSToUnixName( char *dst, const char *src )
{
	while ( *src )
	{
		if ( *src == '\\' )
			*dst = '/';
		else
			*dst = *src;
		dst++; src++;
	}
	*dst = 0;
}

/*
=============
StrDup
=============
*/
char* StrDup(char* pStr)
{ 
  if (pStr)
  {
    return strcpy(new char[strlen(pStr)+1], pStr); 
  }
  return NULL;
}

/*
=============
StrDup
=============
*/
char* StrDup(const char* pStr)
{ 
  if (pStr)
  {
    return strcpy(new char[strlen(pStr)+1], pStr); 
  }
  return NULL;
}

/*
================
FS_InitToolFilesystem
================
*/
void FS_InitToolFilesystem( const char *basepath, const char *game, const char *basegame ) {
	Swap_Init();
	Cvar_Init();
	if ( basepath ) {
		Cvar_Set( "fs_basepath", basepath );
	}
	if ( basegame ) {
		Cvar_Set( "fs_basegame", basegame );
	}
	if ( game ) {
		Cvar_Set( "fs_game", game );
	}
	FS_InitFilesystem();
}

/*
=============
qprintf
=============
*/
void qprintf( const char *format, ... ) {
	va_list argptr;

	if (!verbose)
		return;

	va_start (argptr,format);
	vprintf (format,argptr);
	va_end (argptr);

}

/*
=============
_printf
=============
*/
void _printf( const char *format, ... ) {
	va_list argptr;
  char text[4096];
  ATOM a;

	va_start (argptr,format);
	vsprintf (text, format, argptr);
	va_end (argptr);

  printf(text);

#ifdef WIN32
  if (!lookedForServer) {
    lookedForServer = qtrue;
    hwndOut = FindWindow(NULL, "Q3Map Process Server");
    if (hwndOut) {
      wm_BroadcastCommand = RegisterWindowMessage( "Q3MPS_BroadcastCommand" );
    }
  }
  if (hwndOut) {
    a = GlobalAddAtom(text);
    PostMessage(hwndOut, wm_BroadcastCommand, 0, (LPARAM)a);
  }
#endif
}

/*
=============
ExpandPath
=============
*/
char *ExpandPath(const char *path)
{
	static char full[1024];

	if (path[0] == '/' || path[0] == '\\' || path[1] == ':')
	{
		strcpy(full, path);
		return full;
	}
	sprintf(full, "%s%s", qdir, path);
	return full;
}

/*
=============
ExpandGamePath
=============
*/
char *ExpandGamePath(const char *path)
{
	static char full[1024];

	if (path[0] == '/' || path[0] == '\\' || path[1] == ':')
	{
		strcpy(full, path);
		return full;
	}
	sprintf(full, "%s%s", gamedir, path);
	return full;
}

/*
=============
copystring
=============
*/
char *copystring(const char *s)
{
	char *b;
	b = (char *)malloc(strlen(s) + 1);
	strcpy(b, s);
	return b;
}

/*
================
I_FloatTime
================
*/
double I_FloatTime(void)
{
	time_t t;

	time(&t);
	return t;
}

/*
=============
Q_getwd
=============
*/
void Q_getwd (char *out)
{
	int i = 0;

#ifdef WIN32
	_getcwd (out, 256);
	strcat (out, "\\");
#else
	getcwd (out, 256);
	strcat (out, "/");
#endif
	while (out[i] != 0)
	{
		if (out[i] == '\\')
			out[i] = '/';
		i++;
	}
}

/*
=============
SetQdirFromPath
=============
*/
void SetQdirFromPath(const char *path, const char *baseDir)
{
	char temp[1024];
	const char *c;
	const char *sep;
	int len, count;

	if (!(path[0] == '/' || path[0] == '\\' || path[1] == ':'))
	{
		Q_getwd(temp);
		strcat(temp, path);
		path = temp;
	}

	len = strlen(baseDir);
	for (c = path + strlen(path) - 1; c != path; c--)
	{
		int i;

		if (!Q_strncasecmp(c, baseDir, len))
		{
			sep = c + len;
			count = 1;
			while (*sep && *sep != '/' && *sep != '\\')
			{
				sep++;
				count++;
			}
			memset(qdir, 0, sizeof(qdir));
			strncpy(qdir, path, c + len + count - path);
			qprintf("qdir: %s\n", qdir);
			for (i = 0; i < strlen(qdir); i++)
			{
				if (qdir[i] == '\\')
					qdir[i] = '/';
			}
			c += len + count;
			while (*c)
			{
				if (*c == '/' || *c == '\\')
				{
					strncpy(gamedir, path, c + 1 - path);
					for (i = 0; i < strlen(gamedir); i++)
					{
						if (gamedir[i] == '\\')
							gamedir[i] = '/';
					}
					qprintf("gamedir: %s\n", gamedir);
					if (!writedir[0])
						strcpy(writedir, gamedir);
					else if (writedir[strlen(writedir) - 1] != '/')
					{
						writedir[strlen(writedir)] = '/';
						writedir[strlen(writedir) + 1] = 0;
					}
					return;
				}
				c++;
			}
			Error("No gamedir in %s", path);
			return;
		}
	}
	Error("SetQdirFromPath: no '%s' in %s", baseDir, path);
}

/*
=============
ExpandArg
=============
*/
char *ExpandArg(const char *path)
{
	static char full[1024];

	if (path[0] != '/' && path[0] != '\\' && path[1] != ':')
	{
		Q_getwd(full);
		strcat(full, path);
	}
	else
		strcpy(full, path);
	return full;
}
