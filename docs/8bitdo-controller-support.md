# 8BitDo 64 input support (post-1.3 candidate)

The report identifies an N64-style 8BitDo controller, probably on Windows;
the exact model, connection and firmware mode are not confirmed. The user linked
https://shop.8bitdo.com/products/8bitdo-64-2-4g-wireless-controller.
Do not equate that link with a verified USB identity.

## Implemented

`native/src/rr64_controller_mappings.hpp` registers the upstream 8BitDo 64
SDL mappings before processing input events. Source: SDL_GameControllerDB
commit `5a12daa568d19344f9b6e9286ef5929833b25c7c`; license retained in the header.
Mappings match exact GUIDs, with explicit platform filtering: Windows and macOS
can share a GUID but use different raw axis numbering. These mappings expose
the C buttons as right-stick directions. No blanket mapping is applied to other
8BitDo products or unidentified receiver modes.

The frontend remapping cancel check now uses the selected player's actual profile
and requires a digital controller binding. Previously it read profile zero and
compared numeric IDs even for keyboard/axis bindings. The menu-toggle button itself
can be rebound; Escape still cancels. Existing saved controls are preserved.

Opt-in `RR64_DIAGNOSTICS=1` device-added records now include GUID, VID/PID,
raw button/axis/hat counts and the selected SDL mapping. No per-frame input logging
was added. A controller not recognized by SDL still needs its device information;
this change does not implement a universal raw-joystick mapper.

## Setup and limits

Assign the controller in Controls, then use Customize Controls. The common default
layout is still intended for a modern dual-stick controller. On the upstream N64
mapping, physical B is SDL B, whereas the common game default for braking is SDL X:
remap Brake/B and menu Back to physical B. Map shoulders and optional dedicated
actions to the available buttons as desired. C inputs can appear as right-stick
directions in the binding labels; this is expected for this device mapping.

Windows production build and 46 ROM-free checks passed. The smoke test uses the
real SDL DLL and actual binding cancellation code, exercising raw virtual-device
buttons, C/steering axes, trigger press/release, diagonal D-pad and removal. It
checks registration by the real GUID, substituting only a virtual GUID for input
injection. It does not emulate receiver firmware or prove physical-device support.
No game launched, no Linux build tested and no release package published for this
change. Reporter acceptance with the actual controller is pending.
