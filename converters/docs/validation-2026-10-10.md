# Desktop validation — 2026-10-10

IGI1Conv and IGI2Conv are separate executables built together from the shared converters tree. GUI and CLI use the same executable. The tested deliverables are macOS Apple Silicon ARM64 and Windows x86_64 Qt packages. Windows testing ran through CrossOver on macOS; a native Windows-host run is not claimed. Intel macOS GUI builds were not produced.

## Format checks

Native macOS: IGI1 33/33, IGI2 38/38 representative end-to-end checks. Final Windows: IGI2 38/38; IGI1 32/33 initially with one Wine timeout while re-exporting an edited LMP, then the isolated edited-LMP case passed all three conversions (5.86, 2.60, 2.16 seconds). The original timeout report is retained. These checks cover representative formats and edited-value persistence, not every possible asset or every original GConv API.

## GUI checks

One app ran at a time and each was quit normally after testing. Both games: shaded model rotation, wireframe, graph rendering/visibility, texture preview/edit/save, QVM display, animation and audio transport. IGI2 adds real Level5 graph4 (875 nodes/3781 links), skeletal MEF, MINA skeleton (31 bones/160ms), invalid DAT save preserving its input and valid DAT edited-field persistence. Retail QVM GUI save recompiles byte-identically. macOS image horizontal flip saves to TEX with decoded pixels reversed; text-MEF export/recompilation is byte-identical. Windows pencil single-click persists painted pixels in rebuilt TEX.

macOS Qt audio transport supports play/pause, volume, seek, restart/end and stop. Wine WinMM GUI playback/pause/seek/volume/stop works for the real IGI1 68.519-second WAV and IGI2 Level5 101.747-second MP3. Independent WinMM assertions verify stable paused position, volume, seek/resume, stop/reset and close. Shared MP3 decoding matches FFmpeg sample counts/channels/rates for mono and stereo fixtures, with maximum 16-bit PCM differences of 1 and 2 respectively.

## Packaging

Four ZIPs extracted successfully and each extracted CLI launched. Both macOS app bundles passed strict deep signature verification; 38 Mach-O files per app have no external Homebrew build dependencies (library identity entries excluded). Each Windows package contains 31 DLLs; recursive import inspection found no missing Qt, FFmpeg or compiler-runtime DLLs. Qt plugins, launch instructions and dependency notices are included. macOS uses ad-hoc signatures.

## Remaining original-engine parity

Original GConv recipe expression evaluation/command sequencing, automatic mipmap/VQ generation, font creation/scanning, full model authoring and callback/option behavior still need reconstruction and differential validation. Existing native conversions and GUI tests do not establish 100% GConv/DLL parity or in-game engine acceptance.

## Follow-up texture editor and original-DLL parity checks

Post-preview changes add optional ARGB8888 `--mipmaps` creation. Square, rectangular and varying-alpha fixtures match original GConv 3.5 with AutoMipMap=ON, AutoVQ=OFF, Dither=OFF exactly. Both readers support eleven-level 1024-square textures. General recipe/VQ/font/model authoring parity remains unfinished.

CUA verifies native macOS IGI2 and CrossOver Windows IGI2 horizontal-flip saves match the original DLL's complete nine-level output. Undo and platform Save shortcuts restore the original complete file. Native and Windows IGI1 unchanged version-9 sprite saves are byte-identical; flipped sprites preserve the header, decode to the exact mirrored pixels and match across platforms. Windows cut/paste/save round trip preserves the original texture. Save confirmations were dismissed through desktop input; converter processes were closed through normal window/application controls. No converter windows remained after these checks.

GUI saves now rebuild supported existing containers atomically; they refuse new SPR/PIC layout authoring without metadata. Existing structural restrictions and unsupported VQ semantics remain. These follow-up changes require new packages and are not in the original `v2026.10.10-qt-desktop` preview.

The follow-up is published as `v2026.10.10-qt-desktop-r2`, built from commit `3a910eaa40c6f65266a4749572f4bb125eee25aa`. All four remotely downloaded ZIPs and SHA256SUMS match local candidates. Extracted macOS GUI startup and normal exit passed for each app separately. Strict signatures and all 38 Mach-O dependencies per app passed; Windows packages contain 31 DLLs with verified closure and packaged CLI alpha-mipmap parity. The original preview remains pinned to its original source.
