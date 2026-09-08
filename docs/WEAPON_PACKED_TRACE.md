# Weapon packed-position diagnostic

The previous inherited-source candidate failed visual testing. Its 1,430 root
checks applied zero source conversions. This build adds read-only capture of
owner/view identity, rider position, float weapon translation and actual packed
translation. It retains the prior candidate's rendering behavior for comparison.

Samples keep the latest value per owner/view/root, up to 64 identities per log
interval, with omissions counted. This is sampled evidence, not every frame.
No per-draw file I/O. Diagnostic overhead has not been measured live.
Offline tests passed with diagnostics on/off, unchanged guest memory, signed
packed-coordinate wrap, view isolation, report draining and bounded capacity.

Rolling terrain remains reverted. Nothing published. Await ready before launch.
Reproduce with two local players separated until the floating weapon appears;
attack briefly, then close normally and report the result.
