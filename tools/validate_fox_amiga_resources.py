#!/usr/bin/env python3
"""TH-14 reference check: bounded pinned packer, resource, level and player routines.

No Amiga OS, trainer or complete game loop is executed. Isolated PAT replay
stores audio registers in private mapped memory. External amitools
and Unicorn are used only by this research tool, never by the native reader.
No executable bytes are embedded. Output must be outside the original directory.
SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

MAIN_SHA = '18ca46dd02a3ad3623b03d425f0e0ae489b5090498882aea6d2578161010bc45'


def snapshot(root):
    return {str(p.relative_to(root)): (p.stat().st_size, p.stat().st_mtime_ns,
            hashlib.sha256(p.read_bytes()).hexdigest())
            for p in root.rglob('*') if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--datapath', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--amitools', type=Path, required=True)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--check-levels', action='store_true', help='Compare native POIL initialization and collision queries')
    parser.add_argument('--check-motion', action='store_true', help='Compare isolated terrain-only player ticks')
    parser.add_argument('--check-audio', action='store_true', help='Compare PAT replay interrupts with the original')
    args = parser.parse_args()
    root, output = args.datapath.resolve(), args.output.resolve()
    if output == root or root in output.parents or output.exists():
        raise ValueError('Use a new output directory outside original data')
    before = snapshot(root)
    raw = (root/'main').read_bytes()
    if hashlib.sha256(raw).hexdigest() != MAIN_SHA:
        raise ValueError('Executable revision not recognized: refuse to emulate')
    sys.path.insert(0, str(args.amitools.resolve()))
    from amitools.binfmt.hunk.BinFmtHunk import BinFmtHunk
    from unicorn import Uc, UC_ARCH_M68K, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE
    from unicorn.m68k_const import (UC_CPU_M68K_M68000, UC_M68K_REG_A1,
        UC_M68K_REG_A4, UC_M68K_REG_A7, UC_M68K_REG_D0, UC_M68K_REG_D6,
        UC_M68K_REG_PC, UC_M68K_REG_A0, UC_M68K_REG_A2, UC_M68K_REG_A3, UC_M68K_REG_D1, UC_M68K_REG_D2, UC_M68K_REG_D3)
    image = BinFmtHunk().load_image(str(root/'main'))
    if [s.get_size() for s in image.segments] != [80, 8, 3680, 9644, 74068, 496, 50324, 516]:
        raise ValueError('Unexpected Hunk layout')
    u = Uc(UC_ARCH_M68K, UC_MODE_BIG_ENDIAN)
    u.ctl_set_cpu_model(UC_CPU_M68K_M68000)
    u.mem_map(0x10000, 0x300000)
    bases = [0x20000+i*0x30000 for i in range(8)]
    for base, segment in zip(bases, image.segments):
        if segment.get_data():
            u.mem_write(base, segment.get_data())
    helper, stop, stack = 0x10000, 0x10002, 0x2f0000
    u.mem_write(helper, b'\x4e\x75\x4e\x71')
    phase, payload, cursor, heap = 'packer', b'', 0, 0x180000

    def be(address):
        return int.from_bytes(u.mem_read(address, 4), 'big')

    def hook(cpu, address, size, user):
        nonlocal cursor, heap
        if address == stop:
            cpu.emu_stop()
            return
        if phase == 'packer':
            if address == helper:
                index = cpu.reg_read(UC_M68K_REG_D6)+1
                if index >= len(bases):
                    raise ValueError('Packer segment index out of bounds')
                cpu.reg_write(UC_M68K_REG_A1, bases[index])
            elif not bases[5] <= address < bases[5]+496:
                raise ValueError('Packer execution escaped its isolated segment')
            return
        if phase == 'audio':
            if 0xe3d6c<=address<0xe43ee:return
            raise ValueError(f'PAT replay escaped to {address:x}')
        if phase == 'motion':
            if address in (0xe6d10,0xe6fb4):
                # Exclude bonus consumption and level completion from this
                # terrain-only differential test. No object/AI steps are run.
                sp=cpu.reg_read(UC_M68K_REG_A7)
                cpu.reg_write(UC_M68K_REG_PC,be(sp));cpu.reg_write(UC_M68K_REG_A7,sp+4)
                return
            ranges=((0xe65d4,0xe69ce),(0xe7028,0xe7a82),(0xe8c34,0xe8da8),(0xe6c74,0xe6c86))
            if any(begin<=address<end for begin,end in ranges):return
            raise ValueError(f'Terrain player step escaped to {address:x}')
        if phase == 'level':
            ranges = ((0xe7b16,0xe8684),(0xeadb0,0xeb450),(0xedbe8,0xeddbc),
                      (0xf18a8,0xf198c),(0xe65d4,0xe661c),(0xe6716,0xe67a4),(0xe6fb4,0xe7028),(0xe46e2,0xe4904))
            if any(begin <= address < end for begin,end in ranges):
                return
            sp = cpu.reg_read(UC_M68K_REG_A7)
            if address == 0xe1d42:  # open the already decoded private stream
                cursor = 0
                cpu.reg_write(UC_M68K_REG_D0, 1)
            elif address == 0xf1dd4:
                requested = be(sp+4)
                if requested != len(payload)+512 or requested>65536:
                    raise ValueError('Unexpected original map allocation')
                cpu.reg_write(UC_M68K_REG_D0,0x190000)
            elif address in (0xf200c,0xf0324,0xf201c):
                pass  # Loading progress UI only; do not enter video/OS code.
            elif address == 0xe1be6:  # logical stream read; no host files or OS
                dest, requested = be(sp+8), be(sp+12)
                if requested > 3060 or cursor+requested > len(payload):
                    raise ValueError('Logical reference read out of bounds')
                if not (stack-4096 <= dest <= stack or 0x180000 <= dest <= 0x280000-requested):
                    raise ValueError('Logical reference destination out of bounds')
                cpu.mem_write(dest, payload[cursor:cursor+requested])
                cursor += requested
            elif address != 0xe1d00:
                raise ValueError(f'Level loader escaped to {address:x}')
            cpu.reg_write(UC_M68K_REG_PC, be(sp))
            cpu.reg_write(UC_M68K_REG_A7, sp+4)
            return
        if 0xe34d0 <= address < 0xe3cd6:
            return
        sp = cpu.reg_read(UC_M68K_REG_A7)
        if address == 0xf1d30:  # Read(handle, buffer, length)
            dest, requested = be(sp+8), be(sp+12)
            if requested > 512 or not 0x180000 <= dest < 0x280000-requested:
                raise ValueError('Reference read outside scratch buffers')
            data = payload[cursor:cursor+requested]
            if data:
                cpu.mem_write(dest, data)
            cursor += len(data)
            cpu.reg_write(UC_M68K_REG_D0, len(data))
        elif address == 0xf1dd4:  # AllocMem(length, flags)
            length = be(sp+4)
            if length > 65536 or heap+length >= 0x230000:
                raise ValueError('Reference allocation limit')
            cpu.mem_write(heap, bytes(length))
            cpu.reg_write(UC_M68K_REG_D0, heap)
            heap = (heap+length+3)&~3
        else:
            raise ValueError(f'Reference decoder escaped to {address:x}')
        # Return to caller directly: no OS implementation is entered.
        cpu.reg_write(UC_M68K_REG_PC, be(sp))
        cpu.reg_write(UC_M68K_REG_A7, sp+4)

    u.hook_add(UC_HOOK_CODE, hook)

    def call(address, arguments=()):
        frame = stop.to_bytes(4, 'big')+b''.join(v.to_bytes(4, 'big') for v in arguments)
        u.mem_write(stack, frame)
        u.reg_write(UC_M68K_REG_A7, stack)
        u.emu_start(address, stop, count=15000000)
        if u.reg_read(UC_M68K_REG_PC) != stop:
            raise ValueError('Reference instruction budget exceeded')
        return u.reg_read(UC_M68K_REG_D0)

    u.reg_write(UC_M68K_REG_A4, helper)
    u.reg_write(UC_M68K_REG_D6, 4)
    call(bases[5])
    phase = 'decoder'
    data = bytes(u.mem_read(bases[3], 9644))
    words = lambda offset, count: [int.from_bytes(data[p:p+2], 'big')
                                  for p in range(offset, offset+count*2, 2)]
    animation_words = []
    for offset in range(0x12da, len(data)-1, 2):
        value=int.from_bytes(data[offset:offset+2],'big')
        if value==0xfc18:break
        animation_words.append(value)
    else:raise ValueError('Missing original enemy animation sentinel')
    code = bytes(u.mem_read(bases[4], image.segments[4].get_size()))
    motion_walk=[int.from_bytes(code[p:p+2],'big') for p in range(0x7a8e,0x7aaa,2)]
    motion_jump=[int.from_bytes(code[p:p+2],'big',signed=True) for p in range(0x7a82,0x7a8c,2)]
    palette = dict(walk_frames=motion_walk,jump_impulses=motion_jump,
                   main_sha256=MAIN_SHA, palette=words(0x2aa, 16),
                   level_color14=words(0x340, 16),
                   enemy_animation_frames=[i in animation_words for i in range(232)])
    output.mkdir(parents=True)
    palette_path = output/'palette.json'
    palette_path.write_text(json.dumps(palette, indent=2)+'\n')
    process = subprocess.run([str(args.probe.resolve()), '--datapath', str(root),
        '--output', str(output/'preview'), '--palette', str(palette_path)],
        capture_output=True, text=True, timeout=60)
    if process.returncode:
        raise ValueError(process.stderr)
    native = json.loads(process.stdout)
    report = dict(schema_version=1, main_sha256=MAIN_SHA, playable=False,
                  reference=('isolated original 68000 resource, map/POIL initialization and query routines'
                             if args.check_levels or args.check_motion else 'isolated original 68000 resource decoders'), resources=[])
    decoded_resources = {}
    for row in native['resources']:
        packed = (root/row['name']).read_bytes()
        payload, cursor, heap = packed[6:], 0, 0x180000
        method, expected = packed[0], row['decoded_size']
        call(0xe3582 if method == 2 else 0xe3b98, (1,))
        decoded = bytearray()
        while len(decoded) < expected:
            count = min(512, expected-len(decoded))
            actual = call(0xe384c if method == 2 else 0xe3c90, (1, 0x240000, count)) & 0xffff
            if actual < count or actual > count+4096 or actual > expected-len(decoded):
                raise ValueError(f'Reference short output: {row["name"]}: {actual}/{count}')
            decoded.extend(u.mem_read(0x240000, actual))
        sha = hashlib.sha256(decoded).hexdigest()
        if sha != row['decoded_sha256']:
            raise ValueError(f'Native/reference mismatch: {row["name"]}')
        report['resources'].append(dict(row, reference_match=True))
        decoded_resources[row['name'].casefold()] = bytes(decoded)
    if args.check_audio:
        phase='audio';u.mem_map(0xdff000,4096);u.mem_map(0xbfe000,4096)
        def set_audio_word(address,value):u.mem_write(address,int(value).to_bytes(2,'big'))
        def set_audio_pointer(address,value):u.mem_write(address,int(value).to_bytes(4,'big'))
        def audio_word(address):return int.from_bytes(u.mem_read(address,2),'big')
        results=[]
        for name,song in decoded_resources.items():
            if not name.endswith('.pat'):continue
            folder=output/('audio-'+name)
            process=subprocess.run([str(args.probe.resolve()),'--datapath',str(root),'--output',str(folder),
                '--audio',name,'--audio-ticks','12000'],capture_output=True,text=True,timeout=60)
            if process.returncode:raise ValueError(f'{name}: {process.stderr}')
            frames=json.loads((folder/'audio.json').read_text())
            u.mem_write(0x190000,song);set_audio_pointer(0x80436,0x190000)
            set_audio_pointer(0x80dec,0x180000)
            u.mem_write(0xe43ee,bytes(0x44e6-0x43ee))
            set_audio_word(0xe43f4,0x8000)
            for channel in range(4):set_audio_word(0xe43f8+24*channel+22,1<<channel)
            for instrument in range(16):set_audio_pointer(0x180000+4*instrument,0x200000+instrument*0x8000)
            u.mem_write(0xdff000,bytes(4096));call(0xe3d6c)
            for tick,expected in enumerate(frames):
                call(0xe3e56)
                observed=[be(0xe44ea),be(0xe44ee)//16,audio_word(0xe44f6),audio_word(0xe43f6)]
                for channel in range(4):
                    observed.extend((audio_word(0xdff0a6+16*channel),audio_word(0xdff0a8+16*channel),audio_word(0xe43f8+24*channel+16)))
                if observed!=expected:raise ValueError(f'{name} audio tick {tick}: native {expected}, original {observed}')
            results.append(dict(reference_match=True,**json.loads(process.stdout)['audio']))
        report['audio']=dict(sequencer_only=True,cases=results)
    if args.check_levels or args.check_motion:
        phase = 'level'
        levels = []
        sha = lambda data: hashlib.sha256(data).hexdigest()
        def w(address):
            return int.from_bytes(u.mem_read(address,2),'big')
        def signed(v):
            return v-65536 if v>=32768 else v
        def set_word(address,value):
            u.mem_write(address, (value&65535).to_bytes(2,'big'))
        def set_pointer(address,value):
            u.mem_write(address,value.to_bytes(4,'big'))
        for level in (list(range(1,15))+[16] if args.check_levels else [1]):
            if level==1:
                state=native['level_state']
            else:
                process=subprocess.run([str(args.probe.resolve()),'--datapath',str(root),
                    '--output',str(output/f'level-{level:02}'),'--palette',str(palette_path),
                    '--level',str(level)],capture_output=True,text=True,timeout=60)
                if process.returncode:
                    raise ValueError(process.stderr)
                state=json.loads(process.stdout)['level_state']
            original_map=decoded_resources[f'level{level:02}.tbl']
            height=len(original_map)//256
            u.mem_write(bases[2],bytes(image.segments[2].get_size()))
            u.mem_write(0x180000,bytes(0x100000))
            addresses={0x8058e:0x180000,0x80592:0x180100,0x80596:0x180200,
                0x80584:0x180300,0x805e4:0x181000,0x80600:0x182000,
                0x8058a:0x184000,0x80614:0x184200,0x805f8:0x185000,
                0x805f4:0x185200,0x805c4:0x186000,0x80580:0x190000}
            for address,value in addresses.items():set_pointer(address,value)
            for address,value in {0x804a0:40,0x804a2:50,0x804ae:100,0x804a4:10,
                                  0x804a6:20,0x80578:256,0x8057a:height,0x804b0:30,
                                  0x804bc:101,0x80508:level}.items():set_word(address,value)
            payload=original_map
            call(0xe46e2,(1,))
            if cursor!=len(original_map):raise ValueError('Original map stream not consumed')
            payload=decoded_resources[f'poil{level:02}.bin']
            call(0xe7b38,(1,))
            if cursor!=3060:raise ValueError('POIL reference stream not consumed')
            projections={}
            for key,pointer,count,stride,fields in (
                ('object_projection_sha256',0x181000,40,24,(0,2,4)),
                ('enemy_projection_sha256',0x182000,w(0x804a8),92,(0,2,4,24)),
                ('elevator_projection_sha256',0x185200,w(0x804aa),40,(0,2,4,24))):
                projection=b''.join(bytes(u.mem_read(pointer+i*stride+field,2))
                                    for i in range(count) for field in fields)
                projections[key]=sha(projection)
            original_state=dict(projections,
                normalized_map_sha256=sha(bytes(u.mem_read(0x190000,len(original_map)))),
                horizontal_sha256=sha(bytes(u.mem_read(0x180000,256))),
                floor_sha256=sha(bytes(u.mem_read(0x180100,256))),
                ceiling_sha256=sha(bytes(u.mem_read(0x180200,256))),
                animation_sha256=sha(bytes(u.mem_read(0x180300,768))),
                dirty_rows_sha256=sha(bytes(u.mem_read(0x184000,height))),
                spawn=[signed(w(0x804fa)),signed(w(0x804fc))],
                finish=[signed(w(0x804b4)),signed(w(0x804b6))],
                camera_trigger_y=w(0x80528),camera_lock_y=w(0x8052a),
                gravity_wall_tile=w(0x805d8),
                gate_projection_sha256=sha(bytes(u.mem_read(0x185000,160))))
            queries=bytearray()
            for i in range(132):
                x,y=(i*37%4096,i*53%(height*16)) if i<128 else (4095 if i&1 else 0,height*16-1 if i&2 else 0)
                queries.append(u.mem_read(0x190000+(y//16)*256+x//16,1)[0])
                for function in (0xe6604,0xe65d4,0xe65ec):
                    for reg,value in ((UC_M68K_REG_A0,0x190000),(UC_M68K_REG_A1,0x180000),
                                      (UC_M68K_REG_A2,0x180100),(UC_M68K_REG_A3,0x180200),
                                      (UC_M68K_REG_D0,x),(UC_M68K_REG_D1,y)):
                        u.reg_write(reg,value)
                    queries.append(call(function)&255)
            original_state['collision_queries_sha256']=sha(queries)
            transitions=bytearray()
            for i in range(20):
                entrance=w(0x185000+i*8)
                if entrance==65535 or not 0<entrance>>8<=height:continue
                set_word(0x805e2,0)
                u.reg_write(UC_M68K_REG_D2,(entrance&255)*16+8)
                u.reg_write(UC_M68K_REG_D3,(entrance>>8)*16-1)
                call(0xe6716)
                if w(0x805e2)!=1:raise ValueError('Original gate did not trigger')
                for address in (0x804fe,0x80500,0x80502,0x80504,0x80506):
                    transitions.extend(u.mem_read(address,2))
            original_state['gate_transitions_sha256']=sha(transitions)

            exits=bytearray()
            finish_x,finish_y=original_state['finish']
            for dy in range(-1,2):
                for dx in range(-1,2):
                    for blocked in (0,1):
                        for carried in (0,27):
                            set_word(0x804f6,max(0,min(4095,finish_x+dx*16+7)))
                            set_word(0x804f8,max(0,min(height*16-1,finish_y+dy*16+7)))
                            set_word(0x80494,blocked)
                            set_word(0x187000,carried)
                            set_pointer(0x8060c,0x187000 if carried else 0)
                            set_word(0x805e2,0)
                            call(0xe6fb4)
                            exits.append(int(w(0x805e2)==4))
            original_state['exit_queries_sha256']=sha(exits)
            for key,value in original_state.items():
                if state[key]!=value:
                    raise ValueError(f'Level {level} mismatch {key}: native {state[key]}, original {value}')
            if args.check_motion and level==1:
                phase='motion'
                neutral={}
                scenarios={
                    'idle':dict(steps=[neutral]*360),
                    'walk_brake':dict(steps=[dict(right=True)]*24+[neutral]*20),
                    'reverse':dict(initial=dict(x=200),steps=[dict(right=True)]*20+[dict(left=True)]*24+[neutral]*16),
                    'short_jump':dict(initial=dict(x=100),steps=[dict(up=True)]*2+[neutral]*60),
                    'held_jump':dict(initial=dict(x=100),steps=[dict(up=True)]*8+[neutral]*60),
                    'moving_jump':dict(initial=dict(x=100),steps=[dict(up=True,right=True)]*8+[dict(right=True)]*45+[neutral]*16),
                    'fall':dict(initial=dict(x=200,y=400,state=3),steps=[neutral]*60),
                    'rising_release':dict(initial=dict(x=200,y=500,state=3,vy=-144),steps=[neutral]*60),
                }
                scenarios.update(
                    wall_right=dict(initial=dict(x=100,y=415),fixture=dict(floor_row=26,wall_columns=[8]),steps=[dict(right=True)]*24+[neutral]*10),
                    wall_left=dict(initial=dict(x=170,y=415),fixture=dict(floor_row=26,wall_columns=[8]),steps=[dict(left=True)]*24+[neutral]*10),
                    head_bump=dict(initial=dict(x=200,y=415),fixture=dict(floor_row=26,ceiling_row=23),steps=[dict(up=True)]*8+[neutral]*25),
                    walk_off=dict(initial=dict(x=236,y=415),fixture=dict(floor_row=26,floor_end_column=16),steps=[dict(right=True)]*20+[neutral]*4))
                for shift,value in ((1,0x81),(2,0x83),(3,0x85)):
                    scenarios[f'ice_{shift}']=dict(initial=dict(x=200,ice_shift=shift),floor_override=value,
                        steps=[dict(right=True)]*40+[neutral]*50)
                ladder=dict(column=12,first_row=20,last_row=25)
                scenarios.update(
                    crouch_idle=dict(initial=dict(x=200,y=415),fixture=dict(floor_row=26),steps=[dict(down=True)]*10+[neutral]*10),
                    crouch_walk=dict(initial=dict(x=200,y=415),fixture=dict(floor_row=26),steps=[dict(down=True,right=True)]*24+[dict(left=True)]*16+[neutral]*10),
                    crouch_jump=dict(initial=dict(x=200,y=415,state=5),fixture=dict(floor_row=26),steps=[dict(up=True)]*8+[neutral]*110),
                    crouch_ceiling=dict(initial=dict(x=200,y=415),fixture=dict(floor_row=26,ceiling_row=24),steps=[dict(down=True)]*6+[neutral]*12+[dict(up=True)]*8),
                    ladder_up=dict(initial=dict(x=200,y=399,state=4),fixture=dict(floor_row=26,ladder=ladder),steps=[dict(up=True)]*36+[neutral]*12),
                    ladder_down=dict(initial=dict(x=200,y=335,state=4),fixture=dict(floor_row=26,ladder=ladder),steps=[dict(down=True)]*30+[neutral]*12),
                    ladder_enter=dict(initial=dict(x=202,y=300,state=3),fixture=dict(floor_row=26,ladder=ladder),steps=[neutral]*12+[dict(up=True)]*12),
                    ladder_side=dict(initial=dict(x=200,y=399,state=4),fixture=dict(floor_row=26,ladder=ladder),steps=[dict(right=True)]*16+[neutral]*20),
                    ladder_from_top=dict(initial=dict(x=202,y=303),fixture=dict(floor_row=26,ladder=ladder),steps=[dict(down=True)]*26+[neutral]*10))
                motion_results=[]
                base_floor=bytes(u.mem_read(0x180100,256))
                base_horizontal=bytes(u.mem_read(0x180000,256));base_ceiling=bytes(u.mem_read(0x180200,256))
                base_map=bytes(u.mem_read(0x190000,len(original_map)))
                fields=(0x804f6,0x804f8,0x8051e,0x80520,0x8050e,0x80514,0x80512,0x80518,0x80496,0x80516,0x8050c,0x8050a)
                for name,scenario in scenarios.items():
                    script=output/f'{name}.json';script.write_text(json.dumps(scenario))
                    folder=output/f'motion-{name}'
                    process=subprocess.run([str(args.probe.resolve()),'--datapath',str(root),
                        '--output',str(folder),'--palette',str(palette_path),'--motion',str(script)],
                        capture_output=True,text=True,timeout=60)
                    if process.returncode:raise ValueError(f'{name}: {process.stderr}')
                    trajectory=json.loads((folder/'motion.json').read_text())
                    initial=trajectory['initial']
                    for address,value in zip(fields,initial):set_word(address,value)
                    for address in (0x8051a,0x80510,0x804ec,0x804ed,0x804ee,0x804f0,0x804f2):set_word(address,0)
                    set_pointer(0x8060c,0);set_word(0x80574,4096);set_word(0x80576,height*16)
                    terrain_map,terrain_horizontal,terrain_floor,terrain_ceiling=base_map,base_horizontal,base_floor,base_ceiling
                    if 'fixture' in scenario:
                        fixture=scenario['fixture'];terrain_map=bytearray(len(base_map))
                        terrain_horizontal=bytearray(256);terrain_floor=bytearray(256);terrain_ceiling=bytearray(256)
                        terrain_floor[1]=1;terrain_horizontal[2]=terrain_floor[2]=terrain_ceiling[2]=1
                        for col in range(fixture.get('floor_end_column',256)):terrain_map[fixture['floor_row']*256+col]=1
                        for col in fixture.get('wall_columns',[]):
                            for row in range(height):terrain_map[row*256+col]=2
                        if 'ceiling_row' in fixture:
                            terrain_map[fixture['ceiling_row']*256:(fixture['ceiling_row']+1)*256]=bytes([2])*256
                        if 'ladder' in fixture:
                            ladder=fixture['ladder'];terrain_floor[3]=6;terrain_ceiling[3]=2
                            for row in range(ladder['first_row'],ladder['last_row']+1):terrain_map[row*256+ladder['column']]=3
                    if 'floor_override' in scenario:terrain_floor=bytes([scenario['floor_override']])*256
                    for address,values in ((0x190000,terrain_map),(0x180000,terrain_horizontal),(0x180100,terrain_floor),(0x180200,terrain_ceiling)):
                        u.mem_write(address,bytes(values))
                    for tick,(keys,expected) in enumerate(zip(scenario['steps'],trajectory['frames'])):
                        for address,key in ((0x804ee,'left'),(0x804ef,'right'),(0x804ec,'up'),(0x804ed,'down')):
                            u.mem_write(address,bytes([int(keys.get(key,False))]))
                        call(0xe70b4)
                        observed=[w(address) for address in fields]
                        observed[:4]=[signed(v) for v in observed[:4]]
                        if expected!=observed:
                            diffs=[(column,a,b) for column,a,b in zip(trajectory['columns'],expected,observed) if a!=b]
                            raise ValueError(f'{name} tick {tick}: native/original {diffs}')
                    native_motion=json.loads(process.stdout)['motion']
                    motion_results.append(dict(name=name,ticks=len(scenario['steps']),reference_match=True,
                        sha256=native_motion['sha256'],synthetic_floor=scenario.get('floor_override'),synthetic_geometry='fixture' in scenario))
                report['motion']=dict(terrain_only=True,columns=trajectory['columns'],cases=motion_results)
                phase='level'
            levels.append(dict(level=level,reference_match=True,fields_verified=len(original_state),
                collision_queries=132,exit_queries=36,gate_transitions=state['gate_transitions'],objects=state['objects'],enemies=state['enemies'],
                elevators=state['elevators'],bonuses=state['bonuses'],gates=state['gates']))
        report['levels']=levels
    report.update(resources_verified=len(report['resources']), native_sprites=native['sprites'],
                  originals_unchanged=before == snapshot(root))
    if not report['originals_unchanged']:
        raise ValueError('Originals changed during validation')
    (output/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='resources'}, indent=2))


if __name__ == '__main__':
    main()
