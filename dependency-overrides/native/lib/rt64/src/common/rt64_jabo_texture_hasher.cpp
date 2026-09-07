//
// RT64 - legacy Jabo texture-pack compatibility
//

#include "rt64_jabo_texture_hasher.h"

#include <algorithm>
#include <array>
#include <cstdio>

#include "contrib/mupen64plus-core/subprojects/md5/md5.h"
#include "shared/rt64_f3d_defines.h"

namespace RT64 {
    namespace {
        constexpr uint32_t TMEMBytes = 0x1000;
        constexpr uint32_t TMEMPalette = 0x800;
        constexpr uint32_t TMEMMask8 = 0xFFF;
        constexpr uint32_t TMEMMask16 = 0x7FF;

        struct RGBA8 {
            uint8_t r;
            uint8_t g;
            uint8_t b;
            uint8_t a;
        };

        uint8_t expand4(uint8_t value) {
            return uint8_t((value << 4) | value);
        }

        uint8_t expand5(uint8_t value) {
            return uint8_t((value << 3) | (value >> 2));
        }

        RGBA8 rgba16(uint16_t value) {
            return {
                expand5(uint8_t((value >> 11) & 0x1F)),
                expand5(uint8_t((value >> 6) & 0x1F)),
                expand5(uint8_t((value >> 1) & 0x1F)),
                uint8_t((value & 1) ? 0xFF : 0x00)
            };
        }

        RGBA8 ia16(uint16_t value) {
            const uint8_t intensity = uint8_t(value >> 8);
            return { intensity, intensity, intensity, uint8_t(value) };
        }

        uint8_t loadTMEM(const uint8_t *tmem, uint32_t relativeAddress, uint32_t addressMask, uint32_t orAddress, bool oddRow, uint32_t textureStart, uint32_t rowSize) {
            uint32_t finalAddress;
            if (oddRow) {
                const uint32_t rowStart = (relativeAddress / rowSize) * rowSize;
                const uint32_t wordIndex = (relativeAddress - rowStart) / 4;
                finalAddress = textureStart + rowStart + ((wordIndex ^ 1U) * 4) + (relativeAddress & 3U);
            }
            else {
                finalAddress = textureStart + relativeAddress;
            }

            return tmem[((finalAddress & addressMask) | orAddress) & TMEMMask8];
        }

        std::string md5(const std::vector<uint8_t> &bytes) {
            md5_state_t state;
            std::array<md5_byte_t, 16> digest = {};
            md5_init(&state);
            md5_append(&state, bytes.data(), int(bytes.size()));
            md5_finish(&state, digest.data());

            char text[33] = {};
            for (size_t i = 0; i < digest.size(); i++) {
                std::snprintf(text + i * 2, 3, "%02x", digest[i]);
            }

            return text;
        }

        void append16(std::vector<uint8_t> &bytes, uint16_t value) {
            bytes.emplace_back(uint8_t(value));
            bytes.emplace_back(uint8_t(value >> 8));
        }
    }

    std::vector<std::string> JaboTextureHasher::hashes(const uint8_t *tmem, size_t tmemSize, const LoadTile &loadTile, uint32_t width, uint32_t height, uint32_t tlut) {
        if ((tmem == nullptr) || (tmemSize < TMEMBytes) || (width == 0) || (height == 0)) {
            return {};
        }

        const bool rgba32 = (loadTile.fmt == G_IM_FMT_RGBA) && (loadTile.siz == G_IM_SIZ_32b);
        const bool usesTLUT = tlut > 0;
        const uint32_t tmemShift = rgba32 ? G_IM_SIZ_16b : loadTile.siz;
        const uint32_t addressMask = (rgba32 || usesTLUT) ? TMEMMask16 : TMEMMask8;
        const uint32_t textureStart = loadTile.tmem << 3;
        uint32_t rowSize = loadTile.line << 3;
        if (rowSize == 0) {
            rowSize = std::max((width << tmemShift) >> 1U, 1U);
        }

        std::vector<RGBA8> pixels;
        pixels.reserve(size_t(width) * height);
        for (uint32_t y = 0; y < height; y++) {
            const bool oddRow = (y & 1U) != 0;
            for (uint32_t x = 0; x < width; x++) {
                const bool oddColumn = (x & 1U) != 0;
                const uint32_t pixelAddress = y * rowSize + ((x << tmemShift) >> 1U);
                const uint8_t p0 = loadTMEM(tmem, pixelAddress, addressMask, 0, oddRow, textureStart, rowSize);
                const uint8_t p1 = loadTMEM(tmem, pixelAddress + 1, addressMask, 0, oddRow, textureStart, rowSize);
                const uint8_t p4 = uint8_t((p0 >> (oddColumn ? 0 : 4)) & 0xF);
                RGBA8 pixel = { 0, 0, 0, 0xFF };

                if (usesTLUT) {
                    const uint32_t paletteAddress = (loadTile.siz == G_IM_SIZ_4b)
                        ? TMEMPalette + (loadTile.palette << 7) + (p4 << 3)
                        : TMEMPalette + (uint32_t(p0) << 3);
                    const uint16_t paletteValue = uint16_t((tmem[paletteAddress & TMEMMask8] << 8) | tmem[(paletteAddress + 1) & TMEMMask8]);
                    pixel = (tlut == G_TT_IA16) ? ia16(paletteValue) : rgba16(paletteValue);
                }
                else if (loadTile.siz == G_IM_SIZ_4b) {
                    if (loadTile.fmt == G_IM_FMT_IA) {
                        const uint8_t intensityBits = p4 & 0xE;
                        const uint8_t intensity = uint8_t((intensityBits << 4) | (intensityBits << 1) | (intensityBits >> 2));
                        pixel = { intensity, intensity, intensity, uint8_t((p4 & 1) ? 0xFF : 0x00) };
                    }
                    else {
                        const uint8_t intensity = expand4(p4);
                        pixel = { intensity, intensity, intensity, intensity };
                    }
                }
                else if (loadTile.siz == G_IM_SIZ_8b) {
                    if (loadTile.fmt == G_IM_FMT_IA) {
                        const uint8_t intensity = expand4(p0 >> 4);
                        pixel = { intensity, intensity, intensity, expand4(p0 & 0xF) };
                    }
                    else {
                        pixel = { p0, p0, p0, p0 };
                    }
                }
                else if (loadTile.siz == G_IM_SIZ_16b) {
                    if (loadTile.fmt == G_IM_FMT_RGBA) {
                        pixel = rgba16(uint16_t((p0 << 8) | p1));
                    }
                    else if (loadTile.fmt == G_IM_FMT_IA) {
                        pixel = ia16(uint16_t((p0 << 8) | p1));
                    }
                    else {
                        pixel = { p0, p1, p0, p1 };
                    }
                }
                else if (loadTile.siz == G_IM_SIZ_32b) {
                    const uint32_t highAddress = rgba32 ? pixelAddress : pixelAddress + 2;
                    const uint32_t highBank = rgba32 ? TMEMPalette : 0;
                    const uint8_t p2 = loadTMEM(tmem, highAddress, addressMask, highBank, oddRow, textureStart, rowSize);
                    const uint8_t p3 = loadTMEM(tmem, highAddress + 1, addressMask, highBank, oddRow, textureStart, rowSize);
                    if (loadTile.fmt == G_IM_FMT_RGBA) {
                        pixel = { p0, p1, p2, p3 };
                    }
                    else if (oddColumn) {
                        pixel = { p0, p1, p0, p1 };
                    }
                    else {
                        pixel = { p2, p3, p2, p3 };
                    }
                }

                pixels.emplace_back(pixel);
            }
        }

        // D3D8 formats used by Jabo: A8R8G8B8, X8R8G8B8, R5G6B5,
        // X1R5G5B5, A1R5G5B5 and A4R4G4B4. The in-memory byte order is
        // little-endian, matching the MD5 input used by the plug-in.
        std::array<std::vector<uint8_t>, 6> candidates;
        for (auto &candidate : candidates) {
            candidate.reserve(pixels.size() * 4);
        }

        for (const RGBA8 &pixel : pixels) {
            candidates[0].insert(candidates[0].end(), { pixel.b, pixel.g, pixel.r, pixel.a });
            candidates[1].insert(candidates[1].end(), { pixel.b, pixel.g, pixel.r, 0xFF });
            append16(candidates[2], uint16_t(((pixel.r >> 3) << 11) | ((pixel.g >> 2) << 5) | (pixel.b >> 3)));
            append16(candidates[3], uint16_t(0x8000 | ((pixel.r >> 3) << 10) | ((pixel.g >> 3) << 5) | (pixel.b >> 3)));
            append16(candidates[4], uint16_t(((pixel.a >= 0x80) ? 0x8000 : 0) | ((pixel.r >> 3) << 10) | ((pixel.g >> 3) << 5) | (pixel.b >> 3)));
            append16(candidates[5], uint16_t(((pixel.a >> 4) << 12) | ((pixel.r >> 4) << 8) | ((pixel.g >> 4) << 4) | (pixel.b >> 4)));
        }

        std::vector<std::string> result;
        result.reserve(candidates.size());
        for (const auto &candidate : candidates) {
            std::string digest = md5(candidate);
            if (std::find(result.begin(), result.end(), digest) == result.end()) {
                result.emplace_back(std::move(digest));
            }
        }

        return result;
    }
};
