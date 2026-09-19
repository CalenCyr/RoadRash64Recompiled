"""Compare paired RR64 CSV captures; local frame counters are not lockstep ticks."""
import csv
import json
import math
from pathlib import Path


def load(path):
    lines = Path(path).read_text().splitlines()
    rows = list(csv.DictReader(line for line in lines if not line.startswith('#')))
    complete = any(line.startswith('# end ') for line in lines)
    incomplete = any(line.startswith('# incomplete') for line in lines)
    dropped = max([int(row['dropped']) for row in rows] +
                  [int(line.split('dropped=')[1]) for line in lines if line.startswith('# end ')] + [0])
    return rows, complete and not incomplete and not dropped


def compare(paths):
    captures = [load(p) for p in paths]
    originals = {}
    for rows, _ in captures:
        for r in rows:
            if r['stage'] == '1' and r['slot'] == r['local'] and r['received'] == '1':
                key = (r['race'], r['slot'], r['tick'])
                originals[key] = tuple(float(r['received_'+axis]) for axis in 'xyz')
    matched = mismatch = missing = 0
    max_error = 0.0
    for rows, _ in captures:
        for r in rows:
            if r['slot'] == r['local'] or r['received'] != '1':
                continue
            source = originals.get((r['race'], r['slot'], r['tick']))
            if source is None:
                missing += 1
                continue
            target = tuple(float(r['received_'+axis]) for axis in 'xyz')
            error = math.dist(source, target)
            if not math.isfinite(error) or error > 0.0001:
                mismatch += 1
            matched += 1
            max_error = max(max_error, error) if math.isfinite(error) else float('inf')
    starts = {}
    initial_differences = []
    for rows, _ in captures:
        for r in rows:
            if r['stage'] != '0' or r['frame'] != '0' or r['valid'] != '1':
                continue
            key = (r['race'], r['slot'])
            if key in starts:
                other = starts[key]
                error = math.dist(tuple(float(r[a]) for a in 'xyz'),
                                  tuple(float(other[a]) for a in 'xyz'))
                if error > 0.0001 or r['setup'] != other['setup'] or r['rng'] != other['rng']:
                    initial_differences.append({'race': r['race'], 'slot': r['slot'],
                        'position_difference': error, 'setup_differs': r['setup'] != other['setup'],
                        'rng_differs': r['rng'] != other['rng']})
            else:
                starts[key] = r
    return {'capture_complete': all(c for _, c in captures), 'matched_updates': matched,
            'mismatched_payloads': mismatch, 'unmatched_updates': missing,
            'maximum_payload_difference': max_error,
            'initial_state_differences': initial_differences,
            'gameplay_sync': 'NOT VERIFIED: sender ticks verify transport only; local frames are independent',
            'pairing': 'User must supply the same session and corresponding race ordinals',
            'limitations': 'Initial states may be from different simulation moments. No shared start barrier/clock; missing samples are not evidence of packet loss'}


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('logs', nargs='+')
    args = parser.parse_args()
    if len(args.logs) < 2:
        parser.error('Provide host and client logs from the same test')
    print(json.dumps(compare(args.logs), indent=2))
