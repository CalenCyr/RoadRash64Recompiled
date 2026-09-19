# Pickup motion versus HUD anchoring

## Health-fill separation follow-up

The user confirmed pickup positioning works in AF99D753, but the green health
fills no longer aligned with their HUD frames. The world-depth exclusion remains
necessary; depth alone is not a reliable indicator of the original draw's purpose.
Native HUD producer 80030220 also emits health fills with mode 0x00552230.

The config hooks now clear only depth compare/write bits (0x30) after that mode
is assembled at 80031DBC and 80032448, yielding 0x00552200. These are the single
view/online and two-player health-fill producers. Blending, colors, bar values,
and frames are preserved. World sprite producers keep their original depth mode
and remain excluded from automatic HUD anchoring. This separates the two uses
at their source instead of weakening the pickup guard. Three/four-player native
paths are unchanged.

Production compilation, 205 HUD widget assertions, sky-filter smoke checks, and
dependency patch reverse check passed. Generated code was inspected to confirm
both hooks run after mode assembly and before command emission. These are offline
checks, not proof of visual correctness.

Prepared, not launched: `pending-hud-producer-separation` under the private
`analysis/release-1.2-online/sky-review-20260919` folder.
SHA256: 0F9B63D9ECD33BB2C1574FBA584F871193872351CB5A5924823A29057D6F11A5.
Next check: health fills remain inside frames while approaching/collecting pickups
in split screen. Await the user's next ready signal before launching.

## Earlier diagnosis

September 19: user reports the HUD is much better in origin-correction build
12D19006. New report: red spray-can pickup, and apparently most pickups, shift
toward their actual position when approached. Two still screenshots do not prove
the motion mechanism or an LOD transition.

Code audit found automaticHUDBounds accepted depth-tested/depth-writing rectangle,
orthographic and raw-triangle draws. Native world sprites can be projected by the
CPU in 8001EF8C (matrix/divide at 1F1BC..1F294, viewport conversion at 1F2E8..1F3F0)
before their sprite draw. A screen-space projection is therefore not proof of HUD
ownership. Bounds-based corner classification could change their horizontal
anchor or lower-view vertical inset as their apparent size/position changes.
Native sprite drawing in 1CFB8 uses the record flag 0x10 to select depth-bearing
render modes (1D0D8 onward); this is distinct from model LOD selection.

Correction: reject draws with either depth compare or depth write enabled before
HUD bounds extraction. Do not change pickup simulation, collision, asset selection,
camera projection or LOD. This prevents a demonstrable classifier hazard; we have
not yet identified the pictured pickup's exact runtime draw and depth flags.
Do not claim the user's symptom is definitively fixed until visually checked.

Verification: 40 cases pass using extracted production bounds/classifier code,
including the same rectangle at different sizes with HUD versus world depth modes.
Full production build and dependency patch reverse check pass. No game launched.
Stage: analysis/release-1.2-online/sky-review-20260919/pending-pickup-hud-guard
SHA256: AF99D7537EDC6BA764ECBA9D69D7991A2764BBAC3C2860BB6CE2CDB448D7E236.
Next focused check: approach the same pickups and verify their visual position
stays attached to the road, while split-screen HUD remains as accepted.
