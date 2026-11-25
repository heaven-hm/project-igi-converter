/******************************************************************************
 * @file    igi2_forest_parser.h
 * @brief   IGI 2 DAT Forest reader/writer (forest_<task_id>.dat)
 *
 * Format (little-endian, from Artiom Rotari's igipy docs/igi2/formats/dat_forest.md):
 *   uint32 (10) + 0x0A + "Ver. 3.0" + 0x0A        -> 14-byte header
 *   repeated 40-byte records:
 *     float[3] position, float[3] rotation, float[3] scale,
 *     uint8 color[4] (r,g,b,a; a unused)
 *
 * The writer is byte-exact for any file accepted by Forest_ParseBytes.
 *****************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace igi2 {

constexpr uint32_t kForestHeaderValue = 10u;
constexpr size_t kForestHeaderSize = 14u;
constexpr size_t kForestRecordSize = 40u;
constexpr char kForestVersion[] = "Ver. 3.0";

struct ForestRecord {
    float position[3] = { 0.0f, 0.0f, 0.0f };
    float rotation[3] = { 0.0f, 0.0f, 0.0f };
    float scale[3] = { 1.0f, 1.0f, 1.0f };
    uint8_t color[4] = { 0, 0, 0, 0 };
};

struct ForestFile {
    uint32_t headerValue = kForestHeaderValue;
    std::string version = kForestVersion;
    std::vector<ForestRecord> records;
    bool valid = false;
    std::string error;
};

// Strict binary reader. Returns false with a human-readable reason in err.
bool Forest_ParseBytes(const void* data, size_t size, ForestFile& out, std::string& err);
bool Forest_ParseFile(const std::string& path, ForestFile& out, std::string& err);

// Native writer. Serializes header + 40-byte records into outBytes.
bool Forest_Serialize(const ForestFile& in, std::vector<uint8_t>& outBytes, std::string& err);
bool Forest_WriteFile(const std::string& path, const ForestFile& in, std::string& err);

// JSON export/import using the igipy-shaped schema:
//   { "header_value": 10, "version": "Ver. 3.0",
//     "record_count": N,
//     "records": [ { "position_x": ..., "color_r": ..., } ... ] }
std::string Forest_FormatJSON(const ForestFile& in);
bool Forest_ParseJSON(const std::string& text, ForestFile& out, std::string& err);

// Human-readable one-screen report for `forest info`.
std::string Forest_FormatReport(const ForestFile& in);

} // namespace igi2
