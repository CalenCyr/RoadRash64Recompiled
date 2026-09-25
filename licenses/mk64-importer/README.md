# Supplemental MK64 importer runtime notices

The files listed below accompany the Windows converter at `tools/mk64-importer`. They supplement its existing Python, NumPy, PyInstaller and N64Recomp notices without changing the tested converter binaries. Preserve both notice directories when redistributing the package. The Linux converter has its own [Linux runtime inventory and package notices](linux/README.md); the Windows OpenSSL/libffi versions below do not describe that Linux build. Python's incorporated-software notice applies to both CPython 3.12.14 runtimes.

| File | Upstream source |
|---|---|
| `OpenSSL-LICENSE.txt` | [OpenSSL 3.5.8 LICENSE.txt](https://github.com/openssl/openssl/blob/openssl-3.5.8/LICENSE.txt) |
| `OpenSSL-AUTHORS.md` | [OpenSSL 3.5.8 AUTHORS.md](https://github.com/openssl/openssl/blob/openssl-3.5.8/AUTHORS.md) |
| `libffi-3.4.4-LICENSE.txt` | [libffi 3.4.4 LICENSE](https://github.com/libffi/libffi/blob/v3.4.4/LICENSE) |
| `libffi-3.5.2-LICENSE.txt` | [libffi 3.5.2 LICENSE](https://github.com/libffi/libffi/blob/v3.5.2/LICENSE) |
| `CPython-additional-notices.rst` | [CPython 3.12.14 incorporated-software notices](https://github.com/python/cpython/blob/v3.12.14/Doc/license.rst) |

The bundled OpenSSL DLLs identify themselves as version 3.5.8. Python is 3.12.14. The `libffi-8.dll` supplied with that Python runtime does not expose a version resource; the package does not claim an exact libffi source revision. CPython's 3.12.14 Windows build recipe names libffi 3.4.4. Both that notice and the later 3.5.2 notice are retained, with unchanged MIT terms and their original copyright ranges. This identifies the notices' source; it does not prove a binary-to-source match.

The converter contains no MIDI conversion program or FFmpeg binary. It uses project code and NumPy to render the supplied ROM's original native audio sequences and instruments. Course audio rights remain with the original rights holders. The Linux player's separate FFmpeg dependency and its source/license requirements are documented in the main third-party notices.
