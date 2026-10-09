#include "pch.h"
#include "cmd_lightmap.h"
#include "qsc_object_parser.h"
#include "lightmap_resolver.h"

namespace fs = std::filesystem;
using igi1conv::LightmapBinding;
using igi1conv::LightmapBindingSet;

static void print_lightmap_help()
{
    std::cout <<
        "Usage: igi2conv lightmap <subcommand> [options]\n"
        "\n"
        "Subcommands:\n"
        "  list    --model <id> --qsc <objects.qsc>\n"
        "          List every placement of <id> bound to a lightmap (task id,\n"
        "          name, position, logical lightmap id).\n"
        "\n"
        "  resolve --model <id> --qsc <objects.qsc> [--task-id <id> | --pos X,Y,Z]\n"
        "          Resolve which placed instance's lightmap applies to <id> and\n"
        "          print its logical id plus the matching .olm file paths.\n"
        "          The same .mef model can be placed at multiple locations, each\n"
        "          with its own baked lightmap, so a bare --model is only enough\n"
        "          when exactly one placement exists; otherwise pass --task-id\n"
        "          (exact match) or --pos (nearest match by Euclidean distance)\n"
        "          to pick one.\n"
        "\n"
        "Options:\n"
        "  --model <id>     Model id / .mef filename stem (e.g. 435_01_1)\n"
        "  --qsc <path>     Path to the level's decompiled objects.qsc\n"
        "  --task-id <id>   Disambiguate by the placed instance's Task_New id\n"
        "  --pos X,Y,Z      Disambiguate by nearest placed position (raw IGI units)\n"
        "  --help           Show this help\n"
        "\n"
        "Exit codes: 0=success 1=bad args 2=file not found 3=no binding 4=ambiguous\n";
}

namespace {

struct LightmapArgs {
    std::string model;
    std::string qscPath;
    bool hasTaskId = false;
    int32_t taskId = -1;
    bool hasPos = false;
    double x = 0, y = 0, z = 0;
};

// Parses "--model <id> --qsc <path> [--task-id <id> | --pos X,Y,Z]" style
// args shared by both subcommands. Returns false (with a message on
// stderr) on a malformed --pos or missing required flags.
bool ParseLightmapArgs(int argc, char** argv, int startIdx, LightmapArgs& out) {
    for (int i = startIdx; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--model" && i + 1 < argc) {
            out.model = argv[++i];
        } else if (arg == "--qsc" && i + 1 < argc) {
            out.qscPath = argv[++i];
        } else if (arg == "--task-id" && i + 1 < argc) {
            out.hasTaskId = true;
            out.taskId = std::atoi(argv[++i]);
        } else if (arg == "--pos" && i + 1 < argc) {
            std::string posArg = argv[++i];
            std::istringstream iss(posArg);
            std::string xs, ys, zs;
            if (!std::getline(iss, xs, ',') || !std::getline(iss, ys, ',') || !std::getline(iss, zs, ',')) {
                std::cerr << "lightmap: --pos expects \"X,Y,Z\", got \"" << posArg << "\"\n";
                return false;
            }
            try {
                out.x = std::stod(xs);
                out.y = std::stod(ys);
                out.z = std::stod(zs);
            } catch (...) {
                std::cerr << "lightmap: --pos expects \"X,Y,Z\" (numbers), got \"" << posArg << "\"\n";
                return false;
            }
            out.hasPos = true;
        }
    }
    if (out.model.empty()) {
        std::cerr << "lightmap: --model is required\n";
        return false;
    }
    if (out.qscPath.empty()) {
        std::cerr << "lightmap: --qsc is required\n";
        return false;
    }
    if (out.hasTaskId && out.hasPos) {
        std::cerr << "lightmap: pass only one of --task-id or --pos, not both\n";
        return false;
    }
    return true;
}

void PrintBinding(const LightmapBinding& b) {
    std::cout << "  task " << b.taskId << " \"" << b.taskName << "\""
               << " -> " << b.logicalId;
    if (b.hasPos) {
        std::cout << " @ (" << b.posX << ", " << b.posY << ", " << b.posZ << ")";
    }
    std::cout << "\n";
}

int do_lightmap_list(const LightmapArgs& args) {
    if (!fs::exists(args.qscPath)) {
        std::cerr << "lightmap: file not found: " << args.qscPath << "\n";
        return 2;
    }
    std::ifstream f(args.qscPath);
    std::string qscText((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    LightmapBindingSet set = LightmapBindingSet::parse(qscText);

    auto matches = set.allBindingsForModel(args.model);
    if (matches.empty()) {
        std::cout << "lightmap: no bindings found for model \"" << args.model << "\"\n";
        return 3;
    }

    std::cout << "lightmap: " << matches.size() << " placement(s) of \"" << args.model << "\":\n";
    for (auto* b : matches) PrintBinding(*b);
    return 0;
}

int do_lightmap_resolve(const LightmapArgs& args) {
    if (!fs::exists(args.qscPath)) {
        std::cerr << "lightmap: file not found: " << args.qscPath << "\n";
        return 2;
    }
    std::ifstream f(args.qscPath);
    std::string qscText((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    LightmapBindingSet set = LightmapBindingSet::parse(qscText);

    auto matches = set.allBindingsForModel(args.model);
    if (matches.empty()) {
        std::cout << "lightmap: no bindings found for model \"" << args.model << "\"\n";
        return 3;
    }

    const LightmapBinding* chosen = nullptr;
    if (args.hasTaskId) {
        chosen = set.bindingForModelAndTaskId(args.model, args.taskId);
        if (!chosen) {
            std::cerr << "lightmap: no placement of \"" << args.model << "\" has task id "
                       << args.taskId << "\n";
            std::cerr << "lightmap: available placements:\n";
            for (auto* b : matches) PrintBinding(*b);
            return 3;
        }
    } else if (args.hasPos) {
        chosen = set.nearestBindingForModelAndPosition(args.model, args.x, args.y, args.z);
        if (!chosen) {
            std::cerr << "lightmap: no placement of \"" << args.model << "\" has a known position\n";
            return 3;
        }
    } else if (matches.size() == 1) {
        chosen = matches.front();
    } else {
        std::cerr << "lightmap: \"" << args.model << "\" is placed at " << matches.size()
                   << " locations - pass --task-id or --pos to disambiguate:\n";
        for (auto* b : matches) PrintBinding(*b);
        return 4;
    }

    std::cout << "lightmap: resolved ";
    PrintBinding(*chosen);

    auto files = igi1conv::ResolveLightmapFilesForLogicalId(args.qscPath, chosen->logicalId);
    if (files.empty()) {
        std::cerr << "lightmap: binding " << chosen->logicalId
                   << " found but no .olm files on disk\n";
        return 3;
    }
    std::cout << "lightmap: " << files.size() << " .olm file(s):\n";
    for (auto& p : files) std::cout << "  " << p << "\n";
    return 0;
}

} // namespace

int cmd_lightmap(int argc, char** argv)
{
    if (argc < 2)
    {
        print_lightmap_help();
        return 1;
    }

    std::string subcmd = argv[1];
    if (subcmd == "--help" || subcmd == "-h")
    {
        print_lightmap_help();
        return 0;
    }

    LightmapArgs args;
    if (subcmd == "list")
    {
        if (!ParseLightmapArgs(argc, argv, 2, args)) { print_lightmap_help(); return 1; }
        return do_lightmap_list(args);
    }
    else if (subcmd == "resolve")
    {
        if (!ParseLightmapArgs(argc, argv, 2, args)) { print_lightmap_help(); return 1; }
        return do_lightmap_resolve(args);
    }

    std::cerr << "lightmap: unknown subcommand '" << subcmd << "'\n";
    print_lightmap_help();
    return 1;
}
