#ifndef C2_TEST_WAV_FIXTURE_H
#define C2_TEST_WAV_FIXTURE_H
#include <cstdint>
#include <string>
#include <vector>
namespace wav_fixture {
inline void U16(std::string& s, uint16_t n) {
    s.push_back(static_cast<char>(n)); s.push_back(static_cast<char>(n >> 8));
}
inline void U32(std::string& s, uint32_t n) {
    for (int i = 0; i < 4; ++i) s.push_back(static_cast<char>(n >> (i * 8)));
}
inline void SetU32(std::string& s, size_t at, uint32_t n) {
    for (int i = 0; i < 4; ++i) s[at + i] = static_cast<char>(n >> (i * 8));
}
inline std::string Chunk(const char* id, const std::string& bytes) {
    std::string s(id, 4); U32(s, static_cast<uint32_t>(bytes.size())); s += bytes;
    if (bytes.size() & 1) s.push_back(0);
    return s;
}
inline std::string Riff(const std::string& chunks) {
    std::string s = "RIFF"; U32(s, static_cast<uint32_t>(chunks.size() + 4));
    return s + "WAVE" + chunks;
}
inline std::string Format(uint16_t channels=1, uint16_t bits=16, uint32_t rate=22050) {
    std::string s; U16(s, 1); U16(s, channels); U32(s, rate);
    U32(s, rate * channels * (bits / 8)); U16(s, channels * (bits / 8)); U16(s, bits);
    return Chunk("fmt ", s);
}
inline std::string Pcm16(std::initializer_list<short> samples, uint16_t channels=1, uint32_t rate=22050) {
    std::string payload; for (short sample : samples) U16(payload, static_cast<uint16_t>(sample));
    return Riff(Format(channels, 16, rate) + Chunk("data", payload));
}
} // namespace wav_fixture
#endif
