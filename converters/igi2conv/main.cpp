#include "../native/command_log.hpp"
#ifdef IGICONV_WITH_GUI
#include "gui_main.h"
#endif
#include <iostream>
#include <string>
#include <vector>

#include "asset_json.hpp"
#include "common.hpp"
#include "qsc/qvm_compiler.h"
int runExtendedCommand(int argc, char **argv);
namespace gconv
{
void printUsage()
{
    std::cout << "igi2conv " IGI1CONV_VERSION " — native C++17 IGI 2 converter\n"
                 "  igi2conv qsc compile INPUT.qsc OUTPUT.qvm [--qvm-version 8.6|8.7]\n"
                 "  igi2conv res list INPUT.res\n"
                 "  igi2conv res extract INPUT.res DIRECTORY\n"
                 "  igi2conv res pack DIRECTORY OUTPUT.res\n"
                 "  igi2conv fnt info INPUT.fnt\n"
                 "  igi2conv fnt extract INPUT.fnt OUTPUT.png|OUTPUT.tga\n"
                 "  igi2conv fnt pack TEMPLATE.fnt IMAGE.png|IMAGE.tga OUTPUT.fnt\n"
                 "  igi2conv tex extract INPUT.tex|INPUT.spr|INPUT.pic DIRECTORY\n"
                 "  igi2conv tex make IMAGE.png|IMAGE.tga OUTPUT.tex [--mipmaps]\n"
                 "  igi2conv tex pack TEMPLATE.tex|TEMPLATE.spr|TEMPLATE.pic "
                 "BITMAP_DIRECTORY OUTPUT\n"
                 "JSON: asset to-json INPUT OUTPUT; asset from-json INPUT OUTPUT.\n"
                 "Images: image convert INPUT OUTPUT; Audio: wav extract/pack, mp3 "
                 "normalize/to-wav.\n"
                 "Additional families: qvm, mef, iff, wav, mtp, dat, graph, terrain, "
                 "olm, lightmap.\n"
                 "This native build does not execute QSC recipes or load Windows "
                 "DLLs.\n";
}
void runCommand(int argc, char **argv)
{
    if (argc < 3)
        fail("Missing arguments; use --help");
    const std::string family = argv[1];
    std::string operation = argv[2];
    if (family == "res" && operation == "unpack") operation = "extract";
    if (family == "fnt" && operation == "export") operation = "extract";
    std::vector<const char*> paths;
    bool mipmaps = false;
    int targetMinor = -1;
    for (int i = 3; i < argc; ++i)
    {
        const std::string argument = argv[i];
        if (argument == "--qvm-version") {
            if (family != "qsc" || operation != "compile" || ++i >= argc) fail("--qvm-version requires qsc compile and 8.6 or 8.7");
            const std::string version = argv[i];
            if (version != "8.6" && version != "8.7") fail("Unsupported IGI 2 QVM target version");
            targetMinor = version == "8.6" ? 6 : 7;
        }
        else if (argument == "--mipmaps")
        {
            if (family != "tex" || operation != "make") fail("--mipmaps requires tex make");
            mipmaps = true;
        }
        else if (argument == "-o" || argument == "--out" || argument == "--output")
        {
            if (++i >= argc) fail("Missing output path");
            paths.push_back(argv[i]);
        }
        else paths.push_back(argv[i]);
    }
    const int pathCount = static_cast<int>(paths.size());
    if (family == "qsc" && operation == "compile" && pathCount == 2)
    {
        compileScript(paths[0], paths[1], targetMinor);
        return;
    }
    if (family == "res" && operation == "list" && pathCount == 1)
    {
        listResources(paths[0]);
        return;
    }
    if (family == "res" && operation == "extract" && pathCount == 2)
    {
        extractResources(paths[0], paths[1]);
        return;
    }
    if (family == "res" && operation == "pack" && pathCount == 2)
    {
        packResources(paths[0], paths[1]);
        return;
    }
    if (family == "fnt" && operation == "info" && pathCount == 1)
    {
        showFontInfo(paths[0]);
        return;
    }
    if (family == "fnt" && operation == "extract" && pathCount == 2)
    {
        extractFont(paths[0], paths[1]);
        return;
    }
    if (family == "fnt" && operation == "pack" && pathCount == 3)
    {
        packFont(paths[0], paths[1], paths[2]);
        return;
    }
    if (family == "tex" && operation == "extract" && pathCount == 2)
    {
        extractTexture(paths[0], paths[1]);
        return;
    }
    if (family == "tex" && operation == "make" && pathCount == 2)
    {
        createTexture(paths[0], paths[1], mipmaps);
        return;
    }
    if (family == "tex" && operation == "pack" && pathCount == 3)
    {
        packTexture(paths[0], paths[1], paths[2]);
        return;
    }
    fail("Unsupported command or argument count; use --help");
}
} // namespace gconv

int main(int argc, char **argv)
{
    gconv::CommandLog commandLog("igi2conv", argc, argv);
    try
    {
#ifdef IGICONV_WITH_GUI
        if (argc < 2 || (argc == 2 && std::string(argv[1]) == "--gui"))
            return run_gui();
        if (argc == 3 && std::string(argv[1]) == "--open") {
            igi2conv_set_initial_open_path(QString::fromLocal8Bit(argv[2]));
            return run_gui();
        }
#endif
        if (argc < 2 || std::string(argv[1]) == "--help")
        {
            gconv::printUsage();
            return 0;
        }
        if (std::string(argv[1]) == "--version")
        {
            std::cout << "igi2conv " IGI1CONV_VERSION " (C++17 native)\n";
            return 0;
        }
        const int audioResult = gconv::runAudioCommand(argc, argv);
        if (audioResult >= 0)
            return audioResult;
        const int assetResult = gconv::runAssetCommand(argc, argv, true);
        if (assetResult >= 0)
            return assetResult;
        const int extendedResult = runExtendedCommand(argc, argv);
        if (extendedResult >= 0)
            return extendedResult;
        gconv::runCommand(argc, argv);
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "igi2conv: " << error.what() << "\n";
        return 1;
    }
}
