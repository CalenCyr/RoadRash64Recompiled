"""Asset-free entry point; PyInstaller freezes this for the shipped importer."""
from mk64_importer.cli import main

if __name__ == '__main__':
    raise SystemExit(main())
