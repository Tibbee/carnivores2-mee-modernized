#include <gtest/gtest.h>
#include "../Menu/ShopSafety.h"
#include <array>
#include <limits>

TEST(ShopSafety, BlitPreservesBottomUpStockOrientation)
{
    std::array<uint16_t, 20> canvas{};
    const uint16_t source[] = {1, 2, 3, 4};
    BlitMenuPicture(canvas.data(), 5, 4, 1, 1, source, 2, 2);
    EXPECT_EQ(canvas[6], 3); EXPECT_EQ(canvas[7], 4);
    EXPECT_EQ(canvas[11], 1); EXPECT_EQ(canvas[12], 2);
    EXPECT_EQ(canvas[0], 0); EXPECT_EQ(canvas[19], 0);
}

TEST(ShopSafety, BlitClipsAllFourEdgesAndPreservesGuards)
{
    const uint16_t source[] = {1, 2, 3, 4};
    for (const auto position : {std::pair<int,int>{-1,-1}, {4,3}, {5,4}, {-10,-10}}) {
        std::array<uint16_t, 28> storage;
        storage.fill(0x1234);
        std::fill(storage.begin() + 4, storage.end() - 4, 0);
        BlitMenuPicture(storage.data() + 4, 5, 4, position.first, position.second, source, 2, 2);
        for (size_t i : {size_t(0), size_t(1), size_t(2), size_t(3), size_t(24), size_t(25), size_t(26), size_t(27)})
            EXPECT_EQ(storage[i], 0x1234);
        if (position.first == -1) EXPECT_EQ(storage[4], 2);
        if (position.first == 4) EXPECT_EQ(storage[23], 3);
    }
}

TEST(ShopSafety, LargePictureClipsToTheRealMenuCanvas)
{
    std::vector<uint16_t> storage(800 * 600 + 16, 0x1234);
    std::vector<uint16_t> source(741 * 600, 0xabcd);
    BlitMenuPicture(storage.data(), 800, 600, 38, 73, source.data(), 741, 600);
    EXPECT_EQ(storage[73 * 800 + 38], 0xabcd);
    EXPECT_EQ(storage[599 * 800 + 778], 0xabcd);
    for (size_t i = 800 * 600; i < storage.size(); ++i) EXPECT_EQ(storage[i], 0x1234);
}

TEST(ShopSafety, OffCanvasAndNullPicturesDoNotWrite)
{
    std::array<uint16_t, 4> canvas{};
    const uint16_t source[] = {1};
    BlitMenuPicture(canvas.data(), 2, 2, std::numeric_limits<int>::min(), 0, source, 1, 1);
    BlitMenuPicture(canvas.data(), 2, 2, std::numeric_limits<int>::max(), 0, source, 1, 1);
    BlitMenuPicture(canvas.data(), 2, 2, 0, 0, nullptr, 1, 1);
    BlitMenuPicture(nullptr, 2, 2, 0, 0, source, 1, 1);
    for (auto pixel : canvas) EXPECT_EQ(pixel, 0);
}

TEST(ShopSafety, SelectedPricesAndRemainingBudgetUseWideArithmetic)
{
    struct Row { int32_t m_Price; };
    const std::vector<Row> rows = {{1500000000}, {1500000000}, {30}};
    std::vector<std::pair<std::string,bool>> selected = {{"camo",true}, {"radar",true}, {"lamp",false}};
    EXPECT_EQ(SelectedRowCost(selected, rows), 3000000000LL);
    EXPECT_EQ(RemainingHuntBudget(100, SelectedRowCost(selected, rows)), -2999999900LL);
    EXPECT_GT(SelectedRowCost(selected, rows), static_cast<HuntBudget>(100));
    selected[0].second = selected[1].second = false;
    selected[2].second = true;
    EXPECT_EQ(SelectedRowCost(selected, rows), 30);
    EXPECT_EQ(RemainingHuntBudget(100, SelectedRowCost(selected, rows)), 70);
    // A list/resource mismatch must not index past the resource table.
    selected.emplace_back("missing", true);
    EXPECT_EQ(SelectedRowCost(selected, rows), 30);
}
