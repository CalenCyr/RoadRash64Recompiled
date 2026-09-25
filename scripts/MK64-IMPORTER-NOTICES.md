# MK64 ROM importer

This tool converts data from ROM files supplied locally by the player. The
distributed converter contains conversion code, address/layout recipes and
third-party runtime libraries. It contains no game ROM, extracted course mesh,
texture, animation stream, sound bank or music track. Conversion happens locally
and does not download game data.

The documented source reference for interpreting the supported USA cartridge is
the n64decomp/mk64 project, revision
`58cfcb022e10f83bc3b889d7e97508cae6837098`:
https://github.com/n64decomp/mk64

Address recipes identify bytes to read from the player's supported ROM. They
are not substitutes for those bytes. The generated mod remains derived from the
player's ROM; this tool does not grant redistribution rights to the generated
assets.

The frozen runtime includes Python and NumPy. Their full license notices
are included in this directory. PyInstaller is used to bundle the converter;
its bootloader has a distribution exception described at
https://pyinstaller.org/en/stable/license.html.

The contact helper executes a bounded subset of this project's native floor
query code against generated collision cells. It does not start the game.
Its N64Recomp support header is covered by the included N64Recomp MIT license.
The motion helper implements the referenced source's spline and path arithmetic
over control points read from the player's ROM. No source spawn/path arrays are
embedded in either helper.
