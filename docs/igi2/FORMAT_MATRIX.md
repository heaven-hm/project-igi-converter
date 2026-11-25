# IGI 2 conversion suite — vertical slice design & format matrix

Scope: GitHub issue [#10](https://github.com/heaven-hm/project-igi-converter/issues/10),
"IGI 2 conversion suite (all core formats, IGI 1 parity)". The issue is a
planning/tracking ticket and explicitly says it is *not a single PR*. This
document records the design of a **safe, reviewable vertical slice** and the
status of every format named in the issue, so follow-up child issues can pick
up the remaining work without re-deriving the layout.

Format knowledge is sourced from Artiom Rotari's
[`igipy`](https://github.com/artiom-rotari/igipy), specifically
[`docs/igi2/formats/`](https://github.com/artiom-rotari/igipy/tree/main/docs/igi2/formats),
plus the repository's own
[IGI 1 vs IGI 2 format overview](../game_file_formats_igi1_igi2.md).

## 1. Design of this slice

### 1.1 Game selector (CLI)

A leading global flag selects the backend before any command is dispatched:

```text
igi1conv [--game=igi1|igi2] <command> [options]
```

* `--game=igi1` (default) keeps **every** existing invocation unchanged.
* `--game=igi2` routes to the IGI 2 backend.
* `--game igi2` is accepted as an alias.
* An unknown value, a missing value and a missing command with an explicit flag
  produce an error/help instead of guessing a game.

Implementation: `source/game_selector.h` (header-only so both the CLI and the
unit tests can use it) and the dispatch/gating block in `igi1conv/main.cpp`.

**Mixed-input rejection.** Commands are gated by game:

* `forest` is an IGI 2 format: without `--game=igi2` the CLI exits with code 1
  and an explanatory message instead of sending it to the IGI 1 `dat` parser.
* With `--game=igi2`, all currently unported commands are rejected with
  "not implemented for IGI 2 yet" rather than being silently routed to the
  IGI 1 parser. This is the safety property called out in the issue
  ("reject mixed input trees"); it is deliberately conservative.

**GUI selector: not implemented in this slice.** The Qt GUI is a separate,
large surface (and is being restructured in a different branch/PR). Adding a
game dropdown here would mix two unrelated changes and cannot be validated in
this environment. See §4.

### 1.2 The chosen format: DAT Forest (`forest_<task_id>.dat`)

DAT Forest is the smallest *complete* IGI 2 format in the issue: its binary
layout is fully decoded, read and write are symmetric, it has no external
dependencies, and it is genuinely IGI 2-only. That makes it a good first
end-to-end path (binary → JSON → binary) with byte-exact verification.

Layout (from [igipy `dat_forest.md`](https://github.com/artiom-rotari/igipy/blob/main/docs/igi2/formats/dat_forest.md)):

```text
uint32 10 | 0x0A | "Ver. 3.0" | 0x0A                    (14-byte header)
repeated 40 bytes:
  float32 position[3], rotation[3], scale[3],
  uint8   color[4] (r,g,b,a; a unused)
```

* Reader: `igi2::Forest_ParseBytes` / `Forest_ParseFile` — rejects a bad magic
  value, a wrong/newline-missing version string, a short file and a record area
  that is not a multiple of 40. Negative-zero rotations (`0x80000000`, common
  in retail files) are preserved bit-for-bit.
* Writer: `igi2::Forest_Serialize` / `Forest_WriteFile` — byte-exact for any
  file the reader accepts.
* JSON: `Forest_FormatJSON` / `Forest_ParseJSON` use an `igi2-forest/1` schema
  with `header_value`, `version`, `record_count` and a `records` array whose
  field names match igipy's `ForestRecord` (`position_x`, `rotation_y`,
  `color_r`, …). The JSON reader is a small recursive-descent parser; malformed
  JSON, missing fields and out-of-range colours are rejected.

CLI (routed through `igi1conv/cmd_igi2_forest.cpp`):

```text
igi1conv --game=igi2 forest info   <forest_*.dat>
igi1conv --game=igi2 forest export <forest_*.dat> [-o <out.json>]
igi1conv --game=igi2 forest compile <in.json>       -o <out.dat>
```

Exit codes follow the existing CLI convention: 0 ok, 1 bad args, 2 file not
found, 3 parse/JSON error, 4 write error.

### 1.3 Test strategy

`tests/test_igi2_forest.cpp` builds fixtures **from the published format** in
memory (no game assets, no hard-coded user paths, no machine-specific data):
header/record decoding including negative zero, zero-record files, malformed
input rejection, binary→JSON→binary byte equality, JSON validation, and game
selector parsing. `CMakeLists.txt` compiles the standalone parser directly
into the test target so the tests do not depend on the Qt/GUI build.

## 2. IGI 2 format matrix

Legend: **RW** = read and write implemented here · **R** = read only ·
**TODO** = not implemented · **shared** = an IGI 1 parser exists but its IGI 2
variant has not been validated. "igipy" links are the upstream documentation.

| Format | igipy reference | Read | Write | Notes / next step |
|---|---|---|---|---|
| `dat_forest` | [dat_forest.md](https://github.com/artiom-rotari/igipy/blob/main/docs/igi2/formats/dat_forest.md) | **RW** | **RW** | This slice. Byte-exact round trip, synthetic fixtures. Real-game validation pending. |
| `dat_graph` | [dat_graph.md](https://github.com/artiom-rotari/igipy/blob/main/docs/igi2/formats/dat_graph.md) | TODO | TODO | IGI 1 `source/parsers/graph_parser.*` is shared but IGI 2 node/edge layout is unverified. |
| `dat_graphcover` | [dat_graphcover.md](https://github.com/artiom-rotari/igipy/blob/main/docs/igi2/formats/dat_graphcover.md) | TODO | TODO | Cover/visibility flags layered on graphs. |
| `dat_mtp` (text table) | [dat_mtp.md](https://github.com/artiom-rotari/igipy/blob/main/docs/igi2/formats/dat_mtp.md) | shared | shared | IGI 1 `dat_parser.*` parses the text table; IGI 2 variant not validated. |
| `.mtp` (binary FORM) | [dat_mtp.md](https://github.com/artiom-rotari/igipy/blob/main/docs/igi2/formats/dat_mtp.md) | shared | shared | IGI 1 `mtp_parser.*` handles MODS/TEXF/INST; IGI 2 `VNAM` variant not validated. `mtp-to-json` TODO. |
| `.mef` (type 0/1/3) | [mef.md](https://github.com/artiom-rotari/igipy/blob/main/docs/igi2/formats/mef.md) | partial | TODO | IGI 2 OCEM/ECAF type 0/3 parsing is in a separate open PR (#17). FBX export and type-1 rig TODO. |
| `.iff` (IGI 2 flavour, 31/47-bone) | [iff.md](https://github.com/artiom-rotari/igipy/blob/main/docs/igi2/formats/iff.md) | shared | TODO | IGI 1 `iff_parser.*` exists; IGI 2 timing/big-endian hybrid layout unverified. |
| `.qvm` / `.qsc` (IGI 2) | [qvm.md](https://github.com/artiom-rotari/igipy/blob/main/docs/igi2/formats/qvm.md) | shared | shared | Shared VM; richer IGI 2 script catalogue not validated. |
| `.syn` | [syn.md](https://github.com/artiom-rotari/igipy/blob/main/docs/igi2/formats/syn.md) | TODO | TODO | Lip-sync/timing envelopes. |
| `.thm` | [thm.md](https://github.com/artiom-rotari/igipy/blob/main/docs/igi2/formats/thm.md) | TODO | TODO | Terrain heightmap metadata. |
| `.tlm` | [tlm.md](https://github.com/artiom-rotari/igipy/blob/main/docs/igi2/formats/tlm.md) | TODO | TODO | Terrain lightmap metadata. |
| `.tmm` | [tmm.md](https://github.com/artiom-rotari/igipy/blob/main/docs/igi2/formats/tmm.md) | TODO | TODO | Terrain material/mission table. |
| `.fnt` (IGI 2) | [fnt.md](https://github.com/artiom-rotari/igipy/blob/main/docs/igi2/formats/fnt.md) | shared | TODO | IGI 1 `fnt_parser.*` exists; IGI 2 atlas layout unverified. PNG/JSON → `.fnt` TODO. |
| `.olm` (IGI 2) | [olm.md](https://github.com/artiom-rotari/igipy/blob/main/docs/igi2/formats/olm.md) | shared | TODO | IGI 1 `lightmap_resolver.*` exists; 1-or-3 layer IGI 2 variant unverified. |

Scripts category documentation (AI, missions, physics, menu, sound, player
combat) is available upstream at
[`docs/igi2/scripts/`](https://github.com/artiom-rotari/igipy/tree/main/docs/igi2/scripts)
and should drive the `.qvm` decompiler work; no category-level mapping is
implemented here.

## 3. Issue #10 acceptance criteria — honest status

| Criterion | Status | Evidence / gap |
|---|---|---|
| CLI and GUI expose a clear IGI 1 / IGI 2 game selection | **Partial** | CLI: `--game=igi1\|igi2` selector, gating and tests implemented (§1.1). GUI dropdown: **not implemented** — separate Qt work, cannot be validated here. |
| At least one end-to-end IGI 2 conversion path implemented and tested **using real game data** | **Partial** | `forest` binary→JSON→binary is implemented and byte-exact on spec-derived synthetic fixtures (§4). **No licensed IGI 2 game data is available in this environment**, so the "real game data" requirement is **unverified**. `mef→fbx` + `mtp-to-json` remain TODO. |
| Documentation clearly lists supported vs TODO IGI 2 formats | **Done** | §2 of this document; linked from `README.md` and `docs/SUPPORTED_FORMATS.md`. |

The issue is therefore **not ready to close**. This PR is an incremental slice,
not a claim of full parity.

## 4. Verification performed

Commands run from the isolated worktree on macOS (arm64), with the system
`g++` (Homebrew clang cannot find an SDK here):

```text
# Unit tests (6/6 passed)
g++ -std=c++20 -I/opt/homebrew/include \
    source/parsers/igi2_forest_parser.cpp tests/test_igi2_forest.cpp \
    -lgtest -lgtest_main -pthread -o /tmp/igi2_forest_tests
/tmp/igi2_forest_tests            -> [  PASSED  ] 6 tests.

# CLI handler compile + end-to-end smoke on a spec-derived 3-record fixture
g++ -std=c++20 -Iigi1conv -Isource -Isource/parsers \
    -Ithird_party/glm-1.0.1 -Ithird_party \
    cli_smoke.cpp igi1conv/cmd_igi2_forest.cpp source/parsers/igi2_forest_parser.cpp \
    -o /tmp/igi2_forest_cli
igi2_forest_cli info forest_2540.dat      -> 3 records, 134 bytes
igi2_forest_cli export ... -o *.json      -> ok
igi2_forest_cli compile *.json -o *.dat   -> ok
cmp forest_2540.dat roundtrip.dat         -> byte-exact PASS

# Dispatcher syntax check
g++ -std=c++20 -Iigi1conv -Isource ... -c igi1conv/main.cpp -o /tmp/igi2_main.o
```

Not run: the full CMake/CTest build. `CMakeLists.txt` requires Qt5/Qt6 with a
Windows-hardcoded prefix and the existing `tests/*.cpp` suite includes
`windows.h`, so the whole project cannot be configured/built on this macOS
host. This is an environment limitation, not a test failure; it is recorded
here rather than being reported as a pass.

## 5. Remaining blockers / follow-up work

1. **Real-game validation** — obtain a licensed IGI 2 install (via the
   runtime `--game-path`/`IGI_GAME_PATH` pattern already used for IGI 1) and
   validate a retail `forest_*.dat` round trip. Until then criterion 2 is not
   met.
2. **GUI game selector** — add the dropdown to the Qt GUI and route it to the
   same `igi_game_t`; do this in the GUI-focused work, not in a parser PR.
3. **`mtp-to-json` + `mef→fbx`** — the issue's example end-to-end path; depends
   on validating the IGI 2 `.mtp` `INST`/`VNAM` tables and on the separate MEF
   work (PR #17).
4. **Remaining formats** — implement per format in child issues, using the
   matrix in §2 and the upstream igipy docs as the source of truth.
5. **Mixed input trees beyond the CLI gate** — the current gate is command
   based; content-based detection (e.g. IGI 1 vs IGI 2 `.res`) is still TODO.
