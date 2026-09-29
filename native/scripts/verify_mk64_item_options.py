"""Run the real native menu fixture and preference reloads in fresh processes."""
from pathlib import Path
import json
import subprocess
import sys

exe = Path(sys.argv[1]).resolve()
scratch = Path(sys.argv[2]).resolve()
scratch.mkdir(parents=True, exist_ok=False)
results = []

def run(*args):
    result = subprocess.run([str(exe), *map(str, args)], capture_output=True, text=True, timeout=30)
    results.append(dict(arguments=list(map(str, args)), exit=result.returncode,
                        stdout=result.stdout, stderr=result.stderr))
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)

try:
    normal = scratch / "native-menu"
    run(normal)
    run(normal, 0)  # A distinct process proves the preference survived exit.
    for index, (saved, expected) in enumerate([
        ("1 0\n", 0), ("1 1\n", 1), ("", 1), ("1", 1), ("1 2", 1),
        ("2 0", 1), ("-1 0", 1), ("1 -1", 1), ("1 0 extra", 1),
        ("1 0 1", 1), ("1 0 \n\t", 0), ("garbage", 1),
    ]):
        case = scratch / f"reload-{index:02}"
        case.mkdir()
        path = case / "mk64-item-options.cfg"
        path.write_text(saved)
        (case / "local-race-options.cfg").write_text("5 134217722\n")
        (case / "thrash-race-options.cfg").write_text("2 134217722 127\n")
        run(case, expected)
        assert path.read_text() == saved
finally:
    (scratch / "proof.json").write_text(json.dumps(dict(
        game_launched=False, results=results), indent=2))
print(f"MK64 options: {len(results)} fresh-process runs passed")
