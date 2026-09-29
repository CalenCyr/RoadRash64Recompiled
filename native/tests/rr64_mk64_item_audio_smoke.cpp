#include "rr64_engine_layout.hpp"
#include "rr64_mk64_item_audio.hpp"
#include "rr64_netplay.hpp"
#include "rr64_prediction_replay.hpp"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <new>
#include <vector>

namespace {
std::atomic<unsigned long long> allocations{0};
unsigned checks = 0;
rr64::netplay::PhysicsRules rules;
rr64::mk64_items::Snapshot native_state;
bool course_active = true;
int replay_presenting = 0;
void check(bool value, const char *name) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "FAIL: %s\n", name);
    std::exit(1);
  }
}
void put(std::vector<std::uint8_t> &b, unsigned v) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    b.push_back(std::uint8_t(v >> shift));
}
std::vector<std::uint8_t> tone_bank() {
  using namespace rr64::mk64_items;
  std::vector<std::uint8_t> b{'R', '6', '4', 'I', 'S', 'F', 'X', '1'};
  put(b, unsigned(Sound::Count));
  constexpr bool loops[]{false, true, false, false, false, false, true,
                         true,  true, true,  false, false, true};
  for (unsigned i = 0; i < unsigned(Sound::Count); ++i) {
    const unsigned channels = i == unsigned(Sound::StarMusic) ? 2 : 1;
    put(b, i);
    put(b, 26800);
    put(b, 26800);
    put(b, channels);
    put(b, loops[i] ? 0 : ~0u);
    for (unsigned n = 0; n < 26800 * channels; ++n) {
      b.push_back(0x10);
      b.push_back(0x27);
    }
  }
  return b;
}
long long energy(std::span<const std::int16_t> stereo, unsigned channel) {
  long long sum = 0;
  for (std::size_t i = channel; i < stereo.size(); i += 2)
    sum += std::abs(int(stereo[i]));
  return sum;
}
} // namespace
namespace rr64::netplay {
PhysicsRules get_physics_rules() { return rules; }
} // namespace rr64::netplay
namespace rr64::experimental_course {
bool active() noexcept { return course_active; }
} // namespace rr64::experimental_course
namespace rr64::mk64_items {
Snapshot capture_state() noexcept { return native_state; }
} // namespace rr64::mk64_items
extern "C" int rr64_highlights_presenting() { return replay_presenting; }
void *operator new(std::size_t n) {
  allocations.fetch_add(1);
  if (auto *p = std::malloc(n ? n : 1))
    return p;
  throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
int main(int argc, char **argv) {
  using namespace rr64::mk64_items;
  check(argc == 2, "private original item audio argument");
  std::ifstream input(argv[1], std::ios::binary);
  std::vector<std::uint8_t> bank{std::istreambuf_iterator<char>(input), {}};
  std::string error;
  check(install_audio_bank(bank, error),
        "actual original item sound bank installs");
  for (std::size_t size = 0; size < bank.size(); size += 17011)
    check(!install_audio_bank(std::span(bank).first(size), error),
          "truncated sound bank rejected atomically");
  auto bad = bank;
  bad[8] = 12;
  check(!install_audio_bank(bad, error), "incomplete original set rejected");
  bad = bank;
  bad.push_back(0);
  check(!install_audio_bank(bad, error), "audio trailing bytes rejected");
  check(audio_available(), "failed load preserves complete installed bank");
  const auto tone = tone_bank();
  check(install_audio_bank(tone, error), "known waveform fixture");
  Snapshot state{};
  state.enabled = 1;
  state.random = 1;
  state.clock = 100;
  std::array<AudioRider, racer_capacity> riders{};
  for (unsigned slot = 0; slot < racer_capacity; ++slot)
    riders[slot] = {{10, 0, 0}, slot + 1, false};
  riders[0] = {{0, 0, 0}, 1, true};
  const std::array<AudioListener, 1> listeners{{{{0, 0, 0}, {1, 0, 0}, 0}}};
  std::array<std::int16_t, 4096> output{};
  auto present = [&] { present_audio(state, riders, listeners, true, false); };
  auto mix = [&] {
    output.fill(0);
    mix_audio(output, 48000, 1, 1);
  };
  auto clean = [&] {
    reset_audio();
    state = {};
    state.enabled = 1;
    state.random = 1;
    state.clock = 100;
    present();
    mix();
  };
  clean();
  state.riders[1].event_serial = 1;
  state.riders[1].cue = Cue::Shell;
  present();
  mix();
  check(energy(output, 1) > 100000 && energy(output, 0) == 0,
        "remote source right of camera pans right after host swap");
  const auto submitted_before = audio_statistics().submitted;
  for (unsigned n = 0; n < 100; ++n)
    present();
  check(audio_statistics().submitted == submitted_before,
        "duplicate authority snapshot never repeats one-shot");
  clean();
  riders[1].position = {-10, 0, 0};
  state.riders[1].event_serial = 1;
  state.riders[1].cue = Cue::Shell;
  present();
  mix();
  check(energy(output, 0) > 100000 && energy(output, 1) == 0,
        "opposite camera side pans left");
  clean();
  riders[1].position = {200, 0, 0};
  state.riders[1].event_serial = 1;
  state.riders[1].cue = Cue::Shell;
  present();
  mix();
  check(energy(output, 0) + energy(output, 1) == 0,
        "distant one-shot inaudible");
  clean();
  state.riders[0].star_until = 400;
  state.riders[0].event_serial = 1;
  state.riders[0].cue = Cue::Star;
  present();
  mix();
  check(audio_statistics().voices == 1 && energy(output, 0) > 0,
        "Star cue begins its sustained score in the same update");
  for (unsigned n = 0; n < 20; ++n) {
    present();
    mix();
  }
  check(audio_statistics().voices == 1 && energy(output, 0) > 0 &&
            item_music_gain() < .001f,
        "Star event starts one original music voice and ducks only music gain");
  const auto allocation_before = allocations.load();
  for (unsigned n = 0; n < 1000; ++n) {
    present();
    mix();
  }
  check(allocations.load() == allocation_before,
        "presentation and mixer allocate nothing on hot path");
  present_audio(state, riders, listeners, false, false);
  mix();
  check(energy(output, 0) == 0 && energy(output, 1) == 0,
        "pause freezes item audio");
  present();
  mix();
  check(energy(output, 0) > 0, "unpause resumes sustained Star");
  present_audio(state, riders, listeners, true, true);
  mix();
  check(audio_statistics().voices == 0 && energy(output, 0) == 0,
        "highlights silence item audio");
  clean();
  state.riders[0].boo_until = 300;
  state.riders[0].cue = Cue::Boo;
  state.riders[0].event_serial = 1;
  for (unsigned n = 0; n < 25; ++n) {
    present();
    mix();
  }
  check(item_music_gain() > .34f && item_music_gain() < .36f,
        "Boo partial music duck");
  state.riders[0].boo_until = 0;
  for (unsigned n = 0; n < 50; ++n) {
    present();
    mix();
  }
  check(audio_statistics().voices == 0 && item_music_gain() > .999f,
        "effect expiry fades loops and restores music");
  clean();
  riders[1].position = {0, 0, 0};
  for (unsigned n = 0; n < 256; ++n) {
    ++state.riders[1].event_serial;
    state.riders[1].cue = Cue::Shell;
    present();
  }
  mix();
  check(audio_statistics().voices <= 16 && audio_statistics().dropped > 0,
        "event and voice saturation remain bounded");
  check(*std::max_element(output.begin(), output.end()) <= 10000,
        "new item mix has fixed headroom budget");
  reset_audio();
  mix();
  check(energy(output, 0) == 0, "reset cancels queued and active sounds");
  // Exercise the real native ownership/listener wrapper for every online
  // canonical local slot, including the local-to-native-zero representation.
  std::vector<unsigned char> memory(8 * 1024 * 1024);
  auto *m = memory.data();
  using namespace rr64::engine;
  write_u32(m, globals::main_mode, 9);
  write_u32(m, globals::pending_mode, 9);
  for (unsigned slot = 0; slot < racer_capacity; ++slot) {
    const unsigned actor = 0x800D8570 + slot * 0x118;
    const unsigned bike = 0x80100000 + slot * bike::stride;
    const unsigned body = 0x80300000 + slot * rider::stride;
    write_u32(m, actor, slot);
    write_u16(m, actor + 0x24, 1);
    write_u32(m, actor + 8, ~0u);
    write_u32(m, actor + 0xE0, bike);
    write_u32(m, actor + 0xE4, body);
    write_u32(m, bike + 4, actor);
    write_u32(m, body + 4, actor);
    write_u32(m, bike + bike::rider_pointer, body);
    write_u32(m, body + rider::bike_pointer, bike);
    write_u16(m, body + rider::bike_attached, 1);
    write_float(m, bike + bike::body_position, slot == 0 ? 0.f : 10.f);
  }
  write_float(m, 0x800B7424 + 4,
              1); // View zero faces world +Y: +X is its right.
  for (unsigned local = 0; local < racer_capacity; ++local) {
    reset_audio();
    native_state = {};
    native_state.enabled = 1;
    native_state.random = 1;
    native_state.clock = 100;
    rules = {};
    rules.active = rules.connected = rules.authoritative =
        rules.replicated_riders = true;
    rules.phase = rr64::netplay::Phase::Race;
    rules.local_slot = std::uint8_t(local);
    write_u32(m, 0x800A657C, 0);
    rr64_mk64_item_audio_step(m);
    mix();
    native_state.riders[local].star_until = 300;
    native_state.riders[local].cue = Cue::Star;
    native_state.riders[local].event_serial = 1;
    for (unsigned n = 0; n < 20; ++n) {
      rr64_mk64_item_audio_step(m);
      mix();
    }
    check(audio_statistics().voices == 1 && item_music_gain() < .001f,
          "all14 canonical local Star owners map to native zero");
    const auto before_private = audio_statistics().submitted;
    {
      rr64::prediction::ReplayScope private_scope;
      rr64_mk64_item_audio_step(m);
    }
    check(audio_statistics().submitted == before_private,
          "private prediction has no mixer side effects");
    replay_presenting = 1;
    rr64_mk64_item_audio_step(m);
    mix();
    check(audio_statistics().voices == 0,
          "native highlights clears active voices");
    replay_presenting = 0;
  }
  for (unsigned local = 0; local < 4; ++local) {
    reset_audio();
    native_state = {};
    native_state.enabled = 1;
    native_state.random = 1;
    native_state.clock = 100;
    rules.replicated_riders = false;
    rules.local_slot = std::uint8_t(local);
    write_u32(m, 0x800A657C + local * 4, local);
    write_float(m, 0x800B7424 + local * 0x24 + 4, 1);
    rr64_mk64_item_audio_step(m);
    mix();
    native_state.riders[local].star_until = 300;
    for (unsigned n = 0; n < 20; ++n) {
      rr64_mk64_item_audio_step(m);
      mix();
    }
    check(audio_statistics().voices == 1 && item_music_gain() < .001f,
          "legacy four-player local view ownership");
  }
  reset_audio();
  native_state = {};
  native_state.enabled = 1;
  native_state.random = 1;
  native_state.clock = 100;
  rules = {};
  write_u32(m, local_race::humans, 1);
  write_u32(m, 0x800D8570 + 8, 0);
  write_u32(m, 0x800A657C, 0);
  write_u16(m, 0x80300000 + rider::bike_attached, 0);
  write_float(m, 0x80100000 + bike::body_position,
              500); // Abandoned bike is far away.
  rr64_mk64_item_audio_step(m);
  mix();
  native_state.riders[1].event_serial = 1;
  native_state.riders[1].cue = Cue::Shell;
  rr64_mk64_item_audio_step(m);
  mix();
  check(energy(output, 1) > 100000 && energy(output, 0) == 0,
        "fallen local listener uses rider position and per-view camera");
  reset_audio();
  native_state = {};
  native_state.enabled = 1;
  native_state.random = 1;
  native_state.clock = 100;
  write_float(m, 0x800B7424 + 4, -1); // Turn the same listener camera around.
  rr64_mk64_item_audio_step(m);
  mix();
  native_state.riders[1].event_serial = 1;
  native_state.riders[1].cue = Cue::Shell;
  rr64_mk64_item_audio_step(m);
  mix();
  check(energy(output, 0) > 100000 && energy(output, 1) == 0,
        "fallen listener camera turn reverses stereo");
  write_u16(m, globals::gameplay_pause_state, 1);
  rr64_mk64_item_audio_step(m);
  mix();
  check(energy(output, 0) + energy(output, 1) == 0,
        "native pause mutes without advancing item sound");
  write_u16(m, globals::gameplay_pause_state, 0);
  course_active = false;
  rr64_mk64_item_audio_step(m);
  mix();
  check(audio_statistics().voices == 0,
        "stock course transition retires item sound");
  clear_audio_bank();
  check(!audio_available(), "unload disables item audio");
  std::printf("MK64 item audio: %u checks passed; original extraction, bounded "
              "mixer and presentation verified offline.\n",
              checks);
}
