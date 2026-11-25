#pragma once
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <streambuf>
#include <string>

namespace gconv {
// Preserve CLI output while recording diagnostics in a writable per-user folder.
class CommandLog {
    class Tee : public std::streambuf {
        std::streambuf* original;
        std::ofstream& file;
        std::mutex& mutex;
    public:
        Tee(std::streambuf* output, std::ofstream& log, std::mutex& lock)
            : original(output), file(log), mutex(lock) {}
        std::streamsize xsputn(const char* bytes, std::streamsize size) override {
            std::lock_guard<std::mutex> lock(mutex);
            if (file) { file.write(bytes, size); file.flush(); }
            return original->sputn(bytes, size);
        }
        int_type overflow(int_type c) override {
            if (traits_type::eq_int_type(c, traits_type::eof())) return traits_type::not_eof(c);
            const char byte = traits_type::to_char_type(c);
            return xsputn(&byte, 1) == 1 ? c : traits_type::eof();
        }
        int sync() override {
            std::lock_guard<std::mutex> lock(mutex);
            if (file) file.flush();
            return original->pubsync();
        }
    };
    std::ofstream file;
    std::mutex mutex;
    std::streambuf *stdoutBuffer, *stderrBuffer;
    Tee output, error;
    bool active = false;
public:
    CommandLog(const char* application, int argc, char** argv)
        : stdoutBuffer(std::cout.rdbuf()), stderrBuffer(std::cerr.rdbuf()),
          output(stdoutBuffer, file, mutex), error(stderrBuffer, file, mutex) {
        if (argc < 2 || std::string(argv[1]) == "--gui" || std::string(argv[1]) == "--open") return;
        const char* enabled = std::getenv("IGICONV_LOG_ENABLED");
        if (enabled && std::string(enabled) == "0") return;
        try {
            std::filesystem::path folder;
            const char* overridePath = std::getenv("IGICONV_LOG_DIR");
            if (overridePath && *overridePath) folder = overridePath;
            else {
#ifdef _WIN32
                const char* root = std::getenv("LOCALAPPDATA");
                if (!root) root = std::getenv("APPDATA");
                if (!root) return;
                folder = std::filesystem::path(root) / application / "logs";
#else
                const char* root = std::getenv("HOME");
                if (!root) return;
#ifdef __APPLE__
                folder = std::filesystem::path(root) / "Library" / "Logs" / application;
#else
                const char* state = std::getenv("XDG_STATE_HOME");
                folder = state && *state ? std::filesystem::path(state) : std::filesystem::path(root) / ".local/state";
                folder /= application;
#endif
#endif
            }
            std::filesystem::create_directories(folder);
            file.open(folder / (std::string(application) + "-cli.log"), std::ios::app | std::ios::binary);
            if (!file) return;
            file << "\n[VERBOSE] " << application << " command:";
            for (int i = 1; i < argc; ++i) file << ' ' << argv[i];
            file << '\n'; file.flush();
            std::cout.rdbuf(&output); std::cerr.rdbuf(&error); active = true;
        } catch (const std::exception& e) {
            std::cerr << "[WARN] Could not initialize diagnostics log: " << e.what() << '\n';
        }
    }
    ~CommandLog() {
        if (active) { std::cout.flush(); std::cerr.flush(); std::cout.rdbuf(stdoutBuffer); std::cerr.rdbuf(stderrBuffer); }
    }
};
}
