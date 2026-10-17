# src

The reconstructed sources of `q3map.exe`, grouped like the original solution:

| Folder | Content |
|---|---|
| `q3map/` | the BSP compiler (Q3 `q3map` lineage plus the Call of Duty changes), including the NvTriStrip glue |
| `common/` | shared tool code (`bspfile`, `polylib`, `imagelib`, `threads`, `q_shared`) |
| `code/` | engine-side code used by the tool (`qcommon`, `game`, `jpeg-6`) |
| `universal/` | Infinity Ward engine code shared with the game (`com_math`, `com_memory`, `cvar`, `q_parse`) |
| `libs/` | the static libraries of the solution (`cmdlib`, `jpeg6`, `nvtristrip`) and zlib |
| `win32/` | Windows helpers |

Which files are compiled, with which flags and in which link order is recorded in `../build/sources.json`.
Includes are MSVC-style (quoted system headers); the tree is meant to be built with Visual C++ 7.0, see
`../build/README.md`.
