#define SDL_MAIN_HANDLED
#include "rr64_controller_mappings.hpp"
#include "recompinput/input_binding.h"
#include "recompinput/profiles.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Stub only profile storage/assignment; exercise the real binding implementation.
static recompinput::InputField menu_binding{};
static int expected_profile = 7;
namespace recompinput::profiles {
int get_input_profile_for_player(int player, InputDevice) { return player == 2 ? expected_profile : 0; }
InputField& get_input_binding(int profile, GameInput, size_t) {
    if (profile != expected_profile) std::abort();
    return menu_binding;
}
void set_input_binding(int, GameInput, size_t, InputField) {}
}
namespace recompinput::players {
bool is_single_player_mode() { return false; }
const Player& get_player(int, bool) { static Player player; return player; }
}

static unsigned checks;
static void check(bool good, const char* what) {
    ++checks;
    if (!good) { std::fprintf(stderr, "FAIL: %s (%s)\n", what, SDL_GetError()); std::exit(1); }
}

int main() {
    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    check(SDL_Init(SDL_INIT_GAMECONTROLLER) == 0, "input initialization");
    check(rr64_register_controller_mappings(), "mapping registration");
    const std::string platform = std::string("platform:") + SDL_GetPlatform() + ",";
    for (const char* mapping : rr64_controller_mappings) {
        if (!std::strstr(mapping, platform.c_str())) continue;
        std::string guid_text(mapping, 32);
        char* registered = SDL_GameControllerMappingForGUID(SDL_JoystickGetGUIDFromString(guid_text.c_str()));
        check(registered != nullptr, "exact device GUID recognized");
        SDL_free(registered);

        // A synthetic joystick has its own GUID. Keep the upstream raw layout
        // unchanged while replacing only that identity for the test device.
        int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 6, 15, 1);
        check(index >= 0, "virtual raw device attached");
        char guid[33]{};
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index), guid, sizeof(guid));
        const std::string fixture = std::string(guid) + (mapping + 32);
        check(SDL_GameControllerAddMapping(fixture.c_str()) >= 0, "virtual mapping");
        auto* controller = SDL_GameControllerOpen(index);
        check(controller != nullptr, "virtual controller recognized");
        auto* joystick = SDL_GameControllerGetJoystick(controller);
        const struct { int raw; SDL_GameControllerButton mapped; } buttons[] = {
            {0, SDL_CONTROLLER_BUTTON_A}, {1, SDL_CONTROLLER_BUTTON_B},
            {6, SDL_CONTROLLER_BUTTON_LEFTSHOULDER}, {7, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER},
            {10, SDL_CONTROLLER_BUTTON_BACK}, {11, SDL_CONTROLLER_BUTTON_START},
            {12, SDL_CONTROLLER_BUTTON_GUIDE}, {13, SDL_CONTROLLER_BUTTON_LEFTSTICK}
        };
        for (auto button : buttons) {
            SDL_JoystickSetVirtualButton(joystick, button.raw, 1); SDL_JoystickUpdate();
            check(SDL_GameControllerGetButton(controller, button.mapped) == 1, "button press");
            SDL_JoystickSetVirtualButton(joystick, button.raw, 0); SDL_JoystickUpdate();
            check(SDL_GameControllerGetButton(controller, button.mapped) == 0, "button release");
        }
        for (int axis = 0; axis < 4; ++axis) {
            // Windows raw axes 2 is unused; Linux/macOS use contiguous XY pairs.
            int raw = axis >= 2 && platform == "platform:Windows," ? axis + 1 : axis;
            for (Sint16 value : {Sint16(-32767), Sint16(32767), Sint16(0)}) {
                SDL_JoystickSetVirtualAxis(joystick, raw, value); SDL_JoystickUpdate();
                check(SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis(axis)) == value,
                    "stick/C direction and neutral");
            }
        }
        for (int trigger = 0; trigger < 2; ++trigger) {
            SDL_JoystickSetVirtualButton(joystick, 8 + trigger, 1); SDL_JoystickUpdate();
            check(SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis(4 + trigger)) > 30000, "Z trigger press");
            SDL_JoystickSetVirtualButton(joystick, 8 + trigger, 0); SDL_JoystickUpdate();
            check(SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis(4 + trigger)) == 0, "Z trigger release");
        }
        SDL_JoystickSetVirtualHat(joystick, 0, SDL_HAT_RIGHTUP); SDL_JoystickUpdate();
        check(SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP) &&
              SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT), "diagonal D-pad");
        SDL_GameControllerClose(controller);
        check(SDL_JoystickDetachVirtual(index) == 0, "disconnect");
    }
    using namespace recompinput;
    binding::start_scanning(2, GameInput::A, 0, InputDevice::Controller);
    menu_binding = {InputType::Keyboard, SDL_CONTROLLER_BUTTON_A};
    check(!binding::is_cancel_button(SDL_CONTROLLER_BUTTON_A), "keyboard ID cannot cancel button");
    menu_binding = {InputType::ControllerAnalog, SDL_CONTROLLER_BUTTON_A};
    check(!binding::is_cancel_button(SDL_CONTROLLER_BUTTON_A), "axis ID cannot cancel button");
    menu_binding = {InputType::ControllerDigital, SDL_CONTROLLER_BUTTON_BACK};
    check(binding::is_cancel_button(SDL_CONTROLLER_BUTTON_BACK), "selected profile cancels");
    check(!binding::is_cancel_button(SDL_CONTROLLER_BUTTON_A), "other button bindable");
    binding::start_scanning(2, GameInput::TOGGLE_MENU, 0, InputDevice::Controller);
    check(!binding::is_cancel_button(SDL_CONTROLLER_BUTTON_BACK), "menu button can be rebound");
    binding::stop_scanning();
    check(!binding::is_cancel_button(SDL_CONTROLLER_BUTTON_BACK), "inactive scan");
    SDL_Quit();
    std::printf("Controller mapping/binding checks passed: %u\n", checks);
}
