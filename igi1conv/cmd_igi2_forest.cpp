// cmd_igi2_forest.cpp - IGI 2 forest_<task_id>.dat CLI (issue #10 vertical slice).
//
// Reached as:  igi1conv --game=igi2 forest <info|export|compile> ...
// The parser/writer lives in source/parsers/igi2_forest_parser.* and mirrors
// the format documented by igipy (docs/igi2/formats/dat_forest.md).
#include "pch.h"
#include "cmd_igi2_forest.h"
#include "igi2_forest_parser.h"

#include <fstream>
#include <iterator>
#include <sstream>

namespace {

void PrintUsage()
{
    std::cerr <<
        "Usage (IGI 2 only):\n"
        "  igi1conv --game=igi2 forest info   <forest_*.dat>\n"
        "  igi1conv --game=igi2 forest export <forest_*.dat> [-o <out.json>]\n"
        "  igi1conv --game=igi2 forest compile <in.json> -o <out.dat>\n";
}

const char* OptVal(int argc, char** argv, const char* name)
{
    for (int i = 1; i < argc - 1; ++i)
        if (std::strcmp(argv[i], name) == 0)
            return argv[i + 1];
    return nullptr;
}

const char* OutOpt(int argc, char** argv)
{
    const char* p = OptVal(argc, argv, "-o");
    if (!p) p = OptVal(argc, argv, "--out");
    if (!p) p = OptVal(argc, argv, "--output");
    return p;
}

bool ReadTextFile(const std::string& path, std::string& text, std::string& err)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        err = "cannot open input file: " + path;
        return false;
    }
    text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

bool WriteTextFile(const std::string& path, const std::string& text, std::string& err)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        err = "cannot open output file: " + path;
        return false;
    }
    out << text;
    if (!out) {
        err = "failed writing output file: " + path;
        return false;
    }
    return true;
}

std::string DefaultOutput(const std::string& input, const std::string& ext)
{
    const size_t dot = input.find_last_of('.');
    return (dot == std::string::npos ? input : input.substr(0, dot)) + ext;
}

} // namespace

int cmd_igi2_forest(int argc, char** argv)
{
    if (argc < 3 || std::strcmp(argv[1], "--help") == 0 || std::strcmp(argv[1], "-h") == 0) {
        PrintUsage();
        return argc < 3 ? 1 : 0;
    }

    const std::string action = argv[1];
    const std::string input = argv[2];
    std::string err;

    if (action == "info") {
        igi2::ForestFile forest;
        if (!igi2::Forest_ParseFile(input, forest, err)) {
            std::cerr << "forest: parse error: " << err << "\n";
            return 3;
        }
        std::cout << igi2::Forest_FormatReport(forest);
        return 0;
    }

    if (action == "export") {
        igi2::ForestFile forest;
        if (!igi2::Forest_ParseFile(input, forest, err)) {
            std::cerr << "forest: parse error: " << err << "\n";
            return 3;
        }
        const char* outArg = OutOpt(argc, argv);
        const std::string out = outArg ? outArg : DefaultOutput(input, ".json");
        if (!WriteTextFile(out, igi2::Forest_FormatJSON(forest), err)) {
            std::cerr << "forest: write error: " << err << "\n";
            return 4;
        }
        std::cout << "forest: exported " << forest.records.size()
                  << " record(s) to " << out << "\n";
        return 0;
    }

    if (action == "compile") {
        std::string json;
        if (!ReadTextFile(input, json, err)) {
            std::cerr << "forest: " << err << "\n";
            return 2;
        }
        igi2::ForestFile forest;
        if (!igi2::Forest_ParseJSON(json, forest, err)) {
            std::cerr << "forest: JSON error: " << err << "\n";
            return 3;
        }
        const char* outArg = OutOpt(argc, argv);
        const std::string out = outArg ? outArg : DefaultOutput(input, ".dat");
        if (!igi2::Forest_WriteFile(out, forest, err)) {
            std::cerr << "forest: write error: " << err << "\n";
            return 4;
        }
        std::cout << "forest: compiled " << forest.records.size()
                  << " record(s) to " << out << "\n";
        return 0;
    }

    std::cerr << "forest: unknown action '" << action << "'\n";
    PrintUsage();
    return 1;
}
