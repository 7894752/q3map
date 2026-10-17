/*
===========================================================================
q3map (Call of Duty, 2003) - source reconstruction.

Reconstructed to compile to the original binary. Distributed under the terms in
COPYING (GPL version 3 or later, with id Software's additional terms).
===========================================================================
*/

#ifndef Q3MAP_PLANE_REMAP_H
#define Q3MAP_PLANE_REMAP_H

/* Output plane indices after used-plane compaction. */
void RemapNodePlanes( const int *planeMap );
void RemapBrushPlanes( const int *planeMap );
void RemapPortalPlanes( const int *planeMap );
void RemapOccluderPlanes( const int *planeMap );

#endif
