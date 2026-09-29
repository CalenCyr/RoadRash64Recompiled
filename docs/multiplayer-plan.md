# Multiplayer and course ownership in 1.4.2

Online remains experimental. Preserve original local multiplayer behavior unless
a change explicitly requires altering it. The active implementation is the
host-authoritative runtime in native/src/rr64_netplay.cpp and the authoritative
step/prediction helpers. Earlier relay/lobby prototypes are not foundations for
new work. Match the protocol constants in source; 1.4.2 uses protocol 65.

- The host owns shared game settings, race state and pause. Each participant
  selects their own rider/bike and uses their own full-screen presentation.
- Local race presets and single-player Thrash have separate persisted state;
  share option semantics without enabling split-screen ownership for solo play.
- MK64 Music is remembered across imported races within those presets. Online
  uses the host's session choice. Earlier per-course enabled bits migrate to
  On; changing the setting keeps the existing packet layout.
- Imported course selection is synchronized by stable course/pack identity.
  Missing, disabled or mismatched packs must be rejected before racing. Every
  participant converts their own ROM locally; the host does not supply assets.
- Offline cheats must remain suppressed during every active online session,
  including transition/failure paths. Overlay state alone is not an authority
  boundary; preserve the native guards too.
- Highlights replay recorded presentation and must not rerun physics, damage,
  inventory grants or race progression. The host owns online highlight controls.
- Proximity voice follows the rider body, including ejection. Do not treat bike
  position or a HUD viewport as the speaking body's location.

See RELEASE_CANDIDATE_EDITING_GUIDE.md for feature ownership. Offline tests do
not establish WAN smoothness, every race mode or the full larger-player-count
matrix. Keep packet/session evidence private and separate from source exports.

MK64 item awards/hits are host-authoritative; local weapon HUD focus does not mutate inventory.
Private prediction must omit live rendering/audio callbacks. See mk64-items.md
for the cycle input boundary, effect snapshots and per-view rendering contracts.
Recent movement/audio/highlight/menu corrections remain experimental.
