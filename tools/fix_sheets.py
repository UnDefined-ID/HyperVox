import sys,json,re
import numpy as np
from PIL import Image,ImageDraw
sys.argv=[sys.argv[0]]
import render_new as R
M=R.parse('/tmp/models_new.txt')
pat=re.compile(sys.argv[1] if len(sys.argv)>1 else '.')
def sheet(names,path,S=96,views=((45,30),(135,30),(225,30),(315,30),(45,-35)),cols=3):
    ks=[k for k,m in enumerate(M) if m['name'] in names]
    cw=S*len(views); ch=S+12; rows=(len(ks)+cols-1)//cols
    im=Image.new('RGB',(cols*cw,rows*ch),(255,255,255)); d=ImageDraw.Draw(im)
    for i,k in enumerate(ks):
        x=(i%cols)*cw; y=(i//cols)*ch; d.text((x+2,y),M[k]['name'][:50],fill=(0,0,0))
        for j,(az,el) in enumerate(views):
            img,_=R.render(M[k],az,el,S); im.paste(Image.fromarray(img),(x+j*S,y+12))
    im.save(path); print(path,len(ks))
