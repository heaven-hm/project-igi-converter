#include "igi2_forest_parser.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <utility>

namespace igi2 {
namespace {

bool Fail(std::string& err, const char* msg)
{
    if (err.empty())
        err = msg;
    return false;
}

uint32_t ReadLE32(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0])
        | (static_cast<uint32_t>(p[1]) << 8)
        | (static_cast<uint32_t>(p[2]) << 16)
        | (static_cast<uint32_t>(p[3]) << 24);
}

void WriteLE32(std::vector<uint8_t>& out, uint32_t v)
{
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

float BitsToFloat(uint32_t bits)
{
    float f = 0.0f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

uint32_t FloatToBits(float f)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &f, sizeof(bits));
    return bits;
}

// ---------------------------------------------------------------------------
// A deliberately small JSON reader. It only needs to understand the flat
// schema emitted by Forest_FormatJSON, but it is a real recursive-descent
// parser (not a substring scan), so malformed input is rejected reliably.
// ---------------------------------------------------------------------------
struct JValue {
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    double number = 0.0;
    bool boolean = false;
    std::string str;
    std::vector<JValue> array;
    std::vector<std::pair<std::string, JValue>> object;

    const JValue* Find(const std::string& key) const
    {
        if (type != Type::Object)
            return nullptr;
        for (const auto& kv : object)
            if (kv.first == key)
                return &kv.second;
        return nullptr;
    }
};

class JsonReader {
public:
    explicit JsonReader(const std::string& text) : text_(text) {}

    bool Parse(JValue& value)
    {
        if (!SkipWhitespace() || !Value(value))
            return false;
        SkipWhitespace();
        return pos_ == text_.size();
    }

    const std::string& Error() const { return error_; }

private:
    const std::string& text_;
    size_t pos_ = 0;
    std::string error_;

    bool Fail(const char* msg)
    {
        if (error_.empty())
            error_ = std::string(msg) + " at byte " + std::to_string(pos_);
        return false;
    }

    bool SkipWhitespace()
    {
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
                ++pos_;
            else
                break;
        }
        return true;
    }

    bool Literal(const char* lit)
    {
        const size_t n = std::strlen(lit);
        if (text_.compare(pos_, n, lit) != 0)
            return Fail("invalid literal");
        pos_ += n;
        return true;
    }

    bool Value(JValue& value)
    {
        if (!SkipWhitespace() || pos_ >= text_.size())
            return Fail("unexpected end of JSON");
        const char c = text_[pos_];
        if (c == '{') return Object(value);
        if (c == '[') return Array(value);
        if (c == '"') {
            value.type = JValue::Type::String;
            return String(value.str);
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            value.type = JValue::Type::Number;
            return Number(value.number);
        }
        if (c == 't') {
            if (!Literal("true")) return false;
            value.type = JValue::Type::Bool;
            value.boolean = true;
            return true;
        }
        if (c == 'f') {
            if (!Literal("false")) return false;
            value.type = JValue::Type::Bool;
            value.boolean = false;
            return true;
        }
        if (c == 'n') {
            if (!Literal("null")) return false;
            value.type = JValue::Type::Null;
            return true;
        }
        return Fail("unexpected character in JSON");
    }

    static void AppendUtf8(std::string& out, uint32_t cp)
    {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        }
        else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    bool Hex4(uint32_t& out)
    {
        if (pos_ + 4 > text_.size())
            return Fail("truncated \\u escape");
        out = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text_[pos_++];
            out <<= 4;
            if (c >= '0' && c <= '9') out |= static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') out |= static_cast<uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') out |= static_cast<uint32_t>(c - 'A' + 10);
            else return Fail("invalid \\u escape");
        }
        return true;
    }

    bool String(std::string& out)
    {
        if (pos_ >= text_.size() || text_[pos_] != '"')
            return Fail("expected string");
        ++pos_;
        out.clear();
        while (pos_ < text_.size()) {
            const char c = text_[pos_++];
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20)
                return Fail("control character in string");
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (pos_ >= text_.size())
                return Fail("truncated escape");
            const char e = text_[pos_++];
            switch (e) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                uint32_t cp = 0;
                if (!Hex4(cp)) return false;
                AppendUtf8(out, cp);
                break;
            }
            default: return Fail("unknown escape");
            }
        }
        return Fail("unterminated string");
    }

    bool Number(double& out)
    {
        const size_t start = pos_;
        if (pos_ < text_.size() && text_[pos_] == '-')
            ++pos_;
        if (pos_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(text_[pos_])))
            return Fail("invalid number");
        if (text_[pos_] == '0') {
            ++pos_;
        }
        else {
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_])))
                ++pos_;
        }
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            if (pos_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(text_[pos_])))
                return Fail("invalid fraction");
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_])))
                ++pos_;
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-'))
                ++pos_;
            if (pos_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(text_[pos_])))
                return Fail("invalid exponent");
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_])))
                ++pos_;
        }
        const std::string token = text_.substr(start, pos_ - start);
        out = std::strtod(token.c_str(), nullptr);
        if (!std::isfinite(out))
            return Fail("number out of range");
        return true;
    }

    bool Array(JValue& value)
    {
        value.type = JValue::Type::Array;
        ++pos_; // '['
        if (!SkipWhitespace())
            return false;
        if (pos_ < text_.size() && text_[pos_] == ']') {
            ++pos_;
            return true;
        }
        for (;;) {
            JValue item;
            if (!Value(item)) return false;
            value.array.push_back(std::move(item));
            if (!SkipWhitespace())
                return false;
            if (pos_ >= text_.size())
                return Fail("unterminated array");
            if (text_[pos_] == ']') {
                ++pos_;
                return true;
            }
            if (text_[pos_] != ',')
                return Fail("expected ',' in array");
            ++pos_;
        }
    }

    bool Object(JValue& value)
    {
        value.type = JValue::Type::Object;
        ++pos_; // '{'
        if (!SkipWhitespace())
            return false;
        if (pos_ < text_.size() && text_[pos_] == '}') {
            ++pos_;
            return true;
        }
        for (;;) {
            if (!SkipWhitespace())
                return false;
            std::string key;
            if (!String(key)) return false;
            if (!SkipWhitespace())
                return false;
            if (pos_ >= text_.size() || text_[pos_] != ':')
                return Fail("expected ':' in object");
            ++pos_;
            JValue item;
            if (!Value(item)) return false;
            value.object.emplace_back(std::move(key), std::move(item));
            if (!SkipWhitespace())
                return false;
            if (pos_ >= text_.size())
                return Fail("unterminated object");
            if (text_[pos_] == '}') {
                ++pos_;
                return true;
            }
            if (text_[pos_] != ',')
                return Fail("expected ',' in object");
            ++pos_;
        }
    }
};

bool GetNumber(const JValue& object, const char* key, double& out, std::string& err)
{
    const JValue* v = object.Find(key);
    if (!v)
        return Fail(err, (std::string("missing JSON field '") + key + "'").c_str());
    if (v->type != JValue::Type::Number)
        return Fail(err, (std::string("JSON field '") + key + "' must be a number").c_str());
    out = v->number;
    return true;
}

bool GetFloat(const JValue& object, const char* key, float& out, std::string& err)
{
    double d = 0.0;
    if (!GetNumber(object, key, d, err))
        return false;
    out = static_cast<float>(d);
    return true;
}

bool GetUint(const JValue& object, const char* key, uint32_t maxValue, uint32_t& out, std::string& err)
{
    double d = 0.0;
    if (!GetNumber(object, key, d, err))
        return false;
    if (!std::isfinite(d) || d < 0.0 || d > static_cast<double>(maxValue) || std::floor(d) != d)
        return Fail(err, (std::string("JSON field '") + key + "' is out of range").c_str());
    out = static_cast<uint32_t>(d);
    return true;
}

std::string JsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out.push_back(c); break;
        }
    }
    return out;
}

} // namespace

// ---------------------------------------------------------------------------
// Binary I/O
// ---------------------------------------------------------------------------

bool Forest_ParseBytes(const void* data, size_t size, ForestFile& out, std::string& err)
{
    out = ForestFile{};
    err.clear();

    const auto* bytes = static_cast<const uint8_t*>(data);
    if (!bytes || size < kForestHeaderSize)
        return Fail(err, "file is smaller than the 14-byte DAT Forest header");

    const uint32_t headerValue = ReadLE32(bytes);
    if (headerValue != kForestHeaderValue) {
        err = "unexpected DAT Forest header value " + std::to_string(headerValue)
            + " (expected 10)";
        return false;
    }
    if (bytes[4] != 0x0A)
        return Fail(err, "missing newline after header value");
    if (std::memcmp(bytes + 5, kForestVersion, 8) != 0)
        return Fail(err, "unexpected DAT Forest version (expected 'Ver. 3.0')");
    if (bytes[13] != 0x0A)
        return Fail(err, "missing newline after version string");

    const size_t recordBytes = size - kForestHeaderSize;
    if (recordBytes % kForestRecordSize != 0) {
        err = "record area of " + std::to_string(recordBytes)
            + " bytes is not a multiple of 40";
        return false;
    }

    out.headerValue = headerValue;
    out.version.assign(reinterpret_cast<const char*>(bytes + 5), 8);
    out.records.reserve(recordBytes / kForestRecordSize);

    for (size_t i = 0; i < recordBytes / kForestRecordSize; ++i) {
        const uint8_t* r = bytes + kForestHeaderSize + i * kForestRecordSize;
        ForestRecord rec;
        for (int k = 0; k < 3; ++k) {
            rec.position[k] = BitsToFloat(ReadLE32(r + k * 4));
            rec.rotation[k] = BitsToFloat(ReadLE32(r + 12 + k * 4));
            rec.scale[k] = BitsToFloat(ReadLE32(r + 24 + k * 4));
        }
        std::memcpy(rec.color, r + 36, 4);
        out.records.push_back(rec);
    }

    out.valid = true;
    return true;
}

bool Forest_ParseFile(const std::string& path, ForestFile& out, std::string& err)
{
    out = ForestFile{};
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        err = "cannot open input file: " + path;
        return false;
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return Forest_ParseBytes(bytes.data(), bytes.size(), out, err);
}

bool Forest_Serialize(const ForestFile& in, std::vector<uint8_t>& outBytes, std::string& err)
{
    err.clear();
    if (in.version.size() != 8 || in.version != kForestVersion)
        return Fail(err, "version must be exactly 'Ver. 3.0'");

    outBytes.clear();
    outBytes.reserve(kForestHeaderSize + in.records.size() * kForestRecordSize);
    WriteLE32(outBytes, in.headerValue);
    outBytes.push_back(0x0A);
    for (char c : in.version)
        outBytes.push_back(static_cast<uint8_t>(c));
    outBytes.push_back(0x0A);

    for (const ForestRecord& rec : in.records) {
        for (int k = 0; k < 3; ++k) WriteLE32(outBytes, FloatToBits(rec.position[k]));
        for (int k = 0; k < 3; ++k) WriteLE32(outBytes, FloatToBits(rec.rotation[k]));
        for (int k = 0; k < 3; ++k) WriteLE32(outBytes, FloatToBits(rec.scale[k]));
        for (int k = 0; k < 4; ++k) outBytes.push_back(rec.color[k]);
    }
    return true;
}

bool Forest_WriteFile(const std::string& path, const ForestFile& in, std::string& err)
{
    std::vector<uint8_t> bytes;
    if (!Forest_Serialize(in, bytes, err))
        return false;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        err = "cannot open output file: " + path;
        return false;
    }
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) {
        err = "failed writing output file: " + path;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// JSON I/O
// ---------------------------------------------------------------------------

std::string Forest_FormatJSON(const ForestFile& in)
{
    std::ostringstream os;
    os << std::setprecision(9);
    os << "{\n";
    os << "  \"schema\": \"igi2-forest/1\",\n";
    os << "  \"header_value\": " << in.headerValue << ",\n";
    os << "  \"version\": \"" << JsonEscape(in.version) << "\",\n";
    os << "  \"record_count\": " << in.records.size() << ",\n";
    os << "  \"records\": [\n";
    for (size_t i = 0; i < in.records.size(); ++i) {
        const ForestRecord& r = in.records[i];
        os << "    {\n";
        os << "      \"position_x\": " << r.position[0] << ",\n";
        os << "      \"position_y\": " << r.position[1] << ",\n";
        os << "      \"position_z\": " << r.position[2] << ",\n";
        os << "      \"rotation_x\": " << r.rotation[0] << ",\n";
        os << "      \"rotation_y\": " << r.rotation[1] << ",\n";
        os << "      \"rotation_z\": " << r.rotation[2] << ",\n";
        os << "      \"scale_x\": " << r.scale[0] << ",\n";
        os << "      \"scale_y\": " << r.scale[1] << ",\n";
        os << "      \"scale_z\": " << r.scale[2] << ",\n";
        os << "      \"color_r\": " << static_cast<uint32_t>(r.color[0]) << ",\n";
        os << "      \"color_g\": " << static_cast<uint32_t>(r.color[1]) << ",\n";
        os << "      \"color_b\": " << static_cast<uint32_t>(r.color[2]) << ",\n";
        os << "      \"color_a\": " << static_cast<uint32_t>(r.color[3]) << "\n";
        os << "    }" << (i + 1 < in.records.size() ? "," : "") << "\n";
    }
    os << "  ]\n";
    os << "}\n";
    return os.str();
}

bool Forest_ParseJSON(const std::string& text, ForestFile& out, std::string& err)
{
    out = ForestFile{};
    err.clear();

    JValue root;
    JsonReader reader(text);
    if (!reader.Parse(root))
        return Fail(err, reader.Error().c_str());
    if (root.type != JValue::Type::Object)
        return Fail(err, "top-level JSON value must be an object");

    if (!GetUint(root, "header_value", std::numeric_limits<uint32_t>::max(), out.headerValue, err))
        return false;
    const JValue* version = root.Find("version");
    if (!version || version->type != JValue::Type::String || version->str != kForestVersion)
        return Fail(err, "JSON field 'version' must be 'Ver. 3.0'");

    const JValue* records = root.Find("records");
    if (!records || records->type != JValue::Type::Array)
        return Fail(err, "JSON field 'records' must be an array");

    const JValue* declaredCount = root.Find("record_count");
    if (declaredCount) {
        uint32_t count = 0;
        if (!GetUint(root, "record_count", std::numeric_limits<uint32_t>::max(), count, err))
            return false;
        if (static_cast<size_t>(count) != records->array.size())
            return Fail(err, "record_count does not match records array length");
    }

    out.version = kForestVersion;
    out.records.reserve(records->array.size());
    for (const JValue& item : records->array) {
        ForestRecord rec;
        if (!GetFloat(item, "position_x", rec.position[0], err)
            || !GetFloat(item, "position_y", rec.position[1], err)
            || !GetFloat(item, "position_z", rec.position[2], err)
            || !GetFloat(item, "rotation_x", rec.rotation[0], err)
            || !GetFloat(item, "rotation_y", rec.rotation[1], err)
            || !GetFloat(item, "rotation_z", rec.rotation[2], err)
            || !GetFloat(item, "scale_x", rec.scale[0], err)
            || !GetFloat(item, "scale_y", rec.scale[1], err)
            || !GetFloat(item, "scale_z", rec.scale[2], err))
            return false;
        uint32_t c = 0;
        const char* colorKeys[4] = { "color_r", "color_g", "color_b", "color_a" };
        for (int k = 0; k < 4; ++k) {
            if (!GetUint(item, colorKeys[k], 255u, c, err))
                return false;
            rec.color[k] = static_cast<uint8_t>(c);
        }
        out.records.push_back(rec);
    }

    out.valid = true;
    return true;
}

std::string Forest_FormatReport(const ForestFile& in)
{
    std::ostringstream os;
    os << "IGI 2 DAT Forest\n";
    os << "  header value : " << in.headerValue << "\n";
    os << "  version      : " << in.version << "\n";
    os << "  records      : " << in.records.size() << "\n";
    os << "  file size    : " << (kForestHeaderSize + in.records.size() * kForestRecordSize) << " bytes\n";
    const size_t shown = in.records.size() < 5 ? in.records.size() : 5;
    for (size_t i = 0; i < shown; ++i) {
        const ForestRecord& r = in.records[i];
        os << "  [" << i << "] pos=(" << r.position[0] << ", " << r.position[1] << ", " << r.position[2] << ")"
           << " rot=(" << r.rotation[0] << ", " << r.rotation[1] << ", " << r.rotation[2] << ")"
           << " scale=(" << r.scale[0] << ", " << r.scale[1] << ", " << r.scale[2] << ")"
           << " rgb=(" << static_cast<int>(r.color[0]) << "," << static_cast<int>(r.color[1])
           << "," << static_cast<int>(r.color[2]) << ")\n";
    }
    if (shown < in.records.size())
        os << "  ... " << (in.records.size() - shown) << " more records\n";
    return os.str();
}

} // namespace igi2
