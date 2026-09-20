#pragma once
#include <SDL.h>
#include <string>

// SDL 2.30.3 HIDAPI Switch driver, full-state report, N64 subtype 12:
// C-Up=Y (button3), C-Left=X (button2), C-Right=Minus (button4),
// C-Down=ZR (axis5). Physical ZR is the stick-click bit (button8).
// The stock SDL N64 map treats Minus as menu Back and ZR as a trigger.
// Normalize C directions to the same right-stick fields as our D-mode maps,
// without changing the controller's raw driver, saved profiles or other pads.
// Sources: libsdl-org/SDL release-2.30.3 SDL_hidapi_switch.c,
// SDL_gamecontroller.c; torvalds/linux drivers/hid/hid-nintendo.c N64 table.
namespace rr64::input {
inline constexpr const char* n64_hidapi_mapping =
    "Nintendo N64 Controller,a:b0,b:b1,dpdown:b12,dpleft:b13,dpright:b14,dpup:b11,"
    "guide:b5,leftshoulder:b9,leftstick:b7,lefttrigger:a4,leftx:a0,lefty:a1,"
    "rightshoulder:b10,righttrigger:b8,start:b6,misc1:b15,"
    "-rightx:b2,+rightx:b4,-righty:b3,+righty:a5,";

inline bool is_n64_hidapi_guid(SDL_JoystickGUID guid) {
    Uint16 vendor = 0, product = 0;
    SDL_GetJoystickGUIDInfo(guid, &vendor, &product, nullptr, nullptr);
    // Raw evdev/DInput GUIDs use other layouts even with this same VID/PID.
    return vendor == 0x057e && product == 0x2019 && guid.data[14] == 'h' && guid.data[15] == 12;
}

inline int register_n64_hidapi_mapping(SDL_JoystickGUID guid) {
    if (!is_n64_hidapi_guid(guid)) return 0;
    char text[33]{};
    SDL_JoystickGetGUIDString(guid, text, sizeof(text));
    const std::string mapping = std::string(text) + "," + n64_hidapi_mapping;
    return SDL_GameControllerAddMapping(mapping.c_str()) < 0 ? -1 : 1;
}
}
