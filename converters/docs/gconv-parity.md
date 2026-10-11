# GConv integration and parity evidence

The shared native C++ modules in `../native/` are integrated into IGI2Conv.
Reference branches: [macos-gconv](https://github.com/heaven-hm/project-igi-converter/tree/macos-gconv) and [feature/igi2conv-cross-platform](https://github.com/heaven-hm/project-igi-converter/tree/feature/igi2conv-cross-platform).

The GitHub `macos-gconv` C++17 implementation supplies the native resource, font, texture and IGI2 script codecs integrated here. It explicitly documents incomplete GConv 3.5 parity. The local Python investigation host in `Downloads/Neonix-IGI-Workbench-updated/macos-gconv` instead executes the original PE32 DLL through Unicorn; it is an emulation reference, not a native C++ port. Earlier regex recipe adapters are not evidence of interpreter parity.

Original gconvapi.dll SHA-256: `f76ce99b51402bba2b93b43135c1e6a622545e82b895f810309dad6fbaf4b6de`. Static inspection identifies 29 exports and 78 imports. Saved export/import/string evidence is under `.qa/2026-10-10/gconv/` at the repository root. String identifiers are candidates, not proof of semantics.

Current shared-build format checks: IGI1 33/33 and IGI2 38/38. These measure representative format roundtrips and edited-value persistence. They do not establish original recipe/API parity.

Pending original-engine parity includes script expression evaluation and command sequencing, VQ and additional mipmap format/options, font creation and scanning, full model authoring, and callback/option semantics. Each requires differential checks against the original tool rather than adding aliases with guessed behavior.

GUI/CrossOver checks and self-contained release packaging remain separate validation requirements.

The original GConv 3.5 executable now runs successfully under CrossOver with its actual Windows Z: path. Its help confirms case-sensitive options, automatic mipmap/VQ defaults and recipe execution. A real IGI2 string-resource ExtractResource recipe completed; it reported 16 strings and produced no output files. Native `res extract` writes resource payload files, so those operations must not be described as identical recipe behavior. Resource callback registration and disassembly evidence are recorded separately.

Post-preview work adds shared `tex make INPUT OUTPUT --mipmaps`. Square and rectangular power-of-two 32-bit textures match original GConv complete files byte-for-byte with AutoMipMap=ON, AutoVQ=OFF and Dither=OFF; Windows and macOS outputs agree. Rectangular chains stop when either dimension reaches one. The filter rounds each 2x2 channel mean with `(sum+2)/4`. Both LOOP11 readers now allow sixteen levels, bounded by 16-bit dimensions. Non-power-of-two dimensions are rejected. This feature is included in the `v2026.10.10-qt-desktop-r2` preview assets. The original recipe registration inventory contains 112 callbacks; it is discovery evidence, not semantic parity.

## Original PC versus Dreamcast texture probe — 2026-10-11

An isolated original GConv 3.5 run used generated 32×32 and 64×64 BGRA gradient
TGAs with AutoMipMap=ON, AutoVQ=ON, ConversionMode=ARGB1555, Dither=OFF and
UpToDateCheck=OFF. All four runs exited successfully. PC_Glide produced LOOP11
mode 2, with 2080 and 8224 byte files respectively (header plus one 16-bit
bitmap). Dreamcast produced mode 0xc4 / 2764 bytes at 32×32 and mode 0xd4 /
3446 bytes at 64×64. Thus the advertised AutoVQ and mipmap options do not
produce the same layouts on these two output platforms. Native IGI2 extraction
accepts the PC_Glide output; it explicitly rejects the Dreamcast 64×64 output
with its unsupported-VQ error and produces no images. Evidence is recorded
under .qa/2026-10-11/gconv-vq-investigation, including original command logs,
input fixtures, output headers and native-reader results. This identifies a
specific compatibility gap; it is not a completed VQ decoder/compressor port.
