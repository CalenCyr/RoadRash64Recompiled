"""Build existing course audio banks without emitting unrelated WAV files."""

import struct

import numpy as np

from . import audio, music
from .common import require


def effects(rom, progress):
    donor = audio.Donor(rom)
    bank = bytearray(b"R64SFX1\0" + struct.pack("<I", len(audio.EFFECTS)))
    for index, (name, group, sound_id, loop, seconds) in enumerate(audio.EFFECTS):
        progress.update(
            "sound", "Converting course sound effects", 76 + 4 * index / len(audio.EFFECTS)
        )
        pcm, _ = donor.effect(group, sound_id, seconds)
        require(
            len(pcm) and np.max(np.abs(pcm.astype(np.int32))) >= 100, "Silent source sound: " + name
        )
        bank += struct.pack("<IIII", index, audio.RATE, len(pcm), int(loop)) + pcm.tobytes()
    return bytes(bank)


def songs(rom, progress):
    bank = bytearray(
        b"R64MUS1\0" + struct.pack("<II", len(music.SEQUENCE_BANKS), len(music.COURSES))
    )
    for course, song in music.COURSES.items():
        bank += course.encode("ascii").ljust(32, b"\0") + struct.pack("<I", song)
    for index, song_id in enumerate(music.SEQUENCE_BANKS):
        progress.update(
            "music", "Converting course music", 80 + 16 * index / len(music.SEQUENCE_BANKS)
        )
        donor = music.MusicDonor(rom, song_id)
        sequence = music.Sequence(donor).parse()
        pcm, loop, proof = music.render(sequence)
        require(proof["peak"] >= 100, "Silent course music")
        bank += struct.pack("<IIII", song_id, music.RATE, len(pcm), loop) + pcm.tobytes()
    return bytes(bank)
