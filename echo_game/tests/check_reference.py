#!/usr/bin/env python3
import json
import math
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
reference = json.loads((root / '.build/reference.json').read_text())
material = {'stone': 0, 'metal': 1, 'water': 2, 'soft': 3}
test_world = reference['testWorld']
vec = lambda p: '{{{:.17g}, {:.17g}}}'.format(p['x'], p['y'])
header = '#pragma once\n#include "EchoEngine.h"\nnamespace echo_game {\nstatic const Segment kTestWalls[] = {\n'
for wall in test_world['walls']:
    header += '  {' + vec(wall['a']) + ', ' + vec(wall['b']) + ', Material::' + wall['material'].title() + '},\n'
header += '};\nstatic const Level kTestLevel = {"test", "TEST", 36, 64, kTestWalls, '
header += str(len(test_world['walls'])) + ', {18,55}, 0, {0,0}, {0,0}, 0, 0};\n}\n'
(root / '.build/TestWorld.h').write_text(header)
if len(sys.argv) == 1:
    sys.exit(0)
binary = sys.argv[1]

def close(a, b):
    assert math.isclose(float(a), float(b), rel_tol=1e-8, abs_tol=1e-8), (a, b)

commands = []
for case in reference['echoes']:
    commands.append('echo {level} {x:.17g} {y:.17g} {heading:.17g}'.format(
        level=case['level'], x=case['origin']['x'], y=case['origin']['y'], heading=case['heading']))
result = subprocess.run([binary], input='\n'.join(commands)+'\n', text=True, capture_output=True, check=True)
lines = iter(result.stdout.splitlines())
total = 0
for case in reference['echoes']:
    assert int(next(lines)) == len(case['values'])
    for echo in case['values']:
        row = next(lines).split()
        expected = [echo['point']['x'], echo['point']['y'], echo['distance'], echo['intensity'], echo['bearing']]
        for actual, wanted in zip(row[:5], expected): close(actual, wanted)
        assert int(row[5]) == material[echo['material']]
        assert int(row[6]) == echo['bounces']
        total += 1
assert next(lines, None) is None

commands, expected = [], []
for trace in reference['traces']:
    commands.append('load {}'.format(trace['level']))
    for event in trace['actions']:
        action = event['action']
        commands.append(action['type'].lower() + (' {:.17g}'.format(action['heading']) if 'heading' in action else ''))
        expected.append(event)
result = subprocess.run([binary], input='\n'.join(commands)+'\n', text=True, capture_output=True, check=True)
rows = result.stdout.splitlines()
assert len(rows) == len(expected)
status = {'playing': 0, 'escaped': 1, 'surrendered': 2}
for row, event in zip(rows, expected):
    values = row.split()
    for actual, wanted in zip(values[:3], [event['pos']['x'], event['pos']['y'], event['heading']]): close(actual, wanted)
    assert [int(x) for x in values[3:]] == [event['calls'], event['moves'], int(event['blocked']), status[event['status']], event['score']]

for route in reference['routes']:
    commands = ['load {}'.format(route['level'])] + ['step'] * route['opening']
    for leg in route['legs']:
        commands += ['call {:.17g}'.format(leg['heading'])] + ['step'] * leg['steps']
    result = subprocess.run([binary], input='\n'.join(commands)+'\n', text=True, capture_output=True, check=True)
    final = result.stdout.splitlines()[-1].split()
    assert int(final[3]) == route['calls'] and int(final[4]) == route['moves']
    assert int(final[6]) == 1 and int(final[7]) == 1000
print('Passed: {} original echo records, {} movement transitions, and all {} optimal escapes.'.format(total, len(expected), len(reference['routes'])))

