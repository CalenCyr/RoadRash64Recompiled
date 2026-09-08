#include "rr64_online_race_sync.hpp"
#include "rr64_local_players.hpp"

#include <algorithm>
#include <atomic>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>

#include "recomp.h"

#include "rr64_native.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_netplay.hpp"

namespace rr64::online_race_sync {
namespace {

std::uint32_t g_local_tick = 0;
thread_local ViewportRenderPlan g_viewport_render_plan{};
std::atomic_bool g_render_race{false};

float read_float(unsigned char* rdram, std::uint32_t address) {
    float value = 0.0f;
    engine::read_float(rdram, address, value);
    return value;
}

void write_float(unsigned char* rdram, std::uint32_t address, float value) {
    engine::write_float(rdram, address, value);
}

bool read_vector(unsigned char* rdram, std::uint32_t address, float& x, float& y, float& z) {
    if (!engine::valid_guest_range(address, 12)) {
        return false;
    }
    x = read_float(rdram, address + 0);
    y = read_float(rdram, address + 4);
    z = read_float(rdram, address + 8);
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
}

void write_vector(unsigned char* rdram, std::uint32_t address, float x, float y, float z) {
    if (!engine::valid_guest_range(address, 12) || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
        return;
    }
    write_float(rdram, address + 0, x);
    write_float(rdram, address + 4, y);
    write_float(rdram, address + 8, z);
}

std::uint32_t bike_pool(unsigned char* rdram) {
    if (rdram == nullptr) {
        return 0;
    }
    std::uint32_t pool = 0;
    engine::read_u32(rdram, engine::globals::bike_pool_pointer, pool);
    return pool;
}

std::uint32_t active_racers(unsigned char* rdram) {
    if (rdram == nullptr) {
        return 0;
    }
    std::uint32_t count_bits = 0;
    // EF5C is the four-controller menu count. 6574 is the populated
    // bike/rider count, including network riders represented by AI slots.
    engine::read_u32(rdram, 0x800A6574u, count_bits);
    const std::int32_t count = static_cast<std::int32_t>(count_bits);
    return count > 0 ? std::min<std::uint32_t>(count, netplay::kMaximumPlayers) : 0;
}

std::uint8_t guest_index_for_slot(std::uint8_t network_slot, std::uint8_t local_slot) {
    // Every machine keeps its locally controlled rider in guest entity zero.
    // Swapping network slot zero with the local slot keeps a unique, stable
    // mapping for all fourteen canonical riders without needing extra N64
    // controller ports.
    if (network_slot == local_slot) {
        return 0;
    }
    if (network_slot == 0) {
        return local_slot;
    }
    return network_slot;
}

std::uint8_t network_slot_for_guest_index(
    std::uint8_t guest_index,
    std::uint8_t local_slot,
    bool replicated_riders)
{
    if (!replicated_riders) {
        return guest_index;
    }
    if (guest_index == 0) {
        return local_slot;
    }
    if (guest_index == local_slot) {
        return 0;
    }
    return guest_index;
}

std::string safe_display_name(const netplay::Status& status, std::uint8_t slot) {
    std::string result;
    if (slot < netplay::kMaximumPlayers) {
        result = status.players[slot].name;
    }
    if (result.empty()) {
        result = "Rider " + std::to_string(static_cast<unsigned>(slot) + 1u);
    }
    for (char& character : result) {
        const unsigned char value = static_cast<unsigned char>(character);
        if (value < 0x20u || value > 0x7Eu) {
            character = '_';
        }
    }
    result.resize(std::min<std::size_t>(
        result.size(),
        engine::globals::multiplayer_display_name_stride - 1u));
    return result;
}

void apply_online_display_names(unsigned char* rdram) {
    const netplay::Status status = netplay::get_status();
    if (!status.active && rdram && local_players::active.load(std::memory_order_acquire)) {
        static_assert(engine::globals::multiplayer_display_name_stride == 12);
        const auto names = local_players::snapshot();
        for (unsigned slot = 0; slot < names.size(); ++slot) {
            const auto address = engine::globals::multiplayer_display_names +
                slot * engine::globals::multiplayer_display_name_stride;
            for (unsigned i = 0; i < engine::globals::multiplayer_display_name_stride; ++i) {
                engine::write_s8(rdram, address + i, i < names[slot].size() ? names[slot][i] : 0);
            }
        }
        return;
    }
    if (!status.active || !status.connected || status.phase < netplay::Phase::CharacterSelect ||
        status.local_slot >= netplay::kMaximumPlayers) {
        return;
    }

    for (std::uint8_t guest = 0;
         guest < engine::globals::multiplayer_display_name_count;
         ++guest) {
        const std::uint8_t slot = network_slot_for_guest_index(
            guest, status.local_slot, status.replicated_riders);
        const std::string name = safe_display_name(status, slot);
        const std::uint32_t address = engine::globals::multiplayer_display_names +
            static_cast<std::uint32_t>(guest) * engine::globals::multiplayer_display_name_stride;
        for (std::uint32_t index = 0;
             index < engine::globals::multiplayer_display_name_stride;
             ++index) {
            const char character = index < name.size() ? name[index] : '\0';
            engine::write_s8(rdram, address + index, static_cast<std::int8_t>(character));
        }
    }
}

bool capture_bike(unsigned char* rdram, std::uint32_t bike, netplay::RiderState& state) {
    if (!engine::valid_guest_range(bike, engine::bike::stride)) {
        return false;
    }
    return read_vector(
               rdram,
               bike + engine::bike::body_position,
               state.position_x,
               state.position_y,
               state.position_z) &&
        read_vector(
               rdram,
               bike + engine::bike::front_wheel_position,
               state.front_wheel_x,
               state.front_wheel_y,
               state.front_wheel_z) &&
        read_vector(
               rdram,
               bike + engine::bike::rear_wheel_position,
               state.rear_wheel_x,
               state.rear_wheel_y,
               state.rear_wheel_z);
}

void apply_bike(unsigned char* rdram, std::uint32_t bike, const netplay::RiderState& state) {
    if (!engine::valid_guest_range(bike, engine::bike::stride)) {
        return;
    }
    write_vector(rdram, bike + engine::bike::body_position, state.position_x, state.position_y, state.position_z);
    write_vector(rdram, bike + engine::bike::front_wheel_position, state.front_wheel_x, state.front_wheel_y, state.front_wheel_z);
    write_vector(rdram, bike + engine::bike::rear_wheel_position, state.rear_wheel_x, state.rear_wheel_y, state.rear_wheel_z);
}

void apply_remote_riders(unsigned char* rdram) {
    const netplay::Status status = netplay::get_status();
    if (!status.active || !status.connected || !status.replicated_riders ||
        status.phase != netplay::Phase::Race || status.local_slot >= netplay::kMaximumPlayers) {
        return;
    }

    const std::uint32_t pool = bike_pool(rdram);
    const std::uint32_t count = active_racers(rdram);
    if (!engine::valid_guest_range(pool, count * engine::bike::stride)) {
        return;
    }

    for (std::uint8_t slot = 0; slot < netplay::kMaximumPlayers; ++slot) {
        if (slot == status.local_slot || !status.players[slot].connected) {
            continue;
        }
        const std::uint8_t guest_index = guest_index_for_slot(slot, status.local_slot);
        if (guest_index >= count) {
            continue;
        }
        netplay::RiderState state{};
        if (netplay::get_interpolated_rider_state(slot, 0.5f, state)) {
            apply_bike(rdram, pool + static_cast<std::uint32_t>(guest_index) * engine::bike::stride, state);
        }
    }
}

void capture_local_rider(unsigned char* rdram) {
    const netplay::Status status = netplay::get_status();
    if (!status.active || !status.connected ||
        status.phase != netplay::Phase::Race || status.local_slot >= netplay::kMaximumPlayers) {
        return;
    }
    const std::uint32_t pool = bike_pool(rdram);
    const std::uint32_t count = active_racers(rdram);
    const std::uint8_t guest_index = status.replicated_riders ? 0 : status.local_slot;
    if (guest_index >= count || !engine::valid_guest_range(pool, count * engine::bike::stride)) {
        return;
    }
    netplay::RiderState state{};
    if (!capture_bike(
            rdram,
            pool + static_cast<std::uint32_t>(guest_index) * engine::bike::stride,
            state)) {
        return;
    }
    state.active = true;
    state.tick = ++g_local_tick;
    state.character = status.players[status.local_slot].character;
    netplay::set_local_rider_state(state);
}

} // namespace

std::uint32_t requested_racer_count(std::uint32_t original_count) {
    const netplay::Status status = netplay::get_status();
    if (!status.active || !status.connected ||
        status.phase < netplay::Phase::GameSetup) {
        return original_count;
    }
    // This hook is in the controller-count chooser, not the racer pool.
    return status.replicated_riders ? 1u : std::clamp<unsigned>(status.connected_players,1u,4u);
}

std::uint32_t prepare_render_layout(std::uint32_t stock_layout) {
    const netplay::Status status = netplay::get_status();
    g_viewport_render_plan = make_viewport_render_plan(
        stock_layout,
        status.active,
        status.connected,
        status.phase == netplay::Phase::Race && g_render_race.load(),
        status.replicated_riders,
        status.local_slot);
    return g_viewport_render_plan.layout;
}

std::uint32_t first_render_viewport(std::uint32_t stock_viewport) {
    return g_viewport_render_plan.peer_fullscreen
        ? g_viewport_render_plan.first_viewport
        : stock_viewport;
}

std::uint32_t geometry_render_viewport(std::uint32_t stock_viewport) {
    return g_viewport_render_plan.peer_fullscreen
        ? g_viewport_render_plan.geometry_viewport
        : stock_viewport;
}

void restore_active_render_viewport(unsigned char* rdram, std::uint32_t stock_viewport) {
    if (!g_viewport_render_plan.peer_fullscreen) {
        return;
    }
    const std::uint32_t local_viewport = g_viewport_render_plan.first_viewport;
    if (stock_viewport == local_viewport) {
        engine::write_u32(rdram, engine::globals::active_viewport, local_viewport);
    }
}

void before_guest_update(unsigned char* rdram, std::uint32_t mode) {
    std::uint32_t pending=0;
    engine::read_u32(rdram,engine::globals::pending_mode,pending);
    g_render_race.store(engine::is_live_race_transition(mode,pending));
    const auto status=netplay::get_status();
    static std::array<std::uint32_t,5> previous{};
    const std::array<std::uint32_t,5> key{mode,pending,unsigned(status.phase),status.local_slot,status.connected_players};
    if(status.active&&status.connected&&key!=previous){
        previous=key;
        std::fprintf(stderr,"[RR64-ONLINE-BOUNDARY] host=%u slot=%u peers=%u replicated=%u mode=%u pending=%u phase=%u setup-revision=%u\n",
            status.is_host,status.local_slot,status.connected_players,status.replicated_riders,mode,pending,unsigned(status.phase),status.game_setup.revision);
        for(const auto address:engine::globals::multiplayer_game_setup_words){std::uint32_t value=0;engine::read_u32(rdram,address,value);std::fprintf(stderr,"[RR64-ONLINE-SETUP] %08X=%08X\n",address,value);}
        std::fflush(stderr);
    }
    if (rr64_is_live_race_mode(mode) != 0) {
        apply_online_display_names(rdram);
        apply_remote_riders(rdram);
    }
}

void after_guest_update(unsigned char* rdram, std::uint32_t mode) {
    std::uint32_t pending=0;engine::read_u32(rdram,engine::globals::pending_mode,pending);
    g_render_race.store(engine::is_live_race_transition(mode,pending));
    if (rr64_is_live_race_mode(mode) == 0) {
        return;
    }
    netplay::Status status = netplay::get_status();
    if (status.is_host && status.active && status.connected &&
        status.phase >= netplay::Phase::CharacterSelect && status.phase != netplay::Phase::Race) {
        netplay::host_set_phase(netplay::Phase::Race);
        status = netplay::get_status();
    }
    if (status.phase == netplay::Phase::Race) {
        capture_local_rider(rdram);
        // The guest simulation may have advanced AI-controlled remote slots;
        // restore the canonical network transforms immediately before render.
        apply_remote_riders(rdram);
    }
}

} // namespace rr64::online_race_sync

extern "C" unsigned int rr64_online_requested_racer_count(unsigned int original_count) {
    return rr64::online_race_sync::requested_racer_count(original_count);
}

extern "C" unsigned int rr64_online_prepare_render_layout(unsigned int stock_layout) {
    return rr64::online_race_sync::prepare_render_layout(stock_layout);
}

extern "C" unsigned int rr64_online_render_first_viewport(unsigned int stock_viewport) {
    return rr64::online_race_sync::first_render_viewport(stock_viewport);
}

extern "C" unsigned int rr64_online_render_geometry_viewport(unsigned int stock_viewport) {
    return rr64::online_race_sync::geometry_render_viewport(stock_viewport);
}

extern "C" void rr64_online_restore_active_viewport(
    unsigned char* rdram,
    unsigned int stock_viewport)
{
    rr64::online_race_sync::restore_active_render_viewport(rdram, stock_viewport);
}

extern "C" void rr64_online_apply_display_names(unsigned char* rdram) {
    rr64::online_race_sync::apply_online_display_names(rdram);
}

extern "C" void rr64_online_race_sync_before_update(unsigned char* rdram, unsigned int mode) {
    rr64::online_race_sync::before_guest_update(rdram, mode);
}

extern "C" void rr64_online_race_sync_after_update(unsigned char* rdram, unsigned int mode) {
    rr64::online_race_sync::after_guest_update(rdram, mode);
}
