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
    fg=Disc(args.disc).archive_file('forest_1st.arc','fgdata.bin')
    templates={struct.unpack_from('>H',fg,p)[0]:fg[p+2:p+514] for p in range(0,len(fg),518)}
    for seed in range(args.seeds):
        town=json.loads(subprocess.check_output([args.generator,str(seed+1234)]))
        body=bytearray(layout['sector_size'])
        for offset in (0,7,14):
            for i,(combo,height,fg_id,kind) in enumerate(town['acres']):
                z,x=divmod(i,7)
                struct.pack_into('>H',body,layout['combi_table']+(z*21+x+offset)*2,(combo<<2)|height)
        before=bytes(body)
        routes=connect(body,layout,templates,here.parents[1])
        after=bytes(body)
        assert len(routes)==2
        assert connect(body,layout,templates,here.parents[1])==routes and bytes(body)==after
        for z in range(10):
            for x in range(21):
                pos=layout['combi_table']+(z*21+x)*2
                if x not in (6,7,13,14) or z not in (1,2,3,4,5,6,7):
                    assert body[pos:pos+2]==before[pos:pos+2]
        for route in routes:
            for x in route['border_columns']:
                pos=layout['fg']+(5*19+x-1)*512
                assert body[pos+32:pos+256]==bytes(224)
    print(f'PASS: {args.seeds} native generator seeds; two coast joins; unchanged interiors; clear shoulders; idempotent rules')
