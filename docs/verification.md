# Verification scope and procedure

## Required real examples

Use fixtures named `qvm.qvm`, `res.res`, `tex.tex`, `spr.spr`, `pic.pic`, `fnt.fnt`, `mef.mef`, `iff.iff`, `wav.wav`, `mtp.mtp`, `mapping.dat`, `graph.dat`, `olm.olm`.

IGI 1 also requires `hmp.hmp`, `bit.bit`, `cmd.cmd`, `ctr.ctr`, `lmp.lmp`, `wav-adpcm.wav` (the distinct IMA ADPCM audio variant), `avi.avi`, `afp.afp`. BEF and QSC examples are decompiled from the real animation and script, with a separate recompile/re-export comparison.

IGI 2 also requires `qvm-retail.qvm` (8.7; the primary script is demo 8.6), `mef-skeletal.mef`, `mef-lightmapped.mef`, `thm.thm`, `tlm.tlm`, `tmm.tmm`, `syn.syn`, `forest.dat`, **nonempty** `graphcover.dat`, `mp3.mp3`, `bmp.bmp`, `jpg.jpg`.

PNG/TGA paths are tested from a real font atlas. Distinct dialects and mesh variants are tested as variants of a format; duplicate examples are not counted as extra format coverage.

## Assertions

- QVM/QSC: exact binary rebuild and exact decompiled source re-export.
- RES: independent archive reader compares every resource name and payload.
- TEX/SPR/PIC/FNT: exact template rebuild, including untouched compressed bytes.
- MEF: exact ILFF JSON rebuild and actual OBJ vertices/faces for each model variant.
- IFF: exact JSON rebuild. IGI 1 BEF commands and numeric keyframes/events are compared with explicit float tolerance.
- WAV: sample count/rate/channel equality; raw PCM exactness or bounded ADPCM sample error.
- MTP/DAT: parsed model/texture mapping equality; required JSON fields accessed directly.
- Terrain, OLM, graphs, forest, cover, SYN: exact rebuild and selected edited fields survive a second parse.
- BMP/JPEG: decoded pixels preserved through a lossless BMP/PNG intermediate.
- MP3: frame normalization is idempotent.
- AVI/AFP: exact preservation, explicitly not a codec conversion.

All required rows fail if a fixture is absent. Original files are copied into temporary directories before conversion; the test harness never edits the user's game files.

## Reproducible commands

```sh
python verification/check_format_coverage.py --binary PATH_TO_CLI --game 1 --samples fixtures/igi1 --report igi1-report.json
python verification/check_format_coverage.py --binary PATH_TO_CLI --game 2 --samples fixtures/igi2 --report igi2-report.json
cmake -S . -B build-sanitized -DIGICONV_ENABLE_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-sanitized --parallel
```

IGI 1's additional GoogleTest suite requires `-DIGI1CONV_BUILD_TESTS=ON` and `IGI_GAME_PATH`. Its historical fixture-dependent skips are reported separately from the mandatory format matrix. IGI 2's synthetic malformed-input/codec/CLI suite is registered with CTest.

Native Windows and Intel macOS CI uses the same representative script and temporary private fixtures. CI uploads binaries, checksums and reports; the temporary fixture archive is removed after verification. Release ZIPs contain tools, their own metadata, documentation and licenses only.

## Evidence and limits

Local reports under `verification/evidence/` identify the exact executable by SHA-256 and distinguish byte fidelity, decoded-content fidelity, edits and preservation-only operations. A successful process exit alone is insufficient. Real-game execution and unsupported original GConv recipe APIs are outside the claims of this asset-converter test matrix.

## Latest local results

IGI 2: 38 mandatory checks across 22 extensions; 98 synthetic checks. Five distinct texture/sprite layouts rebuild exactly.

The native Windows and Intel macOS matrix first passed in run 38005115413; a second run verifies the final graph overflow regression. The IGI 2 legacy source-directory build entry point now forwards to the complete native target.
