#ifndef C2_WAVE_READER_H
#define C2_WAVE_READER_H

// Shared game/menu RIFF reader. Raw CAR/RSC samples retain their legacy contract.
// Decode PCM8/PCM16 mono/stereo, then normalize to mono16/22050 so existing sound
// APIs and spatialization remain unchanged. Stock mono16/22050 PCM is bit-exact.
#include <cstdint>
#include <cstring>
#include <istream>
#include <vector>

namespace c2audio {
constexpr uint32_t kSampleRate = 22050;
constexpr uint32_t kMaxWaveBytes = 16u << 20;

inline uint16_t WaveU16(const unsigned char* p)
{
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}
inline uint32_t WaveU32(const unsigned char* p)
{
    return p[0] | (static_cast<uint32_t>(p[1]) << 8) |
        (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

inline bool ReadWave(std::istream& file, std::vector<short>& outputSamples, const char*& error)
{
    error = nullptr;
    const auto fail = [&](const char* reason) { error = reason; return false; };
    file.seekg(0, std::ios::end);
    const auto fileEnd = file.tellg();
    if (fileEnd < 12) return fail("truncated WAV header");
    const uint64_t fileSize = static_cast<uint64_t>(fileEnd);
    file.seekg(0);
    unsigned char header[12]{};
    if (!file.read(reinterpret_cast<char*>(header), 12) ||
        std::memcmp(header, "RIFF", 4) || std::memcmp(header + 8, "WAVE", 4))
        return fail("not a RIFF/WAVE file");
    const uint64_t riffEnd = 8ull + WaveU32(header + 4);
    if (riffEnd < 12 || riffEnd > fileSize) return fail("truncated RIFF payload");

    uint16_t channels = 0, bits = 0, align = 0;
    uint32_t rate = 0, dataBytes = 0;
    uint64_t dataOffset = 0;
    bool haveFormat = false, haveData = false;
    uint64_t offset = 12;
    unsigned chunks = 0;
    while (offset < riffEnd) {
        if (++chunks > 4096) return fail("too many WAV chunks");
        if (riffEnd - offset < 8) return fail("truncated WAV chunk header");
        file.seekg(static_cast<std::streamoff>(offset));
        unsigned char chunk[8]{};
        if (!file.read(reinterpret_cast<char*>(chunk), 8)) return fail("truncated WAV chunk header");
        const uint32_t size = WaveU32(chunk + 4);
        const uint64_t payload = offset + 8;
        const uint64_t next = payload + size + (size & 1u);
        if (next > riffEnd) return fail("truncated WAV chunk payload");
        if (!std::memcmp(chunk, "fmt ", 4)) {
            if (haveFormat || size < 16) return fail("invalid WAV format chunk");
            unsigned char fmt[16]{};
            if (!file.read(reinterpret_cast<char*>(fmt), 16)) return fail("truncated WAV format");
            channels = WaveU16(fmt + 2); rate = WaveU32(fmt + 4);
            align = WaveU16(fmt + 12); bits = WaveU16(fmt + 14);
            if (WaveU16(fmt) != 1 || (channels != 1 && channels != 2) ||
                (bits != 8 && bits != 16) || !rate || rate > 384000 ||
                align != channels * (bits / 8) ||
                WaveU32(fmt + 8) != static_cast<uint64_t>(rate) * align)
                return fail("unsupported WAV format (use PCM8/16 mono/stereo)");
            haveFormat = true;
        } else if (!std::memcmp(chunk, "data", 4)) {
            if (haveData || !size || size > kMaxWaveBytes) return fail("WAV data length out of range");
            dataOffset = payload; dataBytes = size; haveData = true;
        }
        offset = next;
    }
    if (!haveFormat || !haveData) return fail("WAV needs fmt and data chunks");
    if (dataBytes % align) return fail("WAV data has an incomplete sample frame");
    const uint64_t frames = dataBytes / align;
    const uint64_t outputFrames = (frames * kSampleRate + rate - 1) / rate;
    if (!outputFrames || outputFrames > kMaxWaveBytes / sizeof(short))
        return fail("normalized WAV data length out of range");

    std::vector<unsigned char> pcmBytes(dataBytes);
    file.seekg(static_cast<std::streamoff>(dataOffset));
    if (!file.read(reinterpret_cast<char*>(pcmBytes.data()), dataBytes)) return fail("truncated WAV data");
    const auto mono = [&](uint64_t frame) {
        int sum = 0;
        for (unsigned channel = 0; channel < channels; ++channel) {
            const auto* p = pcmBytes.data() + static_cast<size_t>(frame * align + channel * (bits / 8));
            if (bits == 8) sum += (static_cast<int>(*p) - 128) * 256;
            else {
                const int rawSample = WaveU16(p);
                sum += rawSample >= 32768 ? rawSample - 65536 : rawSample;
            }
        }
        return sum / channels;
    };
    std::vector<short> decoded(static_cast<size_t>(outputFrames));
    // C2 is x86/little-endian: the common stock format needs no conversion.
    static_assert(sizeof(short) == 2, "mono16 storage must be 16-bit");
    if (channels == 1 && bits == 16 && rate == kSampleRate) {
        std::memcpy(decoded.data(), pcmBytes.data(), dataBytes);
    } else for (uint64_t i = 0; i < outputFrames; ++i) {
        const uint64_t position = i * rate;
        const uint64_t first = position / kSampleRate;
        const uint64_t second = first + 1 < frames ? first + 1 : first;
        const int64_t fraction = position % kSampleRate;
        // Linear compatibility resampling, not a music/streaming-quality filter.
        const int64_t sample = mono(first) * (kSampleRate - fraction) + mono(second) * fraction;
        decoded[static_cast<size_t>(i)] = static_cast<short>(sample / kSampleRate);
    }
    outputSamples.swap(decoded); // Commit only a fully validated/decoded sound.
    return true;
}
} // namespace c2audio
#endif
