# zlib 1.1.4 (Call of Duty adaptation)

Source: zlib 1.1.4 by Jean-loup Gailly and Mark Adler, <https://github.com/madler/zlib/releases/tag/v1.1.4>.
The original `q3map.exe` reports the version string `1.1.4` (`zlibVersion`, `compress2`). The unmodified 1.1.4 C sources
reproduce the original instruction bodies with VC7.0 and the per-file flags in `build/sources.json`.

Call of Duty adaptation in `zutil.c`: `zcalloc` calls `Z_MallocInternal(items * size)`, which clears memory and applies
the engine's allocation failure policy; `zcfree` calls `Z_Free`. All other files keep the zlib code and its copyright and
license notices; see `THIRD_PARTY_NOTICES.md`.

The `inflate` code contains switch tables whose compiled form depends on the exact source layout of the files
(`infblock.c` carries a copy of the `inflate_codes_state` definition that `infcodes.c` also has).
