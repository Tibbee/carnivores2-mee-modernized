#include <gtest/gtest.h>
#include "Audio/WaveReader.h"
#include "wav_fixture.h"
#include <sstream>
using namespace wav_fixture;

namespace {
bool Decode(const std::string& bytes,std::vector<short>& samples) {
    std::istringstream file(bytes,std::ios::binary); const char* error=nullptr;
    return c2audio::ReadWave(file,samples,error);
}
}
TEST(WaveReader, StockPcm16IsBitExactIncludingExtremes) {
    std::vector<short> out;
    ASSERT_TRUE(Decode(Pcm16({-32768,-456,0,123,32767}),out));
    EXPECT_EQ(out,(std::vector<short>{-32768,-456,0,123,32767}));
}
TEST(WaveReader, ChunkOrderAndPaddingAreHandled) {
    // An unknown chunk's text must be skipped by length, not scanned for
    // markers; its odd size leaves a pad byte behind.
    std::string samples;U16(samples,123);U16(samples,456);
    std::vector<short> out;
    ASSERT_TRUE(Decode(Riff(Chunk("JUNK","data!")+Format()+Chunk("data",samples)),out));
    EXPECT_EQ(out,(std::vector<short>{123,456}));

    // The data chunk may precede the format chunk.
    std::string leading;U16(leading,123);std::vector<short> ordered;
    ASSERT_TRUE(Decode(Riff(Chunk("data",leading)+Format()),ordered));EXPECT_EQ(ordered[0],123);
}
TEST(WaveReader, Converts8BitStereoAndSampleRates) {
    std::vector<short> out;
    ASSERT_TRUE(Decode(Riff(Format(1,8)+Chunk("data",std::string("\0\x80\xff",3))),out));
    EXPECT_EQ(out,(std::vector<short>{-32768,0,32512}));
    ASSERT_TRUE(Decode(Pcm16({-32768,-32768,32767,32767,1000,-1000},2),out));
    EXPECT_EQ(out,(std::vector<short>{-32768,32767,0}));

    ASSERT_TRUE(Decode(Pcm16({0,100,200,300},1,44100),out));
    EXPECT_EQ(out,(std::vector<short>{0,200}));
    ASSERT_TRUE(Decode(Pcm16({0,200},1,11025),out));
    EXPECT_EQ(out,(std::vector<short>{0,100,200,200}));
}
TEST(WaveReader, EveryTruncationFailsWithoutChangingTheDestination) {
    const auto complete=Pcm16({123,-456,789});
    for(size_t n=0;n<complete.size();++n) {
        std::vector<short> out{42};
        EXPECT_FALSE(Decode(complete.substr(0,n),out))<<n;
        EXPECT_EQ(out,(std::vector<short>{42}));
    }
}
TEST(WaveReader, RejectsMalformedChunkStructuresAndFormats) {
    std::vector<short> out;
    EXPECT_FALSE(Decode(Riff(Format()),out));
    EXPECT_FALSE(Decode(Riff(Chunk("data","xx")),out));
    EXPECT_FALSE(Decode(Riff(Format()+Format()+Chunk("data","xx")),out));
    EXPECT_FALSE(Decode(Riff(Format()+Chunk("data","")),out));
    EXPECT_FALSE(Decode(Riff(Format()+Chunk("data","xx")+Chunk("data","xx")),out));
    auto oversized=Pcm16({123});SetU32(oversized,40,0xffffffffu);
    EXPECT_FALSE(Decode(oversized,out));

    EXPECT_FALSE(Decode(Riff(Format()+Chunk("data","x")),out));
    EXPECT_FALSE(Decode(Riff(Format(2)+Chunk("data","xx")),out));
    auto unsupported=Pcm16({123});unsupported[20]=3;EXPECT_FALSE(Decode(unsupported,out));
    auto badAlign=Pcm16({123});badAlign[32]=4;EXPECT_FALSE(Decode(badAlign,out));
    auto zeroRate=Pcm16({123});SetU32(zeroRate,24,0);EXPECT_FALSE(Decode(zeroRate,out));
}
TEST(WaveReader, OutputExpansionAndChunkCountAreBounded) {
    std::vector<short> out;
    std::string data(1024,0);
    EXPECT_FALSE(Decode(Riff(Format(1,16,1)+Chunk("data",data)),out));
    std::string many;for(int i=0;i<4097;++i)many+=Chunk("JUNK","");
    EXPECT_FALSE(Decode(Riff(many+Format()+Chunk("data","xx")),out));
}
