"""Apply the shared world-generation coast rule to an isolated preview copy."""
import argparse,json,runpy,struct
from pathlib import Path
here=Path(__file__).resolve().parent
Disc=runpy.run_path(str(here/'disc-image.py'))['Disc']
connect=runpy.run_path(str(here/'world-generation.py'))['coastal_connections']

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
    destination=Path(destination);destination.parent.mkdir(parents=True,exist_ok=True)
    with destination.open('xb') as f:f.write(result)
    return dict(source_unchanged=True,connections=routes,bytes=len(result))

if __name__=='__main__':
    p=argparse.ArgumentParser()
    for name in ('source','destination','disc','layout'):p.add_argument(name)
    a=p.parse_args()
    print(json.dumps(migrate(a.source,a.destination,a.disc,
        json.loads(Path(a.layout).read_text(encoding='utf-8-sig'))),indent=2))
