#include "common.hpp"
#include <iostream>
#include <string>
int runNativeCommand(int argc, char **argv)
{
    if (argc < 3)
        return -1;
    const std::string family = argv[1], op = argv[2];
    try
    {
        if (family == "fnt" && op == "pack" && argc == 6)
        {
            gconv::packFont(argv[3], argv[4], argv[5]);
            return 0;
        }
        if (family == "fnt" && op == "extract" && argc == 5)
        {
            gconv::extractFont(argv[3], argv[4]);
            return 0;
        }
        if (family == "tex" && op == "pack" && argc == 6)
        {
            gconv::packTexture(argv[3], argv[4], argv[5]);
            return 0;
        }
        if (family == "tex" && op == "extract" && argc == 5)
        {
            gconv::extractTexture(argv[3], argv[4]);
            return 0;
        }
        if (family == "tex" && op == "make" && argc == 5)
        {
            gconv::createTexture(argv[3], argv[4]);
            return 0;
        }
        if (family == "res" && op == "pack" && argc == 5)
        {
            gconv::packResources(argv[3], argv[4]);
            return 0;
        }
        if (family == "res" && (op == "extract" || op == "unpack"))
        {
            if (argc == 5)
            {
                gconv::extractResources(argv[3], argv[4]);
                return 0;
            }
            if (argc == 6 && std::string(argv[4]) == "-o")
            {
                gconv::extractResources(argv[3], argv[5]);
                return 0;
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "igi1conv: " << e.what() << '\n';
        return 3;
    }
    return -1;
}
