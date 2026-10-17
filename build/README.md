# Building q3map.exe from source

A standalone build of the reconstructed CoD1 `q3map.exe` (2003-10-27 tool build). It needs **your own
copy of Visual C++ 7.0** (Visual Studio .NET 2002). Nothing from Microsoft and no original binary is part of
this repository; only the SHA-256 of the original exe is stored (in `build.py`) so the result can be compared.

## Requirements

| What | Version |
|---|---|
| Compiler | `cl.exe` **13.00.9466** (Visual C++ 7.0, product version 7.00.9466) |
| Linker / librarian | `link.exe` / `lib.exe` **7.00.9466** |
| CRT | the `libcmt.lib`, `libcpmt.lib`, `oldnames.lib` of that same install (static CRT, `/MT`) |
| Platform SDK | the one bundled with that VC7 install: headers (`GL\glaux.h`, `windows.h`, ...) and import libs, including `glaux.lib` (GlAux.Lib, glaux.obj/tk.obj built with 13.00.9176/9178) |
| Python | 3.8 or newer, standard library only |
| OS | Windows (the tools are 32-bit Windows programs) |

`build.py` prints a warning if `cl.exe`, `link.exe`, `lib.exe`, `libcmt.lib`, `libcpmt.lib`, `oldnames.lib` or
`glaux.lib` differ from the files the result was verified with (`--strict` makes it an error). A different compiler
build (for example VC 7.1, 13.10.3077) compiles most files but produces different code; it will not reproduce the
exe. The "stripped VC7.0" distribution <https://github.com/ifarbod/msvc700> has exactly these files.

## Getting the toolchain automatically (`--fetch-toolchain`)

```bat
python build\build.py --fetch-toolchain --identical
```

`build\fetch_toolchain.py` (also usable on its own) downloads the exact toolchain from public archive.org images of
retail Microsoft media, extracts only the 131 files the build touches (about 22 MB), verifies each against the SHA-256 in
`build\toolchain_manifest.json` and deletes the downloads. Nothing Microsoft is stored in this repository
(`build/toolchain/` and `build/out/` are git-ignored); the files are fetched to your machine from archive.org on request.

| Image (archive.org item) | Size | Used for |
|---|---|---|
| `X08-65717`, Visual Studio .NET 2002 Professional, Disc 1 | 311 MB | `cl.exe`, `link.exe`, `lib.exe`, `c1.dll`, `c1xx.dll`, `c2.dll`, `mspdb70.dll`, `msobj10.dll`, `msvcr70.dll`, `msvcp70.dll`, `libcmt.lib`, `libcpmt.lib`, `oldnames.lib`, C++ headers |
| `X08-65718`, Visual Studio .NET 2002 Professional, Disc 2 | 550 MB | the C runtime headers (`stdio.h`, `string.h`, ...) |
| `microsoft-msdn-disc-0004.3-platform-sdk-august-2001-edition-...`, Platform SDK August 2001 | 571 MB | Platform SDK headers (66 files), `GlAux.Lib`, `UUID.Lib`, `GLU32`/`OPENGL32`/`WSOCK32`/`KERNEL32`/`USER32`/`GDI32`/`ADVAPI32` import libs |

Requirements: Python 3, **7-Zip** (`7z.exe` on `PATH`, in `C:\Program Files\7-Zip`, or `--7z <path>`), about 1.5 GB of
free disk space while it runs (one image at a time; about 40 MB remain) and a network connection. The CD images are
only unpacked (7-Zip, then a built-in sector de-framing of the `.mdf`); no installer or program from them is ever run.
Use `--keep` to keep the downloaded archives and `--verify-only` to check an existing `build\toolchain`.
`build.py` finds `build\toolchain\vc7` automatically when no `--vc7` is given.

The VC7 root is the folder that contains `bin\cl.exe`, `include\`, `lib\` and `PlatformSDK\`
(`...\Microsoft Visual Studio .NET\Vc7`). `mspdb70.dll` must be next to `cl.exe` or in
`..\Common7\IDE` (the script adds both to `PATH` for the compiler and linker it starts).

## Steps

```bat
git clone <this repository>
cd <repo>
build\build.bat --vc7 "C:\Program Files\Microsoft Visual Studio .NET\Vc7"
```

or `python build\build.py --vc7 <Vc7 folder>`. The `VCINSTALLDIR` environment variable of a Visual Studio command
prompt is used if `--vc7` is omitted. Options:

| Option | Meaning |
|---|---|
| `--sdk <dir>` | Platform SDK root (`include\`, `lib\`); default `<vc7>\PlatformSDK` |
| `--fetch-toolchain` | download the toolchain into `build\toolchain` first (needs 7-Zip; `--7z <path>` if not on `PATH`) |
| `--identical` | additionally write `q3map_identical.exe` (see below) |
| `--out <dir>` | output directory (default `build\out`) |
| `--jobs N` | parallel `cl.exe` processes, 1 or 2 (default 2) |
| `--wine [CMD]`, `--dry-run` | run the tools under Wine (experimental, see below) / print the tool command lines only |
| `--link-only` | relink the objects already in `<out>\obj` |
| `--clean`, `--keep-going`, `--strict` | delete `<out>` first / do not stop at the first compile error / fail on toolchain hash mismatch |

What it does (the structure of the original Visual Studio solution):

1. Reads `build\sources.json` (static manifest, no other tooling): 75 translation units from `src`, each with
   its flag set (`/O2 /Ob1 /GF /Gy /MT /Zi`, `/GS` per TU, `/EHsc` for C++ that needs it, `/YX` for the two NvTriStrip
   files) and its project: `q3map` (the exe), `jpeg6` (19 libjpeg files + `jpgload.cpp`), `cmdlib`, `nvtristrip`.
2. Compiles every TU to `<out>\obj` (include search: the TU's directory, `src`, VC7 `include`, SDK `include`).
3. Builds the three static libraries `<out>\lib\{jpeg6,cmdlib,nvtristrip}.lib` with your `lib.exe /NOLOGO`, members in
   the manifest order, then links the exe objects with: `jpeg6.lib cmdlib.lib glaux.lib GLU32.lib WSOCK32.lib
   OPENGL32.lib KERNEL32.lib USER32.lib GDI32.lib ADVAPI32.lib nvtristrip.lib` (GlAux.Lib is the Platform SDK's;
   `libcmt`, `oldnames`, `libcpmt` are the implicit default libraries named by the objects' own directives). Link options:
   `/INCREMENTAL:NO /DEBUG /STACK:4194304,4096 /PDBALTPATH:d:\projects\mk\bin\q3map.pdb`; there are no `/INCLUDE:`
   pulls and no other special options.
4. Prints the SHA-256 of the result next to the SHA-256 of the original exe.

The exe is never executed by the build. Output: `<out>\q3map.exe` (+ `.map`, `.pdb`, `link.log`).

## Linux / macOS (experimental, UNTESTED)

`build.py` can run the VC7 tools under [Wine](https://www.winehq.org/). **This path has never been run by the
maintainers (there is no Wine on the build machines).** VC7.0 (2002) may not run under every Wine version; if anything in
the toolchain behaves differently the output may not be byte-identical. The build prints `IDENTICAL` or `differs` either way,
so you can tell. Apple Silicon needs an x86 Wine layer (Rosetta 2 or similar), also untested.

Needs: Wine with 32-bit support (WoW64 or a 32-bit prefix), Python 3, `winepath` (ships with Wine), and 7-Zip (`7z`) if you use
`--fetch-toolchain`.

```sh
python3 build/build.py --fetch-toolchain --identical
# or with a toolchain you already have: python3 build/build.py --vc7 /path/to/Vc7 --identical
# explicit Wine command (default "wine"):   python3 build/build.py --wine "wine64" --vc7 ...
```

`--wine` is switched on automatically when Python is not running on Windows. Every `cl.exe`, `lib.exe` and `link.exe` call
becomes `wine <tool>.exe ...`; every path handed to a tool (sources, `/I`, `/Fo`, `/OUT`, `/LIBPATH`, response files, the
TMP directory) is converted to a Windows path with `winepath -w` (fallback: the `Z:` drive mapping); long command lines go
through response files; `mspdb70.dll` is found via `WINEPATH`. Windows behaviour is unchanged.

Troubleshooting: `--dry-run` prints the exact tool command lines after path conversion without running anything (it works
without a toolchain and on any OS; `--wine --dry-run` shows the Wine form). `python build/tools/test_wine_cmds.py` unit-tests
the conversion and command construction.

## `--identical`

`link.exe` 7.00.9466 cannot reproduce three header items of the original, so `build.py --identical` runs
`build\stamp_meta.py apply` on a copy (`q3map_identical.exe`; the linker output is kept untouched):

- COFF and debug-directory **timestamp** (the linker stamps "now"; it has no `/TIMESTAMP`),
- CodeView **RSDS GUID and age** (random per link; the original PDB had been updated 163 times); the RSDS *path*
  is produced by the linker itself with `/PDBALTPATH`.

The values live in `build\meta_stamp.json` (extracted once from the original: constants only). The PE checksum is 0
in both. The **Rich header is not stamped**: since the translation units use the original language modes (C or C++) the
linker's own Rich header equals the original's (verified 2026-10-17). Without `--identical` those three items are the
only intended header differences you will see.

## No modelled inputs

Everything on the library side is stock Visual Studio .NET 2002 / Platform SDK content used as shipped. The 15 padding
bytes before `std::_String_base::_Xran` come from the three `oldnames.lib` alias objects pulled by the POSIX spellings
`mkdir`/`getcwd`/`stricmp` (the sources use the original names) together with the objects' default-library order
`libcmt, oldnames, libcpmt`. The CRT member order follows from the original solution's static libraries (`jpeg6.lib`,
`cmdlib.lib`) listed before the import libraries. The only values that cannot come out of the linker are the three
stamped header items above.

## Link-option ablation (what each option does)

Measured 2026-10-17 on the final static-library build by a maintainer script (not included in this repository): the same objects are re-linked with one change and compared byte
for byte (after the stamp) with the unchanged link. "link fails" = unresolved externals.

Options that are passed (removing them changes the image):

| Removed | Effect |
|---|---|
| `/INCREMENTAL:NO` | incremental image, 700416 bytes instead of 536576 |
| `/DEBUG` | no debug directory, image 405504 bytes |
| `/STACK:4194304,4096` | 1 byte (stack reserve) |
| `/PDBALTPATH:...` | RSDS record differs, 5008 bytes differ |
| `/NOLOGO` | none (cosmetic; kept) |

Options of the earlier models that are NOT passed (re-added one at a time, no byte changes): `/FILEALIGN:4096`, `/FIXED`,
`/BASE:0x400000`, `/MACHINE:IX86`, `/OPT:NOREF`, `/OPT:NOICF`, `/HEAP:1048576,4096`, `/VERSION:0.0`,
`/SUBSYSTEM:CONSOLE[,4.0]`, `/NODEFAULTLIB:nvtristrip.lib`. These break the image: `/OPT:REF` (405504 bytes),
`/OPT:ICF` (528384), `/ALIGN:4096` alone (529920; `/ALIGN` also selects 512-byte file alignment), `/ALIGN:512`,
`/FILEALIGN:512`.

Libraries and inputs:

| Change | Effect |
|---|---|
| without `jpeg6.lib`, `cmdlib.lib`, `nvtristrip.lib`, `glaux.lib`, GLU32, WSOCK32, OPENGL32, USER32, GDI32 or ADVAPI32 | link fails |
| without KERNEL32.lib | links (the CRT supplies it) but 124777 bytes differ (descriptor/thunk order) |
| import libs in any of 12 random other orders | 2454 to 2768 bytes differ |
| import libs after `nvtristrip.lib` | 36315 bytes differ |
| `jpeg6.lib` / `cmdlib.lib` last instead of first | 177230 bytes differ |
| `cmdlib.lib` before `jpeg6.lib` | 38903 bytes differ |
| `jpeg6.lib` member order reversed | none (the linker pulls members by reference, not archive order) |
| explicit `libcmt oldnames libcpmt` first | 213556 bytes differ |
| explicit `libcmt libcpmt oldnames` last | 144 bytes differ |
| all objects linked directly, no static libraries | link fails |
| reconstructed `glaux.c`/`tk.c` instead of GlAux.Lib | link fails (they define no data; the SDK code is the original) |

Earlier versions needed 515 `/INCLUDE:` pulls and a patched libcpmt; both are gone since the static-library structure
and the POSIX spellings (`mkdir`, `getcwd`, `stricmp`) were recovered.

## Result

Status 2026-10-17: `build.py --identical` on the current sources prints **IDENTICAL**: the stamped `q3map_identical.exe`
has SHA-256 `b7a3cbb2d003e241e97b87d824aa6c61b29cb9b42481641eaca6aed07bc6226a`, the SHA-256 of the original q3map.exe, and
equals the output of the maintainers' relink (`relink.py --final`) byte for byte. The unstamped `q3map.exe` differs from the
original only in the 25 stamped bytes (two timestamps, the PDB GUID and the PDB age).

## Source audit (src)

`python build\tools\audit_sources.py`, measured 2026-10-17 on the current tree (jpeg-6 as `.cpp`):

- no absolute paths; no includes outside `src`; no include-case mismatches; ASCII only;
- results are independent of the checkout location, and of line endings: the same SHA-256 from the
  tree built in place, from a copy under another directory, from an all-LF copy and from an all-CRLF copy
  (14 files currently mix CRLF and LF, harmless);
- 10 quoted includes (`"windows.h"`, `"stdio.h"`, `"stdlib.h"`, `"qfiles.h"`, `"surfaceflags.h"`, `"jpegint.h"`,
  `"jerror.h"`, ...) resolve only through the compiler's search of system directories and of the *including files'*
  directories (MSVC behaviour); they work with VC7 and would not with GCC/Clang;
- not built: `code\jpeg-6\jchuff.h` (unused), the reconstructions of the SDK GLAux library (GlAux.Lib from the SDK is linked instead; they are not part of
  this repository).

## Layout

```
build/build.py  build.bat         the build
build/sources.json                static manifest: TUs, projects/libraries, link options
build/stamp_meta.py meta_stamp.json   --identical stamp (stdlib only)
build/tools/audit_sources.py      portability audit
build/tools/test_wine_cmds.py     unit test of the Wine path conversion / command construction
```
