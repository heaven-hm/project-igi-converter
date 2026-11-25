// game_selector.h - shared CLI game selection for the dual-game converter.
//
// This file is intentionally header-only so both the CLI and the standalone
// test target can use it without extra link dependencies. The default game is
// IGI 1, preserving the behaviour of every existing invocation.
#pragma once

#include <string>

enum class igi_game_t {
    IGI1,
    IGI2,
};

struct game_selection_t {
    igi_game_t game = igi_game_t::IGI1;
    int command_index = 1; // index of the command token inside argv
    bool explicit_flag = false;
    std::string error;
};

inline const char* Game_Name(igi_game_t game)
{
    return game == igi_game_t::IGI2 ? "igi2" : "igi1";
}

// Parse a leading --game=igi1|igi2 (or --game igi1|igi2) flag.
// The returned command_index points at the first argument after the flag, so
// callers can keep using the existing "shift argv by one" dispatch pattern.
inline game_selection_t Game_Select(int argc, char** argv)
{
    game_selection_t sel;
    sel.command_index = argc;

    if (argc <= 1)
        return sel;

    int idx = 1;
    const std::string first = argv[idx];
    std::string value;

    if (first.rfind("--game=", 0) == 0) {
        value = first.substr(7);
        ++idx;
    }
    else if (first == "--game") {
        if (idx + 1 >= argc) {
            sel.error = "--game requires a value (igi1 or igi2)";
            return sel;
        }
        value = argv[idx + 1];
        idx += 2;
    }
    else {
        sel.command_index = 1;
        return sel;
    }

    if (value == "igi1")
        sel.game = igi_game_t::IGI1;
    else if (value == "igi2")
        sel.game = igi_game_t::IGI2;
    else {
        sel.error = "invalid --game value '" + value + "' (expected igi1 or igi2)";
        return sel;
    }

    sel.explicit_flag = true;
    sel.command_index = idx;
    return sel;
}
