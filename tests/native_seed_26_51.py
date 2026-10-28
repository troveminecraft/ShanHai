"""Check 1.26.51.01's real seed key and getters without launching Minecraft.
Only allocation-free native paths run; vtable dispatch and memcmp are stubbed.
Map nodes are fixtures, not a claim that a game world has been entered.
"""
import argparse,struct,pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_64,UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('image');a=p.parse_args()
pe=pefile.PE(a.image,fast_load=True);base=pe.OPTIONAL_HEADER.ImageBase
assert pe.FILE_HEADER.TimeDateStamp==0x6aa482fd
u=Uc(UC_ARCH_X86,UC_MODE_64);mapped=set()
def region(address,size):
 for page in range(address&~4095,(address+size+4095)&~4095,4096):
  if page not in mapped:u.mem_map(page,4096);mapped.add(page)
def native(rva,size):
 region(base+rva,size);u.mem_write(base+rva,pe.get_data(rva,size))
for rva,size in [(0x64e48c,0x43),(0x62f050,0x25e),(0x1178860,0x85)]:native(rva,size)
region(base+0x11d2c540,0x30);region(base+0xf0fbaa8,8);region(base+0xe5a6380,1)
mem=0x70000000;region(mem,0x20000)
stack,stop,dispatch,level,data,vt=mem+0x1000,mem+0x2000,mem+0x3000,mem+0x4000,mem+0x5000,mem+0x6000
pack=lambda *v:struct.pack('<'+'Q'*len(v),*v)
def q(address,value):u.mem_write(address,pack(value))
def ret(value):
 sp=u.reg_read(UC_X86_REG_RSP);u.reg_write(UC_X86_REG_RAX,value);u.reg_write(UC_X86_REG_RIP,struct.unpack('<Q',u.mem_read(sp,8))[0]);u.reg_write(UC_X86_REG_RSP,sp+8)
def hook(uc,address,size,_):
 if address==dispatch:ret(data)
 elif address==base+0xe5a6380:
  n=uc.reg_read(UC_X86_REG_R8);x=bytes(uc.mem_read(uc.reg_read(UC_X86_REG_RCX),n));y=bytes(uc.mem_read(uc.reg_read(UC_X86_REG_RDX),n));ret(0 if x==y else 1)
u.hook_add(UC_HOOK_CODE,hook)
def run(rva,rcx=0,rdx=0,until=None):
 sp=stack+0x808;q(sp,stop);u.reg_write(UC_X86_REG_RSP,sp);u.reg_write(UC_X86_REG_RCX,rcx);u.reg_write(UC_X86_REG_RDX,rdx);u.reg_write(UC_X86_REG_XMM6,0)
 u.emu_start(base+rva,base+until if until else stop,count=2000)
 assert u.reg_read(UC_X86_REG_RIP)==(base+until if until else stop)
 return u.reg_read(UC_X86_REG_RAX)
run(0x64e48c,until=0x64e4cf)
key=bytes(u.mem_read(base+0x11d2c540,48));assert key[8:12]==b'seed' and struct.unpack_from('<Q',key)[0]==0x92eb667ea331f1e6
q(base+0xf0fbaa8,dispatch);q(level,vt)
def table(offset,node,bits=None,tag=7):
 head=node+0x100;buckets=node+0x200
 u.mem_write(head,pack(head,head));u.mem_write(buckets,pack(*([head]*16)))
 q(data+offset+8,head);q(data+offset+24,buckets);q(data+offset+48,7)
 if bits is not None:
  u.mem_write(node,pack(head,head)+key);q(node+0x40,bits);u.mem_write(node+0x60,bytes([tag]));slot=0x92eb667ea331f1e6&7;u.mem_write(buckets+slot*16,pack(node,node))
for bits in [0,1,2934997776556350534,0x8000000000000000,0xffffffffffffffff]:
 table(0x588,mem+0x8000,bits);table(0x5c8,mem+0x9000)
 assert run(0x11788a0,level,mem+0xa000)==mem+0xa000
 assert struct.unpack('<Q',u.mem_read(mem+0xa000,8))[0]==bits
 assert run(0x1178860,level)==bits&0xffffffff
 # This is the old adapter's NBT-name lookup, against the same native map.
 old=b'RandomSeed';h=0xcbf29ce484222325
 for c in old:h=((h*0x100000001b3)^c)&0xffffffffffffffff
 u.mem_write(mem+0xb000,pack(h)+old.ljust(16,b'\0')+pack(len(old),15,0))
 assert run(0x62f050,data,mem+0xb000)==0
 table(0x5c8,mem+0x9000,42)
 run(0x11788a0,level,mem+0xa000);assert struct.unpack('<Q',u.mem_read(mem+0xa000,8))[0]==42
print('Native 26.51 seed initializer, property lookup, overrides, 64/32-bit getters: passed; old RandomSeed lookup reproduced null for every case')
