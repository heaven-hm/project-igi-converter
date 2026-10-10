#include "asset_json.hpp"

namespace gconv
{
namespace
{
uint32_t readBigEndian(const Bytes &bytes, size_t position)
{
    requireRange(bytes, position, 4);
    return (uint32_t(bytes[position]) << 24) | (uint32_t(bytes[position + 1]) << 16) |
           (uint32_t(bytes[position + 2]) << 8) | bytes[position + 3];
}
} // namespace

Json exportFormAnimation(const Bytes &bytes)
{
    if (!hasTag(bytes, 0, "FORM"))
        fail("Expected IGI 1 FORM animation");
    Json chunks = Json::array();
    size_t position = 0;
    while (position < bytes.size())
    {
        requireRange(bytes, position, 8);
        const std::string tag(reinterpret_cast<const char *>(bytes.data() + position), 4);
        const size_t size = readBigEndian(bytes, position + 4);
        const size_t length = tag == "FORM" ? 12 : 8 + size + (size % 2);
        requireRange(bytes, position, length);
        Json item = {{"tag", tag},
                     {"header", encodeHex(bytes, position, 8)},
                     {"data", encodeHex(bytes, position + 8, length - 8)}};
        if (tag == "TLST" && size % 4 == 0)
        {
            item["translations"] = Json::array();
            for (size_t offset = 0; offset < size; offset += 4)
                item["translations"].push_back(readFloat(bytes, position + 8 + offset));
        }
        if (tag == "BOAH" && size >= 12)
            item["duration"] = readFloat(bytes, position + 8);
        chunks.push_back(std::move(item));
        position += length;
    }
    return {{"kind", "form-animation"}, {"chunks", std::move(chunks)}};
}

Bytes importFormAnimation(const Json &document)
{
    Bytes bytes;
    for (const auto &item : document.at("chunks"))
    {
        const auto header = decodeHex(item.at("header"));
        auto payload = decodeHex(item.at("data"));
        if (header.size() != 8)
            fail("Invalid FORM chunk header");
        const std::string tag(reinterpret_cast<const char *>(header.data()), 4);
        const size_t size = readBigEndian(header, 4);
        if (item.at("tag") != tag || payload.size() != (tag == "FORM" ? 4 : size + (size % 2)))
            fail("FORM chunk tag or length does not match its header");
        if (item.contains("translations"))
        {
            if (tag != "TLST" || item.at("translations").size() * 4 != size)
                fail("Invalid skeleton translation array");
            for (size_t i = 0; i < item.at("translations").size(); ++i)
                writeFloat(payload, i * 4, item.at("translations")[i].get<float>());
        }
        if (item.contains("duration"))
        {
            if (tag != "BOAH" || size < 12)
                fail("Invalid animation header");
            writeFloat(payload, 0, item.at("duration").get<float>());
        }
        bytes.insert(bytes.end(), header.begin(), header.end());
        bytes.insert(bytes.end(), payload.begin(), payload.end());
    }
    // Validate the rebuilt stream before returning it to the output writer.
    exportFormAnimation(bytes);
    return bytes;
}
} // namespace gconv
