"""Migrate an isolated three-town save into the enclosed ocean layout.

The source is read-only; output creation is exclusive. Native indoor templates,
residents, designs, services and island progress survive offset/alignment changes.
"""
import argparse,json,runpy,struct
from pathlib import Path
here=Path(__file__).resolve().parent
connect=runpy.run_path(str(here/'world-generation.py'))['coastal_connections']
Disc=runpy.run_path(str(here/'disc-image.py'))['Disc']

def migrate(source,destination,disc,old,new):
    source=Path(source);destination=Path(destination)
    if source.resolve()==destination.resolve():raise ValueError('Source/output must differ')
    raw=source.read_bytes()
    if len(raw)!=64+old['file_data_size'] or raw[:4]!=b'GAFE':raise ValueError('Expected converted dev save')
    a=raw[64+old['others']:64+old['others']+old['sector_size']]
    words=lambda n:'>'+str(n//2)+'H'
    if sum(struct.unpack(words(old['save_size']),a[:old['save_size']]))&65535:raise ValueError('Invalid source checksum')
    if old['width']!=21 or old['height']!=10 or new['height']!=18:raise ValueError('Unsupported source/target layout')
    b=bytearray(new['sector_size'])
    b[:old['combi_table']]=a[:old['combi_table']]
    b[new['combi_table']:new['combi_table']+10*21*2]=a[old['combi_table']:old['combi_table']+10*21*2]
    spans=[('animals','melody'),('melody','deposit'),('deposit','last_grow_time'),
           ('last_grow_time','bridge'),('bridge','needlework'),('needlework','time_delta'),
           ('time_delta','island'),('island','end')]
    for start,end in spans:
        # Only compiler alignment padding differs at the bridge/design boundary.
        size=min(old[end]-old[start],new[end]-new[start])
        if start=='bridge' and size<8:raise ValueError('Invalid bridge layout')
        b[new[start]:new[start]+size]=a[old[start]:old[start]+size]
    fg=Disc(disc).archive_file('forest_1st.arc','fgdata.bin')
    templates={struct.unpack_from('>H',fg,p)[0]:fg[p+2:p+514] for p in range(0,len(fg),518)}
    routes=connect(b,new,templates,here.parents[1])
    for start,end in spans:
        size=min(old[end]-old[start],new[end]-new[start])
        if b[new[start]:new[start]+size]!=a[old[start]:old[start]+size]:raise ValueError('Preservation failed: '+start)
    import re
    entries=re.findall(r'\{\s*(BG_TYPE_\w+)\s*,\s*(FG_TYPE_\w+)\s*,\s*(mFM_BLOCK_TYPE_\w+)\s*\}',
        (here.parents[1]/'src/data/combi/data_combi.c').read_text())
    for row in range(2):
        for x in range(21):
            p=old['combi_table']+((8+row)*21+x)*2
            word=struct.unpack_from('>H',a,p)[0]
            if entries[word>>2][0].startswith('BG_TYPE_ROM_'):
                q=new['combi_table']+((16+row)*21+x)*2
                if b[q:q+2]!=a[p:p+2]:raise ValueError('Indoor template changed')
    b[0x12:0x14]=bytes(2)
    struct.pack_into('>H',b,0x12,(-sum(struct.unpack(words(new['save_size']),b[:new['save_size']])))&65535)
    assert sum(struct.unpack(words(new['save_size']),b[:new['save_size']]))&65535==0
    result=bytearray(raw)
    for p in (64+new['others'],64+new['others']+new['sector_size']):result[p:p+len(b)]=b
    destination.parent.mkdir(parents=True,exist_ok=True)
    with destination.open('xb') as f:f.write(result)
    if source.read_bytes()!=raw:raise ValueError('Source changed during migration')
    return dict(source_unchanged=True,bytes=len(result),field=[21,18],outdoor=[21,16],
                island=[18,12,2,1],mainland_port=[19,6],connections=routes,
                preserved=['residents','homes','services','designs','bridge','island progress','indoor templates'],
                checksum='passed; mirrored backup',format='OCE1')

if __name__=='__main__':
    p=argparse.ArgumentParser()
    for name in ('source','destination','disc','old_layout','new_layout'):p.add_argument(name)
    a=p.parse_args();read=lambda p:json.loads(Path(p).read_text(encoding='utf-8-sig'))
    print(json.dumps(migrate(a.source,a.destination,a.disc,read(a.old_layout),read(a.new_layout)),indent=2))
