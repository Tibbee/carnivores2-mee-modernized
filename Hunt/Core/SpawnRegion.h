#pragma once

#include <cstdint>
#include <limits>

// rRand takes an inclusive signed maximum: rand() % (maximum + 1).
// Reversed authored bounds can work with that legacy formula, including the
// shipped area7 trap region. Preserve their result and RNG draw; only recover
// a zero divisor or arithmetic that cannot be represented by the old int path.
inline int SpawnRegionRandomMaximum(int minimum, int maximum)
{
    const int64_t span = static_cast<int64_t>(maximum) - minimum;
    if (span == -1 || span < (std::numeric_limits<int>::min)() ||
        span >= (std::numeric_limits<int>::max)())
        return 0;
    return static_cast<int>(span);
}
