"""Label a native minimap export, including the actual saved boat island."""
import argparse,json,struct
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont

def render_continuous(terrain,symbols,layout,output):
    """Every outdoor acre at the same scale, including the surrounding sea."""
    width,rows=layout['width'],layout['visible_rows']
    if terrain.size!=(width*64,rows*64):raise ValueError('Atlas and world layout differ')
    scale=112;pad=32;top=70
    canvas=Image.new('RGB',(width*scale+pad*2,rows*scale+top+pad),'#102c3c')
    canvas.paste(terrain.resize((width*scale,rows*scale),Image.Resampling.BICUBIC),(pad,top))
    draw=ImageDraw.Draw(canvas)
    font=lambda size:ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',size)
    for region,name in enumerate(('WEST','CENTER','EAST')):
        draw.text((pad+(layout.get('town_start',0)+region*7)*scale,20),name,font=font(26),fill='#e2eddf')
    glyphs=['H','o','S','P','T','M','!','A','W','B','D','^','L']
    colors=['#cfaa52','#929989','#659b62','#688ea9','#ae665b','#81799c','#587f9e',
            '#b781a4','#5caaa0','#548da6','#8a986b','#ab9462','#c59b61']
    for s in sorted(symbols,key=lambda s:s['kind']!=1):
        x,y=pad+s['x']*scale,top+s['z']*scale;k=s['kind'];r=8 if k==1 else 11
        draw.ellipse((x-r,y-r,x+r,y+r),fill=colors[k],outline='#10252b',width=2)
        text=glyphs[k];box=draw.textbbox((0,0),text,font=font(15))
        draw.text((x-(box[2]-box[0])/2,y-11),text,font=font(15),fill='white')
    draw.text((pad+(width/2-1)*scale,top+10*scale),'OCEAN',font=font(29),fill='#b8d4e8',stroke_width=1,stroke_fill='#234d8c')
    draw.text((pad+(layout.get('town_start',0)+17.9)*scale,top+13.2*scale),'ISLAND',font=font(22),fill='#dfebd7',stroke_width=1,stroke_fill='#17437a')
    canvas.save(output)

def render(atlas,save,layout,output):
    terrain=Image.open(atlas).convert('RGB').transpose(Image.Transpose.FLIP_TOP_BOTTOM)
    symbols=json.loads(Path(str(atlas)+'.json').read_text())
    if layout.get('height',10)>=18:return render_continuous(terrain,symbols,layout,output)
    if terrain.size!=(1344,576):raise ValueError('Expected expanded-world planning export')
    font=lambda size:ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',size)
    canvas=Image.new('RGB',(2464,1370),'#13252b');draw=ImageDraw.Draw(canvas)
    left,top,scale=56,118,112
    main=terrain.crop((0,48,1344,512)).resize((2352,812),Image.Resampling.BICUBIC)
    canvas.paste(main,(left,top))
    draw.text((56,22),'WORLD MAP + BOAT ISLAND',font=font(30),fill='#f4f3df')
    draw.text((56,65),'WEST',font=font(24),fill='#f4f3df')
    draw.text((56+7*scale,65),'CENTER',font=font(24),fill='#f4f3df')
    draw.text((56+14*scale,65),'EAST',font=font(24),fill='#f4f3df')
    for x in range(22):
        px=left+x*scale
        draw.line((px,top,px,top+812),fill='#656a58',width=1 if x%7 else 3)
        if x<21:draw.text((px+48,96),str(x+1),font=font(15),fill='#bbcac7')
    for z in range(1,8):
        y=top+(z-.75)*scale
        draw.line((left,y,left+2352,y),fill='#656a58')
        draw.text((23,y+40),chr(64+z),font=font(18),fill='#bbcac7')
    styles=[('H','Homes','#cfaa52'),('o','Villagers','#929989'),('S','Shop','#659b62'),
            ('P','Post office','#688ea9'),('T','Station','#ae665b'),('M','Museum','#81799c'),
            ('!','Police','#587f9e'),('A','Tailor','#b781a4'),('W','Well','#5caaa0'),
            ('B','Boat dock','#548da6'),('D','Dump','#8a986b'),('^','Tent','#ab9462'),
            ('L','Lighthouse','#c59b61')]
    def badge(x,y,kind,radius=12):
        text,_,color=styles[kind]
        draw.ellipse((x-radius,y-radius,x+radius,y+radius),fill=color,outline='#0b1d24',width=2)
        box=draw.textbbox((0,0),text,font=font(16))
        draw.text((x-(box[2]-box[0])/2,y-12),text,font=font(16),fill='white')
    for s in sorted(symbols,key=lambda s:s['kind']!=1):
        if .75<=s['z']<8:badge(left+s['x']*scale,top+(s['z']-.75)*scale,s['kind'],9 if s['kind']==1 else 12)
    # Native island combinations are row 8, center-town columns 11 and 12.
    island=terrain.crop((11*64,8*64,13*64,9*64)).resize((560,280),Image.Resampling.BICUBIC)
    ix,iy=left,990
    canvas.paste(island,(ix,iy))
    draw.text((left,945),'BOAT ISLAND - enlarged 2.5x',font=font(25),fill='#f4f3df')
    raw=Path(save).read_bytes();body=raw[64+layout['others']:]
    for block in range(2):
        items=struct.unpack_from('>256H',body,layout['island']+0x14+block*512)
        for i,item in enumerate(items):
            # Cottage anchors from the actual island save, including placeholders.
            if item in (0x5850,0x5851,0xf123,0xf124):
                badge(ix+(block+(i%16+.5)/16)*280,iy+(i//16+.5)/16*280,0)
    draw.text((660,970),'Map key',font=font(24),fill='#f4f3df')
    for i,(_,name,_) in enumerate(styles):
        x=675+(i%4)*420;y=1023+(i//4)*60
        badge(x,y,i);draw.text((x+24,y-15),name,font=font(20),fill='#dbe5dd')
    draw.text((56,1310),'Actual saved terrain. Island shown separately; its panel position and enlargement are for planning.',font=font(19),fill='#b6cac7')
    draw.text((56,1337),'Terrain + building markers; trees and other foreground meshes are not part of the minimap terrain pass.',font=font(18),fill='#b6cac7')
    canvas.save(output)

if __name__=='__main__':
    p=argparse.ArgumentParser()
    for name in ('atlas','save','layout','output'):p.add_argument(name)
    a=p.parse_args();render(a.atlas,a.save,json.loads(Path(a.layout).read_text()),a.output)
