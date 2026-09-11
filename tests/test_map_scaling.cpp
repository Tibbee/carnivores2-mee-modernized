// test_map_scaling.cpp -- Tests for map-scaling algorithms and HUD stale-region clearing.
//
// These are C2-specific adaptations of the C1 test_map_scaling.cpp.  C2's
// DrawHMap doesn't use a scaling cache (it draws MapPic at native size),
// but the HUD system has a similar stale-region problem in
// ClearStaleHUDRegions.  The generic scaling algorithms are tested here
// because they may be useful in future map optimisations.

#include <gtest/gtest.h>
#include <vector>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <chrono>

// ============================================================================
// Generic scaling algorithms (same as C1 version)
// ============================================================================
namespace {

// Old approach: per-pixel scaling loop with divisions and 0-skip
void old_scale_map(const std::vector<uint16_t>& src, int srcW, int srcH,
                   std::vector<uint16_t>& dst, int dstW, int dstH)
{
    for (int yy = 0; yy < dstH; yy++) {
        int sy = yy * srcH / dstH;
        for (int xx = 0; xx < dstW; xx++) {
            int sx = xx * srcW / dstW;
            uint16_t c = src[sy * srcW + sx];
            if (c != 0) {
                dst[yy * dstW + xx] = c;
            }
        }
    }
}

// New approach: pre-scale once, then memcpy
void new_scale_map(const std::vector<uint16_t>& src, int srcW, int srcH,
                   std::vector<uint16_t>& dstCache, int dstW, int dstH)
{
    dstCache.assign(static_cast<size_t>(dstW) * dstH, 0);
    for (int yy = 0; yy < dstH; yy++) {
        int sy = yy * srcH / dstH;
        for (int xx = 0; xx < dstW; xx++) {
            int sx = xx * srcW / dstW;
            dstCache[static_cast<size_t>(yy) * dstW + xx] =
                src[static_cast<size_t>(sy) * srcW + sx];
        }
    }
}

template <typename Fn>
double time_it(Fn&& fn, int iters = 60)
{
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; i++) fn();
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Scaling algorithm correctness & performance
// ---------------------------------------------------------------------------

TEST(MapScaling, OldVsNewAtTypicalResolution)
{
    constexpr int kSrcW = 400, kSrcH = 400;
    constexpr int kDstW = 1080, kDstH = 1080;
    constexpr int kIters = 30;

    std::vector<uint16_t> src(static_cast<size_t>(kSrcW) * kSrcH);
    for (size_t i = 0; i < src.size(); i++) {
        src[i] = (i & 1) ? 0 : static_cast<uint16_t>(0x7C00 | (i & 0x1F));
    }

    std::vector<uint16_t> oldDst(static_cast<size_t>(kDstW) * kDstH);
    std::vector<uint16_t> newCache;

    // Warmup
    old_scale_map(src, kSrcW, kSrcH, oldDst, kDstW, kDstH);
    new_scale_map(src, kSrcW, kSrcH, newCache, kDstW, kDstH);

    double oldMs = time_it([&] {
        old_scale_map(src, kSrcW, kSrcH, oldDst, kDstW, kDstH);
    }, kIters);

    double newSetupMs = time_it([&] {
        new_scale_map(src, kSrcW, kSrcH, newCache, kDstW, kDstH);
    }, kIters / 10);

    std::vector<uint16_t> dstBuffer(static_cast<size_t>(kDstW) * kDstH);
    double newSteadyMs = time_it([&] {
        std::memcpy(dstBuffer.data(), newCache.data(),
                    newCache.size() * sizeof(uint16_t));
    }, kIters * 10);

    RecordProperty("old_per_frame_ms", oldMs);
    RecordProperty("new_first_frame_ms", newSetupMs);
    RecordProperty("new_steady_state_ms", newSteadyMs);
    RecordProperty("speedup_steady_state", oldMs / newSteadyMs);

    EXPECT_LT(newSteadyMs, oldMs);
}

TEST(MapScaling, ResultIsIdentical)
{
    constexpr int kSrcW = 400, kSrcH = 400;
    constexpr int kDstW = 800, kDstH = 800;

    std::vector<uint16_t> src(static_cast<size_t>(kSrcW) * kSrcH);
    for (size_t i = 0; i < src.size(); i++) {
        src[i] = static_cast<uint16_t>((i * 31) & 0x7FFF);
    }

    std::vector<uint16_t> oldDst(static_cast<size_t>(kDstW) * kDstH, 0xCAFE);
    std::vector<uint16_t> newCache;

    old_scale_map(src, kSrcW, kSrcH, oldDst, kDstW, kDstH);
    new_scale_map(src, kSrcW, kSrcH, newCache, kDstW, kDstH);

    // Old skips 0-pixels (leaves 0xCAFE sentinel), new writes 0 for those.
    // Verify: where old wrote non-sentinel, new agrees; where new wrote
    // non-zero, old agrees.
    for (int i = 0; i < kDstW * kDstH; i++) {
        if (oldDst[i] != 0xCAFE) {
            EXPECT_EQ(newCache[i], oldDst[i])
                << "mismatch at pixel " << i;
        }
        if (newCache[i] != 0) {
            EXPECT_EQ(oldDst[i], newCache[i])
                << "mismatch at pixel " << i;
        }
    }
}

// ---------------------------------------------------------------------------
// HUD stale-region clearing (adapted for C2's ClearStaleHUDRegions pattern)
// ---------------------------------------------------------------------------
//
// C2's HUD overlay uses a dirty-rect system.  Each frame:
//   1. HUD elements draw into lpVideoBuf (a 16-bit 1555 buffer).
//   2. The previous frame's dirty rects are cleared to zero.
//   3. Both old and new rects are uploaded to the GPU texture.
//
// These tests validate the clearing logic independently of
// the full rendering pipeline.

TEST(MapScaling, ClearStaleRegionLeavesTransparent)
{
    // Simulate a 16-bit lpVideoBuf at 800x600 (typical menu/game res).
    constexpr int kWinW = 800, kWinH = 600;
    constexpr int kVideoPitch = kWinW;

    // Fill buffer with non-zero sentinel (simulates scene content).
    std::vector<uint16_t> lpVideoBuf(static_cast<size_t>(kVideoPitch) * kWinH, 0xBEEF);

    // Simulate drawing a HUD element: a 200x100 box at (100, 50).
    constexpr int kRectX = 100, kRectY = 50, kRectW = 200, kRectH = 100;
    for (int yy = kRectY; yy < kRectY + kRectH; yy++) {
        for (int xx = kRectX; xx < kRectX + kRectW; xx++) {
            lpVideoBuf[static_cast<size_t>(yy) * kVideoPitch + xx] = 0x7C00; // red
        }
    }

    // Verify the rect contains non-zero content.
    int nonZero = 0;
    for (int yy = kRectY; yy < kRectY + kRectH; yy++) {
        for (int xx = kRectX; xx < kRectX + kRectW; xx++) {
            if (lpVideoBuf[static_cast<size_t>(yy) * kVideoPitch + xx] != 0)
                nonZero++;
        }
    }
    EXPECT_GT(nonZero, 0) << "precondition: rect should have non-zero pixels";

    // ---- Simulate ClearStaleHUDRegions: zero out the old rect ----
    // Clamp to buffer bounds (same logic as the real function).
    int cx = kRectX, cy = kRectY, cw = kRectW, ch = kRectH;
    if (cx < 0) { cw += cx; cx = 0; }
    if (cy < 0) { ch += cy; cy = 0; }
    if (cx + cw > kWinW) cw = kWinW - cx;
    if (cy + ch > kWinH) ch = kWinH - cy;

    if (cw > 0 && ch > 0) {
        uint16_t* row = lpVideoBuf.data() + static_cast<size_t>(cy) * kVideoPitch + cx;
        for (int yy = 0; yy < ch; yy++) {
            std::memset(row, 0, static_cast<size_t>(cw) * sizeof(uint16_t));
            row += kVideoPitch;
        }
    }

    // Verify: the rect is now all 0 (transparent on the HUD shader).
    int nonZeroAfter = 0;
    for (int yy = kRectY; yy < kRectY + kRectH; yy++) {
        for (int xx = kRectX; xx < kRectX + kRectW; xx++) {
            if (lpVideoBuf[static_cast<size_t>(yy) * kVideoPitch + xx] != 0)
                nonZeroAfter++;
        }
    }
    EXPECT_EQ(nonZeroAfter, 0) << "after ClearStale, rect should be all 0";

    // Verify surrounding pixels are still intact.
    EXPECT_EQ(lpVideoBuf[0], 0xBEEF) << "top-left pixel should be untouched";
    EXPECT_EQ(lpVideoBuf[static_cast<size_t>(kRectY - 1) * kVideoPitch + kRectX], 0xBEEF)
        << "pixel just above the rect should be untouched";
}

TEST(MapScaling, ClearStaleClippedAtEdges)
{
    // Verify that ClearStaleHUDRegions clamping doesn't write out of bounds.
    constexpr int kWinW = 800, kWinH = 600;
    constexpr int kVideoPitch = kWinW;

    std::vector<uint16_t> lpVideoBuf(static_cast<size_t>(kVideoPitch) * kWinH, 0xBEEF);

    // Rect that extends beyond the left and top edges.
    int cx = -10, cy = -5, cw = 100, ch = 50;
    // Clamp (same as real ClearStaleHUDRegions).
    if (cx < 0) { cw += cx; cx = 0; }
    if (cy < 0) { ch += cy; cy = 0; }
    if (cx + cw > kWinW) cw = kWinW - cx;
    if (cy + ch > kWinH) ch = kWinH - cy;

    ASSERT_GT(cw, 0);
    ASSERT_GT(ch, 0);

    uint16_t* row = lpVideoBuf.data() + static_cast<size_t>(cy) * kVideoPitch + cx;
    for (int yy = 0; yy < ch; yy++) {
        std::memset(row, 0, static_cast<size_t>(cw) * sizeof(uint16_t));
        row += kVideoPitch;
    }

    // The clamped region should be zeroed.
    for (int yy = cy; yy < cy + ch; yy++) {
        for (int xx = cx; xx < cx + cw; xx++) {
            EXPECT_EQ(lpVideoBuf[static_cast<size_t>(yy) * kVideoPitch + xx], 0);
        }
    }
    // Pixels immediately outside the clamped rect should be untouched.
    // Pixel (0, 0) is inside the clamped region, so it must not be used
    // for this check.
    ASSERT_LT(cx + cw, kWinW);
    EXPECT_EQ(lpVideoBuf[static_cast<size_t>(cy) * kVideoPitch + cx + cw], 0xBEEF);
}

// ---------------------------------------------------------------------------
// HUD overlay lifecycle (adapted from C1's toggle lifecycle test)
// ---------------------------------------------------------------------------
//
// C2's HUD overlay has a similar lifecycle to C1's map background:
//   frame 1: draw HUD → dirty rects recorded, upload happens
//   frame 2: HUD off → ClearStaleHUDRegions zeros old rects
//   frame 3: HUD on  → fresh draw, fresh upload

TEST(MapScaling, HUDLifecycle)
{
    // Model the HUD overlay state machine as a struct with counters.
    struct HUDState {
        bool  overlayVisible = false;   // is the HUD element being drawn?
        int   dirtyRectCount  = 0;     // rects drawn this frame
        int   prevDirtyCount  = 0;     // rects from previous frame
        bool  needsFullClear  = false; // m_hudNeedsFullClear
        bool  needsFullUpload = false; // m_hudNeedsFullUpload
        bool  didUpload       = false; // did a texture upload happen?
    };

    auto draw_hud = [](HUDState& s) {
        if (s.overlayVisible) {
            s.dirtyRectCount = 1;  // one rect drawn
        } else {
            s.dirtyRectCount = 0;
        }
    };

    auto clear_stale = [](HUDState& s) {
        if (s.needsFullClear) {
            s.needsFullClear = false;
            s.needsFullUpload = true;
            return;
        }
        // ClearStaleHUDRegions zeros prev rects (modelled by resetting prevDirtyCount).
        s.prevDirtyCount = 0;
    };

    auto upload = [](HUDState& s) {
        if (s.needsFullUpload || s.dirtyRectCount > 0 || s.prevDirtyCount > 0) {
            s.didUpload = true;
            s.needsFullUpload = false;
        } else {
            s.didUpload = false;
        }
    };

    auto end_frame = [](HUDState& s) {
        s.prevDirtyCount = s.dirtyRectCount;
        s.dirtyRectCount = 0;
        s.didUpload = false;
    };

    HUDState s;
    s.needsFullUpload = true;  // initial state

    // Frame 1: HUD visible → draw + upload
    s.overlayVisible = true;
    draw_hud(s);
    clear_stale(s);
    upload(s);
    EXPECT_TRUE(s.didUpload) << "frame 1: must upload";
    end_frame(s);
    EXPECT_EQ(s.prevDirtyCount, 1);

    // Frame 2: HUD hidden → no draw, stale cleared
    s.overlayVisible = false;
    draw_hud(s);
    EXPECT_EQ(s.dirtyRectCount, 0);
    clear_stale(s);
    EXPECT_EQ(s.prevDirtyCount, 0) << "stale rects cleared";
    upload(s);
    // No upload needed if no dirty rects and no full-upload flag.
    EXPECT_FALSE(s.didUpload) << "frame 2: no content, no upload needed";
    end_frame(s);

    // Frame 3: HUD visible again → draw + upload
    s.overlayVisible = true;
    draw_hud(s);
    EXPECT_EQ(s.dirtyRectCount, 1);
    clear_stale(s);
    upload(s);
    EXPECT_TRUE(s.didUpload) << "frame 3: must upload fresh content";
    end_frame(s);
    EXPECT_EQ(s.prevDirtyCount, 1);
}

TEST(MapScaling, FullClearTriggersFullUpload)
{
    // When a full-buffer DIB write (CopyHARDToDIB) happens, m_hudNeedsFullClear
    // is set.  This forces a full clear + full upload next frame.
    struct State {
        bool needsFullClear  = false;
        bool needsFullUpload = false;
        int  prevDirtyCount  = 0;
        int  dirtyRectCount  = 0;
    };

    State s;
    s.needsFullClear = true;  // after CopyHARDToDIB

    // Simulate ClearStaleHUDRegions when m_hudNeedsFullClear is true
    if (s.needsFullClear) {
        s.needsFullClear = false;
        s.needsFullUpload = true;
    }

    EXPECT_FALSE(s.needsFullClear)  << "clear flag consumed";
    EXPECT_TRUE(s.needsFullUpload)  << "full upload must be scheduled";
}
