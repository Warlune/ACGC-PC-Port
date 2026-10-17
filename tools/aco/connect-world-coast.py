"""Apply the shared world-generation coast rule to an isolated preview copy."""
import argparse,json,re,runpy,struct
from pathlib import Path
here=Path(__file__).resolve().parent
Disc=runpy.run_path(str(here/'disc-image.py'))['Disc']
world_rules=runpy.run_path(str(here/'world-generation.py'))
connect=world_rules['coastal_connections']
world_service_map=world_rules['world_service_map']

def migrate(source,destination,disc,layout):
    if Path(source).resolve()==Path(destination).resolve():raise ValueError('Source/output must differ')
    raw=Path(source).read_bytes()
    if len(raw)!=64+layout['file_data_size'] or raw[:4]!=b'GAFE':raise ValueError('Expected triple-town dev save')
    start=64+layout['others'];body=bytearray(raw[start:start+layout['sector_size']])
    words='>'+str(layout['save_size']//2)+'H'
    if sum(struct.unpack(words,body[:layout['save_size']]))&65535:raise ValueError('Invalid source checksum')
    fg=Disc(disc).archive_file('forest_1st.arc','fgdata.bin')
    templates={struct.unpack_from('>H',fg,p)[0]:fg[p+2:p+514] for p in range(0,len(fg),518)}
    routes=connect(body,layout,templates,here.parents[1])
    body[0x12:0x14]=bytes(2)
    struct.pack_into('>H',body,0x12,(-sum(struct.unpack(words,body[:layout['save_size']])))&65535)
    result=bytearray(raw)
    for offset in (start,start+layout['sector_size']):result[offset:offset+len(body)]=body
    report=dict(source_unchanged=True,connections=routes,bytes=len(result))
    if layout.get('town_start',0):
        entries=re.findall(r'\{\s*(BG_TYPE_\w+)\s*,\s*(FG_TYPE_\w+)\s*,\s*(mFM_BLOCK_TYPE_\w+)\s*\}',
            (here.parents[1]/'src/data/combi/data_combi.c').read_text())
        report['world_services']=world_service_map(body,layout,entries,templates,here.parents[1],False)
        report['ocean']=runpy.run_path(str(here/'world-generation.py'))['ocean_destinations'](body,layout,entries)
    destination=Path(destination);destination.parent.mkdir(parents=True,exist_ok=True)
    with destination.open('xb') as f:f.write(result)
    return report

if __name__=='__main__':
    p=argparse.ArgumentParser()
    for name in ('source','destination','disc','layout'):p.add_argument(name)
    a=p.parse_args()
    print(json.dumps(migrate(a.source,a.destination,a.disc,
        json.loads(Path(a.layout).read_text(encoding='utf-8-sig'))),indent=2))
