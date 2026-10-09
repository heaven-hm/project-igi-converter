#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "common.hpp"
namespace gconv
{
std::vector<ResourceEntry> readResources(const Bytes &b)
{
    std::vector<ResourceEntry> e;
    std::string name;
    bool have = false;
    for (auto c : walkIlffChunks(b, "IRES"))
    {
        if (c.name == "NAME")
        {
            name.assign(reinterpret_cast<const char *>(b.data() + c.position), c.size);
            while (!name.empty() && !name.back())
                name.pop_back();
            have = true;
        }
        else if (c.name == "BODY")
        {
            if (!have)
                fail("BODY without NAME");
            e.push_back({name, Bytes(b.begin() + c.position, b.begin() + c.position + c.size)});
            have = false;
        }
    }
    return e;
}
fs::path resourcePath(std::string s)
{
    if (s.substr(0, 6) == "LOCAL:")
        s.erase(0, 6);
    std::replace(s.begin(), s.end(), '\\', '/');
    fs::path p(s);
    if (p.empty() || p.is_absolute() || s.find(':') != std::string::npos)
        fail("Unsafe resource name: " + s);
    for (auto part : p)
        if (part == "..")
            fail("Unsafe resource name: " + s);
    return p;
}
void listResources(const fs::path &input)
{
    for (const auto &resource : readResources(readFile(input)))
        std::cout << resource.bytes.size() << "\t" << resource.name << "\n";
}
void extractResources(const fs::path &input, const fs::path &output)
{
    auto e = readResources(readFile(input));
    auto root = fs::weakly_canonical(fs::absolute(output));
    std::vector<fs::path> targets;
    for (auto &v : e)
    {
        auto p = root / resourcePath(v.name);
        auto canonical = fs::weakly_canonical(p);
        auto relative = canonical.lexically_relative(root);
        if (relative.empty() || *relative.begin() == "..")
            fail("Resource path leaves output directory");
        std::string lowered = canonical.string();
        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        for (auto &t : targets)
            if (t.string() == lowered)
                fail("Duplicate resource output: " + v.name);
        targets.emplace_back(lowered);
        if (fs::exists(p))
            fail("Output already exists: " + p.string());
    }
    fs::create_directories(root);
    for (auto &v : e)
        writeFile(root / resourcePath(v.name), v.bytes);
    std::cout << "Extracted " << e.size() << " resources\n";
}
void packResources(const fs::path &dir, const fs::path &output)
{
    if (!fs::is_directory(dir))
        fail("Resource input must be a directory");
    std::vector<fs::path> files;
    for (auto &p : fs::recursive_directory_iterator(dir))
    {
        if (p.is_symlink())
            fail("Symlink in resource input");
        if (p.is_regular_file())
        {
            requireDifferentFiles(p.path(), output,
                                  "Archive output must differ from every input resource");
            files.push_back(p.path());
        }
    }
    std::sort(files.begin(), files.end());
    Bytes b(20);
    std::memcpy(b.data(), "ILFF", 4);
    writeUnsigned(b, 8, 4);
    std::memcpy(b.data() + 16, "IRES", 4);
    size_t previous = 0;
    auto add = [&](const char *id, const Bytes &data)
    {
        size_t p = b.size();
        if (previous)
            writeUnsigned(b, previous + 12, uint32_t(p - previous));
        previous = p;
        b.resize(p + 16);
        std::memcpy(b.data() + p, id, 4);
        writeUnsigned(b, p + 4, uint32_t(data.size()));
        writeUnsigned(b, p + 8, 4);
        b.insert(b.end(), data.begin(), data.end());
        while (b.size() % 4)
            b.push_back(0);
    };
    for (auto &p : files)
    {
        auto s = "LOCAL:" + fs::relative(p, dir).generic_string();
        Bytes name(s.begin(), s.end());
        name.push_back(0);
        while (name.size() % 4)
            name.push_back(0);
        add("NAME", name);
        add("BODY", readFile(p));
    }
    writeUnsigned(b, 4, uint32_t(b.size()));
    writeFile(output, b);
    std::cout << "Packed " << files.size() << " resources\n";
}
} // namespace gconv
