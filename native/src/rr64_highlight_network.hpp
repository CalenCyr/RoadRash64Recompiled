#pragma once
#include "rr64_highlight_recording.hpp"
#include <functional>
#include <optional>
#include <vector>

namespace rr64::highlight_network {
constexpr std::size_t maximum_raw_bytes = 96u * 1024u * 1024u;
constexpr std::size_t maximum_compressed_bytes = 32u * 1024u * 1024u;
constexpr unsigned chunk_bytes = 960, maximum_datagram = 1000;
struct Blob {
    std::vector<std::uint8_t> bytes;
    std::uint32_t raw_size = 0;
    std::uint64_t checksum = 0;
};
struct Playlist {
    std::array<std::vector<highlights::Frame>, highlights::maximum_clips> frames;
    std::array<highlights::Clip, highlights::maximum_clips> clips;
    unsigned count = 0;
    Playlist() = default;
    Playlist(Playlist &&) = default;
    Playlist &operator=(Playlist &&) = default;
    Playlist(const Playlist &) = delete;
    Playlist &operator=(const Playlist &) = delete;
    std::span<const highlights::Clip> view() const {
        return {clips.data(), count};
    }
};
// Called outside the network lock, only after the race has ended. Explicit
// little-endian fields omit inactive records and unused bone/pose tails.
bool encode(std::span<const highlights::Clip>, Blob &) noexcept;
bool decode(const Blob &, Playlist &) noexcept;

enum class Stage : unsigned { Idle, Transferring, Waiting, Playing, Finished };
struct Status {
    Stage stage = Stage::Idle;
    unsigned waiting_mask = 0;
    std::uint64_t elapsed_us = 0, duration_us = 0;
    bool received = false;
};
class Channel {
  public:
    using Send = std::function<void(unsigned, std::span<const std::uint8_t>)>;
    Channel();
    ~Channel();
    Channel(Channel &&) noexcept;
    Channel &operator=(Channel &&) noexcept;
    Channel(const Channel &) = delete;
    Channel &operator=(const Channel &) = delete;
    void reset(bool host, unsigned round);
    bool publish(Blob, std::uint64_t duration_us, std::uint64_t now_us);
    // peer is authenticated by the enclosing existing session/endpoint checks.
    void receive(unsigned peer, std::span<const std::uint8_t>, std::uint64_t now_us,
                 std::uint64_t one_way_us = 0);
    void service(std::uint64_t now_us, unsigned connected_mask, const Send &);
    std::optional<Blob> take_received();
    void decoded(bool accepted);
    bool begin(std::uint64_t now_us, unsigned connected_mask);
    bool skip();
    Status status(std::uint64_t now_us, unsigned connected_mask) const;

  private:
    struct State;
    std::unique_ptr<State> state_;
};
}
