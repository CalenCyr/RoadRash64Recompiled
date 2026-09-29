"""Original item cues and Star sequence, extracted from the user's ROM only."""
import hashlib
import struct
import numpy as np
from . import audio, music
from .common import require

# Native sound argument bank and ID; channel startup sets banks 1/2/4 to
# large-note mode. Tables come from sequence00's six original channel programs.
# A loop point of zero means repeated sample; FFFFFFFF means one-shot.
EFFECTS = (
    ("shell_launch", 1, 0x04, False, 1.5),
    ("shell_motion", 1, 0x54, True, 1.5),
    ("banana_fakebox", 1, 0x12, False, 1.5),
    ("mushroom", 1, 0x0B, False, 2.0),
    ("lightning", 1, 0x13, False, 3.0),
    ("boo_start", 1, 0x59, False, 2.0),
    ("boo_loop", 0, 0x4C, True, 2.5),
    ("star_loop", 0, 0x2C, True, 2.5),
    ("star_distant", 3, 0x08, True, 2.5),
    ("shell_warning", 5, 0x08, True, 2.5),
    ("lightning_impact", 5, 0x0C, False, 3.0),
    ("item_hit", 1, 0x06, False, 1.6),
)


def extract(rom, progress=None):
    donor = audio.Donor(rom)
    bank = bytearray(b"R64ISFX1" + struct.pack("<I", len(EFFECTS) + 1))
    proof = []
    for index, (name, group, sound_id, loop, seconds) in enumerate(EFFECTS):
        if progress:
            progress.update("sound", "Converting original item sounds", 79)
        pcm, source = donor.effect(group, sound_id, seconds)
        require(len(pcm) and np.max(np.abs(pcm.astype(np.int32))) >= 100,
                "Silent original item sound: " + name)
        bank += struct.pack("<IIIII", index, audio.RATE, len(pcm), 1,
                            0 if loop else 0xFFFFFFFF) + pcm.tobytes()
        proof.append({"id": index, "name": name, "group": group, "sound_id": sound_id,
                      "samples": len(pcm), "sha256": hashlib.sha256(pcm.tobytes()).hexdigest(),
                      "sequence_entry": source["sequence_entry"], "source_samples": source["samples"]})
    # SEQ_EVENT_POWERUP is sequence0x11 with original instrument set0x0E.
    star = music.MusicDonor(rom, 0x11, bank_id=0x0E)
    pcm, loop, source = music.render(music.Sequence(star).parse())
    require(source["peak"] >= 100 and 0 <= loop < len(pcm), "Original Star sequence")
    bank += struct.pack("<IIIII", len(EFFECTS), audio.RATE, len(pcm), 2, loop) + pcm.tobytes()
    proof.append({"id": len(EFFECTS), "name": "star_music", "sequence": 0x11,
                  "bank": 0x0E, "samples": len(pcm), "loop": loop,
                  "sha256": hashlib.sha256(pcm.tobytes()).hexdigest()})
    require(len(bank) < 16 * 1024 * 1024, "Bounded item audio bank")
    return bytes(bank), {"effects": proof, "bytes": len(bank),
                         "sha256": hashlib.sha256(bank).hexdigest()}
