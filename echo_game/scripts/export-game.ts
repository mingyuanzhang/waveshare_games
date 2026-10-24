import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { LEVELS } from '../../../echo/src/game/levels';
import { BEAM, emit } from '../../../echo/src/game/echo';
import { initialRun, runReducer, scoreRun } from '../../../echo/src/game/run';
import { TEST_WORLD } from '../../../echo/src/game/world';
import { solve, replay } from '../.build/solver';

const root = process.argv[2];
if (!root) throw new Error('Supply the echo_game directory.');
const levels = LEVELS;
const number = (n: number) => Number.isInteger(n) ? `${n}.0` : n.toPrecision(17);
const vector = (p: {x: number, y: number}) => `{${number(p.x)}, ${number(p.y)}}`;
const material = {stone: 'Stone', metal: 'Metal', water: 'Water', soft: 'Soft'};
const notice = '// Generated from echo/src/game/levels.ts by scripts/sync-game.sh.\n';
let header = notice + '#pragma once\n#include "EchoEngine.h"\nnamespace echo_game {\n';
header += `constexpr size_t kLevelCount = ${levels.length};\nextern const Level kLevels[kLevelCount];\n}\n`;
fs.writeFileSync(path.join(root, 'firmware/echo_game/EchoLevels.h'), header);
let data = notice + '#include "EchoLevels.h"\nnamespace echo_game {\n';
levels.forEach((level, i) => {
  data += `static const Segment kWalls${i}[] = {\n`;
  level.world.walls.forEach(w => {
    data += `  {${vector(w.a)}, ${vector(w.b)}, Material::${material[w.material]}},\n`;
  });
  data += '};\n';
});
data += 'const Level kLevels[kLevelCount] = {\n';
levels.forEach((level, i) => {
  data += `  {"${level.id}", "${level.name}", ${number(level.world.size.w)}, ${number(level.world.size.h)}, `;
  data += `kWalls${i}, ${level.world.walls.length}, ${vector(level.start)}, ${number(level.startHeading)}, `;
  data += `${vector(level.exit.a)}, ${vector(level.exit.b)}, ${level.best.pings}, ${level.best.moves}},\n`;
});
data += '};\n}\n';
fs.writeFileSync(path.join(root, 'firmware/echo_game/EchoLevels.cpp'), data);
console.log(`Exported geometry for ${levels.length} rooms.`);

// Cache the expensive original solver, invalidating when its inputs or rules change.
const original = path.resolve(root, '../../echo');
const fingerprint = createHash('sha256').update(JSON.stringify(levels));
for (const file of ['scripts/solve-levels.ts', 'src/game/geometry.ts', 'src/game/run.ts']) {
  fingerprint.update(fs.readFileSync(path.join(original, file)));
}
const key = fingerprint.digest('hex');
const cacheFile = path.join(root, '.build/optimal-routes.json');
let cache: any = {key, routes: {}};
if (fs.existsSync(cacheFile)) {
  const saved = JSON.parse(fs.readFileSync(cacheFile, 'utf8'));
  if (saved.key === key) cache = saved;
}

// Expected values come from the original TypeScript implementation, not the port.
const reference: any = {echoes: [], traces: [], routes: []};
levels.forEach((level, index) => {
  for (let bearing = 0; bearing < 12; bearing++) {
    const heading = bearing * Math.PI / 6 + 0.037;
    reference.echoes.push({level: index, origin: level.start, heading,
      values: emit(level.world, level.start, heading, BEAM)});
  }
  let state = initialRun(level);
  const actions: any[] = [];
  for (let i = 0; i < 48; i++) {
    const action = i % 4 === 0 ? {type: 'CALL', heading: (i * 1.739) % (2 * Math.PI)}
      : i % 4 === 2 ? {type: 'TURN', heading: (i * 0.723) % (2 * Math.PI)} : {type: 'STEP'};
    state = runReducer(state, action as any);
    actions.push({action, pos: state.pos, heading: state.heading, calls: state.pings,
      moves: state.moves, blocked: state.blocked, status: state.status, score: scoreRun(state)});
  }
  reference.traces.push({level: index, actions});

  let found = cache.routes[level.id];
  if (!found) {
    console.log(`Solving ${index + 1}/${levels.length}: ${level.name} with the original solver...`);
    found = solve(level);
    if (!found) throw new Error(`Original solver cannot escape ${level.name}`);
    cache.routes[level.id] = found;
    fs.writeFileSync(cacheFile, JSON.stringify(cache));
  }
  const drift = replay(level, found);
  if (drift || found.pings !== level.best.pings || found.moves !== level.best.moves)
    throw new Error(`Original route/par disagreement in ${level.name}: ${drift || JSON.stringify(found)}`);
  reference.routes.push({level: index, opening: found.moves - found.legs.reduce((n: number, leg: any) => n + leg.steps, 0),
    legs: found.legs, calls: found.pings, moves: found.moves});
});
for (let bearing = 0; bearing < 8; bearing++) {
  const heading = bearing * Math.PI / 4 + 0.021;
  reference.echoes.push({level: -1, origin: {x: 18, y: 55}, heading,
    values: emit(TEST_WORLD, {x: 18, y: 55}, heading, BEAM)});
}
reference.testWorld = TEST_WORLD;
fs.mkdirSync(path.join(root, '.build'), {recursive: true});
fs.writeFileSync(path.join(root, '.build/reference.json'), JSON.stringify(reference));
console.log(`Exported ${levels.length} rooms, ${reference.echoes.length} echo cases, and ${reference.routes.length} optimal routes.`);

