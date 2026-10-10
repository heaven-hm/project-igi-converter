#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace gconv
{
namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;

[[noreturn]] void fail(const std::string &message);
void requireRange(const Bytes &bytes, size_t position, size_t length);
uint16_t readU16(const Bytes &bytes, size_t position);
uint32_t readU32(const Bytes &bytes, size_t position);
void writeUnsigned(Bytes &bytes, size_t position, uint32_t value, size_t width = 4);
bool hasTag(const Bytes &bytes, size_t position, const char *expected);
Bytes readFile(const fs::path &path);
void writeFile(const fs::path &path, const Bytes &bytes);
void requireDifferentFiles(const fs::path &input, const fs::path &output,
                           const std::string &message);
struct Chunk
{
    std::string name;
    size_t position, size;
    uint32_t alignment;
};
std::vector<Chunk> walkIlffChunks(const Bytes &bytes, const char *type);

struct Image
{
    uint32_t width;
    uint32_t height;
    uint16_t bitsPerPixel;
    Bytes pixels;
};
void validateImage(const Image &image);
Image loadImage(const fs::path &path);
void saveImage(const fs::path &path, const Image &image);
Bytes toRgba(const Image &image);
Image fromRgba(uint32_t width, uint32_t height, const Bytes &rgba, uint16_t bitsPerPixel);

struct ResourceEntry
{
    std::string name;
    Bytes bytes;
};
std::vector<ResourceEntry> readResources(const Bytes &bytes);
void listResources(const fs::path &input);
void extractResources(const fs::path &input, const fs::path &output);
void packResources(const fs::path &directory, const fs::path &output);

struct FontData
{
    Bytes bytes;
    Image atlas;
    size_t bodyPosition;
};
FontData readFont(const fs::path &path);
void showFontInfo(const fs::path &path);
void extractFont(const fs::path &input, const fs::path &output);
void packFont(const fs::path &templatePath, const fs::path &imagePath, const fs::path &outputPath);

std::vector<Image> readTextureBitmaps(const fs::path &path);
std::vector<Image> makeMipmaps(Image image);
void extractTexture(const fs::path &input, const fs::path &outputDirectory);
void createTexture(const fs::path &imagePath, const fs::path &outputPath,
                   bool mipmaps = false);
void packTexture(const fs::path &templatePath, const fs::path &bitmapDirectory,
                 const fs::path &outputPath);
void compileScript(const fs::path &input, const fs::path &output);
} // namespace gconv
