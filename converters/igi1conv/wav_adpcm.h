// igi1conv/wav_adpcm.h - IMA ADPCM decoder for IGI's ILSF audio.
//
// The Python dconv reference at D:\IGI-Tools\GM_123\IGI MEF CONV\tools\dconv
// \format\wav.py contains a `_decode_adpcm(sounddata, width, step=None)`
// helper that delegates to `audioop.adpcm2lin(sounddata, 2, None)` - the
// standard Python IMA/Intel/DVI ADPCM decoder.  The intended call is
// `audioop.adpcm2lin(data, 2, None)` which starts the decoder with state
// (predictor=0, step_index=0) and processes the input as a pure 4-bit
// nibble stream (2 samples per byte, high nibble first, then low nibble).
//
// We mirror that contract here in C++ so the same IGI files decode the
// same way in igi1conv as they would have in the Python dconv tool.
//
// See https://wiki.multimedia.cx/index.php?title=IMA_ADPCM for the format
// reference and the standard step / index tables reproduced below.
#pragma once
#include <cstdint>
#include <vector>

namespace igi1conv::wav {

// IMA ADPCM step table (89 entries).  Index 0 is used for the initial
// (smallest) step size; index 88 is the largest (32767).
inline const int* ima_step_table() {
    static const int kTable[89] = {
        7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
        19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
        50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
        130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
        337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
        876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
        2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
        5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
        15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
    };
    return kTable;
}

// IMA ADPCM step-index adjustment table (16 entries, indexed by 4-bit
// code 0..15).  Negative = shrink, positive = grow.
inline const int* ima_index_table() {
    static const int kTable[16] = {
        -1, -1, -1, -1, 2, 4, 6, 8,
        -1, -1, -1, -1, 2, 4, 6, 8
    };
    return kTable;
}

// Decode a 4-bit IMA ADPCM nibble stream to 16-bit signed LE PCM samples.
// `data` is the raw ADPCM byte payload (2 samples per byte); the decoder
// reads every byte as 2 nibbles (high then low) and produces 16-bit LE
// PCM samples.  The decoder state starts at (predictor=0, step_index=0),
// matching the `audioop.adpcm2lin(data, 2, None)` call from the Python
// dconv reference.
//
// `skip_warmup_samples` is the number of leading decoded samples to
// DISCARD before returning.  IGI's ILSF ADPCM files start with a
// 4-byte payload whose first nibbles are 0xF / 0xF / 0x7 / 0xF, which
// with state (0, 0) produces ~8 extreme samples (clipping at the
// negative rail) before the adaptive step_size pulls the predictor
// back into range.  Dropping the first 16 samples (~0.7 ms at 22 kHz)
// eliminates that initial transient and makes the audio noticeably
// less "muffled" / click-prone.  Pass 0 to disable.
//
// If `max_samples` is non-zero the decoder stops once that many samples
// have been produced (useful when the encoded data is exactly
// `ceil(n_samples/2)` bytes and the file has a trailing pad byte).
inline std::vector<int16_t> decode_ima_adpcm(const std::vector<uint8_t>& data,
                                              size_t max_samples,
                                              size_t skip_warmup_samples)
{
    const int* step = ima_step_table();
    const int* ix   = ima_index_table();

    std::vector<int16_t> out;
    out.reserve(data.size() * 2);

    int pred = 0;
    int si   = 0;
    size_t dropped = 0;
    bool dropping = skip_warmup_samples > 0;

    for (uint8_t byte : data) {
        for (int nib = 0; nib < 2; ++nib) {
            int code = (nib == 0) ? ((byte >> 4) & 0xF) : (byte & 0xF);

            int diff = step[si] >> 3;
            if (code & 4) diff += step[si];
            if (code & 2) diff += step[si] >> 1;
            if (code & 1) diff += step[si] >> 2;
            if (code & 8) diff = -diff;

            pred += diff;
            if (pred >  32767) pred =  32767;
            if (pred < -32768) pred = -32768;

            si += ix[code];
            if (si <  0) si = 0;
            if (si > 88) si = 88;

            if (dropping) {
                if (++dropped >= skip_warmup_samples) dropping = false;
            } else {
                out.push_back(static_cast<int16_t>(pred));
                if (max_samples && out.size() >= max_samples) return out;
            }
        }
    }
    return out;
}

// Decode a stereo 4-bit ADPCM stream where the two channels are
// interleaved at the nibble level: each input byte carries 1 nibble
// from channel 0 (high) and 1 nibble from channel 1 (low).  Returns
// `frame_count * 2` interleaved 16-bit LE PCM samples ready to feed
// directly to a standard PCM WAV writer.
//
// This matches what the Python dconv `audioop.adpcm2lin` call would
// produce on a stereo stream, and what real IGI ADPCM stereo files
// (e.g. m13_ambience.wav) contain - the two channels have very
// similar energy in the decoded output, which is exactly what you
// expect from an ambient recording.
inline std::vector<int16_t> decode_ima_adpcm_stereo_interleaved(
    const std::vector<uint8_t>& data, size_t frame_count)
{
    auto mono = decode_ima_adpcm(data, frame_count * 2, 0);
    // We don't need to de-interleave here: the mono decode already
    // produces alternating ch0 / ch1 / ch0 / ch1 ... samples in
    // stream order.  Return as-is; the WAV writer can treat the
    // resulting PCM as a 2-channel stream of (L, R, L, R, ...).
    return mono;
}

}  // namespace igi1conv::wav
