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

// This file is compiled as C++; everything in it has C linkage.
extern "C" {
#include "../common/cmdlib.h"
#include "q_parse.h"

typedef struct parseInfo_t
{
	char token[1024];
	int currentLine;
	qboolean ungetReady;
	qboolean spaceDelimited;
	qboolean csv;
	qboolean parseNegativeNumbers;
	int ungetLineSave;
	char *ungetTokenSave;
	char filename[64];
} parseInfo_t;

typedef struct parseMark_t
{
	int currentLine;
	char *data;
	qboolean ungetReady;
	int ungetLineSave;
	char *ungetTokenSave;
} parseMark_t;

enum
{
	ERR_FATAL = 0,
	ERR_DROP = 1
};

void Com_Error( int code, const char *fmt, ... );
void Com_Printf( const char *fmt, ... );
void Q_strcat( char *dest, int size, const char *src );
void Q_strncpyz( char *dest, const char *src, int destsize );


#define MAX_TOKEN_CHARS 1024

static const char *com_parseSeparators[] = {
	"+=", "-=", "*=", "/=", "&=", "|=",
	"++", "--", "&&", "||", "<=", ">=", "==", "!=",
	NULL
};

static parseInfo_t parseInfo_stack[16] = {
	{ "", 1, qfalse, qtrue, qfalse, qtrue, 1, NULL, "" }
};
static parseInfo_t *parseInfo = parseInfo_stack;
static int parseInfo_level;
static char com_lineBuffer[MAX_TOKEN_CHARS] = {0};
static char *prevTokenPos = NULL;
static char *lastTokenPos = NULL;



/*
===================
Com_BeginParseSession
===================
*/
void Com_BeginParseSession( const char *name ) {
	if ( parseInfo_level == 15 ) {
		Com_Error( ERR_FATAL, "\x15" "Com_BeginParseSession: session overflow" );
	}
	parseInfo_level++;
	parseInfo = &parseInfo_stack[parseInfo_level];
	parseInfo->currentLine = 1;
	parseInfo->ungetReady = qfalse;
	parseInfo->spaceDelimited = qtrue;
	parseInfo->csv = qfalse;
	Q_strncpyz( parseInfo->filename, name, sizeof( parseInfo->filename ) );
}

/*
===================
Com_EndParseSession
===================
*/
void Com_EndParseSession( void ) {
	if ( !parseInfo_level ) {
		Com_Error( ERR_FATAL, "\x15" "Com_EndParseSession: session underflow" );
	}
	parseInfo_level--;
	parseInfo = &parseInfo_stack[parseInfo_level];
}

/*
=============
Com_ResetParseSessions
=============
*/
void Com_ResetParseSessions( void ) {
	parseInfo_level = 0;
	parseInfo = parseInfo_stack;
}

/*
=============
Com_SetSpaceDelimited
=============
*/
void Com_SetSpaceDelimited( qboolean enabled ) {
	parseInfo->spaceDelimited = enabled;
}

/*
=============
Com_SetCSV
=============
*/
void Com_SetCSV( qboolean enabled ) {
	parseInfo->csv = enabled;
}

/*
=============
Com_SetParseNegativeNumbers
=============
*/
void Com_SetParseNegativeNumbers( qboolean enabled ) {
	parseInfo->parseNegativeNumbers = enabled;
}

/*
===================
Com_GetCurrentParseLine
===================
*/
int Com_GetCurrentParseLine( void ) {
	return parseInfo->currentLine;
}

/*
===================
Com_ScriptError

Prints the script name and line number in the message
===================
*/
void Com_ScriptError( const char *fmt, ... ) {
	va_list argptr;
	char string[32000];

	va_start( argptr, fmt );
	vsprintf( string, fmt, argptr );
	va_end( argptr );

	Com_Error( ERR_DROP, "\x15" "File %s, line %i: %s", parseInfo->filename, parseInfo->currentLine, string );
}

/*
=============
Com_ScriptWarning
=============
*/
void Com_ScriptWarning( const char *fmt, ... ) {
	va_list argptr;
	char string[32000];

	va_start( argptr, fmt );
	vsprintf( string, fmt, argptr );
	va_end( argptr );

	Com_Printf( "File %s, line %i: %s", parseInfo->filename, parseInfo->currentLine, string );
}

/*
===================
Com_UngetToken

Calling this will make the next Com_Parse return
the current token instead of advancing the pointer
===================
*/
void Com_UngetToken( void ) {
	if ( parseInfo->ungetReady ) {
		Com_ScriptError( "UngetToken called twice" );
	}
	parseInfo->ungetReady = qtrue;
	lastTokenPos = prevTokenPos;
}

/*
=============
Com_ParseSetMark
=============
*/
void Com_ParseSetMark( char **data_p, parseMark_t *mark ) {
	mark->currentLine = parseInfo->currentLine;
	mark->data = *data_p;
	mark->ungetReady = parseInfo->ungetReady;
	mark->ungetLineSave = parseInfo->ungetLineSave;
	mark->ungetTokenSave = parseInfo->ungetTokenSave;
}

/*
=============
Com_ParseReturnToMark
=============
*/
void Com_ParseReturnToMark( char **data_p, const parseMark_t *mark ) {
	parseInfo->currentLine = mark->currentLine;
	*data_p = mark->data;
	parseInfo->ungetReady = mark->ungetReady;
	parseInfo->ungetLineSave = mark->ungetLineSave;
	parseInfo->ungetTokenSave = mark->ungetTokenSave;
}

/*
==============
SkipWhitespace

Parse a token out of a string
Will never return NULL, just empty strings

If "allowLineBreaks" is qtrue then an empty
string will be returned if the next token is
a newline.
==============
*/
static char *SkipWhitespace( char *data, qboolean *hasNewLines ) {
	int c;
	parseInfo_t *pi;

	c = *data;
	if ( c > ' ' ) {
		return data;
	}
	pi = parseInfo;
	do {
		if ( !c ) {
			return NULL;
		}
		if ( c == '\n' ) {
			pi->currentLine++;
			*hasNewLines = qtrue;
		}
		c = *++data;
	} while ( c <= ' ' );
	return data;
}

/*
=============
Com_Compress
=============
*/
int Com_Compress( char *data_p ) {
	char    *datai, *datao;
	char c;
	int size;

	size = 0;
	datao = datai = data_p;
	if ( datai ) {
		while ( ( c = *datai ) != 0 ) {
			if ( c == '\r' || c == '\n' ) {
				*datao++ = c;
				datai++;
				size++;
			} else if ( c == '/' && datai[1] == '/' ) {
				while ( *datai && *datai != '\n' ) {
					datai++;
				}
			}
			else if ( c == '/' && datai[1] == '*' ) {
				if ( *datai ) {
					while ( *datai && ( *datai != '*' || datai[1] != '/' ) ) {
						datai++;
					}
					if ( *datai ) {
						datai += 2;
					}
				}
			} else {
				*datao++ = c;
				datai++;
				size++;
			}
		}
	}
	*datao = 0;
	return size;
}

/*
=============
Com_GetLastTokenPos
=============
*/
char *Com_GetLastTokenPos( void ) {
	return lastTokenPos;
}

/*
=============
Com_ParseCSV
=============
*/
static char *Com_ParseCSV( char **data_p, qboolean allowLineBreaks ) {
	parseInfo_t *pi;
	char *data;
	int len;
	char c;

	data = *data_p;
	len = 0;
	pi = parseInfo;
	pi->token[0] = 0;

	if ( !allowLineBreaks ) {
		c = *data;
		if ( c == '\r' || c == '\n' ) {
			return pi->token;
		}
	} else {
		while ( ( c = *data ) == '\r' || c == '\n' ) {
			data++;
		}
	}

	prevTokenPos = lastTokenPos;
	lastTokenPos = data;

	while ( *data && *data != ',' && *data != '\n' ) {
		c = *data;
		if ( c == '\r' ) {
			data++;
		} else if ( c != '"' ) {
			if ( len < MAX_TOKEN_CHARS - 1 ) {
				pi->token[len] = c;
				len++;
			}
			data++;
		} else {
			data++;
			for ( ;; ) {
				c = *data;
				if ( c == '"' ) {
					if ( data[1] != '"' ) {
						data++;
						break;
					}
					if ( len < MAX_TOKEN_CHARS - 1 ) {
						pi->token[len] = '"';
						len++;
					}
					data += 2;
				} else {
					if ( len < MAX_TOKEN_CHARS - 1 ) {
						pi->token[len] = c;
						len++;
					}
					data++;
				}
			}
		}
	}

	if ( *data ) {
		if ( *data != '\n' ) {
			data++;
		}
		*data_p = data;
	} else {
		*data_p = NULL;
	}

	pi->token[len] = 0;
	return pi->token;
}

/*
==============
Com_ParseExt

Parse a token out of a string
Will never return NULL, just empty strings.
An empty string will only be returned at end of file.

If "allowLineBreaks" is qtrue then an empty
string will be returned if the next token is
a newline.
==============
*/
char *Com_ParseExt( char **data_p, qboolean allowLineBreaks ) {
	char c;
	int len;
	qboolean hasNewLines = qfalse;
	char    *data;

	if ( !data_p ) {
		Com_Error( ERR_FATAL, "\x15" "Com_ParseExt: NULL data_p" );
	}
	data = *data_p;
	len = 0;
	parseInfo->token[0] = 0;

	if ( !data ) {
		*data_p = NULL;
		return parseInfo->token;
	}

	parseInfo->ungetLineSave = parseInfo->currentLine;
	parseInfo->ungetTokenSave = *data_p;

	if ( parseInfo->csv ) {
		return Com_ParseCSV( data_p, allowLineBreaks );
	}

	while ( 1 ) {
		data = SkipWhitespace( data, &hasNewLines );
		if ( !data ) {
			*data_p = NULL;
			return parseInfo->token;
		}
		if ( hasNewLines && !allowLineBreaks ) {
			*data_p = data;
			return parseInfo->token;
		}
		c = *data;
		if ( c == '/' && data[1] == '/' ) {
			while ( *data && *data != '\n' ) {
				data++;
			}
			continue;
		}
		if ( c == '/' && data[1] == '*' ) {
			while ( *data && ( *data != '*' || data[1] != '/' ) ) {
				if ( *data == '\n' ) {
					parseInfo->currentLine++;
				}
				data++;
			}
			if ( *data ) {
				data += 2;
			}
			continue;
		}
		break;
	}

	prevTokenPos = lastTokenPos;
	lastTokenPos = data;

	if ( c == '\"' ) {
		data++;
		while ( 1 )
		{
			c = *data++;
			if ( c == '\\' && *data == '\"' ) {
				c = '\"';
				data++;
			} else if ( c == '\"' || !c ) {
				break;
			} else if ( *data == '\n' ) {
				parseInfo->currentLine++;
			}
			if ( len < MAX_TOKEN_CHARS - 1 ) {
				parseInfo->token[len] = c;
				len++;
			}
		}
		parseInfo->token[len] = 0;
		*data_p = data;
		return parseInfo->token;
	}

	if ( parseInfo->spaceDelimited ) {
		do
		{
			if ( len < MAX_TOKEN_CHARS - 1 ) {
				parseInfo->token[len] = c;
				len++;
			}
			data++;
			c = *data;
		} while ( c > ' ' );

		if ( len == MAX_TOKEN_CHARS ) {
			len = 0;
		}
		parseInfo->token[len] = 0;
		*data_p = data;
		return parseInfo->token;
	}

	if ( ( c >= '0' && c <= '9' ) ||
		 ( parseInfo->parseNegativeNumbers && c == '-' && data[1] >= '0' && data[1] <= '9' ) ||
		 ( c == '.' && data[1] >= '0' && data[1] <= '9' ) ) {
		do {
			if ( len < MAX_TOKEN_CHARS - 1 ) {
				parseInfo->token[len] = c;
				len++;
			}
			data++;
			c = *data;
		} while ( ( c >= '0' && c <= '9' ) || c == '.' );

		if ( c == 'e' || c == 'E' ) {
			if ( len < MAX_TOKEN_CHARS - 1 ) {
				parseInfo->token[len] = c;
				len++;
			}
			data++;
			c = *data;
			if ( c == '-' || c == '+' ) {
				if ( len < MAX_TOKEN_CHARS - 1 ) {
					parseInfo->token[len] = c;
					len++;
				}
				data++;
				c = *data;
			}
			do {
				if ( len < MAX_TOKEN_CHARS - 1 ) {
					parseInfo->token[len] = c;
					len++;
				}
				data++;
				c = *data;
			} while ( c >= '0' && c <= '9' );
		}
	} else if ( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) ||
				c == '_' || c == '/' || c == '\\' ) {
		do {
			if ( len < MAX_TOKEN_CHARS - 1 ) {
				parseInfo->token[len] = c;
				len++;
			}
			data++;
			c = *data;
		} while ( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) ||
				  c == '_' || ( c >= '0' && c <= '9' ) ||
				  c == '/' || c == '\\' || c == ':' || c == '.' );
	} else {
		const char **sep = com_parseSeparators;
		if ( *sep ) {
			do {
				int seplen = strlen( *sep );
				int i;
				for ( i = 0; i < seplen; i++ ) {
					if ( data[i] != ( *sep )[i] ) {
						break;
					}
				}
				if ( i == seplen ) {
					memcpy( parseInfo->token, *sep, seplen );
					parseInfo->token[seplen] = 0;
					*data_p = data + seplen;
					return parseInfo->token;
				}
				sep++;
			} while ( *sep );
		}
		parseInfo->token[0] = *data;
		parseInfo->token[1] = 0;
		*data_p = data + 1;
		return parseInfo->token;
	}

	if ( len == MAX_TOKEN_CHARS ) {
		len = 0;
	}
	parseInfo->token[len] = 0;
	*data_p = data;
	return parseInfo->token;
}

/*
===================
Com_Parse
===================
*/
char *Com_Parse( char **data_p ) {
	if ( parseInfo->ungetReady ) {
		parseInfo->ungetReady = qfalse;
		*data_p = parseInfo->ungetTokenSave;
		parseInfo->currentLine = parseInfo->ungetLineSave;
	}
	return Com_ParseExt( data_p, qtrue );
}

/*
===================
Com_ParseOnLine
===================
*/
char *Com_ParseOnLine( char **data_p ) {
	if ( parseInfo->ungetReady ) {
		parseInfo->ungetReady = qfalse;
		if ( !parseInfo->spaceDelimited ) {
			return parseInfo->token;
		}
		*data_p = parseInfo->ungetTokenSave;
		parseInfo->currentLine = parseInfo->ungetLineSave;
	}
	return Com_ParseExt( data_p, qfalse );
}

/*
==================
Com_MatchToken
==================
*/
void Com_MatchToken( char **data_p, const char *match, int warn ) {
	char    *token;

	token = Com_Parse( data_p );

	if ( strcmp( token, match ) ) {
		if ( warn ) {
			Com_ScriptWarning( "MatchToken: %s != %s", token, match );
		} else {
			Com_ScriptError( "MatchToken: %s != %s", token, match );
		}
	}
}

/*
=================
Com_SkipBracedSection

The next token should be an open brace.
Skips until a matching close brace is found.
Internal brace depths are properly skipped.
=================
*/
int Com_SkipBracedSection( char **data_p, int depth ) {
	char    *token;
	int braceLevel;
	qboolean found;

	found = qfalse;
	braceLevel = 0;
	do {
		token = Com_Parse( data_p );
		if ( token[1] == 0 ) {
			if ( token[0] == '{' ) {
				if ( braceLevel == depth ) {
					found = qtrue;
				} else {
					braceLevel++;
				}
			} else if ( token[0] == '}' ) {
				braceLevel--;
			}
		}
	} while ( braceLevel && *data_p );
	return found;
}

/*
=================
Com_SkipRestOfLine
=================
*/
void Com_SkipRestOfLine( char **data_p ) {
	char    *p;
	int c;

	p = *data_p;
	if ( !p ) {
		return;
	}
	while ( ( c = *p ) != 0 ) {
		p++;
		if ( c == '\n' ) {
			parseInfo->currentLine++;
			break;
		}
	}
	*data_p = p;
}

/*
====================
Com_ParseRestOfLine
====================
*/
char *Com_ParseRestOfLine( char **data_p ) {
	char    *token;

	com_lineBuffer[0] = 0;
	while ( 1 ) {
		token = Com_ParseOnLine( data_p );
		if ( !token[0] ) {
			return com_lineBuffer;
		}

		if ( com_lineBuffer[0] ) {
			Q_strcat( com_lineBuffer, sizeof( com_lineBuffer ), " " );
		}
		Q_strcat( com_lineBuffer, sizeof( com_lineBuffer ), token );
	}
}

/*
=============
Com_ParseFloat
=============
*/
float Com_ParseFloat( char **data_p ) {
	char    *token;

	token = Com_Parse( data_p );
	if ( !*token ) {
		return 0;
	}
	return atof( token );
}

/*
=============
Com_ParseInt
=============
*/
int Com_ParseInt( char **data_p ) {
	char    *token;

	token = Com_Parse( data_p );
	if ( !*token ) {
		return 0;
	}
	return atoi( token );
}

/*
=============
Com_Parse1DMatrix
=============
*/
void Com_Parse1DMatrix( char **data_p, int x, float *m ) {
	int i;

	Com_MatchToken( data_p, "(", 0 );
	for ( i = 0; i < x; i++ ) {
		m[i] = atof( Com_Parse( data_p ) );
	}
	Com_MatchToken( data_p, ")", 0 );
}

/*
=============
Com_Parse2DMatrix
=============
*/
void Com_Parse2DMatrix( char **data_p, int y, int x, float *m ) {
	int i;

	Com_MatchToken( data_p, "(", 0 );

	for ( i = 0 ; i < y ; i++ ) {
		Com_Parse1DMatrix( data_p, x, m + i * x );
	}

	Com_MatchToken( data_p, ")", 0 );
}

/*
=============
Com_Parse3DMatrix
=============
*/
void Com_Parse3DMatrix( char **data_p, int z, int y, int x, float *m ) {
	int i;

	Com_MatchToken( data_p, "(", 0 );

	for ( i = 0 ; i < z ; i++ ) {
		Com_Parse2DMatrix( data_p, y, x, m + i * x * y );
	}

	Com_MatchToken( data_p, ")", 0 );
}
}
