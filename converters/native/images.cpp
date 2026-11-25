#include <algorithm>

#include "common.hpp"
#include "tinygltf/stb_image.h"
#include "tinygltf/stb_image_write.h"
namespace gconv
{
void validateImage(const Image &i)
{
    if (!i.width || !i.height || i.width > 65535 || i.height > 65535 ||
        (i.bitsPerPixel != 16 && i.bitsPerPixel != 24 && i.bitsPerPixel != 32) ||
        uint64_t(i.width) * i.height * (i.bitsPerPixel / 8) != i.pixels.size())
        fail("Invalid image dimensions/depth");
}
Bytes toRgba(const Image &i)
{
    validateImage(i);
    Bytes r(size_t(i.width) * i.height * 4);
    for (size_t p = 0; p < size_t(i.width) * i.height; ++p)
    {
        auto dst = p * 4;
        auto src = p * (i.bitsPerPixel / 8);
        if (i.bitsPerPixel == 16)
        {
            uint16_t v = readU16(i.pixels, src);
            auto expand = [](int v) { return uint8_t((v << 3) | (v >> 2)); };
            r[dst] = expand((v >> 10) & 31);
            r[dst + 1] = expand((v >> 5) & 31);
            r[dst + 2] = expand(v & 31);
            r[dst + 3] = (v & 0x8000) ? 255 : 0;
        }
        else
        {
            r[dst] = i.pixels[src + 2];
            r[dst + 1] = i.pixels[src + 1];
            r[dst + 2] = i.pixels[src];
            r[dst + 3] = i.bitsPerPixel == 32 ? i.pixels[src + 3] : 255;
        }
    }
    return r;
}
Image fromRgba(uint32_t w, uint32_t h, const Bytes &r, uint16_t bits)
{
    if (r.size() != uint64_t(w) * h * 4)
        fail("Invalid RGBA size");
    Image i{w, h, bits, Bytes(size_t(w) * h * (bits / 8))};
    validateImage(i);
    for (size_t p = 0; p < size_t(w) * h; ++p)
    {
        size_t d = p * (bits / 8), s = p * 4;
        if (bits == 16)
            writeUnsigned(i.pixels, d,
                          (r[s + 3] >= 128 ? 0x8000 : 0) | ((r[s] >> 3) << 10) |
                              ((r[s + 1] >> 3) << 5) | (r[s + 2] >> 3),
                          2);
        else
        {
            i.pixels[d] = r[s + 2];
            i.pixels[d + 1] = r[s + 1];
            i.pixels[d + 2] = r[s];
            if (bits == 32)
                i.pixels[d + 3] = r[s + 3];
        }
    }
    return i;
}
void saveImage(const fs::path &p, const Image &i)
{
    validateImage(i);
    auto extension = p.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (extension == ".png" || extension == ".bmp" || extension == ".jpg" || extension == ".jpeg")
    {
        auto r = toRgba(i);
        Bytes encoded;
        auto callback = [](void *context, void *data, int size)
        {
            auto &bytes = *static_cast<Bytes *>(context);
            const auto *first = static_cast<const uint8_t *>(data);
            bytes.insert(bytes.end(), first, first + size);
        };
        int success = 0;
        if (extension == ".png")
            success = stbi_write_png_to_func(callback, &encoded, i.width, i.height, 4, r.data(),
                                             i.width * 4);
        else if (extension == ".bmp")
            success = stbi_write_bmp_to_func(callback, &encoded, i.width, i.height, 4, r.data());
        else
            success =
                stbi_write_jpg_to_func(callback, &encoded, i.width, i.height, 4, r.data(), 95);
        if (!success)
            fail("Cannot encode image");
        writeFile(p, encoded);
    }
    else if (extension == ".tga")
    {
        Bytes b(18);
        b[2] = 2;
        writeUnsigned(b, 12, i.width, 2);
        writeUnsigned(b, 14, i.height, 2);
        b[16] = uint8_t(i.bitsPerPixel);
        b[17] = uint8_t(32 + (i.bitsPerPixel == 16 ? 1 : i.bitsPerPixel == 32 ? 8 : 0));
        b.insert(b.end(), i.pixels.begin(), i.pixels.end());
        writeFile(p, b);
    }
    else
        fail("Image output must end in .png, .tga, .bmp or .jpg");
}
Image loadImage(const fs::path &p)
{
    auto extension = p.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (extension == ".png" || extension == ".bmp" || extension == ".jpg" || extension == ".jpeg")
    {
        const auto bytes = readFile(p);
        int width = 0, height = 0, channels = 0;
        if (!stbi_info_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height,
                                   &channels))
            fail("Invalid image header");
        if (width <= 0 || height <= 0 || width > 65535 || height > 65535 ||
            uint64_t(width) * height > 100000000)
            fail("Oversized image");
        auto *pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width,
                                             &height, &channels, 4);
        if (!pixels)
            fail("Cannot decode image");
        Bytes rgba(pixels, pixels + size_t(width) * height * 4);
        stbi_image_free(pixels);
        return fromRgba(width, height, rgba, 32);
    }
    auto b = readFile(p);
    requireRange(b, 0, 18);
    if (b[1] || (b[2] != 2 && b[2] != 10))
        fail("Expected true-color TGA (raw or RLE)");
    Image i{readU16(b, 12), readU16(b, 14), b[16], {}};
    if (i.bitsPerPixel != 16 && i.bitsPerPixel != 24 && i.bitsPerPixel != 32)
        fail("Unsupported TGA depth");
    if (!i.width || !i.height)
        fail("Empty TGA");
    size_t stride = i.bitsPerPixel / 8, total = size_t(i.width) * i.height, offset = 18 + b[0];
    Bytes raw;
    raw.reserve(total * stride);
    if (b[2] == 2)
    {
        requireRange(b, offset, total * stride);
        raw.assign(b.begin() + offset, b.begin() + offset + total * stride);
    }
    else
        while (raw.size() < total * stride)
        {
            requireRange(b, offset, 1);
            uint8_t control = b[offset++];
            size_t n = (control & 127) + 1;
            if (n > (total * stride - raw.size()) / stride)
                fail("TGA RLE overrun");
            if (control & 128)
            {
                requireRange(b, offset, stride);
                for (size_t t = 0; t < n; ++t)
                    raw.insert(raw.end(), b.begin() + offset, b.begin() + offset + stride);
                offset += stride;
            }
            else
            {
                requireRange(b, offset, n * stride);
                raw.insert(raw.end(), b.begin() + offset, b.begin() + offset + n * stride);
                offset += n * stride;
            }
        }
    i.pixels.resize(raw.size());
    for (size_t y = 0; y < i.height; ++y)
        for (size_t x = 0; x < i.width; ++x)
        {
            size_t sx = (b[17] & 16) ? i.width - 1 - x : x,
                   sy = (b[17] & 32) ? y : i.height - 1 - y;
            std::copy_n(raw.begin() + (sy * i.width + sx) * stride, stride,
                        i.pixels.begin() + (y * i.width + x) * stride);
        }
    validateImage(i);
    return i;
}
} // namespace gconv
