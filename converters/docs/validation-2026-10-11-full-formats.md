# Extracted r4 format coverage — 2026-10-11

These checks run the executables extracted from the published r4 ZIPs, not a development build. Windows checks use CrossOver on macOS, not a native Windows host. The reusable harness checks conversion round trips, preservation of resource payloads and supported edits, and rejection of destructive or invalid input.

| Application | macOS ARM64 | Windows x64 through CrossOver |
| --- | --- | --- |
| IGI1Conv | 33 passed, 0 failed | 33 passed, 0 failed |
| IGI2Conv | 37 passed, 1 invalid fixture | 37 passed, 1 invalid fixture |

IGI1 covers all 23 listed extensions and IGI2 exercises all 22 listed extensions. These counts are checks, not distinct formats. IGI2 animation remains unvalidated in this fresh matrix: the available IFF sample belongs to IGI1 and is correctly rejected by the IGI2 parser because it lacks the ILFF/MINA animation container. It is not evidence of an IGI2 converter regression. A valid independent IGI2 animation sample is required before claiming complete coverage.

Most samples come from the user's retail IGI1 and IGI2 assets. Shared PIC uses an IGI2 resource, the additional IGI2 QVM 8.6 sample is derived, and the BMP intermediary is decoded from a retail JPEG. AVI and AFP checks establish byte-preserving copy only. Proprietary samples and raw logs remain in local .qa storage and are not distributed.

The Windows harness uses output files rather than captured pipes because Wine helper processes can keep pipe handles open after a converter exits. An optional launcher translates canonical macOS paths to Windows Z: paths. Reports hash the actual converter executable.

GUI playback, texture brightness, script editing, font preferences, logging and dependency packaging are documented separately in [the desktop follow-up](validation-2026-10-11.md). This CLI matrix does not establish every format's GUI workflow or acceptance of every rebuilt asset by the game. Complete original GConv recipe and output-variant parity remains unfinished.
