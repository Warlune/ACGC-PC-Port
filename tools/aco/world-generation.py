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
        world_service_map(save,layout,entries,templates,source_root,True)
    return result


def world_service_map(save,layout,entries,templates=None,source_root=None,apply=True):
    """Deduplicate world services on native pads and expose their coordinates.

    Centre starter anchors remain at their original acre. Generated rail pads in
    west/east become scrap-yard pads when an extra post office is removed.
    Flat-landmarks are cleared with the native flat template while preserving the
    acreage height bits and all save-owned foreground/deposit data in the centre.
    """
    width=layout['width'];pad=layout.get('town_start',0);fw=layout['fg_width']
    if not pad or width-pad*2<21:return dict(services=[],seed=None)
    def combo(x,z):return struct.unpack_from('>H',save,layout['combi_table']+(z*width+x)*2)[0]>>2
    def put_combo(x,z,index):
        p=layout['combi_table']+(z*width+x)*2;old=struct.unpack_from('>H',save,p)[0]
        struct.pack_into('>H',save,p,(index<<2)|(old&3))
    role_kinds={
        'nook':'mFM_BLOCK_TYPE_TRACKS_SHOP',
        'post_office':'mFM_BLOCK_TYPE_TRACKS_POST_OFFICE',
        'dump':'mFM_BLOCK_TYPE_TRACKS_DUMP',
        'police':'mFM_BLOCK_TYPE_POLICE_BOX',
        'museum':'mFM_BLOCK_TYPE_MUSEUM',
        'tailor':'mFM_BLOCK_TYPE_NEEDLEWORK',
        'shrine':'mFM_BLOCK_TYPE_SHRINE',
        'player_house':'mFM_BLOCK_TYPE_PLAYER_HOUSE',
    }
    def anchors(kind):
        out=[]
        for town in range(3):
            lo=pad+town*7;hi=lo+7
            for z in range(min(8,layout.get('height',10))):
                for x in range(lo,hi):
                    ci=combo(x,z)
                    if 0<=ci<len(entries) and entries[ci][2]==kind:
                        out.append(dict(region=town,x=x,z=z,combo=ci,bg=entries[ci][0],fg=entries[ci][1]))
        return out
    # Hash only untouched ordinary mainland terrain. This keeps choices stable
    # after the duplicate pads have been converted on a second generator pass.
    service_types=set(role_kinds.values())
    core=bytearray()
    for z in range(1,min(7,layout.get('height',10))):
        for x in range(pad,pad+21):
            ci=combo(x,z)
            if ci<len(entries) and entries[ci][2] not in service_types and entries[ci][2]!='mFM_BLOCK_TYPE_FLAT':
                core.extend(struct.pack('>H',ci))
    seed=int.from_bytes(hashlib.blake2s(core,digest_size=4,person=b'ACOsvc01').digest(),'big')
    # A separate deterministic stream per role means removing one role cannot
    # change which region retains another.
    def choose(items,name,count=1):
        if len(items)<=count:return items[:]
        role_seed=hashlib.blake2s(seed.to_bytes(4,'big')+name.encode(),digest_size=4).digest()
        rng=random.Random(int.from_bytes(role_seed,'big'));copy=items[:];rng.shuffle(copy)
        return copy[:count]
    flat_idx=next((i for i,e in enumerate(entries) if e[0]=='BG_TYPE_GRD_S_F_1' and e[2]=='mFM_BLOCK_TYPE_FLAT'),None)
    dump_indices=[i for i,e in enumerate(entries) if e[2]=='mFM_BLOCK_TYPE_TRACKS_DUMP']
    if flat_idx is None or not dump_indices:raise ValueError('Native flat and dump pads are required')
    structure_ids={'police':0x580c,'museum':0x584a,'shrine':0x5825,'tailor':0x584d,
        'post_office':0x5808,'dump':0x583b}
    def acre_items(anchor):
        x,z=anchor['x'],anchor['z']
        if not (1<=z<=6 and 1<=x-pad<=19):return None
        return layout['fg']+((z-1)*fw+x-1)*512
    def remove_structure(anchor,name):
        if not apply:return
        p=acre_items(anchor)
        if p is not None:
            for unit in range(256):
                if struct.unpack_from('>H',save,p+unit*2)[0]==structure_ids[name]:
                    struct.pack_into('>H',save,p+unit*2,0)
    def clear_anchor(anchor,name):
        if not apply:return
        put_combo(anchor['x'],anchor['z'],flat_idx)
        # Remove only the duplicate building actor; preserve all other foreground
        # objects, buried-item bytes and player-owned plot anchors in this acre.
        remove_structure(anchor,name)
    # Keep one museum, police box, and wishing well, randomly distributed across
    # the three regions. All choices derive from the stable map seed.
    for name in ('museum','police','shrine'):
        found=anchors(role_kinds[name]);keep=choose(found,name,1)
        for a in found:
            if a not in keep:clear_anchor(a,name)
    # The center retains its native tailor when present. One noncenter tailor is
    # reassigned to the customization shop; any remaining duplicate is removed.
    tailors=anchors(role_kinds['tailor']);center=[a for a in tailors if a['region']==1]
    sides=[a for a in tailors if a['region']!=1]
    if center:
        customization=choose(sides,'customization',1)
        kept=center[:1]+customization
    else:
        native_tailor=choose(tailors,'native-tailor',1)
        customization=choose([a for a in tailors if a not in native_tailor],'customization',1)
        kept=native_tailor+customization
    for a in tailors:
        if a not in kept:clear_anchor(a,'tailor')
    # Nook pads in the two generated regions become the new market/outfitter
    # front doors in the PC event layer. Their native combination backgrounds
    # remain valid shop pads and thus continue to carry native door collision.
    # Post-office pads in those regions use the native railroad-compatible dump
    # combinations, while the center starter post office stays untouched.
    shops=anchors(role_kinds['nook'])
    new_shop_anchors=[a for a in shops if a['region']!=1]
    posts=anchors(role_kinds['post_office'])
    for a in posts:
        if a['region']==1:continue
        # Pick a dump template deterministically per town while retaining height.
        pick=dump_indices[(seed+a['x']*17+a['z'])%len(dump_indices)]
        if apply:
            put_combo(a['x'],a['z'],pick)
            p=acre_items(a)
            if p is not None:
                for unit in range(256):
                    if struct.unpack_from('>H',save,p+unit*2)[0]==structure_ids['post_office']:
                        struct.pack_into('>H',save,p+unit*2,structure_ids['dump'])
    def actor_anchor(anchor,role):
        actor_id={'nook':(0x5804,0x5807),'tailor':(0x584d,0x584d),
            'post_office':(0x5808,0x5808),'dump':(0x583b,0x583b),
            'police':(0x580c,0x580c),'museum':(0x584a,0x584a),
            'shrine':(0x5825,0x5825)}.get(role)
        info=dict(region=anchor['region'],acre_x=anchor['x'],acre_z=anchor['z'],
            world_x=anchor['x']*640+320,world_z=anchor['z']*640+320,
            combo=anchor['combo'],bg=anchor['bg'],fg=anchor['fg'])
        p=acre_items(anchor)
        if actor_id and p is not None:
            for unit in range(256):
                item=struct.unpack_from('>H',save,p+unit*2)[0]
                if actor_id[0]<=item<=actor_id[1]:
                    ux,uz=unit%16,unit//16
                    info.update(actor_item=item,unit_x=ux,unit_z=uz,
                        world_x=anchor['x']*640+ux*40+20,world_z=anchor['z']*640+uz*40+20)
                    if role in ('nook','tailor'):
                        info['world_x']-=20;info['world_z']+=20
                    break
        return info
    result=[]
    for role in ('nook','post_office','dump','police','museum','tailor','shrine','player_house'):
        for a in anchors(role_kinds[role]):
            result.append(dict(service=role,**actor_anchor(a,role)))
    # Report the four real player-house FG actors separately from their shared
    # native house-acre block role. These are physical plots, not player slots.
    for region in range(3):
        for z in range(1,7):
            for x in range(pad+region*7+1,pad+region*7+6):
                a=dict(region=region,x=x,z=z)
                p=acre_items(a)
                if p is None:continue
                for unit in range(256):
                    item=struct.unpack_from('>H',save,p+unit*2)[0]
                    if 0x5800<=item<=0x5803:
                        ux,uz=unit%16,unit//16
                        result.append(dict(service='player_house_plot',region=region,
                            plot_index=item-0x5800,acre_x=x,acre_z=z,unit_x=ux,unit_z=uz,
                            actor_item=item,world_x=x*640+ux*40+20,world_z=z*640+uz*40+20))
    # Shop pads retain the native Nook exterior/collision while the PC event layer
    # routes west/east entrances to their distinct businesses.
    for a,name in zip(sorted(new_shop_anchors,key=lambda v:v['region']),
                       ('night_market','maritime_outfitter')):
        result.append(dict(service=name,**actor_anchor(a,'nook')))
    # Include custom service identity independently of its still-native pad type.
    for a in customization:
        result.append(dict(service='customization',**actor_anchor(a,'tailor')))
    return dict(services=result,seed=seed)

def enclosed_ocean(save,layout,entries):
    """One east port/island, continuous deep water and an exterior mountain rim.

    The last two rows remain native indoor templates, outside the outdoor map.
    The format tag also makes repeated generator passes idempotent.
    """
    width=layout['width'];height=layout['height'];fw=layout['fg_width'];pad=layout.get('town_start',0)
    tag=b'OCE3' if pad else b'OCE1'
    saved_destinations=_saved_ocean_destinations(save,layout,entries) if pad else None
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
        put(pad,6,border)
        put(pad+20,6,border)
    if pad:
        if saved_destinations:
            # Keep authored exploration destinations byte-for-byte when this
            # rule is reapplied to an existing OCE3 save. In particular, do
            # not derive new positions from a changed mainland service hash.
            for x,z,word in saved_destinations['marker_words']:
                struct.pack_into('>H',save,position(x,z),word)
        else:
            _generate_ocean_destinations(save,layout,entries)
    save[0x1c:0x20]=tag


def _saved_ocean_destinations(save,layout,entries):
    """Read a complete set of saved exploration destinations without mutation."""
    width=layout['width'];pad=layout.get('town_start',0)
    height=layout.get('height',10)
    if not pad:return None
    def pos(x,z):return layout['combi_table']+(z*width+x)*2
    def get(x,z):return struct.unpack_from('>H',save,pos(x,z))[0]>>2
    markers={}
    for z in range(7,height-2):
        for x in range(1,width-1):
            combo=entries[get(x,z)]
            match=re.fullmatch(r'BG_TYPE_GRD_S_(IL|IR)_([1-4])',combo[0])
            if match and combo[1]=='FG_TYPE_EMPTY' and combo[2]=='mFM_BLOCK_TYPE_OCEAN_8':
                markers[(x,z)]=(match.group(1),int(match.group(2)),struct.unpack_from('>H',save,pos(x,z))[0])
    if not markers:return None
    islands=[];landings=[];used=set()
    for (x,z),(half,variant,_) in markers.items():
        if half!='IL' or (x,z) in used:continue
        right=markers.get((x+1,z))
        if right and right[0]=='IR' and 8<=z<=13:
            islands.append(dict(x=x,z=z,left_variant=variant,right_variant=right[1]))
            used.add((x,z));used.add((x+1,z))
    # Wall landings are the inner face of each straight outer wall.
    for side,x,half in (('west',1,'IR'),('east',width-2,'IL')):
        choices=[(z,entry) for (mx,z),entry in markers.items() if mx==x and entry[0]==half and 8<=z<=13]
        if len(choices)==1:
            z,entry=choices[0]
            landings.append(dict(side=side,x=x,z=z,variant=entry[1]))
            used.add((x,z))
    if (len(used)!=len(markers) or len(islands)!=4 or len(landings)!=2 or
            len({(i['left_variant'],i['right_variant']) for i in islands})!=4):
        raise ValueError('Existing ocean destination markers are incomplete or invalid; refusing to relocate them')
    blocked={(x,z) for z in range(7,15) for x in range(pad+17,pad+22)}
    occupied=set()
    def reserve(cells):
        for x,z in cells:
            occupied.update((x+dx,z+dz) for dx in (-1,0,1) for dz in (-1,0,1))
    for item in landings:
        if (item['x'],item['z']) in blocked or (item['x'],item['z']) in occupied:
            raise ValueError('Existing wall landing conflicts with the protected ferry route or another island')
        reserve([(item['x'],item['z'])])
    for item in islands:
        cells={(item['x'],item['z']),(item['x']+1,item['z'])}
        if cells & blocked or cells & occupied:
            raise ValueError('Existing exploration island conflicts with the protected route or another island')
        reserve(cells)
    marker_words=[(x,z,entry[2]) for (x,z),entry in markers.items()]
    return dict(seed=None,source='saved',islands=islands,wall_landings=landings,marker_words=marker_words)


def ocean_destinations(save,layout,entries):
    """Analyze the destination markers actually present in the saved combo table."""
    if not layout.get('town_start',0):return dict(seed=None,source='none',islands=[],wall_landings=[])
    result=_saved_ocean_destinations(save,layout,entries)
    if result is None:
        return dict(seed=None,source='none',islands=[],wall_landings=[])
    return {key:value for key,value in result.items() if key!='marker_words'}


def _generate_ocean_destinations(save,layout,entries):
    """Create new native-scale destinations only when no saved markers exist."""
    width=layout['width'];pad=layout.get('town_start',0)
    def pos(x,z):return layout['combi_table']+(z*width+x)*2
    def get(x,z):return struct.unpack_from('>H',save,pos(x,z))[0]>>2
    stable_types={'mFM_BLOCK_TYPE_TRACKS_SHOP','mFM_BLOCK_TYPE_TRACKS_POST_OFFICE','mFM_BLOCK_TYPE_TRACKS_DUMP',
        'mFM_BLOCK_TYPE_POLICE_BOX','mFM_BLOCK_TYPE_MUSEUM','mFM_BLOCK_TYPE_NEEDLEWORK',
        'mFM_BLOCK_TYPE_SHRINE','mFM_BLOCK_TYPE_PLAYER_HOUSE'}
    core=b''.join(save[pos(x,z):pos(x,z)+2] for z in range(1,6)
        for region in range(3) for x in range(pad+region*7+1,pad+region*7+6)
        if entries[get(x,z)][2] not in stable_types and entries[get(x,z)][0]!='BG_TYPE_GRD_S_F_1')
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
