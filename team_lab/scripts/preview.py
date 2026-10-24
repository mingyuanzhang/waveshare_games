#!/usr/bin/env python3
import pathlib,wave
from PIL import Image,ImageDraw,ImageFont
ROOT=pathlib.Path(__file__).resolve().parent.parent;PREVIEW=ROOT/'.build/preview'
for path in PREVIEW.glob('*.rgb565'):
 data=path.read_bytes();img=Image.new('RGB',(360,360));pixels=[]
 for i in range(0,len(data),2):
  n=data[i]|data[i+1]<<8;pixels.append((((n>>11)&31)*255//31,((n>>5)&63)*255//63,(n&31)*255//31))
 img.putdata(pixels);mask=Image.new('L',(360,360));ImageDraw.Draw(mask).ellipse((0,0,359,359),fill=255)
 img=Image.composite(img,Image.new('RGB',(360,360),'#0c1118'),mask);img.save(path.with_suffix('.png'))
sheet=Image.new('RGB',(360*3,390*2),'#0c1118');draw=ImageDraw.Draw(sheet)
for shape,name in enumerate(['RAIN','FIREWORKS','BALLOONS','GARDEN','RIBBONS','ORBITS']):
 img=Image.open(PREVIEW/f'effect-{shape}-1.png');x=shape%3*360;y=shape//3*390;sheet.paste(img,(x,y+25));draw.text((x+150,y+6),name,fill='#ddcbb2')
sheet.save(PREVIEW/'discoveries.png')
for path in PREVIEW.glob('*.pcm'):
 with wave.open(str(path.with_suffix('.wav')),'wb') as out:out.setnchannels(1);out.setsampwidth(2);out.setframerate(24000);out.writeframes(path.read_bytes())
print('Native frame and audio previews:',PREVIEW)
