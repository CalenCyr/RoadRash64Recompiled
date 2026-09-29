"""Extract the actual generated native mode setters, including production hooks."""
import pathlib
import sys

source = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
setup_source = pathlib.Path(sys.argv[1]).with_name("funcs_7.c").read_text(encoding="utf-8")
setup_start = setup_source.index("RECOMP_FUNC void func_80027A90(")
setup_prologue = setup_source[setup_start:setup_source.index("// 0x80027A90:", setup_start)]
if "if (rr64_online_postrace_wait_for_setup()) return;" not in setup_prologue:
    raise SystemExit("Native setup must wait before its prologue and input/draw hooks")
pieces = ['#include "recomp.h"\n#include "rr64_native.hpp"\nextern "C" {\n']
for name in ("func_80048544", "func_80048558"):
    start = source.index("RECOMP_FUNC void " + name + "(")
    end = source.index("RECOMP_FUNC void ", start + 1)
    body = source[start:end]
    if "ctx->r4 = rr64_online_postrace_route_mode(rdram, ctx->r4);" not in body:
        raise SystemExit("Regenerate CPU functions before testing post-race mode setters")
    pieces.append(body)
pieces.append("}\n")
pathlib.Path(sys.argv[2]).write_text("".join(pieces), encoding="utf-8")
