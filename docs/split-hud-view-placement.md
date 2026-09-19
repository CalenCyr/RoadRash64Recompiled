# Split-screen HUD placement candidate

The cloud-free test showed inconsistent HUD anchors: the upper player's speed
display stayed inset while the lower player's health/weapon cluster stayed
inset and touched the divider. Existing HUD classification used full framebuffer
dimensions, so those widgets fell outside the Big Game corner regions.

The renderer now discovers split regions from perspective projection scissors
in the submitted workload. HUD bounds are classified relative to the containing
region. Local left/right anchors map to that region's actual framebuffer edges;
four-player inner edges map to the center instead of the full-screen outside
edges. Long-name tail grouping has separate history per view. Lower-row upper
clusters receive the native Big Game top margin, and anchored draw scissors
are clipped to the owning region. The obsolete sky-size heuristic is removed.
The world projection and simulation are not modified. Full-screen online uses
its rendered view, not online peer count, to select the layout.

This remains a coordinate-classification implementation, not explicit widget
identity tagging. Unexpected native widget bounds may need follow-up. The
private opt-in `RR64_HUD_LAYOUT_TRACE=1` records at most 160 HUD bounds/regions
on the renderer thread to verify activation during the next visual test.

Validation: production build passes; 28 offline checks using the actual
classifier and view math pass for two-player screenshot-shaped coordinates,
four quadrants, center/outside offsets and full-screen behavior. The cloud
removal fixture and reverse dependency-patch validation also pass. No two- or
four-player visual acceptance is claimed yet. Verify bars, frame, weapon/rank,
nameplate backgrounds/text, speed, pause and race messages together.
# Follow-up: whole widgets

The visual test exposed separated nameplate backings and a shared countdown
across the divider. See [native widget placement](hud-widget-placement.md) for
the follow-up candidate and its remaining visual checks. The coordinate tests
below alone did not validate complete-widget behavior.
