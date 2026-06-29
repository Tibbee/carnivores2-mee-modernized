// ==========================================================================
// Assert.h — Debug assertion framework
// ==========================================================================
// ASSERT(cond, msg) triggers a debug break in _DEBUG builds and logs the
// failure. In release builds it compiles away to nothing.
//
// Usage:
//   ASSERT(ptr != nullptr, "character info pointer is null");
//   ASSERT(CCX >= 0 && CCX < ctMapSize, "CCX out of map bounds");

#pragma once

#include "Log.h"

#ifdef _DEBUG
    #define ASSERT(cond, msg) do { \
        if (!(cond)) { \
            LOG_ERROR("ASSERTION FAILED: %s (condition: %s)", msg, #cond); \
            __debugbreak(); \
        } \
    } while(0)
#else
    #define ASSERT(cond, msg) ((void)0)
#endif
