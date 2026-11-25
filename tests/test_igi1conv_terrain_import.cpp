// test_igi1conv_terrain_import.cpp -- strict, corpus-free round-trip tests for
// the PGM -> LMP write-back path.  Every fixture is synthesized in a TempDir,
// so these tests run on Windows, Linux and macOS without retail game files.
#include "igi1conv_test_util.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace igi1conv_test;

namespace {

std::vector<uint8_t> make_lmp(uint32_t size, const std::vector<uint8_t>& luma)
{
    std::vector<uint8_t> out;
    out.push_back(static_cast<uint8_t>(size & 0xFF));
    out.push_back(static_cast<uint8_t>((size >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((size >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((size >> 24) & 0xFF));
    out.insert(out.end(), luma.begin(), luma.end());
    return out;
}

std::vector<uint8_t> read_bytes(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(f),
                                std::istreambuf_iterator<char>());
}

void write_bytes(const std::string& path, const std::vector<uint8_t>& bytes)
{
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
}

void write_text(const std::string& path, const std::string& text)
{
    std::ofstream f(path, std::ios::binary);
    f.write(text.data(), static_cast<std::streamsize>(text.size()));
}

} // namespace

TEST(TerrainPgmImport, RoundTripsSyntheticLmpThroughPgm)
{
    TempDir tmp;
    const uint32_t size = 4;
    std::vector<uint8_t> luma;
    for (uint32_t i = 0; i < size * size; ++i)
        luma.push_back(static_cast<uint8_t>((i * 37u + 11u) & 0xFFu));

    const std::string src = tmp / "light.lmp";
    const std::string pgm = tmp / "light.pgm";
    const std::string dst = tmp / "rebuilt.lmp";
    const auto original = make_lmp(size, luma);
    write_bytes(src, original);

    ASSERT_EQ(RunIGI1Conv("terrain export-lmp " + Q(src) + " -o " + Q(pgm)), 0);
    const auto pgm_bytes = read_bytes(pgm);
    ASSERT_EQ(std::string(pgm_bytes.begin(), pgm_bytes.begin() + 13), "P5\n4 4\n65535\n");

    ASSERT_EQ(RunIGI1Conv("terrain import-lmp " + Q(pgm) + " -o " + Q(dst)), 0);
    EXPECT_EQ(read_bytes(dst), original);
}

TEST(TerrainPgmImport, AcceptsEightBitPgm)
{
    TempDir tmp;
    const std::string pgm = tmp / "eight.pgm";
    const std::string dst = tmp / "eight.lmp";
    const std::vector<uint8_t> luma = {0, 17, 34, 51, 68, 85, 102, 119,
                                       136, 153, 170, 187, 204, 221, 238, 255};
    std::string text = "P5\n# generated fixture\n4 4\n255\n";
    text.append(reinterpret_cast<const char*>(luma.data()), luma.size());
    write_text(pgm, text);

    ASSERT_EQ(RunIGI1Conv("terrain import-lmp " + Q(pgm) + " -o " + Q(dst)), 0);
    std::vector<uint8_t> flipped;
    for (uint32_t row = 4; row-- > 0; )
        for (uint32_t col = 0; col < 4; ++col)
            flipped.push_back(luma[row * 4 + col]);
    EXPECT_EQ(read_bytes(dst), make_lmp(4, flipped));
}

TEST(TerrainPgmImport, MultiChunkLmpRoundTripsChunkByChunk)
{
    TempDir tmp;
    const std::vector<uint8_t> a = {1, 2, 3, 4};
    const std::vector<uint8_t> b = {9, 8, 7, 6, 5, 4, 3, 2, 1};
    auto original = make_lmp(2, a);
    const auto tail = make_lmp(3, b);
    original.insert(original.end(), tail.begin(), tail.end());

    const std::string src = tmp / "multi.lmp";
    const std::string pgm0 = tmp / "multi_0.pgm";
    const std::string pgm1 = tmp / "multi_1.pgm";
    const std::string out0 = tmp / "out_0.lmp";
    const std::string out1 = tmp / "out_1.lmp";
    write_bytes(src, original);

    ASSERT_EQ(RunIGI1Conv("terrain export-lmp " + Q(src) + " -o " + Q(tmp / "multi.pgm")), 0);
    ASSERT_EQ(RunIGI1Conv("terrain import-lmp " + Q(pgm0) + " -o " + Q(out0)), 0);
    ASSERT_EQ(RunIGI1Conv("terrain import-lmp " + Q(pgm1) + " -o " + Q(out1)), 0);

    auto rebuilt = read_bytes(out0);
    const auto second = read_bytes(out1);
    rebuilt.insert(rebuilt.end(), second.begin(), second.end());
    EXPECT_EQ(rebuilt, original);
}

TEST(TerrainPgmImport, RejectsInvalidPgm)
{
    TempDir tmp;
    const std::string bad_magic = tmp / "bad_magic.pgm";
    const std::string non_square = tmp / "non_square.pgm";
    const std::string truncated = tmp / "truncated.pgm";
    write_text(bad_magic, "P6\n2 2\n255\n\0\0\0\0\0\0\0\0\0\0\0\0");
    write_text(non_square, "P5\n2 3\n255\n\0\0\0\0\0\0");
    write_text(truncated, "P5\n4 4\n65535\n\0\0");

    EXPECT_NE(RunIGI1Conv("terrain import-lmp " + Q(bad_magic) + " -o " + Q(tmp / "bad1.lmp")), 0);
    EXPECT_NE(RunIGI1Conv("terrain import-lmp " + Q(non_square) + " -o " + Q(tmp / "bad2.lmp")), 0);
    EXPECT_NE(RunIGI1Conv("terrain import-lmp " + Q(truncated) + " -o " + Q(tmp / "bad3.lmp")), 0);
}
