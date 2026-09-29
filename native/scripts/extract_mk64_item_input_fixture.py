"""Extract the production input callback edge and authority action drain."""
from pathlib import Path
import sys

source=Path(sys.argv[1])
output=Path(sys.argv[2])
main=(source/'main.cpp').read_text(encoding='utf-8')
start=main.index('#ifdef RR64_EXPERIMENTAL_COURSE\n        static std::array<bool, 4> item_was_held{};')
end=main.index('#endif',start)+len('#endif')
edge=main[start:end]
shims=(source/'rr64_runtime_shims.cpp').read_text(encoding='utf-8')
start=shims.index('extern "C" unsigned rr64_authority_take_actions(unsigned controller) {')
brace=shims.index('{',start)
depth=1
end=brace+1
while depth:
    depth+=(shims[end]=='{')-(shims[end]=='}')
    end+=1
output.write_text('// Exact production input callback block and action drain.\n'
    'void sample_item_input(int profile_index,bool got_response,bool dismissal_input_blocked) {\n'
    '    if(profile_index>=0 && profile_index<4) {\n'+edge+'\n    }\n}\n'+shims[start:end]+'\n',encoding='utf-8')
