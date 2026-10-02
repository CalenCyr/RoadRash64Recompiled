#include "rr64_highlight_traffic.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <stdexcept>

namespace rr64::highlights {
namespace {
constexpr unsigned missing = ~0u;
constexpr unsigned maximum_upload = 1024u * 1024u;
void require(bool ok, const char *reason) { if (!ok) throw std::runtime_error(reason); }
unsigned word(std::span<const unsigned char> bytes, std::size_t p) {
    require(p <= bytes.size() && bytes.size() - p >= 4, "traffic ROM word range");
    return unsigned(bytes[p]) << 24 | unsigned(bytes[p+1]) << 16 |
           unsigned(bytes[p+2]) << 8 | unsigned(bytes[p+3]);
}
bool traffic_model(unsigned id) {
    return (id >= 0xd8 && id <= 0x101) || id == 0x125 || id == 0x126;
}
}
bool build_traffic_assets(std::span<const unsigned char> rom, TrafficAssets &output,
                          std::string &error) noexcept {
    try {
        require(rom.size() >= 0x2000000 && rom.size() <= 0x4000000 &&
                word(rom, 0) == 0x80371240 && word(rom, 0x1303204) == 295,
                "traffic cache requires the supported Road Rash ROM");
        std::array<unsigned, 295> descriptors{};
        unsigned p = 0x1303208;
        for (auto &d : descriptors) {
            require(p < rom.size() && rom.size() - p >= 8, "traffic descriptor range");
            d = p;
            p += 8u + 16u * rom[p] + 14u * rom[p+1];
        }
        require(p == 0x1304db0, "traffic descriptor layout changed");
        TrafficAssets candidate;
        candidate.models.fill(missing);
        std::map<unsigned, unsigned> retained;
        std::vector<unsigned> resources;
        for (unsigned id = 0; id < candidate.models.size(); ++id) {
            if (!traffic_model(id)) continue;
            const auto d = descriptors[id == 0xf8 ? 0xf7 : id];
            const unsigned group = rom[d+4];
            require(group == 5, "traffic model is not in the native vehicle group");
            unsigned resource = rom[d+3];
            for (unsigned g = 0; g < group; ++g) resource += word(rom, 0xa8334 + g*4);
            require(resource < 373, "traffic resource index range");
            auto [found, inserted] = retained.emplace(resource,unsigned(resources.size()));
            if (inserted) resources.push_back(resource);
            candidate.models[id] = found->second;
        }
        // Exact native F958 material packets and 15A90/F2L child matrices,
        // without packet rewriting or unrelated course/placement compilation.
        if (!world::build_object_model_assets(rom,resources,candidate.assets,error))
            throw std::runtime_error(error);
        require(candidate.assets.bytes.size() <= maximum_upload, "traffic cache budget exceeded");
        for (const auto &model : candidate.assets.models)
            require(model.source_bank == 2 && model.vertices && model.triangles,
                    "traffic static source-2 model is unavailable");
        for (const auto &t : candidate.assets.textures) {
            require(t.model_index < resources.size() && t.raw_offset <= candidate.assets.bytes.size() &&
                        t.raw_size <= candidate.assets.bytes.size()-t.raw_offset,
                    "traffic texture ownership range");
            // Original flashing-light pixels/materials are retained in full.
            // The driver selects their frame from recorded time. Native
            // visibility-dependent starting phase is not in the wire format.
        }
        for (const auto &r : candidate.assets.relocations)
            require(r.word_offset <= candidate.assets.bytes.size() &&
                        candidate.assets.bytes.size()-r.word_offset >= 4 &&
                        r.target_offset < candidate.assets.bytes.size(),
                    "traffic relocation escapes owned upload");
        output = std::move(candidate);
        error.clear();
        return true;
    } catch (const std::exception &e) { error = e.what(); return false; }
    catch (...) { error = "unknown traffic asset preparation failure"; return false; }
}
}
