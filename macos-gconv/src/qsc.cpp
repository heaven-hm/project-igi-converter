#include "common.hpp"
#include "qsc/qvm_compiler.h"
namespace gconv {
void compileScript(const fs::path& src, const fs::path& dst) {
  auto b = readFile(src);
  auto lex = qsc::Lex(std::string(b.begin(), b.end()));
  if (!lex.ok)
    fail(lex.error);
  auto ast = qsc::Parse(lex.tokens);
  if (!ast.ok)
    fail(ast.error);
  auto result = qvm::Compile(*ast.program);
  if (!result.ok)
    fail(result.error);
  writeFile(dst, result.binary);
}
}  // namespace gconv
