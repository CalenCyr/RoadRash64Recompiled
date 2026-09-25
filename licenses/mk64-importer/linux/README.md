# Linux importer runtime notices

The 1.4.0 Linux x86-64 importer includes CPython 3.12.14, built from the
unmodified python.org source archive, NumPy 2.3.5's CPython 3.12 manylinux wheel,
and the PyInstaller 6.22.3 bootloader. The converter keeps their full notices in
its own `licenses` directory. Python's incorporated-software notices are also
provided in the parent directory. These Linux notices do not identify the
Windows converter's OpenSSL or libffi builds.

`runtime-libraries.json` records the exact host library binary digests and Ubuntu
source-package versions. The adjacent copyright files come from those installed
packages; full common license texts are included for their references.

The NumPy wheel includes OpenBLAS 0.3.30/LAPACK, libgfortran (GPL with GCC Runtime
Library Exception) and libquadmath (LGPL 2.1 or later). Preserve the complete
NumPy LICENSE.txt, including its separate libquadmath terms. These wheel runtime
libraries are distinct from the Ubuntu GCC 12 libraries listed in the manifest.
Their source identity must be obtained from the wheel's upstream build, not
inferred from the local compiler version.

The importer and helpers are ordinary dynamically linked ELF files. Its shared
libraries remain replaceable in `_internal`; no signatures prevent replacement.
See the Linux dependency source archive for corresponding redistributed sources
and build records. Generated ROM-derived tracks, textures, animations and audio
are not part of the distributed importer or dependency source archive.
