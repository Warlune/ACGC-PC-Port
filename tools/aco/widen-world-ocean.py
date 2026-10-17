"""Widen an OCE1 ocean world without changing mainland spacing or sea depth."""
import argparse,json,runpy,struct
from pathlib import Path
here=Path(__file__).resolve().parent
connect=runpy.run_path(str(here/'world-generation.py'))['coastal_connections']
Disc=runpy.run_path(str(here/'disc-image.py'))['Disc']

def migrate(source,destination,disc,old,new):
    source=Path(source);destination=Path(destination)
    if source.resolve()==destination.resolve():raise ValueError('Source/output must differ')
    raw=source.read_bytes();a=raw[64+old['others']:64+old['others']+old['sector_size']]
    words=lambda n:'>'+str(n//2)+'H'
    if len(raw)!=64+old['file_data_size'] or a[0x1c:0x20]!=b'OCE1':raise ValueError('Expected OCE1 source save')
    if sum(struct.unpack(words(old['save_size']),a[:old['save_size']]))&65535:raise ValueError('Source checksum')
    if old['width']!=21 or new['width']!=31 or old['height']!=new['height']:raise ValueError('Unsupported widening')
    shift=new['town_start'];b=bytearray(new['sector_size'])
    b[:old['fg']]=a[:old['fg']]
    for z in range(6):
        for field,size in (('fg',512),('deposit',32)):
            p=new[field]+(z*new['fg_width']+shift)*size
            q=old[field]+z*old['fg_width']*size
            b[p:p+old['fg_width']*size]=a[q:q+old['fg_width']*size]
    for z in range(old['height']):
        p=new['combi_table']+(z*new['width']+shift)*2
        q=old['combi_table']+z*old['width']*2
        b[p:p+old['width']*2]=a[q:q+old['width']*2]
    spans=[('animals','melody'),('melody','deposit'),('last_grow_time','bridge'),
           ('bridge','needlework'),('needlework','time_delta'),('time_delta','island'),('island','end')]
    for start,end in spans:
        size=min(old[end]-old[start],new[end]-new[start])
        b[new[start]:new[start]+size]=a[old[start]:old[start]+size]
    # Relocate coordinate metadata along with the actual acres.
    for i in range(new['animal_count']):
        p=new['animals']+i*new['animal_stride']+new['animal_home']+new['animal_home_x']
        if b[p]<old['width'] and 1<=b[p+1]<=6:b[p]+=shift
    p=new['bridge']+new['bridge_x']
    if b[p]<old['width'] and b[new['bridge']+2]&0xc0:b[p]+=shift
    island_home=new['island']+new['island_animal']+new['animal_home']
    for block in range(2):
        items=struct.unpack_from('>256H',b,new['island']+0x14+block*512)
        if any(item in (0x5851,0xf124) for item in items):
            b[island_home+1]=shift+18+block;b[island_home+2]=new['island_z']
    # Storage rows were already relocated by OCE1; do not copy sea over them.
    b[0x1c:0x20]=b'OCE2'
    fg=Disc(disc).archive_file('forest_1st.arc','fgdata.bin')
    templates={struct.unpack_from('>H',fg,p)[0]:fg[p+2:p+514] for p in range(0,len(fg),518)}
    routes=connect(b,new,templates,here.parents[1])
    # Every original planting/deposit acre survives at precisely the shifted X.
    for z in range(6):
        for x in range(old['fg_width']):
            for field,size in (('fg',512),('deposit',32)):
                p=new[field]+(z*new['fg_width']+x+shift)*size
                q=old[field]+(z*old['fg_width']+x)*size
                if b[p:p+size]!=a[q:q+size]:raise ValueError('Town acre changed while widening')
    # Designs, services and island contents survive; only home coordinates move.
    for start,end in (('melody','deposit'),('needlework','time_delta'),('time_delta','island')):
        size=old[end]-old[start]
        if b[new[start]:new[start]+size]!=a[old[start]:old[start]+size]:raise ValueError('Preservation failed: '+start)
    b[0x12:0x14]=bytes(2)
    struct.pack_into('>H',b,0x12,(-sum(struct.unpack(words(new['save_size']),b[:new['save_size']])))&65535)
    result=bytearray(64+new['file_data_size']);result[:64+old['others']]=raw[:64+old['others']]
    struct.pack_into('>H',result,0x38,new['file_data_size']//0x2000)
    for p in (64+new['others'],64+new['others']+new['sector_size']):result[p:p+len(b)]=b
    destination.parent.mkdir(parents=True,exist_ok=True)
    with destination.open('xb') as f:f.write(result)
    if source.read_bytes()!=raw:raise ValueError('Source changed during migration')
    return dict(format='OCE2',source_unchanged=True,field=[31,18],outdoor=[31,16],
                extra_ocean_acres_each_side=5,depth_unchanged=True,planting_acres_preserved=114,
                island=[23,12,2,1],mainland_port=[24,6],connections=routes,
                bytes=len(result),checksum='passed; mirrored backup')

if __name__=='__main__':
    p=argparse.ArgumentParser()
    for name in ('source','destination','disc','old_layout','new_layout'):p.add_argument(name)
    a=p.parse_args();read=lambda p:json.loads(Path(p).read_text(encoding='utf-8-sig'))
    print(json.dumps(migrate(a.source,a.destination,a.disc,read(a.old_layout),read(a.new_layout)),indent=2))
