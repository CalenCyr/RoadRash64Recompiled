#include "rr64_video_mode.hpp"
#include "ultramodern/config.hpp"

extern "C" unsigned int rr64_combined_video_callback(unsigned char* memory, unsigned int original) {
    return rr64::video::combined_callback(memory, original, true);
}
