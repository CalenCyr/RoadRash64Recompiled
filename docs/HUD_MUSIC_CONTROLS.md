# HUD and music controls candidate

HUD classification now uses actual disjoint split-camera rectangles from the
submitted framebuffer pair. Supports horizontal/vertical halves, quadrants and
mixed half/quadrant three-player arrangements. Local HUD edges map to the
corresponding global framebuffer boundary, and clipping keeps anchored elements
inside their viewport. Name-tail anchors reset between regions. Ambiguous layouts
fall back to prior behavior; this is not yet verified on live 3/4-player captures.
Single fullscreen online cameras do not imply split HUD from player count alone.

Music Volume (saved under the existing custom_music_volume key) controls original
and custom soundtracks. Effects retain their own volume. Original volume is
scaled at submission without overwriting its authored fades. Per-sequence change
tracking updates crossfades with bounded state. Shared volume/mixing tests pass.

Compile-time HUD classification/anchor checks and full game build pass. Existing
aspect/FPS controls, Draw Distance and weapon fix retained. Live HUD acceptance
and original/custom audible comparison pending. Nothing published.
Wait for ready before launch. Test HUD Placement settings in 2, 3 and 4 screens.
