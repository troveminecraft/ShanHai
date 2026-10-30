"""Seed-worker protocol and validated native chunk decoding."""
import json, subprocess, struct
import numpy as np

def run(exe,source,requests):
    r=subprocess.run([str(exe),str(source)],input=''.join(json.dumps(q)+'\n' for q in requests),text=True,capture_output=True,check=True)
    return [json.loads(s) for s in r.stdout.splitlines()]

def decode(path,palette=43,biomes=20,height=384):
    if type(height) is not int or height not in (384,576):raise ValueError('Unsupported water-check chunk height')
    b=path.read_bytes();wide=b[:4]==b'KW2\0';offset=4 if wide else 0;pos=256+offset;cols=[]
    if len(b)<pos or max(b[offset:offset+256])>=biomes:raise ValueError('Invalid chunk biome palette')
    for _ in range(256):
        if pos+2>len(b):raise ValueError('Truncated chunk column')
        n=struct.unpack_from('<H',b,pos)[0];pos+=2;col=[]
        for _ in range(n):
            if pos+(4 if wide else 3)>len(b):raise ValueError('Truncated chunk run')
            length,block=struct.unpack_from('<HH' if wide else '<HB',b,pos);pos+=4 if wide else 3
            if not 0<length<=height-len(col) or block>=palette:raise ValueError('Invalid chunk run or palette')
            col.extend([block]*length)
        if len(col)!=height:raise ValueError('Chunk height differs from expected recipe')
        cols.append(col)
    if pos!=len(b):raise ValueError('Trailing chunk data')
    return np.array(cols,dtype=np.uint16).reshape(16,16,height)
