import csv
import tempfile
from pathlib import Path
from compare_sync_logs import compare

with tempfile.TemporaryDirectory() as temp:
    paths = [Path(temp)/'host.csv', Path(temp)/'client.csv']
    base = dict(host='1', local='0', race='1', frame='0', phase='4', setup='5',
                options='0', rng='6', stage='1', us='1', slot='0', valid='1',
                input_valid='1', buttons='0', stick_x='0', stick_y='0', x='10',
                y='20', z='30', received='1', tick='42', received_x='10',
                received_y='20', received_z='30', dropped='0')
    def write(index, row, footer=True):
        with paths[index].open('w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=base)
            writer.writeheader(); writer.writerow(row)
            if footer: f.write('# end dropped=0\n')
    write(0, base)
    remote = dict(base, host='0', local='1', frame='97')
    write(1, remote)
    r = compare(paths)
    assert r['matched_updates'] == 1 and r['mismatched_payloads'] == 0
    write(1, dict(remote, received_x='99'))
    assert compare(paths)['mismatched_payloads'] == 1
    write(1, dict(remote, tick='43'), footer=False)
    r = compare(paths)
    assert not r['capture_complete'] and r['unmatched_updates'] == 1
    write(1, dict(remote, dropped='9'))
    assert not compare(paths)['capture_complete']
print('Sync report tests passed: matching ticks, altered payload, missing origin, incomplete capture, dropped records.')
