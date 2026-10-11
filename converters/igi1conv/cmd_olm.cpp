#include "pch.h"
#include "cmd_olm.h"
#include "../../third_party/tinygltf/stb_image_write.h"
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

static void print_olm_help()
{
    std::cout <<
        "Usage: igi1conv olm <subcommand> [options]\n"
        "\n"
        "Subcommands:\n"
        "  info   <input.olm>\n"
        "  to-png <input.olm> [-o <out.png>]\n"
        "  to-tga <input.olm> [-o <out.tga>]\n"
        "\n"
        "Exit codes: 0=success 1=bad args 2=file not found 3=parse error 4=write error\n";
}

OLMFile ParseOlm(const std::string& path) {
    OLMFile olm;
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        olm.error = "Failed to open file";
        return olm;
    }

    file.read(reinterpret_cast<char*>(&olm.header), sizeof(OlmMainHeader));
    if (file.gcount() != sizeof(OlmMainHeader)) {
        olm.error = "Failed to read main header";
        return olm;
    }

    if (olm.header.version1 < 0.11f || olm.header.version1 > 0.13f) {
        olm.error = "Invalid version1";
        return olm;
    }

    file.read(reinterpret_cast<char*>(&olm.layer), sizeof(OlmLayerDescriptor));
    if (file.gcount() != sizeof(OlmLayerDescriptor)) {
        olm.error = "Failed to read layer descriptor";
        return olm;
    }

    uint32_t numPixels = olm.layer.pixel_width * olm.layer.pixel_height;
    olm.pixels.resize(numPixels);
    file.read(reinterpret_cast<char*>(olm.pixels.data()), numPixels * sizeof(OlmPixel));
    if (file.gcount() != numPixels * sizeof(OlmPixel)) {
        olm.error = "Failed to read pixel data";
        return olm;
    }

    olm.valid = true;
    return olm;
}

static int do_olm_info(const std::string& input)
{
    if (!fs::exists(input))
    {
        std::cerr << "olm: file not found: " << input << "\n";
        return 2;
    }

    OLMFile olm = ParseOlm(input);
    if (!olm.valid)
    {
        std::cerr << "olm: parse error: " << olm.error << "\n";
        return 3;
    }

    std::cout << "file:       " << input << "\n";
    std::cout << "version1:   " << olm.header.version1 << "\n";
    std::cout << "version2:   " << olm.header.version2 << "\n";
    std::cout << "date:       " << olm.header.year << "-" << olm.header.month << "-" << olm.header.day << " "
              << olm.header.hour << ":" << olm.header.minute << ":" << olm.header.second << "\n";
    std::cout << "grid:       " << olm.header.width << "x" << olm.header.height << "\n";
    std::cout << "uv_scale:   " << olm.header.uv_scale_u << ", " << olm.header.uv_scale_v << "\n";
    std::cout << "resolution: " << olm.layer.pixel_width << "x" << olm.layer.pixel_height << "\n";
    std::cout << "pixels:     " << olm.pixels.size() << "\n";
    return 0;
}

static void SwapChannels(std::vector<OlmPixel>& pixels) {
    // Swap R and B to match BGRA target
    for (auto& p : pixels) {
        std::swap(p.r, p.b);
    }
}

static int do_olm_convert(const std::string& input, std::string outpath, const std::string& format)
{
    if (!fs::exists(input))
    {
        std::cerr << "olm: file not found: " << input << "\n";
        return 2;
    }

    OLMFile olm = ParseOlm(input);
    if (!olm.valid)
    {
        std::cerr << "olm: parse error: " << olm.error << "\n";
        return 3;
    }

    if (outpath.empty()) {
        fs::path p(input);
        p.replace_extension(format);
        outpath = p.string();
    }

    SwapChannels(olm.pixels); // Swap R and B

    int res = 0;
    if (format == "png") {
        res = stbi_write_png(outpath.c_str(), olm.layer.pixel_width, olm.layer.pixel_height, 4, olm.pixels.data(), olm.layer.pixel_width * 4);
    } else if (format == "tga") {
        res = stbi_write_tga(outpath.c_str(), olm.layer.pixel_width, olm.layer.pixel_height, 4, olm.pixels.data());
    }

    if (res == 0) {
        std::cerr << "olm: failed to write " << format << " file: " << outpath << "\n";
        return 4;
    }

    std::cout << "olm: wrote " << outpath << "\n";
    return 0;
}

int cmd_olm(int argc, char** argv)
{
    if (argc < 2)
    {
        print_olm_help();
        return 1;
    }

    std::string subcmd = argv[1];

    if (subcmd == "info")
    {
        if (argc < 3) { print_olm_help(); return 1; }
        return do_olm_info(argv[2]);
    }
    else if (subcmd == "to-png" || subcmd == "to-tga")
    {
        if (argc < 3) { print_olm_help(); return 1; }
        std::string input = argv[2];
        std::string outpath = "";

        for (int i = 3; i < argc; ++i)
        {
            std::string arg = argv[i];
            if (arg == "-o" && i + 1 < argc)
            {
                outpath = argv[++i];
            }
        }
        return do_olm_convert(input, outpath, subcmd == "to-png" ? "png" : "tga");
    }

    std::cerr << "olm: unknown subcommand '" << subcmd << "'\n";
    print_olm_help();
    return 1;
}
