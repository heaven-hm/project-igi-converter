#include <string>

#include "cmd_dat.h"
#include "cmd_graph.h"
#include "cmd_iff.h"
#include "cmd_lightmap.h"
#include "cmd_mef.h"
#include "cmd_mtp.h"
#include "cmd_olm.h"
#include "cmd_qvm.h"
#include "cmd_terrain.h"
#include "cmd_tex.h"
#include "cmd_wav.h"
int runExtendedCommand(int argc, char **argv)
{
    if (argc < 3)
        return -1;
    const std::string command = argv[1];
    if (command == "mef")
        return cmd_mef(argc - 1, argv + 1);
    if (command == "qvm")
        return cmd_qvm(argc - 1, argv + 1);
    if (command == "mtp")
        return cmd_mtp(argc - 1, argv + 1);
    if (command == "terrain")
        return cmd_terrain(argc - 1, argv + 1);
    if (command == "graph")
        return cmd_graph(argc - 1, argv + 1);
    if (command == "dat")
        return cmd_dat(argc - 1, argv + 1);
    if (command == "iff")
        return cmd_iff(argc - 1, argv + 1);
    if (command == "wav")
        return cmd_wav(argc - 1, argv + 1);
    if (command == "olm")
        return cmd_olm(argc - 1, argv + 1);
    if (command == "lightmap")
        return cmd_lightmap(argc - 1, argv + 1);
    if (command == "tex" && std::string(argv[2]) != "extract" && std::string(argv[2]) != "pack" &&
        std::string(argv[2]) != "make")
        return cmd_tex(argc - 1, argv + 1);
    return -1;
}
