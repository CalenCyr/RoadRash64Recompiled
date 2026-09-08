Diagnostic candidate, not an artifact fix. Rolling terrain remains reverted.
RR64_WEAPON_DIAGNOSTICS=1 records finite root matrices with absolute components
at least 30000 before existing LOD normalization, in 2–4-view scenes. Includes all
root types; records are not yet identified as weapons. A pre-normalized near-limit
value alone is not proof of wrapping. 64 bounded hashed slots are drained through
the existing periodic logger; replacement count reports collisions, and later
samples for the same root replace earlier samples. No per-root file writes.
Actor runtime smoke passes enabled and disabled; actual weapon capture pending.
No terrain, attack animation, camera, or pose behavior was changed. No launch.
