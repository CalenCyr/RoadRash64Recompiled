"""Check release logging gates with offline fixtures; never starts the game."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    binaries = Path(sys.argv[1]).resolve()
    suffix = ".exe" if os.name == "nt" else ""
    environment = os.environ.copy()
    for key in ("RR64_DIAGNOSTICS", "RR64_RUNTIME_TRACE", "RR64_AUTOTEST", "RR64_SYNC_LOG"):
        environment.pop(key, None)

    with tempfile.TemporaryDirectory(prefix="rr64-logging-check-") as directory:
        for mode, value, expected in (("default", None, "0"), ("disabled", "0", "0"),
                                      ("invalid", "true", "0"), ("enabled", "1", "1")):
            child = environment.copy()
            if value is not None:
                child["RR64_DIAGNOSTICS"] = value
            options = subprocess.run(
                [str(binaries / ("RR64DiagnosticOptionsSmoke" + suffix)),
                 expected, "0", expected, expected],
                env=child, cwd=directory, capture_output=True, text=True, check=True)
            archive = subprocess.run(
                [str(binaries / ("RR64RiderSkinArchiveSmoke" + suffix))],
                env=child, cwd=directory, capture_output=True, text=True, check=True)
            if ("[RR64-RIDER-SKINS] Loaded" in archive.stderr) != (expected == "1"):
                raise AssertionError(f"{mode}: routine report did not follow diagnostics")
            if "[RR64-RIDER-MOD]" not in archive.stderr:
                raise AssertionError(f"{mode}: invalid archive errors were suppressed")
            if list(Path(directory).glob("*.log")):
                raise AssertionError(f"{mode}: offline fixtures created an unsolicited log")
            print(f"{mode}: {options.stdout.strip()} / {archive.stdout.strip()}")

    print("Release logging: default/disabled/invalid/enabled gates and retained errors passed.")


if __name__ == "__main__":
    main()
