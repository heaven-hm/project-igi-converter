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
        requireRange(b, 52, count * (v == 9 ? 32 : 40));
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
    for (size_t n = 0; n < (v == 11 ? 16 : count); ++n)
    {
        uint32_t width = v == 11 ? w >> n : w, height = v == 11 ? h >> n : h;
        if (v == 7 || v == 9)
        {
            const size_t record = 52 + n * (v == 9 ? 32 : 40);
            const size_t tableEnd = 52 + count * (v == 9 ? 32 : 40);
            offset = readU32(b, record);
            if (offset < tableEnd)
                fail("Sprite bitmap overlaps its metadata table");
            if (v == 9)
            {
                mode = readU32(b, record + 4);
                width = readU16(b, record + 10);
                height = readU16(b, record + 12);
            }
            else
            {
                width = readU32(b, record + 8);
                height = readU32(b, record + 12);
            }
            if (!width || !height || width > 65535 || height > 65535)
                fail("Invalid sprite bitmap dimensions");
        }
        if (!width || !height)
            break;
        const uint16_t bits = mode == 2 ? 16 : (mode == 3 || mode == 67) ? 32 : 0;
        if (!bits)
            fail("Unsupported texture mode (VQ decoding is not implemented)");
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
std::vector<Image> makeMipmaps(Image image)
{
    validateImage(image);
    if (image.bitsPerPixel != 32)
        fail("Mipmap generation requires 32-bit BGRA pixels");
    std::vector<Image> levels;
    levels.push_back(std::move(image));
    while (levels.back().width > 1 && levels.back().height > 1)
    {
        const auto &previous = levels.back();
        const uint32_t width = previous.width / 2, height = previous.height / 2;
        Bytes pixels(size_t(width) * height * 4);
        for (uint32_t y = 0; y < height; ++y)
            for (uint32_t x = 0; x < width; ++x)
                for (size_t channel = 0; channel < 4; ++channel)
                {
                    const size_t top = (size_t(y * 2) * previous.width + x * 2) * 4 + channel;
                    const size_t bottom = top + size_t(previous.width) * 4;
                    pixels[(size_t(y) * width + x) * 4 + channel] =
                        (unsigned(previous.pixels[top]) + previous.pixels[top + 4] +
                         previous.pixels[bottom] + previous.pixels[bottom + 4] + 2) / 4;
                }
        levels.push_back(Image{width, height, 32, std::move(pixels)});
    }
    return levels;
}

void createTexture(const fs::path &src, const fs::path &dst, bool mipmaps)
{
    requireDifferentFiles(src, dst, "Texture output must differ from source image");
    auto i = loadImage(src);
    if (mipmaps && ((i.width & (i.width - 1)) || (i.height & (i.height - 1))))
        fail("Mipmap texture dimensions must be powers of two");
    if (i.bitsPerPixel == 24 || (mipmaps && i.bitsPerPixel != 32))
        i = fromRgba(i.width, i.height, toRgba(i), 32);
    Bytes b(32);
    std::memcpy(b.data(), "LOOP", 4);
    writeUnsigned(b, 4, 11);
    writeUnsigned(b, 8, mipmaps ? 67 : i.bitsPerPixel == 16 ? 2 : 3);
    if (mipmaps)
        writeUnsigned(b, 20, 5, 2); // Original GConv default texture scale factor.
    writeUnsigned(b, 22, i.width, 2);
    writeUnsigned(b, 24, i.height, 2);
    writeUnsigned(b, 26, i.width, 2);
    writeUnsigned(b, 28, i.height, 2);
    writeUnsigned(b, 30, i.bitsPerPixel / 8, 2);
    // GConv ARGB8888 mipmaps average each 2x2 BGRA block with round-to-nearest.
    // Rectangular chains stop when either dimension cannot be halved.
    if (mipmaps)
        for (const auto &level : makeMipmaps(std::move(i)))
            b.insert(b.end(), level.pixels.begin(), level.pixels.end());
    else
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
        if (v == 7 || v == 9)
            offset = readU32(b, 52 + n * (v == 9 ? 32 : 40));
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
