"""Offline generation extent shares the native build boundary."""
from pathlib import Path
import re
_header=(Path(__file__).resolve().parents[1]/"src/WorldBounds.h").read_text()
CHUNKS=int(re.search(r"Chunks=(\d+)",_header).group(1))
HALF=CHUNKS*8
SIDE=HALF*2
MIN=-HALF
MAX=HALF-1
