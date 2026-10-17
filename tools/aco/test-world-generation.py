"""Exercise coastal world stitching against many real native generator seeds."""
import argparse,json,runpy,struct,subprocess
from pathlib import Path
here=Path(__file__).resolve().parent
connect=runpy.run_path(str(here/'world-generation.py'))['coastal_connections']
Disc=runpy.run_path(str(here/'disc-image.py'))['Disc']

if __name__=='__main__':
    p=argparse.ArgumentParser()
    p.add_argument('generator');p.add_argument('disc');p.add_argument('layout')
    p.add_argument('--seeds',type=int,default=60)
    args=p.parse_args()
    layout=json.loads(Path(args.layout).read_text(encoding='utf-8-sig'))
    deep=layout.get('height',10)>=18
    width=layout['width'];fw=layout['fg_width'];pad=layout.get('town_start',0)
    fg=Disc(args.disc).archive_file('forest_1st.arc','fgdata.bin')
    templates={struct.unpack_from('>H',fg,p)[0]:fg[p+2:p+514] for p in range(0,len(fg),518)}
    ocean_seeds=set()
    for seed in range(args.seeds):
        town=json.loads(subprocess.check_output([args.generator,str(seed+1234)]))
        body=bytearray(layout['sector_size'])
        for offset in (pad,pad+7,pad+14):
            for i,(combo,height,fg_id,kind) in enumerate(town['acres']):
                z,x=divmod(i,7)
                struct.pack_into('>H',body,layout['combi_table']+(z*width+x+offset)*2,(combo<<2)|height)
                if 1<=z<=6 and 1<=x<=5:
                    p=layout['fg']+((z-1)*fw+x+offset-1)*512
                    body[p:p+512]=templates[fg_id]
        before=bytes(body)
        routes=connect(body,layout,templates,here.parents[1])
        after=bytes(body)
        assert len(routes)==2
        assert connect(body,layout,templates,here.parents[1])==routes and bytes(body)==after
        for x,combination in ((0,203),(width-1,206)):
            for z in range(7,16) if deep and pad else range(6,16) if deep else (6,7):
                pos=layout['combi_table']+(z*width+x)*2
                assert struct.unpack_from('>H',body,pos)[0]==combination<<2
        for z in range(layout.get('height',10)):
            for x in range(width):
                pos=layout['combi_table']+(z*width+x)*2
                outer_extension=x in (0,width-1) and z in (6,7)
                ocean_edit=deep and (z>=7 or (z==6 and (x<pad+14 or x==pad+20)) or (pad and (x<pad or x>pad+20)))
                if not ocean_edit and not outer_extension and (x not in (pad+6,pad+7,pad+13,pad+14) or z not in (1,2,3,4,5,6,7)):
                    assert body[pos:pos+2]==before[pos:pos+2]
        for route in routes:
            for x in route['border_columns']:
                pos=layout['fg']+(5*fw+x-1)*512
                assert body[pos+32:pos+256]==bytes(224)
        if deep:
            assert body[0x1c:0x20]==(b'OCE3' if pad else b'OCE1')
            # Inspect actual native combination types, not fixed combo numbers.
            import re
            entries=re.findall(r'\{\s*(BG_TYPE_\w+)\s*,\s*(FG_TYPE_\w+)\s*,\s*(mFM_BLOCK_TYPE_\w+)\s*\}',
                (here.parents[1]/'src/data/combi/data_combi.c').read_text())
            islands=[];ports=[];boats=[];signs=[]
            for z in range(layout['height']):
                for x in range(width):
                    c=struct.unpack_from('>H',body,layout['combi_table']+(z*width+x)*2)[0]>>2
                    kind=entries[c][2]
                    if kind.startswith('mFM_BLOCK_TYPE_ISLAND_'):islands.append((x,z))
                    if kind=='mFM_BLOCK_TYPE_PORT':ports.append((x,z))
                    if 7<=z<15 and 0<x<width-1 and (x,z) not in ((pad+18,12),(pad+19,12)):
                        assert kind=='mFM_BLOCK_TYPE_OCEAN_8'
            for z in range(6):
                for x in range(1,width-1):
                    items=struct.unpack_from('>256H',body,layout['fg']+(z*fw+x-1)*512)
                    for unit,item in enumerate(items):
                        if item==0x584f:boats.append((x,z+1,unit))
                        if item==0x5852:signs.append((x,z+1))
            if pad:
                generator=runpy.run_path(str(here/'world-generation.py'))['ocean_destinations']
                report=generator(body,layout,entries);ocean_seeds.add(report['seed'])
                assert bytes(body)==after and generator(body,layout,entries)==report
                assert len(report['islands'])==4 and len(report['wall_landings'])==2
                assert len({(i['left_variant'],i['right_variant']) for i in report['islands']})==4
                land=set()
                for island in report['islands']:
                    cells={(island['x'],island['z']),(island['x']+1,island['z'])}
                    assert not any((x+dx,z+dz) in land for x,z in cells for dx in (-1,0,1) for dz in (-1,0,1))
                    land|=cells
                for shelf in report['wall_landings']:
                    assert shelf['x'] in (1,width-2)
                    land.add((shelf['x'],shelf['z']))
                canonical={(pad+18,12),(pad+19,12)}
                route={(x,z) for x in range(pad+17,pad+22) for z in range(7,15)}
                assert not land & route
                # Every generated destination uses actual native island geometry
                # and collision without acquiring the unique saved-island role.
                for x,z in land:
                    c=struct.unpack_from('>H',body,layout['combi_table']+(z*width+x)*2)[0]>>2
                    assert entries[c][0].startswith(('BG_TYPE_GRD_S_IL_','BG_TYPE_GRD_S_IR_'))
                    assert entries[c][1:] == ('FG_TYPE_EMPTY','mFM_BLOCK_TYPE_OCEAN_8')
                water={(x,z) for x in range(1,width-1) for z in range(7,15)}-land-canonical
                reached={(pad+19,7)};front=list(reached)
                while front:
                    x,z=front.pop()
                    for q in ((x+1,z),(x-1,z),(x,z+1),(x,z-1)):
                        if q in water and q not in reached:reached.add(q);front.append(q)
                assert reached==water,'Disconnected sailing channel'
            assert islands==[(pad+18,12),(pad+19,12)] and ports==[(pad+19,6)]
            assert boats==[(pad+19,6,165)] and signs==[(pad+19,6)]
            for row in range(2):
                for x in range(width):
                    p=layout['combi_table']+((8+row)*width+x)*2
                    word=struct.unpack_from('>H',before,p)[0]
                    q=layout['combi_table']+((16+row)*width+x)*2
                    if entries[word>>2][0].startswith('BG_TYPE_ROM_'):assert body[q:q+2]==before[p:p+2]
                    else:assert entries[struct.unpack_from('>H',body,q)[0]>>2][2]=='mFM_BLOCK_TYPE_OCEAN_8'
    if pad and args.seeds>=10:assert len(ocean_seeds)>=10,'World seeds must vary island placement'
    print(f'PASS: {args.seeds} native generator seeds; coast joins; preserved towns; clear shoulders; idempotent rules'+
          ('; enclosed sea, unique ferry island, spaced exploration islands/wall landings and connected sailing channels; preserved indoor templates' if deep else ''))
