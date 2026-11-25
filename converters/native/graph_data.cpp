#include <cmath>
#include <cstring>

#include "asset_json.hpp"

namespace gconv
{
namespace
{
double readDouble(const Bytes &bytes, size_t position)
{
    const uint64_t bits = readU32(bytes, position) | uint64_t(readU32(bytes, position + 4)) << 32;
    double value;
    std::memcpy(&value, &bits, sizeof(value));
    if (!std::isfinite(value))
        fail("Non-finite graph coordinate");
    return value;
}

void writeDouble(Bytes &bytes, size_t position, double value)
{
    if (!std::isfinite(value))
        fail("Non-finite graph coordinate");
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    writeUnsigned(bytes, position, static_cast<uint32_t>(bits));
    writeUnsigned(bytes, position + 4, static_cast<uint32_t>(bits >> 32));
}
} // namespace

Json exportGraphData(const Bytes &bytes, bool igi2)
{
    const bool legacy = !igi2 && !bytes.empty() && bytes[0] == 5;
    if (legacy)
    {
        requireRange(bytes, 0, 15);
        if (bytes[5] != 5 || bytes[10] != 5)
            fail("Invalid legacy graph header");
        const auto capacity = readU32(bytes, 6);
        const uint64_t entries = uint64_t(capacity) * capacity;
        if (!capacity || entries > (bytes.size() - 15) / 8 || readU32(bytes, 1) > capacity)
            fail("Invalid legacy graph adjacency table size");
        const size_t tableBytes = static_cast<size_t>(entries * 8);
        const size_t tableStart = bytes.size() - tableBytes;
        // The tagged stream terminates with 00, followed by an array marker
        // (0a), an int32 length marker (05), and the adjacency byte count.
        if (tableStart < 22)
            fail("Truncated legacy graph adjacency header");
        const size_t end = tableStart - 7;
        if (bytes[end] != 0 || bytes[end + 1] != 10 || bytes[end + 2] != 5 ||
            readU32(bytes, end + 3) != tableBytes)
            fail("Invalid legacy graph adjacency header");
        Json document = {{"kind", "graph"}, {"layout", "legacy-tagged"},
            {"header", encodeHex(bytes, 0, 15)}, {"records", Json::array()},
            {"adjacency", encodeHex(bytes, end, bytes.size() - end)}};
        size_t position = 15;
        while (position < end)
        {
            const auto tag = bytes[position++];
            Json record = {{"header", encodeHex(bytes, position - 1, 1)}, {"type", tag * 257}};
            size_t length = tag == 8 ? 24 : 4;
            if (tag == 9)
            {
                if (position == end)
                    fail("Truncated legacy graph string length");
                requireRange(bytes, position, 1);
                length = bytes[position++];
            }
            if (length > end - position)
                fail("Truncated legacy graph property");
            if (tag == 5)
                record["value"] = static_cast<int32_t>(readU32(bytes, position));
            else if (tag == 6)
                record["value"] = readFloat(bytes, position);
            else if (tag == 8)
                record["value"] = Json::array({readDouble(bytes, position),
                    readDouble(bytes, position + 8), readDouble(bytes, position + 16)});
            else if (tag == 9)
                record["value"] = encodeHex(bytes, position, length);
            else
                fail("Unsupported legacy graph property tag");
            document["records"].push_back(std::move(record));
            position += length;
        }
        return document;
    }
    if (readU32(bytes, 0) != 0xffeeddcc)
        fail("Invalid graph signature");
    size_t position = 4;
    if (!igi2)
    {
        const auto capacity = readU32(bytes, 12);
        const uint64_t entries = uint64_t(capacity) * capacity;
        if (entries > bytes.size() / 8)
            fail("Invalid IGI 1 adjacency table size");
        const uint64_t tableSize = entries * 8;
        position = 30 + static_cast<size_t>(tableSize);
        requireRange(bytes, 0, position);
    }
    Json document = {
        {"kind", "graph"}, {"header", encodeHex(bytes, 0, position)}, {"records", Json::array()}};
    while (position < bytes.size())
    {
        requireRange(bytes, position, 8);
        if (bytes[position] != 4)
            fail("Invalid graph record marker");
        const auto type = readU16(bytes, position + 6);
        Json record = {{"header", encodeHex(bytes, position, 8)}, {"type", type}};
        position += 8;
        if (type == 0x0505)
        {
            record["value"] = readU32(bytes, position);
            position += 4;
        }
        else if (type == 0x0606)
        {
            record["value"] = readFloat(bytes, position);
            position += 4;
        }
        else if (type == 0x0808)
        {
            record["value"] = Json::array();
            for (size_t axis = 0; axis < 3; ++axis)
                record["value"].push_back(readDouble(bytes, position + axis * 8));
            position += 24;
        }
        else if (type == 0x0909)
        {
            requireRange(bytes, position, 1);
            const size_t length = bytes[position++];
            requireRange(bytes, position, length);
            record["value"] = encodeHex(bytes, position, length);
            position += length;
        }
        else
        {
            fail("Unsupported graph property type: " + std::to_string(type));
        }
        document["records"].push_back(std::move(record));
    }
    return document;
}

Bytes importGraphData(const Json &document)
{
    const bool legacy = document.value("layout", std::string()) == "legacy-tagged";
    auto bytes = decodeHex(document.at("header"));
    if (legacy && (bytes.size() != 15 || bytes[0] != 5 || bytes[5] != 5 || bytes[10] != 5))
        fail("Invalid legacy graph header");
    for (const auto &record : document.at("records"))
    {
        const auto header = decodeHex(record.at("header"));
        if ((!legacy && (header.size() != 8 || header[0] != 4)) ||
            (legacy && header.size() != 1))
            fail("Invalid graph record header");
        const auto type = legacy ? header[0] * 257 : readU16(header, 6);
        if (record.at("type") != type)
            fail("Graph property type cannot change");
        bytes.insert(bytes.end(), header.begin(), header.end());
        const size_t position = bytes.size();
        if (type == 0x0505)
        {
            bytes.resize(position + 4);
            writeUnsigned(bytes, position, legacy
                ? static_cast<uint32_t>(jsonInteger<int32_t>(record.at("value")))
                : jsonInteger<uint32_t>(record.at("value")));
        }
        else if (type == 0x0606)
        {
            bytes.resize(position + 4);
            writeFloat(bytes, position, record.at("value").get<float>());
        }
        else if (type == 0x0808)
        {
            if (record.at("value").size() != 3)
                fail("Graph position needs three coordinates");
            bytes.resize(position + 24);
            for (size_t axis = 0; axis < 3; ++axis)
                writeDouble(bytes, position + axis * 8, record.at("value")[axis].get<double>());
        }
        else if (type == 0x0909)
        {
            const auto value = decodeHex(record.at("value"));
            if (value.size() > 255)
                fail("Graph variable property exceeds 255 bytes");
            bytes.push_back(static_cast<uint8_t>(value.size()));
            bytes.insert(bytes.end(), value.begin(), value.end());
        }
        else
        {
            fail("Unsupported graph property type");
        }
    }
    if (legacy)
    {
        const auto adjacency = decodeHex(document.at("adjacency"));
        bytes.insert(bytes.end(), adjacency.begin(), adjacency.end());
        exportGraphData(bytes, false);
    }
    return bytes;
}
} // namespace gconv
