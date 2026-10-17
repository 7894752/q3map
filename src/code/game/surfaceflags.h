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
// This file must be identical in the quake and utils directories

// contents flags are seperate bits
// a given brush can contribute multiple content bits

// these definitions also need to be in q_shared.h!

#define	CONTENTS_SOLID			1		// an eye is never valid in a solid
#define	CONTENTS_LAVA			8
#define	CONTENTS_SLIME			16
#define	CONTENTS_WATER			32
#define	CONTENTS_FOG			64

#define CONTENTS_NOTTEAM1		0x0080
#define CONTENTS_NOTTEAM2		0x0100
#define CONTENTS_NOBOTCLIP		0x0200

// Call of Duty contents; these overlap unused Quake III flags.
// Names follow the surfaceparm table of q3map.exe (infoParms) and, where the
// bit matches, the later IW engine names (CONTENTS_NONCOLLIDING, CONTENTS_ITEMCLIP, ...).
#define CONTENTS_FOLIAGE		0x0002
#define CONTENTS_NONCOLLIDING	0x0004
#define CONTENTS_GLASS			0x0010
#define CONTENTS_CANSHOOTCLIP	0x0040
#define CONTENTS_MISSILECLIP		0x0080
#define CONTENTS_VEHICLECLIP		0x0200
#define CONTENTS_ITEMCLIP		0x0400
#define CONTENTS_SKY			0x0800
#define CONTENTS_AI_NOSIGHT		0x1000
#define CONTENTS_CLIPSHOT			0x2000

#define	CONTENTS_AREAPORTAL		0x8000

#define	CONTENTS_PLAYERCLIP		0x10000
#define	CONTENTS_MONSTERCLIP	0x20000
//bot specific contents types
#define	CONTENTS_TELEPORTER		0x40000
#define	CONTENTS_JUMPPAD		0x80000
#define CONTENTS_CLUSTERPORTAL	0x100000
#define CONTENTS_DONOTENTER		0x200000
#define CONTENTS_BOTCLIP		0x400000
#define CONTENTS_MOVER			0x800000

#define	CONTENTS_ORIGIN			0x1000000	// removed before bsping an entity

#define	CONTENTS_BODY			0x2000000	// should never be on a brush, only in game
#define	CONTENTS_CORPSE			0x4000000
#define	CONTENTS_DETAIL			0x8000000	// brushes not used for the bsp
#define	CONTENTS_STRUCTURAL		0x10000000	// brushes used for the bsp
#define	CONTENTS_TRANSLUCENT	0x20000000	// don't consume surface fragments inside
#define	CONTENTS_TRIGGER		0x40000000
#define	CONTENTS_NODROP			0x80000000	// don't leave bodies or items (death fog, lava)

#define	SURF_NODAMAGE			0x1		// never give falling damage
#define	SURF_SLICK				0x2		// effects game physics
#define	SURF_SKY				0x4		// lighting from environment map
#define	SURF_LADDER				0x8
#define	SURF_NOIMPACT			0x10	// don't make missile explosions
#define	SURF_NOMARKS			0x20	// don't leave missile marks
#define	SURF_FLESH				0x40	// make flesh sounds and effects
#define	SURF_NODRAW				0x80	// don't generate a drawsurface at all
#define	SURF_HINT				0x100	// make a primary bsp splitter
#define	SURF_SKIP				0x200	// completely ignore, allowing non-closed brushes
#define	SURF_NOLIGHTMAP			0x400	// surface doesn't need a lightmap
#define	SURF_POINTLIGHT			0x800	// generate lighting info at vertexes
#define	SURF_METALSTEPS			0x1000	// clanking footsteps
#define	SURF_NOSTEPS			0x2000	// no footstep sounds
#define	SURF_NONSOLID			0x4000	// don't collide against curves with this set
#define	SURF_LIGHTFILTER		0x8000	// act as a light filter during q3map -light
#define	SURF_ALPHASHADOW		0x10000	// do per-pixel light shadow casting in q3map
#define	SURF_NODLIGHT			0x20000	// don't dlight even if solid (solid lava, skies)
#define SURF_DUST				0x40000 // leave a dust trail when walking on this surface
#define SURF_CASTSHADOW			0x40000	// "castshadow" in the surfaceparm table (same bit as SURF_DUST)
#define SURF_PORTAL				0x80000000

// material type: a 5 bit index stored in the surface flags
#define SURF_TYPE_SHIFT			20
#define SURF_TYPE_MASK			0x01F00000
#define SURF_TYPE_BARK			( 1 << SURF_TYPE_SHIFT )
#define SURF_TYPE_BRICK			( 2 << SURF_TYPE_SHIFT )
#define SURF_TYPE_CARPET		( 3 << SURF_TYPE_SHIFT )
#define SURF_TYPE_CLOTH			( 4 << SURF_TYPE_SHIFT )
#define SURF_TYPE_CONCRETE		( 5 << SURF_TYPE_SHIFT )
#define SURF_TYPE_DIRT			( 6 << SURF_TYPE_SHIFT )
#define SURF_TYPE_FLESH			( 7 << SURF_TYPE_SHIFT )
#define SURF_TYPE_FOLIAGE		( 8 << SURF_TYPE_SHIFT )
#define SURF_TYPE_GLASS			( 9 << SURF_TYPE_SHIFT )
#define SURF_TYPE_GRASS			( 10 << SURF_TYPE_SHIFT )
#define SURF_TYPE_GRAVEL		( 11 << SURF_TYPE_SHIFT )
#define SURF_TYPE_ICE			( 12 << SURF_TYPE_SHIFT )
#define SURF_TYPE_METAL			( 13 << SURF_TYPE_SHIFT )
#define SURF_TYPE_MUD			( 14 << SURF_TYPE_SHIFT )
#define SURF_TYPE_PAPER			( 15 << SURF_TYPE_SHIFT )
#define SURF_TYPE_PLASTER		( 16 << SURF_TYPE_SHIFT )
#define SURF_TYPE_ROCK			( 17 << SURF_TYPE_SHIFT )
#define SURF_TYPE_SAND			( 18 << SURF_TYPE_SHIFT )
#define SURF_TYPE_SNOW			( 19 << SURF_TYPE_SHIFT )
#define SURF_TYPE_WATER			( 20 << SURF_TYPE_SHIFT )
#define SURF_TYPE_WOOD			( 21 << SURF_TYPE_SHIFT )
#define SURF_TYPE_ASPHALT		( 22 << SURF_TYPE_SHIFT )
#define SURF_TYPE_OPAQUEGLASS	SURF_TYPE_GLASS
