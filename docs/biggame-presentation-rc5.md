# Big Game menu framing and highlight detail

RC4 owner testing reported horizontal clipping on the Bike Shop screen at a new
Big Game, plus occasional coarse NPC models in crash highlights with Max LOD on.
The reported executable and graphics settings matched the prepared RC4 build.

## Bike Shop preview

The shop calls the shared native model preview with one preview. That branch
selects layout zero and immediately rebuilds its viewport, but the layout cache
can still contain the dimensions of an earlier high-resolution framebuffer.
The stale model scissor then expands the renderer's accumulated rectangle even
when subsequent text commands use current menu dimensions.

Refresh the dimension cache only in the single-preview branch, before its
paired layout and viewport calls. Do not put the refresh in the general layout
helper: race startup can reuse a prepared camera and skip the viewport rebuild.
The fixture generator checks the actual generated single-count branch and rejects
unsafe placement. No HUD, pickup or renderer aspect rule changes are needed.

## Highlight recording

Detailed pose preparation previously inherited the live camera's distance
admission, and completely hidden bike nodes could skip the pair observer. A
later cinematic camera could consequently move close to a coarse recorded NPC.

Recording now has an explicit purpose distinct from live drawing. Its bounded
roster observation occurs before the visibility loop, for viewport zero, while
the original camera inputs are available. Preparation still uses validated
allocations, actor ownership and original animation evaluation in private memory.
The recording retains detailed model graphs together with matching child poses;
changing only the model index would make coarse bones incompatible.

Recording-only data does not bypass the live draw certificate. Existing draw
distance checks and camera limits remain in force. Invalid ownership/resources
retain a valid fallback; this does not make missing animation data safe to use.
No gameplay physics, network format, terrain or importer changes are involved.

## Verification and limits

Private evidence is stored under `analysis/biggame-presentation-20260925-c40`.
The menu fixture exercises original native preview/layout/scissor instructions,
repeated video transitions, other preview counts and race reuse controls, with
the old missing-refresh path as a negative control. Highlight checks cover
distant and hidden actors, frame/view ownership and live-memory preservation.
The release manifest binds the final source, generated hooks and build tests.

These are offline checks. Owner visual acceptance of the packaged RC5 remains
pending. The c39 converter and corrected RC4 map pack are unchanged: RC4 users
do not need to reimport their tracks for these presentation fixes.
