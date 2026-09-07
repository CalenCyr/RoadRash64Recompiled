#include <cmath>
#include <cstdio>

#include "rr64_voice_chat.hpp"

int main() {
    const float near_gain = rr64::voice_chat::proximity_gain(20.0f);
    const float middle_gain = rr64::voice_chat::proximity_gain(500.0f);
    const float far_gain = rr64::voice_chat::proximity_gain(1200.0f);
    if (std::abs(near_gain - 1.0f) > 0.001f ||
        !(middle_gain > 0.0f && middle_gain < near_gain) ||
        std::abs(far_gain) > 0.001f) {
        std::fprintf(stderr, "Invalid proximity curve: %.3f %.3f %.3f\n",
            near_gain, middle_gain, far_gain);
        return 1;
    }
    std::puts("RR64 proximity voice attenuation smoke test passed.");
    return 0;
}
