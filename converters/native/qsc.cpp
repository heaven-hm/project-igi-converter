#include "asset_json.hpp"
#include "qsc/qvm_compiler.h"
namespace gconv
{
void compileScript(const fs::path &src, const fs::path &dst, int targetMinor)
{
    requireDifferentFiles(src, dst, "QVM output must differ from source script");
    auto b = readFile(src);
    const std::string source(b.begin(), b.end());
    const std::string marker = "// igi2conv QVM version: ";
    uint32_t minor = 7;
    const fs::path metadata = src.string() + ".qvm-version";
    if (targetMinor < 0 && fs::exists(metadata)) {
        auto bytes = readFile(metadata);
        std::string version(bytes.begin(), bytes.end());
        while (!version.empty() && (version.back() == '\n' || version.back() == '\r')) version.pop_back();
        if (version == "8.6") minor = 6;
        else if (version != "8.7") fail("Unsupported IGI 2 QVM sidecar version");
    }
    if (source.compare(0, marker.size(), marker) == 0)
    {
        const auto end = source.find_first_of("\r\n", marker.size());
        const auto version = source.substr(marker.size(), end - marker.size());
        if (version == "8.6")
            minor = 6;
        else if (version != "8.7")
            fail("Unsupported IGI 2 QVM source version");
    }
    if (targetMinor >= 0) {
        if (targetMinor != 6 && targetMinor != 7) fail("Unsupported IGI 2 QVM target version");
        minor = static_cast<uint32_t>(targetMinor);
    }
    auto lex = qsc::Lex(source);
    if (!lex.ok)
        fail(lex.error);
    auto ast = qsc::Parse(lex.tokens);
    if (!ast.ok)
        fail(ast.error);
    auto result = qvm::Compile(*ast.program, minor);
    if (!result.ok)
        fail(result.error);
    writeUnsigned(result.binary, 8, minor);
    const fs::path layoutPath = src.string() + ".qvm-layout";
    if (fs::exists(layoutPath)) {
        requireDifferentFiles(layoutPath, dst, "QVM output must differ from layout metadata");
        const auto layout = readJson(layoutPath);
        if (layout.value("kind", "") != "igi2-qvm-layout" || layout.value("version", 0) != 1 ||
            jsonInteger<uint32_t>(layout.at("minor")) != minor || layout.at("reserved").size() != 2 ||
            result.binary.size() < 64 || readU32(result.binary, 12) != 64)
            fail("Invalid QVM layout metadata");
        const auto offset = jsonInteger<uint32_t>(layout.at("trailer_offset"));
        if (offset && offset != jsonInteger<uint32_t>(layout.at("section_end")))
            fail("Unsupported QVM trailer offset");
        const auto trailer = decodeHex(layout.at("trailer"));
        if (result.binary.size() > UINT32_MAX || trailer.size() > UINT32_MAX - result.binary.size())
            fail("QVM extended payload exceeds the format range");
        writeUnsigned(result.binary, 52, jsonInteger<uint32_t>(layout.at("reserved")[0]));
        writeUnsigned(result.binary, 56, jsonInteger<uint32_t>(layout.at("reserved")[1]));
        writeUnsigned(result.binary, 60, offset ? static_cast<uint32_t>(result.binary.size()) : 0);
        result.binary.insert(result.binary.end(), trailer.begin(), trailer.end());
    }
    writeFile(dst, result.binary);
}
} // namespace gconv
