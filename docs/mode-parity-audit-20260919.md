# Shared fixes audit — September 19, 2026

Scope: active release-1.2-online source only. This is a code-path and focused
regression audit, not proof that a live online race is synchronized.

| Area | Big Game | Local split screen | Online | Evidence / limits |
| --- | --- | --- | --- | --- |
| Invalid previous-attacker assist credit | Shared native guard | Same guard | Same guard, including generated prediction | 72 native-branch cases; no changes to damage or knockdown |
| Windows guest allocation | Shared runtime | Shared runtime | Shared runtime | Reserve4GiB/commit512MiB; no per-mode branch |
| Achievement persistence | Background worker | Same worker | Same worker | File I/O removed from UI; timing gain not measured |
| Rider/weapon detail | Shared actor/attachment hooks | Per-view scope1..4 | Local presentation uses shared hooks and online matrices | View/weapon fixtures pass; distant pedestrian distortion and online visuals not fully accepted |
| Aspect/HUD | Existing layout retained | Existing per-view layout retained | Online viewport/HUD-owner path remains separate | Video and online viewport fixtures pass; not a fresh visual test of all layouts |
| Wrong-way signs / catch-up | Existing intended mode scope | Shared removed warning / catch-up hooks | Same native hooks | Does not disable genuine crash/bust recovery |
| Mounted roaming | Big Game stock recovery retained | Local human slots1..4 | Fixed: authoritative human actor bitmask, all14slots, captured replay rules | New host/client replay cases preserve crash, eject, busted and AI exclusions |
| Manual eject | Existing local action | Four local bindings | Authoritative command action and fourteen-slot health protection | Menu/eject fixture passes; live input delivery still needs paired testing |
| Custom cop health, arrest, siren, equipment | Not a campaign override | Existing custom rules | Rules, native state and outcomes have authority/replay paths | Initialization/menu parity above4online players is incomplete; see below |
| Traffic distance | Extended native spawn proposal and per-view visibility | Same helpers | Host native spawn uses helper; world-state ownership differs on clients |20-car cap retained. Authority collision/terrain mismatch remains open |
| Sky | Original sky producer | Same producer with native view scissor | Same local sky presentation | Also includes attract demo; worker handoff fixed. Visual acceptance pending |

## Concrete correction in this audit

`rr64_local_player_roaming` previously used live transport state and a maximum
of four local humans. It now uses `prediction::status_for_rules()` and, under
authority, the canonical human actor mask. The original four-player local path
is unchanged. Missing/invalid actors, AI, real crashes, detached bodies, busts,
invalid health and depleted bikes still enter stock recovery.

The seven rebuilt fixtures and outputs are retained in
`analysis/release-1.2-online/mode-parity-checks.json` (workspace root). The old
standalone weapon fixture needed an offline identity stub for its newer online
presentation dependency; that fixture does not validate network smoothing.

## Gaps that prevent claiming complete uniformity

1. More than four online participants select `replicated_riders`. The custom
   options gate in `rr64_local_race_options.cpp` excludes that path. Race bike
   overrides and roster construction still assume at most four native human
   profile records. Do not remove these bounds without replacing the stock
   four-entry menu/profile use.14actor protocol capacity is not14player feature
   acceptance.
2. Candidate23's authoritative ground-height/position disagreement is unresolved.
   Shared graphics hooks do not repair streamed collision-terrain residency.
3. Extended traffic spawning depends on draw-distance preference, which is a
   presentation setting with a simulation consequence. The host governs traffic,
   but differing peer preferences/replay settings require an explicit rule audit
   before claiming equivalent speculative simulation.
4. Runtime MAX LOD changes remain restart-only. A live-toggle expectation must
   not be mistaken for a missing split-screen code path.
5. Cop effects and remote crash/pedestrian/weapon visuals need actual paired
   online acceptance. Local fixtures alone cannot establish this.

No release ZIP was created by this audit. Existing candidate packages remain
unchanged. Do not label online parity, synchronization, or the sky fully fixed
based solely on the offline checks.
