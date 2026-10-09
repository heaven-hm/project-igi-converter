#include <algorithm>
#include <iostream>

#include "common.hpp"
namespace gconv
{
FontData readFont(const fs::path &path)
{
    FontData f{readFile(path), {}, 0};
    size_t header = 0, bodySize = 0;
    for (auto &c : walkIlffChunks(f.bytes, "FONT"))
    {
        if (c.name == "TEXH" && !header)
        {
            if (c.size != 24)
                fail("Expected 24-byte font TEXH");
            header = c.position;
        }
        if (c.name == "BODY" && !f.bodyPosition)
        {
            f.bodyPosition = c.position;
            bodySize = c.size;
        }
    }
    if (!header || !f.bodyPosition)
        fail("Missing font atlas");
    f.atlas = {
        readU16(f.bytes, header + 14), readU16(f.bytes, header + 16), readU16(f.bytes, header + 22),
        Bytes(f.bytes.begin() + f.bodyPosition, f.bytes.begin() + f.bodyPosition + bodySize)};
    validateImage(f.atlas);
    return f;
}
void showFontInfo(const fs::path &path)
{
    const auto font = readFont(path);
    std::cout << "FONT " << font.atlas.width << "x" << font.atlas.height << " "
              << font.atlas.bitsPerPixel << "-bit\n";
}
void extractFont(const fs::path &input, const fs::path &output)
{
    requireDifferentFiles(input, output, "Font input and image output must differ");
    saveImage(output, readFont(input).atlas);
}
void packFont(const fs::path &templatePath, const fs::path &imagePath, const fs::path &outputPath)
{
    requireDifferentFiles(templatePath, outputPath, "Output must differ from font template");
    requireDifferentFiles(imagePath, outputPath, "Bitmap input and output must differ");
    auto font = readFont(templatePath);
    auto atlas = loadImage(imagePath);
    if (atlas.width != font.atlas.width || atlas.height != font.atlas.height)
        fail("Font atlas dimensions must match template");
    atlas = fromRgba(atlas.width, atlas.height, toRgba(atlas), font.atlas.bitsPerPixel);
    std::copy(atlas.pixels.begin(), atlas.pixels.end(), font.bytes.begin() + font.bodyPosition);
    writeFile(outputPath, font.bytes);
}
} // namespace gconv
