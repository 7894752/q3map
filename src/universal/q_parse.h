/*
===========================================================================
q3map (Call of Duty, 2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

#ifndef Q3MAP_Q_PARSE_H
#define Q3MAP_Q_PARSE_H

/* CoD cursor-based parser; independent of common/scriplib's global token. */
char *Com_ParseOnLine( char **data_p );
void Com_MatchToken( char **data_p, const char *match, int warn );
void Com_Parse1DMatrix( char **data_p, int x, float *m );
void Com_Parse2DMatrix( char **data_p, int y, int x, float *m );

#endif
