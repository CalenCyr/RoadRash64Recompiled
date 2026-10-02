"""Extract the real frame dispatcher and deterministic-clock highlight runtime.

Only the clock alias differs in the runtime copy. The dispatcher retains the
generated native instructions, then applies the current source-config hooks.
No ROM, saved game, rendered frame, or captured gameplay asset is required.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re


def dispatch_hooks(config):
    # These maintained inline TOML records use the JSON-compatible quoted
    # string subset. Parse only this function's hooks so Python 3.10/Linux
    # needs no extra package. Refuse format changes instead of skipping them.
    record = re.compile(r'\s*\{\s*func\s*=\s*"func_8004E830",\s*'
                        r'before_vram\s*=\s*(0x[0-9A-Fa-f]+),\s*'
                        r'text\s*=\s*("(?:\\.|[^"\\])*")\s*\},?\s*')
    found = {}
    for line in config.splitlines():
        if line.lstrip().startswith("#") or 'func = "func_8004E830"' not in line or "before_vram" not in line:
            continue
        match = record.fullmatch(line)
        if not match:
            raise ValueError("Unsupported frame-dispatch hook record format")
        pc = int(match[1], 16)
        if 0x8004E998 <= pc <= 0x8004EA04:
            if pc in found:
                raise ValueError(f"Duplicate dispatcher hook 0x{pc:08X}")
            found[pc] = {"func": "func_8004E830", "before_vram": pc, "text": json.loads(match[2])}
    expected = {0x8004E998, 0x8004E9BC, 0x8004E9C4, 0x8004E9DC, 0x8004E9E4, 0x8004E9FC, 0x8004EA04}
    if found.keys() != expected:
        raise ValueError("Expected exactly seven hooks spanning the native results dispatcher")
    return list(found.values())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recompiled", required=True, type=Path)
    parser.add_argument("--config", required=True, type=Path)
    parser.add_argument("--runtime", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()
    source = args.recompiled.read_text(encoding="utf-8")
    match = re.search(r"RECOMP_FUNC void func_8004E830\(.*?(?=\nRECOMP_FUNC |\Z)", source, re.S)
    if not match:
        raise ValueError("Native frame dispatcher func_8004E830 not found")
    function = match[0]
    first, last = (function.index(f"    // 0x{pc:08X}:") for pc in (0x8004E998, 0x8004EA04))
    body = "\n".join(line for line in function[first:last].splitlines() if "rr64_" not in line)
    body += "\n    // 0x8004EA04: fixture boundary\n"
    selected = dispatch_hooks(args.config.read_text(encoding="utf-8"))
    for hook in selected:
        pc = hook["before_vram"]
        needle = f"    // 0x{pc:08X}:"
        if body.count(needle) != 1:
            raise ValueError(f"Ambiguous hook anchor {needle}")
        body = body.replace(needle, "    " + hook["text"] + "\n" + needle, 1)
    if body.count("if (rr64_highlights_frame_gate(") != 1 or body.count("if (rr64_highlights_block_dispatch())") != 2:
        raise ValueError("Expected update, preparation and results highlight guards")
    prefix = '#include "recomp.h"\n#include "rr64_highlights.hpp"\n'
    for name in sorted(set(re.findall(r"\b(rr64_\w+)\s*\(", body))):
        if not name.startswith("rr64_highlights_"):
            prefix += f"#define {name}(...) ((void)0)\n"
    prefix += '''
using FixtureFn = void (*)(unsigned char *, recomp_context *);
extern "C" void fixture_callback(unsigned, unsigned char *, recomp_context *);
static unsigned callback_target;
static void callback(unsigned char *m, recomp_context *c) { fixture_callback(callback_target, m, c); }
static FixtureFn fixture_lookup(unsigned target) { callback_target = target; return callback; }
#undef LOOKUP_FUNC
#define LOOKUP_FUNC(x) fixture_lookup(static_cast<unsigned>(x))
extern "C" void highlight_dispatch_native(unsigned char *rdram, recomp_context *ctx) {
'''
    runtime = args.runtime.read_text(encoding="utf-8")
    alias = "using Clock = std::chrono::steady_clock;"
    if runtime.count(alias) != 1:
        raise ValueError("Expected exactly one production steady-clock alias")
    replacement = '#include "rr64_highlight_transition_clock.hpp"\n' + runtime.replace(
        alias, "using Clock = rr64::highlight_test::Clock;")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / "rr64_highlight_transition_dispatch.cpp").write_text(prefix + body + "\n}\n", encoding="utf-8")
    (args.output_dir / "rr64_highlight_transition_runtime.cpp").write_text(replacement, encoding="utf-8")
    provenance = {
        "sources": {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in (args.recompiled, args.config, args.runtime)},
        "native_region": {"function": "8004E830", "first": "8004E998", "end_exclusive": "8004EA04", "hooks": selected},
        "runtime_change": "Only Clock alias; synthetic time preserves steady_clock duration/time_point semantics",
    }
    (args.output_dir / "rr64_highlight_transition_extraction.json").write_text(json.dumps(provenance, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
