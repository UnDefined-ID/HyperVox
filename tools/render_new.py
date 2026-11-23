import numpy as np, sys, math, os, json
from PIL import Image, ImageDraw
from multiprocessing import Pool
ATLAS=np.fromfile('/tmp/atlas.rgba',dtype=np.uint8).reshape(1024,1024,4)
SHADE=[0.431,1.0,0.686,0.686,0.843,0.843]
NRM=np.array([[0,-1,0],[0,1,0],[-1,0,0],[1,0,0],[0,0,-1],[0,0,1]],float)
def parse(path):
    models=[]; f=open(path)
    while True:
        h=f.readline()
        if not h: break
        p=h.split(); assert p[0]=='M'; bid=int(p[1]); name=p[2]; nv,ni,nt=int(p[3]),int(p[4]),int(p[5])
        v=np.array([[int(x) for x in f.readline().split()] for _ in range(nv)],dtype=np.int64).reshape(nv,2)
        idx=np.array([int(x) for x in f.readline().split()],dtype=np.int64); tid=np.array([int(x) for x in f.readline().split()],dtype=np.int64)
        a=v[:,0]; b=v[:,1]
        x=(a&511)/16; z=((a>>9)&511)/16; y=(((a>>18)&4095)|((b>>31)<<12))/16
        face=((a>>30)&3)|(((b>>30)&1)<<2); u=(b&511)/16; vv=((b>>9)&511)/16; tile=(b>>18)&4095
        models.append(dict(id=bid,name=name,P=np.stack([x,y,z],1),U=np.stack([u,vv],1),tile=tile,face=face,idx=idx,tidx=tid))
    return models
def tris(m):
    out=[]
    for arr,tr in ((m['idx'],False),(m['tidx'],True)):
        for i in range(0,len(arr)-2,3): out.append((arr[i],arr[i+1],arr[i+2],tr))
    return out
def render(m,az,el,S=64):
    if len(m['P'])==0: return np.full((S,S,3),205,np.uint8),0
    P=m['P'].copy(); cen=np.array([8.5,74.5,8.5]);
    if P[:,1].max()>75.5: cen[1]=75.0
    P=P-cen
    th=math.radians(az); ph=math.radians(el)
    c=np.array([math.cos(ph)*math.sin(th),math.sin(ph),math.cos(ph)*math.cos(th)])
    right=np.cross([0,1,0],c); right/=np.linalg.norm(right); up=np.cross(c,right)
    sx=P@right; sy=P@up; dz=P@c
    scale=S/2.1/ (1.0 if True else 1)/1.0; scale=S/2.0/1.0*0.52
    X=S/2+sx*scale*1.0*2.0/1.0*0.5*2; Y=S/2-sy*scale*2.0
    X=S/2+sx*S*0.40; Y=S/2-sy*S*0.40
    img=np.full((S,S,3),205,np.float32); zb=np.full((S,S),-1e9,np.float32); cnt=0
    order=sorted(tris(m),key=lambda t:t[3])
    for (i0,i1,i2,tr) in order:
        p0,p1,p2=P[i0],P[i1],P[i2]
        n=np.cross(p1-p0,p2-p0)
        if n@c<=1e-9: continue
        x0,x1,x2=X[[i0,i1,i2]]; y0,y1,y2=Y[[i0,i1,i2]]
        minx=max(int(math.floor(min(x0,x1,x2))),0); maxx=min(int(math.ceil(max(x0,x1,x2))),S-1)
        miny=max(int(math.floor(min(y0,y1,y2))),0); maxy=min(int(math.ceil(max(y0,y1,y2))),S-1)
        if minx>maxx or miny>maxy: continue
        gx,gy=np.meshgrid(np.arange(minx,maxx+1)+0.5,np.arange(miny,maxy+1)+0.5)
        den=(y1-y2)*(x0-x2)+(x2-x1)*(y0-y2)
        if abs(den)<1e-9: continue
        l0=((y1-y2)*(gx-x2)+(x2-x1)*(gy-y2))/den; l1=((y2-y0)*(gx-x2)+(x0-x2)*(gy-y2))/den; l2=1-l0-l1
        inside=(l0>=-1e-4)&(l1>=-1e-4)&(l2>=-1e-4)
        if not inside.any(): continue
        z=l0*dz[i0]+l1*dz[i1]+l2*dz[i2]
        uu=l0*m['U'][i0,0]+l1*m['U'][i1,0]+l2*m['U'][i2,0]; vv=l0*m['U'][i0,1]+l1*m['U'][i1,1]+l2*m['U'][i2,1]
        tile=int(m['tile'][i0]); shade=SHADE[int(m['face'][i0])]
        tx=(tile%64)*16+np.clip(((uu-np.floor(uu))*16).astype(int),0,15); ty=(tile//64)*16+np.clip(((vv-np.floor(vv))*16).astype(int),0,15)
        tex=ATLAS[ty,tx].astype(np.float32)
        ok=inside&(tex[...,3]>=26)&(z>zb[miny:maxy+1,minx:maxx+1]-1e-4)
        if not ok.any(): continue
        sub=img[miny:maxy+1,minx:maxx+1]; zs=zb[miny:maxy+1,minx:maxx+1]
        col=tex[...,:3]*shade
        if tr: a=(tex[...,3:4]/255.0)*0.85; col=col*a+sub*(1-a)
        sub[ok]=col[ok]
        if not tr: zs[ok]=z[ok]
        cnt+=int(ok.sum())
    return np.clip(img,0,255).astype(np.uint8),cnt
VIEWS=[(45,30),(135,30),(225,30),(315,30),(45,-30),(135,-30),(225,-30),(315,-30)]
def work(args):
    k,m=args; S=64
    ims=[];counts=[]
    for az,el in VIEWS:
        im,c=render(m,az,el,S); ims.append(im); counts.append(c)
    top=np.concatenate(ims[:4],1); bot=np.concatenate(ims[4:],1)
    return k,np.concatenate([top,bot],0),counts
if __name__=='__main__':
    models=parse('/tmp/models_new.txt'); print(len(models),'models',flush=True)
    only=sys.argv[1] if len(sys.argv)>1 else None
    os.makedirs('/tmp/shots_new',exist_ok=True)
    jobs=[(k,m) for k,m in enumerate(models)]
    res={}
    with Pool(1) as pool:
        for k,im,counts in pool.imap_unordered(work,jobs,chunksize=8):
            Image.fromarray(im).save(f'/tmp/shots_new/m{k:05d}.png'); res[k]=counts
    json.dump({str(k):v for k,v in res.items()},open('/tmp/counts_new.json','w'))
    json.dump([(m['id'],m['name']) for m in models],open('/tmp/names_new.json','w'))
    print('done')
