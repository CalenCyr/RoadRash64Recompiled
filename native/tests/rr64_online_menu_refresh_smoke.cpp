// Runs the production update_ui body extracted by CMake. UI doubles preserve
// deferred text updates and shown-only draining, the lifecycle that previously
// let a hidden lobby accumulate text allocations throughout an offline race.
// This checks control flow and queue bounds, not Rml rendering or live transport.
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace fixture {
unsigned checks = 0;
void require(bool condition, const char* message) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "Online menu refresh: %s\n", message);
        std::exit(1);
    }
}
int current_context = 2; // Caller context must be restored after each update.
bool shown = false;
unsigned text_writes = 0, status_reads = 0, shutdowns = 0;
unsigned resets = 0, queued_entries = 0, focus_passes = 0, rumble_stops = 0;
bool single_player = true, all_ready = false;
}

namespace recompui {
enum class Display { None, Flex };
struct Element {
    std::string text;
    bool enabled = true;
    Display display = Display::Flex;
    void set_text(std::string value);
    void set_enabled(bool value) { enabled = value; }
    void set_display(Display value) { display = value; }
};
using Label = Element;
using Button = Element;
std::vector<std::pair<Element*, std::string>> pending_text;
void Element::set_text(std::string value) {
    fixture::require(fixture::current_context == 1, "text must belong to the open lobby context");
    ++fixture::text_writes;
    pending_text.emplace_back(this, std::move(value));
}
struct ContextId {
    int id = 0;
    static ContextId null() { return {}; }
    bool operator!=(ContextId other) const { return id != other.id; }
    void open() const {
        fixture::require(fixture::current_context == 0, "context opened without closing its predecessor");
        fixture::current_context = id;
    }
    void close() const {
        fixture::require(fixture::current_context == id, "wrong context closed");
        fixture::current_context = 0;
    }
    void set_autofocus_element(Element*) const {}
};
ContextId try_close_current_context() {
    ContextId result{fixture::current_context};
    fixture::current_context = 0;
    return result;
}
bool is_context_shown(ContextId) { return fixture::shown; }
void show_context(ContextId, const char*) { fixture::shown = true; }
void hide_context(ContextId) { fixture::shown = false; }
void drain_visible() {
    if (!fixture::shown) return;
    for (auto& [element, text] : pending_text) element->text = std::move(text);
    pending_text.clear();
}
}

namespace recompinput {
void suspend_all_rumble() { ++fixture::rumble_stops; }
namespace players {
void set_single_player_mode(bool value) { fixture::single_player = value; }
}
}
namespace rr64 {
namespace local_players { std::atomic_bool active = false; }
namespace race_pack { enum class Compatibility { Ready, Pending, Missing, Disabled, Different }; }
namespace netplay {
constexpr std::uint8_t kMaximumPlayers = 14;
enum class Phase { Offline, Lobby, GameSetup, CharacterSelect, Race };
struct PlayerInfo {
    bool connected = false, ready = false;
    std::string name;
    unsigned ping_ms = 0;
    std::uint8_t slot = 0;
    race_pack::Compatibility course_compatibility = race_pack::Compatibility::Ready;
};
struct Status {
    bool active = false, connected = false, is_host = false;
    Phase phase = Phase::Offline;
    std::uint8_t local_slot = 255;
    std::string message = "Offline";
    std::array<PlayerInfo, kMaximumPlayers> players{};
    struct { bool valid = false; } game_setup;
};
Status state;
Status get_status() { ++fixture::status_reads; return state; }
void shutdown() { ++fixture::shutdowns; state = {}; }
void configure(Status value) { state = std::move(value); }
bool all_connected_players_ready() { return fixture::all_ready; }
const char* course_compatibility_message(race_pack::Compatibility) { return "Fixture pack state"; }
}
namespace online_menu {
enum class Page { Connect, Lobby };
struct UiState {
    recompui::ContextId context{1};
    recompui::Label* lobby_status = nullptr;
    recompui::Label* course_error = nullptr;
    std::array<recompui::Label*, netplay::kMaximumPlayers> player_labels{};
    recompui::Button *ready_button = nullptr, *continue_button = nullptr, *disconnect_button = nullptr;
    Page page = Page::Connect;
    std::optional<Page> pending_page;
    recompui::Element* pending_focus = nullptr;
    unsigned focus_delay_frames = 0;
    bool initialized = false, online_entry_queued = false, course_blocked = false;
};
UiState g_ui;
std::atomic_bool g_reset_local_requested = false, g_reset_session_requested = false;
std::atomic_bool g_force_multiplayer_transition = false, g_allow_original_multiplayer = false;
std::atomic_bool g_local_start_requested = false, g_show_requested = false;
std::atomic_bool g_return_to_lobby_requested = false;
bool has_course_problem(const netplay::Status&) { return false; }
void online_log(const char*, ...) {}
const char* phase_name(netplay::Phase) { return "PHASE"; }
void reset_guest_setup_progress() { ++fixture::resets; }
void queue_original_multiplayer() { ++fixture::queued_entries; }
void set_page(Page page) { g_ui.page = page; }
void apply_pending_page() {
    if (g_ui.pending_page) { set_page(*g_ui.pending_page); g_ui.pending_page.reset(); }
}
void restore_page_focus_if_ready() { ++fixture::focus_passes; }

#include "rr64_online_menu_update_fixture.inc"
}
}

int main() {
    using namespace rr64;
    using namespace online_menu;
    using fixture::require;
    std::array<recompui::Label, 14> labels;
    recompui::Label lobby;
    recompui::Button ready, continue_button, disconnect;
    g_ui.lobby_status = &lobby;
    g_ui.ready_button = &ready;
    g_ui.continue_button = &continue_button;
    g_ui.disconnect_button = &disconnect;
    for (unsigned i = 0; i < labels.size(); ++i) g_ui.player_labels[i] = &labels[i];
    update_ui();
    require(fixture::status_reads == 0 && fixture::current_context == 2,
        "uninitialized UI must be a no-op");
    g_ui.initialized = true;
    for (unsigned frame = 0; frame < 100'000; ++frame) {
        update_ui();
        require(recompui::pending_text.empty(), "hidden offline lobby queued a text update");
        require(fixture::current_context == 2, "hidden update lost the caller context");
    }
    require(fixture::text_writes == 0 && fixture::resets == 100'000,
        "hidden refresh must retain offline setup cleanup without allocating text");

    // Show requests are handled before visibility: latest names/ready state
    // must be queued on that same frame, with no empty or stale first frame.
    netplay::state.active = netplay::state.connected = netplay::state.is_host = true;
    netplay::state.phase = netplay::Phase::Lobby;
    netplay::state.local_slot = 0;
    netplay::state.players[0] = {true, true, "Host", 0};
    netplay::state.players[13] = {true, false, "Last Rider", 47};
    fixture::all_ready = true;
    g_show_requested = true;
    update_ui();
    require(fixture::shown && recompui::pending_text.size() == 17,
        "show request must repaint all lobby labels immediately");
    recompui::drain_visible();
    require(labels[13].text.find("Last Rider") != std::string::npos &&
        labels[13].text.find("47 ms") != std::string::npos &&
        labels[0].text.find("(YOU)") != std::string::npos,
        "shown lobby lost name, ping, or local identity");
    require(ready.text == "NOT READY" && ready.enabled && continue_button.enabled,
        "shown ready/continue state changed");

    // A joining peer can progress while its overlay is hidden. Never place
    // these transport-owned transitions behind the visibility optimization.
    fixture::shown = false;
    netplay::state.phase = netplay::Phase::GameSetup;
    const auto old_entries = fixture::queued_entries;
    update_ui();
    update_ui();
    require(fixture::queued_entries == old_entries + 1 && g_ui.online_entry_queued,
        "hidden GameSetup must queue exactly one original-menu transition");
    require(recompui::pending_text.empty(), "hidden GameSetup accumulated text");
    netplay::state = {};
    update_ui();
    require(!g_ui.online_entry_queued, "offline teardown must clear online-entry state");

    // Guest menu teardown/re-entry is consumed while hidden, including local
    // multiplayer input assignment and return-to-single-player requests.
    g_ui.pending_page = Page::Lobby;
    g_reset_session_requested = true;
    g_local_start_requested = true;
    const auto old_shutdowns = fixture::shutdowns;
    update_ui();
    require(fixture::shutdowns == old_shutdowns + 2 && !g_ui.pending_page &&
        local_players::active && !fixture::single_player,
        "hidden local start or session teardown was skipped");
    g_reset_local_requested = true;
    update_ui();
    require(!local_players::active && fixture::single_player && fixture::rumble_stops >= 2,
        "hidden local cleanup was skipped");

    // Reopening after a hidden session must paint the newly connected peer,
    // and disconnected/not-ready state still redirects the pending focus.
    netplay::state.active = true;
    netplay::state.phase = netplay::Phase::Lobby;
    netplay::state.players[1] = {true, false, "Replacement", 31};
    g_show_requested = true;
    g_ui.pending_focus = &ready;
    update_ui();
    recompui::drain_visible();
    require(labels[1].text.find("Replacement") != std::string::npos &&
        labels[13].text.find("EMPTY") != std::string::npos,
        "reopening retained stale hidden-session text");
    require(g_ui.pending_focus == &disconnect && !ready.enabled,
        "visible disconnected lobby lost its focus recovery");
    require(fixture::current_context == 2 && fixture::focus_passes > 100'000,
        "focus processing or caller context restoration was skipped");
    std::printf("Online menu refresh passed: %u checks, 100000 hidden production updates.\n", fixture::checks);
    return 0;
}
