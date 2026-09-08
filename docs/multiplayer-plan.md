# In-Game Direct-Connect Multiplayer Plan

## Decision

Replace the previously attempted online multiplayer implementation with one clean, in-game direct-connect system. The old launcher/overlay online flow, centralized relay service, matchmaking/public-lobby flow, lobby codes, and experimental Direct-IP path are legacy and should be removed before the replacement is integrated.

No multiplayer or game-code removal is part of this notes-only change.

## Required player flow

**Main Menu → Multiplayer → Online → Host / Join → Lobby → Host Game/Race Setup → Per-Player Character/Bike Select → Race**

Online setup and session progression belong inside the game, not in the launcher.

The host is authoritative while the stock game-type and race-option screens are active. Connected clients mirror those choices and cannot change them independently. Once the host confirms the setup, every peer regains control on the stock character/bike screen for its own rider.

## Network model

- Prefer a host/client direct-connect design over centralized matchmaking or relay servers.
- One player hosts; the others connect directly to that host.
- Keep the network lobby separate from the original local-player selection structures where practical.
- The lobby must show connected players, ping, and ready state.
- Keep the lobby UI/data model scalable beyond four players even though the first working target is four-player online.
- Expand the online player limit later only if the recompiled game's player, race, camera, UI, and related structures can safely support it.
- Preserve existing local multiplayer while replacing only the attempted online implementation.

## Implementation order for future work

1. Audit and remove the previous online multiplayer hooks, launcher/overlay UI, relay-server integration, matchmaking/lobby-code paths, and experimental Direct-IP path.
2. Verify the base recompilation still builds and runs normally, including local multiplayer.
3. Add the in-game Online menu and Host / Join flow.
4. Add the separate network lobby with connected-player, ping, and ready-state handling.
5. Integrate host-authoritative game/race setup, per-player Character/Bike Select, and Race synchronization for an initial four-player online session.
6. Evaluate higher player counts only after the four-player implementation is stable and the underlying game structures have been validated.

## Initial scope boundary

The first milestone is stable four-player, host/client direct-connect multiplayer. Centralized servers, public matchmaking, relay-backed lobbies, and a higher player limit are not part of that milestone.

## Implementation status (v0.8.0)

- The launcher and settings-overlay online entries have been removed.
- The relay server, lobby-code, public matchmaking, Quick Play, proximity-voice, 14-player override, and experimental remote-transform implementation have been removed from the active project.
- Selecting the original main-menu **Multiplayer** option now opens an in-game choice between **Local Multiplayer** and **Online**.
- **Local Multiplayer** resumes the original game mode and controller setup without network changes.
- **Online** provides direct **Host** and **Join** actions, followed by a separate four-slot network lobby with player names, ping, and ready state.
- When the host begins, each peer enters the original multiplayer character-selection, track-selection, and race path. Network-assigned controller slots carry each peer's input through those stock screens and gameplay.
- Protocol-v2 automated tests validate four simultaneous peers, slot assignment, ready/phase propagation, and all four synchronized controller streams without loading the game.

Runtime UI and gameplay validation is intentionally pending until the user asks to launch the build. Direct internet hosting may require the host's UDP port to be reachable; NAT traversal and relays remain outside this direct-connect milestone.

## Experimental 14-rider checkpoint (post-v0.8.0)

The higher-player-count evaluation requested after the four-player checkpoint is now implemented as a separate network mode, while the original local multiplayer path and the proven two-to-four-player controller-stream path remain intact.

- The in-game lobby and protocol now admit fourteen network peers. Network slots are separate from the N64's four controller ports.
- Sessions with two to four players retain protocol-v2's controller-stream behavior. A host that begins with five or more connected players latches replicated-rider mode for that session.
- Static analysis verified that Road Rash allocates fourteen native bike records and fourteen native rider records. Replicated mode uses those existing pools instead of inventing extra controller ports.
- Protocol v3 adds authenticated per-slot rider proposals and host-distributed canonical race snapshots. Snapshot sequencing, finite-value validation, interpolation history, disconnect cleanup, and a sub-fragmentation packet-size limit are in place.
- The game-side bridge keeps each machine's locally controlled rider in native entity zero and maps the other canonical network slots onto distinct native bike entities. It synchronizes the bike body and both wheel transforms before simulation and again before rendering.
- A headless host plus thirteen clients now passes admission, ready/phase propagation, and all-fourteen-rider snapshot validation. The normal two-peer compatibility test also passes.

This is an experimental gameplay checkpoint, not a stable 14-player release. Live testing still must verify stock character/track flow with more than four peers, rider/bike visual binding, collision and combat behavior, race results, cameras, late disconnects, and recovery. Direct-connect/NAT requirements are unchanged.

## Host-authoritative setup checkpoint (protocol v4)

- **Begin Game Setup** now enters the original multiplayer setup screens with only the host controlling game type, race, track, and related options.
- Every client mirrors the host's controller stream during setup. The host's finalized setup words are then captured, versioned, and distributed in the canonical lobby snapshot.
- The stock transition into character/bike selection is detected directly from the game's multiplayer stage. Clients apply the host's finalized settings before their own controls are enabled.
- After that boundary, each connected peer controls its own assigned stock selector in the two-to-four-player path. The experimental higher-player path maps each peer's local selector to its own network rider slot.
- Returning to the main menu and selecting Multiplayer again now tears down the previous online socket, protocol phase, and pending stock-menu transition before opening a fresh Local/Online choice.
- Live multiplayer rendering and race synchronization recognize all four gameplay-state pairs in the stock dispatcher, including the Thrash-specific path, so every game type receives the same widescreen projection handling.
- Original local multiplayer bypasses all of these online hooks and retains the game's normal controller and menu behavior.

## Race identity and proximity voice checkpoint (protocol v5)

- The display name entered in the in-game Host / Join screen is now carried into the stock multiplayer race presentation. The four stock `Player 1`-style HUD strings are replaced at their original initialization boundary; original local multiplayer names remain untouched.
- Online races publish the local bike position in both the two-to-four-player controller-stream mode and the experimental replicated-rider mode. This presentation-only snapshot supplies voice distance without changing physics, collision, or AI.
- Every direct-connect race renders one full-screen camera for the local peer. Two-to-four-player sessions retain every synchronized controller stream for simulation while selecting only the peer's assigned camera; replicated-rider sessions retain their local-rider-in-viewport-zero mapping. Stock split screen remains exclusive to Local Multiplayer.
- Race-only proximity voice uses 48 kHz mono Opus frames over the authenticated direct-connect session. Clients send only to the host; the host validates the assigned speaker slot and sequence, rate-limits packets, and relays them to the remaining peers.
- Nearby riders are heard at full volume and fade smoothly to silence at long range. Decoded audio is bounded and stale out-of-range speech is discarded.
- A **Proximity Voice Chat** switch is available in the recompilation Audio settings. The microphone is closed in lobbies, menus, single player, and whenever the setting is disabled.
- Protocol-v5 headless tests validate two-peer bidirectional voice relay alongside the original lobby/input/setup flow. The fourteen-peer admission and canonical-rider test continues to pass.

## R33 candidate corrections (2026-09-06)

The user requests finalizing music and online play, a selectable maximum of fourteen players, and reports host-only crash at online race finish with one joiner. The tested folder is unknown; retain this issue for the next test and preserve the host log.

Protocol 6 adds the original A6680/A6690 race-option words to finalized host setup. Setup controller presence no longer hides remote ports; only the host provides setup actions. Online Player Limit on the connect page cycles 2..14 and gates host admission. EF5C is the four-controller menu count and remains <=4 (one for replicated mode); A6574 is the actual rider pool and can reach fourteen. This separation replaces the previous unsafe experimental write of fourteen into the menu count. A6578 retains the physical human count. The larger roster still requires live gameplay validation.

The online camera plan now also checks actual live mode/pending mode, and transition logs capture settings, role, network slot and counts. This is a candidate correction, not proof that the reported host finish crash is solved. Preserve original local multiplayer behavior. Both peers must use protocol 6/R33 for validation.

## R38 connection candidate (2026-09-06)

The user reports R36 remaining on Connecting. R38 retains the direct-connect design and protocol 6 gameplay structures, adding a 15-second join deadline, a five-second host-loss deadline, explicit full/started/version rejection, trimmed IPv4/hostname:port input, exclusive host UDP binding, and sparse connection diagnostics. Initial welcome validation prevents delayed welcome packets from rewinding connected sessions. Receive processing is capped at 256 datagrams per update.

Local multiplayer, player limits, game setup and rider replication are preserved. No legacy relay, matchmaking or overlay lobby is restored. Loopback transport tests can verify these changes but cannot establish internet reachability. Test both peers on R38 and retain both runtime logs. The available R36 log contained no online attempt; the original WAN block and previously reported host finish crash remain unconfirmed.

## Local settings setup candidate (2026-09-07)

The user requested local controller/name setup in the launcher/settings overlay before choosing Local Multiplayer. The candidate adds four controller cards with saved in-game names, automatic connected-device assignment, optional keyboard enrollment and four-port limits. Local selection resumes the stock race menu directly. Online names/input remain separate. See docs/LOCAL_PLAYER_SETUP.md. Offline checks pass; live multi-controller validation is pending a new ready. No publication authorized.
