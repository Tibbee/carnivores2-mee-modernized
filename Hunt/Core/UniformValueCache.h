#pragma once

#include <array>
#include <cstddef>
#include <cstring>

// GL-independent dirty-value policy for float uniforms and packed UBO data.
// Own one cache per uniform/program or per buffer; invalidate on external
// writes, allocation, or program/context rebuild.
// First use always uploads; exact float bytes (no epsilon) determine changes.
// Do not use this if another path can write the uniform without updating it.
template<std::size_t N>
class UniformValueCache {
    static_assert(N > 0, "Uniform must have at least one component");
public:
    void invalidate() { valid_ = false; }

    bool shouldUpload(const std::array<float, N>& value) {
        if (valid_ && std::memcmp(value.data(), value_.data(), N * sizeof(float)) == 0)
            return false;
        value_ = value;
        valid_ = true;
        return true;
    }
private:
    std::array<float, N> value_{};
    bool valid_ = false;
};
