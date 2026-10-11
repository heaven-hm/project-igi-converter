#pragma once
#include <string>
#include <vector>
#include <cstdint>

#pragma pack(push, 1)
struct OlmMainHeader {
    float version1;
    float version2;
    uint32_t year;
    uint32_t month;
    uint32_t day;
    uint32_t hour;
    uint32_t minute;
    uint32_t second;
    uint32_t millisecond;
    uint32_t unknown_0;
    uint32_t count1;
    uint32_t layer_count;
    uint32_t reserved[4];
    uint16_t width;
    uint16_t height;
    uint16_t total_stride;
    uint16_t format;
    uint32_t pad;
    float uv_scale_u;
    float uv_scale_v;
    float zero;
};

struct OlmLayerDescriptor {
    uint32_t flags;
    uint32_t ptr1;
    uint32_t ptr2;
    uint16_t pixel_width;
    uint16_t pixel_height;
};

struct OlmPixel {
    uint8_t r, g, b, a;
};
#pragma pack(pop)

struct OLMFile {
    bool valid = false;
    std::string error;
    OlmMainHeader header;
    OlmLayerDescriptor layer;
    std::vector<OlmPixel> pixels;
};

OLMFile ParseOlm(const std::string& path);
int cmd_olm(int argc, char** argv);
