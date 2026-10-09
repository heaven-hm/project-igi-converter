# Native converter research evidence

The installed real Ghidra Headless MCP backend (Ghidra 12.1.2 / PyGhidra 3.1.0) opened the supplied IGI 2 gconvapi.dll and decompiled the exported gcExecuteScript function at 0x10007110 after seed disassembly. Radare2 independently disassembled that same export from the original DLL. The recorded DLL SHA-256 identifies the analyzed input.

The initial TCP import sessions stalled. A single in-process MCP batch using the existing project succeeded; the actual calls and structured results are saved here. No fake backend was used. Ghidra function creation modifies only the temporary analysis database.

Layout implementation additionally follows actual IGI 1 files, official IGI 2 demo assets, retail LEVEL5 assets, and the independently documented game-specific structures in [Artiom Rotari's igipy](https://github.com/artiom-rotari/igipy). Related-game PDB symbols are hints, not IGI 1 layout authority.

This establishes research provenance, not a claim that every original GConv API has been ported.
