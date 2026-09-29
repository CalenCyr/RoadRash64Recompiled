#include "rr64_campaign_bonus_save.hpp"
#include "rr64_engine_layout.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

extern "C" void func_8001F960(unsigned char *, recomp_context *);
extern "C" void func_80020ECC(unsigned char *, recomp_context *);
extern "C" void func_8005F480(unsigned char *, recomp_context *);
extern "C" void test_bonus_native_save(unsigned char *, recomp_context *);
extern "C" void test_bonus_native_read(unsigned char *, recomp_context *);

namespace {
using namespace rr64::engine;
constexpr unsigned profile = 0x800D6A40u, cache = 0x800C07C8u,
                   buffer = 0x800C0470u;
constexpr unsigned stack = 0x807FE000u, status = 0x8009DE08u;
using File = std::array<unsigned char, 256>;
std::array<File, 6> files{};
std::array<bool, 6> exists{};
unsigned io_slot = 0, write_error = 0, read_error = 0, scan_error = 0;
unsigned writes = 0, reads = 0, accepts = 0, checks = 0;

void check(bool value, const char *message) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "Campaign bonus save: %s\n", message);
    std::exit(1);
  }
}
unsigned word(unsigned char *m, unsigned address) {
  unsigned result = 0;
  read_u32(m, address, result);
  return result;
}
std::vector<unsigned char> bytes(unsigned char *m, unsigned address,
                                 unsigned size) {
  std::vector<unsigned char> result(size);
  for (unsigned i = 0; i < size; ++i)
    read_u8(m, address + i, result[i]);
  return result;
}
void copy(unsigned char *m, unsigned destination, unsigned source,
          unsigned size) {
  const auto data = bytes(m, source, size);
  for (unsigned i = 0; i < size; ++i)
    write_s8(m, destination + i, data[i]);
}
void native_checksum(unsigned char *m, unsigned record) {
  write_u32(m, record + 4, 0);
  recomp_context ctx{};
  ctx.r4 = guest_address(record);
  func_8001F960(m, &ctx);
  write_u32(m, record + 4, unsigned(ctx.r2));
}
void seed(unsigned char *m, unsigned level, unsigned identity) {
  rr64_campaign_bonus_new_profile(m, profile);
  for (unsigned i = 0; i < 0xF8; i += 4)
    write_u32(m, profile + i, 0xA5000000u + identity * 0x100u + i);
  write_u32(m, profile, 4);
  write_u32(m, profile + 0x20, identity % 5);
  write_u32(m, profile + 0x3C, level);
  write_u32(m, profile + 0x50, 0x12345678u);
  native_checksum(m, profile);
}
unsigned bonus(unsigned char *m) {
  return rr64_campaign_qualification_word(m, profile + 0x54,
                                          word(m, profile + 0x54));
}
void save(unsigned char *m, unsigned slot) {
  io_slot = slot;
  recomp_context ctx{};
  ctx.r29 = guest_address(stack);
  write_u32(m, stack + 0x74, profile);
  write_u32(m, buffer - 4, 0x11223344u);
  write_u32(m, buffer + 256, 0x55667788u);
  test_bonus_native_save(m, &ctx);
  check(unsigned(ctx.r16) == write_error, "native Pak write result retained");
  check(word(m, buffer - 4) == 0x11223344u &&
            word(m, buffer + 256) == 0x55667788u,
        "256-byte serialization preserves neighboring memory");
}
unsigned load(unsigned char *m, unsigned slot) {
  // Native load checks cached slot availability before performing a rescan.
  write_u32(m, status + slot * 4, 2);
  write_u32(m, 0x800C0450u, 0);
  recomp_context ctx{};
  ctx.r29 = guest_address(stack);
  ctx.r4 = slot;
  ctx.r5 = guest_address(profile);
  func_80020ECC(m, &ctx);
  return unsigned(ctx.r2);
}
} // namespace

// Model only the Pak device. The actual generated code chooses byte counts,
// clears its file buffer, calculates checksums, copies profiles and checks
// errors.
extern "C" void func_8000C3AC(unsigned char *m, recomp_context *ctx) {
  ++writes;
  check(unsigned(ctx->r7) == 256, "native write requests exactly256bytes");
  const unsigned source = word(m, unsigned(ctx->r29) + 0x10);
  check(source == buffer, "native write uses its original staging buffer");
  if (write_error == 0) {
    for (unsigned i = 0; i < 256; ++i)
      read_u8(m, source + i, files[io_slot][i]);
    exists[io_slot] = true;
  }
  ctx->r2 = write_error;
}
extern "C" void func_8000C2AC(unsigned char *m, recomp_context *ctx) {
  ++reads;
  check(unsigned(ctx->r7) == 256, "native read requests exactly256bytes");
  const unsigned destination = word(m, unsigned(ctx->r29) + 0x10);
  check(destination == buffer, "native read uses its original staging buffer");
  if (read_error == 0 && exists[io_slot])
    for (unsigned i = 0; i < 256; ++i)
      write_s8(m, destination + i, files[io_slot][i]);
  ctx->r2 = read_error != 0 ? read_error : exists[io_slot] ? 0 : 1;
}
extern "C" void func_8001F990(unsigned char *, recomp_context *) {}
extern "C" void rr64_campaign_restore_unlocks(unsigned char *, unsigned) {
  ++accepts;
}
extern "C" void func_800207DC(unsigned char *m, recomp_context *ctx) {
  rr64_campaign_bonus_scan(m);
  if (scan_error != 0) {
    ctx->r2 = scan_error;
    return;
  }
  for (io_slot = 0; io_slot < 6; ++io_slot) {
    if (!exists[io_slot])
      continue;
    // Empty stock cache is its first-read checksum baseline. Subsequent
    // scans retain the original base record exactly, as production does.
    recomp_context read{};
    read.r29 = guest_address(stack - 0x200);
    read.r23 = guest_address(0x800C0570u);
    read.r19 = guest_address(0x800C0590u);
    write_s8(m, unsigned(read.r29) + 0x18,
             static_cast<unsigned char>('A' + io_slot));
    write_u32(m, unsigned(read.r29) + 0x40, 0x800C0440u);
    test_bonus_native_read(m, &read);
  }
  ctx->r2 = read_error;
}

int main() {
  std::vector<unsigned char> memory(kRdramSize);
  auto *m = memory.data();

  // Full nibble range survives native I/O, including signed high-bit words.
  for (unsigned slot = 0; slot < 6; ++slot) {
    seed(m, 5, slot + 1);
    const unsigned results = 0xFEDCBA98u - slot * 0x11111111u;
    const unsigned reputation = word(m, profile + 0x54);
    check(rr64_campaign_qualification_store(m, profile + 0x54, results) ==
              reputation,
          "bonus writes preserve original reputation word");
    const auto live = bytes(m, profile, 0xF8);
    save(m, slot);
    check(bytes(m, profile, 0xF8) == live,
          "native save preserves live profile and index5");
    check(word(m, buffer + 0x3C) == 4,
          "serialized profile keeps backward-compatible index4");
    for (unsigned i = 0; i < 0xF8; ++i) {
      if ((i >= 4 && i < 8) || (i >= 0x3C && i < 0x40))
        continue;
      check(files[slot][i] == live[i],
            "every original result, inventory and statistic survives");
    }
    // An original build copies only248bytes and still receives the exact
    // completed original campaign. No extra profile word is overwritten.
    copy(m, 0x80300000u, buffer, 0xF8);
    check(word(m, 0x8030003Cu) == 4 && word(m, 0x80300050u) == 0x12345678u &&
              word(m, 0x80300054u) == reputation,
          "old-style profile copy remains valid completed main campaign");
    seed(m, 0, 99);
    check(load(m, slot) == 1, "native selected load succeeds");
    check(word(m, profile + 0x3C) == 5 && bonus(m) == results,
          "selected slot restores its full independent bonus word");
    check(word(m, profile + 0x54) == reputation, "load preserves reputation");
  }

  // Each track has all16 native counter values, including zeros and caps.
  for (unsigned track = 0; track < 8; ++track)
    for (unsigned count = 0; count < 16; ++count) {
      seed(m, 5, 7);
      const unsigned results = count << (track * 4);
      rr64_campaign_qualification_store(m, profile + 0x54, results);
      save(m, 0);
      seed(m, 0, 98);
      check(
          load(m, 0) == 1 && word(m, profile + 0x3C) == 5 &&
              bonus(m) == results,
          "all eight full4bit counters roundtrip through actual native copies");
    }

  // Base saves are accepted byte-for-byte; merely reading never adds a tail.
  seed(m, 4, 2);
  const auto original = bytes(m, profile, 0xF8);
  save(m, 0);
  const File legacy = files[0];
  check(word(m, buffer + 0xF8) == 0 && word(m, buffer + 0xFC) == 0,
        "ordinary save retains native zero padding");
  seed(m, 0, 98);
  const unsigned saved_writes = writes;
  check(load(m, 0) == 1 && bytes(m, profile, 0xF8) == original,
        "old completed save loads without profile conversion");
  check(writes == saved_writes && files[0] == legacy,
        "load never writes or upgrades save automatically");

  seed(m, 5, 4);
  rr64_campaign_qualification_store(m, profile + 0x54, 0x10203040u);
  save(m, 0);
  const File valid = files[0];
  // Corrupt every bit of the extension; preserve valid base campaign and
  // reject bonus activation, including corruption in the integrity word.
  for (unsigned bit = 0; bit < 64; ++bit) {
    files[0] = valid;
    files[0][0xF8 + bit / 8] ^= 1u << (bit % 8);
    seed(m, 0, 98);
    check(load(m, 0) == 1 && word(m, profile + 0x3C) == 4,
          "corrupt extension falls back to completed original campaign");
    check(rr64_campaign_bonus_record(m, cache) == 0,
          "invalid tail never claims bonus unlock");
  }
  files[0] = valid;
  for (unsigned i = 0xF8; i < 256; ++i)
    files[0][i] = 0;
  seed(m, 0, 98);
  check(load(m, 0) == 1 && word(m, profile + 0x3C) == 4,
        "truncated or old-stylezero tail keeps main progress intact");

  // A valid base checksum is not enough to attach another profile's tail.
  // Change cash, recompute the original checksum, and retain the old tail.
  for (unsigned i = 0; i < 256; ++i)
    write_s8(m, buffer + i, valid[i]);
  write_u32(m, buffer + 0x38, word(m, buffer + 0x38) + 123);
  native_checksum(m, buffer);
  for (unsigned i = 0; i < 256; ++i)
    read_u8(m, buffer + i, files[0][i]);
  seed(m, 0, 98);
  check(load(m, 0) == 1 && word(m, profile + 0x3C) == 4,
        "native-checksummed changed profile cannot borrow another bonus tail");

  files[0] = valid;
  seed(m, 0, 98);
  check(load(m, 0) == 1 && bonus(m) == 0x10203040u, "valid cache primed");
  check(rr64_campaign_bonus_record(m, cache) == 1,
        "validated slot exposes bonus unlock");
  write_u32(m, cache + 0x38, word(m, cache + 0x38) + 1);
  check(rr64_campaign_bonus_record(m, cache) == 0,
        "slot cache binds to exact profile contents");
  rr64_campaign_bonus_scan(m);
  check(rr64_campaign_bonus_record(m, cache) == 0,
        "new Pak scan clears extension cache");
  check(rr64_campaign_bonus_record(m, cache + 1) == 0 &&
            rr64_campaign_bonus_record(m, cache + 6 * 0xF8) == 0,
        "unaligned and out-of-range slots rejected");

  // Failed writes do not alter the existing file; failed loads do not erase
  // the current unsaved bonus progress or reuse a previous slot extension.
  seed(m, 5, 5);
  rr64_campaign_qualification_store(m, profile + 0x54, 0x76543210u);
  const auto unsaved = bytes(m, profile, 0xF8);
  files[0] = valid;
  write_error = 7;
  save(m, 0);
  check(files[0] == valid && bytes(m, profile, 0xF8) == unsaved &&
            bonus(m) == 0x76543210u,
        "failed native write preserves disk and live profile");
  write_error = 0;
  scan_error = 8;
  check(load(m, 0) == 8 && bytes(m, profile, 0xF8) == unsaved &&
            bonus(m) == 0x76543210u,
        "failed scan leaves current unsaved bonus unchanged");
  check(rr64_campaign_bonus_record(m, cache) == 0,
        "failed scan cannot reuse cached tail");
  scan_error = 0;
  read_error = 9;
  check(load(m, 0) == 9 && bytes(m, profile, 0xF8) == unsaved &&
            bonus(m) == 0x76543210u,
        "failed native read leaves current unsaved bonus unchanged");
  read_error = 0;

  // The original profile initializer invokes the reset and clears counters;
  // changing RDRAM mappings also cannot borrow another session's progress.
  recomp_context fresh{};
  fresh.r4 = guest_address(profile);
  func_8005F480(m, &fresh);
  write_u32(m, profile + 0x3C, 5);
  check(bonus(m) == 0, "native New Game clears bonus progress");
  rr64_campaign_qualification_store(m, profile + 0x54, 0xFFFFFFFFu);
  std::vector<unsigned char> other(kRdramSize);
  write_u32(other.data(), profile + 0x3C, 5);
  check(bonus(other.data()) == 0,
        "new memory mapping has no stale bonus progress");
  check(rr64_campaign_qualification_word(nullptr, profile + 0x54, 123) == 123,
        "null mapping remains a native passthrough");
  for (unsigned level = 0; level < 5; ++level) {
    seed(m, level, level);
    const unsigned address = profile + 0x40 + level * 4;
    check(rr64_campaign_qualification_word(m, address, 0xA5A55A5Au) ==
                  0xA5A55A5Au &&
              rr64_campaign_qualification_store(m, address, 0x01020304u) ==
                  0x01020304u,
          "all original campaign qualification loads and stores pass through");
  }

  std::printf("Campaign bonus save: %u checks passed; %u native writes, %u "
              "native reads, %u base acceptances\n",
              checks, writes, reads, accepts);
}
