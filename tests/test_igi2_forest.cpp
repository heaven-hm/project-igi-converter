// Portable unit tests for the IGI 2 DAT Forest vertical slice (issue #10).
// No game assets are used: fixtures are synthesized from the published format
// (igipy docs/igi2/formats/dat_forest.md) so they run on any machine/CI.
#include "../source/game_selector.h"
#include "../source/parsers/igi2_forest_parser.h"

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> MakeBytes(uint32_t field)
{
    std::vector<uint8_t> b(4);
    b[0] = static_cast<uint8_t>(field & 0xFF);
    b[1] = static_cast<uint8_t>((field >> 8) & 0xFF);
    b[2] = static_cast<uint8_t>((field >> 16) & 0xFF);
    b[3] = static_cast<uint8_t>((field >> 24) & 0xFF);
    return b;
}

void AppendFloat(std::vector<uint8_t>& out, float value)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    const auto b = MakeBytes(bits);
    out.insert(out.end(), b.begin(), b.end());
}

// Build one valid 14-byte header + N documented 40-byte records.
std::vector<uint8_t> MakeForestBytes(size_t recordCount)
{
    std::vector<uint8_t> bytes;
    const auto header = MakeBytes(10u);
    bytes.insert(bytes.end(), header.begin(), header.end());
    bytes.push_back(0x0A);
    const char* version = "Ver. 3.0";
    for (int i = 0; i < 8; ++i)
        bytes.push_back(static_cast<uint8_t>(version[i]));
    bytes.push_back(0x0A);

    for (size_t i = 0; i < recordCount; ++i) {
        AppendFloat(bytes, 43643.95f + static_cast<float>(i));   // position x
        AppendFloat(bytes, 1736414.38f);                        // position y
        AppendFloat(bytes, 155616.58f);                         // position z
        AppendFloat(bytes, 0.0f);                               // rotation x
        AppendFloat(bytes, -0.0f);                              // rotation y (0x80000000 in retail files)
        AppendFloat(bytes, 1.25f);                              // rotation z
        AppendFloat(bytes, 0.2f);                               // scale x
        AppendFloat(bytes, 0.2f);                               // scale y
        AppendFloat(bytes, 0.2f);                               // scale z
        bytes.push_back(static_cast<uint8_t>(60 + i));          // color r
        bytes.push_back(static_cast<uint8_t>(120 + i));         // color g
        bytes.push_back(static_cast<uint8_t>(180 + i));         // color b
        bytes.push_back(0);                                     // color a (unused)
    }
    return bytes;
}

} // namespace

TEST(Igi2ForestParser, ReadsDocumentedHeaderAndRecordLayout)
{
    const auto bytes = MakeForestBytes(2);
    ASSERT_EQ(bytes.size(), 14u + 2u * 40u);

    igi2::ForestFile forest;
    std::string err;
    ASSERT_TRUE(igi2::Forest_ParseBytes(bytes.data(), bytes.size(), forest, err)) << err;
    EXPECT_TRUE(forest.valid);
    EXPECT_EQ(forest.headerValue, 10u);
    EXPECT_EQ(forest.version, "Ver. 3.0");
    ASSERT_EQ(forest.records.size(), 2u);

    const auto& r = forest.records[1];
    EXPECT_FLOAT_EQ(r.position[0], 43644.95f);
    EXPECT_FLOAT_EQ(r.position[1], 1736414.38f);
    EXPECT_FLOAT_EQ(r.position[2], 155616.58f);
    EXPECT_FLOAT_EQ(r.rotation[0], 0.0f);
    EXPECT_FLOAT_EQ(r.rotation[1], -0.0f);
    EXPECT_FLOAT_EQ(r.rotation[2], 1.25f);
    EXPECT_FLOAT_EQ(r.scale[0], 0.2f);
    EXPECT_EQ(r.color[0], 61);
    EXPECT_EQ(r.color[1], 121);
    EXPECT_EQ(r.color[2], 181);
    EXPECT_EQ(r.color[3], 0);

    // The retail negative-zero bit pattern must survive the round trip.
    std::vector<uint8_t> again;
    ASSERT_TRUE(igi2::Forest_Serialize(forest, again, err)) << err;
    EXPECT_EQ(again, bytes);
    const size_t rotYOffset = 14u + 40u + 16u;
    EXPECT_EQ(again[rotYOffset], 0x00);
    EXPECT_EQ(again[rotYOffset + 1], 0x00);
    EXPECT_EQ(again[rotYOffset + 2], 0x00);
    EXPECT_EQ(again[rotYOffset + 3], 0x80);
}

TEST(Igi2ForestParser, HandlesZeroRecords)
{
    const auto bytes = MakeForestBytes(0);
    igi2::ForestFile forest;
    std::string err;
    ASSERT_TRUE(igi2::Forest_ParseBytes(bytes.data(), bytes.size(), forest, err)) << err;
    EXPECT_TRUE(forest.records.empty());

    std::vector<uint8_t> again;
    ASSERT_TRUE(igi2::Forest_Serialize(forest, again, err)) << err;
    EXPECT_EQ(again, bytes);
}

TEST(Igi2ForestParser, RejectsMalformedInput)
{
    std::string err;
    igi2::ForestFile forest;

    auto bytes = MakeForestBytes(1);
    bytes[0] = 11; // wrong header value
    EXPECT_FALSE(igi2::Forest_ParseBytes(bytes.data(), bytes.size(), forest, err));
    EXPECT_FALSE(err.empty());

    bytes = MakeForestBytes(1);
    bytes[4] = 0x0D; // missing newline
    err.clear();
    EXPECT_FALSE(igi2::Forest_ParseBytes(bytes.data(), bytes.size(), forest, err));

    bytes = MakeForestBytes(1);
    bytes[5] = 'X'; // wrong version
    err.clear();
    EXPECT_FALSE(igi2::Forest_ParseBytes(bytes.data(), bytes.size(), forest, err));

    bytes = MakeForestBytes(1);
    bytes.pop_back(); // record area no longer a multiple of 40
    err.clear();
    EXPECT_FALSE(igi2::Forest_ParseBytes(bytes.data(), bytes.size(), forest, err));

    err.clear();
    EXPECT_FALSE(igi2::Forest_ParseBytes(bytes.data(), 13, forest, err));
}

TEST(Igi2ForestParser, JsonExportImportRoundTripsToIdenticalBinary)
{
    const auto bytes = MakeForestBytes(3);
    igi2::ForestFile forest;
    std::string err;
    ASSERT_TRUE(igi2::Forest_ParseBytes(bytes.data(), bytes.size(), forest, err)) << err;

    const std::string json = igi2::Forest_FormatJSON(forest);
    EXPECT_NE(json.find("\"schema\": \"igi2-forest/1\""), std::string::npos);
    EXPECT_NE(json.find("\"color_r\""), std::string::npos);

    igi2::ForestFile fromJson;
    ASSERT_TRUE(igi2::Forest_ParseJSON(json, fromJson, err)) << err;
    ASSERT_EQ(fromJson.records.size(), 3u);

    std::vector<uint8_t> rebuilt;
    ASSERT_TRUE(igi2::Forest_Serialize(fromJson, rebuilt, err)) << err;
    EXPECT_EQ(rebuilt, bytes);
}

TEST(Igi2ForestParser, JsonRejectsOutOfRangeColorAndMissingFields)
{
    std::string err;
    igi2::ForestFile forest;

    const std::string badColor = R"({"header_value":10,"version":"Ver. 3.0","records":[
        {"position_x":0,"position_y":0,"position_z":0,"rotation_x":0,"rotation_y":0,
         "rotation_z":0,"scale_x":1,"scale_y":1,"scale_z":1,
         "color_r":256,"color_g":0,"color_b":0,"color_a":0}]})";
    EXPECT_FALSE(igi2::Forest_ParseJSON(badColor, forest, err));
    EXPECT_NE(err.find("color_r"), std::string::npos);

    err.clear();
    const std::string missing = R"({"header_value":10,"version":"Ver. 3.0","records":[{"position_x":0}]})";
    EXPECT_FALSE(igi2::Forest_ParseJSON(missing, forest, err));

    err.clear();
    EXPECT_FALSE(igi2::Forest_ParseJSON("{ not json", forest, err));
}

TEST(GameSelector, DefaultsToIgi1AndAcceptsBothFlagSpellings)
{
    char arg0[] = "igi1conv";
    char cmd[] = "forest";
    char flagEq[] = "--game=igi2";
    char flagSep[] = "--game";
    char value[] = "igi2";

    char* a1[] = { arg0, cmd };
    auto sel = Game_Select(2, a1);
    EXPECT_EQ(sel.game, igi_game_t::IGI1);
    EXPECT_EQ(sel.command_index, 1);
    EXPECT_FALSE(sel.explicit_flag);
    EXPECT_TRUE(sel.error.empty());
    EXPECT_STREQ(Game_Name(sel.game), "igi1");

    char* a2[] = { arg0, flagEq, cmd };
    sel = Game_Select(3, a2);
    EXPECT_EQ(sel.game, igi_game_t::IGI2);
    EXPECT_EQ(sel.command_index, 2);
    EXPECT_TRUE(sel.explicit_flag);

    char* a3[] = { arg0, flagSep, value, cmd };
    sel = Game_Select(4, a3);
    EXPECT_EQ(sel.game, igi_game_t::IGI2);
    EXPECT_EQ(sel.command_index, 3);

    char bad[] = "--game=igi3";
    char* a4[] = { arg0, bad, cmd };
    sel = Game_Select(3, a4);
    EXPECT_FALSE(sel.error.empty());

    char* a5[] = { arg0, flagSep };
    sel = Game_Select(2, a5);
    EXPECT_FALSE(sel.error.empty());
}
