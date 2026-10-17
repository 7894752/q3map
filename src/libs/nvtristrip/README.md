# NvTriStrip (restored older revision)

NVIDIA's NvTriStrip triangle stripifier, in the older revision that the original `q3map.exe` was built with. This is not
the version found in current public repositories; the differences that matter for the compiled code are:

- the four-argument `GenerateStrips` returns `void`, and there is no primitive restart or validation interface;
- `WordVec` uses the Windows `WORD` type (unsigned short);
- `NvFaceInfo` contains six integers, with no later fake-face flag; synthetic faces use the repeated first/third
  vertex shape, input degenerates are filtered by `BuildStripifyInfo`;
- the index overload of `IsDegenerate` is a non-static member; `CreateStrips` takes four arguments;
- loop counters are signed and the Windows types are kept, not the portability edits of later versions;
- `VertexCache` is implemented in its header; a separate `VertexCache.cpp` of later versions must not be compiled with it;
- `FindAllStrips` has no `numTrisWeight` local.

The library is compiled as C++ with exception handling (`/EHsc`) and automatic precompiled headers (`/YX`); see
`build/sources.json`. The project glue is in `q3map/tristrip_nv.cpp` and `q3map/tristrip.c`.

NVIDIA published NvTriStrip freely; no license text accompanies the library. See `THIRD_PARTY_NOTICES.md`.
