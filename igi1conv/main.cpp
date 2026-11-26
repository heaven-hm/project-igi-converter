#include "pch.h"
#include "command_dispatch.h"
#ifdef IGI1CONV_WITH_GUI
#include "mcp_execution.h"
#include "mcp_transport.h"
#endif

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <system_error>
#include "game_selector.h"
#include "cmd_tex.h"
#include "cmd_mef.h"
#include "cmd_qsc.h"
#include "cmd_qvm.h"
#include "cmd_res.h"
#include "cmd_mtp.h"
#include "cmd_terrain.h"
#include "cmd_graph.h"
#include "cmd_dat.h"
#include "cmd_fnt.h"
#include "cmd_test.h"
#include "cmd_iff.h"
#include "cmd_wav.h"
#include "cmd_olm.h"
#include "cmd_lightmap.h"
#include "cmd_igi2_forest.h"
//   0 = success
//   1 = bad args
//   2 = file not found
//   3 = parse error
//   4 = write error

#ifndef IGI1CONV_VERSION
#define IGI1CONV_VERSION "1.11.0"
#endif

static void print_help()
{
    std::cout <<
        "igi1conv v" IGI1CONV_VERSION " \xe2\x80\x94 IGI Game Converter\n"
        "\n"
        "Usage: igi1conv [--game=igi1|igi2] <command> [options]\n"
        "\n"
        "Game selection (default: igi1):\n"
        "  --game=igi1     Route to the IGI 1 backends (current behaviour)\n"
        "  --game=igi2     Route to the IGI 2 backends\n"
        "\n"
        "Commands:\n"
        "  tex      TEX/SPR/PIC texture operations (decode, info, to-png, to-tga)\n"
        "  mef      MEF 3D mesh operations (export to OBJ, bundle, dump, info)\n"
        "  qsc      QSC QScript (compile to QVM, validate)\n"
        "  qvm      QVM bytecode (decompile to QSC, disasm, info)\n"
        "  res      RES archive (list, extract, compile, pack, unpack)\n"
        "  mtp      MTP terrain properties (dump to JSON, info, to-dat)\n"
        "  terrain  Terrain height/cube data (export-lmp, export-ctr, info)\n"
        "  graph    AI navigation graph (export to JSON, info)\n"
        "  dat      DAT model-texture data (info, export, to-mtp)\n"
        "  fnt      FNT font file (info, export PNG)\n"
        "  iff      IFF skeletal animation format (info, test, decompile, convert, create, rebuild, emit-qsc, export-gif)\n"
        "  wav      IGI audio (ILSF container -> .wav, info, convert, convert-dir)\n"
        "  olm      OLM lightmap operations (info, to-png, to-tga)\n"
        "  lightmap Lightmap binding resolution (list, resolve by task-id/position)\n"
        "  test     Run advanced test suite on game directory\n"
        "\n"
        "IGI 2 commands (require --game=igi2):\n"
        "  forest   DAT Forest placement data (info, export, compile)\n"
        "           See docs/igi2/FORMAT_MATRIX.md for the full format status matrix.\n"
        "\n"
        "Run 'igi1conv <command> --help' for command-specific help.\n";
}

#ifdef IGI1CONV_WITH_GUI
#include "gui_main.h"
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

void PrintMcpHelp() {
    std::cout <<
        "Usage: igi1conv mcp [options]\n"
        "\n"
        "Options:\n"
        "  --transport stdio|http  Transport (default: stdio)\n"
        "  --host <ip|localhost>   HTTP bind address (default: 127.0.0.1)\n"
        "  --port <n>              HTTP port (default: 8765)\n"
        "  --endpoint <path>       HTTP endpoint (default: /mcp)\n"
        "  --auth-token <token>    Bearer token (prefer IGI1CONV_MCP_TOKEN)\n"
        "  --origin <origin>       Allow an HTTP Origin (repeatable)\n"
        "  --help                  Show this help\n";
}

#ifdef IGI1CONV_WITH_GUI
bool ParsePort(const std::string& text, std::uint16_t& port) {
    if (text.empty()) return false;
    unsigned int value = 0;
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto parsed = std::from_chars(begin, end, value);
    if (parsed.ec != std::errc() || parsed.ptr != end || value > 65535u)
        return false;
    port = static_cast<std::uint16_t>(value);
    return true;
}

int RunMcpCommand(const std::vector<std::string>& args) {
    igi1conv::McpHttpOptions options;
    std::string transport = "stdio";

    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string& option = args[i];
        if (option == "--help" || option == "-h") {
            PrintMcpHelp();
            return 0;
        }
        if (option == "--transport" || option == "--host" || option == "--port"
            || option == "--endpoint" || option == "--auth-token" || option == "--origin") {
            if (i + 1 >= args.size()) {
                std::cerr << "igi1conv mcp: " << option << " requires a value\n";
                return 1;
            }
            const std::string& value = args[++i];
            if (option == "--transport") {
                transport = value;
            } else if (option == "--host") {
                options.host = value;
            } else if (option == "--port") {
                if (!ParsePort(value, options.port)) {
                    std::cerr << "igi1conv mcp: invalid HTTP port\n";
                    return 1;
                }
            } else if (option == "--endpoint") {
                options.endpoint = value;
            } else if (option == "--auth-token") {
                options.authToken = value;
            } else {
                options.allowedOrigins.push_back(value);
            }
            continue;
        }
        std::cerr << "igi1conv mcp: unknown option '" << option << "'\n";
        return 1;
    }

    if (transport != "stdio" && transport != "http") {
        std::cerr << "igi1conv mcp: --transport must be stdio or http\n";
        return 1;
    }

    if (options.authToken.empty()) {
        if (const char* environmentToken = std::getenv("IGI1CONV_MCP_TOKEN"))
            options.authToken = environmentToken;
    } else {
        std::cerr << "igi1conv mcp: warning: --auth-token is visible in the process command line; "
                     "prefer IGI1CONV_MCP_TOKEN\n";
    }

    igi1conv::McpDispatcher dispatcher(igi1conv::ExecuteMcpGameCommand);
    if (transport == "stdio") return igi1conv::RunMcpStdio(dispatcher);
    return igi1conv::RunMcpHttp(dispatcher, options);
}

#endif
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
#ifdef IGI1CONV_WITH_GUI
#ifdef _WIN32
        HWND consoleWnd = GetConsoleWindow();
        if (consoleWnd) {
            ShowWindow(consoleWnd, SW_HIDE);
        }
#endif
        return run_gui();
#else
        print_help();
        return 1;
#endif
    }

    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(argc - 1));
    for (int i = 1; i < argc; ++i)
        args.emplace_back(argv[i]);
    if (!args.empty() && args[0] == "mcp")
    {
#ifdef IGI1CONV_WITH_GUI
        return RunMcpCommand(args);
#else
        std::cerr << "igi1conv: MCP support requires a Qt-enabled build\n";
        return 1;
#endif
    }
    return igi1conv::RunCommandVector(args);
    if (argc == 2 && std::string(argv[1]) == "--gui")
    {
#ifdef IGI1CONV_WITH_GUI
        return run_gui();
#else
        std::cerr << "igi1conv: this build does not include the GUI\n";
        return 1;
#endif
    }

    const game_selection_t selection = Game_Select(argc, argv);
    if (!selection.error.empty())
    {
        std::cerr << "igi1conv: " << selection.error << "\n";
        return 1;
    }
    const int cmd_idx = selection.command_index;
    (void)selection;

    std::string cmd = argv[cmd_idx];

    if (cmd == "--help" || cmd == "-h")
    {
        print_help();
        return 0;
    }

    if (cmd == "--version" || cmd == "-v")
    {
        std::cout << "igi1conv version " IGI1CONV_VERSION "\n";
        return 0;
    }

    // Each handler sees argv[0] = command name, as before.
    const int sub_argc = argc - cmd_idx;
    char** sub_argv = argv + cmd_idx;

    // ---- IGI 2 backend (issue #10 vertical slice) ----
    if (cmd == "forest")
    {
        if (selection.game != igi_game_t::IGI2)
        {
            std::cerr << "igi1conv: 'forest' is an IGI 2 format; pass --game=igi2 before the command.\n";
            return 1;
        }
        return cmd_igi2_forest(sub_argc, sub_argv);
    }

    // Never silently feed IGI 1 assets to an IGI 2 request (or vice versa).
    if (selection.game == igi_game_t::IGI2)
    {
        std::cerr << "igi1conv: command '" << cmd << "' is not implemented for IGI 2 yet.\n"
                  << "Available IGI 2 commands: forest\n"
                  << "See docs/igi2/FORMAT_MATRIX.md for the format status matrix.\n";
        return 1;
    }

    // ---- IGI 1 backend (unchanged) ----
    if (cmd == "tex")     return cmd_tex(sub_argc, sub_argv);
    if (cmd == "mef")     return cmd_mef(sub_argc, sub_argv);
    if (cmd == "mex")     return cmd_mef(sub_argc, sub_argv);
    if (cmd == "qsc")     return cmd_qsc(sub_argc, sub_argv);
    if (cmd == "qvm")     return cmd_qvm(sub_argc, sub_argv);
    if (cmd == "res")     return cmd_res(sub_argc, sub_argv);
    if (cmd == "mtp")     return cmd_mtp(sub_argc, sub_argv);
    if (cmd == "terrain") return cmd_terrain(sub_argc, sub_argv);
    if (cmd == "graph")   return cmd_graph(sub_argc, sub_argv);
    if (cmd == "dat")     return cmd_dat(sub_argc, sub_argv);
    if (cmd == "fnt")     return cmd_fnt(sub_argc, sub_argv);
    if (cmd == "iff")     return cmd_iff(sub_argc, sub_argv);
    if (cmd == "wav")     return cmd_wav(sub_argc, sub_argv);
    if (cmd == "olm")     return cmd_olm(sub_argc, sub_argv);
    if (cmd == "lightmap") return cmd_lightmap(sub_argc, sub_argv);
    if (cmd == "test")    return cmd_test(sub_argc, sub_argv);

    std::cerr << "igi1conv: unknown command '" << cmd << "'\n";
    std::cerr << "Run 'igi1conv --help' for usage.\n";
    return 1;
}
