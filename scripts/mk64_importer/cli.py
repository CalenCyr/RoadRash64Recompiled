"""Private staging-only CLI used by the in-game Mods importer."""

import argparse
import sys
import traceback
from pathlib import Path
from . import CONVERTER_VERSION
from .common import Progress, Cancelled, atomic_json


def _bundled_helper(name):
    """Frozen builds keep both native helpers beside the importer executable."""
    suffix = ".exe" if sys.platform == "win32" else ""
    return Path(sys.executable).parent / (name + suffix)


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Convert your MK64 ROM into a Road Rash 64 course mod."
    )
    parser.add_argument("--version", action="version", version=CONVERTER_VERSION)
    parser.add_argument("--mk64-rom", type=Path, required=True)
    parser.add_argument("--rr64-rom", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--progress", type=Path, required=True)
    parser.add_argument("--cancel-file", type=Path, required=True)
    parser.add_argument("--contact-helper", type=Path, help=argparse.SUPPRESS)
    parser.add_argument("--motion-helper", type=Path, help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    progress = Progress(args.progress, args.cancel_file)
    # Result/progress belong to the job, never to the installed pack inventory.
    result = args.output.parent / "result.json"
    helper = args.contact_helper or _bundled_helper("rr64-mk64-contact")
    motion_helper = args.motion_helper or _bundled_helper("rr64-mk64-motion")
    try:
        from .pipeline import convert

        catalogue = convert(
            args.mk64_rom, args.rr64_rom, args.output, helper, motion_helper, progress
        )
        atomic_json(
            result,
            {
                "success": True,
                "catalogue_sha256": catalogue,
                "converter_version": CONVERTER_VERSION,
            },
        )
        return 0
    except Cancelled:
        atomic_json(result, {"success": False, "error": "Course import cancelled."})
        return 2
    except Exception as error:
        traceback.print_exc()
        message = str(error).strip() or "The course conversion could not be completed."
        atomic_json(result, {"success": False, "error": message[:1024]})
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
