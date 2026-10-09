#include <algorithm>
#include <cstring>

#include "asset_json.hpp"
#include "wav_adpcm.h"

namespace gconv
{
namespace
{
struct PcmAudio
{
    uint16_t channels;
    uint32_t rate;
    Bytes samples;
};

PcmAudio readPcm(const fs::path &path)
{
    const auto bytes = readFile(path);
    requireRange(bytes, 0, 12);
    if (!hasTag(bytes, 0, "RIFF") || !hasTag(bytes, 8, "WAVE"))
        fail("Expected standard RIFF/WAVE audio");
    PcmAudio audio{};
    bool formatSeen = false, dataSeen = false;
    size_t position = 12;
    while (position < bytes.size())
    {
        requireRange(bytes, position, 8);
        const size_t length = readU32(bytes, position + 4);
        const size_t start = position + 8;
        requireRange(bytes, start, length);
        if (hasTag(bytes, position, "fmt "))
        {
            requireRange(bytes, start, 16);
            if (length < 16 || readU16(bytes, start) != 1 || readU16(bytes, start + 14) != 16)
                fail("WAV packing requires signed 16-bit PCM");
            audio.channels = readU16(bytes, start + 2);
            audio.rate = readU32(bytes, start + 4);
            if ((audio.channels != 1 && audio.channels != 2) || !audio.rate ||
                readU16(bytes, start + 12) != audio.channels * 2)
                fail("Invalid PCM channel count, rate or block alignment");
            formatSeen = true;
        }
        else if (hasTag(bytes, position, "data"))
        {
            if (dataSeen)
                fail("Multiple WAV data chunks are unsupported");
            audio.samples.assign(bytes.begin() + start, bytes.begin() + start + length);
            dataSeen = true;
        }
        position = start + length + (length & 1);
        if (position > bytes.size())
            fail("Truncated WAV chunk padding");
    }
    if (!formatSeen || !dataSeen || audio.samples.size() % (audio.channels * 2))
        fail("Missing or incomplete PCM audio");
    return audio;
}

Bytes standardWav(const PcmAudio &audio)
{
    Bytes bytes(44);
    std::memcpy(bytes.data(), "RIFF", 4);
    writeUnsigned(bytes, 4, static_cast<uint32_t>(36 + audio.samples.size()));
    std::memcpy(bytes.data() + 8, "WAVEfmt ", 8);
    writeUnsigned(bytes, 16, 16);
    writeUnsigned(bytes, 20, 1, 2);
    writeUnsigned(bytes, 22, audio.channels, 2);
    writeUnsigned(bytes, 24, audio.rate);
    writeUnsigned(bytes, 28, audio.rate * audio.channels * 2);
    writeUnsigned(bytes, 32, audio.channels * 2, 2);
    writeUnsigned(bytes, 34, 16, 2);
    std::memcpy(bytes.data() + 36, "data", 4);
    writeUnsigned(bytes, 40, static_cast<uint32_t>(audio.samples.size()));
    bytes.insert(bytes.end(), audio.samples.begin(), audio.samples.end());
    return bytes;
}

PcmAudio decodeIlsf(const Bytes &bytes)
{
    requireRange(bytes, 0, 20);
    if (!hasTag(bytes, 0, "ILSF") || readU16(bytes, 6) != 16)
        fail("Expected 16-bit InnerLoop ILSF audio");
    const auto method = readU16(bytes, 4);
    PcmAudio audio{readU16(bytes, 8), readU32(bytes, 12), {}};
    if ((audio.channels != 1 && audio.channels != 2) || !audio.rate || method > 3)
        fail("Unsupported ILSF method, channels or sample rate");
    const uint64_t samples = uint64_t(readU32(bytes, 16)) * audio.channels;
    if (samples > 250000000)
        fail("Oversized ILSF audio");
    if (method < 2)
    {
        requireRange(bytes, 20, static_cast<size_t>(samples) * 2);
        audio.samples.assign(bytes.begin() + 20, bytes.begin() + 20 + samples * 2);
    }
    else
    {
        requireRange(bytes, 20, static_cast<size_t>((samples + 1) / 2));
        const Bytes encoded(bytes.begin() + 20, bytes.end());
        const auto decoded = igi1conv::wav::decode_ima_adpcm(encoded, samples, 0);
        if (decoded.size() != samples)
            fail("ADPCM sample count mismatch");
        audio.samples.resize(decoded.size() * 2);
        for (size_t i = 0; i < decoded.size(); ++i)
            writeUnsigned(audio.samples, i * 2, static_cast<uint16_t>(decoded[i]), 2);
    }
    return audio;
}

Bytes encodeAdpcm(const Bytes &pcm)
{
    const auto *steps = igi1conv::wav::ima_step_table();
    const auto *indices = igi1conv::wav::ima_index_table();
    const size_t count = pcm.size() / 2;
    Bytes result((count + 1) / 2);
    int predictor = 0, index = 0;
    for (size_t i = 0; i < count; ++i)
    {
        const int sample = static_cast<int16_t>(readU16(pcm, i * 2));
        int difference = sample - predictor;
        uint8_t code = difference < 0 ? 8 : 0;
        if (difference < 0)
            difference = -difference;
        const int step = steps[index];
        int delta = step >> 3;
        if (difference >= step)
        {
            code |= 4;
            difference -= step;
            delta += step;
        }
        if (difference >= (step >> 1))
        {
            code |= 2;
            difference -= step >> 1;
            delta += step >> 1;
        }
        if (difference >= (step >> 2))
        {
            code |= 1;
            delta += step >> 2;
        }
        predictor = std::clamp(predictor + ((code & 8) ? -delta : delta), -32768, 32767);
        index = std::clamp(index + indices[code], 0, 88);
        result[i / 2] |= code << (i % 2 == 0 ? 4 : 0);
    }
    return result;
}

void packAudio(const fs::path &input, const fs::path &pcmPath, const fs::path &output)
{
    const auto original = readFile(input);
    decodeIlsf(original); // Validate the complete template before creating output.
    const auto audio = readPcm(pcmPath);
    requireDifferentFiles(input, output, "Template and output must differ");
    requireDifferentFiles(pcmPath, output, "PCM input and output must differ");
    if (audio.channels != readU16(original, 8) || audio.rate != readU32(original, 12))
        fail("PCM channels and sample rate must match the template");
    Bytes bytes(original.begin(), original.begin() + 20);
    writeUnsigned(bytes, 16, static_cast<uint32_t>(audio.samples.size() / (audio.channels * 2)));
    const auto payload = readU16(original, 4) < 2 ? audio.samples : encodeAdpcm(audio.samples);
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    writeFile(output, bytes);
}

size_t mp3FrameSize(const Bytes &bytes, size_t position)
{
    if (position > bytes.size() || bytes.size() - position < 4 || bytes[position] != 0xff ||
        (bytes[position + 1] & 0xe0) != 0xe0)
        return 0;
    const auto version = (bytes[position + 1] >> 3) & 3;
    const auto layer = (bytes[position + 1] >> 1) & 3;
    const auto bitrate = (bytes[position + 2] >> 4) & 15;
    const auto frequency = (bytes[position + 2] >> 2) & 3;
    const auto padding = (bytes[position + 2] >> 1) & 1;
    if (version == 1 || !layer || !bitrate || bitrate == 15 || frequency == 3)
        return 0;
    constexpr int rates[] = {44100, 48000, 32000};
    constexpr int bitrates[5][16] = {
        {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0},
        {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
        {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0},
        {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0},
        {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}};
    const int rate = rates[frequency] / (version == 3 ? 1 : version == 2 ? 2 : 4);
    const int row = version == 3 ? 3 - layer : layer == 3 ? 3 : 4;
    const int bits = bitrates[row][bitrate] * 1000;
    if (layer == 3)
        return (12 * bits / rate + padding) * 4;
    return (layer == 1 && version != 3 ? 72 : 144) * bits / rate + padding;
}

void normalizeMp3(const fs::path &input, const fs::path &output)
{
    requireDifferentFiles(input, output, "MP3 input and output must differ");
    const auto bytes = readFile(input);
    size_t start = 0;
    if (bytes.size() >= 10 && std::memcmp(bytes.data(), "ID3", 3) == 0)
    {
        if ((bytes[6] | bytes[7] | bytes[8] | bytes[9]) & 0x80)
            fail("Invalid ID3 length");
        start = 10 + (size_t(bytes[6]) << 21) + (size_t(bytes[7]) << 14) + (size_t(bytes[8]) << 7) +
                bytes[9] + ((bytes[5] & 0x10) ? 10 : 0);
    }
    requireRange(bytes, start, 4);
    size_t position = start;
    while (position < bytes.size())
    {
        const size_t length = mp3FrameSize(bytes, position);
        if (!length || length > bytes.size() - position)
            break;
        position += length;
    }
    if (position == start)
        fail("No complete MP3 audio frames found");
    // Metadata after the final complete MPEG frame is intentionally removed.
    writeFile(output, Bytes(bytes.begin() + start, bytes.begin() + position));
}
} // namespace

int runAudioCommand(int argc, char **argv)
{
    if (argc < 3)
        return -1;
    const std::string family = argv[1], operation = argv[2];
    if (family == "wav" && operation == "extract" && argc == 5)
    {
        requireDifferentFiles(argv[3], argv[4], "Audio input and output must differ");
        writeFile(argv[4], standardWav(decodeIlsf(readFile(argv[3]))));
        return 0;
    }
    if (family == "wav" && operation == "pack" && argc == 6)
    {
        packAudio(argv[3], argv[4], argv[5]);
        return 0;
    }
    if (family == "mp3" && operation == "normalize" && argc == 5)
    {
        normalizeMp3(argv[3], argv[4]);
        return 0;
    }
    return -1;
}
} // namespace gconv
