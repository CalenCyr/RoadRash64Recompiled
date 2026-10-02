"""Extract actual Controls routing methods for the headless regression fixture.

The fixture supplies rendering/input services, while profile selection, stale
scan cancellation, mutation guards and refresh behavior come from production.
"""
from pathlib import Path
import argparse

SIGNATURES = [
    'void ConfigPageControls::process_event(',
    'void ConfigPageControls::force_update(',
    'void ConfigPageControls::render_all(',
    'bool ConfigPageControls::should_show_mappings(',
    'void ConfigPageControls::on_select_player_profile(',
    'void ConfigPageControls::on_edit_player_profile(',
    'bool ConfigPageControls::set_current_profile_index(',
    'bool ConfigPageControls::can_edit_current_profile(',
    'void ConfigPageControls::update_control_mappings(',
    'recompinput::InputDevice ConfigPageControls::get_player_input_device(',
    'void ConfigPageControls::on_bind_click(',
    'void ConfigPageControls::on_clear_or_reset_game_input(',
]


def method(source, signature):
    assert source.count(signature) == 1, ('Missing or ambiguous Controls method', signature)
    start = source.index(signature)
    body = source.index('{', start)
    depth = 1
    pos = body + 1
    while depth:
        depth += (source[pos] == '{') - (source[pos] == '}')
        pos += 1
    return source[start:pos]


def extract_methods(source):
    return [method(source, signature) for signature in SIGNATURES]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    methods = extract_methods(args.source.read_text(encoding='utf-8'))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text('\n\n'.join(methods) + '\n', encoding='utf-8')
    print(f'Extracted {len(methods)} actual Controls routing methods')


if __name__ == '__main__':
    main()
