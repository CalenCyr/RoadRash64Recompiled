# Local Multiplayer with no assigned controller

The September 26 report describes a selectable Multiplayer menu that ignores
navigation and Back. The reporter's device/OS has not been confirmed. Source
tracing found a matching failure for keyboard-only players: the main menu uses
shared solo input, but Local Multiplayer switched unconditionally to the
assigned-player roster. Keyboard enrollment is explicit, so a fresh keyboard
setup can have zero assigned players. Both the device query and input callback
then report all four guest ports disconnected.

Local entry now retains the shared port-one input path when the assigned roster
is empty. This keeps the existing solo keyboard bindings, including remapped A
and B. Gameplay still enters Local Multiplayer, with its custom race settings
and AI roster. If one or more devices are assigned, their existing per-player
input routing and profiles are retained. No device cards are created or moved.
Returning to the main menu uses the existing ownership reset.

The native menu already clamps ordinary race modes to one through four humans
according to controller presence (`func_80027A90`, `0x80027D7C..0x80027E0C`).
It can therefore run one human against AI without inventing a second controller.
Original mode-specific player requirements remain intact.

The local-player assignment/name smoke test passes after the change. The menu
refresh fixture also runs the complete production UI-update function, guest
device-query callback and selected-profile resolver. Zero-, one-, two- and
four-assignment starts preserve the expected ports and bindings, including a
custom solo keyboard mapping and an explicitly assigned keyboard profile.
It passes 500,124 checks; a negative control restoring the former unconditional
port switch fails because it loses the only usable controller. This is source
and headless verification; the exact user-reported setup still needs a live
confirmation. No game was launched. The follow-up visual check is one
keyboard-only fresh setup: enter Multiplayer, change race options, use Back,
then select one human with AI and start a race. Existing assigned-pad split
screen should continue to use independent controls.
