# GConv integration and parity evidence

The shared native C++ modules in `../native/` are integrated into IGI2Conv.
Reference branches: [macos-gconv](https://github.com/heaven-hm/project-igi-converter/tree/macos-gconv) and [feature/igi2conv-cross-platform](https://github.com/heaven-hm/project-igi-converter/tree/feature/igi2conv-cross-platform).

The GitHub `macos-gconv` C++17 implementation supplies the native resource, font, texture and IGI2 script codecs integrated here. It explicitly documents incomplete GConv 3.5 parity. The local Python investigation host in `Downloads/Neonix-IGI-Workbench-updated/macos-gconv` instead executes the original PE32 DLL through Unicorn; it is an emulation reference, not a native C++ port. Earlier regex recipe adapters are not evidence of interpreter parity.

Original gconvapi.dll SHA-256: `f76ce99b51402bba2b93b43135c1e6a622545e82b895f810309dad6fbaf4b6de`. Static inspection identifies 29 exports and 78 imports. Saved export/import/string evidence is under `.qa/2026-10-10/gconv/` at the repository root. String identifiers are candidates, not proof of semantics.

Current shared-build format checks: IGI1 33/33 and IGI2 38/38. These measure representative format roundtrips and edited-value persistence. They do not establish original recipe/API parity.

Pending original-engine parity includes script expression evaluation and command sequencing, VQ and additional mipmap format/options, font creation and scanning, full model authoring, and callback/option semantics. Each requires differential checks against the original tool rather than adding aliases with guessed behavior.

GUI/CrossOver checks and self-contained release packaging remain separate validation requirements.

The original GConv 3.5 executable now runs successfully under CrossOver with its actual Windows Z: path. Its help confirms case-sensitive options, automatic mipmap/VQ defaults and recipe execution. A real IGI2 string-resource ExtractResource recipe completed; it reported 16 strings and produced no output files. Native `res extract` writes resource payload files, so those operations must not be described as identical recipe behavior. Resource callback registration and disassembly evidence are recorded separately.

Post-preview work adds shared `tex make INPUT OUTPUT --mipmaps`. Square and rectangular power-of-two 32-bit textures match original GConv complete files byte-for-byte with AutoMipMap=ON, AutoVQ=OFF and Dither=OFF; Windows and macOS outputs agree. Rectangular chains stop when either dimension reaches one. The filter rounds each 2x2 channel mean with `(sum+2)/4`. Both LOOP11 readers now allow sixteen levels, bounded by 16-bit dimensions. Non-power-of-two dimensions are rejected. This feature is not yet in the published Qt preview assets. The original recipe registration inventory contains 112 callbacks; it is discovery evidence, not semantic parity.
