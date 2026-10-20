"""Regression for valid mushroom IDs rejected by the former 149-entry limit."""
import argparse, struct, sys, tempfile
from pathlib import Path
root = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root / 'tools'))
from runtime_palette import sizes
from verify_water import decode
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);args=p.parse_args()
args.output.mkdir(parents=True,exist_ok=True)
palette,biomes=sizes()
assert palette>167
with tempfile.TemporaryDirectory(dir=args.output,prefix='palette-') as folder:
    f=Path(folder)/'chunk.bin'
    def chunk(block): return b'KW2\0'+bytes(256)+struct.pack('<HHH',1,384,block)*256
    f.write_bytes(chunk(167));assert (decode(f,palette,biomes)==167).all()
    f.write_bytes(chunk(palette))
    try: decode(f,palette,biomes)
    except AssertionError: pass
    else: raise AssertionError('Out-of-palette blocks must still be rejected')
print(f'PASS: mushroom 167 accepted; invalid {palette} rejected; palette={palette}, biomes={biomes}')
