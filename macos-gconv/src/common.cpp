#include "common.hpp"

#include <cstring>
#include <fstream>
#include <stdexcept>
namespace gconv
{
[[noreturn]] void fail(const std::string &s)
{
    throw std::runtime_error(s);
}
void requireRange(const Bytes &b, size_t p, size_t n)
{
    if (p > b.size() || n > b.size() - p)
        fail("Truncated input at byte " + std::to_string(p));
}
uint16_t readU16(const Bytes &b, size_t p)
{
    requireRange(b, p, 2);
    return b[p] | uint16_t(b[p + 1]) << 8;
}
uint32_t readU32(const Bytes &b, size_t p)
{
    requireRange(b, p, 4);
    return b[p] | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 | uint32_t(b[p + 3]) << 24;
}
void writeUnsigned(Bytes &b, size_t p, uint32_t v, size_t n)
{
    requireRange(b, p, n);
    for (size_t i = 0; i < n; ++i)
        b[p + i] = uint8_t(v >> (8 * i));
}
bool hasTag(const Bytes &b, size_t p, const char *t)
{
    requireRange(b, p, 4);
    return std::memcmp(b.data() + p, t, 4) == 0;
}
Bytes readFile(const fs::path &p)
{
    std::ifstream f(p, std::ios::binary);
    if (!f)
        fail("Cannot open " + p.string());
    f.seekg(0, std::ios::end);
    auto n = f.tellg();
    if (n < 0 || n > 1024LL * 1024 * 1024)
        fail("Invalid or oversized input");
    Bytes b(static_cast<size_t>(n));
    f.seekg(0);
    if (!b.empty() && !f.read(reinterpret_cast<char *>(b.data()), b.size()))
        fail("Read failed");
    return b;
}
void writeFile(const fs::path &p, const Bytes &b)
{
    if (p.has_parent_path())
        fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary);
    if (!f || (!b.empty() && !f.write(reinterpret_cast<const char *>(b.data()), b.size())))
        fail("Write failed: " + p.string());
}
void requireDifferentFiles(const fs::path &input, const fs::path &output,
                           const std::string &message)
{
    if (fs::weakly_canonical(input) == fs::weakly_canonical(output) ||
        (fs::exists(input) && fs::exists(output) && fs::equivalent(input, output)))
    {
        fail(message);
    }
}

std::vector<Chunk> walkIlffChunks(const Bytes &b, const char *type)
{
    requireRange(b, 0, 20);
    if (!hasTag(b, 0, "ILFF") || !hasTag(b, 16, type))
        fail(std::string("Expected ILFF/") + type);
    std::vector<Chunk> c;
    const size_t alignment = readU32(b, 8);
    if (alignment > 4096 || (alignment & (alignment - 1)))
        fail("Invalid ILFF alignment");
    size_t p = alignment ? (20 + alignment - 1) & ~(alignment - 1) : 20;
    if (p > b.size())
        fail("Truncated aligned ILFF header");
    while (p < b.size())
    {
        requireRange(b, p, 16);
        size_t n = readU32(b, p + 4), next = readU32(b, p + 12);
        const uint32_t chunkAlignment = readU32(b, p + 8);
        if (chunkAlignment > 4096 || (chunkAlignment && (chunkAlignment & (chunkAlignment - 1))))
            fail("Invalid ILFF chunk alignment");
        const size_t dataStart =
            chunkAlignment ? (p + 16 + chunkAlignment - 1) & ~(chunkAlignment - 1) : p + 16;
        requireRange(b, dataStart, n);
        c.push_back({std::string(reinterpret_cast<const char *>(b.data() + p), 4), dataStart, n,
                     chunkAlignment});
        if (!next)
            break;
        if (next < dataStart - p + n || next > b.size() - p)
            fail("Invalid ILFF chunk advance");
        p += next;
    }
    return c;
}
} // namespace gconv
