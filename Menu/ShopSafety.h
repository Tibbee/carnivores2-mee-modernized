#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

// A hunt setup reserves a budget; it does not permanently spend profile score.
using HuntBudget = int64_t;

inline HuntBudget RemainingHuntBudget(int32_t score, HuntBudget selectedCost)
{
    return static_cast<HuntBudget>(score) - selectedCost;
}

template <typename Row>
HuntBudget SelectedRowCost(const std::vector<std::pair<std::string, bool>>& selected,
                           const std::vector<Row>& rows)
{
    HuntBudget cost = 0;
    for (size_t i = 0; i < std::min(selected.size(), rows.size()); ++i)
        if (selected[i].second) cost += static_cast<HuntBudget>(rows[i].m_Price);
    return cost;
}

// Menu art is stored bottom-up. Clip source and destination together rather
// than allowing wide/tall mod pictures to wrap into other canvas rows.
inline void BlitMenuPicture(uint16_t* destination, int width, int height,
                            int x, int y, const uint16_t* source,
                            unsigned sourceWidth, unsigned sourceHeight)
{
    if (!destination || !source || width <= 0 || height <= 0 ||
        !sourceWidth || !sourceHeight) return;
    const int64_t left = std::max<int64_t>(0, x);
    const int64_t top = std::max<int64_t>(0, y);
    const int64_t right = std::min<int64_t>(width, static_cast<int64_t>(x) + sourceWidth);
    const int64_t bottom = std::min<int64_t>(height, static_cast<int64_t>(y) + sourceHeight);
    if (right <= left || bottom <= top) return;
    for (int64_t row = top; row < bottom; ++row) {
        const size_t srcRow = static_cast<size_t>(sourceHeight - 1 - (row - y));
        const size_t srcColumn = static_cast<size_t>(left - x);
        std::memcpy(destination + static_cast<size_t>(row) * width + static_cast<size_t>(left),
                    source + srcRow * sourceWidth + srcColumn,
                    static_cast<size_t>(right - left) * sizeof(uint16_t));
    }
}
