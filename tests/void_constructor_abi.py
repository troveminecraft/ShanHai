"""Replay the shipped constructor detour with the native scalar argument ABI.

Requires pefile, unicorn and llvm-pdbutil. Does not load the DLL or start Minecraft.
The two called functions are observed/stubbed; their game-side work is not tested.
"""
import argparse
import re
import struct
import subprocess
from pathlib import Path
import pefile
from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE, UC_HOOK_MEM_INVALID
from unicorn.x86_const import UC_X86_REG_RIP, UC_X86_REG_RSP, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_RAX


def check(dll, pdb, pdbutil, broken=False):
    pe = pefile.PE(str(dll), fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    raw = subprocess.check_output([str(pdbutil), 'dump', '-publics', str(pdb)], text=True, encoding='utf-8')
    symbols = {}
    for match in re.finditer(r'S_PUB32[^\n]*`([^`]+)`\s+flags = [^\n]*addr = ([0-9]+):([0-9]+)', raw):
        name, section, offset = match.groups()
        if not 1 <= int(section) <= len(pe.sections): continue
        symbols[name] = base + pe.sections[int(section)-1].VirtualAddress + int(offset)
    def symbol(prefix):
        found = [addr for name, addr in symbols.items() if name.startswith(prefix)]
        assert len(found) == 1, (prefix, found)
        return found[0]
    entry = symbol('?detour@VoidGeneratorConstruction@lab@@')
    origin = symbol('?_OriginalFunc@VoidGeneratorConstruction@lab@@')
    bind = symbol('?bindVoidGenerator@lab@@')
    native, stop, stack, obj, dimension = 0x70000000, 0x70001000, 0x71000000, 0x72000000, 0x73000000
    cases = [0] if broken else [0, 1, 255, 256, 65535]
    for biome in cases:
        u = Uc(UC_ARCH_X86, UC_MODE_64)
        u.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
        u.mem_write(base, pe.get_memory_mapped_image())
        for address, size in [(native, 8192), (stack, 0x10000), (obj, 4096), (dimension, 4096)]:
            u.mem_map(address, size)
        u.mem_write(origin, struct.pack('<QQ', native, 0))
        sp = stack + 0x8008
        u.mem_write(sp, struct.pack('<Q', stop))
        for register, value in [(UC_X86_REG_RSP,sp),(UC_X86_REG_RCX,obj),(UC_X86_REG_RDX,dimension),(UC_X86_REG_R8,biome)]:
            u.reg_write(register,value)
        calls, faults = [], []
        def intercept(uc, address, size, _):
            if address not in (native, bind): return
            if address == native:
                assert uc.reg_read(UC_X86_REG_RCX) == obj
                assert uc.reg_read(UC_X86_REG_RDX) == dimension
                assert uc.reg_read(UC_X86_REG_R8) & 0xffff == biome
                uc.reg_write(UC_X86_REG_RAX,obj)
                calls.append('native')
            else:
                assert calls == ['native'] and uc.reg_read(UC_X86_REG_RCX) == obj
                calls.append('bind')
                uc.reg_write(UC_X86_REG_RAX,0)  # binding must not overwrite constructor result
            rsp = uc.reg_read(UC_X86_REG_RSP)
            destination = struct.unpack('<Q', uc.mem_read(rsp,8))[0]
            uc.reg_write(UC_X86_REG_RSP,rsp+8)
            uc.reg_write(UC_X86_REG_RIP,destination)
        def invalid(uc, access, address, size, value, _):
            faults.append((address, uc.reg_read(UC_X86_REG_RIP)-base))
            return False
        u.hook_add(UC_HOOK_CODE,intercept)
        u.hook_add(UC_HOOK_MEM_INVALID,invalid)
        try:
            u.emu_start(entry,stop,count=1000)
        except UcError:
            if not broken: raise
        if broken:
            assert faults and faults[0][0] == 0 and not calls, (faults,calls)
            print(f'{dll.name}: reproduced null read at RVA {faults[0][1]:#x} with native biome ID 0')
        else:
            assert not faults and calls == ['native','bind'], (faults,calls)
            assert u.reg_read(UC_X86_REG_RIP)==stop
            assert u.reg_read(UC_X86_REG_RSP)==sp+8
            assert u.reg_read(UC_X86_REG_RAX)==obj
            print(f'{dll.name}: native biome ID {biome}, constructor arguments, bind order and return value passed')

if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--dll',type=Path,required=True);p.add_argument('--pdb',type=Path,required=True)
    p.add_argument('--pdbutil',type=Path,required=True);p.add_argument('--expect-broken',action='store_true')
    a=p.parse_args();check(a.dll,a.pdb,a.pdbutil,a.expect_broken)
