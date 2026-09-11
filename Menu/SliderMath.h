// SliderMath.h -- dependency-free menu slider helpers
#pragma once

inline int DiscreteSliderValue(float position, int minimum, int maximum, int step) noexcept
{
    const int lastIndex = (maximum - minimum) / step;
    int index = static_cast<int>(position * static_cast<float>(lastIndex) + 0.5f);
    if (index < 0) index = 0;
    if (index > lastIndex) index = lastIndex;
    return minimum + index * step;
}
