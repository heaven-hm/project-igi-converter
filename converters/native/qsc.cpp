#include "common.hpp"
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
    writeFile(dst, result.binary);
}
} // namespace gconv
