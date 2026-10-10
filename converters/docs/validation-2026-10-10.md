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
