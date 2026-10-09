#include <algorithm>
#include <cstring>
#include <iostream>

#include "common.hpp"
namespace gconv
{
std::vector<Image> readTextureBitmaps(const fs::path &p)
{
    auto b = readFile(p);
    requireRange(b, 0, 8);
    if (!hasTag(b, 0, "LOOP"))
        fail("Expected LOOP texture");
    uint32_t v = readU32(b, 4), mode, w, h;
    size_t offset, count = 1;
    std::vector<Image> images;
    if (v == 11)
    {
        requireRange(b, 0, 32);
        mode = readU32(b, 8);
        w = readU16(b, 26);
        h = readU16(b, 28);
        offset = 32;
    }
    else if (v == 7 || v == 9)
    {
        requireRange(b, 0, 52);
        count = readU32(b, 32);
        w = readU32(b, 40);
        h = readU32(b, 44);
        mode = readU32(b, 48);
        offset = 52 + count * (v == 9 ? 32 : 40);
        if (count > 100000)
            fail("Oversized sprite table");
    }
    else if (v == 2)
    {
        requireRange(b, 0, 24);
        w = readU16(b, 18);
        h = readU16(b, 20);
        mode = readU16(b, 22);
        offset = 24;
    }
    else
        fail("Unsupported LOOP texture version");
    if (!w || !h || w > 65535 || h > 65535)
    {
        fail("Invalid texture dimensions");
    }
    if (!count)
    {
        fail("Texture contains no bitmaps");
    }
    uint16_t bits = mode == 2 ? 16 : (mode == 3 || mode == 67) ? 32 : 0;
    if (!bits)
        fail("Unsupported texture mode (VQ decoding is not implemented)");
    for (size_t n = 0; n < (v == 11 ? 10 : count); ++n)
    {
        uint32_t width = v == 11 ? w >> n : w, height = v == 11 ? h >> n : h;
        if (!width || !height)
            break;
        size_t bytes = size_t(width) * height * (bits / 8);
        if (n > 0 && offset == b.size() && v == 11)
            break;
        requireRange(b, offset, bytes);
        Image i{width, height, bits, Bytes(b.begin() + offset, b.begin() + offset + bytes)};
        validateImage(i);
        images.push_back(std::move(i));
        offset += bytes;
    }
    return images;
}
void createTexture(const fs::path &src, const fs::path &dst)
{
    requireDifferentFiles(src, dst, "Texture output must differ from source image");
    auto i = loadImage(src);
    if (i.bitsPerPixel == 24)
        i = fromRgba(i.width, i.height, toRgba(i), 32);
    Bytes b(32);
    std::memcpy(b.data(), "LOOP", 4);
    writeUnsigned(b, 4, 11);
    writeUnsigned(b, 8, i.bitsPerPixel == 16 ? 2 : 3);
    writeUnsigned(b, 22, i.width, 2);
    writeUnsigned(b, 24, i.height, 2);
    writeUnsigned(b, 26, i.width, 2);
    writeUnsigned(b, 28, i.height, 2);
    writeUnsigned(b, 30, i.bitsPerPixel / 8, 2);
    b.insert(b.end(), i.pixels.begin(), i.pixels.end());
    writeFile(dst, b);
}
void packTexture(const fs::path &src, const fs::path &directory, const fs::path &dst)
{
    requireDifferentFiles(src, dst, "Output must differ from texture template");
    auto images = readTextureBitmaps(src);
    auto b = readFile(src);
    auto v = readU32(b, 4);
    size_t offset = v == 11 ? 32 : v == 2 ? 24 : 52 + readU32(b, 32) * (v == 9 ? 32 : 40);
    for (size_t n = 0; n < images.size(); ++n)
    {
        auto p = directory / (src.stem().string() + "." + std::to_string(n) + ".tga");
        requireDifferentFiles(p, dst, "Bitmap input and output must differ");
        auto i = loadImage(p);
        auto &original = images[n];
        if (i.width != original.width || i.height != original.height)
            fail("Bitmap dimensions must match template: " + p.string());
        i = fromRgba(i.width, i.height, toRgba(i), original.bitsPerPixel);
        requireRange(b, offset, i.pixels.size());
        std::copy(i.pixels.begin(), i.pixels.end(), b.begin() + offset);
        offset += i.pixels.size();
    }
    writeFile(dst, b);
}
void extractTexture(const fs::path &input, const fs::path &outputDirectory)
{
    const auto bitmaps = readTextureBitmaps(input);
    for (size_t index = 0; index < bitmaps.size(); ++index)
    {
        const auto filename =
            fs::path(input).stem().string() + "." + std::to_string(index) + ".tga";
        const auto output = outputDirectory / filename;
        requireDifferentFiles(input, output, "Texture input and image output must differ");
        saveImage(output, bitmaps[index]);
    }
    std::cout << "Extracted " << bitmaps.size() << " bitmap(s)\n";
}
} // namespace gconv
