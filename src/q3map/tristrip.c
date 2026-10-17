/*
===========================================================================
q3map (Call of Duty, 2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

#include <stdlib.h>

#include "tristrip.h"

/*
=============
GenerateTriangleListGroups
=============
*/
int GenerateTriangleListGroups( const unsigned short *indices,
	unsigned short numIndices, triangleIndexGroup_t **groups,
	unsigned short *remap, unsigned short numVerts ) {
	unsigned short numGroups;
	int i;

	numGroups = numIndices / 3;
	*groups = malloc( numGroups * sizeof( **groups ) );
	for ( i = 0; i < numGroups; i++ ) {
		(*groups)[i].numIndices = 3;
		(*groups)[i].indices = malloc( (*groups)[i].numIndices * sizeof( *indices ) );
		(*groups)[i].indices[0] = *indices++;
		(*groups)[i].indices[1] = *indices++;
		(*groups)[i].indices[2] = *indices++;
	}
	for ( i = 0; i < numVerts; i++ ) {
		remap[i] = i;
	}
	return numGroups;
}
