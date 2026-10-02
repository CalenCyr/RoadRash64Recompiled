"""Keep native streamed-cell lookup and missing-floor camera math in the test.

The inputs are synthetic; no ROM bytes, saved race, or game assets are embedded.
The lookup retains the maintained handoff hook at its actual branch boundary.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re


def function(source, name):
    found = re.search(r"RECOMP_FUNC void " + name + r"\(.*?(?=\nRECOMP_FUNC |\Z)", source, re.S)
    if not found:
        raise ValueError("Missing native function " + name)
    return found[0]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--recompiled-dir", type=Path, required=True)
    p.add_argument("--config", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    files = [a.recompiled_dir / "funcs_3.c", a.recompiled_dir / "funcs_14.c", a.config]
    lookup = function(files[0].read_text(encoding="utf-8"), "func_800146C8")
    camera = function(files[1].read_text(encoding="utf-8"), "func_8005D9A4")
    config = a.config.read_text(encoding="utf-8")
    # Hooks outside this narrow test concern online physics and imported maps.
    # Reinsert this feature's current hook from its production source config.
    lookup = "\n".join(line for line in lookup.splitlines() if "rr64_" not in line)
    record = re.compile(r'\s*\{\s*func\s*=\s*"func_800146C8",\s*before_vram\s*=\s*0x80014768,\s*text\s*=\s*("(?:\\.|[^"\\])*")\s*\},?\s*')
    hooks = []
    for line in config.splitlines():
        if "rr64_highlight_camera_floor_cell" not in line or line.lstrip().startswith("#"):
            continue
        match = record.fullmatch(line)
        if not match:
            raise ValueError("Camera cell hook no longer at the audited native lookup branch")
        hooks.extend(part.strip() + ";" for part in json.loads(match[1]).split(";")
                     if "rr64_highlight_camera_floor_cell" in part)
    if len(hooks) != 1:
        raise ValueError("Expected one maintained camera cell hook")
    needle = "    // 0x80014768:"
    if lookup.count(needle) != 1:
        raise ValueError("Ambiguous native cell-state branch")
    lookup = lookup.replace(needle, "    " + hooks[0] + "\n" + needle)
    start = camera.index("    // 0x8005E2A0:")
    # Include the final Z store in the following call's delay slot, without
    # calling the unrelated floor-record initializer after the eye is written.
    end = camera.index("    func_80014604(rdram, ctx);", start)
    body = camera[start:end]
    if "func_" in body or "rr64_" in body:
        raise ValueError("Unexpected calls in native missing-floor camera math")
    prefix = '#include "recomp.h"\n#include "rr64_highlight_camera.hpp"\nextern "C" void func_800146C8(unsigned char *, recomp_context *);\n'
    missing = '''
extern "C" void highlight_missing_floor_native(unsigned char *rdram, recomp_context *ctx) {
    int c1cs = 0;
'''+body+"\n}\n"
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(prefix + lookup + "\n" + missing, encoding="utf-8")
    a.output.with_suffix(".json").write_text(json.dumps({
        "sources": {str(f): hashlib.sha256(f.read_bytes()).hexdigest() for f in files},
        "lookup": "func_800146C8: original native state5 and resource-root checks",
        "camera_region": "8005E2A0..8005E338 inclusive: original missing-floor chase-arm and eye calculation; stop before14604 call",
        "hook": hooks[0], "inputs": "Synthetic guest records and arithmetic coefficients only",
    }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
