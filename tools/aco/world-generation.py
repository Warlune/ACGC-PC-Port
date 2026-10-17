"""World-level rules applied after the game's native per-town generator.

Native combination assets supply the beaches; internal ridges use the PC
modular cliff mesh and its corresponding heightfield. No coordinate warps.
"""
import re,struct,hashlib,random
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
    width=layout['width'];pad=layout.get('town_start',0);town_width=width-2*pad
    if town_width%7 or town_width<14:raise ValueError('Expected adjacent native 7-column towns')
    fw=layout['fg_width'];result=[]
    def combi(x,z,c):
        struct.pack_into('>H',save,layout['combi_table']+(z*width+x)*2,c<<2)
    # Only the far outside borders continue to the end of the ocean. The
    # shared-town coastal crossings retain their existing open shoulders.
    for x,bg in ((0,'BG_TYPE_GRD_S_E2_1'),(width-1,'BG_TYPE_GRD_S_E3_1')):
        for z in (6,7):combi(x,z,find(bg))
    for seam in range(pad+7,pad+town_width,7):
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
    if layout.get('height',10)>=18:
        enclosed_ocean(save,layout,entries)
    return result

def enclosed_ocean(save,layout,entries):
    """One east port/island, continuous deep water and an exterior mountain rim.

    The last two rows remain native indoor templates, outside the outdoor map.
    The format tag also makes repeated generator passes idempotent.
    """
    width=layout['width'];height=layout['height'];fw=layout['fg_width'];pad=layout.get('town_start',0)
    tag=b'OCE3' if pad else b'OCE1'
    def find(bg,kind=None):
        return next(i for i,e in enumerate(entries) if e[0]==bg and (kind is None or e[2]==kind))
    def position(x,z):return layout['combi_table']+(z*width+x)*2
    def get(x,z):return struct.unpack_from('>H',save,position(x,z))[0]>>2
    def put(x,z,c):struct.pack_into('>H',save,position(x,z),c<<2)
    ocean=find('BG_TYPE_GRD_S_O_1','mFM_BLOCK_TYPE_OCEAN_8')
    if save[0x1c:0x20] not in (b'OCE1',b'OCE2',b'OCE3'):
        # Preserve authored indoor scene templates before replacing old sea rows.
        old=bytes(save[layout['combi_table']+8*width*2:layout['combi_table']+10*width*2])
        p=layout['combi_table']+(height-2)*width*2
        save[p:p+len(old)]=old
    # Keep real indoor templates, not the discarded island copies that shared
    # their original storage rows. Global island searches must find one island.
    for z in range(height-2,height):
        for x in range(width):
            if not entries[get(x,z)][0].startswith('BG_TYPE_ROM_'):put(x,z,ocean)
    beach=find('BG_TYPE_GRD_S_M_1')
    flat=find('BG_TYPE_GRD_S_F_1')
    # Remove dock terrain and boat/sign anchors from west and center only.
    east_start=pad+14
    for x in range(pad,east_start):
        if entries[get(x,6)][2]=='mFM_BLOCK_TYPE_PORT':put(x,6,beach)
    for z in range(6):
        for x in range(1,east_start):
            p=layout['fg']+(z*fw+x-1)*512
            for unit in range(256):
                item=struct.unpack_from('>H',save,p+unit*2)[0]
                if item in (0x584f,0x5852):struct.pack_into('>H',save,p+unit*2,0)
    ports=[x for x in range(east_start,pad+21) if entries[get(x,6)][2]=='mFM_BLOCK_TYPE_PORT']
    if len(ports)!=1:raise ValueError('Expected exactly one native east-town port')
    # Native dock berth: unit (5,10), shared by the boat's authored approach.
    p=layout['fg']+(5*fw+ports[0]-1)*512+(10*16+5)*2
    struct.pack_into('>H',save,p,0x584f)
    for z in range(7,height-2):
        for x in range(width):put(x,z,ocean)
    # The real saved island survives, with its native foreground and designs.
    island_z=layout['island_z'];left=pad+18
    variants=save[layout['island']+0x18c8:layout['island']+0x18ca]
    for x,prefix,variant in ((left,'IL',variants[0]),(left+1,'IR',variants[1])):
        put(x,island_z,find(f'BG_TYPE_GRD_S_{prefix}_{(variant&3)+1}'))
    # Native straight side-wall collision. The bottom rim has a rotated native
    # profile installed by the client with a matching solid plateau heightfield.
    for z in range(6,height-2):
        put(0,z,find('BG_TYPE_GRD_S_E2_1'));put(width-1,z,find('BG_TYPE_GRD_S_E3_1'))
    for x in range(1,width-1):put(x,height-3,flat)
    if pad:
        # Unused upper corners are outside the mainland; the sea opens wider
        # below them. Authored coastal corner pieces join the new north rims.
        border=find('BG_TYPE_GRD_S_E1_1')
        for z in range(6):
            for x in range(width):
                if x<pad or x>pad+20:put(x,z,border)
        for x in range(width):
            if x<pad or x>pad+20:put(x,6,border)
        put(pad,6,find('BG_TYPE_GRD_S_E2_M_1'))
        put(pad+20,6,find('BG_TYPE_GRD_S_E3_M_1'))
    if pad: ocean_destinations(save,layout,entries)
    save[0x1c:0x20]=tag


def ocean_destinations(save,layout,entries):
    """Stable native-scale exploration islands, leaving navigable sea lanes.

    Hash only untouched mainland interiors, so reapplying generation cannot
    move destinations. New combinations keep the canonical island unique.
    """
    width=layout['width'];pad=layout.get('town_start',0)
    if not pad:return dict(seed=None,islands=[],wall_landings=[])
    def pos(x,z):return layout['combi_table']+(z*width+x)*2
    core=b''.join(save[pos(x,z):pos(x,z)+2] for z in range(1,6)
        for region in range(3) for x in range(pad+region*7+1,pad+region*7+6))
    seed=int.from_bytes(hashlib.blake2s(core,digest_size=4,person=b'ACOsea01').digest(),'big')
    rng=random.Random(seed)
    def marker(half,variant):
        return next(i for i,e in enumerate(entries) if e==
            (f'BG_TYPE_GRD_S_{half}_{variant}','FG_TYPE_EMPTY','mFM_BLOCK_TYPE_OCEAN_8'))
    def put(x,z,half,variant):struct.pack_into('>H',save,pos(x,z),marker(half,variant)<<2)
    # A generous protected route around the authored east ferry waypoints.
    blocked={(x,z) for z in range(7,15) for x in range(pad+17,pad+22)}
    occupied=set();landings=[];islands=[]
    def reserve(cells):
        for x,z in cells:
            occupied.update((x+dx,z+dz) for dx in (-1,0,1) for dz in (-1,0,1))
    for side,x,half in (('west',1,'IR'),('east',width-2,'IL')):
        z=rng.randint(9,13);v=rng.randint(1,4)
        put(x,z,half,v);reserve([(x,z)])
        landings.append(dict(side=side,x=x,z=z,variant=v))
    candidates=[(x,z) for z in range(8,14) for x in range(3,width-4)]
    rng.shuffle(candidates)
    target=4
    outlines=[(left,right) for left in range(1,5) for right in range(1,5)]
    rng.shuffle(outlines)
    for x,z in candidates:
        cells={(x,z),(x+1,z)}
        if cells & (blocked|occupied):continue
        lv,rv=outlines[len(islands)]
        put(x,z,'IL',lv);put(x+1,z,'IR',rv);reserve(cells)
        islands.append(dict(x=x,z=z,left_variant=lv,right_variant=rv))
        if len(islands)==target:break
    if len(islands)!=target:raise ValueError('Insufficient navigable island spacing')
    return dict(seed=seed,islands=islands,wall_landings=landings)
