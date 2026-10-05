#include <gtest/gtest.h>
#include "Audio/AudioMath.h"
#include <limits>

TEST(AudioMath, ForwardFootstepsRemainExactAndBackwardAreSymmetric) {
    for(float speed : {0.0f,0.1f,0.3f,0.7f,8.0f}) for(bool water : {false,true}) {
        const int old = (water?64:24) + static_cast<int>(speed*(water?30.0f:50.0f));
        EXPECT_EQ(FootstepVolume(speed,water),old);
        EXPECT_EQ(FootstepVolume(-speed,water),old);
    }
    EXPECT_EQ(FootstepVolume(-0.7f,false),59);
}
TEST(AudioMath, InvalidSpeedsAndNegativeGainAreSafe) {
    EXPECT_EQ(FootstepVolume(std::numeric_limits<float>::infinity(),false),24);
    EXPECT_EQ(FootstepVolume(std::numeric_limits<float>::quiet_NaN(),true),64);
    EXPECT_EQ(FootstepVolume(std::numeric_limits<float>::max(),false),424);
    EXPECT_FLOAT_EQ(AudioGain(-11),0);
    EXPECT_FLOAT_EQ(AudioGain(256),1);
    EXPECT_FLOAT_EQ(AudioGain(512),2); // Do not reset authored above-unity volume.
}
TEST(AudioMath, ListenerBasisMatchesAimAndIsOrthonormal) {
    for(float a : {-3.1f,-0.4f,0.0f,1.3f,3.1f}) for(float b : {-1.4f,-0.5f,0.0f,0.5f,1.4f}) {
        float o[6]; AudioListenerOrientation(a,b,o);
        EXPECT_FLOAT_EQ(o[0],std::sin(a)*std::cos(b));
        EXPECT_FLOAT_EQ(o[1],-std::sin(b));
        EXPECT_FLOAT_EQ(o[2],-std::cos(a)*std::cos(b));
        float dot=0,f=0,u=0;
        for(int i=0;i<3;++i) {dot+=o[i]*o[i+3];f+=o[i]*o[i];u+=o[i+3]*o[i+3];}
        EXPECT_NEAR(dot,0,0.000001);EXPECT_NEAR(f,1,0.000001);EXPECT_NEAR(u,1,0.000001);
    }
}
