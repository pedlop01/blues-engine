#!/usr/bin/env python3
"""TH-15 read-only resource cross-check against independent Python readers.

Checks bytes/pixels/masks, not original gameplay or final sprite palettes.
"""
import argparse,hashlib,json,struct,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from audit_amiga_data import lzss

def digest(data):return hashlib.sha256(data).hexdigest()
def unpackbits(data,expected):
    out=bytearray();pos=0
    while pos<len(data):
        token=data[pos];pos+=1
        if token==128:continue
        if token<128:
            n=token+1;assert pos+n<=len(data);out+=data[pos:pos+n];pos+=n
        else:
            assert pos<len(data);out+=bytes([data[pos]])*(257-token);pos+=1
        assert len(out)<=expected
    assert len(out)==expected
    return out

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['probe','datapath','output']:parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--windows',action='store_true');args=parser.parse_args();source=args.datapath.resolve();output=args.output.resolve()
    assert not output.exists() and output.is_relative_to(ROOT/'build')
    def snapshot():return {str(p.relative_to(source)):(p.stat().st_size,p.stat().st_mtime_ns,digest(p.read_bytes())) for p in source.rglob('*') if p.is_file()}
    before=snapshot();options=['--datapath',str(source),'--output',str(output)]
    if args.windows:
        result=subprocess.run([sys.executable,str(ROOT/'tools/run_windows_probe.py'),'--program',str(args.probe.resolve()),'--output-dir',str(output.parent/(output.name+'-runner')),'--timeout','60','--',*options],capture_output=True,text=True,timeout=70)
        assert result.returncode==0,result.stderr;wrapped=json.loads(result.stdout);assert wrapped['exit']==0,wrapped
    else:
        result=subprocess.run([str(args.probe.resolve()),*options],capture_output=True,text=True,timeout=60);assert result.returncode==0,result.stderr
    report=json.loads((output/'report.json').read_text());counts=dict(mat=0,sprites=0,pc1=0,samples=0)
    for row in report['resources']:
        data=(source/row['name']).read_bytes();assert digest(data)==row['sha256']
        if row['format']=='mat':
            mono=row['name'].lower()=='char_set1.mat';raw=data if mono else lzss(data)[0];assert digest(raw)==row['decoded_sha256'];count=struct.unpack_from('>H',raw)[0];assert count==len(row['sprites']);pos=2
            for sprite in row['sprites']:
                w,h,size=struct.unpack_from('>HHH',raw,pos);pos+=6;plane=((w+7)//8)*h;length=size or plane*(1 if mono else 5);payload=raw[pos:pos+length];pos+=length
                if size:payload=unpackbits(payload,plane*5)
                colors=bytearray();mask=bytearray()
                for y in range(h):
                    for x in range(w):
                        offset=y*((w+7)//8)+x//8;bit=7-x%8
                        colors.append(15 if mono else sum(((payload[p*plane+offset]>>bit)&1)<<p for p in range(4)))
                        mask.append((payload[(0 if mono else 4*plane)+offset]>>bit)&1)
                assert (w,h)==(sprite['width'],sprite['height']) and digest(colors)==sprite['colors_sha256'] and digest(mask)==sprite['mask_sha256']
            assert pos==len(raw);counts['mat']+=1;counts['sprites']+=count
        elif row['format']=='pc1':
            palette=[((data[i]>>4)*17<<16)|((data[16+i]>>4)*17<<8)|((data[32+i]>>4)*17) for i in range(16)];assert palette==row['palette'][:16]
            size=struct.unpack_from('>I',data,48)[0];flags=unpackbits(data[52:52+size],4000);pos=56+size;assert struct.unpack_from('>I',data,52+size)[0]==len(data)-pos
            raw=bytearray(32000);index=0;previous=0
            for plane in range(4):
                for x in range(40):
                    for group in range(25):
                        flag=flags[index];index+=1
                        for bit in range(8):
                            if not (flag>>bit)&1:previous=data[pos];pos+=1
                            raw[plane*8000+(group*8+bit)*40+x]=previous
            assert pos==len(data);pixels=bytearray(sum(((raw[p*8000+y*40+x//8]>>(7-x%8))&1)<<p for p in range(4)) for y in range(200) for x in range(320));assert digest(pixels)==row['pixels_sha256'];counts['pc1']+=1
        else:
            pos=12;chunks={};assert data[:4]==b'FORM' and data[8:12]==b'8SVX' and int.from_bytes(data[4:8],'big')==len(data)-8
            while pos<len(data):
                tag=data[pos:pos+4];size=int.from_bytes(data[pos+4:pos+8],'big');chunks[tag]=data[pos+8:pos+8+size];pos+=8+size+(size&1)
            one,repeat,cycle,rate,octaves,compression,volume=struct.unpack('>IIIHBBI',chunks[b'VHDR']);assert (one,repeat,cycle,rate,volume)==tuple(row[k] for k in ['one_shot','repeat','cycle','rate','volume']);assert octaves==1 and compression==0 and digest(chunks[b'BODY'])==row['pcm_sha256'];counts['samples']+=1
    assert before==snapshot();summary=dict(schema_version=1,playable=False,counts=counts,reference='independent bounded Python readers; no original simulation',original_files=len(before),originals_unchanged=True)
    (output/'validation.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary))
if __name__=='__main__':main()
