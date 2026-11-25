#pragma once

#include <limits>
#include <type_traits>

#include "common.hpp"
#include "nlohmann/json.hpp"

namespace gconv
{
using Json = nlohmann::json;

template <typename T> T jsonInteger(const Json &value)
{
    static_assert(std::is_integral<T>::value, "Expected an integer type");
    if (!value.is_number_integer())
        fail("Expected a JSON integer");
    if (value.is_number_unsigned())
    {
        const auto number = value.get<uint64_t>();
        if (number > static_cast<uint64_t>(std::numeric_limits<T>::max()))
            fail("JSON integer exceeds the format range");
        return static_cast<T>(number);
    }
    const auto number = value.get<int64_t>();
    if constexpr (std::is_unsigned<T>::value)
    {
        if (number < 0 || static_cast<uint64_t>(number) > std::numeric_limits<T>::max())
            fail("JSON integer exceeds the format range");
    }
    else
    {
        if (number < std::numeric_limits<T>::min() || number > std::numeric_limits<T>::max())
            fail("JSON integer exceeds the format range");
    }
    return static_cast<T>(number);
}

std::string encodeHex(const Bytes &bytes, size_t start, size_t length);
Bytes decodeHex(const std::string &text);
float readFloat(const Bytes &bytes, size_t position);
void writeFloat(Bytes &bytes, size_t position, float value);
Json readJson(const fs::path &path);
void saveJson(const fs::path &path, const Json &document);
Json exportTerrainData(const fs::path &path);
Bytes importTerrainData(const Json &document);
Json exportGraphData(const Bytes &bytes, bool igi2);
Bytes importGraphData(const Json &document);
Json exportMetadata(const fs::path &path, bool igi2);
Bytes importMetadata(const Json &document, bool igi2);
void exportMapImage(const fs::path &input, const fs::path &output);
void packMapImage(const fs::path &input, const fs::path &image, const fs::path &output);
int runAudioCommand(int argc, char **argv);
int runAssetCommand(int argc, char **argv, bool igi2);
Json exportFormAnimation(const Bytes &bytes);
Bytes importFormAnimation(const Json &document);
} // namespace gconv
