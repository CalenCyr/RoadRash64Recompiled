#pragma once

extern "C" void rival_native_traffic_doppler(unsigned char *, recomp_context *);

namespace {
float traffic_doppler_reference(unsigned source, unsigned listener,
                                bool detached = false) {
  // Feed the actual original traffic instruction sequence through its native
  // actor-zero listener contract. Private scratch data has no audio samples.
  constexpr unsigned traffic = 0x807D0000, listening_bike = 0x807D0100;
  const unsigned position_address =
      detached ? body(listener) + 0x8C
               : bike(listener) + rr64::engine::bike::body_position;
  const unsigned velocity_address =
      detached ? body(listener) + 0x98 : bike(listener) + 0x178;
  for (unsigned axis = 0; axis < 3; ++axis) {
    put(traffic + 0xA8 + axis * 4,
        word(bike(source) + rr64::engine::bike::body_position + axis * 4));
    put(traffic + 0xB4 + axis * 4, word(bike(source) + 0x178 + axis * 4));
    put(listening_bike + 0x16C + axis * 4, word(position_address + axis * 4));
    put(listening_bike + 0x178 + axis * 4, word(velocity_address + axis * 4));
  }
  const unsigned original_bike = word(actor(0) + 0xE0);
  put(actor(0) + 0xE0, listening_bike);
  auto c = context();
  c.r19 = rr64::engine::guest_address(traffic);
  rival_native_traffic_doppler(m, &c);
  put(actor(0) + 0xE0, original_bike);
  return c.f0.fl;
}

float engine_pitch(unsigned slot) {
  const unsigned sound = row_for(word(cache(slot)));
  check(sound != 0, "Doppler source retains an owned native engine");
  return value(sound + 0x30);
}

float ordinary_engine_pitch(unsigned slot = 12) {
  put(actor(slot) + 8, slot);
  call(stock_func_800571DC, bike(slot));
  return engine_pitch(slot);
}

void pitch_matches(unsigned source, float native, float expected,
                   const char *label) {
  check(std::abs(engine_pitch(source) - (native + expected)) < .0001f, label);
}

void test_rival_doppler_cases() {
  // Compare steady-loop pitch to the untouched local engine, then compare
  // motion offsets to original traffic math rather than the C++ adaptation.
  for (float source_speed : {-150.f, -60.f, 0.f, 60.f, 150.f}) {
    reset();
    position(1, 8, 0);
    vec(bike(1) + 0x178, source_speed, 0);
    const float native = ordinary_engine_pitch();
    call(func_800571DC, bike(12));
    pitch_matches(12, native, 0,
                  "unscoped local engine retains original pitch");
    const float expected = traffic_doppler_reference(1, 0);
    frames();
    pitch_matches(1, native, expected,
                  "rival Doppler matches original traffic projection");
    check(source_speed == 0 || expected * source_speed < 0,
          "approaching rises and receding falls");
    const unsigned sound = word(cache(1));
    frames(60);
    check(word(cache(1)) == sound, "Doppler updates preserve sound handle");
    pitch_matches(1, native, expected,
                  "Doppler does not accumulate across frames");
  }

  reset();
  position(1, 8, 4, 30);
  vec(bike(0) + 0x178, 100, 20, -50);
  vec(bike(1) + 0x178, 100, 20, 80);
  const float parallel_native = ordinary_engine_pitch();
  frames();
  pitch_matches(1, parallel_native, 0,
                "parallel bikes retain authored RPM despite vertical motion");
  vec(bike(1) + 0x178, -100, 20, 80);
  frames();
  pitch_matches(
      1, parallel_native, traffic_doppler_reference(1, 0),
      "Doppler keeps native horizontal projection for elevated source");

  // A reversal is smoothed, while corrupt and coincident motion immediately
  // returns to the original pitch and never silences a valid engine.
  reset();
  position(1, 8, 0);
  vec(bike(1) + 0x178, -100, 0);
  const float native = ordinary_engine_pitch();
  frames();
  const float before = engine_pitch(1);
  vec(bike(1) + 0x178, 100, 0);
  const float after = native + traffic_doppler_reference(1, 0);
  frame(.001f);
  check(engine_pitch(1) < before && engine_pitch(1) > after,
        "pass-by pitch reversal is smoothed");
  frames();
  pitch_matches(1, native, after - native,
                "smoothed Doppler reaches receding target");
  scalar(bike(1) + 0x178, NAN);
  frame();
  pitch_matches(1, native, 0,
                "invalid source velocity safely retains original engine pitch");
  vec(bike(1) + 0x178, -100, 0);
  position(1, 0, 0);
  frame();
  pitch_matches(
      1, native, 0,
      "coincident source and listener has no divide or Doppler spike");
  position(1, 8, 0);
  vec(bike(1) + 0x178, -100000, 0);
  frames(30);
  pitch_matches(1, native, traffic_doppler_reference(1, 0),
                "extreme motion obeys native upper clamp");
  vec(bike(1) + 0x178, 100000, 0);
  frames(30);
  pitch_matches(1, native, traffic_doppler_reference(1, 0),
                "extreme motion obeys native lower clamp");

  reset();
  position(1, 100, 0);
  vec(bike(1) + 0x178, -100, 0);
  const float teleport_native = ordinary_engine_pitch();
  frames();
  position(0, 180, 0);
  frame();
  pitch_matches(1, teleport_native, 0,
                "listener teleport resets stale pass-by pitch");
  frames();
  pitch_matches(1, teleport_native, traffic_doppler_reference(1, 0),
                "listener teleport establishes new Doppler direction");
  position(1, 200, 0);
  frame();
  pitch_matches(1, teleport_native, 0,
                "source teleport starts without stale pitch");

  // During a crash, position AND velocity come from the body. A fast bike
  // left behind cannot skew the pitch heard by a stationary fallen rider.
  reset();
  position(1, 8, 0);
  vec(bike(1) + 0x178, -80, 0);
  vec(bike(0) + 0x178, 100, 0);
  const float fallen_native = ordinary_engine_pitch();
  shortword(body(0) + rr64::engine::rider::bike_attached, 0);
  vec(body(0) + 0x8C, 0, 0);
  vec(body(0) + 0x98, 0, 0);
  frames();
  pitch_matches(1, fallen_native, traffic_doppler_reference(1, 0, true),
                "fallen listener uses own body velocity");
  vec(body(0) + 0x98, -80, 0);
  frames();
  pitch_matches(1, fallen_native, 0,
                "body moving with rival has no relative Doppler");

  // One shared split-screen mix uses the same strongest listener for all
  // positional properties, and a listener handoff resets its old offset.
  reset(16, 2);
  position(0, 0, 0);
  position(1, 200, 0);
  position(2, 20, 0);
  vec(bike(0) + 0x178, 30, 0);
  vec(bike(1) + 0x178, -30, 0);
  const float split_native = ordinary_engine_pitch();
  frames();
  pitch_matches(2, split_native, traffic_doppler_reference(2, 0),
                "shared mix Doppler uses nearest local listener");
  position(0, 400, 0);
  position(1, 25, 0);
  frame();
  pitch_matches(2, split_native, 0,
                "split listener handoff clears previous offset");
  frames();
  pitch_matches(2, split_native, traffic_doppler_reference(2, 1),
                "shared mix switches Doppler with gain and pan listener");

  for (bool replicated : {false, true}) {
    reset(16, 4);
    rules.active = rules.connected = true;
    rules.phase = rr64::netplay::Phase::Race;
    rules.local_slot = 3;
    rules.replicated_riders = replicated;
    const unsigned local = replicated ? 0 : 3, remote = replicated ? 1 : 0;
    position(local, 0, 0);
    position(remote, 8, 0);
    vec(bike(remote) + 0x178, -90, 0);
    const float online_native = ordinary_engine_pitch();
    frames();
    pitch_matches(remote, online_native,
                  traffic_doppler_reference(remote, local),
                  "online Doppler uses this peer's mapped listener");
    const auto snapshot = memory;
    auto c = context();
    c.f20.fl = 3.5f;
    const auto registers = c;
    {
      rr64::prediction::ReplayScope replay;
      rr64_rival_engine_frame(m, &c);
      rr64_rival_engine_pitch(m, &c);
    }
    check(memory == snapshot && std::memcmp(&c, &registers, sizeof(c)) == 0,
          "private prediction cannot change live Doppler or native pitch");
    frames();
    pitch_matches(
        remote, online_native, traffic_doppler_reference(remote, local),
        "private prediction leaves subsequent live Doppler unchanged");
  }
}
} // namespace
