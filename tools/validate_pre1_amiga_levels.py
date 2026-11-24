#!/usr/bin/env python3
"""TH-15 research: compare native scenes with isolated original Amiga routines.

No OS/game loop/trainer or hardware blitter is executed. Runtime code never
uses this tool. Output contains private original sections and previews and must
stay under build/. No commercial bytes are embedded in the checker.
SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse,hashlib,json,struct,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from audit_amiga_data import lzss
MAIN='d48f952eca0d74547d9e99531b54ac05beaabb42a55203fec510279bd9bd5295'

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['datapath','amitools','probe','output']:parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();root=args.datapath.resolve();out=args.output.resolve()
    if out.exists() or not out.is_relative_to(ROOT/'build'):raise ValueError('Use a new private build/ output')
    def snapshot():return {str(p.relative_to(root)):(p.stat().st_size,p.stat().st_mtime_ns,hashlib.sha256(p.read_bytes()).hexdigest()) for p in root.rglob('*') if p.is_file()}
    before=snapshot();assert hashlib.sha256((root/'main').read_bytes()).hexdigest()==MAIN
    sys.path.insert(0,str(args.amitools.resolve()))
    from amitools.binfmt.hunk.BinFmtHunk import BinFmtHunk
    from unicorn import Uc,UC_ARCH_M68K,UC_MODE_BIG_ENDIAN,UC_HOOK_CODE
    from unicorn.m68k_const import UC_CPU_M68K_M68000,UC_M68K_REG_A7,UC_M68K_REG_PC,UC_M68K_REG_D0
    image=BinFmtHunk().load_image(str(root/'main'));assert [s.get_size() for s in image.segments]==[8,5332,8520,89164]
    bases=[0x20000,0x50000,0x80000,0xb0000];sections=[];out.mkdir(parents=True)
    for segment in image.segments:
        data=bytearray(segment.get_size());data[:len(segment.get_data() or b'')]=segment.get_data() or b''
        for target in segment.get_reloc_to_segs():
            for relocation in segment.get_reloc(target).get_relocs():
                offset=relocation.get_offset();assert relocation.get_width()==2 # width is log2(bytes)
                value=int.from_bytes(data[offset:offset+4],'big')+bases[image.segments.index(target)]
                data[offset:offset+4]=value.to_bytes(4,'big')
        sections.append(bytes(data));(out/f'section-{len(sections)-1}.bin').write_bytes(data)
    result=subprocess.run([str(args.probe.resolve()),'--datapath',str(root),'--output',str(out/'native')],capture_output=True,text=True,timeout=60)
    if result.returncode:raise ValueError(result.stderr)
    native=json.loads((out/'native/report.json').read_text());assert native['section_sha256']==[hashlib.sha256(s).hexdigest() for s in sections]
    cases=[];palettes=[]
    def machine():
        u=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);u.ctl_set_cpu_model(UC_CPU_M68K_M68000);u.mem_map(0x10000,0x300000)
        for base,data in zip(bases,sections):u.mem_write(base,data)
        return u
    for number,counts,routine in [(1,0xd40,0xb71f4),(3,0x189a,0xc0fb8),(5,0x1952,0xb71f4),(7,0x19d8,0xc4064)]:
        dat=(root/f'level{number}.dat').read_bytes();tile_size,object_size=struct.unpack_from('>HH',dat);assert len(dat)==4+tile_size+object_size
        raw,_=lzss((root/f'ELEML{number}.MAT').read_bytes());elements=int.from_bytes(raw[:2],'big')
        for screen in range(19):
            u=machine();plots=[];rectangles=[];commands=[]
            def put(address,value,size=2):u.mem_write(address,(value&((1<<(8*size))-1)).to_bytes(size,'big'))
            def long(address):return int.from_bytes(u.mem_read(address,4),'big')
            def signed(n):return n-0x100000000 if n>=0x80000000 else n
            def ret():
                sp=u.reg_read(UC_M68K_REG_A7);u.reg_write(UC_M68K_REG_PC,long(sp));u.reg_write(UC_M68K_REG_A7,sp+4);u.reg_write(UC_M68K_REG_D0,0)
            def hook(cpu,pc,size,unused):
                if pc==0x10002:cpu.emu_stop();return
                if pc==0xb130a:
                    sp=cpu.reg_read(UC_M68K_REG_A7);plots.append([signed(long(sp+4)),signed(long(sp+8)),long(sp+12)-1000,long(sp+16)]);commands.append([0,*plots[-1][:3],0,0]);ret();return
                if pc==0xb2d60:
                    sp=cpu.reg_read(UC_M68K_REG_A7);rectangles.append([signed(long(sp+i)) for i in [4,8,12,16,20]]);commands.append([1,*rectangles[-1]]);ret();return
                if pc in [0xb3088,0xb2d3e]:ret();return
                if 0xb70b2<=pc<0xb735a or 0xc0fb8<=pc<0xc132c or 0xc4064<=pc<0xc42ee or 0xc5230<=pc<0xc5320 or 0xbcd98<=pc<0xbd1a0:return
                raise ValueError((number,screen,hex(pc),'routine escaped'))
            u.hook_add(UC_HOOK_CODE,hook);u.mem_write(0x180000,dat[4:4+tile_size]);dur=(root/f'LEVEL{number}.DUR').read_bytes()
            u.mem_write(0x509ac,b''.join(dur[i:i+2][::-1] for i in range(0,960,2)));put(0x50d6c,0x1a0000,4);put(0x509a6,1000);put(0x509aa,1000+elements);put(0x50988,number)
            effective=10 if number==1 and screen==11 else screen;sp=0x2f0000;put(sp,0x10002,4);put(sp+4,0x80000+counts,4);put(sp+8,0x180000,4);put(sp+12,effective,4)
            u.reg_write(UC_M68K_REG_A7,sp);u.emu_start(routine,0,count=2000000);assert u.reg_read(UC_M68K_REG_PC)==0x10002,'scene instruction limit'
            bitmap=bytes(u.mem_read(0x1a0000,8000));actual=native['levels'][(number-1)//2]['screens'][screen];assert actual['scene_collision_sha256']==hashlib.sha256(bitmap).hexdigest(),(number,screen,'collision')
            assert [[c[1],c[2],c[3],0] for c in actual['commands'] if c[0]==0]==plots,(number,screen,'sprites')
            assert [[c[1],c[2],c[3],c[4],c[5]] for c in actual['commands'] if c[0]==1]==rectangles,(number,screen,'rectangles')
            assert actual['commands']==commands,(number,screen,'draw order')
            (out/f'collision-{number}-{screen}.bin').write_bytes(bitmap)
            cases.append(dict(level=number,screen=screen,plots=len(plots),rectangles=len(rectangles),original_match=True));print(f'Level {number} screen {screen}: draw commands and collision match',flush=True)
    for index,routine in enumerate([0xbcd98,0xbce44,0xbcef0,0xbcf9c,0xbd048,0xbd0f4]):
        u=machine()
        def palette_hook(cpu,pc,size,unused):
            if pc==0x10002:cpu.emu_stop();return
            if not 0xbcd98<=pc<0xbd1a0:raise ValueError('Palette routine escaped')
        u.hook_add(UC_HOOK_CODE,palette_hook);u.reg_write(UC_M68K_REG_A7,0x2efffc);u.mem_write(0x2efffc,(0x10002).to_bytes(4,'big'));u.emu_start(routine,0,count=100000)
        assert u.reg_read(UC_M68K_REG_PC)==0x10002
        actual=list(struct.unpack('>16H',u.mem_read(0x80070,32)));assert actual==native['palettes_rgb12'][index]
        palettes.append(dict(index=index,colors=16,original_match=True))
    assert before==snapshot()
    report=dict(schema_version=1,main_sha256=MAIN,playable=False,section_sha256=native['section_sha256'],cases=cases,palettes=palettes,originals_unchanged=True,scope='isolated static scenes and collision; no game loop or screen transition animation')
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
