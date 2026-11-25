#include "mef_compiler.h"
#include "mef_exporter.h"
#include "mef_parser.h"
#include "../logger.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;

namespace MefCompiler {

namespace {

// ---------------------------------------------------------------------------
// Binary write helpers (little-endian)
// ---------------------------------------------------------------------------

static void WriteU8(std::vector<uint8_t>& out, uint8_t v) {
    out.push_back(v);
}

static void WriteU16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

static void WriteI16(std::vector<uint8_t>& out, int16_t v) {
    WriteU16(out, static_cast<uint16_t>(v));
}

static void WriteU32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

static void WriteFloat(std::vector<uint8_t>& out, float f) {
    uint32_t bits = 0;
    std::memcpy(&bits, &f, sizeof(float));
    WriteU32(out, bits);
}

static void WriteZeros(std::vector<uint8_t>& out, size_t count) {
    out.insert(out.end(), count, 0u);
}

static void WriteTag(std::vector<uint8_t>& out, const char tag[4]) {
    out.insert(out.end(), tag, tag + 4);
}

static void PadTo4(std::vector<uint8_t>& out) {
    while (out.size() % 4 != 0)
        out.push_back(0u);
}

// ---------------------------------------------------------------------------
// Chunk header: FourCC | dataSize | alignment=4 | skip
//   skip = 0 means this is the last chunk (no following chunk).
//   skip != 0: offset from chunk start to next chunk start.
//   For non-last chunks: skip = 16 (header) + aligned(dataSize).
// ---------------------------------------------------------------------------
static void WriteChunkHeader(std::vector<uint8_t>& out,
                              const char fourcc[4],
                              uint32_t dataSize,
                              uint32_t skip) {
    WriteTag(out, fourcc);
    WriteU32(out, dataSize);
    WriteU32(out, 4); // alignment
    WriteU32(out, skip);
}

// ---------------------------------------------------------------------------
// Per-vertex combined data:
//   Type 0 — 32 bytes: pos + normal + uv
//   Type 1 — 40 bytes: pos + normal + uv + weight + localVertId/boneIdx
//   Type 3 — 40 bytes: pos + normal + uv0 + uv1 (lightmap)
// ---------------------------------------------------------------------------
struct BinaryVertex {
    float    px{0}, py{0}, pz{0};
    float    nx{0}, ny{0}, nz{1};
    float    u{0},  v{0};
    // Type 1 bone fields / Type 3 lightmap UV1
    float    weight{1.0f};      // Type 1: bone weight, Type 3: uv1.u
    uint16_t localVertexId{0};  // Type 1 only
    uint16_t boneIndex{0};      // Type 1 only
    float    uv1_v{0.0f};       // Type 3: lightmap V
};

// ---------------------------------------------------------------------------
// Build vertex buffer, deduplicating on (vertex_index, normal_index) pairs
// so that the binary vertex has exactly one normal.
// ---------------------------------------------------------------------------
static std::vector<BinaryVertex> BuildVertices(
    const MEFObject& obj,
    const std::vector<MEFFace>& faces,
    std::vector<uint32_t>& outRemappedA,
    std::vector<uint32_t>& outRemappedB,
    std::vector<uint32_t>& outRemappedC)
{
    std::vector<BinaryVertex> verts;
    std::map<std::pair<int,int>, uint32_t> cache; // (vi, ni) -> output idx

    outRemappedA.reserve(faces.size());
    outRemappedB.reserve(faces.size());
    outRemappedC.reserve(faces.size());

    auto getVert = [&](int vi, int ni) -> uint32_t {
        auto key = std::make_pair(vi, ni);
        auto it = cache.find(key);
        if (it != cache.end()) return it->second;

        BinaryVertex bv;
        if (vi >= 0 && vi < static_cast<int>(obj.vertices.size())) {
            bv.px = obj.vertices[vi][0];
            bv.py = obj.vertices[vi][1];
            bv.pz = obj.vertices[vi][2];
        }
        if (ni >= 0 && ni < static_cast<int>(obj.normals.size())) {
            bv.nx = obj.normals[ni][0];
            bv.ny = obj.normals[ni][1];
            bv.nz = obj.normals[ni][2];
        }
        // UV: use vi as UV index (matches ExportToMefAscii output)
        if (vi >= 0 && vi < static_cast<int>(obj.uvs.size())) {
            const auto& uv = obj.uvs[vi];
            if (uv.size() >= 2) { bv.u = uv[0]; bv.v = uv[1]; }
        }
        // Bone data: bone_vertices are in-order by vertex index
        if (vi >= 0 && vi < static_cast<int>(obj.bone_vertices.size())) {
            bv.weight        = obj.bone_vertices[vi].weight;
            bv.localVertexId = obj.bone_vertices[vi].local_vert_id;
            bv.boneIndex     = obj.bone_vertices[vi].bone_index;
        }

        uint32_t idx = static_cast<uint32_t>(verts.size());
        verts.push_back(bv);
        cache[key] = idx;
        return idx;
    };

    for (const auto& face : faces) {
        outRemappedA.push_back(getVert(face.v0, face.n0));
        outRemappedB.push_back(getVert(face.v1, face.n1));
        outRemappedC.push_back(getVert(face.v2, face.n2));
    }

    return verts;
}

// ---------------------------------------------------------------------------
// Compute bounding sphere radius from origin
// ---------------------------------------------------------------------------
static float ComputeRadius(const std::vector<BinaryVertex>& verts) {
    float r2 = 0.0f;
    for (const auto& v : verts) {
        float d2 = v.px * v.px + v.py * v.py + v.pz * v.pz;
        if (d2 > r2) r2 = d2;
    }
    return std::sqrt(r2);
}

// ---------------------------------------------------------------------------
// Build HSEM chunk data (156 bytes, marks file as IGI1)
// ---------------------------------------------------------------------------
static std::vector<uint8_t> BuildHsem(uint32_t numFaces,
                                       uint32_t numVerts,
                                       uint32_t numBlocks,
                                       uint16_t numAttachments,
                                       float    radius,
                                       uint32_t modelType = 0)
{
    std::vector<uint8_t> d;
    d.reserve(156);

    WriteFloat(d, 0.10f);         // _V = version 0.10 required by engine (4)
    WriteZeros(d, 28);            // Date[7] (28)
    WriteU32(d, modelType);       // model_type: 0=rigid, 1=bone (4) [offset 32]
    WriteZeros(d, 12);            // reserved_0[3] (12)
    WriteZeros(d, 48);            // vectors[12] (48)
    WriteU32(d, numFaces);        // num_r_faces (4)
    WriteU32(d, numVerts);        // num_r_verts (4)
    WriteU32(d, numBlocks);       // num_r_buffer (4)
    WriteZeros(d, 12);            // sum_c_faces/verts/buffer (12)
    WriteFloat(d, radius);        // model_radius (4)
    WriteU16(d, 0u);              // num_mverts (2)
    WriteU16(d, numAttachments);  // num_attachments (2)
    WriteU16(d, 0u);              // num_pverts (2)
    WriteU16(d, 0u);              // num_pfaces (2)
    WriteU16(d, 0u);              // num_portals (2)
    WriteZeros(d, 22);            // rs[22] (22)

    return d; // 4+28+4+12+48+4+4+4+12+4+2+2+2+2+2+22 = 156
}

// ---------------------------------------------------------------------------
// Build D3DR chunk data:
//   Type 0 — 16 bytes: unknown, numFaces, numMeshes, numVerts
//   Type 1 — 24 bytes: unknown, numFaces, numMeshes(=0), verts0, verts1, numVerts
//   Type 3 — 20 bytes: unknown, unknown, numFaces, numMeshes, numVerts
//   numMeshes=0 forces the parser to use packed-DNER path for Type 1.
// ---------------------------------------------------------------------------
static std::vector<uint8_t> BuildD3dr(uint32_t numFaces,
                                       uint32_t numMeshes,
                                       uint32_t numVerts,
                                       uint32_t modelType = 0)
{
    std::vector<uint8_t> d;
    if (modelType == 1) {
        d.reserve(24);
        WriteU32(d, 0u);        // unknown
        WriteU32(d, numFaces);
        WriteU32(d, 0u);        // numMeshes=0 → packed DNER path
        WriteU32(d, numVerts);  // verts0
        WriteU32(d, 0u);        // verts1
        WriteU32(d, numVerts);  // numVerts
    } else if (modelType == 3) {
        d.reserve(20);
        WriteU32(d, 0u);        // unknown
        WriteU32(d, 0u);        // unknown
        WriteU32(d, numFaces);
        WriteU32(d, numMeshes);
        WriteU32(d, numVerts);
    } else {
        d.reserve(16);
        WriteU32(d, 0u);        // unknown
        WriteU32(d, numFaces);
        WriteU32(d, numMeshes);
        WriteU32(d, numVerts);
    }
    return d;
}

// ---------------------------------------------------------------------------
// Build XTRV chunk data:
//   Type 0 — 32 bytes/vertex: px,py,pz, nx,ny,nz, u,v
//   Type 1 — 40 bytes/vertex: px,py,pz, nx,ny,nz, u,v, weight, localVertId, boneIdx
// ---------------------------------------------------------------------------
static std::vector<uint8_t> BuildXtrv(const std::vector<BinaryVertex>& verts,
                                       uint32_t modelType = 0) {
    std::vector<uint8_t> d;
    const size_t stride = (modelType == 1 || modelType == 3) ? 40 : 32;
    d.reserve(verts.size() * stride);
    for (const auto& v : verts) {
        WriteFloat(d, v.px);
        WriteFloat(d, v.py);
        WriteFloat(d, v.pz);
        WriteFloat(d, v.nx);
        WriteFloat(d, v.ny);
        WriteFloat(d, v.nz);
        WriteFloat(d, v.u);
        WriteFloat(d, v.v);
        if (modelType == 1) {
            WriteFloat(d, v.weight);
            WriteU16(d, v.localVertexId);
            WriteU16(d, v.boneIndex);
        } else if (modelType == 3) {
            WriteFloat(d, v.weight);  // uv1.u stored in weight field
            WriteFloat(d, v.uv1_v);  // uv1.v
        }
    }
    return d;
}

// ---------------------------------------------------------------------------
// Build DNER chunk data (packed).
// Groups faces by material and emits one render block per material.
//
// Type 0/1 block layout (28-byte header + uint16 indices):
//   bytes  0-11: px,py,pz (float, zeroed)
//   bytes 12-13: indexCount (uint16) = numFacesInBlock * 3
//   bytes 14-15: nextoffs (int16): indexCount*2 for non-last, -1 for last
//   bytes 16-17: td / materialSlot (int16)
//   bytes 18-19: offVerts (uint16): min vertex index in block
//   bytes 20-21: numVerts (uint16): max - min + 1
//   bytes 22-23: opacity (uint16): 0 = opaque
//   bytes 24-27: eflame, mshine, scolor, opacitd (uint8 x4)
//   bytes 28+:   uint16 indices[indexCount] (local, relative to offVerts)
//
// Type 3 block layout (32-byte header + uint16 indices):
//   bytes  0-11: px,py,pz (float, zeroed)
//   bytes 12-13: indexCount (uint16)
//   bytes 14-15: nextoffs (int16): indexCount*2 for non-last, -1 for last
//   bytes 16-17: materialSlot (int16) — extra field for Type 3
//   bytes 18-19: offVerts (uint16)
//   bytes 20-21: numVerts (uint16)
//   bytes 22-23: opacity (uint16)
//   bytes 24-27: eflame, mshine, scolor, opacitd (uint8 x4)
//   bytes 28-31: padding/zero (4 bytes)
//   bytes 32+:   uint16 indices[indexCount]
// ---------------------------------------------------------------------------
static std::vector<uint8_t> BuildDner(
    const std::vector<MEFFace>& faces,
    const std::vector<uint32_t>& remA,
    const std::vector<uint32_t>& remB,
    const std::vector<uint32_t>& remC,
    int numMaterials,
    uint32_t modelType = 0)
{
    // Collect sorted unique material indices
    std::vector<int> matList;
    {
        std::map<int, bool> seen;
        for (const auto& f : faces) seen[f.material_index] = true;
        for (auto& [k, _] : seen) matList.push_back(k);
    }
    if (matList.empty()) matList.push_back(0);

    const uint32_t headerSize = (modelType == 3) ? 32u : 28u;
    std::vector<uint8_t> d;

    for (size_t bi = 0; bi < matList.size(); ++bi) {
        const int mat = matList[bi];
        const bool isLast = (bi == matList.size() - 1);

        // Gather face indices for this material
        std::vector<size_t> faceIdxs;
        for (size_t fi = 0; fi < faces.size(); ++fi) {
            if (faces[fi].material_index == mat) faceIdxs.push_back(fi);
        }
        if (faceIdxs.empty()) continue;

        // Find vertex range
        uint32_t minV = UINT32_MAX, maxV = 0;
        for (size_t fi : faceIdxs) {
            minV = std::min(minV, remA[fi]);
            minV = std::min(minV, remB[fi]);
            minV = std::min(minV, remC[fi]);
            maxV = std::max(maxV, remA[fi]);
            maxV = std::max(maxV, remB[fi]);
            maxV = std::max(maxV, remC[fi]);
        }

        const uint16_t offVerts  = static_cast<uint16_t>(minV);
        const uint16_t numVerts  = static_cast<uint16_t>(maxV - minV + 1);
        const uint16_t indexCount = static_cast<uint16_t>(faceIdxs.size() * 3);

        // Write block header (28 bytes for Type 0/1, 32 bytes for Type 3)
        WriteFloat(d, 0.0f); WriteFloat(d, 0.0f); WriteFloat(d, 0.0f); // px,py,pz (patched later from sidecar)
        WriteU16(d, indexCount);
        WriteI16(d, isLast ? int16_t(-1) : static_cast<int16_t>(indexCount * 2));  // nextoffs = index byte size
        if (modelType == 3) {
            WriteI16(d, static_cast<int16_t>(mat));   // materialSlot (extra field for Type 3)
        }
        WriteI16(d, static_cast<int16_t>(mat));           // td = material slot (for Type 0/1) / offVerts-related (for Type 3 this is duplicate slot)
        WriteU16(d, offVerts);
        WriteU16(d, numVerts);
        WriteU16(d, 0u);  // opacity (0 = opaque)
        WriteU8 (d, 0u);  // eflame
        WriteU8 (d, 0u);  // mshine
        WriteU8 (d, 0u);  // scolor
        WriteU8 (d, 0u);  // opacitd
        if (modelType == 3) {
            WriteU32(d, 0u);  // padding for Type 3 (4 bytes)
        }

        // Write local face indices
        for (size_t fi : faceIdxs) {
            WriteU16(d, static_cast<uint16_t>(remA[fi] - minV));
            WriteU16(d, static_cast<uint16_t>(remB[fi] - minV));
            WriteU16(d, static_cast<uint16_t>(remC[fi] - minV));
        }
    }

    // Pad to 4-byte boundary
    while (d.size() % 4 != 0) d.push_back(0u);

    return d;
}

// ---------------------------------------------------------------------------
// Build TAMC chunk data (12 bytes per material, IGI1 format)
// One record per material slot 0..numMaterials-1
// ---------------------------------------------------------------------------
static std::vector<uint8_t> BuildTamc(const std::vector<MEFMaterial>& mats,
                                       int numSlots)
{
    std::vector<uint8_t> d;
    d.reserve(static_cast<size_t>(numSlots) * 12);
    for (int i = 0; i < numSlots; ++i) {
        // Find material with this index slot
        float opacity = 1.0f;
        if (i < static_cast<int>(mats.size())) {
            opacity = mats[i].has_collision ? 0.0f : 1.0f;
        }
        WriteFloat(d, opacity);   // opacity (4)
        WriteU16(d, 0u);          // portal (2)
        WriteI16(d, -1);          // diffuse (-1 = none) (2)
        WriteI16(d, static_cast<int16_t>(i)); // mat_id (2)
        WriteU16(d, 0u);          // unknown (2)
    }
    return d;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Sidecar-aware compile with custom topology support.
//
// A sidecar carries all opaque ILFF chunks (HPSC skinning, XTVC/ECFC collision,
// TAMC materials, attachments …) that cannot be reconstructed from the ASCII
// text.  When the text Face() list differs from the sidecar's original render
// mesh we rebuild only the render geometry (D3DR / XTRV / DNER) from the text
// and keep every other chunk verbatim.
//
// Everything in this path is fail-closed: if the sidecar is internally
// inconsistent, uses a different model type, or the text refers to vertices
// the sidecar cannot address, compilation fails instead of emitting a MEF
// whose skinning/collision metadata no longer matches its geometry.
// ---------------------------------------------------------------------------
namespace {

struct SidecarTopology {
    ParsedGeometry::RawChunk* hsem = nullptr;
    ParsedGeometry::RawChunk* d3dr = nullptr;
    ParsedGeometry::RawChunk* dner = nullptr;
    ParsedGeometry::RawChunk* xtrv = nullptr;
    uint32_t faceCount = 0;
    uint32_t renderVerts = 0;
    uint32_t xtrvVerts = 0;
};

static uint32_t SidecarReadU32(const std::vector<uint8_t>& d, size_t off) {
    if (off + 4 > d.size()) return 0;
    return static_cast<uint32_t>(d[off]) |
           (static_cast<uint32_t>(d[off+1]) << 8) |
           (static_cast<uint32_t>(d[off+2]) << 16) |
           (static_cast<uint32_t>(d[off+3]) << 24);
}

static uint16_t SidecarReadU16(const std::vector<uint8_t>& d, size_t off) {
    if (off + 2 > d.size()) return 0;
    return static_cast<uint16_t>(d[off]) |
           (static_cast<uint16_t>(d[off+1]) << 8);
}

static void SidecarWriteU32(std::vector<uint8_t>& d, size_t off, uint32_t v) {
    if (off + 4 > d.size()) return;
    d[off]   = static_cast<uint8_t>(v & 0xFF);
    d[off+1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    d[off+2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    d[off+3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

static void WriteFloatAt(std::vector<uint8_t>& d, size_t off, float f) {
    uint32_t bits = 0;
    std::memcpy(&bits, &f, sizeof(bits));
    SidecarWriteU32(d, off, bits);
}

static bool SidecarFail(const std::string& msg) {
    Logger::Get().Log(LogLevel::ERR,
        "[MefCompiler] Sidecar validation failed: " + msg);
    return false;
}

static bool ReadSidecarTopology(std::vector<ParsedGeometry::RawChunk>& sidecar,
                                uint32_t modelType, SidecarTopology& out) {
    for (auto& rc : sidecar) {
        if      (rc.isTag("HSEM") && !out.hsem) out.hsem = &rc;
        else if (rc.isTag("D3DR") && !out.d3dr) out.d3dr = &rc;
        else if (rc.isTag("DNER") && !out.dner) out.dner = &rc;
        else if (rc.isTag("XTRV") && !out.xtrv) out.xtrv = &rc;
    }
    if (!out.hsem || !out.d3dr || !out.dner || !out.xtrv)
        return SidecarFail("sidecar is missing HSEM, D3DR, DNER or XTRV");

    const size_t minD3dr = (modelType == 1) ? 24u : (modelType == 3) ? 20u : 16u;
    if (out.hsem->data.size() < 156)
        return SidecarFail("HSEM chunk is too short");
    if (out.d3dr->data.size() < minD3dr)
        return SidecarFail("D3DR chunk is too short for this model type");

    if (SidecarReadU32(out.hsem->data, 32) != modelType)
        return SidecarFail("sidecar model type does not match ModelType() in the text file");

    const size_t faceOff = (modelType == 3) ? 8u : 4u;
    out.faceCount = SidecarReadU32(out.d3dr->data, faceOff);

    // Do not trust the D3DR vertex-count slots: retail IGI1 MEFs and this
    // compiler's writer place render/skinning counts in different fields.
    // XTRV size is authoritative for the total vertex count; DNER block
    // extents are authoritative for the render (addressable) vertex count.
    const uint32_t stride = (modelType == 1 || modelType == 3) ? 40u : 32u;
    if (out.xtrv->data.size() % stride != 0)
        return SidecarFail("XTRV chunk size is not a multiple of the vertex stride");
    out.xtrvVerts = static_cast<uint32_t>(out.xtrv->data.size() / stride);
    if (out.faceCount == 0 || out.xtrvVerts == 0)
        return SidecarFail("sidecar has zero faces or vertices");
    if (out.xtrvVerts > 65536u)
        return SidecarFail("XTRV vertex count exceeds the 16-bit DNER index range");

    if (SidecarReadU32(out.hsem->data, 96) != out.faceCount ||
        SidecarReadU32(out.hsem->data, 100) != out.xtrvVerts)
        return SidecarFail("HSEM render counts disagree with D3DR/XTRV");

    return true;
}

// Walk the packed DNER blocks of the sidecar, verify they describe exactly the
// face count advertised by D3DR, and derive the render vertex count from the
// block extents (offVerts + numVerts).  This also guarantees that the block
// header copy below never reads past the chunk.
static bool ValidateSidecarDner(const std::vector<uint8_t>& dner,
                                uint32_t headerSize, uint32_t expectedFaces,
                                uint32_t totalVerts, uint32_t& outRenderVerts) {
    size_t p = 0;
    uint32_t faces = 0;
    uint32_t maxVertex = 0;
    const size_t offVertsOff = (headerSize == 32u) ? 20u : 18u;
    const size_t numVertsOff = (headerSize == 32u) ? 22u : 20u;
    while (p + headerSize <= dner.size()) {
        const uint16_t indexCount = SidecarReadU16(dner, p + 12);
        if (indexCount == 0 || indexCount % 3 != 0)
            return SidecarFail("DNER block has a non-triangle index count");
        const uint32_t stride = (headerSize + indexCount * 2u + 3u) & ~3u;
        if (p + stride > dner.size())
            return SidecarFail("DNER block extends past the end of the chunk");
        int16_t next = 0;
        std::memcpy(&next, dner.data() + p + 14, 2);
        const uint32_t blockEnd = static_cast<uint32_t>(
            SidecarReadU16(dner, p + offVertsOff)) +
            SidecarReadU16(dner, p + numVertsOff);
        maxVertex = std::max(maxVertex, blockEnd);
        faces += indexCount / 3;
        p += stride;
        // Type 3 DNER has no linked-list sentinel; the chunk boundary ends the
        // walk.  For types 0/1 -1 marks the final block.
        if (next == int16_t(-1)) break;
    }
    if (faces != expectedFaces)
        return SidecarFail("sidecar DNER has " + std::to_string(faces) +
                           " faces but D3DR advertises " +
                           std::to_string(expectedFaces));
    if (maxVertex == 0 || maxVertex > totalVerts)
        return SidecarFail("DNER vertex range is outside the XTRV vertex buffer");
    outRenderVerts = maxVertex;
    return true;
}

static bool ValidateTextTopology(const MEFObject& obj, uint32_t totalVerts) {
    if (obj.faces.empty())
        return SidecarFail("text file has no Face() entries");
    if (obj.vertices.size() > totalVerts)
        return SidecarFail("text vertices do not fit the sidecar vertex buffer");
    std::map<int, uint32_t> facesPerMaterial;
    for (size_t fi = 0; fi < obj.faces.size(); ++fi) {
        const auto& f = obj.faces[fi];
        if (f.v0 < 0 || f.v1 < 0 || f.v2 < 0 ||
            f.v0 >= static_cast<int>(obj.vertices.size()) ||
            f.v1 >= static_cast<int>(obj.vertices.size()) ||
            f.v2 >= static_cast<int>(obj.vertices.size()))
            return SidecarFail("Face " + std::to_string(fi) +
                               " references a vertex outside the text Vertex() range");
        if (f.material_index < 0 || f.material_index > 32767)
            return SidecarFail("Face " + std::to_string(fi) +
                               " has a material slot outside the DNER int16 range");
        uint32_t& count = facesPerMaterial[f.material_index];
        if (count >= 65535u / 3u)
            return SidecarFail("material " + std::to_string(f.material_index) +
                               " has too many faces for a 16-bit DNER index count");
        ++count;
    }
    return true;
}

// Custom-topology XTRV: start from the sidecar buffer verbatim so that skinning
// weights, lightmap UVs and skinning-only vertices survive, then overwrite the
// first `obj.vertices.size()` entries with the text positions/normals/UVs.
static std::vector<uint8_t> BuildTopologyXtrv(const MEFObject& obj,
                                              const std::vector<uint8_t>& sidecarXtrv,
                                              uint32_t modelType) {
    const size_t stride = (modelType == 1 || modelType == 3) ? 40u : 32u;
    std::vector<uint8_t> d = sidecarXtrv;
    const size_t n = std::min(obj.vertices.size(), d.size() / stride);
    for (size_t i = 0; i < n; ++i) {
        const size_t base = i * stride;
        WriteFloatAt(d, base + 0, obj.vertices[i][0]);
        WriteFloatAt(d, base + 4, obj.vertices[i][1]);
        WriteFloatAt(d, base + 8, obj.vertices[i][2]);
        if (i < obj.normals.size()) {
            WriteFloatAt(d, base + 12, obj.normals[i][0]);
            WriteFloatAt(d, base + 16, obj.normals[i][1]);
            WriteFloatAt(d, base + 20, obj.normals[i][2]);
        }
        if (i < obj.uvs.size() && obj.uvs[i].size() >= 2) {
            WriteFloatAt(d, base + 24, obj.uvs[i][0]);
            WriteFloatAt(d, base + 28, obj.uvs[i][1]);
        }
        // Bytes 32..39 (bone weight / local vertex id / bone index for type 1,
        // lightmap UV1 for type 3) are deliberately left untouched.
    }
    while (d.size() % 4) d.push_back(0);
    return d;
}

// Copy DNER block centers and material properties from the sidecar, matched by
// td (material slot), so custom topology keeps the original shading metadata.
static void PatchDnerProperties(const std::vector<uint8_t>& sidecarDner,
                                std::vector<uint8_t>& newDner,
                                uint32_t headerSize, uint32_t modelType) {
    struct DnerHdr { uint8_t centers[12]; uint8_t matProps[6]; };
    std::map<int16_t, DnerHdr> hdrs;
    size_t p = 0;
    while (p + headerSize <= sidecarDner.size()) {
        DnerHdr h;
        std::memcpy(h.centers,  sidecarDner.data() + p,      12);
        // Type 3's 32-byte header uses bytes 22..27 for numVerts/opacity, so
        // only the type 0/1 layout has material properties there.
        if (modelType != 3)
            std::memcpy(h.matProps, sidecarDner.data() + p + 22, 6);
        else
            std::memset(h.matProps, 0, sizeof(h.matProps));
        const uint16_t ic = SidecarReadU16(sidecarDner, p + 12);
        int16_t next = 0, td = 0;
        std::memcpy(&next, sidecarDner.data() + p + 14, 2);
        std::memcpy(&td,   sidecarDner.data() + p + 16, 2);
        hdrs[td] = h;
        p += (headerSize + ic * 2u + 3u) & ~3u;
        if (next == int16_t(-1)) break;
    }
    p = 0;
    while (p + headerSize <= newDner.size()) {
        const uint16_t ic = SidecarReadU16(newDner, p + 12);
        int16_t next = 0, td = 0;
        std::memcpy(&next, newDner.data() + p + 14, 2);
        std::memcpy(&td,   newDner.data() + p + 16, 2);
        auto it = hdrs.find(td);
        if (it != hdrs.end()) {
            std::memcpy(newDner.data() + p, it->second.centers, 12);
            if (modelType != 3)
                std::memcpy(newDner.data() + p + 22, it->second.matProps, 6);
        }
        p += (headerSize + ic * 2u + 3u) & ~3u;
        if (next == int16_t(-1)) break;
    }
}

} // anonymous namespace

static bool CompileWithSidecar(const MEFObject& obj,
                               const std::string& outBinaryPath,
                               std::vector<ParsedGeometry::RawChunk>& sidecar) {

    const uint32_t modelType = static_cast<uint32_t>(obj.model_type);
    const uint32_t dnerHeaderSize = (modelType == 3) ? 32u : 28u;

    SidecarTopology topo;
    if (!ReadSidecarTopology(sidecar, modelType, topo)) return false;
    if (!ValidateSidecarDner(topo.dner->data, dnerHeaderSize, topo.faceCount,
                             topo.xtrvVerts, topo.renderVerts)) return false;
    if (!ValidateTextTopology(obj, topo.xtrvVerts)) return false;

    // The text Vertex() indices must stay in the sidecar's render-vertex space:
    // HPSC skinning and the skinning-only XTRV tail are keyed by those indices.
    if (obj.vertices.size() != topo.renderVerts) {
        Logger::Get().Log(LogLevel::ERR,
            "[MefCompiler] Vertex() count edits are not supported when compiling with sidecar. "
            "Original sidecar has " + std::to_string(topo.renderVerts) +
            " render vertices, but text file has " +
            std::to_string(obj.vertices.size()) + " Vertex() entries.");
        return false;
    }

    const bool topologyChanged = (obj.faces.size() != topo.faceCount);

    // Without a topology change, type 0/3 keep their XTRV+DNER verbatim because
    // the text format cannot express UV1/lightmap data.  Type 1 rebuilds them
    // from text (BoneVertex lines carry the skinning fields).
    bool useSidecarVerbatim = !topologyChanged && (modelType == 0 || modelType == 3);

    std::vector<uint8_t> newXtrv;
    std::vector<uint8_t> newDner;
    std::vector<uint8_t> newD3dr = topo.d3dr->data;
    uint32_t numVerts = 0;
    const uint32_t numFaces = static_cast<uint32_t>(obj.faces.size());

    if (useSidecarVerbatim) {
        newXtrv = topo.xtrv->data;
        newDner = topo.dner->data;
        while (newXtrv.size() % 4) newXtrv.push_back(0);
        while (newDner.size() % 4) newDner.push_back(0);
        numVerts = static_cast<uint32_t>(newXtrv.size()) /
                   ((modelType == 1 || modelType == 3) ? 40u : 32u);
    } else if (topologyChanged) {
        // Custom topology: keep the sidecar vertex buffer (including its
        // skinning tail and bone/lightmap fields) and rebuild only the face
        // indices, using the text vertex indices directly so that every DNER
        // index still addresses a valid sidecar vertex.
        newXtrv = BuildTopologyXtrv(obj, topo.xtrv->data, modelType);
        numVerts = static_cast<uint32_t>(newXtrv.size()) /
                   ((modelType == 1 || modelType == 3) ? 40u : 32u);

        std::map<int, bool> matSeen;
        for (const auto& f : obj.faces) matSeen[f.material_index] = true;
        const int numMatSlots = static_cast<int>(obj.materials.empty()
                                    ? (matSeen.empty() ? 1 : matSeen.rbegin()->first + 1)
                                    : obj.materials.size());
        std::vector<uint32_t> remA, remB, remC;
        remA.reserve(obj.faces.size());
        remB.reserve(obj.faces.size());
        remC.reserve(obj.faces.size());
        for (const auto& f : obj.faces) {
            remA.push_back(static_cast<uint32_t>(f.v0));
            remB.push_back(static_cast<uint32_t>(f.v1));
            remC.push_back(static_cast<uint32_t>(f.v2));
        }
        newDner = BuildDner(obj.faces, remA, remB, remC, numMatSlots, modelType);
        while (newDner.size() % 4) newDner.push_back(0);

        PatchDnerProperties(topo.dner->data, newDner, dnerHeaderSize, modelType);

        // Only the face count is updated: retail and compiler-generated D3DR
        // chunks disagree on which slot holds the render/skinning vertex
        // counts, and the vertex count is unchanged on this path.
        SidecarWriteU32(newD3dr, (modelType == 3) ? 8u : 4u, numFaces);

        Logger::Get().Log(LogLevel::INFO,
            "[MefCompiler] Custom topology: rebuilt D3DR/DNER for " +
            std::to_string(numFaces) + " faces from text while preserving " +
            std::to_string(sidecar.size()) + " sidecar chunks.");
    } else {
        // Type 1 with unchanged topology: rebuild XTRV + DNER from text.
        std::vector<uint32_t> remA, remB, remC;
        std::vector<BinaryVertex> verts = BuildVertices(obj, obj.faces, remA, remB, remC);
        numVerts = static_cast<uint32_t>(verts.size());

        if (verts.size() > topo.xtrvVerts) {
            return SidecarFail("rebuilt render vertices (" +
                std::to_string(verts.size()) + ") exceed the sidecar XTRV count (" +
                std::to_string(topo.xtrvVerts) + "); skinning data cannot be remapped");
        }

        std::map<int, bool> matSeen;
        for (const auto& f : obj.faces) matSeen[f.material_index] = true;
        const int numMatSlots = static_cast<int>(obj.materials.empty()
                                    ? (matSeen.empty() ? 1 : matSeen.rbegin()->first + 1)
                                    : obj.materials.size());

        newXtrv = BuildXtrv(verts, modelType);
        newDner = BuildDner(obj.faces, remA, remB, remC, numMatSlots, modelType);
        while (newXtrv.size() % 4) newXtrv.push_back(0);
        while (newDner.size() % 4) newDner.push_back(0);

        // Append skinning-only verts from sidecar XTRV (indices beyond render
        // mesh, referenced only by HPSC bone data — not by any DNER face).
        const size_t tailOffset = static_cast<size_t>(topo.renderVerts) *
            ((modelType == 1 || modelType == 3) ? 40u : 32u);
        if (topo.xtrv->data.size() > tailOffset) {
            newXtrv.insert(newXtrv.end(),
                topo.xtrv->data.begin() + static_cast<ptrdiff_t>(tailOffset),
                topo.xtrv->data.end());
            while (newXtrv.size() % 4) newXtrv.push_back(0);
        }
        numVerts = static_cast<uint32_t>(newXtrv.size()) /
                   ((modelType == 1 || modelType == 3) ? 40u : 32u);

        PatchDnerProperties(topo.dner->data, newDner, dnerHeaderSize, modelType);
    }

    // Refresh HSEM geometry counts whenever the render geometry was rebuilt.
    // Verified offsets (156-byte IGI1 HSEM):
    //   96  = num_r_faces, 100 = num_r_verts, 104 = render buffer size.
    // Collision counts at 108/112/116 stay untouched (collision is verbatim).
    if (!useSidecarVerbatim) {
        const uint32_t totalVerts = static_cast<uint32_t>(newXtrv.size()) /
            ((modelType == 1 || modelType == 3) ? 40u : 32u);
        const uint32_t renderBufSize = static_cast<uint32_t>(newD3dr.size()) +
                                       static_cast<uint32_t>(newXtrv.size()) +
                                       static_cast<uint32_t>(newDner.size());
        SidecarWriteU32(topo.hsem->data,  96, numFaces);
        SidecarWriteU32(topo.hsem->data, 100, totalVerts);
        SidecarWriteU32(topo.hsem->data, 104, renderBufSize);
    }

    std::vector<uint8_t> newAtta;
    const size_t attaStride = 68;
    newAtta.resize(obj.attachments.size() * attaStride, 0);
    for (size_t i = 0; i < obj.attachments.size(); ++i) {
        std::memcpy(&newAtta[i * attaStride], &obj.attachments[i], attaStride);
    }

    // Build output chunk list in sidecar order, replacing the geometry chunks.
    struct OutChunk { char fourcc[4]; const std::vector<uint8_t>* data; };
    std::vector<OutChunk> outChunks;
    outChunks.reserve(sidecar.size());

    for (const auto& rc : sidecar) {
        OutChunk oc;
        std::memcpy(oc.fourcc, rc.fourcc, 4);
        if (rc.isTag("XTRV"))
            oc.data = &newXtrv;
        else if (rc.isTag("DNER"))
            oc.data = &newDner;
        else if (rc.isTag("D3DR"))
            oc.data = &newD3dr;
        else if (rc.isTag("ATTA"))
            oc.data = newAtta.empty() ? &rc.data : &newAtta;
        else
            oc.data = &rc.data;
        outChunks.push_back(oc);
    }

    // Compute file size: 20 (ILFF) + Σ(16 + aligned(dataSize))
    uint32_t fileSize = 20;
    for (const auto& oc : outChunks) {
        const uint32_t aligned = (static_cast<uint32_t>(oc.data->size()) + 3) & ~3u;
        fileSize += 16 + aligned;
    }

    std::vector<uint8_t> out;
    out.reserve(fileSize);
    WriteTag(out, "ILFF");
    WriteU32(out, fileSize);
    WriteU32(out, 4u);
    WriteU32(out, 0u);
    WriteTag(out, "OCEM");

    for (size_t ci = 0; ci < outChunks.size(); ++ci) {
        const bool isLast = (ci == outChunks.size() - 1);
        const uint32_t dataSize = static_cast<uint32_t>(outChunks[ci].data->size());
        const uint32_t aligned  = (dataSize + 3) & ~3u;
        const uint32_t skip     = isLast ? 0u : (16u + aligned);
        WriteChunkHeader(out, outChunks[ci].fourcc, dataSize, skip);
        out.insert(out.end(), outChunks[ci].data->begin(), outChunks[ci].data->end());
        for (uint32_t p = dataSize; p < aligned; ++p) out.push_back(0);
    }

    if (out.size() != fileSize) {
        Logger::Get().Log(LogLevel::ERR,
            "[MefCompiler] Sidecar size mismatch: expected " +
            std::to_string(fileSize) + " got " + std::to_string(out.size()));
        return false;
    }

    std::ofstream f(outBinaryPath, std::ios::binary);
    if (!f.is_open()) {
        Logger::Get().Log(LogLevel::ERR,
            "[MefCompiler] Cannot open output file: " + outBinaryPath);
        return false;
    }
    f.write(reinterpret_cast<const char*>(out.data()),
            static_cast<std::streamsize>(out.size()));
    Logger::Get().Log(LogLevel::INFO,
        "[MefCompiler] (sidecar) Compiled to: " + outBinaryPath +
        " (" + std::to_string(out.size()) + " bytes, " +
        std::to_string(numVerts) + " verts, " +
        std::to_string(numFaces) + " faces)");
    return true;
}

bool Compile(const std::string& textMefPath, const std::string& outBinaryPath) {
    // 1. Parse ASCII MEF
    MEFParser parser;
    std::vector<MEFObject> objects;
    try {
        objects = parser.parse_file(textMefPath);
    } catch (const std::exception& e) {
        Logger::Get().Log(LogLevel::ERR,
            std::string("[MefCompiler] Parse error: ") + e.what());
        return false;
    }

    if (objects.empty()) {
        Logger::Get().Log(LogLevel::ERR,
            "[MefCompiler] No objects found in: " + textMefPath);
        return false;
    }

    const MEFObject& obj = objects[0];

    if (obj.vertices.empty() || obj.faces.empty()) {
        Logger::Get().Log(LogLevel::ERR,
            "[MefCompiler] Object has no vertices or faces: " + textMefPath);
        return false;
    }

    // 2. Try sidecar-aware path first (preserves HPSC, XTVC, ECFC, TAMC, etc.)
    std::string sidecarPath = textMefPath;
    size_t extPos = sidecarPath.find_last_of(".");
    std::string ext = (extPos != std::string::npos) ? sidecarPath.substr(extPos) : "";
    if (ext == ".mex") {
        sidecarPath += ".extra";
    } else {
        if (extPos != std::string::npos) sidecarPath = sidecarPath.substr(0, extPos) + ".mex";
        else sidecarPath += ".mex";
    }
    if (fs::exists(sidecarPath)) {
        auto sidecar = MefExporter::ReadMefSidecar(sidecarPath);
        if (!sidecar.empty()) {
            return CompileWithSidecar(obj, outBinaryPath, sidecar);
        }
    }

    // 3. Fallback: minimal 5-chunk output (no sidecar available)
    const uint32_t modelType = static_cast<uint32_t>(obj.model_type);

    std::vector<uint32_t> remA, remB, remC;
    std::vector<BinaryVertex> verts = BuildVertices(obj, obj.faces, remA, remB, remC);

    const uint32_t numVerts  = static_cast<uint32_t>(verts.size());
    const uint32_t numFaces  = static_cast<uint32_t>(obj.faces.size());
    const float    radius    = ComputeRadius(verts);

    std::map<int, bool> matSeen;
    for (const auto& f : obj.faces) matSeen[f.material_index] = true;
    const uint32_t numBlocks = static_cast<uint32_t>(matSeen.empty() ? 1 : matSeen.size());
    const int numMatSlots    = static_cast<int>(obj.materials.empty()
                                ? (matSeen.empty() ? 1 : matSeen.rbegin()->first + 1)
                                : obj.materials.size());

    auto hsemData = BuildHsem(numFaces, numVerts, numBlocks, 0, radius, modelType);
    auto d3drData = BuildD3dr(numFaces, numBlocks, numVerts, modelType);
    auto xtrvData = BuildXtrv(verts, modelType);
    auto dnerData = BuildDner(obj.faces, remA, remB, remC, numMatSlots, modelType);
    auto tamcData = BuildTamc(obj.materials, numMatSlots);

    struct ChunkDesc { const char* tag; std::vector<uint8_t>* data; };
    std::vector<ChunkDesc> chunks;
    chunks.push_back({"HSEM", &hsemData});
    chunks.push_back({"D3DR", &d3drData});
    chunks.push_back({"XTRV", &xtrvData});
    chunks.push_back({"DNER", &dnerData});
    if (!tamcData.empty())
        chunks.push_back({"TAMC", &tamcData});

    for (auto& c : chunks) {
        while (c.data->size() % 4 != 0) c.data->push_back(0u);
    }

    uint32_t fileSize = 20;
    for (auto& c : chunks) fileSize += 16 + static_cast<uint32_t>(c.data->size());

    std::vector<uint8_t> out;
    out.reserve(fileSize);
    WriteTag(out, "ILFF");
    WriteU32(out, fileSize);
    WriteU32(out, 4u);
    WriteU32(out, 0u);
    WriteTag(out, "OCEM");

    for (size_t ci = 0; ci < chunks.size(); ++ci) {
        const bool isLast = (ci == chunks.size() - 1);
        const uint32_t dataSize = static_cast<uint32_t>(chunks[ci].data->size());
        const uint32_t skip = isLast ? 0u : (16u + dataSize);
        WriteChunkHeader(out, chunks[ci].tag, dataSize, skip);
        out.insert(out.end(), chunks[ci].data->begin(), chunks[ci].data->end());
    }

    if (out.size() != fileSize) {
        Logger::Get().Log(LogLevel::ERR,
            "[MefCompiler] Size mismatch: expected " +
            std::to_string(fileSize) + " got " + std::to_string(out.size()));
        return false;
    }

    std::ofstream f(outBinaryPath, std::ios::binary);
    if (!f.is_open()) {
        Logger::Get().Log(LogLevel::ERR,
            "[MefCompiler] Cannot open output file: " + outBinaryPath);
        return false;
    }
    f.write(reinterpret_cast<const char*>(out.data()),
            static_cast<std::streamsize>(out.size()));

    Logger::Get().Log(LogLevel::INFO,
        "[MefCompiler] Compiled binary MEF to: " + outBinaryPath +
        " (without sidecar)");
    return true;
}

bool BuildRigidFromParsedGeometry(const ParsedGeometry& geo, const std::string& outBinaryPath) {
    MEFObject obj;
    obj.model_type = geo.modelType;
    
    bool hasRawPos = false;
    for (const auto& v : geo.vertices) {
        if (v.rawPos.x != 0 || v.rawPos.y != 0 || v.rawPos.z != 0) { hasRawPos = true; break; }
    }
    
    for (size_t i = 0; i < geo.vertices.size(); ++i) {
        const auto& v = geo.vertices[i];
        glm::vec3 p = hasRawPos ? v.rawPos : (v.pos * 40.96f);
        obj.vertices.push_back({p.x, p.y, p.z});
        obj.normals.push_back({v.normal.x, v.normal.y, v.normal.z});
        obj.uvs.push_back({v.uv.x, v.uv.y});
        MEFBoneVertex bv;
        bv.index = static_cast<int>(i);
        bv.bone_index = v.boneIndex;
        bv.weight = v.weight;
        bv.local_vert_id = v.localVertexId;
        obj.bone_vertices.push_back(bv);
    }
    
    for (const auto& block : geo.renderBlocks) {
        for (size_t i = 0; i < block.triangleCount; ++i) {
            const auto& tri = geo.triangles[block.triangleStart + i];
            MEFFace f;
            f.v0 = tri[0]; f.v1 = tri[1]; f.v2 = tri[2];
            f.n0 = tri[0]; f.n1 = tri[1]; f.n2 = tri[2];
            f.material_index = block.materialSlot;
            obj.faces.push_back(f);
        }
    }
    
    for (size_t i = 0; i < geo.tamcRecords.size(); ++i) {
        MEFMaterial m;
        m.index = static_cast<int>(i);
        m.name = "mat_" + std::to_string(i);
        obj.materials.push_back(m);
    }
    
    std::vector<uint32_t> remA, remB, remC;
    std::vector<BinaryVertex> verts = BuildVertices(obj, obj.faces, remA, remB, remC);
    
    const uint32_t numVerts  = static_cast<uint32_t>(verts.size());
    const uint32_t numFaces  = static_cast<uint32_t>(obj.faces.size());
    const float    radius    = ComputeRadius(verts);
    const uint32_t numBlocks = static_cast<uint32_t>(geo.tamcRecords.empty() ? 1 : geo.tamcRecords.size());
    const int numMatSlots    = static_cast<int>(numBlocks);
    
    auto hsemData = BuildHsem(numFaces, numVerts, numBlocks, geo.mefAttachments.size(), radius, obj.model_type);
    auto d3drData = BuildD3dr(numFaces, numBlocks, numVerts, obj.model_type);
    auto xtrvData = BuildXtrv(verts, obj.model_type);
    auto dnerData = BuildDner(obj.faces, remA, remB, remC, numMatSlots, obj.model_type);
    auto tamcData = BuildTamc(obj.materials, numMatSlots);
    
    std::vector<uint8_t> attaData(geo.mefAttachments.size() * 68, 0);
    for (size_t i = 0; i < geo.mefAttachments.size(); ++i) {
        std::memcpy(&attaData[i * 68], &geo.mefAttachments[i], 68);
    }
    
    std::vector<uint8_t> pmtlData;
    for (const auto& p : geo.portals) {
        WriteU32(pmtlData, p.materialId);
        WriteZeros(pmtlData, 12);
    }
    
    std::vector<uint8_t> xvtpData, cftpData, tropData;
    for (const auto& p : geo.portals) {
        uint32_t vOff = static_cast<uint32_t>(xvtpData.size() / 12);
        uint32_t vCnt = static_cast<uint32_t>(p.verts.size());
        for (const auto& v : p.verts) {
            WriteFloat(xvtpData, v.x);
            WriteFloat(xvtpData, v.y);
            WriteFloat(xvtpData, v.z);
        }
        
        uint32_t fOff = static_cast<uint32_t>(cftpData.size() / 12);
        uint32_t fCnt = static_cast<uint32_t>(p.faces.size());
        for (const auto& f : p.faces) {
            WriteU32(cftpData, f[0]);
            WriteU32(cftpData, f[1]);
            WriteU32(cftpData, f[2]);
        }
        
        WriteU32(tropData, vOff);
        WriteU32(tropData, vCnt);
        WriteU32(tropData, fOff);
        WriteU32(tropData, fCnt);
        WriteU32(tropData, p.portalId);
    }
    
    if (hsemData.size() >= 156) {
        auto wU16 = [&](size_t off, uint16_t v) {
            hsemData[off]   =  v       & 0xFF;
            hsemData[off+1] = (v >> 8) & 0xFF;
        };
        wU16(132, static_cast<uint16_t>(geo.mefAttachments.size()));
        wU16(136, static_cast<uint16_t>(geo.portals.size()));
        wU16(140, static_cast<uint16_t>(xvtpData.size() / 12));
        wU16(142, static_cast<uint16_t>(cftpData.size() / 12));
    }
    
    struct ChunkDesc { const char* tag; std::vector<uint8_t>* data; };
    std::vector<ChunkDesc> chunks;
    chunks.push_back({"HSEM", &hsemData});
    chunks.push_back({"D3DR", &d3drData});
    chunks.push_back({"XTRV", &xtrvData});
    chunks.push_back({"DNER", &dnerData});
    if (!tamcData.empty()) chunks.push_back({"TAMC", &tamcData});
    if (!attaData.empty()) chunks.push_back({"ATTA", &attaData});
    if (!pmtlData.empty()) chunks.push_back({"PMTL", &pmtlData});
    if (!xvtpData.empty()) chunks.push_back({"XVTP", &xvtpData});
    if (!cftpData.empty()) chunks.push_back({"CFTP", &cftpData});
    if (!tropData.empty()) chunks.push_back({"TROP", &tropData});
    
    for (auto& c : chunks) {
        while (c.data->size() % 4 != 0) c.data->push_back(0u);
    }
    
    uint32_t fileSize = 20;
    for (auto& c : chunks) fileSize += 16 + static_cast<uint32_t>(c.data->size());
    
    std::vector<uint8_t> out;
    out.reserve(fileSize);
    WriteTag(out, "ILFF");
    WriteU32(out, fileSize);
    WriteU32(out, 4u);
    WriteU32(out, 0u);
    WriteTag(out, "OCEM");
    
    for (size_t ci = 0; ci < chunks.size(); ++ci) {
        const bool isLast = (ci == chunks.size() - 1);
        const uint32_t dataSize = static_cast<uint32_t>(chunks[ci].data->size());
        const uint32_t skip = isLast ? 0u : (16u + dataSize);
        WriteChunkHeader(out, chunks[ci].tag, dataSize, skip);
        out.insert(out.end(), chunks[ci].data->begin(), chunks[ci].data->end());
    }
    
    std::ofstream f(outBinaryPath, std::ios::binary);
    if (!f.is_open()) return false;
    f.write(reinterpret_cast<const char*>(out.data()), out.size());
    return true;
}

} // namespace MefCompiler

// End of mef_compiler.cpp
