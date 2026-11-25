#pragma once
#include "mef_native.h"
#include "mef_parser.h"
#include <string>
#include <vector>

namespace MefExporter {

// OBJ cannot represent a skeleton, bone weights or animation.  Exporting a
// rigged MEF (modelType 1) straight to OBJ therefore silently degrades it to a
// static mesh.  These entry points FAIL CLOSED for rigged input; pass
// allowGeometryOnly = true only when that loss is explicitly intended.
bool ExportToObj(const ParsedGeometry &geometry, const std::string &outpath,
                 const std::string &datPath = "", bool allowGeometryOnly = false);
bool ExportToObjGeometryOnly(const ParsedGeometry &geometry, const std::string &outpath,
                             const std::string &datPath = "");

// Detect whether an output path's format can carry skeleton + weights.
// Binary/text MEF and the .rig.json sidecar are supported; OBJ/MTL/PNG/etc.
// are geometry-only.
bool FormatSupportsRigging(const std::string &outputPath);

// Explicit, fail-closed rig-preserving export for IGI1 modelType 1 meshes.
// Writes <outDir>/<modelStem>.rig.json containing the restored bone hierarchy
// (name/parent/pivot) and every vertex's boneIndex/weight/localVertexId.
// The companion OBJ remains geometry-only and is annotated as such.
struct RigExportOptions {
    std::string archetype;  // optional explicit BoneRigType name (case-insensitive)
    bool writeObj = true;   // also emit the geometry-only OBJ + MTL
};

struct RigExportResult {
    bool ok = false;
    std::string error;         // populated when ok == false (fail closed)
    std::string archetype;     // "JonesCinematic" | "StandardSoldier" | ...
    std::string sidecarPath;   // written .rig.json path
    size_t boneCount = 0;
    size_t weightCount = 0;
};

RigExportResult ExportRiggedMef(const ParsedGeometry &geometry,
                                const std::string &modelStem,
                                const std::string &outDir,
                                const RigExportOptions &options = {});

// Export text-format MEFObjects (from MEFParser) to OBJ + MTL.
// Uses DiffuseTMap paths from materials when available.  Fails closed for
// text MEFs carrying Type 1 bone weights unless allowGeometryOnly is set.
bool ExportTextMefToObj(const std::vector<MEFObject> &objects,
                        const std::string &outpath,
                        bool allowGeometryOnly = false);

// Export to a self-contained bundle folder: outDir/modelStem/{modelStem.obj,
// modelStem.mtl, mat_0.tga … mat_N.tga}.  Resolves real texture names from
// datPath and converts .tex files found in texDir to TGA.
bool ExportToObjBundle(const ParsedGeometry &geometry,
                       const std::string &modelStem,
                       const std::string &outDir,
                       const std::string &datPath,
                       const std::string &texDir,
                       bool skipObj = false,
                       bool allowGeometryOnly = false);

// Export a pre-merged geometry to a bundle folder using already-resolved texNames.
// texNames[N] is the filename of the TGA for material slot N (already written to bundleDir).
// Writes OBJ + MTL only; the caller is responsible for texture files.
bool ExportMergedToObjBundle(const ParsedGeometry &geometry,
                              const std::string &modelStem,
                              const std::string &outDir,
                              const std::vector<std::string> &texNames,
                              bool skipObj = false,
                              bool allowGeometryOnly = false);

// Export binary ParsedGeometry to text-based MEF format (parsed by MEFParser).
bool ExportToMefAscii(const ParsedGeometry &geometry,
                      const std::string &outpath);

// Write/read the binary sidecar that preserves all opaque ILFF chunks for
// lossless text→binary round-trips.  The sidecar path is <textPath>.extra.
bool WriteMefSidecar(const std::vector<ParsedGeometry::RawChunk>& chunks,
                     const std::string &sidecarPath);

std::vector<ParsedGeometry::RawChunk> ReadMefSidecar(const std::string &sidecarPath);

} // namespace MefExporter
