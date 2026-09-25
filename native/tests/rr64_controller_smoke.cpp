#define SDL_MAIN_HANDLED
#include "rr64_controller_mappings.hpp"
#include "recompinput/input_binding.h"
#include "recompinput/profiles.h"
#include "recompinput/rr64_n64_switch_mapping.h"
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
size_t get_number_of_assigned_players() { return 0; }
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
        if (std::strstr(mapping, ",NSO N64 Controller,")) continue; // Exercised separately below.

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

    // Independent raw C-button fixtures: Windows USB, Linux/macOS raw HID,
    // and the Linux kernel's N64-specific evdev button ordering.
    struct SwitchLayout { const char* guid; int left, right, up, down; };
    const SwitchLayout layouts[] = {
        {"030000007e05000019200000000000",3,8,7,2},
        {"030000007e05000019200000010000",3,8,2,7},
        {"030000007e05000019200000118100",4,2,10,3},
        {"050000007e05000019200000010000",3,8,2,7},
        {"050000007e05000019200000018000",4,2,10,3},
    };
    for (const char* mapping : rr64_controller_mappings) {
        if (!std::strstr(mapping, platform.c_str()) || !std::strstr(mapping, ",NSO N64 Controller,")) continue;
        const SwitchLayout* layout = nullptr;
        for (const auto& candidate : layouts) if (std::strncmp(mapping, candidate.guid, 28) == 0) layout = &candidate;
        check(layout != nullptr, "Switch layout fixture identified");
        int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 6, 16, 1);
        check(index >= 0, "Switch virtual device attached");
        char guid[33]{};
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index), guid, sizeof(guid));
        check(SDL_GameControllerAddMapping((std::string(guid)+(mapping+32)).c_str()) >= 0, "Switch fixture mapped");
        auto* controller=SDL_GameControllerOpen(index);
        check(controller != nullptr, "Switch fixture opened");
        auto* joystick=SDL_GameControllerGetJoystick(controller);
        const int buttons[]={layout->left,layout->right,layout->up,layout->down};
        for (int i=0;i<4;++i) {
            const auto axis=i<2?SDL_CONTROLLER_AXIS_RIGHTX:SDL_CONTROLLER_AXIS_RIGHTY;
            SDL_JoystickSetVirtualButton(joystick,buttons[i],1);SDL_JoystickUpdate();
            const int value=SDL_GameControllerGetAxis(controller,axis);
            check(i%2?value>30000:value< -30000,"Switch C press recognized");
            check(!SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_BACK),"C input does not become menu cancel");
            SDL_JoystickSetVirtualButton(joystick,buttons[i],0);SDL_JoystickUpdate();
            check(SDL_GameControllerGetAxis(controller,axis)==0,"Switch C release");
        }
        // Generic raw HID has a spare right-stick-click report slot b11;
        // Linux's N64-specific kernel layout instead uses b11 for Start.
        if (layout->left == 3) {
            SDL_JoystickSetVirtualButton(joystick,11,1);SDL_JoystickUpdate();
            check(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_LEFTSTICK)==1,"raw stick-click exposed as L3");
            check(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)==0,"L3 is independent of ZR");
            check(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_RIGHTX)==0 && SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_RIGHTY)==0,"L3 does not press C directions");
            SDL_JoystickSetVirtualButton(joystick,11,0);SDL_JoystickUpdate();
            check(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_LEFTSTICK)==0,"L3 release");
        } else {
            SDL_JoystickSetVirtualButton(joystick,11,1);SDL_JoystickUpdate();
            check(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_START)==1 && !SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_LEFTSTICK),"kernel Start is not mislabeled L3");
        }
        SDL_GameControllerClose(controller);SDL_JoystickDetachVirtual(index);
    }

    // HIDAPI reports are already decoded by SDL; its raw order is different
    // from all OS joystick layouts above, even when VID/PID are identical.
    auto hid_guid=SDL_JoystickGetGUIDFromString("030000007e050000192000000000680c");
    check(rr64::input::is_n64_hidapi_guid(hid_guid),"HIDAPI N64 identity");
    check(rr64::input::register_n64_hidapi_mapping(hid_guid)==1,"HIDAPI mapping registered");
    for (int field : {4,8,14,15}) {
        auto other=hid_guid;other.data[field]^=1;
        check(!rr64::input::is_n64_hidapi_guid(other),"other driver/product/subtype untouched");
    }
    int hid_index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN,6,20,0);
    check(hid_index>=0,"HIDAPI virtual device attached");
    char hid_text[33]{};
    SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(hid_index),hid_text,sizeof(hid_text));
    check(SDL_GameControllerAddMapping((std::string(hid_text)+","+rr64::input::n64_hidapi_mapping).c_str())>=0,"HIDAPI fixture mapped");
    auto* hid=SDL_GameControllerOpen(hid_index);check(hid!=nullptr,"HIDAPI fixture opened");
    auto* raw=SDL_GameControllerGetJoystick(hid);
    SDL_JoystickSetVirtualAxis(raw,4,-32768);SDL_JoystickSetVirtualAxis(raw,5,-32768);SDL_JoystickUpdate();
    check(SDL_GameControllerGetAxis(hid,SDL_CONTROLLER_AXIS_RIGHTY)==0,"C-Down neutral");
    for (int button : {2,3,4}) {
        SDL_JoystickSetVirtualButton(raw,button,1);SDL_JoystickUpdate();
        int value=SDL_GameControllerGetAxis(hid,button==3?SDL_CONTROLLER_AXIS_RIGHTY:SDL_CONTROLLER_AXIS_RIGHTX);
        check(button==4?value>30000:value< -30000,"HIDAPI C direction");
        check(!SDL_GameControllerGetButton(hid,SDL_CONTROLLER_BUTTON_BACK),"HIDAPI C is not Back");
        SDL_JoystickSetVirtualButton(raw,button,0);SDL_JoystickUpdate();
    }
    SDL_JoystickSetVirtualAxis(raw,5,32767);SDL_JoystickUpdate();
    check(SDL_GameControllerGetAxis(hid,SDL_CONTROLLER_AXIS_RIGHTY)>30000,"C-Down from ZR report bit");
    check(SDL_GameControllerGetAxis(hid,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)==0,"C-Down does not activate physical ZR");
    SDL_JoystickSetVirtualAxis(raw,5,-32768);SDL_JoystickSetVirtualButton(raw,8,1);SDL_JoystickUpdate();
    check(SDL_GameControllerGetAxis(hid,SDL_CONTROLLER_AXIS_RIGHTY)==0,"C-Down released");
    check(SDL_GameControllerGetAxis(hid,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)>30000,"physical ZR retained");
    SDL_GameControllerClose(hid);SDL_JoystickDetachVirtual(hid_index);
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
