#pragma once

#include "Constants.h"
#include <cmath>

namespace water_wave {

struct Parameters { float amp1, amp2, amp3, speed; };
struct Surface { float wx[3], wy[3], wz[3]; };

// Cache direct legacy float expressions, not angle-addition approximations.
// RealTime is absolute system uptime: regrouped phases can materially change
// waves/lighting at long uptimes. Preserve their original addition order.
class Cache {
public:
    Surface surface[32][32]{};
    float lighting[32][32]{};

    void update(int time, const int (&random)[32][32], Parameters params)
    {
        const bool timeChanged = !ready || time != previousTime;
        const bool surfaceChanged = timeChanged || params.amp1 != previous.amp1 ||
            params.amp2 != previous.amp2 || params.amp3 != previous.amp3 ||
            params.speed != previous.speed;
        const float t = static_cast<float>(time) / 200.f * params.speed;
        const float amp1y = params.amp1 * 0.8f;
        const float amp2y = params.amp2 * 0.8f;
        const float amp3y = params.amp3 * 0.8f;
        for (int wy = 0; wy < 32; ++wy) {
            for (int wx = 0; wx < 32; ++wx) {
                const int r1 = random[wy][wx];
                const int r2 = random[(wy + 11) & 31][(wx + 7) & 31];
                const bool random1Changed = !ready || r1 != phases[wy][wx].r1;
                const bool random2Changed = !ready || r2 != phases[wy][wx].r2;
                if (timeChanged || random1Changed) {
                    // Integer division is part of the original lighting phase.
                    lighting[wy][wx] = std::sin(-pi / 2 + r1 / 128 +
                                               static_cast<float>(time) / 200.f);
                }
                if (surfaceChanged || random1Changed || random2Changed) {
                    const float px = static_cast<float>(wx) * 0.5f;
                    const float py = static_cast<float>(wy) * 0.5f;
                    Surface& wc = surface[wy][wx];
                    wc.wx[0] = std::sin(px + py + t) * params.amp1;
                    wc.wy[0] = std::sin(px * 0.8f + py * 0.6f + t * 0.9f) * amp1y;
                    wc.wz[0] = std::sin(pi / 2.f + px + py + t) * params.amp1;
                    wc.wx[1] = std::sin(px * 1.5f - py * 0.7f + t * 1.3f + r1 * 0.01f) * params.amp2;
                    wc.wy[1] = std::sin(px * 1.2f - py * 0.9f + t * 1.1f + r1 * 0.01f) * amp2y;
                    wc.wz[1] = std::sin(pi / 3.f + px * 1.5f - py * 0.7f + t * 1.3f + r2 * 0.01f) * params.amp2;
                    wc.wx[2] = std::sin(px * 2.3f + py * 1.2f + t * 2.1f + r2 * 0.01f) * params.amp3;
                    wc.wy[2] = std::sin(px * 2.0f + py * 1.5f + t * 1.8f + r2 * 0.01f) * amp3y;
                    wc.wz[2] = std::sin(pi / 4.f + px * 2.3f + py * 1.2f + t * 2.1f + r1 * 0.01f) * params.amp3;
                }
                phases[wy][wx] = {r1, r2};
            }
        }
        previousTime = time;
        previous = params;
        ready = true;
    }

private:
    struct Phases { int r1 = 0, r2 = 0; };
    Phases phases[32][32]{};
    Parameters previous{};
    int previousTime = 0;
    bool ready = false;
};

} // namespace water_wave
