#include <cstring>

#include "asset_json.hpp"

namespace gconv
{
namespace
{
Json floats(const Bytes &bytes, size_t start, size_t count)
{
    Json values = Json::array();
    for (size_t i = 0; i < count; ++i)
        values.push_back(readFloat(bytes, start + i * 4));
    return values;
}

Json exportIlff(const Bytes &bytes)
{
    requireRange(bytes, 0, 20);
    const std::string type(reinterpret_cast<const char *>(bytes.data() + 16), 4);
    Json document = {{"template", encodeHex(bytes, 0, bytes.size())},
                     {"container", type},
                     {"chunks", Json::array()}};
    for (const auto &chunk : walkIlffChunks(bytes, type.c_str()))
    {
        Json item = {{"tag", chunk.name},
                     {"position", chunk.position},
                     {"data", encodeHex(bytes, chunk.position, chunk.size)}};
        if (chunk.name == "DHNA" && chunk.size >= 52)
        {
            item["animation"] = {{"version", readU32(bytes, chunk.position + 8)},
                                 {"looping", readU32(bytes, chunk.position + 12)},
                                 {"duration", readU32(bytes, chunk.position + 16)},
                                 {"bone_count", readU32(bytes, chunk.position + 20)},
                                 {"entry_count", readU32(bytes, chunk.position + 28)},
                                 {"root_motion", readU32(bytes, chunk.position + 36)}};
        }
        if (chunk.name == "REIH" && (chunk.size % 13 == 0 || chunk.size % 13 == 1))
        {
            const size_t count = chunk.size / 13;
            const size_t start = chunk.position + chunk.size - count * 12;
            item["bones"] = Json::array();
            for (size_t i = 0; i < count; ++i)
                item["bones"].push_back({{"children", bytes[chunk.position + i]},
                                         {"offset", floats(bytes, start + i * 12, 3)}});
        }
        if (chunk.name == "AICH" && chunk.size == 8)
        {
            item["node_count"] = readU32(bytes, chunk.position);
            item["version"] = readU32(bytes, chunk.position + 4);
        }
        if (chunk.name == "AICN" && chunk.size == 144)
        {
            item["cover_words"] = Json::array();
            for (size_t i = 0; i < 36; ++i)
                item["cover_words"].push_back(readU32(bytes, chunk.position + i * 4));
        }
        document["chunks"].push_back(std::move(item));
    }
    return document;
}

void validateModelHeader(const Json &container, bool igi2)
{
    const size_t expectedSize = igi2 ? 176 : 156;
    bool headerFound = false;
    for (const auto &chunk : container.at("chunks"))
        if (chunk.at("tag") == "HSEM")
        {
            if (chunk.at("data").get<std::string>().size() != expectedSize * 2)
                fail("MEF model header does not match this game");
            headerFound = true;
        }
    if (!headerFound)
        fail("Missing MEF model header");
}

Bytes importIlff(const Json &document)
{
    auto bytes = decodeHex(document.at("template"));
    const auto type = document.at("container").get<std::string>();
    if (type.size() != 4)
        fail("Invalid ILFF container type");
    const auto original = walkIlffChunks(bytes, type.c_str());
    const auto &chunks = document.at("chunks");
    if (chunks.size() != original.size())
        fail("Template chunk count cannot change");
    for (size_t index = 0; index < original.size(); ++index)
    {
        const auto &chunk = original[index];
        const auto &item = chunks[index];
        if (item.at("tag") != chunk.name || item.at("position") != chunk.position)
            fail("Template chunk order or position cannot change");
        const auto payload = decodeHex(item.at("data"));
        if (payload.size() != chunk.size)
            fail("Template chunk dimensions cannot change");
        std::copy(payload.begin(), payload.end(), bytes.begin() + chunk.position);
        if (item.contains("animation"))
        {
            if (chunk.name != "DHNA" || chunk.size < 52)
                fail("Invalid animation chunk");
            const auto &value = item.at("animation");
            for (const auto &field :
                 std::vector<std::pair<std::string, size_t>>{{"version", 8},
                                                             {"looping", 12},
                                                             {"duration", 16},
                                                             {"bone_count", 20},
                                                             {"entry_count", 28},
                                                             {"root_motion", 36}})
                writeUnsigned(bytes, chunk.position + field.second,
                              value.at(field.first).get<uint32_t>());
        }
        if (item.contains("bones"))
        {
            if (chunk.name != "REIH" || item.at("bones").size() != chunk.size / 13)
                fail("Skeleton bone count cannot change");
            const auto &bones = item.at("bones");
            const size_t start = chunk.position + chunk.size - bones.size() * 12;
            for (size_t i = 0; i < bones.size(); ++i)
            {
                bytes[chunk.position + i] = bones[i].at("children").get<uint8_t>();
                if (bones[i].at("offset").size() != 3)
                    fail("Expected three bone offsets");
                for (size_t axis = 0; axis < 3; ++axis)
                    writeFloat(bytes, start + i * 12 + axis * 4,
                               bones[i].at("offset")[axis].get<float>());
            }
        }
        if (chunk.name == "AICH")
        {
            if (chunk.size != 8 || item.at("node_count") != readU32(bytes, chunk.position) ||
                item.at("version") != readU32(bytes, chunk.position + 4))
                fail("Graph-cover node count and version cannot change");
        }
        if (item.contains("cover_words"))
        {
            if (chunk.name != "AICN" || item.at("cover_words").size() != 36)
                fail("Invalid graph-cover record");
            for (size_t i = 0; i < 36; ++i)
                writeUnsigned(bytes, chunk.position + i * 4,
                              jsonInteger<uint32_t>(item.at("cover_words")[i]));
        }
    }
    return bytes;
}

Json exportOlm(const Bytes &bytes, bool igi2)
{
    requireRange(bytes, 0, 88);
    const auto count = readU32(bytes, 44);
    if (!count || count > 64)
        fail("Invalid OLM layer count");
    const size_t descriptorSize = igi2 ? 24 : 16;
    const size_t extraSize = igi2 ? 28 : 0;
    const size_t pixelStart = 88 + count * descriptorSize + (count - 1) * extraSize;
    requireRange(bytes, 0, pixelStart);
    Json document = {{"header", encodeHex(bytes, 0, pixelStart)}, {"layers", Json::array()}};
    size_t position = pixelStart;
    for (size_t i = 0; i < count; ++i)
    {
        const size_t descriptor = 88 + i * (descriptorSize + extraSize);
        const auto width = readU16(bytes, descriptor + descriptorSize - 4);
        const auto height = readU16(bytes, descriptor + descriptorSize - 2);
        if (!width || !height || uint64_t(width) * height > 100000000)
            fail("Invalid OLM layer dimensions");
        const size_t length = size_t(width) * height * 4;
        requireRange(bytes, position, length);
        Json values = Json::array();
        for (size_t pixel = position; pixel < position + length; ++pixel)
            values.push_back(bytes[pixel]);
        document["layers"].push_back(
            {{"width", width}, {"height", height}, {"rgba", std::move(values)}});
        position += length;
    }
    if (position != bytes.size())
        fail("OLM header and pixel payload sizes disagree");
    return document;
}

Bytes importOlm(const Json &document, bool igi2)
{
    auto bytes = decodeHex(document.at("header"));
    requireRange(bytes, 0, 88);
    const auto count = readU32(bytes, 44);
    const size_t descriptorSize = igi2 ? 24 : 16;
    const size_t extraSize = igi2 ? 28 : 0;
    if (!count || count > 64 || count != document.at("layers").size() ||
        bytes.size() != 88 + count * descriptorSize + (count - 1) * extraSize)
        fail("Invalid OLM header or layer count");
    for (size_t i = 0; i < count; ++i)
    {
        const auto &layer = document.at("layers")[i];
        const size_t descriptor = 88 + i * (descriptorSize + extraSize);
        const auto width = readU16(bytes, descriptor + descriptorSize - 4);
        const auto height = readU16(bytes, descriptor + descriptorSize - 2);
        if (layer.at("width") != width || layer.at("height") != height ||
            layer.at("rgba").size() != size_t(width) * height * 4)
            fail("OLM layer dimensions cannot change");
        for (const auto &value : layer.at("rgba"))
        {
            const auto number = jsonInteger<int64_t>(value);
            if (number < 0 || number > 255)
                fail("OLM pixel channel out of range");
            bytes.push_back(static_cast<uint8_t>(number));
        }
    }
    exportOlm(bytes, igi2);
    return bytes;
}
} // namespace

Json exportMetadata(const fs::path &path, bool igi2)
{
    const auto bytes = readFile(path);
    auto format = path.extension().string();
    std::transform(format.begin(), format.end(), format.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    Json document = {{"schema", "igi-asset/1"}, {"format", format}, {"game", igi2 ? 2 : 1}};
    if (format == ".dat" && bytes.size() >= 4 && readU32(bytes, 0) == 0xffeeddcc)
    {
        document.update(exportGraphData(bytes, igi2));
        return document;
    }
    if (format == ".syn")
    {
        if (bytes.size() % 4)
            fail("SYN payload must contain float32 samples");
        document["samples"] = floats(bytes, 0, bytes.size() / 4);
        return document;
    }
    if (format == ".olm")
    {
        document.update(exportOlm(bytes, igi2));
        return document;
    }
    if (format == ".dat" && bytes.size() >= 14 && readU32(bytes, 0) == 10 && bytes[4] == '\n' &&
        std::memcmp(bytes.data() + 5, "Ver. 3.0\n", 9) == 0)
    {
        if ((bytes.size() - 14) % 40)
            fail("Invalid forest record size");
        document["kind"] = "forest";
        document["header"] = encodeHex(bytes, 0, 14);
        document["records"] = Json::array();
        for (size_t position = 14; position < bytes.size(); position += 40)
        {
            Json color = Json::array();
            for (size_t i = 0; i < 4; ++i)
                color.push_back(bytes[position + 36 + i]);
            document["records"].push_back(
                {{"transform", floats(bytes, position, 9)}, {"rgba", std::move(color)}});
        }
        return document;
    }
    if (!igi2 && format == ".iff" && hasTag(bytes, 0, "FORM"))
    {
        document.update(exportFormAnimation(bytes));
        return document;
    }
    if (format == ".iff" && (!igi2 || !hasTag(bytes, 16, "MINA")))
        fail("Animation container does not match this game");
    if (bytes.size() >= 20 && hasTag(bytes, 0, "ILFF"))
    {
        document["kind"] = "ilff";
        auto container = exportIlff(bytes);
        if (format == ".mef")
            validateModelHeader(container, igi2);
        document.update(container);
        return document;
    }
    fail("Unsupported metadata format; use its dedicated command family");
}

Bytes importMetadata(const Json &document, bool igi2)
{
    const auto format = document.at("format").get<std::string>();
    if (format == ".mef")
        validateModelHeader(document, igi2);
    if (format == ".iff" && ((igi2 && document.value("container", "") != "MINA") ||
                             (!igi2 && document.value("kind", "") != "form-animation")))
        fail("Animation JSON container does not match this game");

    if (!igi2 && document.value("kind", "") == "form-animation")
        return importFormAnimation(document);
    if (format == ".syn")
    {
        Bytes bytes(document.at("samples").size() * 4);
        for (size_t i = 0; i < document.at("samples").size(); ++i)
            writeFloat(bytes, i * 4, document.at("samples")[i].get<float>());
        return bytes;
    }
    if (format == ".olm")
        return importOlm(document, igi2);
    if (document.value("kind", "") == "graph")
        return importGraphData(document);
    if (document.value("kind", "") == "forest")
    {
        auto bytes = decodeHex(document.at("header"));
        if (bytes.size() != 14)
            fail("Invalid forest header length");
        for (const auto &record : document.at("records"))
        {
            if (record.at("transform").size() != 9 || record.at("rgba").size() != 4)
                fail("Invalid forest record arrays");
            const size_t position = bytes.size();
            bytes.resize(position + 40);
            for (size_t i = 0; i < 9; ++i)
                writeFloat(bytes, position + i * 4, record.at("transform")[i].get<float>());
            for (size_t i = 0; i < 4; ++i)
                bytes[position + 36 + i] = jsonInteger<uint8_t>(record.at("rgba")[i]);
        }
        return bytes;
    }
    if (document.value("kind", "") == "ilff")
        return importIlff(document);
    fail("Unsupported metadata JSON format");
}
} // namespace gconv
