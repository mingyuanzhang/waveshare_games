#!/bin/bash
set -euo pipefail
ECHO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPO_ROOT="$(cd "$ECHO_ROOT/../.." && pwd)"
mkdir -p "$ECHO_ROOT/.build"
python3 - "$REPO_ROOT" "$ECHO_ROOT" <<'PY'
from pathlib import Path
import sys
repo, project = map(Path, sys.argv[1:])
source = (repo / 'echo/scripts/solve-levels.ts').read_text()
marker = 'let bad = 0;'
if source.count(marker) != 1:
    raise SystemExit('The original solver changed; review its reference-test adapter.')
adapter = source.split(marker)[0].replace("'../src/game/", "'../../../echo/src/game/")
(project / '.build/solver.ts').write_text(adapter + '\nexport { solve, replay };\n')
PY
"$REPO_ROOT/echo/node_modules/.bin/rolldown" "$ECHO_ROOT/scripts/export-game.ts" \
  -f esm -p node -o "$ECHO_ROOT/.build/export-game.mjs"
node "$ECHO_ROOT/.build/export-game.mjs" "$ECHO_ROOT"
