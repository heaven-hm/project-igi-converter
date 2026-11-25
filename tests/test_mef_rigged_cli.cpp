// test_mef_rigged_cli.cpp — end-to-end CLI coverage for the fail-closed
// skeleton/bone-weight export added for issue #20.
//
// These tests synthesise a small Type 1 MEF themselves (via `mef compile`), so
// no retail corpus or IGI_GAME_PATH is needed.
#include "igi1conv_test_util.h"

#include <fstream>
#include <string>

using namespace igi1conv_test;

namespace {

void WriteRiggedTextMef(const std::string& path, const std::string& stem,
                        const std::string& boneIndex = "3") {
    std::ofstream f(path);
    f << "NewObject(\"model_mesh\");\n";
    f << "ModelType(1);\n";
    f << "Material(0, \"mat_0\", 1.0, 1.0, 1.0, 0.5, 0.5, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1);\n";
    f << "Vertex(0, 0, 0, 0);\nVertex(1, 10, 0, 0);\nVertex(2, 0, 10, 0);\n";
    f << "Normal(0, 0, 0, 1);\nNormal(1, 0, 0, 1);\nNormal(2, 0, 0, 1);\n";
    f << "UV(0, 0, 0);\nUV(1, 1, 0);\nUV(2, 0, 1);\n";
    f << "BoneVertex(0, " << boneIndex << ", 1.0, 0);\n";
    f << "BoneVertex(1, 4, 0.75, 1);\n";
    f << "BoneVertex(2, 7, 0.5, 2);\n";
    f << "Face(0, 0, 1, 2, 0, 1, 2, 0);\n";
    f.close();
    (void)stem;
}

std::string Slurp(const std::string& path) {
    std::ifstream f(path);
    return std::string(std::istreambuf_iterator<char>(f),
                       std::istreambuf_iterator<char>());
}

} // namespace

TEST_F(IGI1ConvTest, MefExportRiggedWritesSkeletonAndWeights) {
    TempDir tmp;
    const std::string txt  = tmp / "001_01_1.txt";
    const std::string mef  = tmp / "001_01_1.mef";
    const std::string out  = tmp / "out";
    const std::string obj  = tmp / "refused.obj";
    WriteRiggedTextMef(txt, "001_01_1");
    ASSERT_EQ(RunIGI1Conv("mef compile " + Q(txt) + " -o " + Q(mef)), 0);

    // Default OBJ export must fail closed for a rigged model.
    EXPECT_NE(RunIGI1Conv("mef export " + Q(mef) + " -o " + Q(obj)), 0);
    EXPECT_FALSE(std::filesystem::exists(obj));

    // Explicit geometry-only opt-in still writes the static mesh.
    const std::string staticObj = tmp / "static.obj";
    EXPECT_EQ(RunIGI1Conv("mef export " + Q(mef) + " -o " + Q(staticObj) +
                          " --geometry-only"), 0);
    EXPECT_TRUE(NonEmptyFile(staticObj));

    // Rig-preserving export writes OBJ + <stem>.rig.json.
    ASSERT_EQ(RunIGI1Conv("mef export-rigged " + Q(mef) + " -o " + Q(out)), 0);
    std::string sidecar = (std::filesystem::path(out) / "001_01_1.rig.json").string();
    ASSERT_TRUE(NonEmptyFile(sidecar)) << sidecar;
    const std::string json = Slurp(sidecar);
    EXPECT_NE(json.find("\"archetype\": \"StandardSoldier\""), std::string::npos);
    EXPECT_NE(json.find("\"boneIndex\": 7"), std::string::npos);
    EXPECT_NE(json.find("\"weight\": 0.75"), std::string::npos);
    EXPECT_TRUE(NonEmptyFile((std::filesystem::path(out) / "001_01_1.obj").string()));
}

TEST_F(IGI1ConvTest, MefExportRiggedFailsClosedWithoutArchetype) {
    TempDir tmp;
    const std::string txt = tmp / "mod_char.txt";
    const std::string mef = tmp / "mod_char.mef";
    WriteRiggedTextMef(txt, "mod_char");
    ASSERT_EQ(RunIGI1Conv("mef compile " + Q(txt) + " -o " + Q(mef)), 0);

    const std::string out1 = tmp / "no_map";
    EXPECT_NE(RunIGI1Conv("mef export-rigged " + Q(mef) + " -o " + Q(out1)), 0);
    EXPECT_FALSE(std::filesystem::exists((std::filesystem::path(out1) / "mod_char.rig.json").string()));

    // Explicit mapping is accepted and recorded.
    const std::string out2 = tmp / "mapped";
    ASSERT_EQ(RunIGI1Conv("mef export-rigged " + Q(mef) + " -o " + Q(out2) +
                          " --rig-archetype HeavySoldier"), 0);
    const std::string json = Slurp((std::filesystem::path(out2) / "mod_char.rig.json").string());
    EXPECT_NE(json.find("\"archetype\": \"HeavySoldier\""), std::string::npos);
}
