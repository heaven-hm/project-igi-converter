// Tests for issue #18: custom MEF sidecars whose text Face() topology differs
// from the original sidecar.  The compiler must rebuild only the render chunks
// (D3DR/XTRV/DNER) and must fail closed when the sidecar cannot be adapted.

#include "../source/parsers/mef_compiler.h"
#include "../source/parsers/mef_exporter.h"
#include "../source/parsers/mef_native.h"
#include "../source/common.h" // log_type_t
#include "../source/utils.h"  // Utils::Trim

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// support.cpp (which defines the legacy Log() and Utils::Trim()) is not linked
// into the test target; test_lightmap_resolver.cpp provides Logger::Log.
void Log(log_type_t, const char*, int, const char*, ...) {}
namespace Utils {
std::string Trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}
} // namespace Utils

namespace {

namespace fs = std::filesystem;

uint32_t ReadU32(const std::vector<uint8_t>& d, size_t off) {
    if (off + 4 > d.size()) return 0;
    return static_cast<uint32_t>(d[off]) |
           (static_cast<uint32_t>(d[off + 1]) << 8) |
           (static_cast<uint32_t>(d[off + 2]) << 16) |
           (static_cast<uint32_t>(d[off + 3]) << 24);
}

uint16_t ReadU16(const std::vector<uint8_t>& d, size_t off) {
    if (off + 2 > d.size()) return 0;
    return static_cast<uint16_t>(d[off]) |
           (static_cast<uint16_t>(d[off + 1]) << 8);
}

void PutU32(std::vector<uint8_t>& d, size_t off, uint32_t v) {
    if (off + 4 > d.size()) return;
    d[off]     = static_cast<uint8_t>(v & 0xFF);
    d[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    d[off + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    d[off + 3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

ParsedGeometry::RawChunk Chunk(const char (&tag)[5], std::vector<uint8_t> data) {
    ParsedGeometry::RawChunk rc;
    std::memcpy(rc.fourcc, tag, 4);
    rc.data = std::move(data);
    return rc;
}

const ParsedGeometry::RawChunk* Find(const ParsedGeometry& g, const char* tag) {
    for (const auto& rc : g.rawChunks) {
        if (rc.isTag(tag)) return &rc;
    }
    return nullptr;
}

fs::path TempDir(const std::string& label) {
    static std::atomic<int> counter{0};
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path p = fs::temp_directory_path() /
                 ("igi1conv_issue18_" + label + "_" + std::to_string(tick) +
                  "_" + std::to_string(++counter));
    fs::create_directories(p);
    return p;
}

bool WriteFile(const fs::path& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
    return true;
}

// A small but structurally valid type-1 (bone) sidecar: 2 render faces,
// 4 render vertices + 2 skinning-only vertices, one opaque "TST!" chunk.
std::vector<ParsedGeometry::RawChunk> MakeSidecar() {
    std::vector<ParsedGeometry::RawChunk> chunks;

    std::vector<uint8_t> hsem(156, 0);
    PutU32(hsem, 32, 1);  // model_type
    PutU32(hsem, 96, 2);  // num_r_faces
    PutU32(hsem, 100, 6); // num_r_verts
    PutU32(hsem, 104, 0); // render buffer size (recomputed by the compiler)
    chunks.push_back(Chunk("HSEM", hsem));

    chunks.push_back(Chunk("ATTA", {}));

    std::vector<uint8_t> d3dr(24, 0);
    PutU32(d3dr, 4, 2);  // num_faces
    PutU32(d3dr, 8, 0);  // num_meshes (packed DNER)
    PutU32(d3dr, 12, 4); // verts0
    PutU32(d3dr, 16, 2); // verts1 (skinning-only)
    PutU32(d3dr, 20, 6); // num_verts
    chunks.push_back(Chunk("D3DR", d3dr));

    std::vector<uint8_t> dner(28 + 12, 0); // one 28-byte block + 6 indices
    dner[12] = 6;  // indexCount
    dner[13] = 0;
    dner[14] = 0xFF; // nextoffs = -1 (last block)
    dner[15] = 0xFF;
    dner[18] = 0; // offVerts
    dner[19] = 0;
    dner[20] = 4; // numVerts -> 4 render vertices
    dner[21] = 0;
    const uint16_t indices[6] = {0, 1, 2, 1, 2, 3};
    std::memcpy(dner.data() + 28, indices, sizeof(indices));
    chunks.push_back(Chunk("DNER", dner));

    chunks.push_back(Chunk("XTRV", std::vector<uint8_t>(6 * 40, 0)));
    chunks.push_back(Chunk("TST!", {0xDE, 0xAD, 0xBE, 0xEF}));
    return chunks;
}

std::string MakeText(uint32_t faceCount, uint32_t vertexCount = 4,
                     bool badModelType = false) {
    std::ostringstream o;
    o << "NewObject(\"model_mesh\");\n";
    o << "ModelType(" << (badModelType ? 0 : 1) << ");\n";
    o << "Material(0, \"mat_0\", 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1);\n";
    for (uint32_t i = 0; i < vertexCount; ++i) {
        o << "Vertex(" << i << ", " << (i * 1.0f) << ", 0, 0);\n";
        o << "Normal(" << i << ", 0, 0, 1);\n";
        o << "UV(" << i << ", 0.5, 0.5);\n";
        o << "BoneVertex(" << i << ", 0, 1, " << i << ");\n";
    }
    for (uint32_t f = 0; f < faceCount; ++f) {
        const uint32_t a = f % vertexCount;
        const uint32_t b = (f + 1) % vertexCount;
        const uint32_t c = (f + 2) % vertexCount;
        o << "Face(" << f << ", " << a << ", " << b << ", " << c
          << ", " << a << ", " << b << ", " << c << ", 0);\n";
    }
    return o.str();
}

fs::path WriteScenario(const fs::path& dir,
                       const std::vector<ParsedGeometry::RawChunk>& chunks,
                       const std::string& text) {
    const fs::path sidecar = dir / "model.mex";
    const fs::path txt = dir / "model.txt";
    if (!MefExporter::WriteMefSidecar(chunks, sidecar.string())) return {};
    if (!WriteFile(txt, text)) return {};
    return txt;
}

} // namespace

TEST(MefSidecarTopology, FaceCountMismatchRebuildsRenderChunksAndPreservesOpaqueChunks) {
    const fs::path dir = TempDir("custom-topology");
    auto chunks = MakeSidecar(); // sidecar has 2 faces
    const fs::path txt = WriteScenario(dir, chunks, MakeText(3)); // text has 3
    ASSERT_FALSE(txt.empty());

    const fs::path out = dir / "custom.mef";
    ASSERT_TRUE(MefCompiler::Compile(txt.string(), out.string()))
        << "custom face topology with sidecar must compile";

    const ParsedGeometry g = ParseMefFile(out.string());
    const auto* hsem = Find(g, "HSEM");
    const auto* d3dr = Find(g, "D3DR");
    const auto* dner = Find(g, "DNER");
    const auto* xtrv = Find(g, "XTRV");
    const auto* test = Find(g, "TST!");
    ASSERT_NE(hsem, nullptr);
    ASSERT_NE(d3dr, nullptr);
    ASSERT_NE(dner, nullptr);
    ASSERT_NE(xtrv, nullptr);
    ASSERT_NE(test, nullptr);

    EXPECT_EQ(ReadU32(hsem->data, 96), 3u);  // num_r_faces
    EXPECT_EQ(ReadU32(hsem->data, 100), 6u); // num_r_verts preserved
    EXPECT_EQ(ReadU32(d3dr->data, 4), 3u);   // D3DR face count
    EXPECT_EQ(ReadU32(d3dr->data, 20), 6u);  // total vertex count
    EXPECT_EQ(ReadU16(dner->data, 12), 9u);  // 3 triangles of indices
    EXPECT_EQ(xtrv->data.size(), 6u * 40u);  // skinning tail preserved
    EXPECT_EQ(test->data, (std::vector<uint8_t>{0xDE, 0xAD, 0xBE, 0xEF}));
}

TEST(MefSidecarTopology, UnchangedTopologyStillCompiles) {
    const fs::path dir = TempDir("unchanged-topology");
    const fs::path txt = WriteScenario(dir, MakeSidecar(), MakeText(2));
    ASSERT_FALSE(txt.empty());
    const fs::path out = dir / "same.mef";
    ASSERT_TRUE(MefCompiler::Compile(txt.string(), out.string()));

    const ParsedGeometry g = ParseMefFile(out.string());
    const auto* d3dr = Find(g, "D3DR");
    ASSERT_NE(d3dr, nullptr);
    EXPECT_EQ(ReadU32(d3dr->data, 4), 2u);
}

TEST(MefSidecarTopology, FailsClosedOnMissingRequiredChunk) {
    const fs::path dir = TempDir("missing-chunk");
    auto chunks = MakeSidecar();
    chunks.erase(chunks.begin()); // drop HSEM
    const fs::path txt = WriteScenario(dir, chunks, MakeText(3));
    ASSERT_FALSE(txt.empty());
    EXPECT_FALSE(MefCompiler::Compile(txt.string(), (dir / "out.mef").string()));
    EXPECT_FALSE(fs::exists(dir / "out.mef"));
}

TEST(MefSidecarTopology, FailsClosedOnModelTypeMismatch) {
    const fs::path dir = TempDir("model-type");
    auto chunks = MakeSidecar();
    PutU32(chunks[0].data, 32, 0); // HSEM says rigid, text says bone
    const fs::path txt = WriteScenario(dir, chunks, MakeText(3));
    ASSERT_FALSE(txt.empty());
    EXPECT_FALSE(MefCompiler::Compile(txt.string(), (dir / "out.mef").string()));
}

TEST(MefSidecarTopology, FailsClosedOnInconsistentSidecarCounts) {
    const fs::path dir = TempDir("bad-sidecar");
    auto chunks = MakeSidecar();
    // Advertise 2 faces in D3DR/HSEM but put 3 faces in DNER.
    auto& dner = chunks[3].data;
    dner.resize(28 + 18, 0);
    dner[12] = 9;
    const uint16_t indices[9] = {0, 1, 2, 1, 2, 3, 0, 2, 3};
    std::memcpy(dner.data() + 28, indices, sizeof(indices));
    const fs::path txt = WriteScenario(dir, chunks, MakeText(3));
    ASSERT_FALSE(txt.empty());
    EXPECT_FALSE(MefCompiler::Compile(txt.string(), (dir / "out.mef").string()));
}

TEST(MefSidecarTopology, FailsClosedOnVertexCountEdit) {
    const fs::path dir = TempDir("vertex-count");
    const fs::path txt = WriteScenario(dir, MakeSidecar(), MakeText(3, 5));
    ASSERT_FALSE(txt.empty());
    EXPECT_FALSE(MefCompiler::Compile(txt.string(), (dir / "out.mef").string()));
}

// Optional end-to-end test with the exact attachment from issue #18.
// Set IGI_ISSUE18_REPRO_DIR to the folder containing 000_01_1_custom.txt
// and 000_01_1_custom.mex (extracted from the ZIP in the issue).
TEST(MefSidecarTopology, Issue18ReproFilesCompileWithCustomTopology) {
    const char* env = std::getenv("IGI_ISSUE18_REPRO_DIR");
    if (env == nullptr) {
        GTEST_SKIP() << "set IGI_ISSUE18_REPRO_DIR to run the issue #18 repro";
    }
    const fs::path src(env);
    const fs::path txtIn = src / "000_01_1_custom.txt";
    const fs::path mexIn = src / "000_01_1_custom.mex";
    if (!fs::exists(txtIn) || !fs::exists(mexIn)) {
        GTEST_SKIP() << "repro files not found in " << src.string();
    }

    const fs::path dir = TempDir("issue18-repro");
    fs::copy(txtIn, dir / txtIn.filename());
    fs::copy(mexIn, dir / mexIn.filename());
    const fs::path out = dir / "000_01_1_custom.mef";
    ASSERT_TRUE(MefCompiler::Compile((dir / txtIn.filename()).string(), out.string()))
        << "issue #18 repro must compile with its custom sidecar";

    const ParsedGeometry g = ParseMefFile(out.string());
    const auto* hsem = Find(g, "HSEM");
    const auto* d3dr = Find(g, "D3DR");
    const auto* xtrv = Find(g, "XTRV");
    const auto* hpsc = Find(g, "HPSC");
    ASSERT_NE(hsem, nullptr);
    ASSERT_NE(d3dr, nullptr);
    ASSERT_NE(xtrv, nullptr);
    ASSERT_NE(hpsc, nullptr); // opaque skinning data preserved verbatim

    EXPECT_EQ(ReadU32(hsem->data, 96), 968u);
    EXPECT_EQ(ReadU32(d3dr->data, 4), 968u);
    EXPECT_EQ(xtrv->data.size(), 1189u * 40u);
    EXPECT_GT(hpsc->data.size(), 0u);
}
