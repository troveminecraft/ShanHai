"""Use the shared runtime palette when validating generated chunk samples."""
import json
from functools import cache
from pathlib import Path

@cache
def sizes():
    profile = json.loads((Path(__file__).resolve().parents[1] / 'data/runtime-template.json').read_bytes())
    palette, biomes = len(profile['palette']), len(profile['biomeNames'])
    if not 0 < palette <= 65536 or not 0 < biomes <= 256:
        raise ValueError('Invalid runtime palette sizes')
    return palette, biomes
