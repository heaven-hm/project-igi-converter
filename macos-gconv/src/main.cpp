#include <iostream>
#include <string>

#include "common.hpp"
#include "qsc/qvm_compiler.h"
namespace gconv {
void printUsage() {
  std::cout << "macos-gconv 0.1.0 — native C++17 IGI 2 CLI\n"
               "  gconv qsc compile INPUT.qsc OUTPUT.qvm\n"
               "  gconv res list INPUT.res\n"
               "  gconv res extract INPUT.res DIRECTORY\n"
               "  gconv res pack DIRECTORY OUTPUT.res\n"
               "  gconv fnt info INPUT.fnt\n"
               "  gconv fnt extract INPUT.fnt OUTPUT.png|OUTPUT.tga\n"
               "  gconv fnt pack TEMPLATE.fnt IMAGE.png|IMAGE.tga OUTPUT.fnt\n"
               "  gconv tex extract INPUT.tex|INPUT.spr|INPUT.pic DIRECTORY\n"
               "  gconv tex make IMAGE.png|IMAGE.tga OUTPUT.tex\n"
               "  gconv tex pack TEMPLATE.tex|TEMPLATE.spr|TEMPLATE.pic "
               "BITMAP_DIRECTORY OUTPUT\n"
               "This native build does not execute QSC recipes or load Windows "
               "DLLs.\n";
}
void runCommand(int argc, char** argv) {
  if (argc < 3)
    fail("Missing arguments; use --help");
  const std::string family = argv[1];
  const std::string operation = argv[2];
  const int pathCount = argc - 3;
  if (family == "qsc" && operation == "compile" && pathCount == 2) {
    compileScript(argv[3], argv[4]);
    return;
  }
  if (family == "res" && operation == "list" && pathCount == 1) {
    listResources(argv[3]);
    return;
  }
  if (family == "res" && operation == "extract" && pathCount == 2) {
    extractResources(argv[3], argv[4]);
    return;
  }
  if (family == "res" && operation == "pack" && pathCount == 2) {
    packResources(argv[3], argv[4]);
    return;
  }
  if (family == "fnt" && operation == "info" && pathCount == 1) {
    showFontInfo(argv[3]);
    return;
  }
  if (family == "fnt" && operation == "extract" && pathCount == 2) {
    extractFont(argv[3], argv[4]);
    return;
  }
  if (family == "fnt" && operation == "pack" && pathCount == 3) {
    packFont(argv[3], argv[4], argv[5]);
    return;
  }
  if (family == "tex" && operation == "extract" && pathCount == 2) {
    extractTexture(argv[3], argv[4]);
    return;
  }
  if (family == "tex" && operation == "make" && pathCount == 2) {
    createTexture(argv[3], argv[4]);
    return;
  }
  if (family == "tex" && operation == "pack" && pathCount == 3) {
    packTexture(argv[3], argv[4], argv[5]);
    return;
  }
  fail("Unsupported command or argument count; use --help");
}
}  // namespace gconv

int main(int argc, char** argv) {
  try {
    if (argc < 2 || std::string(argv[1]) == "--help") {
      gconv::printUsage();
      return 0;
    }
    if (std::string(argv[1]) == "--version") {
      std::cout << "macos-gconv 0.1.0 (C++17 native)\n";
      return 0;
    }
    gconv::runCommand(argc, argv);
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "gconv: " << error.what() << "\n";
    return 1;
  }
}
