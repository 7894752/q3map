# Third-party notices

The reconstruction as a whole is distributed under `COPYING` (GPL-3.0-or-later with id Software's additional terms).
The components below are included under their own terms.

## id Software (Quake III Arena, Return to Castle Wolfenstein, Wolfenstein: Enemy Territory)

Most of `src/q3map`, `src/common`, `src/code/qcommon`, `src/code/game`, `src/universal` and `src/libs/cmdlib` descends from
code published by id Software. Those files keep their original headers ("Copyright (C) 1999-2005 Id Software, Inc.",
GNU GPL version 2 or, at your option, any later version). The id headers must not be removed. A small number of
functions are close to the RTCW/ET versions (for example `FS_FileCompare`, `AxisToAngles`, `Q_isalphanumeric`,
`Q_isnumeric`, `FS_Delete` and the critical-section wrappers); the additional terms in `COPYING` apply to the work
for that reason.

## Independent JPEG Group, libjpeg 6 (`src/code/jpeg-6`, `src/libs/jpeg6`)

Copyright (C) 1991-1998, Thomas G. Lane. The IJG license is in `src/code/jpeg-6/README` ("LEGAL ISSUES") and must be kept
with redistributions. This software is based in part on the work of the Independent JPEG Group.

## zlib 1.1.4 (`src/libs/zlib`)

Copyright (C) 1995-2002 Jean-loup Gailly and Mark Adler. The zlib license (verbatim from `zlib.h`):

> This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held
> liable for any damages arising from the use of this software.
>
> Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter
> it and redistribute it freely, subject to the following restrictions:
>
> 1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software.
>    If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not
>    required.
> 2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
> 3. This notice may not be removed or altered from any source distribution.

The only alteration is in `zutil.c`: `zcalloc` and `zcfree` call the engine's memory allocator (see `src/libs/zlib/README.md`).

## minizip / unzip 0.15 (`src/code/qcommon/unzip.c`, `unzip_layout.h`)

"unzip 0.15 Copyright 1998 Gilles Vollant". The code was published by id Software inside the Quake III Arena GPL release,
together with the notice string kept in `unzip.c`. minizip 0.15 carries no separate license text.

## NvTriStrip (`src/libs/nvtristrip`)

NvTriStrip is a triangle stripifier published by NVIDIA (authors named in NVIDIA's distribution: Curtis Beeson, Joe Demers,
Jason Regier, Jon Stone). It was published freely, but none of its files, and no README of the library, contains a
license text. The sources in this repository are a reconstruction of an older revision of the library (not the version on
GitHub today), kept with their original attribution because the byte-exact result depends on that revision. If NVIDIA or
the rights holder objects, these five files will be removed; the build would then need a user-supplied copy of the library.

## Not included

No Microsoft software (compiler, linker, libraries, Platform SDK, `GlAux.Lib`) and no part of the original `q3map.exe`
is distributed here. `build/meta_stamp.json` records three header values of the original (timestamp, PDB GUID, PDB age),
which are constants, not code.
