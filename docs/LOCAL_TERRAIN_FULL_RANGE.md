# Full-map split-screen terrain slider

Settings > Graphics > Split-screen Terrain Distance: 0–100%, step 5%.
0 preserves original terrain. 100 removes the distance window and restores
full-map terrain extension for the current course. Frustum and disconnected
course-island filtering remain. Intermediate percentages scale the radius to
the farthest allowed terrain bounds from the camera. This is distance percentage,
not percentage of triangles or performance. Motion retains a 25% margin;
changing the slider clears the old retention for immediate range reduction.

Requires MAX World Distance enabled before Start Game. The slider applies live.
A new saved key avoids interpreting the old 0–12000 units as a percentage;
initial value is 0. Set 100, then lower it until performance is acceptable.
Only local 2–4-view terrain is affected. Weapon fix remains. Single-view and
online behavior remain unchanged; this does not extend roadside objects.

Offline terrain driver checks passed. User accepted the previous adjustable
version and requested this full-map endpoint. Live acceptance pending.
Nothing published. Wait for ready before launch.
