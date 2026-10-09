#include <iostream>
#include <set>

#include "asset_json.hpp"

namespace gconv
{
namespace
{
std::string lowerExtension(const fs::path &path)
{
    auto value = path.extension().string();
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return value;
}

bool terrainFormat(const std::string &extension)
{
    static const std::set<std::string> formats = {".thm", ".tlm", ".tmm", ".hmp",
                                                  ".bit", ".cmd", ".ctr", ".lmp"};
    return formats.count(extension) != 0;
}

void checkGameFormat(const std::string &extension, bool igi2)
{
    if (igi2 && (extension == ".hmp" || extension == ".bit" || extension == ".cmd" ||
                 extension == ".ctr" || extension == ".lmp"))
        fail("Use igi1conv for this IGI 1 terrain format");
    if (!igi2 &&
        (extension == ".thm" || extension == ".tlm" || extension == ".tmm" || extension == ".syn"))
        fail("Use igi2conv for this IGI 2 format");
}

void extractOlmImages(const fs::path &input, const fs::path &directory, bool igi2)
{
    const auto document = exportMetadata(input, igi2);
    size_t index = 0;
    for (const auto &layer : document.at("layers"))
    {
        Bytes rgba;
        for (const auto &value : layer.at("rgba"))
            rgba.push_back(jsonInteger<uint8_t>(value));
        const auto output = directory / (std::to_string(index++) + ".tga");
        requireDifferentFiles(input, output, "Lightmap input and image output must differ");
        saveImage(output, fromRgba(layer.at("width"), layer.at("height"), rgba, 32));
    }
}

void packOlmImages(const fs::path &input, const fs::path &directory, const fs::path &output,
                   bool igi2)
{
    auto document = exportMetadata(input, igi2);
    size_t index = 0;
    for (auto &layer : document.at("layers"))
    {
        const auto imagePath = directory / (std::to_string(index++) + ".tga");
        requireDifferentFiles(imagePath, output, "Bitmap input and output must differ");
        const auto image = loadImage(imagePath);
        if (image.width != layer.at("width") || image.height != layer.at("height"))
            fail("OLM image dimensions must match each original layer");
        layer["rgba"] = toRgba(image);
    }
    requireDifferentFiles(input, output, "Template and output must differ");
    writeFile(output, importMetadata(document, igi2));
}
} // namespace

int runAssetCommand(int argc, char **argv, bool igi2)
{
    if (argc < 3)
        return -1;
    const std::string family = argv[1], operation = argv[2];
    // Legacy commands use -o; apply the same source preservation rule.
    if (argc > 3 && fs::is_regular_file(argv[3]))
    {
        checkGameFormat(lowerExtension(argv[3]), igi2);
        for (int index = 4; index + 1 < argc; ++index)
            if (std::string(argv[index]) == "-o")
                requireDifferentFiles(argv[3], argv[index + 1], "Input and output must differ");
    }

    if (family == "image" && operation == "convert" && argc == 5)
    {
        requireDifferentFiles(argv[3], argv[4], "Input and output must differ");
        saveImage(argv[4], loadImage(argv[3]));
        return 0;
    }
    if (igi2 && family == "mef" && operation == "build-rigid")
        fail("IGI 2 rigid model authoring is unavailable; use native JSON template edits");
    if (igi2 && family == "mef" && (operation == "to-text" || operation == "compile"))
    {
        if (argc != 6 || std::string(argv[4]) != "-o")
            fail("Usage: igi2conv mef to-text INPUT.mef -o OUTPUT.json; "
                 "igi2conv mef compile INPUT.json -o OUTPUT.mef");
        requireDifferentFiles(argv[3], argv[5], "Input and output must differ");
        if (operation == "to-text")
        {
            auto document = exportMetadata(argv[3], true);
            if (document.at("format") != ".mef")
                fail("Expected an IGI 2 MEF model");
            saveJson(argv[5], document);
        }
        else
        {
            const auto document = readJson(argv[3]);
            if (document.at("schema") != "igi-asset/1" || document.at("game") != 2 ||
                document.at("format") != ".mef")
                fail("Expected native IGI 2 MEF JSON from mef to-text or asset to-json");
            if (lowerExtension(argv[5]) != ".mef")
                fail("Model output must end in .mef");
            writeFile(argv[5], importMetadata(document, true));
        }
        return 0;
    }
    const bool familyMatches = family == "asset" || family == "terrain" || family == "syn" ||
                               family == "forest" || family == "graphcover" || family == "iff" ||
                               family == "olm" || family == "mef";
    if (!familyMatches)
        return -1;
    if (igi2 && family == "iff" && argc >= 4 &&
        (operation == "info" || operation == "test" || operation == "rebuild"))
    {
        const auto document = exportMetadata(argv[3], true);
        if (document.value("container", "") != "MINA")
            fail("Expected IGI 2 ILFF/MINA animation");
        if (operation == "rebuild")
        {
            if (argc != 5)
                fail("Usage: igi2conv iff rebuild INPUT OUTPUT");
            requireDifferentFiles(argv[3], argv[4], "Input and output must differ");
            writeFile(argv[4], importMetadata(document, true));
        }
        else
        {
            std::cout << document.dump(2) << '\n';
        }
        return 0;
    }
    if (family == "olm" && argc >= 4 &&
        (operation == "info" || operation == "to-png" || operation == "to-tga"))
    {
        const auto document = exportMetadata(argv[3], igi2);
        if (operation == "info")
        {
            std::cout << "layers: " << document.at("layers").size() << '\n';
            const auto &first = document.at("layers")[0];
            std::cout << "resolution: " << first.at("width") << " x " << first.at("height") << '\n';
        }
        else
        {
            if (argc != 6 || std::string(argv[4]) != "-o")
                fail("Expected -o IMAGE output path");
            requireDifferentFiles(argv[3], argv[5], "Input and image output must differ");
            const auto &layer = document.at("layers")[0];
            Bytes rgba;
            for (const auto &value : layer.at("rgba"))
                rgba.push_back(jsonInteger<uint8_t>(value));
            saveImage(argv[5], fromRgba(layer.at("width"), layer.at("height"), rgba, 32));
        }
        return 0;
    }
    if (operation == "to-json" && argc == 5)
    {
        const auto extension = lowerExtension(argv[3]);
        checkGameFormat(extension, igi2);
        requireDifferentFiles(argv[3], argv[4], "Source and JSON output must differ");
        Json document =
            terrainFormat(extension) ? exportTerrainData(argv[3]) : exportMetadata(argv[3], igi2);
        document["game"] = igi2 ? 2 : 1;
        saveJson(argv[4], document);
        return 0;
    }
    if (operation == "from-json" && argc == 5)
    {
        const auto document = readJson(argv[3]);
        if (document.at("schema") != "igi-asset/1" || document.at("game") != (igi2 ? 2 : 1))
            fail("JSON schema or game does not match this converter");
        const auto extension = document.at("format").get<std::string>();
        checkGameFormat(extension, igi2);
        if (lowerExtension(argv[4]) != extension)
            fail("Output extension must match JSON format");
        requireDifferentFiles(argv[3], argv[4], "JSON input and output must differ");
        writeFile(argv[4], terrainFormat(extension) ? importTerrainData(document)
                                                    : importMetadata(document, igi2));
        return 0;
    }
    if (family == "terrain" && operation == "extract" && argc == 5)
    {
        checkGameFormat(lowerExtension(argv[3]), igi2);
        exportMapImage(argv[3], argv[4]);
        return 0;
    }
    if (family == "terrain" && operation == "pack" && argc == 6)
    {
        checkGameFormat(lowerExtension(argv[3]), igi2);
        packMapImage(argv[3], argv[4], argv[5]);
        return 0;
    }
    if (family == "olm" && operation == "extract" && argc == 5)
    {
        extractOlmImages(argv[3], argv[4], igi2);
        return 0;
    }
    if (family == "olm" && operation == "pack" && argc == 6)
    {
        packOlmImages(argv[3], argv[4], argv[5], igi2);
        return 0;
    }
    if (family == "asset" && operation == "copy" && argc == 5)
    {
        requireDifferentFiles(argv[3], argv[4], "Input and output must differ");
        writeFile(argv[4], readFile(argv[3]));
        return 0;
    }
    return -1;
}
} // namespace gconv
