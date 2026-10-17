"""World-level rules applied after the game's native per-town generator.

Keep terrain and collision together by selecting native combination assets.
No render-only displacement, invisible floors or coordinate teleportation.
"""
import re,struct
from pathlib import Path

def coastal_connections(save,layout,templates,source_root):
    entries=re.findall(r'\{\s*(BG_TYPE_\w+)\s*,\s*(FG_TYPE_\w+)\s*,\s*(mFM_BLOCK_TYPE_\w+)\s*\}',
        (Path(source_root)/'src/data/combi/data_combi.c').read_text())
    def find(bg):
        return next(i for i,e in enumerate(entries) if e[0]==bg)
    beach=find('BG_TYPE_GRD_S_M_1')
    ocean=find('BG_TYPE_GRD_S_O_1')
    caps=[find('BG_TYPE_GRD_S_E3_M_1'),find('BG_TYPE_GRD_S_E2_M_1')]
    fg_id=int(entries[beach][1].split('_')[-1],16)  # FG_TYPE_0061
    if layout['width']%7 or layout['width']<14:raise ValueError('Expected adjacent native 7-column towns')
    width=layout['width'];fw=layout['fg_width'];result=[]
    def combi(x,z,c):
        struct.pack_into('>H',save,layout['combi_table']+(z*width+x)*2,c<<2)
    for seam in range(7,width,7):
        for x,cap in zip((seam-1,seam),caps):
            # End each mountain one acre north using the native coastal cap.
            # The original playable town acres remain on either side.
            combi(x,5,cap);combi(x,6,beach);combi(x,7,ocean)
            p=layout['fg']+((6-1)*fw+x-1)*512
            save[p:p+512]=templates[fg_id]
            # Keep the northern beach shoulder free of trees/objects. The
            # native beach collision still owns shoreline/water boundaries.
            save[p+1*32:p+8*32]=bytes(7*32)
            p=layout['deposit']+((6-1)*fw+x-1)*32
            save[p:p+32]=bytes(32)
        result.append(dict(border_columns=[seam-1,seam],beach_row=6,
            walk_z=4020,gate_x=seam*640,mountain_cap_row=5,
            ridge_x_scale=4,mountain_cap_z_scale=1.5))
    return result
