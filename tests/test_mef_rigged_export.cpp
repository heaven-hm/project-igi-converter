// test_mef_rigged_export.cpp — fail-closed skeleton / bone-weight export.
//
// No game assets are required: a synthetic text MEF is compiled into a real
// binary IGI1 (modelType 1) MEF with MefCompiler, parsed back with ParseMefFile,
// and exported through the new rig-preserving path. This keeps the suite
// runnable on machines without the retail corpus.
#include <gtest/gtest.h>

#include "parsers/mef_compiler.h"
#include "parsers/mef_exporter.h"
#include "parsers/mef_native.h"
#include "renderer/hardcoded_bones.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct ScopedDir {
    fs::path path;
    explicit ScopedDir(const std::string& name) {
        path = fs::temp_directory_path() / ("igi1conv_rig_test_" + name + "_" +
               std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        std::error_code ec;
        fs::remove_all(path, ec);
        fs::create_directories(path, ec);
    }
    ~ScopedDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

// Build a minimal but structurally valid text MEF with Type 1 bone weights.
std::string WriteSyntheticTextMef(const fs::path& dir, const std::string& stem,
                                  const std::vector<uint16_t>& boneIndices,
                                  const std::vector<float>& weights) {
    const fs::path txt = dir / (stem + ".txt");
    std::ofstream f(txt);
    f << "NewObject(\"model_mesh\");\n";
    f << "ModelType(1);\n";
    f << "Material(0, \"mat_0\", 1.0, 1.0, 1.0, 0.5, 0.5, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1);\n";
    f << "MaterialShininess(0, 0.0);\n";
    for (size_t i = 0; i < boneIndices.size(); ++i) {
        f << "Vertex(" << i << ", " << (i * 10.0f) << ", 0.0, 0.0);\n";
        f << "Normal(" << i << ", 0.0, 0.0, 1.0);\n";
        f << "UV(" << i << ", 0.0, 0.0);\n";
        f << "BoneVertex(" << i << ", " << boneIndices[i] << ", "
          << std::setprecision(9) << weights[i] << ", " << i << ");\n";
    }
    // Always emit faces (the compiler requires at least one triangle). Wrap
    // indices so two-vertex fixtures still produce a valid face buffer.
    const size_t n = boneIndices.size();
    for (size_t i = 0; i < n; ++i) {
        const size_t a = i % n, b = (i + 1) % n, c = (i + 2) % n;
        f << "Face(" << i << ", " << a << ", " << b << ", " << c
          << ", " << a << ", " << b << ", " << c << ", 0);\n";
    }
    f.close();
    return txt.string();
}

std::string CompileRiggedMef(const fs::path& dir, const std::string& stem,
                             const std::vector<uint16_t>& boneIndices,
                             const std::vector<float>& weights) {
    const std::string txt = WriteSyntheticTextMef(dir, stem, boneIndices, weights);
    const fs::path mef = dir / (stem + ".mef");
    EXPECT_TRUE(MefCompiler::Compile(txt, mef.string())) << "synthetic MEF compile failed";
    return mef.string();
}

std::string ReadFile(const fs::path& p) {
    std::ifstream f(p);
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

} // namespace

// ── strict archetype resolution ─────────────────────────────────────────────

TEST(MefRiggedExport, StrictArchetypeSelectsKnownMappings) {
    auto jones = SelectIgi1BoneRigTypeStrict("000_01_1", 31);
    ASSERT_TRUE(jones.has_value());
    EXPECT_EQ(*jones, BoneRigType::JonesCinematic);

    auto heavy = SelectIgi1BoneRigTypeStrict("012_01_1", 31);
    ASSERT_TRUE(heavy.has_value());
    EXPECT_EQ(*heavy, BoneRigType::HeavySoldier);

    // Companion parts inherit the main model's archetype.
    auto companion = SelectIgi1BoneRigTypeStrict("012_01_2", 31);
    ASSERT_TRUE(companion.has_value());
    EXPECT_EQ(*companion, BoneRigType::HeavySoldier);

    auto standard = SelectIgi1BoneRigTypeStrict("001_01_1", 31);
    ASSERT_TRUE(standard.has_value());
    EXPECT_EQ(*standard, BoneRigType::StandardSoldier);

    auto fingers = SelectIgi1BoneRigTypeStrict("997_01_1", 48);
    ASSERT_TRUE(fingers.has_value());
    EXPECT_EQ(*fingers, BoneRigType::AdvancedFingerRig);
}

TEST(MefRiggedExport, StrictArchetypeRejectsUnknownNames) {
    EXPECT_FALSE(SelectIgi1BoneRigTypeStrict("", 5).has_value());
    EXPECT_FALSE(SelectIgi1BoneRigTypeStrict("a_static_prop", 5).has_value());
    EXPECT_FALSE(SelectIgi1BoneRigTypeStrict("001_01_1_extra", 5).has_value());
}

TEST(MefRiggedExport, ExplicitArchetypeNameParsing) {
    EXPECT_EQ(ParseBoneRigTypeName("heavysoldier"), BoneRigType::HeavySoldier);
    EXPECT_EQ(ParseBoneRigTypeName("HEAVY"), BoneRigType::HeavySoldier);
    EXPECT_EQ(ParseBoneRigTypeName("48"), BoneRigType::AdvancedFingerRig);
    EXPECT_FALSE(ParseBoneRigTypeName("not-a-rig").has_value());
}

// ── supported-format detection ─────────────────────────────────────────────

TEST(MefRiggedExport, FormatSupportsRiggingDetection) {
    EXPECT_FALSE(MefExporter::FormatSupportsRigging("out/model.obj"));
    EXPECT_FALSE(MefExporter::FormatSupportsRigging("out/model.mtl"));
    EXPECT_TRUE(MefExporter::FormatSupportsRigging("out/model.mef"));
    EXPECT_TRUE(MefExporter::FormatSupportsRigging("out/model.mex"));
    EXPECT_TRUE(MefExporter::FormatSupportsRigging("out/model.rig.json"));
}

// ── binary MEF round-trip of weight data ───────────────────────────────────

TEST(MefRiggedExport, SyntheticBinaryMefPreservesBoneWeights) {
    ScopedDir dir("parse");
    const std::string mef = CompileRiggedMef(dir.path, "001_01_1",
                                             {3u, 4u, 7u}, {1.0f, 0.75f, 0.5f});
    ASSERT_FALSE(mef.empty());

    ParsedGeometry geo = ParseMefFile(mef);
    ASSERT_EQ(geo.modelType, 1u);
    EXPECT_TRUE(geo.isIgi1);
    ASSERT_EQ(geo.vertices.size(), 3u);
    EXPECT_EQ(geo.vertices[0].boneIndex, 3u);
    EXPECT_FLOAT_EQ(geo.vertices[0].weight, 1.0f);
    EXPECT_EQ(geo.vertices[1].boneIndex, 4u);
    EXPECT_FLOAT_EQ(geo.vertices[1].weight, 0.75f);
    EXPECT_EQ(geo.vertices[2].boneIndex, 7u);
    EXPECT_FLOAT_EQ(geo.vertices[2].weight, 0.5f);
    EXPECT_EQ(geo.vertices[2].localVertexId, 2u);
    EXPECT_EQ(geo.bones.size(),
              GetIgi1HardcodedBonesForRig(BoneRigType::StandardSoldier).size());
}

// ── fail-closed rig-preserving export ──────────────────────────────────────

TEST(MefRiggedExport, WritesSidecarWithHierarchyAndWeights) {
    ScopedDir dir("sidecar");
    const std::string mef = CompileRiggedMef(dir.path, "001_01_1",
                                             {3u, 4u, 7u}, {1.0f, 0.75f, 0.5f});
    ParsedGeometry geo = ParseMefFile(mef);

    const fs::path outDir = dir.path / "out";
    MefExporter::RigExportResult r =
        MefExporter::ExportRiggedMef(geo, "001_01_1", outDir.string());
    ASSERT_TRUE(r.ok) << r.error;
    EXPECT_EQ(r.archetype, "StandardSoldier");
    EXPECT_EQ(r.boneCount,
              GetIgi1HardcodedBonesForRig(BoneRigType::StandardSoldier).size());
    EXPECT_EQ(r.weightCount, 3u);
    EXPECT_TRUE(fs::exists(r.sidecarPath));
    EXPECT_TRUE(fs::exists(outDir / "001_01_1.obj"));

    const std::string json = ReadFile(r.sidecarPath);
    EXPECT_NE(json.find("\"format\": \"igi1-rig-v1\""), std::string::npos);
    EXPECT_NE(json.find("\"archetype\": \"StandardSoldier\""), std::string::npos);
    EXPECT_NE(json.find("\"name\": \"upper body\""), std::string::npos);
    EXPECT_NE(json.find("\"parent\": -1"), std::string::npos);
    EXPECT_NE(json.find("\"boneIndex\": 7"), std::string::npos);
    EXPECT_NE(json.find("\"weight\": 0.75"), std::string::npos);
    EXPECT_NE(json.find("\"localVertexId\": 2"), std::string::npos);
}

TEST(MefRiggedExport, ExplicitArchetypeOverrideOnUnknownModelName) {
    ScopedDir dir("override");
    // Filename does not match a known archetype: strict resolution fails...
    const std::string mef = CompileRiggedMef(dir.path, "mod_char",
                                             {3u, 4u}, {1.0f, 0.5f});
    ParsedGeometry geo = ParseMefFile(mef);
    auto autoResult = MefExporter::ExportRiggedMef(
        geo, "mod_char", (dir.path / "auto").string());
    EXPECT_FALSE(autoResult.ok);
    EXPECT_NE(autoResult.error.find("no known skeletal archetype"), std::string::npos);
    EXPECT_FALSE(fs::exists(dir.path / "auto" / "mod_char.rig.json"));

    // ...but an explicit mapping is accepted and recorded.
    MefExporter::RigExportOptions opt;
    opt.archetype = "HeavySoldier";
    opt.writeObj = false;
    auto explicitResult = MefExporter::ExportRiggedMef(
        geo, "mod_char", (dir.path / "explicit").string(), opt);
    ASSERT_TRUE(explicitResult.ok) << explicitResult.error;
    EXPECT_EQ(explicitResult.archetype, "HeavySoldier");
    EXPECT_TRUE(fs::exists(explicitResult.sidecarPath));
    EXPECT_FALSE(fs::exists(dir.path / "explicit" / "mod_char.obj"));
}

TEST(MefRiggedExport, FailsClosedOnBoneIndexOutsideArchetype) {
    ScopedDir dir("badbone");
    const std::string mef = CompileRiggedMef(dir.path, "001_01_1",
                                             {3u, 99u}, {1.0f, 1.0f});
    ParsedGeometry geo = ParseMefFile(mef);
    auto r = MefExporter::ExportRiggedMef(geo, "001_01_1", (dir.path / "out").string());
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.error.find("references bone 99"), std::string::npos);
    EXPECT_FALSE(fs::exists(dir.path / "out" / "001_01_1.rig.json"));
}

TEST(MefRiggedExport, FailsClosedOnInvalidWeight) {
    ScopedDir dir("badweight");
    const std::string mef = CompileRiggedMef(dir.path, "001_01_1",
                                             {3u, 4u}, {1.0f, 2.5f});
    ParsedGeometry geo = ParseMefFile(mef);
    auto r = MefExporter::ExportRiggedMef(geo, "001_01_1", (dir.path / "out").string());
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.error.find("invalid bone weight"), std::string::npos);
}

TEST(MefRiggedExport, FailsClosedOnNonRiggedGeometry) {
    ScopedDir dir("rigid");
    ParsedGeometry geo;
    geo.modelType = 0;
    geo.vertices.resize(1);
    auto r = MefExporter::ExportRiggedMef(geo, "435_01_1", dir.path.string());
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.error.find("modelType"), std::string::npos);
}

// ── plain OBJ path is fail-closed ──────────────────────────────────────────

TEST(MefRiggedExport, ObjExportFailsClosedForRiggedGeometry) {
    ScopedDir dir("objfail");
    const std::string mef = CompileRiggedMef(dir.path, "001_01_1",
                                             {3u, 4u}, {1.0f, 0.5f});
    ParsedGeometry geo = ParseMefFile(mef);

    const fs::path obj = dir.path / "refused.obj";
    EXPECT_FALSE(MefExporter::ExportToObj(geo, obj.string()));
    EXPECT_FALSE(fs::exists(obj));

    // Explicit opt-in still works for users who only want the static mesh.
    const fs::path obj2 = dir.path / "geometry_only.obj";
    EXPECT_TRUE(MefExporter::ExportToObjGeometryOnly(geo, obj2.string()));
    EXPECT_TRUE(fs::exists(obj2));
}

TEST(MefRiggedExport, MergedBundleFailsClosedOnRiggedSource) {
    ScopedDir dir("merged");
    ParsedGeometry geo;
    geo.modelType = 0;
    geo.hasRiggedSource = true; // e.g. a bundle that swallowed a Type 1 child
    geo.vertices.resize(3);

    EXPECT_FALSE(MefExporter::ExportMergedToObjBundle(
        geo, "bundle", (dir.path / "refused").string(), {}, false));
    EXPECT_TRUE(MefExporter::ExportMergedToObjBundle(
        geo, "bundle", (dir.path / "allowed").string(), {}, false, true));
}

TEST(MefRiggedExport, TextMefObjExportFailsClosedForBoneVertices) {
    ScopedDir dir("textobj");
    const std::string txt = WriteSyntheticTextMef(dir.path, "001_01_1",
                                                  {3u, 4u}, {1.0f, 0.5f});
    MEFParser parser;
    std::vector<MEFObject> objects = parser.parse_file(txt);
    ASSERT_FALSE(objects.empty());
    ASSERT_FALSE(objects[0].bone_vertices.empty());

    const fs::path obj = dir.path / "refused.obj";
    EXPECT_FALSE(MefExporter::ExportTextMefToObj(objects, obj.string()));
    EXPECT_FALSE(fs::exists(obj));
    EXPECT_TRUE(MefExporter::ExportTextMefToObj(objects, obj.string(), true));
    EXPECT_TRUE(fs::exists(obj));
}
