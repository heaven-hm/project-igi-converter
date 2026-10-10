#include "asset_json.hpp"

#include <cmath>
#include <cstring>

namespace gconv
{
std::string encodeHex(const Bytes &bytes, size_t start, size_t length)
{
    requireRange(bytes, start, length);
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(length * 2);
    for (size_t i = start; i < start + length; ++i)
    {
        result.push_back(digits[bytes[i] >> 4]);
        result.push_back(digits[bytes[i] & 15]);
    }
    return result;
}

Bytes decodeHex(const std::string &text)
{
    if (text.size() % 2 || text.size() > 1024ULL * 1024 * 1024)
        fail("Invalid hexadecimal payload size");
    auto digit = [](char value) -> uint8_t
    {
        if (value >= '0' && value <= '9')
            return value - '0';
        if (value >= 'a' && value <= 'f')
            return value - 'a' + 10;
        if (value >= 'A' && value <= 'F')
            return value - 'A' + 10;
        fail("Invalid hexadecimal digit");
    };
    Bytes result(text.size() / 2);
    for (size_t i = 0; i < result.size(); ++i)
        result[i] = (digit(text[i * 2]) << 4) | digit(text[i * 2 + 1]);
    return result;
}

float readFloat(const Bytes &bytes, size_t position)
{
    const uint32_t bits = readU32(bytes, position);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    if (!std::isfinite(value))
        fail("Non-finite floating-point value");
    return value;
}

void writeFloat(Bytes &bytes, size_t position, float value)
{
    if (!std::isfinite(value))
        fail("Non-finite floating-point value");
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    writeUnsigned(bytes, position, bits);
}

Json readJson(const fs::path &path)
{
    const auto bytes = readFile(path);
    return Json::parse(bytes.begin(), bytes.end());
}

void saveJson(const fs::path &path, const Json &document)
{
    const auto text = document.dump(2) + "\n";
    writeFile(path, Bytes(text.begin(), text.end()));
}
} // namespace gconv
