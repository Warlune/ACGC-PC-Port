"""Local, reproducible 4x texture pack pipeline for the supplied GAFE01 disc.

Extracts only source-described Dolphin textures with known format/size/palette;
unresolved dynamic textures are reported, never guessed. The original game's
decoder is used through a bounded binary worker. No game data is uploaded.
"""
from __future__ import annotations
import argparse, collections, hashlib, json, os, re, shutil, struct, subprocess, sys
from pathlib import Path
SOURCE_CHECKOUT = Path(__file__).resolve().parents[2]
ROOT = Path(os.environ.get('ACO_WORKSPACE', str(SOURCE_CHECKOUT.parent)))
sys.path.insert(0, str(ROOT / 'toolchain/python-packages'))
from PIL import Image, ImageDraw
import numpy as np
import xxhash
SRC = SOURCE_CHECKOUT
WORK = ROOT / 'texture-work/aco-original-4x-v1'
PACK = ROOT / 'texture-packs/ACO-Original-4x-v1'
MODEL = ROOT / 'toolchain/realesrgan-full'
MODEL_NAME = 'realesrgan-x4plus-anime'

class Disc:
    def __init__(self, path):
        self.file = open(path, 'rb')
        h = self.file.read(0x8000)
        self.block_size = struct.unpack_from('<I', h, 4)[0] if h[:4] == b'CISO' else 0
        self.blocks = []
        count = 0
        if self.block_size:
            for flag in h[8:]:
                self.blocks.append(count if flag else -1)
                if flag: count += 1
        boot = self.read(0, 0x440)
        assert boot[:6] == b'GAFE01' and boot[7] == 0, 'Unsupported disc revision'
        self.dol_offset = struct.unpack_from('>I', boot, 0x420)[0]
        fst_offset, fst_size = struct.unpack_from('>II', boot, 0x424)
        fst = self.read(fst_offset, fst_size)
        entries = struct.unpack_from('>I', fst, 8)[0]
        self.files = {}
        for i in range(1, entries):
            name_word, offset, size = struct.unpack_from('>III', fst, i * 12)
            if name_word >> 24: continue
            start = entries * 12 + (name_word & 0xffffff)
            name = fst[start:fst.index(b'\0', start)].decode('ascii')
            self.files[name] = (offset, size)

    def read(self, offset, size):
        if not self.block_size:
            self.file.seek(offset); data = self.file.read(size)
            assert len(data) == size; return data
        out = bytearray()
        while size:
            block, inner = divmod(offset, self.block_size)
            n = min(size, self.block_size - inner)
            physical = self.blocks[block]
            if physical < 0: out.extend(bytes(n))
            else:
                self.file.seek(0x8000 + physical * self.block_size + inner)
                data = self.file.read(n); assert len(data) == n; out.extend(data)
            offset += n; size -= n
        return bytes(out)

def yaz0(data):
    if data[:4] != b'Yaz0': return data
    size = struct.unpack_from('>I', data, 4)[0]
    out = bytearray(); pos = 16
    while len(out) < size:
        bits = data[pos]; pos += 1
        for bit in range(7, -1, -1):
            if len(out) >= size: break
            if bits & (1 << bit): out.append(data[pos]); pos += 1
            else:
                a, b = data[pos:pos+2]; pos += 2
                back = ((a & 15) << 8) + b + 1
                length = a >> 4
                if not length: length = data[pos] + 0x12; pos += 1
                else: length += 2
                if back > len(out): raise ValueError('Invalid Yaz0 distance')
                for _ in range(length): out.append(out[-back])
    assert len(out) == size
    return bytes(out)

def strip_comments(text):
    return re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S)

def calls(text):
    for m in re.finditer(r'\b(gsDPLoadTLUT_Dolphin|gsDPLoadTextureBlock_4b_Dolphin|gsDPSetTextureImage_Dolphin|gsDPSetTile_Dolphin)\s*\(', text):
        start=m.end(); depth=1; i=start
        while depth and i<len(text):
            if text[i]=='(': depth+=1
            if text[i]==')': depth-=1
            i+=1
        body=text[start:i-1]
        if '(' not in body: yield m.group(1), [v.strip() for v in body.split(',')]

def data_size(w,h,fmt):
    bw,bh,bs = (8,8,32) if fmt in (0,8,14) else (8,4,32) if fmt in (1,2,9) else (4,4,64) if fmt==6 else (4,4,32)
    return ((w+bw-1)//bw)*((h+bh-1)//bh)*bs

def texture_name(data,w,h,fmt,pal):
    # Match the native pack lookup's byte length, including its current clamp.
    bpp = {0:4,1:8,2:8,3:16,4:16,5:16,6:32,8:4,9:8,14:4}[fmt]
    size = min(data_size(w,h,fmt), w*h*bpp//8)
    texture_hash=xxhash.xxh64(data[:size]).hexdigest()
    if fmt in (8,9):
        indices = [v for byte in data[:size] for v in (byte>>4,byte&15)] if fmt==8 else list(data[:size])
        low,high=min(indices),max(indices)
        palette_hash=xxhash.xxh64(pal[low*2:(high+1)*2]).hexdigest()
        return f'tex1_{w}x{h}_{texture_hash}_{palette_hash}_{fmt}'
    return f'tex1_{w}x{h}_{texture_hash}_{fmt}'

def build_codec():
    source=(SRC/'pc/src/pc_gx_texture.c').read_text()
    code=source[source.index('/* --- GC texture format decoders'):source.index('static void pc_gx_load_tex_obj_impl')]
    (SRC/'pc/build32/texture_decode.inc').write_text('static void decode_rgb5a3_entry(u16,u8*,u8*,u8*,u8*);\n'+code)
    mingw=ROOT/'toolchain/msys64/mingw32'
    includes=[SRC/'pc/include',SRC/'pc/lib/glad/include',mingw/'include/SDL2',SRC/'include']
    env=os.environ.copy(); env['PATH']=str(mingw/'bin')+os.pathsep+env['PATH']
    subprocess.run([str(mingw/'bin/gcc.exe'),'-O2','-std=gnu11','-DTARGET_PC',
        *['-I'+str(p) for p in includes],str(SRC/'pc/tools/texture_decode.c'),'-static-libgcc',
        '-o',str(ROOT/'tools/texture-decode.exe')],check=True,env=env)

def extract():
    for folder in ('originals','ai-input','ai-output','review'):
        (WORK/folder).mkdir(parents=True,exist_ok=True)
    PACK.mkdir(parents=True,exist_ok=True)
    disc=Disc(next((ROOT/'native-runtime/rom').glob('*.ciso')))
    rel=yaz0(disc.read(*disc.files['foresta.rel.szs']))
    dol_header=disc.read(disc.dol_offset,0x100)
    dol_size=max(a+b for a,b in zip(struct.unpack_from('>18I',dol_header),struct.unpack_from('>18I',dol_header,0x90)))
    dol=disc.read(disc.dol_offset,dol_size)
    roms=(rel,dol)
    assets={}; bypath={}
    central=(SRC/'pc/src/pc_assets.c').read_text()
    for p,var,size,off,src,swap in re.findall(r'\{"([^"]+)",\s*(\w+),\s*(0x[\da-fA-F]+),\s*(0x[\da-fA-F]+),\s*(\d+),\s*(\d+)\}',central):
        record=(int(src),int(off,16),int(size,16))
        assets[var]=record; bypath[p]=record
    source_files=sorted((SRC/'src').rglob('*.c'))
    texts={}
    for path in source_files:
        t=path.read_text(errors='replace'); texts[path]=t
        for p,var,size,off,src in re.findall(r'pc_load_asset\("([^"]+)",\s*(\w+),\s*(0x[\da-fA-F]+),\s*(0x[\da-fA-F]+),\s*(\d+),',t):
            record=(int(src),int(off,16),int(size,16))
            bypath[p]=record
            # Function-local names can collide; only global/dummy variants are
            # used for cross-file resolution below. Local metadata overrides.
            if var not in assets: assets[var]=record

    def raw(var, local):
        m=re.fullmatch(r'&?(\w+)(?:\[(\d+)\])?',var.strip())
        if not m: raise KeyError(var)
        sym,index=m.group(1),int(m.group(2) or 0)
        src,off,size=local.get(sym,assets.get(sym,(2,0,0)))
        if src>1 or off+size>len(roms[src]): raise KeyError(var)
        # Indexed byte arrays only; palette array slicing is explicit elsewhere.
        return roms[src][off+index:off+size]

    field=texts[SRC/'src/game/m_field_make.c']
    dummy_block=re.search(r'l_bg_tex_common_dummy\[27\]\s*=\s*\{(.*?)\n\};',field,re.S).group(1)
    dummy_names=re.findall(r'\{\s*(\w+),',dummy_block)
    variants=[]
    for block in re.findall(r'l_bg_tex_segment_rom_start_[sw]_\d\[[^]]+\]\s*=\s*\{(.*?)\};',field,re.S):
        names=re.findall(r'\b(?:mFM_\w+)\b',block)
        if len(names)==len(dummy_names): variants.append(dict(zip(dummy_names,names)))
    monthly={'earth_pal_dummy':'mFM_earth_pal','cliff_pal_dummy':'mFM_cliff_pal',
             'bush_pal_dummy':'mFM_bush_pal','rail_pal_dummy':'mFM_rail_pal','beach_pal_dummy2':'mFM_beach_pal'}
    fmtmap={('G_IM_FMT_CI','4b'):8,('G_IM_FMT_CI','8b'):9,('G_IM_FMT_I','4b'):0,('G_IM_FMT_I','8b'):1,
            ('G_IM_FMT_IA','8b'):2,('G_IM_FMT_IA','16b'):3,('G_IM_FMT_RGBA','4b'):14,
            ('G_IM_FMT_RGBA','16b'):5,('G_IM_FMT_RGBA','32b'):6,('G_IM_FMT_I','16b'):4}
    worker=subprocess.Popen([str(ROOT/'tools/texture-decode.exe')],stdin=subprocess.PIPE,stdout=subprocess.PIPE)
    manifest={}; skipped=collections.Counter(); eligible_files=0
    def emit(tex,fmt,w,h,palname,entries,wrap_s,wrap_t,path,local,variant):
        try:
            data=raw(variant.get(tex,tex),local)
            size=data_size(w,h,fmt)
            if len(data)<size: skipped['asset_shorter_than_texture']+=1; return
            data=data[:size]
            pal_variants=[b'']
            if fmt in (8,9):
                if palname in monthly:
                    allpal=raw(monthly[palname],local)
                    pal_variants=[allpal[i:i+32] for i in range(0,len(allpal),32)]
                else: pal_variants=[raw(variant.get(palname,palname),local)[:entries*2]]
            for pal in pal_variants:
                n=len(pal)//2
                if fmt in (8,9) and (n<entries or n>256): skipped['palette_size']+=1; continue
                name=texture_name(data,w,h,fmt,pal)
                if name in manifest: continue
                worker.stdin.write(struct.pack('<6I',w,h,fmt,2,n,len(data))+data+pal); worker.stdin.flush()
                rgba=worker.stdout.read(w*h*4)
                if len(rgba)!=w*h*4: raise RuntimeError('Texture worker failed')
                image=Image.frombytes('RGBA',(w,h),rgba)
                alpha=np.asarray(image.getchannel('A'))
                if not alpha.any(): skipped['fully_transparent']+=1; continue
                if len(image.getcolors(2) or [])==1: skipped['solid_color']+=1; continue
                stem=tex.lower()
                text_sensitive=any(s in stem for s in ('font','moji','name_tex','_kanban','_number','_logo'))
                alpha_sensitive=bool((alpha<255).any())
                mode='filtered-alpha' if alpha_sensitive else 'filtered-text' if text_sensitive else 'ai-4x'
                image.save(WORK/'originals'/f'{name}.png')
                manifest[name]={'name':name,'width':w,'height':h,'format':fmt,'symbol':variant.get(tex,tex),
                    'palette':variant.get(palname,palname),'source':str(path.relative_to(SRC)).replace('\\','/'),
                    'wrap_s':wrap_s!='GX_CLAMP','wrap_t':wrap_t!='GX_CLAMP','method':mode,
                    'address_s':wrap_s,'address_t':wrap_t,
                    'original_sha256':hashlib.sha256(rgba).hexdigest()}
        except KeyError: skipped['unresolved_symbol_or_palette']+=1

    for path,text in texts.items():
        relpath=str(path.relative_to(SRC)).replace('\\','/')
        if not (relpath.startswith('src/data/field/') or (relpath.startswith('src/data/model/') and
                (path.name.startswith('obj_') or path.name.startswith('int_')))):
            continue
        eligible_files+=1; local={}
        for _,var,size,off,src in re.findall(r'pc_load_asset\("([^"]+)",\s*(\w+),\s*(0x[\da-fA-F]+),\s*(0x[\da-fA-F]+),\s*(\d+),',text):
            local[var]=(int(src),int(off,16),int(size,16))
        palettes={}; pending=None
        for command,a in calls(strip_comments(text)):
            try:
                if command=='gsDPLoadTLUT_Dolphin': palettes[int(a[0],0)]=(a[3],int(a[1],0))
                elif command=='gsDPLoadTextureBlock_4b_Dolphin':
                    tex,imfmt,w,h,slot,ws,wt=a[:7]
                    pal,n=palettes.get(int(slot,0),('',0))
                    pending=(tex,imfmt,'4b',int(w,0),int(h,0),pal,n,ws,wt)
                elif command=='gsDPSetTextureImage_Dolphin':
                    imfmt,siz,w,h,tex=a
                    pending=(tex,imfmt,siz.replace('G_IM_SIZ_',''),int(w,0),int(h,0),'',0,'GX_REPEAT','GX_REPEAT')
                    continue
                elif command=='gsDPSetTile_Dolphin' and pending:
                    tex,imfmt,siz,w,h,_,_,_,_=pending
                    # A non-default direct GX format override needs explicit handling.
                    if a[0] not in ('15','G_DOLPHIN_TLUT_DEFAULT_MODE'): pending=None; skipped['gx_format_override']+=1; continue
                    pal,n=palettes.get(int(a[2],0),('',0))
                    pending=(tex,imfmt,siz,w,h,pal,n,a[3],a[4])
                else: continue
                if pending:
                    tex,imfmt,siz,w,h,pal,n,ws,wt=pending; pending=None
                    fmt=fmtmap.get((imfmt,siz))
                    if fmt is None or not (4<=w<=512 and 4<=h<=512): skipped['unsupported_format_or_dimensions']+=1; continue
                    if fmt in (8,9) and not pal: skipped['missing_palette']+=1; continue
                    choices=variants if tex.endswith('_dummy') else [{}]
                    for variant in choices: emit(tex,fmt,w,h,pal,n,ws,wt,path,local,variant)
            except (ValueError,IndexError): skipped['dynamic_macro_arguments']+=1
    worker.stdin.close(); worker.wait()
    if worker.returncode: raise RuntimeError('Decoder worker exited with error')
    records=list(manifest.values())
    (WORK/'manifest.json').write_text(json.dumps(records,indent=2))
    report={'source_assets':len(assets),'source_files_considered':eligible_files,'exported_unique':len(records),
        'methods':dict(collections.Counter(r['method'] for r in records)),'skipped_reasons':dict(skipped),
        'coverage':'Source-described field, exterior object and furniture textures; not a complete game texture inventory.',
        'codec_source_sha256':hashlib.sha256((SRC/'pc/src/pc_gx_texture.c').read_bytes()).hexdigest()}
    (WORK/'extraction-report.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report,indent=2),flush=True)

def write_dds(path,image):
    image=image.convert('RGBA'); w,h=image.size
    # Explicit RGBA masks, tightly packed rows; matches the native loader.
    header=struct.pack('<31I',124,0x100f,h,w,w*4,0,0,*([0]*11),
        32,0x41,0,32,0xff,0xff00,0xff0000,0xff000000,0x1000,0,0,0,0)
    path.write_bytes(b'DDS '+header+image.tobytes())

def records(): return json.loads((WORK/'manifest.json').read_text())

def upscale():
    entries=records(); ai=[r for r in entries if r['method']=='ai-4x']
    changed=[]
    for r in ai:
        dest=WORK/'ai-input'/(r['name']+'.png')
        image=Image.open(WORK/'originals'/(r['name']+'.png')).convert('RGB')
        arr=np.asarray(image)
        def address(axis):
            value=r.get('address_'+axis)
            if value in ('GX_MIRROR','2'): return 'symmetric'
            if value in ('GX_REPEAT','1'): return 'wrap'
            if value is not None: return 'edge'
            return 'wrap' if r['wrap_'+axis] else 'edge'
        arr=np.pad(arr,((0,0),(8,8),(0,0)),mode=address('s'))
        arr=np.pad(arr,((8,8),(0,0),(0,0)),mode=address('t'))
        padded=Image.fromarray(arr)
        if dest.exists():
            old=Image.open(dest).convert('RGB')
            if old.size==padded.size and old.tobytes()==padded.tobytes(): continue
            changed.append(r)
        padded.save(dest)
    missing=[r for r in ai if not (WORK/'ai-output'/(r['name']+'.png')).exists()]
    if missing:
        print(f'Running local {MODEL_NAME} on {len(ai)} padded textures...',flush=True)
        with open(WORK/'inference.log','w') as log:
            subprocess.run([str(MODEL/'realesrgan-ncnn-vulkan.exe'),'-i',str(WORK/'ai-input'),
                '-o',str(WORK/'ai-output'),'-m',str(MODEL/'models'),'-n',MODEL_NAME,
                '-s','4','-t','128','-j','2:2:2','-f','png'],cwd=MODEL,stdout=log,stderr=log,check=True)
    elif changed:
        # Only rerun contexts that changed; preserve the completed other images.
        retry_in=WORK/'corrected-input';retry_out=WORK/'corrected-output'
        retry_in.mkdir(exist_ok=True);retry_out.mkdir(exist_ok=True)
        for r in changed: shutil.copy2(WORK/'ai-input'/(r['name']+'.png'),retry_in)
        with open(WORK/'corrected-inference.log','w') as log:
            subprocess.run([str(MODEL/'realesrgan-ncnn-vulkan.exe'),'-i',str(retry_in),'-o',str(retry_out),
                '-m',str(MODEL/'models'),'-n',MODEL_NAME,'-s','4','-t','128','-j','2:2:2','-f','png'],
                cwd=MODEL,stdout=log,stderr=log,check=True)
        for r in changed: shutil.copy2(retry_out/(r['name']+'.png'),WORK/'ai-output')
    (WORK/'upscaled').mkdir(exist_ok=True)
    for r in entries:
        size=(r['width']*4,r['height']*4)
        if r['method']=='ai-4x':
            image=Image.open(WORK/'ai-output'/(r['name']+'.png')).convert('RGBA')
            expected=((r['width']+16)*4,(r['height']+16)*4)
            if image.size!=expected: raise ValueError(f'AI output dimensions: {r["name"]}')
            image=image.crop((32,32,32+size[0],32+size[1]))
        else:
            original=Image.open(WORK/'originals'/(r['name']+'.png')).convert('RGBA')
            # Premultiplied-alpha interpolation prevents dark fringe colors.
            image=original.convert('RGBa').resize(size,Image.Resampling.LANCZOS).convert('RGBA')
        image.save(WORK/'upscaled'/(r['name']+'.png'))
        write_dds(PACK/(r['name']+'.dds'),image)
    provenance={'scale':4,'ai_model':MODEL_NAME,'ai_source':'https://github.com/xinntao/Real-ESRGAN',
        'model_sha256':hashlib.sha256((MODEL/'models'/(MODEL_NAME+'.bin')).read_bytes()).hexdigest(),
        'count':len(entries),'methods':dict(collections.Counter(r['method'] for r in entries)),
        'wrap_context_padding_source_pixels':8,'alpha_handling':'Premultiplied Lanczos 4x; no AI redraw',
        'scope':'Field, exterior object and furniture textures with resolved source metadata. Fonts/menus/characters are not replaced.'}
    (PACK/'pack.json').write_text(json.dumps(provenance,indent=2))
    shutil.copy2(WORK/'manifest.json',PACK/'manifest.json')
    print(json.dumps(provenance,indent=2),flush=True)
    contact_sheet()

def contact_sheet():
    entries=records(); chosen=[]; seen=set()
    # Favor opaque terrain, then exterior materials, then furniture.
    pool=sorted(entries,key=lambda r:(not '/field/' in r['source'],not r['symbol'].startswith('obj_')))
    for r in pool:
        if r['method']!='ai-4x' or r['symbol'] in seen or min(r['width'],r['height'])<16: continue
        chosen.append(r); seen.add(r['symbol'])
        if len(chosen)==12: break
    canvas=Image.new('RGB',(960,900),'#272b28'); draw=ImageDraw.Draw(canvas)
    for i,r in enumerate(chosen):
        x=(i%4)*240;y=(i//4)*300
        before=Image.open(WORK/'originals'/(r['name']+'.png')).convert('RGB')
        after=Image.open(WORK/'upscaled'/(r['name']+'.png')).convert('RGB')
        factor=min(112/before.width,224/before.height)
        dims=(round(before.width*factor),round(before.height*factor))
        before=before.resize(dims,Image.Resampling.NEAREST)
        after=after.resize(dims,Image.Resampling.LANCZOS)
        canvas.paste(before,(x,y+35));canvas.paste(after,(x+120,y+35))
        draw.text((x,y+5),r['symbol'][:32],fill='white')
        draw.text((x,y+264),'Original / nearest',fill='#bac5ba')
        draw.text((x+120,y+264),'Local AI 4x',fill='#bac5ba')
    canvas.save(WORK/'review/before-after.png')

def validate():
    total=0; total_bytes=0; external_roundtrips=0
    for r in records():
        p=PACK/(r['name']+'.dds'); data=p.read_bytes()
        assert data[:4]==b'DDS '
        h,w=struct.unpack_from('<II',data,12)
        assert (w,h)==(r['width']*4,r['height']*4), p.name
        assert len(data)==128+w*h*4, p.name
        expected=Image.open(WORK/'upscaled'/(r['name']+'.png')).convert('RGBA')
        assert struct.unpack_from('<8I',data,76)==(32,0x41,0,32,0xff,0xff00,0xff0000,0xff000000)
        assert data[128:]==expected.tobytes(),f'DDS channel mismatch: {p.name}'
        # The game reads raw RGBA, not Pillow's generic bitmask decoder.
        # Check all raw payloads and independently decode representative DDSs.
        if total<8 or total%256==0:
            assert Image.open(p).convert('RGBA').tobytes()==expected.tobytes()
            external_roundtrips+=1
        if r['method']=='ai-4x': assert expected.getchannel('A').getextrema()==(255,255)
        total+=1; total_bytes+=len(data)
    report={'validated':total,'dds_bytes':total_bytes,'exact_scale':4,'all_rgba_payloads':'passed',
        'independent_pillow_dds_roundtrips':external_roundtrips,
        'in_game_loading':'not yet checked','coverage':'Partial game inventory; see extraction report.'}
    (PACK/'validation.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report,indent=2),flush=True)

def install():
    validate()
    shutil.copy2(WORK/'extraction-report.json',PACK/'extraction-report.json')
    destination=ROOT/'native-runtime/texture_pack/ACO-Original-4x-v1'
    if destination.exists(): raise RuntimeError('Pack already installed; avoid overwriting manually edited files')
    shutil.copytree(PACK,destination)
    print(f'Installed {destination}',flush=True)

def main():
    parser=argparse.ArgumentParser(); parser.add_argument('phase',choices=['extract','upscale','install','validate']); args=parser.parse_args()
    if args.phase=='extract': build_codec(); extract()
    elif args.phase=='upscale': upscale()
    elif args.phase=='install': install()
    elif args.phase=='validate': validate()

if __name__=='__main__': main()
