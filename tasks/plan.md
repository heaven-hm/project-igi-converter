# Implementation plan: IGI 1 and IGI 2 native converters

## Deliverables
- `feature/igi1conv-cross-platform`: IGI 1 converter `igi1conv`.
- `feature/igi2conv-cross-platform`: native C++17 GConv port renamed `igi2conv`.
- CLI only on both platforms; macOS `.app` bundles contain the native CLI executable, with no GUI.
- Windows x64 and macOS Intel/ARM64 release archives, source, reproducible verification reports, updated README and command documentation.

## Architecture
Keep game-specific QVM opcode maps and font/texture layouts separate. Reuse verified readers only where actual headers and game assets prove compatibility. IGI 1 compiles LOOP 8.5; IGI 2 compiles LOOP 8.7 with its additional header word. Never pass an IGI 2 asset through an IGI 1 writer by silently changing a version.
The IGI 2 CLI is native C++17 and does not load Windows DLLs on macOS. Both native CLI targets now use C++17; existing optional GUI code is excluded from these release builds. Preserve all existing uncommitted work in the original checkout.

## Ordered work
1. [x] Inventory tracked source, current local changes, supplied samples and both game executables. Save format/version counts and source hashes; inspect original EXE/DLL imports and exports with radare2. Query the real Ghidra MCP backend for relevant format routines.
   Acceptance: evidence identifies each corpus and actual QVM versions; no game binaries committed.
2. [x] Create isolated branches/worktrees; carry the existing macOS portability fixes and latest native gconv fixes into their respective branches.
   Acceptance: original checkout unchanged; branches have correctly named CLI targets.
3. [x] Build each macOS executable, run all existing tests, add deterministic representative format verification with no silent missing-fixture passes; the legacy random corpus tests are supplemental. Fix observed failures and rerun affected checks.
   Acceptance: per-file reports record pass/fail/unsupported, payload equivalence and actual pixel/audio comparisons where encoding differs.
4. [x] Complete IGI 2 CLI command support needed by supplied files, validate explicit rejection of malformed and wrong-game QVMs, and verify source/output cannot overwrite each other.
   Acceptance: fonts, texture/sprite/picture, RES and QVM operations pass the supplied LEVEL5/download corpus; additional supported commands have process-level checks.
5. [ ] Build Windows x64 CLIs with a native Windows CI runner and run synthetic/end-to-end command tests there. Cross-build locally when possible; a successful cross-build is not a Windows runtime test.
   Acceptance: CI reports for both executables; dependency DLLs are packaged only if linked. macOS bundles have no third-party runtime dependency where practical.
6. [x] Run AddressSanitizer/UndefinedBehaviorSanitizer tests and representative format verification. Run OpenQodex on each branch, address actual findings, update README/format matrix/changelog and bug notes.
   Acceptance: review report records any scanner/reviewer limitations; zero test failures required for verified paths.
7. [ ] Package archives with checksums, unpack them in clean directories and run commands from the packaged binaries; publish only converter project branches and releases to the private repository.
   Acceptance: archive contents audited, packaged binaries match tested hashes, Windows/macOS execution evidence attached; no Neonix application, proprietary game fixture, executable or PDB in release assets.

## Verification scope
IGI 1: recursively inventory Downloads/IGI including igi.exe as reverse engineering evidence, then verify one actual example per format and materially different variant, using additional legacy corpus tests as supplemental evidence. IGI 2: LEVEL5, supplied sprites/fonts/textures/menusystem resources and official-demo fixtures already acquired. Missing fixture classes, unsupported commands, non-identical structural rewrites and skips must appear explicitly in the report. Do not claim complete GConv 3.5 parity or game-engine acceptance based solely on a round trip.

## Risks
- Existing documentation incorrectly equates IGI 1 and IGI 2 QVM versions: correct it using actual headers.
- Native GConv currently lacks sound, VQ, mipmap generation, animation/model and recipe APIs: implement needed paths or record the exact incomplete scope; no placeholder conversion.
- Windows CI credentials/network and Ghidra service may need environment access; record actual errors and continue independent local work.
- OpenQodex only starts Claude/Codex reviewers, while repo instructions allow only OpenRouter free models: use deterministic scanners and its current-agent fallback when supported; do not silently start a paid model.
- Game execution validation needs the game runtime; report converter verification separately from in-game acceptance.

## Expanded requirement
Support all discovered IGI 1 and IGI 2 game asset types and their applicable conversions. Inventory archive contents as well as standalone files; include model, animation, audio, terrain, graph, mapping and metadata operations. Every missing read/export/rebuild path is implementation work, never counted as a pass.

## Acceptance clarification
User requests complete format coverage with one representative real file per format (plus materially different variants), rather than repeated samples of the same format. Maintain one repository and two game-specific branches; publish a combined release containing igi1conv and igi2conv for Windows and macOS. Existing bulk test evidence is supplementary.
