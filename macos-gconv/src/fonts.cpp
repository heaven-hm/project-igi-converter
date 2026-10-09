#include <algorithm>
#include <iostream>

#include "common.hpp"
namespace gconv {
FontData readFont(const fs::path& path) {
  FontData f{readFile(path), {}, 0};
  size_t header = 0, bodySize = 0;
  for (auto& c : walkIlffChunks(f.bytes, "FONT")) {
    if (c.name == "TEXH" && !header) {
      if (c.size != 24)
        fail("Expected IGI 2 24-byte TEXH");
      header = c.position;
    }
    if (c.name == "BODY" && !f.bodyPosition) {
      f.bodyPosition = c.position;
      bodySize = c.size;
    }
  }
  if (!header || !f.bodyPosition)
    fail("Missing font atlas");
  f.atlas = {readU16(f.bytes, header + 14), readU16(f.bytes, header + 16),
             readU16(f.bytes, header + 22),
             Bytes(f.bytes.begin() + f.bodyPosition,
                   f.bytes.begin() + f.bodyPosition + bodySize)};
  validateImage(f.atlas);
  return f;
}
void showFontInfo(const fs::path& path) {
  const auto font = readFont(path);
  std::cout << "IGI 2 FONT " << font.atlas.width << "x" << font.atlas.height
            << " " << font.atlas.bitsPerPixel << "-bit\n";
}
void extractFont(const fs::path& input, const fs::path& output) {
  saveImage(output, readFont(input).atlas);
}
void packFont(const fs::path& templatePath, const fs::path& imagePath,
              const fs::path& outputPath) {
  if (fs::weakly_canonical(templatePath) == fs::weakly_canonical(outputPath))
    fail("Output must differ from font template");
  auto font = readFont(templatePath);
  auto atlas = loadImage(imagePath);
  if (atlas.width != font.atlas.width || atlas.height != font.atlas.height)
    fail("Font atlas dimensions must match template");
  atlas = fromRgba(atlas.width, atlas.height, toRgba(atlas),
                   font.atlas.bitsPerPixel);
  std::copy(atlas.pixels.begin(), atlas.pixels.end(),
            font.bytes.begin() + font.bodyPosition);
  writeFile(outputPath, font.bytes);
}
}  // namespace gconv
