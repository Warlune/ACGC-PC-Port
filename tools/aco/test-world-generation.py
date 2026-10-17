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
    import re
    entries=re.findall(r'\{\s*(BG_TYPE_\w+)\s*,\s*(FG_TYPE_\w+)\s*,\s*(mFM_BLOCK_TYPE_\w+)\s*\}',
        (here.parents[1]/'src/data/combi/data_combi.c').read_text())
    ocean_layouts=set()
    landmark_regions={name:set() for name in ('museum','police','shrine')}
    for seed in range(args.seeds):
        town=json.loads(subprocess.check_output([args.generator,str(seed+1234)]))
        center=json.loads(subprocess.check_output([args.generator,str(seed+7234)]))
        body=bytearray(layout['sector_size'])
        for offset,region_town in ((pad,town),(pad+7,center),(pad+14,town)):
            for i,(combo,height,fg_id,kind) in enumerate(region_town['acres']):
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
        if deep and pad:
            service_map=runpy.run_path(str(here/'world-generation.py'))['world_service_map']
            services=service_map(body,layout,entries,templates,here.parents[1],False)['services']
            assert {s['service'] for s in services if s['service'] in ('night_market','maritime_outfitter','customization')} == \
                {'night_market','maritime_outfitter','customization'}
            plots=[s for s in services if s['service']=='player_house_plot']
            assert len(plots)==12
            for region in range(3):
                regional=[p for p in plots if p['region']==region]
                assert {p['plot_index'] for p in regional}=={0,1,2,3}
            for name in ('night_market','maritime_outfitter','customization'):
                anchor=next(s for s in services if s['service']==name)
                assert anchor['actor_item'] in (range(0x5804,0x5808) if name!='customization' else (0x584d,))
                assert 'unit_x' in anchor and 'unit_z' in anchor
            for name in landmark_regions:
                landmark_regions[name].update(s['region'] for s in services if s['service']==name)
            kinds=[entries[struct.unpack_from('>H',body,layout['combi_table']+(z*width+x)*2)[0]>>2][2]
                for z in range(layout['height']) for x in range(pad,pad+21)]
            for kind in ('mFM_BLOCK_TYPE_MUSEUM','mFM_BLOCK_TYPE_POLICE_BOX','mFM_BLOCK_TYPE_SHRINE'):
                assert kinds.count(kind)==1,kind
            actor_counts={item:0 for item in (0x5808,0x580c,0x5825,0x583b,0x584a,0x584d)}
            for z in range(1,7):
                for x in range(pad,pad+21):
                    p=layout['fg']+((z-1)*fw+x-1)*512
                    for item in struct.unpack_from('>256H',body,p):
                        if item in actor_counts:actor_counts[item]+=1
            for item in (0x5808,0x580c,0x5825,0x584a):
                assert actor_counts[item]==1,(hex(item),actor_counts[item])
            assert actor_counts[0x584d]==2,actor_counts[0x584d]
            for region in (0,1,2):
                xs=range(pad+region*7,pad+(region+1)*7)
                local=[entries[struct.unpack_from('>H',body,layout['combi_table']+(z*width+x)*2)[0]>>2][2]
                    for z in range(layout['height']) for x in xs]
                assert local.count('mFM_BLOCK_TYPE_PLAYER_HOUSE')==1
                assert local.count('mFM_BLOCK_TYPE_TRACKS_DUMP')>=1
                for house in range(4):
                    found=[]
                    for z in range(1,7):
                        for x in xs:
                            p=layout['fg']+((z-1)*fw+x-1)*512
                            items=struct.unpack_from('>256H',body,p)
                            found.extend((x,z,u) for u,item in enumerate(items) if item==0x5800+house)
                    assert len(found)==1,(region,house,found)
        for x,combination in ((0,203),(width-1,206)):
            for z in range(7,16) if deep and pad else range(6,16) if deep else (6,7):
                pos=layout['combi_table']+(z*width+x)*2
                assert struct.unpack_from('>H',body,pos)[0]==combination<<2
        for z in range(layout.get('height',10)):
            for x in range(width):
                pos=layout['combi_table']+(z*width+x)*2
                outer_extension=x in (0,width-1) and z in (6,7)
                ocean_edit=deep and (z>=7 or (z==6 and (x<pad+14 or x==pad+20)) or (pad and (x<pad or x>pad+20)))
                before_type=entries[struct.unpack_from('>H',before,pos)[0]>>2][2]
                service_edit=before_type in ('mFM_BLOCK_TYPE_TRACKS_POST_OFFICE','mFM_BLOCK_TYPE_POLICE_BOX',
                    'mFM_BLOCK_TYPE_MUSEUM','mFM_BLOCK_TYPE_NEEDLEWORK','mFM_BLOCK_TYPE_SHRINE')
                if before_type=='mFM_BLOCK_TYPE_TRACKS_POST_OFFICE' and (x<pad+7 or x>=pad+14):
                    after_word=struct.unpack_from('>H',body,pos)[0]
                    assert entries[after_word>>2][2]=='mFM_BLOCK_TYPE_TRACKS_DUMP'
                    assert (after_word&3)==(struct.unpack_from('>H',before,pos)[0]&3)
                    p=layout['fg']+((z-1)*fw+x-1)*512
                    old_items=struct.unpack_from('>256H',before,p);new_items=struct.unpack_from('>256H',body,p)
                    assert sum(item==0x5808 for item in old_items)==1 and sum(item==0x583b for item in new_items)==1
                    assert next(i for i,item in enumerate(old_items) if item==0x5808)==next(i for i,item in enumerate(new_items) if item==0x583b)
                if not ocean_edit and not service_edit and not outer_extension and (x not in (pad+6,pad+7,pad+13,pad+14) or z not in (1,2,3,4,5,6,7)):
                    assert body[pos:pos+2]==before[pos:pos+2]
        for route in routes:
            for x in route['border_columns']:
                pos=layout['fg']+(5*fw+x-1)*512
                assert body[pos+32:pos+256]==bytes(224)
        if deep:
            assert body[0x1c:0x20]==(b'OCE3' if pad else b'OCE1')
            # Inspect actual native combination types, not fixed combo numbers.
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
                for corner_x in (pad,pad+20):
                    c=struct.unpack_from('>H',body,layout['combi_table']+(6*width+corner_x)*2)[0]>>2
                    assert entries[c][0]=='BG_TYPE_GRD_S_E1_1','Solid mountain base required at outer corner'
                generator=runpy.run_path(str(here/'world-generation.py'))['ocean_destinations']
                report=generator(body,layout,entries)
                ocean_layouts.add((tuple((i['x'],i['z'],i['left_variant'],i['right_variant'])
                    for i in report['islands']),tuple((w['x'],w['z'],w['variant']) for w in report['wall_landings'])))
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
                if seed==0:
                    # Simulate a previously generated OCE3 save whose islands
                    # predate the current mainland service-map hash. Migration
                    # must preserve those exact combo words and report their
                    # actual saved locations instead of choosing new ones.
                    preserved=bytearray(after)
                    ocean=next(i for i,e in enumerate(entries) if e==
                        ('BG_TYPE_GRD_S_O_1','FG_TYPE_EMPTY','mFM_BLOCK_TYPE_OCEAN_8'))
                    def put_ocean(x,z,index):
                        pos=layout['combi_table']+(z*width+x)*2
                        old=struct.unpack_from('>H',preserved,pos)[0]
                        struct.pack_into('>H',preserved,pos,(index<<2)|(old&3))
                    for z in range(7,15):
                        for x in range(1,width-1):
                            idx=struct.unpack_from('>H',preserved,layout['combi_table']+(z*width+x)*2)[0]>>2
                            if entries[idx][0].startswith(('BG_TYPE_GRD_S_IL_','BG_TYPE_GRD_S_IR_')) and entries[idx][1:] == \
                                    ('FG_TYPE_EMPTY','mFM_BLOCK_TYPE_OCEAN_8'):
                                put_ocean(x,z,ocean)
                    expected_islands=[(10,8,3,2),(20,10,4,4),(14,11,3,4),(5,13,2,3)]
                    expected_walls=[(1,13,'IR',2),(width-2,10,'IL',3)]
                    for x,z,left_v,right_v in expected_islands:
                        left=next(i for i,e in enumerate(entries) if e==
                            (f'BG_TYPE_GRD_S_IL_{left_v}','FG_TYPE_EMPTY','mFM_BLOCK_TYPE_OCEAN_8'))
                        right=next(i for i,e in enumerate(entries) if e==
                            (f'BG_TYPE_GRD_S_IR_{right_v}','FG_TYPE_EMPTY','mFM_BLOCK_TYPE_OCEAN_8'))
                        put_ocean(x,z,left);put_ocean(x+1,z,right)
                    for x,z,half,variant in expected_walls:
                        marker=next(i for i,e in enumerate(entries) if e==
                            (f'BG_TYPE_GRD_S_{half}_{variant}','FG_TYPE_EMPTY','mFM_BLOCK_TYPE_OCEAN_8'))
                        put_ocean(x,z,marker)
                    sea_before=bytes(preserved[layout['combi_table']+7*width*2:
                        layout['combi_table']+15*width*2])
                    connect(preserved,layout,templates,here.parents[1])
                    sea_after=bytes(preserved[layout['combi_table']+7*width*2:
                        layout['combi_table']+15*width*2])
                    assert sea_after==sea_before,'Migration changed existing OCE3 sea destinations'
                    saved_report=generator(preserved,layout,entries)
                    assert [(i['x'],i['z'],i['left_variant'],i['right_variant']) for i in saved_report['islands']]==expected_islands
                    assert [(w['x'],w['z'],w['variant']) for w in saved_report['wall_landings']]==[(1,13,2),(width-2,10,3)]
                    assert saved_report['source']=='saved' and saved_report['seed'] is None
            assert islands==[(pad+18,12),(pad+19,12)] and ports==[(pad+19,6)]
            assert boats==[(pad+19,6,165)] and signs==[(pad+19,6)]
            for row in range(2):
                for x in range(width):
                    p=layout['combi_table']+((8+row)*width+x)*2
                    word=struct.unpack_from('>H',before,p)[0]
                    q=layout['combi_table']+((16+row)*width+x)*2
                    if entries[word>>2][0].startswith('BG_TYPE_ROM_'):assert body[q:q+2]==before[p:p+2]
                    else:assert entries[struct.unpack_from('>H',body,q)[0]>>2][2]=='mFM_BLOCK_TYPE_OCEAN_8'
    if pad and args.seeds>=10:
        assert len(ocean_layouts)>=10,'World seeds must vary island placement'
        for name,regions in landmark_regions.items():
            assert regions=={0,1,2},f'{name} must distribute across all towns: {regions}'
    print(f'PASS: {args.seeds} native generator seeds; coast joins; preserved towns; clear shoulders; idempotent rules'+
          ('; enclosed sea, unique ferry island, spaced exploration islands/wall landings and connected sailing channels; preserved indoor templates' if deep else ''))
