#include "qsc/qvm_compiler.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <png.h>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;
[[noreturn]] void fail(const std::string &s) { throw std::runtime_error(s); }
void range(const Bytes &b, size_t p, size_t n) {
  if (p > b.size() || n > b.size() - p)
    fail("Truncated input at byte " + std::to_string(p));
}
uint16_t u16(const Bytes &b, size_t p) {
  range(b, p, 2);
  return b[p] | uint16_t(b[p + 1]) << 8;
}
uint32_t u32(const Bytes &b, size_t p) {
  range(b, p, 4);
  return b[p] | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 |
         uint32_t(b[p + 3]) << 24;
}
void put(Bytes &b, size_t p, uint32_t v, size_t n = 4) {
  range(b, p, n);
  for (size_t i = 0; i < n; ++i)
    b[p + i] = uint8_t(v >> (8 * i));
}
void append(Bytes &b, uint32_t v, size_t n = 4) {
  size_t p = b.size();
  b.resize(p + n);
  put(b, p, v, n);
}
bool tag(const Bytes &b, size_t p, const char *t) {
  range(b, p, 4);
  return std::memcmp(b.data() + p, t, 4) == 0;
}
Bytes read(const fs::path &p) {
  std::ifstream f(p, std::ios::binary);
  if (!f)
    fail("Cannot open " + p.string());
  f.seekg(0, std::ios::end);
  auto n = f.tellg();
  if (n < 0 || n > 1024LL * 1024 * 1024)
    fail("Invalid or oversized input");
  Bytes b(static_cast<size_t>(n));
  f.seekg(0);
  if (!b.empty() && !f.read(reinterpret_cast<char *>(b.data()), b.size()))
    fail("Read failed");
  return b;
}
void write(const fs::path &p, const Bytes &b) {
  if (p.has_parent_path())
    fs::create_directories(p.parent_path());
  std::ofstream f(p, std::ios::binary);
  if (!f || (!b.empty() &&
             !f.write(reinterpret_cast<const char *>(b.data()), b.size())))
    fail("Write failed: " + p.string());
}
struct Chunk {
  std::string name;
  size_t pos, size;
  uint32_t alignment;
};
std::vector<Chunk> chunks(const Bytes &b, const char *type) {
  range(b, 0, 20);
  if (!tag(b, 0, "ILFF") || !tag(b, 16, type))
    fail(std::string("Expected ILFF/") + type);
  std::vector<Chunk> c;
  size_t p = 20;
  while (p < b.size()) {
    range(b, p, 16);
    size_t n = u32(b, p + 4), next = u32(b, p + 12);
    range(b, p + 16, n);
    c.push_back({std::string(reinterpret_cast<const char *>(b.data() + p), 4),
                 p + 16, n, u32(b, p + 8)});
    if (!next)
      break;
    if (next < 16 + n || next > b.size() - p)
      fail("Invalid ILFF chunk advance");
    p += next;
  }
  return c;
}
struct Entry {
  std::string name;
  Bytes bytes;
};
std::vector<Entry> resources(const Bytes &b) {
  std::vector<Entry> e;
  std::string name;
  bool have = false;
  for (auto c : chunks(b, "IRES")) {
    if (c.name == "NAME") {
      name.assign(reinterpret_cast<const char *>(b.data() + c.pos), c.size);
      while (!name.empty() && !name.back())
        name.pop_back();
      have = true;
    } else if (c.name == "BODY") {
      if (!have)
        fail("BODY without NAME");
      e.push_back({name, Bytes(b.begin() + c.pos, b.begin() + c.pos + c.size)});
      have = false;
    }
  }
  return e;
}
fs::path resourcePath(std::string s) {
  if (s.substr(0, 6) == "LOCAL:")
    s.erase(0, 6);
  std::replace(s.begin(), s.end(), '\\', '/');
  fs::path p(s);
  if (p.empty() || p.is_absolute() || s.find(':') != std::string::npos)
    fail("Unsafe resource name: " + s);
  for (auto part : p)
    if (part == "..")
      fail("Unsafe resource name: " + s);
  return p;
}
void extractResources(const fs::path &input, const fs::path &output) {
  auto e = resources(read(input));
  auto root = fs::weakly_canonical(fs::absolute(output));
  std::vector<fs::path> targets;
  for (auto &v : e) {
    auto p = root / resourcePath(v.name);
    auto canonical = fs::weakly_canonical(p);
    auto relative = canonical.lexically_relative(root);
    if (relative.empty() || *relative.begin() == "..")
      fail("Resource path leaves output directory");
    std::string lowered = canonical.string();
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    for (auto &t : targets)
      if (t.string() == lowered)
        fail("Duplicate resource output: " + v.name);
    targets.emplace_back(lowered);
    if (fs::exists(p))
      fail("Output already exists: " + p.string());
  }
  for (auto &v : e)
    write(root / resourcePath(v.name), v.bytes);
  std::cout << "Extracted " << e.size() << " resources\n";
}
void packResources(const fs::path &dir, const fs::path &output) {
  if (!fs::is_directory(dir))
    fail("Resource input must be a directory");
  auto out = fs::absolute(output).lexically_normal();
  std::vector<fs::path> files;
  for (auto &p : fs::recursive_directory_iterator(dir)) {
    if (p.is_symlink())
      fail("Symlink in resource input");
    if (p.is_regular_file() && fs::absolute(p.path()).lexically_normal() != out)
      files.push_back(p.path());
  }
  std::sort(files.begin(), files.end());
  Bytes b(20);
  std::memcpy(b.data(), "ILFF", 4);
  put(b, 8, 4);
  std::memcpy(b.data() + 16, "IRES", 4);
  size_t previous = 0;
  auto add = [&](const char *id, const Bytes &data) {
    size_t p = b.size();
    if (previous)
      put(b, previous + 12, uint32_t(p - previous));
    previous = p;
    b.resize(p + 16);
    std::memcpy(b.data() + p, id, 4);
    put(b, p + 4, uint32_t(data.size()));
    put(b, p + 8, 4);
    b.insert(b.end(), data.begin(), data.end());
    while (b.size() % 4)
      b.push_back(0);
  };
  for (auto &p : files) {
    auto s = "LOCAL:" + fs::relative(p, dir).generic_string();
    Bytes name(s.begin(), s.end());
    name.push_back(0);
    while (name.size() % 4)
      name.push_back(0);
    add("NAME", name);
    add("BODY", read(p));
  }
  put(b, 4, uint32_t(b.size()));
  write(output, b);
  std::cout << "Packed " << files.size() << " resources\n";
}
struct Image {
  uint32_t w, h;
  uint16_t bits;
  Bytes pixels;
};
void checkImage(const Image &i) {
  if (!i.w || !i.h || i.w > 65535 || i.h > 65535 ||
      (i.bits != 16 && i.bits != 24 && i.bits != 32) ||
      uint64_t(i.w) * i.h * (i.bits / 8) != i.pixels.size())
    fail("Invalid image dimensions/depth");
}
Bytes rgba(const Image &i) {
  checkImage(i);
  Bytes r(size_t(i.w) * i.h * 4);
  for (size_t p = 0; p < size_t(i.w) * i.h; ++p) {
    auto dst = p * 4;
    auto src = p * (i.bits / 8);
    if (i.bits == 16) {
      uint16_t v = u16(i.pixels, src);
      auto expand = [](int v) { return uint8_t((v << 3) | (v >> 2)); };
      r[dst] = expand((v >> 10) & 31);
      r[dst + 1] = expand((v >> 5) & 31);
      r[dst + 2] = expand(v & 31);
      r[dst + 3] = (v & 0x8000) ? 255 : 0;
    } else {
      r[dst] = i.pixels[src + 2];
      r[dst + 1] = i.pixels[src + 1];
      r[dst + 2] = i.pixels[src];
      r[dst + 3] = i.bits == 32 ? i.pixels[src + 3] : 255;
    }
  }
  return r;
}
Image fromRGBA(uint32_t w, uint32_t h, const Bytes &r, uint16_t bits) {
  if (r.size() != uint64_t(w) * h * 4)
    fail("Invalid RGBA size");
  Image i{w, h, bits, Bytes(size_t(w) * h * (bits / 8))};
  checkImage(i);
  for (size_t p = 0; p < size_t(w) * h; ++p) {
    size_t d = p * (bits / 8), s = p * 4;
    if (bits == 16)
      put(i.pixels, d,
          (r[s + 3] >= 128 ? 0x8000 : 0) | ((r[s] >> 3) << 10) |
              ((r[s + 1] >> 3) << 5) | (r[s + 2] >> 3),
          2);
    else {
      i.pixels[d] = r[s + 2];
      i.pixels[d + 1] = r[s + 1];
      i.pixels[d + 2] = r[s];
      if (bits == 32)
        i.pixels[d + 3] = r[s + 3];
    }
  }
  return i;
}
void saveImage(const fs::path &p, const Image &i) {
  checkImage(i);
  if (p.extension() == ".png") {
    auto r = rgba(i);
    png_image img{};
    img.version = PNG_IMAGE_VERSION;
    img.width = i.w;
    img.height = i.h;
    img.format = PNG_FORMAT_RGBA;
    if (p.has_parent_path())
      fs::create_directories(p.parent_path());
    if (!png_image_write_to_file(&img, p.c_str(), 0, r.data(), 0, nullptr))
      fail(img.message);
    png_image_free(&img);
  } else if (p.extension() == ".tga") {
    Bytes b(18);
    b[2] = 2;
    put(b, 12, i.w, 2);
    put(b, 14, i.h, 2);
    b[16] = uint8_t(i.bits);
    b[17] = uint8_t(32 + (i.bits == 16 ? 1 : i.bits == 32 ? 8 : 0));
    b.insert(b.end(), i.pixels.begin(), i.pixels.end());
    write(p, b);
  } else
    fail("Image output must end in .png or .tga");
}
Image loadImage(const fs::path &p) {
  if (p.extension() == ".png") {
    png_image img{};
    img.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_file(&img, p.c_str()))
      fail(img.message);
    img.format = PNG_FORMAT_RGBA;
    if (img.width > 65535 || img.height > 65535 ||
        uint64_t(img.width) * img.height > 100000000) {
      png_image_free(&img);
      fail("Oversized PNG");
    }
    Bytes r(PNG_IMAGE_SIZE(img));
    if (!png_image_finish_read(&img, nullptr, r.data(), 0, nullptr)) {
      std::string msg = img.message;
      png_image_free(&img);
      fail(msg);
    }
    auto i = fromRGBA(img.width, img.height, r, 32);
    png_image_free(&img);
    return i;
  }
  auto b = read(p);
  range(b, 0, 18);
  if (b[1] || (b[2] != 2 && b[2] != 10))
    fail("Expected true-color TGA (raw or RLE)");
  Image i{u16(b, 12), u16(b, 14), b[16], {}};
  if (i.bits != 16 && i.bits != 24 && i.bits != 32)
    fail("Unsupported TGA depth");
  if (!i.w || !i.h)
    fail("Empty TGA");
  size_t stride = i.bits / 8, total = size_t(i.w) * i.h, offset = 18 + b[0];
  Bytes raw;
  raw.reserve(total * stride);
  if (b[2] == 2) {
    range(b, offset, total * stride);
    raw.assign(b.begin() + offset, b.begin() + offset + total * stride);
  } else
    while (raw.size() < total * stride) {
      range(b, offset, 1);
      uint8_t control = b[offset++];
      size_t n = (control & 127) + 1;
      if (n > (total * stride - raw.size()) / stride)
        fail("TGA RLE overrun");
      if (control & 128) {
        range(b, offset, stride);
        for (size_t t = 0; t < n; ++t)
          raw.insert(raw.end(), b.begin() + offset,
                     b.begin() + offset + stride);
        offset += stride;
      } else {
        range(b, offset, n * stride);
        raw.insert(raw.end(), b.begin() + offset,
                   b.begin() + offset + n * stride);
        offset += n * stride;
      }
    }
  i.pixels.resize(raw.size());
  for (size_t y = 0; y < i.h; ++y)
    for (size_t x = 0; x < i.w; ++x) {
      size_t sx = (b[17] & 16) ? i.w - 1 - x : x,
             sy = (b[17] & 32) ? y : i.h - 1 - y;
      std::copy_n(raw.begin() + (sy * i.w + sx) * stride, stride,
                  i.pixels.begin() + (y * i.w + x) * stride);
    }
  checkImage(i);
  return i;
}
struct Font {
  Bytes data;
  Image image;
  size_t body;
};
Font font(const fs::path &path) {
  Font f{read(path), {}, 0};
  size_t header = 0, bodySize = 0;
  for (auto &c : chunks(f.data, "FONT")) {
    if (c.name == "TEXH" && !header) {
      if (c.size != 24)
        fail("Expected IGI 2 24-byte TEXH");
      header = c.pos;
    }
    if (c.name == "BODY" && !f.body) {
      f.body = c.pos;
      bodySize = c.size;
    }
  }
  if (!header || !f.body)
    fail("Missing font atlas");
  f.image = {
      u16(f.data, header + 14), u16(f.data, header + 16),
      u16(f.data, header + 22),
      Bytes(f.data.begin() + f.body, f.data.begin() + f.body + bodySize)};
  checkImage(f.image);
  return f;
}
std::vector<Image> textures(const fs::path &p) {
  auto b = read(p);
  range(b, 0, 8);
  if (!tag(b, 0, "LOOP"))
    fail("Expected LOOP texture");
  uint32_t v = u32(b, 4), mode, w, h;
  size_t offset, count = 1;
  std::vector<Image> images;
  if (v == 11) {
    range(b, 0, 32);
    mode = u32(b, 8);
    w = u16(b, 26);
    h = u16(b, 28);
    offset = 32;
  } else if (v == 7 || v == 9) {
    range(b, 0, 52);
    count = u32(b, 32);
    w = u32(b, 40);
    h = u32(b, 44);
    mode = u32(b, 48);
    offset = 52 + count * (v == 9 ? 32 : 40);
    if (count > 100000)
      fail("Oversized sprite table");
  } else if (v == 2) {
    range(b, 0, 24);
    w = u16(b, 18);
    h = u16(b, 20);
    mode = u16(b, 22);
    offset = 24;
  } else
    fail("Unsupported LOOP texture version");
  uint16_t bits = mode == 2 ? 16 : (mode == 3 || mode == 67) ? 32 : 0;
  if (!bits)
    fail("Unsupported texture mode (VQ decoding is not implemented)");
  for (size_t n = 0; n < (v == 11 ? 10 : count); ++n) {
    uint32_t width = v == 11 ? w >> n : w, height = v == 11 ? h >> n : h;
    if (!width || !height)
      break;
    size_t bytes = size_t(width) * height * (bits / 8);
    if (offset == b.size() && v == 11)
      break;
    range(b, offset, bytes);
    Image i{width, height, bits,
            Bytes(b.begin() + offset, b.begin() + offset + bytes)};
    checkImage(i);
    images.push_back(std::move(i));
    offset += bytes;
  }
  return images;
}
void makeTexture(const fs::path &src, const fs::path &dst) {
  auto i = loadImage(src);
  if (i.bits == 24)
    i = fromRGBA(i.w, i.h, rgba(i), 32);
  Bytes b(32);
  std::memcpy(b.data(), "LOOP", 4);
  put(b, 4, 11);
  put(b, 8, i.bits == 16 ? 2 : 3);
  put(b, 22, i.w, 2);
  put(b, 24, i.h, 2);
  put(b, 26, i.w, 2);
  put(b, 28, i.h, 2);
  put(b, 30, i.bits / 8, 2);
  b.insert(b.end(), i.pixels.begin(), i.pixels.end());
  write(dst, b);
}
void packTexture(const fs::path &src, const fs::path &directory,
                 const fs::path &dst) {
  if (fs::weakly_canonical(src) == fs::weakly_canonical(dst))
    fail("Output must differ from texture template");
  auto images = textures(src);
  auto b = read(src);
  auto v = u32(b, 4);
  size_t offset = v == 11  ? 32
                  : v == 2 ? 24
                           : 52 + u32(b, 32) * (v == 9 ? 32 : 40);
  for (size_t n = 0; n < images.size(); ++n) {
    auto p =
        directory / (src.stem().string() + "." + std::to_string(n) + ".tga");
    auto i = loadImage(p);
    auto &original = images[n];
    if (i.w != original.w || i.h != original.h)
      fail("Bitmap dimensions must match template: " + p.string());
    i = fromRGBA(i.w, i.h, rgba(i), original.bits);
    range(b, offset, i.pixels.size());
    std::copy(i.pixels.begin(), i.pixels.end(), b.begin() + offset);
    offset += i.pixels.size();
  }
  write(dst, b);
}
void compileScript(const fs::path &src, const fs::path &dst) {
  auto b = read(src);
  auto lex = qsc::Lex(std::string(b.begin(), b.end()));
  if (!lex.ok)
    fail(lex.error);
  auto ast = qsc::Parse(lex.tokens);
  if (!ast.ok)
    fail(ast.error);
  auto result = qvm::Compile(*ast.program);
  if (!result.ok)
    fail(result.error);
  write(dst, result.binary);
}
void usage() {
  std::cout
      << "macos-gconv 0.1.0 — native C++17 IGI 2 CLI\n"
         "  gconv qsc compile INPUT.qsc OUTPUT.qvm\n  gconv res list "
         "INPUT.res\n  gconv res extract INPUT.res "
         "DIRECTORY\n  gconv res pack DIRECTORY OUTPUT.res\n"
         "  gconv fnt info INPUT.fnt\n  gconv fnt extract INPUT.fnt "
         "OUTPUT.png|OUTPUT.tga\n  gconv fnt pack TEMPLATE.fnt "
         "IMAGE.png|IMAGE.tga OUTPUT.fnt\n"
         "  gconv tex extract INPUT.tex|INPUT.spr|INPUT.pic DIRECTORY\n  gconv "
         "tex make IMAGE.png|IMAGE.tga OUTPUT.tex\n  gconv tex pack "
         "TEMPLATE.tex|TEMPLATE.spr|TEMPLATE.pic BITMAP_DIRECTORY OUTPUT\n"
         "This native build does not execute QSC recipes or load Windows "
         "DLLs.\n";
}
int main(int argc, char **argv) {
  try {
    if (argc < 2 || std::string(argv[1]) == "--help") {
      usage();
      return 0;
    }
    if (std::string(argv[1]) == "--version") {
      std::cout << "macos-gconv 0.1.0 (C++17 native)\n";
      return 0;
    }
    if (argc < 4)
      fail("Missing arguments; use --help");
    std::string type = argv[1], op = argv[2];
    if (type == "qsc" && op == "compile" && argc == 5) {
      compileScript(argv[3], argv[4]);
    } else if (type == "res" && op == "list" && argc == 4) {
      for (auto &e : resources(read(argv[3])))
        std::cout << e.bytes.size() << "\t" << e.name << "\n";
    } else if (type == "res" && op == "extract" && argc == 5)
      extractResources(argv[3], argv[4]);
    else if (type == "res" && op == "pack" && argc == 5)
      packResources(argv[3], argv[4]);
    else if (type == "fnt" && op == "info" && argc == 4) {
      auto f = font(argv[3]);
      std::cout << "IGI 2 FONT " << f.image.w << "x" << f.image.h << " "
                << f.image.bits << "-bit\n";
    } else if (type == "fnt" && op == "extract" && argc == 5)
      saveImage(argv[4], font(argv[3]).image);
    else if (type == "fnt" && op == "pack" && argc == 6) {
      if (fs::weakly_canonical(argv[3]) == fs::weakly_canonical(argv[5]))
        fail("Output must differ from font template");
      auto f = font(argv[3]);
      auto i = loadImage(argv[4]);
      if (i.w != f.image.w || i.h != f.image.h)
        fail("Font atlas dimensions must match template");
      i = fromRGBA(i.w, i.h, rgba(i), f.image.bits);
      std::copy(i.pixels.begin(), i.pixels.end(), f.data.begin() + f.body);
      write(argv[5], f.data);
    } else if (type == "tex" && op == "extract" && argc == 5) {
      auto images = textures(argv[3]);
      for (size_t n = 0; n < images.size(); ++n)
        saveImage(fs::path(argv[4]) / (fs::path(argv[3]).stem().string() + "." +
                                       std::to_string(n) + ".tga"),
                  images[n]);
      std::cout << "Extracted " << images.size() << " bitmap(s)\n";
    } else if (type == "tex" && op == "make" && argc == 5)
      makeTexture(argv[3], argv[4]);
    else if (type == "tex" && op == "pack" && argc == 6)
      packTexture(argv[3], argv[4], argv[5]);
    else
      fail("Unsupported command or argument count; use --help");
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "gconv: " << e.what() << "\n";
    return 1;
  }
}
