# Race audio at the race-to-highlights boundary

Owner testing reported braking and sliding/scraping sounds repeating for the
entire highlight reel. RC3 stopped the brake slot alone; the owner then confirmed
that bike scraping could still persist. Highlights intentionally hold
the native results dispatcher and stop simulation updates. Audio keeps running,
but the bike update that normally expires an unrefreshed brake loop no longer
runs. Replaying only recorded visual poses does not release a live sound handle.

## Ownership and correction

In the supported Road Rash USA game, native `5980C` expires the first five slots
of each thirteen-entry racer sound cache through handle query `80768` and stop
`806B4`. These own engine, brake/skid, rider sliding, bike/surface scraping and
police siren sounds. Slot zero can also contain the engine's start transient;
it belongs to the race engine producer, not music. Slots five through twelve
own independent effects and voices and are outside the cleanup domain.
The cache has fourteen racer rows; player versus AI ownership is irrelevant.
The addresses and stride are centralized in `engine::racer_audio`.

`rr64_highlight_audio.cpp` uses that same five-slot domain and native handle
operations once when
highlights begin holding the dispatcher. Online preparation/waiting also uses
this boundary on each participant. It clears only the owned handles to their native
unused sentinel; it does not restore stopped handles, clear all effects, mute
the device, change music selection, or modify recorded physics/presentation.
The later eight cache entries and all cache metadata remain unchanged. Next-race
initialization and the original producers can allocate fresh sound handles.

The dispatcher hook passes its existing guest context. Cleanup operates on a
copy with a rebound floating-point pointer, on the original game thread, and
rejects prediction, null input and an invalid call stack. It never runs on the
audio callback. A per-race latch prevents repeated stopping through reel loops;
reset/new-race enrollment clears the latch. No-clips, menus and attract mode
retain their existing audio behavior. A malformed context leaves cleanup
pending for the next valid frame instead of falsely declaring it complete.

## Verification limits

The private checks execute extracted original sound producers, expiry and
handle-stop functions against bounded test memory. A separate fixture executes
the current highlights runtime and generated dispatcher, including native handle
stop effects, offline modes, online ownership, repeated playback and exit paths.
Caller context and unrelated voice/cache data must remain intact. The RC3
brake-only implementation is retained as a failing scraping control. These are offline checks;
audible confirmation in the packaged game remains an owner test.

RC4 reuses the RC3 dispatcher hook and changes only its native audio helper.
No course data or importer change is needed for the audio repair itself; any
separate terrain repairs in the same candidate retain their own import/version
requirements. Network packet layout remains unchanged.
