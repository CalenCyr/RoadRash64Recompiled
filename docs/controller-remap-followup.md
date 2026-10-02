# Assigned controller layouts and remapping

The assigned-profile routing described in
[controller-profile-routing.md](controller-profile-routing.md) is extended by
persistent layout association and synchronized editor/scanner ownership.

Choosing a saved layout updates both the current player assignment and the
controller's remembered association. Registration reconstructs the runtime
identity from stored device fields after a restart. Reassigning another device
preserves the existing local player slots' selected layouts. Controllers with
identical stored identity fields and no serial cannot be distinguished after
reconnection; current player-slot selections still remain separate.

A binding scan stays attached to its original device/profile. Reassignment
must not let a pending scan edit the newly assigned profile. The Controls
editor refreshes its displayed layout when assignment changes. Polling resolves
the intended player's profile before translating SDL input into native N64 bits.

Loaded user bindings are not rewritten by the obsolete startup C-button
migration. Fresh and reset profiles obtain their defaults from the default
table. The right stick supplies the default C actions; R3 retains the dedicated
spoke-jam action without also cycling weapons. Keep gameplay actions separate
from the overlay's tab-navigation bindings.

Digital shoulders and analog triggers have distinct binding types. On an SDL
recognized DualSense, L1/R1 are shoulders and L2/R2 are trigger axes. Fresh
defaults assign native N64 L to the left shoulder, R to the right trigger, and
Z to the left trigger. The right shoulder has the dedicated weapon-trick action.
The common SDL route does not need a controller-model-specific swap.

Relevant frontend sources are `recompinput/src/players.cpp`, `profiles.cpp`,
`input_binding.cpp` and the Controls editor/scanner implementation. Export changes
through the pinned RecompFrontend patch. `RR64ControllerRemapSmoke` and
`RR64ControllerUISmoke` exercise production profile/polling code, virtual SDL
controllers, extracted editor methods and separate persistence processes.
These checks do not replace a physical-controller or rendered-menu test.
