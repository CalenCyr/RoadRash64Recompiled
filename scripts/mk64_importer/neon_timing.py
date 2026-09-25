"""Deterministic palette/visibility clock for the three animated neon signs.

This small state machine implements the source timer semantics. It generates
the sequence at conversion time rather than distributing captured frame data
or requiring a C compiler on the player's computer.
"""

from dataclasses import dataclass, astuple
from .common import require


@dataclass
class Clock:
    state: int = 1
    status: int = 0
    active: int = 0
    timer: int = 0
    frame: int = 0
    toggle: int = 0
    repeats: int = 0

    def enter(self, state=None):
        self.active = 0
        self.status &= ~0x2000
        self.state = self.state + 1 if state is None else state

    def wait(self, ticks, hidden=False):
        if not self.active:
            self.active = 1
            self.timer = ticks
            if hidden:
                self.status |= 0x80000
                self.frame = 0
        self.timer -= 1
        if self.timer < 0:
            if hidden:
                self.status &= ~0x80000
            self.enter()

    def alternate(self, first, second, ticks, repeats, visibility=False):
        if not self.status & 0x2000:
            self.timer, self.frame, self.toggle, self.repeats = ticks, first, 1, repeats
            self.status |= 0x2000
            return
        self.timer -= 1
        if self.timer < 0:
            self.timer = ticks
            self.toggle -= 1
            if visibility:
                self.status = self.status & ~0x80000 if self.toggle & 1 else self.status | 0x80000
            else:
                self.frame = first if self.toggle & 1 else second
            if self.toggle < 0:
                self.toggle = 1
                if self.repeats > 0:
                    self.repeats -= 1
                if not self.repeats:
                    self.enter()

    def sweep(self, first, last, step, ticks, repeats):
        if not self.status & 0x2000:
            self.frame, self.timer, self.repeats = first, ticks, repeats
            self.active = 1
            self.status |= 0x2000
            return
        self.timer -= 1
        if self.timer <= 0:
            self.timer = ticks
            self.frame += step
            if (step > 0 and self.frame > last) or (step < 0 and self.frame < last):
                if self.repeats > 0:
                    self.repeats -= 1
                if not self.repeats:
                    self.frame = last
                    self.enter()
                else:
                    self.frame = first


def update(clock, family):
    state = clock.state
    if state == 1:
        clock.enter()
    elif family == "mushroom":
        if state in (2, 5):
            clock.sweep(0, 4, 1, 12, 5)
        elif state == 3:
            clock.alternate(3, 4, 4, 10)
        elif state in (4, 6):
            clock.wait(20)
        elif state == 7:
            clock.alternate(3, 4, 0, 20)
        elif state == 8:
            clock.enter(2)
    elif family == "mario":
        if state == 2:
            clock.sweep(0, 4, 1, 12, 1)
        elif state == 3:
            clock.alternate(3, 4, 12, 1)
        elif state == 4:
            clock.wait(12, True)
        elif state == 5:
            clock.enter(2)
    else:
        if state == 2:
            clock.sweep(0, 4, 1, 5, 1)
        elif state in (3, 5):
            clock.wait(30)
        elif state == 4:
            clock.alternate(4, 4, 0, 7, True)
        elif state == 6:
            clock.sweep(3, 0, -1, 5, 1)
        elif state == 7:
            clock.wait(15, True)
        elif state == 8:
            clock.enter(2)


def sequences():
    result = {}
    for family in ("mushroom", "mario", "boo"):
        clock = Clock()
        states = []
        for _ in range(6000):
            update(clock, family)
            states.append(astuple(clock))
        seen = {}
        for tick, key in enumerate(states):
            if key not in seen:
                seen[key] = tick
                continue
            start = seen[key]
            loop = states[start:tick]
            require(0 < len(loop) <= 4096, "Neon loop length")
            require(states[start : start + 2 * len(loop)] == loop * 2, "Neon cycle is not stable")
            result[family] = {
                "frame_sequence": [s[4] for s in loop],
                "visible_sequence": [not bool(s[1] & 0x80000) for s in loop],
                "frame_ticks": 1,
                "source_loop_start_update": start,
                "source_loop_updates": len(loop),
                "initial_frame_sequence": [s[4] for s in states[:start]],
                "initial_visible_sequence": [not bool(s[1] & 0x80000) for s in states[:start]],
                "method": "Deterministic source timer/state semantics; no asset arrays embedded.",
            }
            break
        else:
            raise ValueError("Neon animation cycle was not found")
    return result
