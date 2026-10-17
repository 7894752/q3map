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
#include <time.h>
#include "cvar.h"
#include "../code/qcommon/files_local.h"

#define HUNK_MAGIC 0x89537892U
#define HUNK_FREE_MAGIC 0x89537893U
#define HUNK_TEMP_HEADER_SIZE 16
#define HUNK_TEMP_ALIGNMENT 16
#define HUNK_MIN_MEGABYTES 80
#define HUNK_BYTES_PER_MEGABYTE ( 1024 * 1024 )
#define HUNK_CACHELINE_ALIGNMENT 32
#define HUNK_CVAR_ARCHIVE 1
#define ERR_FATAL 0

typedef struct {
	unsigned int magic;
	int size;
} hunkHeader_t;

typedef enum
{
	PRE_READ,									// prefetch assuming that buffer is used for reading only
	PRE_WRITE,									// prefetch assuming that buffer is used for writing only
	PRE_READ_WRITE								// prefetch assuming that buffer is used for both reading and writing
} e_prefetch;

typedef struct qtime_s {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
} qtime_t;

typedef struct hunkblock_s {
	int size;
	byte printed;
	struct hunkblock_s *next;
	char *label;
	char *file;
	int line;
} hunkblock_t;

typedef struct {
	int mark;
	int tempMark;
	int permanent;
	int temp;
} hunkUsed_t;

/* Definition order reproduces the original .bss layout of this file. */
int s_hunkTotal = 0;
int s_zoneTotal = 0;
hunkUsed_t hunk_high = { 0 };
hunkblock_t *hunkblocks = 0;
fileHandle_t logfile = 0;
hunkUsed_t hunk_low = { 0 };
byte *s_hunkData = 0;

void FS_ClearDataForFiles( void *lowEnd, void *highEnd );
qboolean FS_Initialized( void );
int FS_LoadStack( void );
void FS_ClearMemory( void );
char *va( const char *format, ... );
void Z_FreeInternal( void *block );
void *Z_MallocInternal( int size );
void Hunk_Clear( void );
void Com_Meminfo_f( void );
void Com_Memset( void *dest, const int val, const size_t count );
void Com_Prefetch( const void *s, const unsigned int bytes, e_prefetch type );

/*
================
Com_RealTime
================
*/
int Com_RealTime( qtime_t *qtime ) {
	time_t t;
	struct tm *tms;

	t = time( NULL );
	if ( !qtime ) {
		return t;
	}
	tms = localtime( &t );
	if ( tms ) {
		qtime->tm_sec = tms->tm_sec;
		qtime->tm_min = tms->tm_min;
		qtime->tm_hour = tms->tm_hour;
		qtime->tm_mday = tms->tm_mday;
		qtime->tm_mon = tms->tm_mon;
		qtime->tm_year = tms->tm_year;
		qtime->tm_wday = tms->tm_wday;
		qtime->tm_yday = tms->tm_yday;
		qtime->tm_isdst = tms->tm_isdst;
	}
	return t;
}

/*
=============
Z_FreeInternal
=============
*/
void Z_FreeInternal( void *block ) {
	free( block );
}

/*
=================
Com_Meminfo_f
=================
*/
void Com_Meminfo_f( void ) {
	int totalHunkInUse;
	Com_Printf( "%8i bytes total hunk\n", s_hunkTotal );
	Com_Printf( "%8i bytes total zone\n", s_zoneTotal );
	Com_Printf( "\n" );
	Com_Printf( "%8i low mark\n", hunk_low.mark );
	Com_Printf( "%8i low permanent\n", hunk_low.permanent );
	if ( hunk_low.temp != hunk_low.permanent ) {
		Com_Printf( "%8i low temp\n", hunk_low.temp );
	}
	Com_Printf( "\n" );
	Com_Printf( "%8i high mark\n", hunk_high.mark );
	Com_Printf( "%8i high permanent\n", hunk_high.permanent );
	if ( hunk_high.temp != hunk_high.permanent ) {
		Com_Printf( "%8i high temp\n", hunk_high.temp );
	}
	Com_Printf( "\n" );
	totalHunkInUse = hunk_high.permanent;
	totalHunkInUse += hunk_low.permanent;
	Com_Printf( "%8i total hunk in use\n", totalHunkInUse );
	Com_Printf( "\n" );
}

/*
=================
Com_InitZoneMemory
=================
*/
void Com_InitZoneMemory( void ) {
}

/*
=================
Hunk_Log
=================
*/
void Hunk_Log( void ) {
	hunkblock_t *block;
	char buf[4096];
	int size, numBlocks;

	if ( !logfile || !FS_Initialized() ) {
		return;
	}
	size = 0;
	numBlocks = 0;
	Com_sprintf( buf, sizeof( buf ),
		"\r\n================\r\nHunk log\r\n================\r\n" );
	FS_Write( buf, strlen( buf ), logfile );
	for ( block = hunkblocks; block; block = block->next ) {
		size += block->size;
		numBlocks++;
	}
	Com_sprintf( buf, sizeof( buf ), "%d Hunk memory\r\n", size );
	FS_Write( buf, strlen( buf ), logfile );
	Com_sprintf( buf, sizeof( buf ), "%d hunk blocks\r\n", numBlocks );
	FS_Write( buf, strlen( buf ), logfile );
}

/*
=================
Hunk_SmallLog
=================
*/
void Hunk_SmallLog( void ) {
	hunkblock_t *block, *block2;
	char buf[4096];
	int size, numBlocks;

	if ( !logfile || !FS_Initialized() ) {
		return;
	}
	for ( block = hunkblocks; block; block = block->next ) {
		block->printed = qfalse;
	}
	size = 0;
	numBlocks = 0;
	Com_sprintf( buf, sizeof( buf ),
		"\r\n================\r\nHunk Small log\r\n================\r\n" );
	FS_Write( buf, strlen( buf ), logfile );
	for ( block = hunkblocks; block; block = block->next ) {
		if ( block->printed ) {
			continue;
		}
		for ( block2 = block->next; block2; block2 = block2->next ) {
			if ( block->line != block2->line ) {
				continue;
			}
			if ( Q_stricmp( block->file, block2->file ) ) {
				continue;
			}
			size += block2->size;
			block2->printed = qtrue;
		}
		size += block->size;
		numBlocks++;
	}
	Com_sprintf( buf, sizeof( buf ), "%d Hunk memory\r\n", size );
	FS_Write( buf, strlen( buf ), logfile );
	Com_sprintf( buf, sizeof( buf ), "%d hunk blocks\r\n", numBlocks );
	FS_Write( buf, strlen( buf ), logfile );
}

/*
====================
Hunk_MemoryRemaining
====================
*/
int Hunk_MemoryRemaining( void ) {
	return s_hunkTotal - hunk_low.temp - hunk_high.temp;
}

/*
=============
Hunk_ClearData
=============
*/
void Hunk_ClearData( void ) {
	FS_ClearDataForFiles( s_hunkData + hunk_low.permanent,
		s_hunkData + s_hunkTotal - hunk_high.permanent );
}

/*
=============
Hunk_SetMark2
=============
*/
int Hunk_SetMark2( void ) {
	hunk_high.mark = hunk_high.permanent;
	return hunk_high.permanent;
}

/*
=============
Hunk_SetHighTempMark
=============
*/
int Hunk_SetHighTempMark( void )
{
	hunk_high.tempMark = hunk_high.permanent;
	return hunk_high.permanent;
}

/*
=================
Hunk_CheckMark
=================
*/
qboolean Hunk_CheckMark( const void *ptr ) {
	return (const byte *)ptr >= s_hunkData + s_hunkTotal - hunk_high.mark;
}

/*
=============
Hunk_ClearToMark2
=============
*/
void Hunk_ClearToMark2( void ) {
	hunk_high.temp = hunk_high.mark;
	hunk_high.permanent = hunk_high.mark;
	Hunk_ClearData();
}

/*
=============
Hunk_ClearToHighTempMark
=============
*/
void Hunk_ClearToHighTempMark( void ) {
	hunk_high.temp = hunk_high.tempMark;
	hunk_high.permanent = hunk_high.tempMark;
	Hunk_ClearData();
}

/*
=============
Hunk_HighMarkIsSet
=============
*/
qboolean Hunk_HighMarkIsSet( void )
{
	return hunk_high.mark != 0;
}

/*
=============
Hunk_SetMarkLow
=============
*/
int Hunk_SetMarkLow( void )
{
	hunk_low.mark = hunk_low.permanent;
	return hunk_low.permanent;
}

/*
=============
Hunk_ClearToMarkLow
=============
*/
void Hunk_ClearToMarkLow( void ) {
	hunk_low.temp = hunk_low.mark;
	hunk_low.permanent = hunk_low.mark;
	Hunk_ClearData();
}

/*
=============
Hunk_ClearToStart
=============
*/
void Hunk_ClearToStart( void ) {
	hunk_low.mark = 0;
	hunk_low.tempMark = 0;
	hunk_low.permanent = 0;
	hunk_low.temp = 0;
	hunk_high.mark = 0;
	hunk_high.tempMark = 0;
	hunk_high.permanent = 0;
	hunk_high.temp = 0;
	Com_Printf( "Hunk_Clear: reset the hunk ok\n" );
	Hunk_ClearData();
	FS_ClearMemory();
}

/*
=================
Hunk_Clear

The server calls this before shutting down or loading a new map
=================
*/
void Hunk_Clear( void ) {
	Hunk_ClearToStart();
}

/*
=============
Hunk_AllocAlignInternal
=============
*/
void *Hunk_AllocAlignInternal( int size, int align ) {
	int mark;
	void *block;

	align--;
	if ( !s_hunkData ) {
		Com_Error( ERR_FATAL, "\x15Hunk_AllocAlign: Hunk memory system not initialized" );
	}

	mark = ( align + size + hunk_high.permanent ) & ~align;
	block = s_hunkData + s_hunkTotal - mark;
	hunk_high.permanent = mark;
	hunk_high.temp = mark;

	if ( mark + hunk_low.temp > s_hunkTotal ) {
		Com_Meminfo_f();
		Com_Error( ERR_DROP, "\x15Hunk_AllocAlign failed on %i", size );
	}

	memset( block, 0, size );
	return block;
}

/*
=============
Hunk_AllocateTempMemoryHighInternal
=============
*/
void *Hunk_AllocateTempMemoryHighInternal( int size ) {
	hunk_high.temp = ( hunk_high.temp + size + HUNK_TEMP_ALIGNMENT - 1 )
		& ~( HUNK_TEMP_ALIGNMENT - 1 );
	if ( hunk_high.temp + hunk_low.temp > s_hunkTotal ) {
		Com_Meminfo_f();
		Com_Error( ERR_DROP, "\x15Hunk_AllocateTempMemoryHigh: failed on %i", size );
	}
	return s_hunkData + s_hunkTotal - hunk_high.temp;
}

/*
=============
Hunk_ClearTempMemoryHigh
=============
*/
int Hunk_ClearTempMemoryHigh( void )
{
	hunk_high.temp = hunk_high.permanent;
	return hunk_high.permanent;
}

/*
=============
Hunk_AllocLowAlignInternal
=============
*/
void *Hunk_AllocLowAlignInternal( int size, int align ) {
	void *block;

	align--;
	hunk_low.permanent = ( hunk_low.permanent + align ) & ~align;
	block = s_hunkData + hunk_low.permanent;
	hunk_low.permanent += size;
	hunk_low.temp = hunk_low.permanent;

	if ( hunk_low.permanent + hunk_high.temp > s_hunkTotal ) {
		Com_Meminfo_f();
		Com_Error( ERR_DROP, "\x15Hunk_AllocLowAlign failed on %i", size );
	}

	memset( block, 0, size );
	return block;
}

/*
=============
Hunk_CommitTempMemory
=============
*/
int Hunk_CommitTempMemory( void )
{
	hunk_low.permanent = hunk_low.temp;
	return hunk_low.temp;
}

/*
=============
Hunk_ReallocateTempMemory
=============
*/
void *Hunk_ReallocateTempMemory( int size ) {
	hunk_low.temp = hunk_low.permanent + size;
	if ( hunk_high.temp + hunk_low.temp > s_hunkTotal ) {
		Com_Meminfo_f();
		Com_Error( ERR_DROP, "\x15Hunk_ReallocateTempMemory: failed on %i", size );
	}
	return s_hunkData + hunk_low.permanent;
}

/*
=============
Hunk_ClearToMarkTemp
=============
*/
int Hunk_ClearToMarkTemp( void ) {
	hunk_low.temp = hunk_low.tempMark;
	return hunk_low.tempMark;
}

/*
=============
Hunk_SetMarkTemp
=============
*/
int Hunk_SetMarkTemp( void )
{
	hunk_low.tempMark = hunk_low.temp;
	return hunk_low.temp;
}

/*
==================
Hunk_FreeTempMemory
==================
*/
void Hunk_FreeTempMemory( void *buf ) {
	hunkHeader_t *hdr;
	int delta;

	if ( !s_hunkData ) {
		Z_FreeInternal( buf );
		return;
	}

	hdr = (hunkHeader_t *)( (byte *)buf - HUNK_TEMP_HEADER_SIZE );
	if ( hdr->magic != HUNK_MAGIC ) {
		Com_Error( ERR_FATAL, "\x15Hunk_FreeTempMemory: bad magic" );
	}
	delta = hdr->size;
	hdr->magic = HUNK_FREE_MAGIC;
	hunk_low.temp -= delta;
}

/*
=================
Hunk_ClearTempMemory

The temp space is no longer needed.  If we have left more
touched but unused memory on this side, have future
permanent allocs use this side.
=================
*/
void Hunk_ClearTempMemory( void )
{
	if ( s_hunkData )
		hunk_low.temp = hunk_low.permanent;
}

/*
=============
Hunk_ConvertTempToPermLowInternal
=============
*/
int Hunk_ConvertTempToPermLowInternal( void )
{
	int old;

	old = hunk_low.permanent;
	hunk_low.permanent = hunk_low.temp;
	return old;
}

/*
=============
Hunk_SetLowUsedInternal
=============
*/
int Hunk_SetLowUsedInternal( int lowUsed )
{
	hunk_low.permanent = lowUsed;
	return lowUsed;
}

/*
=================
Hunk_Trash
=================
*/
void Hunk_Trash( void )
{
}

/*
=============
_copyDWord
=============
*/
void _copyDWord (unsigned int* dest, const unsigned int constant, const unsigned int count) {
	__asm
	{
			mov		edx,dest
			mov		eax,constant
			mov		ecx,count
			and		ecx,~7
			jz		padding
			sub		ecx,8
			jmp		loopu
			align	16
loopu:		
			test	[edx+ecx*4 + 28],ebx		// fetch next block destination to L1 cache
			mov		[edx+ecx*4 + 0],eax
			mov		[edx+ecx*4 + 4],eax
			mov		[edx+ecx*4 + 8],eax
			mov		[edx+ecx*4 + 12],eax
			mov		[edx+ecx*4 + 16],eax
			mov		[edx+ecx*4 + 20],eax
			mov		[edx+ecx*4 + 24],eax
			mov		[edx+ecx*4 + 28],eax
			sub		ecx,8
			jge		loopu
padding:	mov		ecx,count
			mov		ebx,ecx
			and		ecx,7
			jz		outta
			and		ebx,~7
			lea		edx,[edx+ebx*4]				// advance dest pointer
			test	[edx+0],eax					// fetch destination to L1 cache
			cmp		ecx,4
			jl		skip4
			mov		[edx+0],eax
			mov		[edx+4],eax
			mov		[edx+8],eax
			mov		[edx+12],eax
			add		edx,16
			sub		ecx,4
skip4:		cmp		ecx,2
			jl		skip2
			mov		[edx+0],eax
			mov		[edx+4],eax
			add		edx,8
			sub		ecx,2
skip2:		cmp		ecx,1
			jl		outta
			mov		[edx+0],eax
outta:
	}
}

/*
=============
Com_Memset
=============
*/
void Com_Memset (void* dest, const int val, const size_t count)
{
	unsigned int fillval;

	if (count < 8)
	{
		__asm
		{
			mov		edx,dest
			mov		eax, val
			mov		ah,al
			mov		ebx,eax
			and		ebx, 0xffff
			shl		eax,16
			add		eax,ebx				// eax now contains pattern
			mov		ecx,count
			cmp		ecx,4
			jl		skip4
			mov		[edx],eax			// copy first dword
			add		edx,4
			sub		ecx,4
	skip4:	cmp		ecx,2
			jl		skip2
			mov		word ptr [edx],ax	// copy 2 bytes
			add		edx,2
			sub		ecx,2
	skip2:	cmp		ecx,0
			je		skip1
			mov		byte ptr [edx],al	// copy single byte
	skip1:
		}
		return;
	}

	fillval = val;
	
	fillval = fillval|(fillval<<8);
	fillval = fillval|(fillval<<16);		// fill dword with 8-bit pattern

	_copyDWord ((unsigned int*)(dest),fillval, count/4);
	
	__asm									// padding of 0-3 bytes
	{
		mov		ecx,count
		mov		eax,ecx
		and		ecx,3
		jz		skipA
		and		eax,~3
		mov		ebx,dest
		add		ebx,eax
		mov		eax,fillval
		cmp		ecx,2
		jl		skipB
		mov		word ptr [ebx],ax
		cmp		ecx,2
		je		skipA					
		mov		byte ptr [ebx+2],al		
		jmp		skipA
skipB:		
		cmp		ecx,0
		je		skipA
		mov		byte ptr [ebx],al
skipA:
	}
}

/*
=============
Com_Memcmp
=============
*/
int Com_Memcmp( const void *src0, const void *src1, const unsigned int count ) {
	unsigned int i;

	if ( count >= 16 ) {
		const unsigned int *dw = src0;
		const unsigned int *sw = src1;
		unsigned int nm2 = count / 16;

		for ( i = 0; i < nm2; i += 4 ) {
			unsigned int tmp = ( dw[i + 0] - sw[i + 0] )
				| ( dw[i + 1] - sw[i + 1] )
				| ( dw[i + 2] - sw[i + 2] )
				| ( dw[i + 3] - sw[i + 3] );
			if ( tmp ) {
				return qfalse;
			}
		}
	}
	if ( count & 15 ) {
		const byte *d = src0;
		const byte *s = src1;

		for ( i = count & 0xfffffff0; i < count; i++ ) {
			if ( d[i] != s[i] ) {
				return qfalse;
			}
		}
	}
	return qtrue;
}

/*
=============
Com_Prefetch
=============
*/
void Com_Prefetch (const void *s, const unsigned int bytes, e_prefetch type)
{
	// write buffer prefetching is performed only if
	// the processor benefits from it. Read and read/write
	// prefetching is always performed.

	switch (type)
	{
		case PRE_WRITE : break;
		case PRE_READ:
		case PRE_READ_WRITE:

		__asm
		{
			mov		ebx,s
			mov		ecx,bytes
			cmp		ecx,4096				// clamp to 4kB
			jle		skipClamp
			mov		ecx,4096
skipClamp:
			add		ecx,0x1f
			shr		ecx,5					// number of cache lines
			jz		skip
			jmp		loopie

			align 16
	loopie:	test	byte ptr [ebx],al
			add		ebx,32
			dec		ecx
			jnz		loopie
	skip:
		}

		break;
	}
}

/*
=============
Z_MallocInternal
=============
*/
void *Z_MallocInternal( int size ) {
	void *block;

	block = malloc( size );
	if ( !block ) {
		Com_Printf( "Z_MallocInternal: failed to allocate %d bytes\n", size );
		Com_Error( ERR_FATAL, "\x15Z_MallocInternal: \x14" "EXE_ERR_OUT_OF_MEMORY" );
	}
	Com_Memset( block, 0, size );
	return block;
}

/*
=============
CopyStringInternal
=============
*/
char *CopyStringInternal( const char *in ) {
	char *out;

	out = Z_MallocInternal( strlen( in ) + 1 );
	strcpy( out, in );
	return out;
}

/*
=============
Com_InitHunkMemory
=============
*/
void Com_InitHunkMemory( void ) {
	cvar_t *cv;

	if ( FS_LoadStack() ) {
		Com_Error( ERR_FATAL,
			"\x15Hunk initialization failed. File system load stack not zero" );
	}
	cv = Cvar_Get( "com_hunkMegs", "128", HUNK_CVAR_ARCHIVE | CVAR_LATCH );
	if ( cv->integer < HUNK_MIN_MEGABYTES ) {
		s_hunkTotal = HUNK_MIN_MEGABYTES * HUNK_BYTES_PER_MEGABYTE;
		Com_Printf( "Minimum com_hunkMegs is %i, allocating %i megs.\n",
			HUNK_MIN_MEGABYTES, HUNK_MIN_MEGABYTES );
	} else {
		s_hunkTotal = cv->integer * HUNK_BYTES_PER_MEGABYTE;
	}
	s_hunkData = malloc( s_hunkTotal + HUNK_CACHELINE_ALIGNMENT - 1 );
	if ( !s_hunkData ) {
		Com_Meminfo_f();
		Com_Error( ERR_FATAL, va( "EXE_ERR_HUNK_ALLOC_FAILED\x15%i",
			s_hunkTotal / HUNK_BYTES_PER_MEGABYTE ) );
	}
	s_hunkData = (byte *)( ( (size_t)s_hunkData + HUNK_CACHELINE_ALIGNMENT - 1 )
		& ~( HUNK_CACHELINE_ALIGNMENT - 1 ) );
	Hunk_Clear();
}

/*
=============
Hunk_AllocInternal
=============
*/
void *Hunk_AllocInternal( int size )
{
	return Hunk_AllocAlignInternal( size, HUNK_CACHELINE_ALIGNMENT );
}

/*
=============
Hunk_AllocLowInternal
=============
*/
void *Hunk_AllocLowInternal( int size )
{
	return Hunk_AllocLowAlignInternal( size, HUNK_CACHELINE_ALIGNMENT );
}

/*
=============
Hunk_AllocateTempMemoryInternal
=============
*/
void *Hunk_AllocateTempMemoryInternal( int size ) {
	hunkHeader_t *hdr;
	int previousLowTemp;

	if ( !s_hunkData ) {
		return Z_MallocInternal( size );
	}

	size += HUNK_TEMP_HEADER_SIZE;
	previousLowTemp = hunk_low.temp;
	hunk_low.temp = ( hunk_low.temp + HUNK_TEMP_ALIGNMENT - 1 ) & ~( HUNK_TEMP_ALIGNMENT - 1 );
	hdr = (hunkHeader_t *)( s_hunkData + hunk_low.temp );
	hunk_low.temp += size;

	if ( hunk_high.temp + hunk_low.temp > s_hunkTotal ) {
		Com_Meminfo_f();
		Com_Error( ERR_DROP, "\x15Hunk_AllocateTempMemory: failed on %i, needs %i",
			size, hunk_high.temp + hunk_low.temp - s_hunkTotal );
	}

	hdr->magic = HUNK_MAGIC;
	hdr->size = hunk_low.temp - previousLowTemp;
	return (byte *)hdr + HUNK_TEMP_HEADER_SIZE;
}

/*
=============
Com_Memcpy
=============
*/
void Com_Memcpy (void* dest, const void* src, const size_t count) {
	Com_Prefetch (src, count, PRE_READ);
	__asm
	{
		push	edi
		push	esi
		mov		ecx,count
		cmp		ecx,0						// count = 0 check (just to be on the safe side)
		je		outta
		mov		edx,dest
		mov		ebx,src
		cmp		ecx,32						// padding only?
		jl		padding

		mov		edi,ecx					
		and		edi,~31					// edi = count&~31
		sub		edi,32

		align 16
loopMisAligned:
		mov		eax,[ebx + edi + 0 + 0*8]
		mov		esi,[ebx + edi + 4 + 0*8]
		mov		[edx+edi+0 + 0*8],eax
		mov		[edx+edi+4 + 0*8],esi
		mov		eax,[ebx + edi + 0 + 1*8]
		mov		esi,[ebx + edi + 4 + 1*8]
		mov		[edx+edi+0 + 1*8],eax
		mov		[edx+edi+4 + 1*8],esi
		mov		eax,[ebx + edi + 0 + 2*8]
		mov		esi,[ebx + edi + 4 + 2*8]
		mov		[edx+edi+0 + 2*8],eax
		mov		[edx+edi+4 + 2*8],esi
		mov		eax,[ebx + edi + 0 + 3*8]
		mov		esi,[ebx + edi + 4 + 3*8]
		mov		[edx+edi+0 + 3*8],eax
		mov		[edx+edi+4 + 3*8],esi
		sub		edi,32
		jge		loopMisAligned
		
		mov		edi,ecx
		and		edi,~31
		add		ebx,edi					// increase src pointer
		add		edx,edi					// increase dst pointer
		and		ecx,31					// new count
		jz		outta					// if count = 0, get outta here

padding:
		cmp		ecx,16
		jl		skip16
		mov		eax,dword ptr [ebx]
		mov		dword ptr [edx],eax
		mov		eax,dword ptr [ebx+4]
		mov		dword ptr [edx+4],eax
		mov		eax,dword ptr [ebx+8]
		mov		dword ptr [edx+8],eax
		mov		eax,dword ptr [ebx+12]
		mov		dword ptr [edx+12],eax
		sub		ecx,16
		add		ebx,16
		add		edx,16
skip16:
		cmp		ecx,8
		jl		skip8
		mov		eax,dword ptr [ebx]
		mov		dword ptr [edx],eax
		mov		eax,dword ptr [ebx+4]
		sub		ecx,8
		mov		dword ptr [edx+4],eax
		add		ebx,8
		add		edx,8
skip8:
		cmp		ecx,4
		jl		skip4
		mov		eax,dword ptr [ebx]	// here 4-7 bytes
		add		ebx,4
		sub		ecx,4
		mov		dword ptr [edx],eax
		add		edx,4
skip4:							// 0-3 remaining bytes
		cmp		ecx,2
		jl		skip2
		mov		ax,word ptr [ebx]	// two bytes
		cmp		ecx,3				// less than 3?
		mov		word ptr [edx],ax
		jl		outta
		mov		al,byte ptr [ebx+2]	// last byte
		mov		byte ptr [edx+2],al
		jmp		outta
skip2:
		cmp		ecx,1
		jl		outta
		mov		al,byte ptr [ebx]
		mov		byte ptr [edx],al
outta:
		pop		esi
		pop		edi
	}
}

