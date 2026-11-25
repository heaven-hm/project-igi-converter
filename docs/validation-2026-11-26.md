# Validation run — 2026-11-26 (portability, PGM→LMP, BEF layer)

All results below are from commands actually executed. Skips are tests that
require a retail `IGI_GAME_PATH` corpus; they are reported as skips, not passes.

## macOS (AppleClang, GUI off)

```
cmake -S . -B build-final -G Ninja -DCMAKE_BUILD_TYPE=Release -DIGI1CONV_BUILD_GUI=OFF
cmake --build build-final --parallel 4
ctest --test-dir build-final -j4 --output-on-failure
```
Result: `100% tests passed out of 118`, 0 failures. Direct gtest summary:
`118 tests from 6 test suites`, `44 PASSED`, `74 SKIPPED`.

```
cmake -S converters -B build-conv-final -G Ninja -DCMAKE_BUILD_TYPE=Release -DIGICONV_BUILD_GUI=OFF
cmake --build build-conv-final --parallel 4
```
Result: `bin/igi1conv` and `bin/igi2conv` both linked.

## Linux (GCC 13.3, native arm64 ubuntu:24.04 container, GUI off)

```
cmake -S . -B build-linux-native -G Ninja -DCMAKE_BUILD_TYPE=Release -DIGI1CONV_BUILD_GUI=OFF
cmake --build build-linux-native --parallel 4
ctest --test-dir build-linux-native -j4 --output-on-failure
```
Result: `100% tests passed, 0 tests failed out of 118`; direct gtest:
`44 PASSED`, `74 SKIPPED`.

Converters:
```
cmake -S converters -B build-conv-native -G Ninja -DCMAKE_BUILD_TYPE=Release -DIGICONV_BUILD_GUI=OFF
cmake --build build-conv-native --parallel 4
```
Result: `igi1conv` and `igi2conv` built (see final verification comment on #22).

## GitHub Actions (workflow added by this change)

Run: https://github.com/heaven-hm/project-igi-converter/actions/runs/45722245355

* `ubuntu-latest`: success — `100% tests passed out of 118`; both converter CLIs linked.
* `macos-latest`: success — `100% tests passed out of 118`; both converter CLIs linked.
* `windows-qt`: first attempt failed because `Qt6_DIR` was null with
  `install-qt-action@v4`; fixed to use `QT_ROOT_DIR`. Rerun status is recorded
  in the evidence comment on issue #22.

## Pure-Python BEF format layer

```
python3 blender/tests/test_bef_format.py
```
Result: `bef_format: 2 tests passed`. All addon modules pass `py_compile`.

The Blender `bpy` path was **not** executed: `/opt/homebrew/bin/blender` is a
shim whose target `/Applications/Blender.app/Contents/MacOS/Blender` does not
exist. No Blender-success claim is made.

## Non-claims

* No IGI1↔IGI2 cross-game conversion is implemented or tested.
* No Windows GUI build is claimed from a local host; only CI evidence is used.
* WAV encode / QAS / ILFF work in open PR #16 is not claimed as shipped here.
