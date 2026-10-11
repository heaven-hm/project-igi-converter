# Desktop diagnostics follow-up — 2026-10-11

Both applications are built from shared source, with separate macOS ARM64 and Windows x64 Qt packages. Windows GUI and CLI checks use CrossOver on macOS; native Windows-host testing is not claimed. The previous representative format matrices remain documented in validation-2026-10-10.md. This follow-up reruns the affected paths rather than claiming another full matrix.

## Corrected model rendering and animation

Qt's TGA reader returned RGB555 colour channels in the 0–31 range. For the real SECURITYBUILDING 400_20_1 exported bundle, pixel (10,10) was RGB (16,15,14) instead of the expanded (131,123,115). Shared previews now read TGA through stb, preserving correctly expanded colours. Textured previews use neutral lighting while retaining explicit lightmaps; untextured meshes retain the headlight. Controlled native screenshots increased mean model-region RGB from (12.68,12.16,10.77) to (99.82,95.65,84.33). CUA confirms the corrected building is visible on Windows too. This is an asset preview, not reproduction of the original game's complete scene lighting.

Animation resolves the selected MEF first and then searches the configured models folder recursively, case-insensitively. A visible Play button loads the selected clip and the first animation is selected automatically. CUA confirms native 015_01_1 and 000_01_1 playback and Windows 015_01_1 playback with advancing time. The real 015 model maps 33 bones with zero maximum mapping difference.

## Scripts, editor settings and logs

Generated QSC has no version comments. IGI2 stores version metadata in a neighboring .qvm-version file and accepts an explicit --qvm-version; GUI saves preserve the original header version. Real IGI1 8.5 and IGI2 8.6/8.7 native and Windows round trips are byte-identical. Fresh focused check_format_coverage.py runs pass 2/2 IGI1 and 3/3 IGI2 checks, including comment rejection and version metadata validation. Unrelated metadata files are rejected before changing existing QSC output. Native and Windows GUI 8.6 saves preserve the original binary.

Settings > Editor Font Size persists 6–72 point text independently for each application. CUA confirms enlarged text on both platforms. Verbose GUI logs are enabled through a one-time preference migration; subsequent explicit user settings remain effective. Shared CLI logging preserves console output and creates per-user files by default. Actual GUI and CLI log files were verified for both games on both platforms.

Wine helper processes can keep captured subprocess pipes open after successful conversion. Initial Windows captured-pipe runs timed out despite complete byte-identical output. Repeating the actual commands with stdout/stderr directed to a file completed cleanly (8.7: 9.71 seconds; 8.6: 1.21 seconds). Both extracted Windows release CLIs exit successfully and append default diagnostic logs.

## Packaging and remaining work

All four ZIPs extract successfully. Each macOS bundle passes strict deep signature verification and has 38 Mach-O files without external Homebrew dependencies (library identity entries excluded). Each Windows package includes 31 DLLs; recursive import checks find no missing non-system DLLs. macOS signatures are ad-hoc. Proprietary fixture files are excluded from source and release assets.

Missing DAT texture-slot warnings are not claimed resolved by the brightness fix. Original GConv recipe/VQ/font/model-authoring parity remains incomplete, as do full skeleton export and unrestricted topology editing. These checks do not establish 100% original-engine parity or acceptance of every converted asset in-game.

Final extracted macOS and Windows GUIs open comment-free QSC with the saved enlarged font, and each app writes its verbose startup message to its actual per-user log. Apps are tested separately and closed normally before starting the next one. Extracted CLIs for all four packages launch successfully. GUI macOS logs live in ~/Library/Preferences/igiNconv/igiNconv.log; CLI logs live in ~/Library/Logs/igiNconv/igiNconv-cli.log. Windows GUI logs live in %LOCALAPPDATA%/igiNconv/igiNconv.log and CLI logs in %LOCALAPPDATA%/igiNconv/logs/igiNconv-cli.log.
