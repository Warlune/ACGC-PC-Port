"""World-level rules applied after the game's native per-town generator.

Native combination assets supply the beaches; internal ridges use the PC
modular cliff mesh and its corresponding heightfield. No coordinate warps.
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
    flat=find('BG_TYPE_GRD_S_F_1')
    fg_id=int(entries[beach][1].split('_')[-1],16)  # FG_TYPE_0061
    if layout['width']%7 or layout['width']<14:raise ValueError('Expected adjacent native 7-column towns')
    width=layout['width'];fw=layout['fg_width'];result=[]
    def combi(x,z,c):
        struct.pack_into('>H',save,layout['combi_table']+(z*width+x)*2,c<<2)
    for seam in range(7,width,7):
        for x in (seam-1,seam):
            # These acres host native-scale modular cliff sections. The PC
            # client installs their matching rounded heightfield on field init.
            # A flat, dry base avoids exterior coastal water/corner attributes.
            for z in range(1,6):
                combi(x,z,flat)
                p=layout['fg']+((z-1)*fw+x-1)*512
                save[p:p+512]=bytes(512)
                p=layout['deposit']+((z-1)*fw+x-1)*32
                save[p:p+32]=bytes(32)
            combi(x,6,beach);combi(x,7,ocean)
            p=layout['fg']+((6-1)*fw+x-1)*512
            save[p:p+512]=templates[fg_id]
            # Keep the northern beach shoulder free of trees/objects. The
            # native beach collision still owns shoreline/water boundaries.
            save[p+1*32:p+8*32]=bytes(7*32)
            p=layout['deposit']+((6-1)*fw+x-1)*32
            save[p:p+32]=bytes(32)
        result.append(dict(border_columns=[seam-1,seam],beach_row=6,
            walk_z=4020,gate_x=seam*640,mountain_cap_row=5,
            ridge_piece_scale=1,corner_radius=160,mountain_end_z=3760))
    return result
