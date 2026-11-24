#!/usr/bin/env python3
"""Research only: contrast COLOR14 scanlines with the isolated original Copper routine.

Uses a private TH-14 actor snapshot, never installed or used by the runtime.
No original code, palettes or pictures are distributed with this tool.
"""
import argparse,json
from pathlib import Path
from unicorn import Uc,UC_ARCH_M68K,UC_MODE_BIG_ENDIAN,UC_HOOK_CODE
from unicorn.m68k_const import UC_CPU_M68K_M68000,UC_M68K_REG_A7,UC_M68K_REG_PC

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();reference=args.reference.resolve();output=args.output.resolve()
    if output.exists() or not output.is_relative_to(Path(__file__).resolve().parents[1]/'build'):
        raise ValueError('Use a new private output under build')
    report=json.loads((reference/'report.json').read_text())
    assert report['main_sha256']=='18ca46dd02a3ad3623b03d425f0e0ae489b5090498882aea6d2578161010bc45'
    snapshot=(reference/'enemy-state-1.bin').read_bytes();assert len(snapshot)==0x300000
    rows=[]
    for level in range(1,17):
        cpu=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);cpu.ctl_set_cpu_model(UC_CPU_M68K_M68000)
        cpu.mem_map(0x10000,0x300000);cpu.mem_write(0x10000,snapshot)
        def put(address,value,size=2):cpu.mem_write(address,value.to_bytes(size,'big'))
        def hook(u,pc,size,unused):
            if pc==0x10002:u.emu_stop();return
            if pc==0xec17c: # Skip hardware Copper publication; retain both lists.
                sp=u.reg_read(UC_M68K_REG_A7)
                u.reg_write(UC_M68K_REG_PC,int.from_bytes(u.mem_read(sp,4),'big'))
                u.reg_write(UC_M68K_REG_A7,sp+4);return
            if 0xec35c<=pc<0xec554 or 0xf18fa<=pc<0xf1990:return
            raise ValueError(f'Original Copper routine escaped to {pc:x}')
        cpu.hook_add(UC_HOOK_CODE,hook)
        put(0x8052c,0x200000,4);put(0x80530,0x210000,4)
        data=bytes(cpu.mem_read(0xb0000,0x3000));constant=int.from_bytes(data[0x340+(level-1)*2:0x342+(level-1)*2],'big')
        put(0xb02c6,constant);sp=0x2f0000;put(sp,0x10002,4);put(sp+4,level,4)
        cpu.reg_write(UC_M68K_REG_A7,sp);cpu.emu_start(0xec35c,0,count=500000)
        assert cpu.reg_read(UC_M68K_REG_PC)==0x10002,'Copper instruction limit'
        kind=data[0x1abc+level-1];assert kind<=6
        expected=[]
        for y in range(176):
            offset=0x1978+(kind-1)*54+2*(y//7+(y%7==5)) if kind else 0x340+(level-1)*2
            expected.append(int.from_bytes(data[offset:offset+2],'big'))
        for base in (0x200000,0x210000):
            actual=[int.from_bytes(cpu.mem_read(base+120+y*8+6,2),'big') for y in range(176)]
            assert actual==expected,(level,kind,'scanline mismatch')
        rows.append(dict(level=level,gradient=kind,scanlines=176,original_match=True))
    output.mkdir(parents=True);(output/'report.json').write_text(json.dumps(dict(schema_version=1,cases=rows),indent=2)+'\n')
    print('16 maps, 2816 scanline colors and both Copper lists match the original routine')
if __name__=='__main__':main()
