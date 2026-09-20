# 8BitDo 64 Switch-mode mappings

Exact raw mappings are pinned to SDL_GameControllerDB 5a12daa568d19344f9b6e9286ef5929833b25c7c. The narrowly matched HIDAPI override is in the RecompFrontend dependency overlay. It applies only to Nintendo N64 VID/PID 057e:2019 and HIDAPI subtype 12; generic raw and Linux kernel layouts are distinct. C directions normalize to right-stick axes. UI layout and profiles are preserved.

Revision 2 includes the reporter-tested controller candidate. The Linux Bluetooth evdev device exposes 13 buttons, two axes and one hat. Its kernel N64 table has no separate thumb button. The generic raw mapping additionally exposes spare b11 as L3; that physical assignment is unconfirmed and must never be copied to the kernel layout, where b11 is Start. HIDAPI remains unchanged from the first candidate.

Windows: 87 offline checks. Linux: 161 offline checks. The Linux reporter confirmed button configuration except L3. No claim of universal firmware, transport or physical controller support follows from virtual tests.

Optional troubleshooting: RR64_DIAGNOSTICS=1 logs controller identity and mapping; RR64_CONTROLLER_BUTTON_TRACE=1 separately logs up to 256 raw/mapped joystick button events. These are off by default. Press the failing button first, followed by known-working controls. No keyboard or continuous-axis trace is collected. The event budget may end before the session ends.

Primary references: SDL release-2.30.3 src/joystick/hidapi/SDL_hidapi_switch.c; Linux drivers/hid/hid-nintendo.c; SDL_GameControllerDB pinned above. See native/tests/rr64_controller_smoke.cpp for press/release, cancellation, identity guard and input independence coverage.
