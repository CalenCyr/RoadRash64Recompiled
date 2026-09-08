# Consolidated Draw Distance candidate

Settings > Graphics > Draw Distance: 0–100%, step 5%, default 100%.
Removes both MAX World Distance and Maximum View Distance controls.
Draw Distance above 0 automatically enables the safe original terrain tier;
0 restores original tier selection and disables cached geometry extension.
No additional distance switch is required. MAX LOD remains independent.
New persisted key rr64_draw_distance avoids reusing the old local-only setting.

Terrain and cached roadside objects share terrain-cell admission in all supported
race views, including single player, local multiplayer and online presentation.
100% draws the full current-course extension; 0% draws no extra cached terrain
or scenery. Intermediate values reduce distance with a 25% retention margin.
Original stock rendering and simulation lifecycles remain intact. Unclassified
extra objects are omitted below 100%. No guarantee of zero pop-in, no fading,
and no changes to entity spawning or online synchronization.

The setting applies live. Full distance can be costly, especially split-screen.
The per-view weapon fix is retained. Offline terrain and object driver suites
pass, including four-viewport scenery, disabled distance, geometry allocation
limits, course masks and stock deduplication. Live acceptance pending.
Nothing published. Wait for ready before launching.
