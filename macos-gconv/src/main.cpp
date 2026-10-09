#include <iostream>
#include <string>

#include "asset_json.hpp"
#include "common.hpp"
#include "qsc/qvm_compiler.h"
int runExtendedCommand(int argc, char **argv);
namespace gconv
{
void printUsage()
{
    std::cout << "igi2conv 0.2.0 — native C++17 IGI 2 CLI\n"
                 "  igi2conv qsc compile INPUT.qsc OUTPUT.qvm\n"
                 "  igi2conv res list INPUT.res\n"
                 "  igi2conv res extract INPUT.res DIRECTORY\n"
                 "  igi2conv res pack DIRECTORY OUTPUT.res\n"
                 "  igi2conv fnt info INPUT.fnt\n"
                 "  igi2conv fnt extract INPUT.fnt OUTPUT.png|OUTPUT.tga\n"
                 "  igi2conv fnt pack TEMPLATE.fnt IMAGE.png|IMAGE.tga OUTPUT.fnt\n"
                 "  igi2conv tex extract INPUT.tex|INPUT.spr|INPUT.pic DIRECTORY\n"
                 "  igi2conv tex make IMAGE.png|IMAGE.tga OUTPUT.tex\n"
                 "  igi2conv tex pack TEMPLATE.tex|TEMPLATE.spr|TEMPLATE.pic "
                 "BITMAP_DIRECTORY OUTPUT\n"
                 "JSON: asset to-json INPUT OUTPUT; asset from-json INPUT OUTPUT.\n"
                 "Images: image convert INPUT OUTPUT; Audio: wav extract/pack, mp3 "
                 "normalize.\n"
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
    const std::string operation = argv[2];
    const int pathCount = argc - 3;
    if (family == "qsc" && operation == "compile" && pathCount == 2)
    {
        compileScript(argv[3], argv[4]);
        return;
    }
    if (family == "res" && operation == "list" && pathCount == 1)
    {
        listResources(argv[3]);
        return;
    }
    if (family == "res" && operation == "extract" && pathCount == 2)
    {
        extractResources(argv[3], argv[4]);
        return;
    }
    if (family == "res" && operation == "pack" && pathCount == 2)
    {
        packResources(argv[3], argv[4]);
        return;
    }
    if (family == "fnt" && operation == "info" && pathCount == 1)
    {
        showFontInfo(argv[3]);
        return;
    }
    if (family == "fnt" && operation == "extract" && pathCount == 2)
    {
        extractFont(argv[3], argv[4]);
        return;
    }
    if (family == "fnt" && operation == "pack" && pathCount == 3)
    {
        packFont(argv[3], argv[4], argv[5]);
        return;
    }
    if (family == "tex" && operation == "extract" && pathCount == 2)
    {
        extractTexture(argv[3], argv[4]);
        return;
    }
    if (family == "tex" && operation == "make" && pathCount == 2)
    {
        createTexture(argv[3], argv[4]);
        return;
    }
    if (family == "tex" && operation == "pack" && pathCount == 3)
    {
        packTexture(argv[3], argv[4], argv[5]);
        return;
    }
    fail("Unsupported command or argument count; use --help");
}
} // namespace gconv

int main(int argc, char **argv)
{
    try
    {
        if (argc < 2 || std::string(argv[1]) == "--help")
        {
            gconv::printUsage();
            return 0;
        }
        if (std::string(argv[1]) == "--version")
        {
            std::cout << "igi2conv 0.2.0 (C++17 native)\n";
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
