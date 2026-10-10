#include <algorithm>
#include <limits>

#include "asset_json.hpp"

namespace gconv
{
namespace
{
Json byteGrid(const Bytes &bytes, size_t start, size_t length)
{
    requireRange(bytes, start, length);
    Json values = Json::array();
    for (size_t i = start; i < start + length; ++i)
        values.push_back(bytes[i]);
    return values;
}

void appendBytes(Bytes &bytes, const Json &values)
{
    if (!values.is_array())
        fail("Expected byte array");
    for (const auto &value : values)
    {
        const auto number = jsonInteger<int64_t>(value);
        if (number < 0 || number > 255)
            fail("Byte value out of range");
        bytes.push_back(static_cast<uint8_t>(number));
    }
}

void appendWord(Bytes &bytes, uint32_t value, size_t width = 4)
{
    const auto offset = bytes.size();
    bytes.resize(offset + width);
    writeUnsigned(bytes, offset, value, width);
}

size_t pixelCount(uint32_t width, uint32_t height)
{
    if (!width || !height || width > 65535 || height > 65535 ||
        uint64_t(width) * height > 100000000)
        fail("Invalid terrain dimensions");
    return size_t(width) * height;
}
} // namespace

Json exportTerrainData(const fs::path &path)
{
    const auto bytes = readFile(path);
    auto format = path.extension().string();
    std::transform(format.begin(), format.end(), format.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    Json document = {{"format", format}, {"schema", "igi-asset/1"}};

    if (format == ".thm" || format == ".tlm" || format == ".tmm")
    {
        const bool heightMap = format == ".thm";
        const size_t headerSize = heightMap ? 52 : 44;
        requireRange(bytes, 0, headerSize);
        const auto type = readU32(bytes, 32);
        if (type != (heightMap ? 2U : format == ".tlm" ? 3U : 0U))
            fail("Terrain type does not match file extension");
        const auto width = readU32(bytes, headerSize - 8);
        const auto height = readU32(bytes, headerSize - 4);
        const auto count = pixelCount(width, height);
        const size_t stride = format == ".tmm" ? 1 : 4;
        requireRange(bytes, headerSize, count * stride);
        document["header"] = encodeHex(bytes, 0, headerSize);
        document["width"] = width;
        document["height"] = height;
        document["values"] = Json::array();
        if (heightMap)
        {
            for (size_t i = 0; i < count; ++i)
                document["values"].push_back(readFloat(bytes, headerSize + i * 4));
        }
        else
        {
            document["values"] = byteGrid(bytes, headerSize, count * stride);
        }
        document["trailing"] = encodeHex(bytes, headerSize + count * stride,
                                         bytes.size() - headerSize - count * stride);
        return document;
    }

    if (format == ".hmp" || format == ".bit")
    {
        // Both IGI 1 formats start with 256 twelve-byte table descriptors.
        constexpr size_t tableSize = 256 * 12;
        requireRange(bytes, 0, tableSize);
        document["header"] = encodeHex(bytes, 0, tableSize);
        document["grids"] = Json::array();
        size_t position = tableSize;
        for (size_t index = 0; index < 256; ++index)
        {
            const auto size = readU32(bytes, index * 12 + 8);
            if (!size)
                continue;
            if (size > 4096)
                fail("Invalid IGI 1 terrain grid size");
            const size_t length =
                format == ".hmp" ? size_t(size + 1) * (size + 1) : (size_t(size) * size + 7) / 8;
            document["grids"].push_back(
                {{"index", index}, {"size", size}, {"values", byteGrid(bytes, position, length)}});
            position += length;
        }
        if (position != bytes.size())
            fail("Terrain table and payload sizes disagree");
        return document;
    }

    if (format == ".lmp")
    {
        document["grids"] = Json::array();
        size_t position = 0;
        while (position < bytes.size())
        {
            const auto size = readU32(bytes, position);
            position += 4;
            if (size > 4096)
                fail("Invalid LMP grid size");
            const size_t length = size_t(size) * size;
            document["grids"].push_back(
                {{"size", size}, {"values", byteGrid(bytes, position, length)}});
            position += length;
        }
        return document;
    }

    if (format == ".cmd")
    {
        document["meshes"] = Json::array();
        size_t position = 0;
        while (position < bytes.size())
        {
            const auto triangles = readU16(bytes, position);
            const auto vertexOffset = readU16(bytes, position + 2);
            const auto parents = readU16(bytes, position + 4);
            const auto children = readU16(bytes, position + 6);
            if (vertexOffset != uint32_t(triangles) * 4)
                fail("CMD triangle table size mismatch");
            position += 8;
            Json mesh = {{"parent_vertices", parents},
                         {"child_vertices", children},
                         {"triangles", Json::array()},
                         {"packed_vertices", Json::array()}};
            for (size_t i = 0; i < triangles; ++i, position += 4)
                mesh["triangles"].push_back(readU32(bytes, position));
            for (size_t i = 0; i < uint32_t(parents) + children; ++i, position += 4)
                mesh["packed_vertices"].push_back(readU32(bytes, position));
            document["meshes"].push_back(std::move(mesh));
        }
        return document;
    }

    if (format == ".ctr")
    {
        if (bytes.size() % 32)
            fail("Invalid CTR record size");
        document["nodes"] = Json::array();
        for (size_t position = 0; position < bytes.size(); position += 32)
        {
            Json node = {{"children", Json::array()},
                         {"transforms", Json::array()},
                         {"mask", bytes[position + 24]},
                         {"padding", encodeHex(bytes, position + 25, 3)},
                         {"cmd_offset", readU32(bytes, position + 28)}};
            for (size_t i = 0; i < 8; ++i)
            {
                node["children"].push_back(static_cast<int16_t>(readU16(bytes, position + i * 2)));
                node["transforms"].push_back(static_cast<int8_t>(bytes[position + 16 + i]));
            }
            document["nodes"].push_back(std::move(node));
        }
        return document;
    }
    fail("Unsupported terrain format");
}

Bytes importTerrainData(const Json &document)
{
    const auto format = document.at("format").get<std::string>();
    Bytes bytes;
    if (format == ".thm" || format == ".tlm" || format == ".tmm")
    {
        const size_t headerSize = format == ".thm" ? 52 : 44;
        bytes = decodeHex(document.at("header"));
        if (bytes.size() != headerSize)
            fail("Invalid terrain header length");
        const auto width = jsonInteger<uint32_t>(document.at("width"));
        const auto height = jsonInteger<uint32_t>(document.at("height"));
        const auto count = pixelCount(width, height);
        if (width != readU32(bytes, headerSize - 8) || height != readU32(bytes, headerSize - 4))
            fail("Terrain dimensions must match the original template");
        writeUnsigned(bytes, headerSize - 8, width);
        writeUnsigned(bytes, headerSize - 4, height);
        const auto &values = document.at("values");
        if (values.size() != count * (format == ".tlm" ? 4 : 1))
            fail("Terrain pixel count mismatch");
        if (format == ".thm")
        {
            bytes.resize(headerSize + count * 4);
            for (size_t i = 0; i < count; ++i)
                writeFloat(bytes, headerSize + i * 4, values[i].get<float>());
        }
        else
        {
            appendBytes(bytes, values);
        }
        const auto trailing = decodeHex(document.at("trailing"));
        bytes.insert(bytes.end(), trailing.begin(), trailing.end());
        return bytes;
    }
    if (format == ".hmp" || format == ".bit")
    {
        bytes = decodeHex(document.at("header"));
        if (bytes.size() != 256 * 12)
            fail("Invalid terrain table length");
        size_t expectedGrids = 0;
        for (size_t i = 0; i < 256; ++i)
            if (readU32(bytes, i * 12 + 8))
                ++expectedGrids;
        if (document.at("grids").size() != expectedGrids)
            fail("Every original terrain grid must be present");
        size_t nextIndex = 0;
        for (const auto &grid : document.at("grids"))
        {
            const auto index = grid.at("index").get<size_t>();
            const auto size = jsonInteger<uint32_t>(grid.at("size"));
            if (index < nextIndex || index >= 256 || !size || size > 4096 ||
                size != readU32(bytes, index * 12 + 8))
                fail("Invalid terrain grid descriptor");
            nextIndex = index + 1;
            const size_t length =
                format == ".hmp" ? size_t(size + 1) * (size + 1) : (size_t(size) * size + 7) / 8;
            if (grid.at("values").size() != length)
                fail("Terrain grid size mismatch");
            appendBytes(bytes, grid.at("values"));
        }
        return bytes;
    }
    if (format == ".lmp")
    {
        for (const auto &grid : document.at("grids"))
        {
            const auto size = jsonInteger<uint32_t>(grid.at("size"));
            if (size > 4096 || grid.at("values").size() != size_t(size) * size)
                fail("LMP grid size mismatch");
            appendWord(bytes, size);
            appendBytes(bytes, grid.at("values"));
        }
        return bytes;
    }
    if (format == ".cmd")
    {
        for (const auto &mesh : document.at("meshes"))
        {
            const auto parents = jsonInteger<uint16_t>(mesh.at("parent_vertices"));
            const auto children = jsonInteger<uint16_t>(mesh.at("child_vertices"));
            const auto &triangles = mesh.at("triangles");
            const auto &vertices = mesh.at("packed_vertices");
            if (triangles.size() > 16383 || vertices.size() != uint32_t(parents) + children)
                fail("Invalid CMD mesh dimensions");
            appendWord(bytes, triangles.size(), 2);
            appendWord(bytes, triangles.size() * 4, 2);
            appendWord(bytes, parents, 2);
            appendWord(bytes, children, 2);
            for (const auto &value : triangles)
                appendWord(bytes, jsonInteger<uint32_t>(value));
            for (const auto &value : vertices)
                appendWord(bytes, jsonInteger<uint32_t>(value));
        }
        return bytes;
    }
    if (format == ".ctr")
    {
        for (const auto &node : document.at("nodes"))
        {
            if (node.at("children").size() != 8 || node.at("transforms").size() != 8)
                fail("Invalid CTR node arrays");
            for (const auto &value : node.at("children"))
                appendWord(bytes, static_cast<uint16_t>(jsonInteger<int16_t>(value)), 2);
            for (const auto &value : node.at("transforms"))
                bytes.push_back(static_cast<uint8_t>(jsonInteger<int8_t>(value)));
            bytes.push_back(jsonInteger<uint8_t>(node.at("mask")));
            const auto padding = decodeHex(node.at("padding"));
            if (padding.size() != 3)
                fail("Invalid CTR padding");
            bytes.insert(bytes.end(), padding.begin(), padding.end());
            appendWord(bytes, jsonInteger<uint32_t>(node.at("cmd_offset")));
        }
        return bytes;
    }
    fail("Unsupported terrain JSON format");
}

void exportMapImage(const fs::path &input, const fs::path &output)
{
    const auto document = exportTerrainData(input);
    const auto format = document.at("format").get<std::string>();
    const auto width = jsonInteger<uint32_t>(document.at("width"));
    const auto height = jsonInteger<uint32_t>(document.at("height"));
    const auto count = pixelCount(width, height);
    const auto &values = document.at("values");
    Bytes rgba(count * 4);
    if (format == ".tlm")
    {
        for (size_t i = 0; i < rgba.size(); ++i)
            rgba[i] = jsonInteger<uint8_t>(values[i]);
    }
    else
    {
        float low = std::numeric_limits<float>::max();
        float high = std::numeric_limits<float>::lowest();
        if (format == ".thm")
        {
            for (const auto &value : values)
            {
                low = std::min(low, value.get<float>());
                high = std::max(high, value.get<float>());
            }
        }
        for (size_t i = 0; i < count; ++i)
        {
            const auto level =
                format == ".tmm" ? jsonInteger<uint8_t>(values[i])
                : high > low
                    ? static_cast<uint8_t>((values[i].get<float>() - low) / (high - low) * 255)
                    : 0;
            rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = level;
            rgba[i * 4 + 3] = 255;
        }
    }
    requireDifferentFiles(input, output, "Input and image output must differ");
    saveImage(output, fromRgba(width, height, rgba, 32));
}

void packMapImage(const fs::path &input, const fs::path &imagePath, const fs::path &output)
{
    auto document = exportTerrainData(input);
    const auto format = document.at("format").get<std::string>();
    if (format == ".thm")
        fail("Use JSON for lossless float heightmap edits");
    const auto image = loadImage(imagePath);
    if (image.width != document.at("width") || image.height != document.at("height"))
        fail("Map image dimensions must match the original");
    const auto rgba = toRgba(image);
    document["values"] = Json::array();
    if (format == ".tlm")
    {
        for (auto value : rgba)
            document["values"].push_back(value);
    }
    else
    {
        for (size_t i = 0; i < rgba.size(); i += 4)
        {
            if (rgba[i] != rgba[i + 1] || rgba[i] != rgba[i + 2])
                fail("TMM input must contain grayscale material indices");
            document["values"].push_back(rgba[i]);
        }
    }
    requireDifferentFiles(input, output, "Template and output must differ");
    requireDifferentFiles(imagePath, output, "Image and output must differ");
    writeFile(output, importTerrainData(document));
}
} // namespace gconv
