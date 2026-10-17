# Reconstruction notes: where the source is a choice, not a proof

This repository is a reconstruction. Every one of the 970 project functions compiles, with the original compiler and flags, to the same bytes and
relocations as the original `q3map.exe`; that is proven by the build, not by this text. What the build cannot prove is *which source text* the
authors wrote. This page lists the places where the source reflects a choice. Evidence levels:

- **Proven** - the compiled image (or the original's symbol/section layout) demands it; any other spelling would give a different binary.
- **Strong** - independent evidence from the same engine family (published Quake III / RTCW / ET sources, debug symbols of other retail builds of the
  same game) agrees with the image.
- **Plausible** - the image allows several readings; this one was picked on style or lineage.

Whenever several spellings compile to identical bytes, the choice among them is never evidence about the original.

## 1. `universal/q_parse` is C++ (Plausible)
The original's Rich header records 42 C++ objects; our build has 41 without this. Ten translation units give a byte-identical image whether compiled as C
or C++: `leakfile`, `mutex`, `q_parse`, `tree`, `tristrip`, `writebsp`, `com_memory`, `mesh`, `patch`, `win_shared` (all other project units are provably C:
C++ would move their uninitialised globals out of COMMON, split their statics, or drop unreferenced constants). `q_parse` was chosen because it is
engine-side code written for this game and compiles unchanged. The whole file is wrapped in `extern "C"` (two lines) so every function and variable keeps its C name: the other units are C and call these functions through plain C declarations, and a C++-mangled name would not link; the original may instead have used `extern "C"` in its headers; the image cannot tell the two apart. Any of the ten
would produce the same image and the same Rich counts.

## 2. Global names chosen to fit the data layout (Proven order, guessed spelling)
The compiler orders uninitialised globals by a hash of their *names*; to reproduce the original `.bss`/COMMON order some names had to be picked
so that they land in the right slot. Names confirmed by lineage (Q3 or the game's symbol tables) are not listed here. The following have **no lineage evidence**;
they are names that happen to fit, and any other name with the same hash slot would give the same binary:
`dportalVerts`, `daabbtrees`, `numaabbtreenodes`, `currentEntNum`, `g_aabbSortMax`/`g_aabbSortMin`, `dEntstring`, `occluderPlane`, `nopassagevis`,
`cell_portals`, `fs_loadingModeNum`, the lightmap grid/material names (`currentMaterialSet`, `materialGroupSortArray`, `lightmapGridMins`,
`lightmapCellsGroup`, `bspLightmapGridHeight`, `numSortShader`), and the function-local statics `buffer`/`vabuf` (`va`) and `buf` (`Info_ValueForKey`).
(Evidence level: the *position* is Proven, the *spelling* is a guess.)

## 3. Placeholder names (Proven value, invented name)
- `lightmapBorder` (`const int`, value 2): an unreferenced read-only constant sits in the original's `.rdata`; only its value and position are real.
- `TJunc_Unused0`, `TJunc_Unused1` (two empty functions before the t-junction code, never called), `Hunk_Trash` (empty in the original, as in the published
  Quake III / RTCW sources where it starts with `return;`): the functions exist in the original; `Hunk_Trash` is an original name (Strong),
  the two `TJunc_Unused*` names are descriptive guesses.

## 4. Function names without a source (Plausible)
Most function names are original (studio releases, or symbol tables of other retail builds of the game). A few are descriptive guesses because no source
names them: `Perp2D`, `NormalFromPoints`, `RoundToPrecision`, `GuessMeshPlane`, `FreeBspTree` (it frees nothing), the polylib `...Double` family
(`AllocWindingDouble`, `BaseWindingForPlaneDouble`, ...), `CheckWindingPlane`, `PointsNearLine`, `WindingPlaneAverage`, and the CoD-specific q3map
helpers in `tris`, `lightmaps`, `patch`, `portals`, `surface`, `terrain` (their names describe what the code does).

## 5. Definition order and explicit zero initialisers (Proven order, neutral spelling)
Several files define globals in an unusual order or with `= 0`/`= NULL` (`com_memory`, `files`, `tris`, `lightmaps`, `terrain`, `q_shared`,
`q_parse`). The image proves the data order; the spelling (`= 0`, one block of definitions) is only a way to get that order from this compiler and carries no
meaning for the original source.

## 6. Quirks preserved from the original code
Shipped game-tool code contains leftovers: disabled debug blocks, extra temporaries, double-precision intermediates, small helper structs, tests whose body was
removed. They are reproduced here as the binary requires, and they are plausible remnants of development. For each, the *bytes* are proven; the exact spelling
is a reconstruction (other spellings that compile to the same bytes may exist, and where the natural spelling was tried and differs, that is noted).

### 6.1 Leftover code and extra temporaries
- `CollapseCollinearVerts` (`tris`): a disabled debug block, `if ( TRIS_DEBUG_COLLAPSE ) { memmove( &anchorIdx, &endpointIdx, ... ); }` with the switch defined as 0. The
  call is never executed, but its presence makes the compiler keep two loop counters in memory (Proven that *something* of this effect exists; the spelling and the
  switch name are a guess).
- `DeriveTextureVectors` (`tris`) and `AllocateLightmapForSurface` / `AllocateLightmapForPatch` (`lightmaps`): extra temporaries and small helper structs whose roles
  (what is a copy, what is reused) were found by search; Proven to compile identically, other equivalent spellings may exist. The same holds for
  `CollectCollapseTriangles` and `UnparseEntitiesWithOrigins` (a dead store `originCopy`).
- `visflow` `BasePortalVis`: the first loop reads the portal and point through block-scope `const` pointer copies; the plain Q3 `DotProduct` form gives a different
  term order. The second loop of the same function is plain Q3 code.
- `patch` `CenterPatchLightmapCoords`: the y centre goes through a `double` accumulator, the x centre through `maxs[0] += mins[0]`; no symmetric spelling reproduces the
  original's x87 register assignment (maxs[0] stays in a register for the whole loop).
- Leftover tests and results: `portals` `EmitOccluders` calls `BoxOnPlaneSide` and discards the result (`if ( ... ) { }`; the call is naked asm and stays);
  `writebsp` `EmitShader` has `if ( strstr( shader, "caulk" ) ) { }`, a test whose body was removed (the call stays); `patch` `EmitTerrainGroup` never uses the result of its
  first `AllocTerrainBuffer`; `portals` `EmitOccluders` leaves its inner search loop with `goto vertexFound` (break-and-test spellings change the code at the same place).
- Initialisers and index tricks: `previous = 0` (`tris`, a counter initialised before a loop that sets it), the XOR swap of two indices (`tris`), `current[-1]` (`tris`,
  a pointer used one element back), `ProcessCoronaLights` (`writebsp`), whose loop is written as `i + 1 < num_entities` over `&entities[1]`.

### 6.2 Double-precision intermediates
Several functions compute in `double` where a `float` would do, as C code written without care for the x87 does. The promotion is visible in the bytes (extra
store/reload or an operand order that only the wider type produces):
`TrianglePairCanShareEdge`, `EmitTriRecord`, `SmoothTriangleNormals` (`tris`), `SetPortalSphere` (`vis`: `double r` compared as `(float)r > bestr`; all float/double
combinations were tried, only this one is exact) and `ParseTerrain` (`terrain`: `surfheight` goes through a `float` and a `double` temporary while `surfwidth` is a
single expression).

### 6.3 Compiler-state and header constructs
- `MatchBrushSideToPortals_r` forward prototype in `portals.c` (Proven needed, role understood): the prototype before the first use changes how the compiler
  schedules `EmitCellPortals`, so removing or moving it changes that function's code. The call site of the plane-distance computation uses a local pointer
  rather than a helper function; that is plausible original code. (An earlier version used a separate inline helper, which also produced a standalone copy
  that the original does not have.) An equivalent spelling also compiles identically: `d = DotProduct( w->p[i], plane->normal ) - (double)plane->dist;`
  (the Q3 macro plus a cast on `dist`). The cast-free local-pointer form was chosen; the image cannot tell the two apart.
- `#include <utility>` at the top of `NvTriStrip.cpp`: the image demands that some standard header (bringing in `<iosfwd>`/`<xutility>` before `<vector>`) comes first,
  because it sets the order of the compiler-generated STL template functions. That *some* include came first is Proven; the exact header is a guess (`<iosfwd>` works
  equally well), and it may have arrived through another header or a forced include.
- `#pragma optimize("", on)` after `__fsqrt` in `com_math` (Q3's idiom for restoring the command-line options, as in Q3 `win_shared.c`): `__fsqrt` is compiled without
  `/Op` while the rest of the file has Q3's file-wide `#pragma optimize("p", on)`. Proven that `__fsqrt` was compiled without `/Op`; the exact pragma spelling is Plausible.
- `VectorRotateAngles` (`com_math`): the rotated components are written through two pointer locals `rj = r + pair[i][0]; rk = r + pair[i][1];` (plain `r[j]`
  indexing changes the addressing). The index table `pair` and the redundant parentheses around the `SinCos_double` argument are the remaining oddities; the
  exact spelling is a guess (Proven that the components are addressed through pointers).

### 6.4 Inline assembly
- `Com_Memset`, `Com_Memcpy`, `Com_Prefetch`, `_copyDWord`, `BoxOnPlaneSide` (naked), `SinCos`/`SinCos_double`/the quaternion sin-cos helpers - the
  same routines as in the published Quake III / RTCW sources: **Strong**.
- `FloorToInt`, `CeilToInt`, `RoundToInt` (biased `fistp` rounding): the instruction sequence is **Proven** by the binary; that it was inline assembly
  in a shared header is **Plausible**.

## 7. Build-structure inferences (Strong)
- Three static libraries (`jpeg6.lib`, `cmdlib.lib`, `nvtristrip.lib`) and their order on the link line: reproduces the original's CRT member order
  (213 of 213) with no special link options.
- `stricmp`/`strnicmp`/`itoa` (POSIX spellings) in the sources: the Rich header contains the three alias objects of `oldnames.lib`, which are only linked
  when those names are used.
- Debug info (`/Zi`): the original has a PDB path; `/Zi` also removes a 32-byte COMMON padding effect that matches the original sizes.
- The per-TU `/GS` split (some units compiled with the stack-cookie option, some without): visible in the code of each unit; the split follows
  groups of files that look like different original libraries.
- `/YX` for the two NvTriStrip files: reproduces the lazy emission order of header functions of the standard library (together with the leading `#include <utility>`, see above).
- jpeg-6 as C++ (19 objects): consistent with the Rich header counts and with how GtkRadiant ships the library.

## 8. The stamp step (Not reproducible)
The original's PE timestamp, PDB GUID and PDB age are build-time artefacts: the linker stamps "now", the GUID is random per link and the age counts PDB
updates. The optional `--identical` mode copies those three values from the original into a copy of the freshly linked image. Everything else in the
image comes from the compiler and linker. (The Rich header is produced by the linker itself from the object inventory; since the C++ object count is now
right, it matches without help.)

## 9. Invented names, by evidence group
Policy: a name is invented only when nothing better exists, and every invention is listed here. Groups, best evidence first. (Names that come from
the published Quake III / RTCW / ET sources or from symbol tables of other builds of the game are not listed.)

### 9.1 Derived from the executable's own data (Strong)
- Flag and contents names whose bits are proven by the original's surfaceparm table (`infoParms[]`, the parser's strings): `CONTENTS_FOLIAGE`,
  `CONTENTS_NONCOLLIDING`, `CONTENTS_GLASS`, `CONTENTS_CANSHOOTCLIP`, `CONTENTS_ITEMCLIP`, `CONTENTS_SKY`, `SURF_PORTAL` (the `portal` surfaceparm, 0x80000000;
  `portals` tests it on `side->surfaceFlags`) and the material family `SURF_TYPE_<material>` (bark, brick, carpet, ... asphalt).
- `MAX_MAP_OCCLUDERS`, `MAX_MAP_OCCLUDER_INDEXES`, `MAX_MAP_PATCHCOLLIDES`, `MAX_MAP_COLLISIONVERTS`, `MAX_MAP_DRAW_INDEXES`, `MAX_SURFACE_INFO`,
  `MAX_MAP_SHADERS`, `MAX_SURFACE_VERTS`: spelled exactly as the original's `Error()` strings; only the numeric values are inferred from the compares.
- struct fields named after the shader keywords or surfaceparms the parser reads: `lightmapSampleSize` (`q3map_lightmapsamplesize`), `isOccluder`
  (`occluder` surfaceparm), `patchSubdivisions` and `subdivisions` (`tesssize`), `twoSided` (`cull none`), `flap` (`deformVertexes flap`),
  `hasPasses`, `clampTexture` (stage `map clamp...`); the `infoParm_t` fields follow the table columns (name, clearSolid, surfaceFlags, contents).
- the entity keys `alphamap`, `layers`, `shader`, and the shader name `textures/common/terrain`: strings in the image.

### 9.2 Lineage-style names (Plausible)
- `qbsp.h` fields added by the CoD tool, named in the Q3 style after what the code does with them: `isTerrain`, `triangleSoup`, `terrainFlags`
  (bit 0 selects the quad diagonal), `cullgroup`, `noBspOutput`, `bspCell`, `collisionSurfReferences`, `portalIgnored`, `portalCellCount[2]`,
  `portalBrush`, `lightmapGroupNum`, `nextOnLightmapGroup`.
- The `SURF_` / `CONTENTS_` prefixes and `SURF_TYPE_SHIFT` / `SURF_TYPE_MASK`: spelling taken from KisakCOD, a community CoD4 reimplementation (synthetic, probably
  derived from the CoD4 symbols; being checked against the real symbol files). Only the bits are proven by the original.
- BSP record types used by `portals`/`writebsp`: `DiskGfxPortal`, `DiskGfxCell`, `DiskGfxAabbTree`, `DiskGfxCullGroup`, `DiskGfxOccluder` and their field
  names (`planeIndex`, `cellIndex`, `firstPortalVertex`, `portalVertexCount`, `firstSurface`, `surfaceCount`, `childCount`, `firstPlane`, `planeCount`, `edgeCount`,
  `firstEdge`, ...) come from the debug info of a CoD2 Mac build (a later game; the layout matches, the spelling is not proven for CoD1's tool). `lump_t{filelen, fileofs}`,
  `HEADER_LUMPS` (33) and `dheader_t{ident, version, lumps[33]}` are the Q3 names with the CoD layout.
- Types: `terrainSurface_t`, `terrainPatchEntry_t`, `cellPortal_t`, `cplane_t` (the engine type, declared locally because `BoxOnPlaneSide` takes it).

### 9.3 Invented descriptive names (forced; the spelling carries no evidence)
- Functions of the CoD-specific q3map code, each named after what it does. `portals`: `PointBehindPlanes`, `LeafBoundaryPlaneOrientation`,
  `CellPortalPlaneForLeaf`, `BuildSidePlanes`, `InitBspTreeNodes`, `BuildBspTree_r`, `SplitBrushList`, `AssignBspCells`, `FreeBspTree`, `NodeForBounds`,
  `CollectCellPortals`, `PortalizeNode`, `PortalizeWorld`, `FloodClassnameExcluded`, `RemapPortalPlanes`, `RemapOccluderPlanes`. `patch`: `AllocTerrainBuffer`,
  `FreeTerrainBuffer`, `FindPatchDrawVert`, `AddPatchTriangle`, `MarkPatchNormalAxes`, `GuessPatchProjectionAxis`, `CenterPatchLightmapCoords`,
  `EmitTerrainGroup`, `CompareTerrainPatches`. `surface`: `EmitPatchCollide`, `EmitTerrainCollide`, `FilterCollisionSurfsIntoTree`, `FilterPatchSurfIntoTree`,
  `FilterTriangleSoupIntoTree`. `shaders`: `LoadShaderImageFile`, `ParseShaderTypeFile`. `writebsp`: `RemapNodePlanes`, `RemapBrushPlanes`. Glue:
  `GenerateTriangleListGroups`, `GenerateTriangleStripGroups`, `RemapTriangleListIndices`.
- `CheckWindingPlane` (`polylib`, was `WindingPlaneNoOp`): an empty three-argument stub called after side windings are built (`brush`, `surface`);
  Q3 only has `CheckWinding(w)`, so this name is invented for a compiled-out check.
- Constants whose values come from code but have no name in the image: `MAX_TERRAIN_GROUP_TRIANGLES`, `MAX_TERRAIN_GROUP_VERTS`, `MAX_TERRAIN_PATCHES`,
  `MAX_PATCH_TRIANGULATION_AXIS`, `PATCH_ST_EPSILON`, `MAX_MAP_CELLS`, `MAX_OCCLUDER_FACE_VERTS`. The flag tested before `WriteDebugSurfaceToFile` /
  `WriteDebugSideToFile` is written `SURF_CASTSHADOW` (0x40000, the `castshadow` surfaceparm bit); that this bit gates the debug dump is Proven, why is unknown.
- `SURF_CASTSHADOW` (the bit that KisakCOD calls `SURF_NOCASTSHADOW`; the polarity in the original's surfaceparm table is `castshadow`) and the constant
  `lightmapBorder`.
- `node_t.pad` (was `unusedLeafWord`): an unreferenced word at +0x44 of the 116-byte node, between `referencepoint` and the leaf fields.
- Notable locals: `GuessPatchProjectionAxis` `firstOrigin/firstSecond/firstThird` and `secondOrigin/secondSecond/secondThird` (the vertices of the two
  triangles of a terrain quad); `MatchBrushSideToPortals_r` `point` (= `w->p[i]`, a pointer copy that fixes the operand order of the dot product);
  `EmitTerrainGroup` `triangles` (a write-only result: the original called `AllocTerrainBuffer` once more than it used).
- Other workers' inventions (collected): `hunkUsed_t` fields `mark` and `tempMark` (`com_memory`; CoD2 and CoD4 have only `{permanent, temp}`, the image has two
  extra words); of the `Hunk_*`/`Z_*` names only `Hunk_SetHighTempMark`, `Hunk_ClearToHighTempMark`, `Hunk_HighMarkIsSet`, `Hunk_CommitTempMemory` and
  `Hunk_SetLowUsedInternal` are descriptive guesses (the rest, including `Hunk_ClearData`, `hunk_low`, `hunk_high`, `s_hunkData`, `infoParm_t`/`infoParms` and
  `Com_SurfaceTypeToName`/`FromName`, are confirmed by the CoD1 Mac symbol table, a CoD2 Mac build or the CoD4 PDB); `bspfile`: the lump constants follow the loader
  function names of the CoD 1.3 Mac symbol table (`COD_LUMP_SHADERS`, ...), while `COD_LUMP_TRIANGLE_SOUPS`, `COD_LUMP_COLLISION_*`, `cod_bsp_occluder_edge_t`
  (`{plane[2], vertex[2]}` bytes; CoD2's occluder edge is a different 16-bit layout), `cod_bsp_tri_soup_t`, `cod_bsp_vertex_t`, `cod_bsp_collision_surface_t`,
  `COD_BSP_*_BYTES` and the `bspLight_t` family are descriptive (widths derived from the loader); the local `originCopy` in `UnparseEntitiesWithOrigins` (a dead
  store the bytes need); `com_math`: the locals `rj`, `rk` in `VectorRotateAngles` and its `pair` axis table; the whole-file `extern "C"` wrapper of
  `q_parse.cpp` (see item 1). The `SURF_` / `CONTENTS_` spellings are decompile-derived (KisakCOD); the bits are proven by the original's `infoParms` table.

## What this page does not claim
Apart from the three stamped header fields (see `docs/KNOWN_DIFFERENCES.md`), the build is byte-identical to the original; that proves the *bytes*, not the
source text. Every statement above about which spelling the authors used is limited by the evidence level given with it.
