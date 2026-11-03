"""Validate the sparse secondary plant layer in native KW3 chunks."""
import struct

def read(data,pos,height,columns,palette=65536):
    if data[:4]==b'KW2\0':
        if pos!=len(data):raise ValueError('Trailing chunk data')
        return []
    if data[:4]!=b'KW3\0' or pos+2>len(data):raise ValueError('Missing snow layers')
    count=struct.unpack_from('<H',data,pos)[0];pos+=2
    if not 0<count<=256 or len(data)-pos!=count*5:raise ValueError('Invalid snow layer count')
    result=[];seen=set()
    for i in range(count):
        col,y,block=struct.unpack_from('<BHH',data,pos+i*5)
        if col in seen or not 0<y<height or block not in (36,37,38,40) or block>=palette or columns[col][y]!=146:raise ValueError('Invalid snow plant')
        if block in (38,40) and (y+1>=height or columns[col][y+1]!=block+1):raise ValueError('Invalid tall snow plant')
        seen.add(col);result.append((col,y,block))
    return result
