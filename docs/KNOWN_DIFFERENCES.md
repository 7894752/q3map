# Known differences from the original `q3map.exe` (2026-10-17)

Status of the standalone build (`builduild.py --identical`) against the original (SHA-256 `b7a3cbb2d003e241e97b87d824aa6c61b29cb9b42481641eaca6aed07bc6226a`).

**There are no code or data differences.** All 970 project functions compile to the original bytes and relocations, and the linked image matches the
original in every byte of `.text`, `.rdata`, `.data` and the headers, including the Rich header, except for three fields that no build can produce:

| # | Difference | Where | Cause |
|---|---|---|---|
| 1 | Link timestamp | COFF header, debug directory | `link.exe` 7.00.9466 stamps "now" and has no option to fix it. |
| 2 | PDB GUID | CodeView (RSDS) record | Random per link. |
| 3 | PDB age (163) | CodeView (RSDS) record | Counts updates of the original PDB. |

`build.py --identical` copies the three recorded original values (`build/meta_stamp.json`) into a copy of the linked image (`q3map_identical.exe`);
that copy equals the original byte for byte. The unstamped `q3map.exe` differs from the original in those fields only (25 bytes). The PE checksum is 0 in both.

Byte identity proves the compiled image, not the source text: where several source spellings compile to the same bytes, the choice among them is not
evidence about the original. See `docs/RECONSTRUCTION_NOTES.md`.

How to check: `python builduild.py --vc7 <Vc7> --identical` links the exe and writes `q3map_identical.exe`; the script prints the SHA-256 of the result next
to the original's and says `RESULT: IDENTICAL`. With your own copy of the original, `python build\stamp_meta.py check --input <exe> --original <original exe>`
lists header, debug-directory and Rich differences. See `build/README.md` ("`--identical`", "Known differences").

Last updated 2026-10-17.
