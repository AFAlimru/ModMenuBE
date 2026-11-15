"""Catch the local1 startup failure at the shipped binary dependency boundary."""
from pathlib import Path
import argparse
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from pe_audit import PeImage, audit_runtime_dependencies

parser = argparse.ArgumentParser()
parser.add_argument('dll', type=Path)
parser.add_argument('--game-d3d12', type=Path)
args = parser.parse_args()
data = args.dll.read_bytes()
if args.game_d3d12:
    exports = PeImage(args.game_d3d12.read_bytes()).exports()
    requested = PeImage(data).imports().get('d3d12.dll', [])
    missing = [symbol for symbol in requested
               if (symbol not in exports if isinstance(symbol, int) else symbol not in exports.values())]
    print(f'App-local dependency comparison: requested={requested}, missing={missing}')
try:
    audit_runtime_dependencies(data)
except ValueError as error:
    print(f'FAIL: {error}')
    sys.exit(1)
print('PASS: no eager d3d12.dll import; app-local ordinal differences cannot reject this DLL at load time')
