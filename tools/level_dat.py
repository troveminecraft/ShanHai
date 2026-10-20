"""Small lossless little-endian NBT reader/writer for stopped Bedrock worlds."""
import struct,json,pathlib,argparse
def read_file(path):
 data=pathlib.Path(path).read_bytes(); pos=8
 def take(n):
  nonlocal pos
  b=data[pos:pos+n];pos+=n
  if len(b)!=n:raise ValueError('truncated NBT')
  return b
 def num(f):return struct.unpack('<'+f,take(struct.calcsize('<'+f)))[0]
 def string():return take(num('H')).decode('utf-8')
 def payload(t):
  if t in range(1,7):return num({1:'b',2:'h',3:'i',4:'q',5:'f',6:'d'}[t])
  if t==7:return take(num('i'))
  if t==8:return string()
  if t==9:
   subtype=num('B');n=num('i');return (subtype,[payload(subtype) for _ in range(n)])
  if t==10:
   tags={}
   while True:
    st=num('B')
    if not st:return tags
    name=string();tags[name]=(st,payload(st))
  if t in (11,12):return [num('i' if t==11 else 'q') for _ in range(num('i'))]
  raise ValueError(t)
 typ=num('B');name=string();root=payload(typ)
 if pos!=len(data):raise ValueError('NBT trailing bytes')
 return struct.unpack('<I',data[:4])[0],name,root
def write_file(path,version,name,root):
 def num(f,v):return struct.pack('<'+f,v)
 def string(v):
  b=v.encode('utf-8');return num('H',len(b))+b
 def payload(t,v):
  if t in range(1,7):return num({1:'b',2:'h',3:'i',4:'q',5:'f',6:'d'}[t],v)
  if t==7:return num('i',len(v))+v
  if t==8:return string(v)
  if t==9:return num('B',v[0])+num('i',len(v[1]))+b''.join(payload(v[0],a) for a in v[1])
  if t==10:return b''.join(num('B',st)+string(k)+payload(st,sv) for k,(st,sv) in v.items())+b'\0'
  if t in (11,12):return num('i',len(v))+b''.join(num('i' if t==11 else 'q',a) for a in v)
  raise ValueError(t)
 b=b'\x0a'+string(name)+payload(10,root);pathlib.Path(path).write_bytes(num('I',version)+num('I',len(b))+b)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('source');p.add_argument('--output');p.add_argument('--name');p.add_argument('--spawn-y',type=int,default=100);args=p.parse_args()
 version,name,root=read_file(args.source)
 if args.output:
  if root.get('Generator') != (3,2) or 'TheVoid' not in root.get('FlatWorldLayers',(8,''))[1]:
   raise ValueError('Template must be the verified flat TheVoid world')
  root.update({'LevelName':(8,args.name),'SpawnX':(3,0),'SpawnZ':(3,0),'SpawnY':(3,args.spawn_y),'Time':(4,0),'LastPlayed':(4,0)})
  write_file(args.output,version,name,root)
 print(json.dumps({k:v for k,(t,v) in root.items() if k in ['LevelName','RandomSeed','Generator','SpawnX','SpawnY','SpawnZ','FlatWorldLayers']},ensure_ascii=True))
