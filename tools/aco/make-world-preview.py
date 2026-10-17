"""Assemble native-generated west + original center + native-generated east.

Requires generator JSON from generate_dev_town and the user's local GAFE01 disc.
Never overwrites a save or edits the input. Output is a dev-only save format.
"""
import argparse,json,runpy,struct
from pathlib import Path
Disc=runpy.run_path(str(Path(__file__).with_name('disc-image.py')))['Disc']

def assemble(source,destination,old,new,west,east,disc):
    if Path(source).resolve()==Path(destination).resolve():raise ValueError('Input/output must differ')
    raw=Path(source).read_bytes()
    if len(raw)!=64+old['file_data_size'] or raw[:4]!=b'GAFE':raise ValueError('Expected normal GAFE save')
    if old['width']!=7 or new['width']!=21 or new['sector_size']!=0x30000:raise ValueError('Unsupported layout')
    a=raw[64+old['others']:64+old['others']+old['save_size']]
    if sum(struct.unpack('>'+str(len(a)//2)+'H',a))&65535:raise ValueError('Invalid source checksum')
    fgdata=Disc(disc).archive_file('forest_1st.arc','fgdata.bin')
    if len(fgdata)%518:raise ValueError('Invalid foreground resource size')
    templates={struct.unpack_from('>H',fgdata,p)[0]:fgdata[p+2:p+514] for p in range(0,len(fgdata),518)}
    b=bytearray(new['sector_size']);b[:old['fg']]=a[:old['fg']]
    for start,end in [('animals','melody'),('melody','deposit'),('last_grow_time','time_delta'),
                      ('time_delta','island'),('island','end')]:
        size=old[end]-old[start]
        if size>new[end]-new[start]:raise ValueError('Incompatible span '+start)
        b[new[start]:new[start]+size]=a[old[start]:old[start]+size]
    for z in range(10):
        p=new['combi_table']+(z*21+7)*2
        b[p:p+14]=a[old['combi_table']+z*14:old['combi_table']+(z+1)*14]
    for z in range(6):
        for x in range(5):
            for field,size in [('fg',512),('deposit',32)]:
                p=new[field]+(z*19+x+7)*size;q=old[field]+(z*5+x)*size
                b[p:p+size]=a[q:q+size]
        for bx in (6,7,13,14):
            p=new['fg']+(z*19+bx-1)*512;b[p:p+512]=templates[203]
    for region,offset in [(west,0),(east,14)]:
        if region['width']!=7 or region['height']!=10 or len(region['acres'])!=70:raise ValueError('Invalid generated town')
        for i,(combo,height,fg_id,kind) in enumerate(region['acres']):
            z,x=divmod(i,7);bx=offset+x
            struct.pack_into('>H',b,new['combi_table']+(z*21+bx)*2,(combo<<2)|height)
            if 1<=z<=6 and 1<=x<=5:
                p=new['fg']+((z-1)*19+bx-1)*512
                b[p:p+512]=templates[fg_id]
    # Original center terrain, trees/items and buried flags remain byte exact.
    for z in range(6):
        for x in range(5):
            for field,size in [('fg',512),('deposit',32)]:
                p=new[field]+(z*19+x+7)*size;q=old[field]+(z*5+x)*size
                if b[p:p+size]!=a[q:q+size]:raise ValueError('Center preservation failed')
    b[0x12:0x14]=bytes(2)
    checksum=(-sum(struct.unpack('>'+str(new['save_size']//2)+'H',b[:new['save_size']])))&65535
    struct.pack_into('>H',b,0x12,checksum)
    if sum(struct.unpack('>'+str(new['save_size']//2)+'H',b[:new['save_size']]))&65535:raise ValueError('Output checksum failed')
    data=bytearray(new['file_data_size']);data[:old['others']]=raw[64:64+old['others']]
    for p in (new['others'],new['others']+new['sector_size']):data[p:p+len(b)]=b
    header=bytearray(raw[:64]);struct.pack_into('>H',header,0x38,len(data)//0x2000)
    destination=Path(destination);destination.parent.mkdir(parents=True,exist_ok=True)
    with destination.open('xb') as f:f.write(header+data)
    return dict(center_acres_preserved=30,west_seed=west['seed'],east_seed=east['seed'],
        native_generated_acres=60,columns=21,bytes=len(data)+64,passages=0,
        checksum='passed; backup mirrors main',source_unchanged=True)

if __name__=='__main__':
    p=argparse.ArgumentParser()
    for name in ('source','destination','disc'):p.add_argument(name)
    for name in ('old-layout','new-layout','west','east'):p.add_argument('--'+name,required=True)
    args=p.parse_args()
    read=lambda path:json.loads(Path(path).read_text(encoding='utf-8-sig'))
    print(json.dumps(assemble(args.source,args.destination,read(args.old_layout),read(args.new_layout),
        read(args.west),read(args.east),args.disc),indent=2))
