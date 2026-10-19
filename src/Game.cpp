#include "Game.hpp"
#include "MemUtil.hpp"
#include <algorithm>
#include <chrono>
#include <atomic>
#include <cctype>
#include <thread>
#include <fstream>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace{
constexpr float kPi=3.14159265f;
inline int floorDiv16(float v){return int(std::floor(v))>>4;}
}

Game::Game(GLFWwindow*w):win_(w){
 settings_.load(); settings_.save();                         // writes the file right away so it always exists
 cacheDir_=Settings::dataDir()/"cache"; { auto d=Settings::packDirs(); packsDir_=d.empty()?Settings::dataDir()/"resourcepacks":d[0]; }
 glfwSetWindowUserPointer(w,this); glfwSetCharCallback(w,charCallback); glfwSetScrollCallback(w,scrollCallback); glfwSetDropCallback(w,dropCallback);
 initSounds();
 registerCommands();
 std::cout<<"Data folder: "<<Settings::dataDir().string()<<"\n";
 lastTime_=glfwGetTime();
 for(int k=32;k<=GLFW_KEY_LAST;k++)keyPrev_[k]=false;
}
Game::~Game(){}
void Game::charCallback(GLFWwindow*w,unsigned int cp){ auto*g=static_cast<Game*>(glfwGetWindowUserPointer(w)); if(g&&cp>=32&&cp<127)g->typed_+=char(cp); }
void Game::dropCallback(GLFWwindow*w,int n,const char**paths){ auto*g=static_cast<Game*>(glfwGetWindowUserPointer(w)); if(g)for(int i=0;i<n;i++)g->dropped_.push_back(std::filesystem::path(paths[i])); }
// Files dropped on the window: .png -> skin, .zip / folder -> texture pack
void Game::handleDrops(){
 if(dropped_.empty())return; const auto files=std::move(dropped_); dropped_.clear(); namespace fs=std::filesystem; std::error_code ec; int skins=0,packs=0;
 for(auto&f:files){ std::string ext=f.extension().string(); for(auto&ch:ext)ch=(char)std::tolower((unsigned char)ch);
  if(ext==".png"){ if(importPackOrSkin(f,Settings::skinsDir()))skins++; }
  else if(ext==".zip"||fs::is_directory(f,ec)){ if(importPackOrSkin(f,packsDir_))packs++; } }
 if(skins){ refreshSkins(); }
 if(packs){ packs_=listResourcePacks(assets_,Settings::packDirs(),cacheDir_/"packs"); }
 if(skins||packs){ noticeMsg_=skins?"Skin added":"Texture pack added"; noticeUntil_=time_+3.0; }
}
void Game::markWorldDirty(){ worldDirty_=true; dirtyAt_=glfwGetTime(); }
void Game::scrollCallback(GLFWwindow*w,double,double dy){ auto*g=static_cast<Game*>(glfwGetWindowUserPointer(w)); if(g)g->scroll_+=float(dy); }

// ------------------------------------------------------------------------------------------------ resources
bool Game::loadResources(){
 const auto t0=std::chrono::steady_clock::now();
 if(assets_.base.empty()){ const auto dd=ensureDataFolder("assets"); if(!dd.empty())assets_.base=AssetPaths::findBase(dd); }
 if(assets_.base.empty()){ assets_.base=AssetPaths::findBase("assets"); if(assets_.base.empty())assets_.base=AssetPaths::findBase("assets/minecraft"); }
 if(assets_.base.empty()){ std::cerr<<"Texture pack not found (expected an 'assets/block' folder next to the executable).\n"; return false; }
 packs_=listResourcePacks(assets_,Settings::packDirs(),cacheDir_/"packs");
 assets_.overlay.clear(); panoRotDone_=false; assets_.packName="default";
 if(!settings_.resourcePack.empty())for(auto&p:packs_)if(!p.builtin&&p.name==settings_.resourcePack&&materializePack(p)){assets_.overlay=AssetPaths::normalizePackRoot(p.dir);assets_.packName=p.name;}
 if(!pack_.load(assets_,cacheDir_)){ std::cerr<<"Could not load block textures.\n"; return false; }
 if(!registryReady_){Registry::get().init(pack_);registryReady_=true;}
 if(!uiAtlas_.load(assets_,pack_,cacheDir_)||!uiAtlas_.hasGlyphs()){ std::cerr<<"Could not build the GUI atlas (font or GUI textures missing).\n"; return false; }
 if(!loadPanorama(assets_,cacheDir_,pano_))std::cerr<<"Panorama textures not found; the title screen will use a plain background.\n";
 const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();
 std::cout<<"Textures ready in "<<int(ms)<<" ms (block atlas "<<(pack_.loadedFromCache()?"cached":"rebuilt")<<", GUI atlas "<<(uiAtlas_.loadedFromCache()?"cached":"rebuilt")<<"), pack: "<<assets_.packName<<"\n";
 return true;
}
void Game::applyResourcePack(){
 if(!loadResources()){ errorMsg_="Failed to load that resource pack"; settings_.resourcePack.clear(); settings_.save(); loadResources(); }
 if(renderer_&&!renderer_->reloadTextures(pack_.atlas(),uiAtlas_.image(),pano_))errorMsg_="Failed to upload textures";
}
void Game::attach(Renderer*r){
 renderer_=r; renderer_->setVsync(settings_.vsync);
 streamer_=std::make_unique<ChunkStreamer>(world_,*r);
}

// ------------------------------------------------------------------------------------------------ input
bool Game::keyDown(int k)const{return k>=32&&k<=GLFW_KEY_LAST&&glfwGetKey(win_,k)==GLFW_PRESS;}
bool Game::keyPressed(int k)const{return k>=32&&k<=GLFW_KEY_LAST&&keyDown(k)&&!keyPrev_[k];}
void Game::snapshotKeys(){for(int k=32;k<=GLFW_KEY_LAST;k++)keyPrev_[k]=glfwGetKey(win_,k)==GLFW_PRESS;}
void Game::captureMouse(bool on){
 if(on){ glfwSetInputMode(win_,GLFW_CURSOR,GLFW_CURSOR_DISABLED); if(glfwRawMouseMotionSupported())glfwSetInputMode(win_,GLFW_RAW_MOUSE_MOTION,GLFW_TRUE); mouseInit_=false; }
 else glfwSetInputMode(win_,GLFW_CURSOR,GLFW_CURSOR_NORMAL);
}
UiInput Game::gatherInput(int fbw,int fbh,double now){
 UiInput in; double cx=0,cy=0; glfwGetCursorPos(win_,&cx,&cy); int ww=1,wh=1; glfwGetWindowSize(win_,&ww,&wh); if(ww<=0)ww=1; if(wh<=0)wh=1;
 in.mx=float(cx*double(fbw)/double(ww)); in.my=float(cy*double(fbh)/double(wh));
 const bool d[3]={glfwGetMouseButton(win_,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS,glfwGetMouseButton(win_,GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS,glfwGetMouseButton(win_,GLFW_MOUSE_BUTTON_MIDDLE)==GLFW_PRESS};
 in.lclick=d[0]&&!mousePrev_[0]; in.rclick=d[1]&&!mousePrev_[1]; in.mclick=d[2]&&!mousePrev_[2]; in.ldown=d[0];
 for(int i=0;i<3;i++)mousePrev_[i]=d[i];
 in.shift=keyDown(GLFW_KEY_LEFT_SHIFT)||keyDown(GLFW_KEY_RIGHT_SHIFT);
 in.wheel=scroll_; scroll_=0; in.typed.swap(typed_); typed_.clear();
 const bool bs=glfwGetKey(win_,GLFW_KEY_BACKSPACE)==GLFW_PRESS;
 if(bs&&!bsPrev_){in.backspace=true;bsNext_=now+0.4;} else if(bs&&now>=bsNext_){in.backspace=true;bsNext_=now+0.04;}
 bsPrev_=bs; in.time=now; return in;
}
void Game::handleEscape(){
 switch(screen_){
  case Screen::Play: openPause(); break;
  case Screen::Pause: closePause(); break;
  case Screen::Inventory: case Screen::Creative: closeInventory(); break;
  case Screen::Options: screen_=optionsFrom_; settings_.save(); break;
  case Screen::Controls: rebinding_=-1; screen_=Screen::Options; settings_.save(); break;
  case Screen::Textures: screen_=Screen::Options; break;
  case Screen::CreateWorld: screen_=Screen::WorldSelect; focus_=-1; break;
  case Screen::ConfirmDelete: screen_=Screen::WorldSelect; break;
  case Screen::WorldSelect: screen_=Screen::Title; break;
  default: break;
 }
}

// ------------------------------------------------------------------------------------------------ player
bool Game::collides(const Vec3&p)const{
 const float hw=0.3f,hh=1.8f,e=1e-4f; const Registry&R=Registry::get();
 const int x0=(int)std::floor(p.x-hw+e),x1=(int)std::floor(p.x+hw-e),y0=(int)std::floor(p.y+e),y1=(int)std::floor(p.y+hh-e),z0=(int)std::floor(p.z-hw+e),z1=(int)std::floor(p.z+hw-e);
 for(int y=y0;y<=y1;y++)for(int z=z0;z<=z1;z++)for(int x=x0;x<=x1;x++){const uint16_t id=world_.get(x,y,z);if(!id)continue;const BlockInfo&bi=R.info(id);if(!bi.solid)continue; if(bi.shape&&bi.shape!=255){ for(const Box&bx:R.boxes(bi.shape)){ const float bx0=float(x)+bx.x0/16.f,bx1=float(x)+bx.x1/16.f,by0=float(y)+bx.y0/16.f,by1=float(y)+bx.y1/16.f,bz0=float(z)+bx.z0/16.f,bz1=float(z)+bx.z1/16.f; if(p.x+hw>bx0+e&&p.x-hw<bx1-e&&p.y+hh>by0+e&&p.y<by1-e&&p.z+hw>bz0+e&&p.z-hw<bz1-e)return true; } continue; } if(bi.height16>=16||p.y<float(y)+float(bi.height16)/16.f-e)return true;}
 return false;
}
bool Game::moveAxis(int axis,float delta){
 if(delta==0.f)return false;
 float*c=axis==0?&pos_.x:(axis==1?&pos_.y:&pos_.z); const float old=*c; *c=old+delta;
 if(!collides(pos_))return false;
 float lo=0.f,hi=1.f; for(int i=0;i<10;i++){const float mid=(lo+hi)*0.5f;*c=old+delta*mid;if(collides(pos_))hi=mid;else lo=mid;}
 *c=old+delta*lo; return true;
}
void Game::movePlayer(float dt){
 // walking into a block no higher than 0.6 (carpet, slab, stair step) climbs it without jumping
 auto tryStep=[&](int axis,float amt)->bool{ if(!onGround_||vel_.y>0.01f||amt==0.f)return false; Vec3 t=pos_; t.y+=0.6f; if(axis==0)t.x+=amt; else t.z+=amt; if(collides(t))return false; float lo=0.f,hi=0.6f; for(int i=0;i<10;i++){ const float mid=(lo+hi)*0.5f; Vec3 u=t; u.y=pos_.y+mid; if(collides(u))lo=mid; else hi=mid; } t.y=pos_.y+hi; pos_=t; return true; };
 Vec3 d=vel_*dt; const float m=std::max(std::fabs(d.x),std::max(std::fabs(d.y),std::fabs(d.z))); const int steps=std::max(1,(int)std::ceil(m/0.2f)); d=d*(1.0f/float(steps));
 for(int s=0;s<steps;s++){
  { const Vec3 o=pos_; if(moveAxis(0,d.x)&&!tryStep(0,d.x))vel_.x=0.f; if(sneakEdge_&&!collides(Vec3{pos_.x,pos_.y-0.05f,pos_.z})){pos_=o;vel_.x=0.f;} }   // sneaking: never walk off an edge
  { const Vec3 o=pos_; if(moveAxis(2,d.z)&&!tryStep(2,d.z))vel_.z=0.f; if(sneakEdge_&&!collides(Vec3{pos_.x,pos_.y-0.05f,pos_.z})){pos_=o;vel_.z=0.f;} }
  if(moveAxis(1,d.y)){vel_.y=0.f;d.y=0.f;}
 }
 onGround_=vel_.y<=0.f&&collides(Vec3{pos_.x,pos_.y-0.02f,pos_.z});
 if(pos_.y<-30.f){pos_=world_.findSpawn();while(collides(pos_)&&pos_.y<120)pos_.y+=1;vel_=Vec3{};}
}

Game::Hit Game::raycast(const Vec3&o,const Vec3&d,float maxDist,bool fluids)const{
 Hit h; const Registry&R=Registry::get();
 int x=(int)std::floor(o.x),y=(int)std::floor(o.y),z=(int)std::floor(o.z);
 const int sx=d.x>0?1:-1,sy=d.y>0?1:-1,sz=d.z>0?1:-1; const float inf=1e30f;
 const float tdx=d.x!=0?std::fabs(1.f/d.x):inf,tdy=d.y!=0?std::fabs(1.f/d.y):inf,tdz=d.z!=0?std::fabs(1.f/d.z):inf;
 float tx=d.x!=0?((sx>0?float(x+1)-o.x:o.x-float(x))*tdx):inf,ty=d.y!=0?((sy>0?float(y+1)-o.y:o.y-float(y))*tdy):inf,tz=d.z!=0?((sz>0?float(z+1)-o.z:o.z-float(z))*tdz):inf;
 int nx=0,ny=0,nz=0; float t=0;
 for(int i=0;i<256;i++){
  const uint16_t id=world_.get(x,y,z);
  if(id&&(fluids||!R.info(id).fluid)){h.hit=true;h.x=x;h.y=y;h.z=z;h.nx=nx;h.ny=ny;h.nz=nz;h.hy=std::clamp(o.y+d.y*t-float(y),0.f,1.f);return h;}
  if(tx<=ty&&tx<=tz){t=tx;tx+=tdx;x+=sx;nx=-sx;ny=0;nz=0;} else if(ty<=tz){t=ty;ty+=tdy;y+=sy;nx=0;ny=-sy;nz=0;} else {t=tz;tz+=tdz;z+=sz;nx=0;ny=0;nz=-sz;}
  if(t>maxDist)break;
 }
 return h;
}

void Game::markDirtyAround(int,int,int n,int out[3][2]){ for(int i=0;i<n;i++)streamer_->markDirty(out[i][0],out[i][1]); }
// right click with a non-block item: buckets and armour
bool Game::useItem(){
 const Registry&R=Registry::get(); InvSlot&h=cur_.inv.s[cur_.inv.selected]; const std::string n=R.itemName(h.id); const bool creative=cur_.mode==MODE_CREATIVE;
 const int as=R.armorSlot(h.id);
 if(as>=0){ std::swap(h,cur_.inv.armor[as]); markWorldDirty(); return true; }    // wear it (swaps with what is worn)
 const bool empty=n=="bucket",water=n=="water_bucket",lava=n=="lava_bucket"; if(!empty&&!water&&!lava)return false;
 const float cy=std::cos(yaw_),sy=std::sin(yaw_),cp=std::cos(pitch_),sp=std::sin(pitch_); const Vec3 eye=pos_+Vec3{0.f,eyeH_,0.f},dir{sy*cp,sp,-cy*cp};
 const Hit f=raycast(eye,dir,creative?6.f:5.f,true); if(!f.hit)return true;
 int out[3][2];
 if(empty){
  const uint16_t b=world_.get(f.x,f.y,f.z); if(!b||!R.info(b).fluid)return true;
  const ItemId filled=R.itemByName(b==lavaId_?"lava_bucket":"water_bucket");
  const int cnt=world_.set(f.x,f.y,f.z,0,out); markDirtyAround(f.x,f.z,cnt,out);
  if(!creative&&filled){ if(h.count<=1){h.id=filled;h.count=1;} else {h.count--; if(cur_.inv.add(filled,1)>0){} } }   // the bucket is now full (creative keeps the empty one)
  markWorldDirty(); return true;
 }
 int px=f.x,py=f.y,pz=f.z; const uint16_t b=world_.get(px,py,pz);
 if(!(b&&R.info(b).fluid)){px+=f.nx;py+=f.ny;pz+=f.nz;}
 if(py<0||py>=Chunk::HEIGHT)return true; const uint16_t ex=world_.get(px,py,pz); if(ex&&!R.info(ex).fluid)return true;
 const uint16_t liquid=water?waterId_:lavaId_; if(!liquid)return true;
 const int cnt=world_.set(px,py,pz,liquid,out); markDirtyAround(px,pz,cnt,out);
 if(!creative){ h.id=R.itemByName("bucket"); h.count=1; }                                                  // empty again (creative keeps the full one)
 markWorldDirty(); return true;
}
// food value (hunger points) and saturation of a food item; false = not food
static bool foodValue(const std::string&n,int&food,float&sat){
 static const struct{const char*n;int f;float s;} t[]={{"apple",4,2.4f},{"bread",5,6.f},{"cooked_beef",8,12.8f},{"cooked_porkchop",8,12.8f},{"cooked_chicken",6,7.2f},{"cooked_mutton",6,9.6f},{"cooked_rabbit",5,6.f},{"cooked_salmon",6,9.6f},{"cooked_cod",5,6.f},{"baked_potato",5,6.f},{"carrot",3,3.6f},{"golden_carrot",6,14.4f},{"melon_slice",2,1.2f},{"cookie",2,0.4f},{"beef",3,1.8f},{"porkchop",3,1.8f},{"chicken",2,1.2f},{"mutton",2,1.2f},{"rabbit",3,1.8f},{"cod",2,0.4f},{"salmon",2,0.4f},{"tropical_fish",1,0.2f},{"potato",1,0.6f},{"poisonous_potato",2,1.2f},{"beetroot",1,1.2f},{"sweet_berries",2,0.4f},{"glow_berries",2,0.4f},{"dried_kelp",1,0.6f},{"pumpkin_pie",8,4.8f},{"rotten_flesh",4,0.8f},{"spider_eye",2,3.2f},{"golden_apple",4,9.6f},{"enchanted_golden_apple",4,9.6f},{"mushroom_stew",6,7.2f},{"rabbit_stew",10,12.f},{"beetroot_soup",6,7.2f},{"suspicious_stew",6,7.2f},{"honey_bottle",6,1.2f},{"chorus_fruit",4,2.4f}};
 for(auto&e:t)if(n==e.n){food=e.f;sat=e.s;return true;} return false;
}
void Game::hungerTick(float dt,double now,bool sprinting,bool jumped){
 const float dx=pos_.x-lastPos_.x,dz=pos_.z-lastPos_.z,dist=std::sqrt(dx*dx+dz*dz); lastPos_=pos_;
 if(dist<2.f&&sprinting)exh_+=0.1f*dist;
 if(jumped)exh_+=sprinting?0.2f:0.05f;
 while(exh_>=4.f){ exh_-=4.f; if(sat_>0.f)sat_=std::max(0.f,sat_-1.f); else food_=std::max(0,food_-1); }
 if(health_>0.f&&health_<20.f&&food_>=18){ healAcc_+=dt; const bool fast=sat_>0.f&&food_>=20; if(healAcc_>=(fast?0.5f:4.f)){ healAcc_=0.f; health_=std::min(20.f,health_+1.f); exh_+=fast?1.f:6.f; } } else healAcc_=0.f;
 if(food_<=0){ starveAcc_+=dt; if(starveAcc_>=4.f){ starveAcc_=0.f; if(health_>1.f)damage(1.f,now,false); } } else starveAcc_=0.f;
}
void Game::damage(float amt,double now,bool armor){
 if(cur_.mode!=MODE_SURVIVAL||dead_||amt<=0.f)return;
 if(armor){ const float a=float(armorTotal(cur_.inv)); amt*=1.f-std::min(20.f,std::max(a/5.f,a-amt/2.f))/25.f; }
 health_-=amt; exh_+=0.1f; (void)now; if(health_<=0.f){health_=0.f;die();}
}
void Game::die(){ dead_=true; screen_=Screen::Death; captureMouse(false); chat_.close(); }
void Game::respawn(){
 pos_=world_.findSpawn(); while(collides(pos_)&&pos_.y<Chunk::HEIGHT-8)pos_.y+=1; vel_=Vec3{}; health_=20.f; fallPeak_=pos_.y; dead_=false; flying_=false; screen_=Screen::Play; captureMouse(true); clickGuard_=true;
}
void Game::breakBlock(bool drop){
 if(!hit_.hit)return; const bool creative=cur_.mode==MODE_CREATIVE;
 const uint16_t id=world_.get(hit_.x,hit_.y,hit_.z); if(!id)return;
 if(!Registry::get().info(id).breakable&&!creative)return;
 int out[3][2]; const int n=world_.set(hit_.x,hit_.y,hit_.z,0,out); markDirtyAround(hit_.x,hit_.z,n,out);
 if(!creative&&drop){ const ItemId d=Registry::get().dropFor(id); if(d)cur_.inv.add(d,1); } if(!creative)exh_+=0.005f;
 markWorldDirty();
}
void Game::placeBlock(){
 if(cur_.mode==MODE_SPECTATOR)return;
 if(!cur_.inv.held().empty()&&cur_.inv.held().id>=kItemBase&&useItem())return;
 if(!hit_.hit||(hit_.nx==0&&hit_.ny==0&&hit_.nz==0))return;
 const InvSlot&h=cur_.inv.held(); if(h.empty()||h.id>=kItemBase)return;
 const Registry&R=Registry::get(); int out[3][2];
 // a slab placed on the open face of a matching slab makes the full block
 { const uint16_t tgt=world_.get(hit_.x,hit_.y,hit_.z); const int hk=R.slabKind((BlockId)h.id),tk=R.slabKind(tgt);
   if(hk&&tk&&R.baseOf(tgt)==R.baseOf((BlockId)h.id)&&((tk==1&&hit_.ny==1)||(tk==2&&hit_.ny==-1))){
    const int n=world_.set(hit_.x,hit_.y,hit_.z,R.materialOf(R.baseOf(tgt)),out); markDirtyAround(hit_.x,hit_.z,n,out);
    if(cur_.mode!=MODE_CREATIVE)cur_.inv.consumeHeld(); markWorldDirty(); return; } }
 const int px=hit_.x+hit_.nx,py=hit_.y+hit_.ny,pz=hit_.z+hit_.nz; if(py<0||py>=Chunk::HEIGHT)return;
 const uint16_t ex=world_.get(px,py,pz); if(ex&&!R.info(ex).fluid&&R.info(ex).shape!=255)return;   // water / flowers / grass can be built over
 if(R.info(h.id).solid){      // do not place a block inside the player
  const bool ox=pos_.x+0.3f>float(px)&&pos_.x-0.3f<float(px+1),oy=pos_.y+1.8f>float(py)&&pos_.y<float(py+1),oz=pos_.z+0.3f>float(pz)&&pos_.z-0.3f<float(pz+1);
  if(ox&&oy&&oz)return;
 }
 const BlockId id=R.placed((BlockId)h.id,hit_.nx,hit_.ny,hit_.nz,yaw_,pitch_,hit_.hy);   // logs follow the clicked face, furnaces face you, slabs use the half you clicked ...
 const int n=world_.set(px,py,pz,id,out); markDirtyAround(px,pz,n,out);                   // (re-mesh the changed chunks)
 if(cur_.mode!=MODE_CREATIVE)cur_.inv.consumeHeld();
 markWorldDirty();
}
void Game::pickBlock(){
 if(!hit_.hit||cur_.mode!=MODE_CREATIVE)return; const uint16_t id=Registry::get().baseOf(world_.get(hit_.x,hit_.y,hit_.z)); if(!id)return;
 for(int i=0;i<9;i++)if(cur_.inv.s[i].id==id&&!cur_.inv.s[i].empty()){cur_.inv.selected=i;return;}
 InvSlot&s=cur_.inv.s[cur_.inv.selected]; s.id=id; s.count=Registry::get().maxStack(id); markWorldDirty();
}

Mat4 Game::projection(float aspect)const{
 const float far=std::max(1024.f,float(settings_.renderDistance)*16.f*1.5f+128.f);
 const float fov=float(settings_.fov)*kPi/180.f;
 return renderer_?renderer_->projection(fov,aspect,0.05f,far):perspectiveRev(fov,aspect,0.05f,far,true);
}
Mat4 Game::viewMatrix()const{
 const float cy=std::cos(yaw_),sy=std::sin(yaw_),cp=std::cos(pitch_),sp=std::sin(pitch_);
 return lookAt(Vec3{0,0,0},Vec3{sy*cp,sp,-cy*cp},Vec3{0,1,0});
}

void Game::updatePlay(float dt,const UiInput&in){
 const double now=in.time; const bool creative=cur_.mode==MODE_CREATIVE,spectator=cur_.mode==MODE_SPECTATOR;
 if(tickRate_>0&&doDaylight_){ timeAcc_+=double(dt)*tickRate_; while(timeAcc_>=1.0){worldTime_++;timeAcc_-=1.0;} }
 if(keyPressed(settings_.keys[ACT_INVENTORY])&&!invJustClosed_&&cur_.mode!=MODE_SPECTATOR){openInventory();return;}
 for(int i=0;i<9;i++)if(keyPressed(GLFW_KEY_1+i))cur_.inv.selected=i;
 if(in.wheel!=0.f){ const int step=in.wheel>0?-1:1; cur_.inv.selected=((cur_.inv.selected+step)%9+9)%9; }
 if(cur_.inv.selected!=lastSelected_){ lastSelected_=cur_.inv.selected; itemNameUntil_=now+2.0; }
 if(spectator)flying_=true; else if(!creative)flying_=false;
 else{
  if(keyPressed(settings_.keys[ACT_JUMP])){ if(now-lastJumpPress_<0.3){flying_=!flying_;vel_=Vec3{};} lastJumpPress_=now; }
 }
 double mx=0,my=0; glfwGetCursorPos(win_,&mx,&my); if(!mouseInit_){lastMX_=mx;lastMY_=my;mouseInit_=true;}
 const float k=0.0005f+settings_.mouseSens*0.004f;
 yaw_+=float(mx-lastMX_)*k; pitch_-=float(my-lastMY_)*k; pitch_=std::clamp(pitch_,-1.55f,1.55f); lastMX_=mx; lastMY_=my;
 auto kd=[&](int a){return keyDown(settings_.keys[a]);};
 const Vec3 f{std::sin(yaw_),0.f,-std::cos(yaw_)},r{std::cos(yaw_),0.f,std::sin(yaw_)}; Vec3 wish{};
 if(kd(ACT_FWD))wish=wish+f; if(kd(ACT_BACK))wish=wish-f; if(kd(ACT_RIGHT))wish=wish+r; if(kd(ACT_LEFT))wish=wish-r;
 wish=normalize(wish); const bool sprint=kd(ACT_SPRINT)&&(cur_.mode!=MODE_SURVIVAL||food_>6); const Registry&R=Registry::get();   // too hungry to sprint
 if(!waterId_){waterId_=(uint16_t)R.blockByName("water");lavaId_=(uint16_t)R.blockByName("lava");}
 const bool sneak=kd(ACT_DOWN)&&!flying_; sneakEdge_=sneak&&onGround_; eyeH_+=((sneak?1.27f:1.62f)-eyeH_)*std::min(1.f,dt*14.f); inFluidNow_=false;
 if(flying_){
  const float sp=sprint?12.f:4.3f,spv=sprint?12.f:6.f;   // flying is walking speed until you sprint
  vel_.x=wish.x*sp;vel_.z=wish.z*sp; vel_.y=((kd(ACT_JUMP)?1.f:0.f)-(kd(ACT_DOWN)?1.f:0.f))*spv;
 }else{
  // water check: eye level AND feet separately (head-in vs feet-in)
  const uint16_t atFeet=world_.get((int)std::floor(pos_.x),(int)std::floor(pos_.y+0.1f),(int)std::floor(pos_.z));
  const uint16_t atEye =world_.get((int)std::floor(pos_.x),(int)std::floor(pos_.y+1.4f),(int)std::floor(pos_.z));
  const bool inWater=atFeet&&R.info(atFeet).fluid;
  const bool headWater=atEye&&R.info(atEye).fluid;
  inFluidNow_=inWater;
  if(inWater&&atFeet==lavaId_&&cur_.mode==MODE_SURVIVAL&&now-lavaTick_>0.5){damage(4.f,now,false);lavaTick_=now;}   // lava burns
  const float sp=(sprint&&!sneak?5.6f:4.3f)*(inWater?0.55f:1.f)*(sneak?0.3f:1.f); vel_.x=wish.x*sp;vel_.z=wish.z*sp;
  if(inWater){
   // buoyancy: push up toward surface slowly
   vel_.y-=9.f*dt;
   if(headWater) vel_.y=std::max(vel_.y,-2.f);        // terminal sink speed in deep water
   else          vel_.y=std::min(vel_.y+5.f*dt,2.5f); // near surface: float up
   // swim/jump: space pressed swims up; at very top of water (feet in, head not) jump out
   if(kd(ACT_JUMP)){
    if(!headWater&&vel_.y<3.5f) vel_.y+=15.f*dt;     // scull to surface
    else vel_.y=std::min(vel_.y+20.f*dt,3.8f);        // swim up through water
   }
   if(kd(ACT_DOWN)) vel_.y=std::max(vel_.y-10.f*dt,-3.f);
   if(kd(ACT_JUMP)&&wish.x*wish.x+wish.z*wish.z>0.01f){      // swimming into a shore: hop out onto land
    const Vec3 ahead{pos_.x+wish.x*0.35f,pos_.y,pos_.z+wish.z*0.35f};
    if(collides(ahead)&&!collides(Vec3{ahead.x,ahead.y+1.05f,ahead.z}))vel_.y=std::max(vel_.y,9.f);
   }
  } else{
   if(onGround_&&vel_.y<0.f)vel_.y=0.f;
   vel_.y-=32.f*dt; vel_.y=std::max(vel_.y,-70.f);
   if(onGround_&&kd(ACT_JUMP))vel_.y=9.f;
  }
 }
 if(spectator){pos_.x+=vel_.x*dt;pos_.y+=vel_.y*dt;pos_.z+=vel_.z*dt;onGround_=false;} else movePlayer(dt);   // spectators fly through blocks
 if(cur_.mode==MODE_SURVIVAL){   // fall damage + natural regeneration
  if(flying_||inFluidNow_)fallPeak_=pos_.y; else if(onGround_){ if(!wasGround_){const float dist=fallPeak_-pos_.y; if(dist>3.f)damage(std::ceil(dist-3.f),now);} fallPeak_=pos_.y; } else fallPeak_=std::max(fallPeak_,pos_.y);
  hungerTick(dt,now,kd(ACT_SPRINT)&&onGround_&&food_>6&&(vel_.x*vel_.x+vel_.z*vel_.z)>1.f,wasGround_&&!onGround_&&vel_.y>3.f);
  wasGround_=onGround_;
 } else {health_=20.f;food_=20;sat_=5.f;exh_=0.f;fallPeak_=pos_.y;lastPos_=pos_;}
 // block interaction
 const float cy=std::cos(yaw_),sy=std::sin(yaw_),cp=std::cos(pitch_),sp2=std::sin(pitch_);
 const Vec3 eye=pos_+Vec3{0.f,eyeH_,0.f},dir{sy*cp,sp2,-cy*cp};
 hit_=raycast(eye,dir,creative?6.f:5.f); if(spectator)hit_=Hit{};
 const bool ldown=mousePrev_[0],rdown=mousePrev_[1];
 if(!ldown&&!rdown)clickGuard_=false; const bool armed=!clickGuard_;      // ignore the click that closed a menu / pressed 'Back to Game'
 if(armed){
  if(creative){ if(ldown){ breakCd_-=dt; if(in.lclick||breakCd_<=0.f){breakBlock();breakCd_=0.16f;} } else breakCd_=0.f; }
  else if(ldown&&hit_.hit){   // survival: vanilla break times (hardness, tool class / tier / speed)
   if(hit_.x!=breakX_||hit_.y!=breakY_||hit_.z!=breakZ_){breakX_=hit_.x;breakY_=hit_.y;breakZ_=hit_.z;breakProg_=0.f;}
   if(now>=breakDelay_){
    const uint16_t bid=world_.get(hit_.x,hit_.y,hit_.z); const InvSlot&hs=cur_.inv.held(); const MineInfo mi=mineInfo(Registry::get(),bid,hs.empty()?ItemId(0):hs.id);
    if(!mi.unbreakable){ breakProg_+=dt/std::max(0.001f,mi.seconds*(onGround_?1.f:5.f)*(inFluidNow_?5.f:1.f)); if(breakProg_>=1.f||mi.seconds<=0.f){breakBlock(mi.canHarvest);breakProg_=0.f;breakDelay_=now+0.25;} }
   }
  } else breakProg_=0.f;
  { // eating: hold the right mouse button with food in hand (1.6 s)
   const InvSlot&hs=cur_.inv.held(); int fv=0; float fsat=0.f; bool eating=false;
   if(rdown&&!hs.empty()&&hs.id>=kItemBase&&cur_.mode==MODE_SURVIVAL&&foodValue(Registry::get().itemName(hs.id),fv,fsat)&&(food_<20||hs.id==Registry::get().itemByName("golden_apple"))){
    eating=true; eatProg_+=dt;
    if(eatProg_>=1.6f){ eatProg_=0.f; food_=std::min(20,food_+fv); sat_=std::min(float(food_),sat_+fsat); cur_.inv.consumeHeld(); markWorldDirty(); }
   }
   if(!eating)eatProg_=0.f; }
  if(rdown){ placeCd_-=dt; if(in.rclick||placeCd_<=0.f){placeBlock();placeCd_=0.2f;} } else placeCd_=0.f;
  if(in.mclick)pickBlock();
 }
}

// ------------------------------------------------------------------------------------------------ world lifecycle
void Game::startLoading(LoadTask t,const WorldInfo*w){
 loadTask_=t; loadStep_=0; loadProgress_=0; screen_=Screen::Loading; loadStart_=glfwGetTime();
 if(t==LoadTask::EnterWorld&&w){pendingWorld_=*w;loadMsg_="Loading world...";}
 else {loadMsg_="Reloading textures...";loadReturn_=Screen::Options;}
}
void Game::enterWorld(const WorldInfo&w){
 health_=w.health>0.f?w.health:20.f; food_=std::clamp(w.food,0,20); sat_=w.sat; exh_=w.exh; healAcc_=starveAcc_=eatProg_=0.f; dead_=false; fallPeak_=0.f; wasGround_=true; timeAcc_=0;
 world_.setDebug(w.debug);
 waitSave(); streamer_->reset(); world_.reset(w.seed);
 store_=std::make_shared<ChunkStore>(saves_.regionDir(w.folder)); world_.setLoader([st=store_](int x,int z,Chunk&c){return st->load(x,z,c);});   // chunks are read from the save one at a time
 cur_=w; const bool creative=w.mode==MODE_CREATIVE;
 if(w.hasPlayer){pos_=Vec3{w.px,w.py,w.pz};yaw_=w.yaw;pitch_=w.pitch;flying_=(w.flying&&creative)||w.mode==MODE_SPECTATOR;}
 else{pos_=world_.findSpawn();while(collides(pos_)&&pos_.y<Chunk::HEIGHT-8)pos_.y+=1;yaw_=0;pitch_=0;flying_=false;}
 vel_=Vec3{}; inWorld_=true; carried_.clear(); lastSelected_=-1; itemNameUntil_=0; lastSave_=glfwGetTime(); hit_=Hit{};
}
// Saves level.dat + only the chunks that changed.  Runs on a background thread unless `sync` (leaving the world / quitting).
void Game::waitSave(){ if(saveFut_.valid())saveFut_.get(); }
void Game::saveWorld(bool sync){
 if(!inWorld_)return;
 cur_.health=health_;cur_.food=food_;cur_.sat=sat_;cur_.exh=exh_; cur_.px=pos_.x;cur_.py=pos_.y;cur_.pz=pos_.z;cur_.yaw=yaw_;cur_.pitch=pitch_;cur_.flying=flying_;cur_.hasPlayer=true;
 cur_.lastPlayed=(int64_t)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
 if(saveFut_.valid()){ if(!sync&&saveFut_.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return; waitSave(); }   // never two saves at once
 auto dirty=world_.dirtyChunks(); WorldInfo info=cur_; auto store=store_; World*w=&world_; const SaveManager*sm=&saves_;
 auto job=[dirty,info,store,w,sm]{ PerfScope ps("save (level.dat + chunks)",1500); sm->saveLevel(info); if(store&&!dirty.empty()&&store->save(dirty))w->markSaved(dirty); };
 if(sync)job(); else saveFut_=std::async(std::launch::async,job);
 lastSave_=glfwGetTime(); worldDirty_=false;
}
void Game::leaveWorld(){
 saveWorld(true); streamer_->reset(); world_.reset(0); inWorld_=false; screen_=Screen::Title; captureMouse(false); carried_.clear();
}
void Game::openPause(){ screen_=Screen::Pause; captureMouse(false); saveWorld(); }
void Game::closePause(){ screen_=Screen::Play; captureMouse(true); }
void Game::openInventory(){ carried_.clear(); screen_=cur_.mode==MODE_CREATIVE?Screen::Creative:Screen::Inventory; captureMouse(false); creaDrag_=false; }
void Game::closeInventory(){
 if(!carried_.empty()&&cur_.mode!=MODE_CREATIVE)cur_.inv.add(carried_.id,carried_.count);
 carried_.clear(); screen_=Screen::Play; captureMouse(true); markWorldDirty();   // inventory changes are saved
}

void Game::updateLoading(){
 const double now=glfwGetTime();
 if(loadTask_==LoadTask::ReloadPack){
  if(loadStep_==0){loadStep_=1;return;}              // draw the loading screen once before blocking
  applyResourcePack(); loadTask_=LoadTask::None; screen_=loadReturn_; return;
 }
 if(loadTask_==LoadTask::EnterWorld){
  if(loadStep_==0){loadStep_=1;return;}
  if(loadStep_==1){enterWorld(pendingWorld_);loadStep_=2;loadStart_=now;loadMsg_="Building terrain...";return;}
  const int pcx=floorDiv16(pos_.x),pcz=floorDiv16(pos_.z),R=std::min(settings_.renderDistance,3);
  int need=0,have=0; for(int dz=-R;dz<=R;dz++)for(int dx=-R;dx<=R;dx++)if(chunkInRange(dx,dz,R)){need++;if(streamer_->isMeshed(pcx+dx,pcz+dz))have++;}
  loadProgress_=need?float(have)/float(need):1.f;
  if(have>=need||now-loadStart_>20.0){ loadTask_=LoadTask::None; screen_=Screen::Play; captureMouse(true); }
 }
}

// ------------------------------------------------------------------------------------------------ panorama (6 cube faces projected on the CPU, slowly rotating)
// The top/bottom faces are matched against the edge of the front face so they are never rotated wrongly.
void Game::computePanoRot(){
 panoRotDone_=true; panoRot_[0]=panoRot_[1]=0; if(!pano_.valid())return;
 const int S=(int)pano_.size; if(S<8)return;
 auto px=[&](int face,float x,float y)->const uint8_t*{ const int ix=std::clamp((int)std::lround(x),0,S-1),iy=std::clamp((int)std::lround(y),0,S-1); return &pano_.rgba[(((size_t)face*S+iy)*S+ix)*4]; };
 for(int w=0;w<2;w++){
  const int face=4+w; const float ey=w?-1.f:1.f; const Vec3 c4{0,w?-1.f:1.f,0};
  Vec3 u{-1,0,0},v=w?Vec3{0,0,1}:Vec3{0,0,-1}; long long best=-1; int bk=0;
  for(int k=0;k<4;k++){
   long long sum=0;
   for(int i=0;i<=32;i++){
    const float s=-1.f+2.f*float(i)/32.f; const Vec3 P{-s,ey,1.f},d=P-c4;
    const uint8_t*a=px(0,(s+1.f)*0.5f*float(S-1),w?float(S-1):0.f);
    const uint8_t*b=px(face,(dot(d,u)+1.f)*0.5f*float(S-1),(1.f-dot(d,v))*0.5f*float(S-1));
    for(int ch=0;ch<3;ch++)sum+=std::abs((int)a[ch]-(int)b[ch]);
   }
   if(best<0||sum<best){best=sum;bk=k;}
   u=Vec3{u.z,u.y,-u.x}; v=Vec3{v.z,v.y,-v.x};
  }
  panoRot_[w]=bk;
 }
}
void Game::buildPanorama(float W,float H,double t){
 panoVerts_.clear(); if(!pano_.valid())return;
 const float yaw=float(t)*0.035f,pitch=0.22f*std::sin(float(t)*0.11f),fov=85.f*kPi/180.f,focal=(H*0.5f)/std::tan(fov*0.5f);
 const float cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch);
 // MC face order: 0=south(+Z) 1=west(-X) 2=north(-Z) 3=east(+X) 4=up(+Y) 5=down(-Y).  The up/down faces are rotated to whatever fits the side faces (computePanoRot).
 if(!panoRotDone_)computePanoRot();
 static const Vec3 C[6]={{ 0,0,1},{-1,0,0},{0,0,-1},{1,0,0},{0,1,0},{0,-1,0}};
 Vec3 U[6]={{-1,0,0},{0,0,-1},{1,0,0},{0,0,1},{-1,0,0},{-1,0,0}};
 Vec3 V[6]={{0,1,0},{0,1,0},{0,1,0},{0,1,0},{0,0,-1},{0,0,1}};
 for(int w=0;w<2;w++)for(int k=0;k<panoRot_[w];k++){ Vec3&u=U[4+w],&v=V[4+w]; u=Vec3{u.z,u.y,-u.x}; v=Vec3{v.z,v.y,-v.x}; }
 const Vec3 f{sy*cp,sp,-cy*cp},r{cy,0.f,sy},up_vec{-sy*sp,cp,cy*sp};
 const int N=16; const float S=float(pano_.size); const uint32_t white=0xFFFFFFFFu;
 struct P{float x,y,u,v;bool ok;};
 for(int face=0;face<6;face++){
  P grid[N+1][N+1];
  for(int j=0;j<=N;j++)for(int i=0;i<=N;i++){
   const float s=-1.f+2.f*float(i)/float(N),tt=1.f-2.f*float(j)/float(N);
   const Vec3 p=C[face]+U[face]*s+V[face]*tt; const float z=dot(p,f);
   P&g=grid[j][i]; g.ok=z>0.05f; if(!g.ok)continue;
   g.x=W*0.5f+dot(p,r)/z*focal; g.y=H*0.5f-dot(p,up_vec)/z*focal;
   g.u=(((s+1.f)*0.5f)*(S-1.f)+0.5f)/S; g.v=((((1.f-tt)*0.5f)*(S-1.f)+0.5f)+float(face)*S)/(6.f*S);
  }
  for(int j=0;j<N;j++)for(int i=0;i<N;i++){
   const P&a=grid[j][i],&b=grid[j][i+1],&c=grid[j+1][i+1],&d=grid[j+1][i]; if(!a.ok||!b.ok||!c.ok||!d.ok)continue;
   const float minx=std::min(std::min(a.x,b.x),std::min(c.x,d.x)),maxx=std::max(std::max(a.x,b.x),std::max(c.x,d.x)),miny=std::min(std::min(a.y,b.y),std::min(c.y,d.y)),maxy=std::max(std::max(a.y,b.y),std::max(c.y,d.y));
   if(maxx<0||minx>W||maxy<0||miny>H)continue;
   panoVerts_.push_back({a.x,a.y,a.u,a.v,white});panoVerts_.push_back({b.x,b.y,b.u,b.v,white});panoVerts_.push_back({c.x,c.y,c.u,c.v,white});panoVerts_.push_back({d.x,d.y,d.u,d.v,white});
  }
 }
}

// ------------------------------------------------------------------------------------------------ frame
void Game::frame(){
 const double now=glfwGetTime(); const float dtRaw=float(now-lastTime_); lastTime_=now; time_=now; const float dt=std::min(0.05f,dtRaw);
 fpsAcc_+=dtRaw; fpsFrames_++; if(fpsAcc_>=0.5){fpsShown_=int(double(fpsFrames_)/fpsAcc_+0.5);fpsAcc_=0;fpsFrames_=0;}
 int fbw=0,fbh=0; glfwGetFramebufferSize(win_,&fbw,&fbh);
 if(fbw<=0||fbh<=0){glfwWaitEventsTimeout(0.1);return;}                 // minimised
 if(dtRaw>0.15f)std::fprintf(stderr,"[hypervox][SPIKE] previous frame took %.0f ms (see [SLOW] lines above for the cause)\n",dtRaw*1000.f);
 handleDrops(); if(screen_!=Screen::Play||chat_.isOpen())clickGuard_=true;
 const UiInput in=gatherInput(fbw,fbh,now);
 if((screen_==Screen::Inventory||screen_==Screen::Creative)&&!chat_.isOpen()&&keyPressed(settings_.keys[ACT_INVENTORY])){closeInventory();invJustClosed_=true;}   // E closes the inventory again
 if(keyPressed(GLFW_KEY_ESCAPE)){ if(screen_==Screen::Death){} else if(chat_.isOpen()){chat_.close();} else if(rebinding_>=0)rebinding_=-1; else handleEscape(); }
 { const bool f3=keyDown(GLFW_KEY_F3);   // F3 = debug screen, F3+F4 = cycle game mode (survival -> creative -> spectator)
   if(f3&&!f3Down_)f3Used_=false;
   if(f3&&keyPressed(GLFW_KEY_F4)&&inWorld_&&(screen_==Screen::Play||screen_==Screen::Pause)){ cur_.mode=(cur_.mode+1)%3; flying_=cur_.mode==MODE_SPECTATOR; vel_=Vec3{}; const char*nm[3]={"Survival","Creative","Spectator"}; chat_.sys(std::string("Game mode set to ")+nm[cur_.mode],now); markWorldDirty(); f3Used_=true; }
   if(!f3&&f3Down_&&!f3Used_){settings_.showDebug=!settings_.showDebug;settings_.save();}
   f3Down_=f3; }
 if(keyPressed(GLFW_KEY_T)&&screen_==Screen::Play&&!chat_.isOpen()){chat_.open();skipTyped_=true;captureMouse(false);}  
 if(keyPressed(47)&&screen_==Screen::Play&&!chat_.isOpen()){chat_.open();chat_.input()="/";skipTyped_=true;captureMouse(false);}
 if(chat_.isOpen()){
  const std::string before=chat_.input();
  if(!skipTyped_)for(char c:in.typed)chat_.addChar(c);        // the key that opened the chat must not be typed into it
  skipTyped_=false;
  if(in.backspace)chat_.backspace();
  size_t tokStart=0; const auto sug=chatSuggest(tokStart); const int ns=(int)sug.size();
  if(chat_.input()!=before)sugSel_=0;
  if(ns>0){
   if(keyPressed(GLFW_KEY_UP))sugSel_=(sugSel_+ns-1)%ns;
   if(keyPressed(GLFW_KEY_DOWN))sugSel_=(sugSel_+1)%ns;
   if(keyPressed(GLFW_KEY_TAB)){ const std::string pick=sug[std::clamp(sugSel_,0,ns-1)]; chat_.input()=chat_.input().substr(0,tokStart)+pick+(tokStart<=1?" ":""); sugSel_=0; }
  } else {
   if(keyPressed(GLFW_KEY_UP))chat_.histUp();
   if(keyPressed(GLFW_KEY_DOWN))chat_.histDown();
  }
  if(keyPressed(GLFW_KEY_ENTER)){chat_.submit(now);if(!chat_.isOpen()||!glfwGetKey(win_,GLFW_KEY_T)){chat_.close();captureMouse(screen_==Screen::Play);}}
 } else if(screen_==Screen::Loading)updateLoading();
 else if(screen_==Screen::Play)updatePlay(dt,in);

 const Vec3 eye=pos_+Vec3{0.f,eyeH_,0.f};
 const int pcx=floorDiv16(pos_.x),pcz=floorDiv16(pos_.z);
 tickSounds(dt);
 if(inWorld_&&streamer_){
  // only chunks inside (a margin around) the camera's horizontal view cone stay on the GPU
  const float aspect=float(fbw)/float(fbh), halfH=std::atan(std::tan(float(settings_.fov)*kPi/360.f)*aspect);
  ViewCone vc=ViewCone::make(pos_.x,pos_.z,yaw_,halfH,0.35f,0.8f); vc.nearChunks=4; if(std::fabs(pitch_)>1.0f)vc.halfL=vc.halfU=4.f;   // looking steeply up/down: keep everything
  { PerfScope ps("chunk streamer update+pump",25); streamer_->update(pcx,pcz,settings_.renderDistance,vc); streamer_->pump(48); }
  if(renderer_&&screen_!=Screen::Loading){ static std::vector<TileUpdate> ups; ups.clear(); pack_.collectAnimationUpdates(now*20.0,ups); if(!ups.empty())renderer_->updateTiles(ups); }   // water, lava, fire ... (20 ticks per second)
  if((worldDirty_||world_.dirtyCount())&&now-lastSave_>60.0)saveWorld(false);   // autosave: at most once a minute, in the background
 }

 ui_.begin(float(fbw),float(fbh),&uiAtlas_,in,settings_.guiScale);
 panoVerts_.clear(); skyVerts_.clear();
 const bool panoScreen=!inWorld_&&screen_!=Screen::Loading;
 if(panoScreen)buildPanorama(float(fbw),float(fbh),now);
 if(inWorld_&&screen_!=Screen::Loading){ buildSky(float(fbw),float(fbh)); { const float s=std::sin(float(worldTime_%24000)/24000.f*2.f*kPi); const float day=std::clamp((s+0.12f)/0.24f,0.f,1.f); if(day<1.f)ui_.rect(0,0,ui_.W(),ui_.H(),rgba(4,6,22,(uint32_t)((1.f-day)*125.f))); } }   // night dims the world
 buildScreen();
 manageMemory(now);

 FrameParams fp; fp.camera=eye; fp.chunkX=pcx; fp.chunkZ=pcz; fp.renderDist=settings_.renderDistance;
 fp.world=inWorld_&&screen_!=Screen::Loading;
 fp.vp=mul(projection(float(fbw)/float(fbh)),viewMatrix());
 if(screen_==Screen::Play&&hit_.hit){fp.outline=true;fp.ox=hit_.x;fp.oy=hit_.y;fp.oz=hit_.z;}

 frameVerts_.clear(); frameVerts_.insert(frameVerts_.end(),skyVerts_.begin(),skyVerts_.end()); frameVerts_.insert(frameVerts_.end(),panoVerts_.begin(),panoVerts_.end()); frameVerts_.insert(frameVerts_.end(),ui_.verts.begin(),ui_.verts.end());
 if(renderer_){ PerfScope ps("renderer draw + present",60); renderer_->draw(fp,frameVerts_.data(),(uint32_t)(skyVerts_.size()/4),(uint32_t)(panoVerts_.size()/4),(uint32_t)(frameVerts_.size()/4)); }
 invJustClosed_=false; snapshotKeys();
}

void Game::shutdown(){
 settings_.save(); if(inWorld_)saveWorld(true); waitSave();
 settings_.save();
 if(streamer_){streamer_->reset();streamer_.reset();}
}

// ------------------------------------------------------------------------------------------------ sky: gradient, stars, sun, moon (drawn before the world)
void Game::buildSky(float W,float H){
 const float a=float(worldTime_%24000)/24000.f*2.f*kPi, s=std::sin(a);
 const Vec3 sun{std::cos(a),s,0.f};
 const float day=std::clamp((s+0.12f)/0.24f,0.f,1.f),tw=std::max(0.f,1.f-std::fabs(s)/0.22f);
 auto mix=[](float x,float y,float t){return x+(y-x)*t;};
 float zen[3]={mix(8,103,day),mix(10,146,day),mix(28,236,day)},hor[3]={mix(18,188,day),mix(22,212,day),mix(48,252,day)};
 const float sunset[3]={255,150,80}; for(int i=0;i<3;i++)hor[i]=mix(hor[i],sunset[i],tw*0.8f);
 auto col=[](const float*c,uint32_t al=255){return rgba((uint32_t)c[0],(uint32_t)c[1],(uint32_t)c[2],al);};
 const Sprite&wh=uiAtlas_.white(); const float wu=(wh.u0+wh.u1)*0.5f,wv=(wh.v0+wh.v1)*0.5f;
 auto quad=[&](float x0,float y0,uint32_t c0,float x1,float y1,uint32_t c1){ skyVerts_.push_back({x0,y0,wu,wv,c0});skyVerts_.push_back({x1,y0,wu,wv,c0});skyVerts_.push_back({x1,y1,wu,wv,c1});skyVerts_.push_back({x0,y1,wu,wv,c1}); };
 const float cy=std::cos(yaw_),sy=std::sin(yaw_),cp=std::cos(pitch_),sp=std::sin(pitch_);
 const Vec3 f{sy*cp,sp,-cy*cp},r{cy,0.f,sy},u{-sy*sp,cp,cy*sp};
 const float focal=(H*0.5f)/std::tan(float(settings_.fov)*kPi/360.f);
 const float yh=std::clamp(H*0.5f+std::tan(pitch_)*focal,-50.f,H*3.f);
 quad(0,-20,col(zen),W,yh,col(hor));                                  // zenith -> horizon
 if(yh<H+20)quad(0,yh,col(hor),W,H+20,col(hor));
 auto proj=[&](const Vec3&p,float&x,float&y){ const float z=dot(p,f); if(z<=0.05f)return false; x=W*0.5f+dot(p,r)/z*focal; y=H*0.5f-dot(p,u)/z*focal; return true; };
 if(day<0.9f){   // stars (they turn with the sky)
  static std::vector<Vec3> stars; if(stars.empty()){ uint32_t st=12345u; auto rnd=[&]{st=st*1664525u+1013904223u;return float(st>>8)/16777216.f;}; for(int i=0;i<260;i++){const float z=rnd()*2.f-1.f,t=rnd()*2.f*kPi,q=std::sqrt(1.f-z*z);stars.push_back(Vec3{q*std::cos(t),q*std::sin(t),z});} }
  const float ca=std::cos(a),sa=std::sin(a),al=(1.f-day)*255.f;
  for(size_t i=0;i<stars.size();i++){ const Vec3&q=stars[i]; const Vec3 p{q.x*ca-q.y*sa,q.x*sa+q.y*ca,q.z}; float x,y; if(!proj(p,x,y))continue;
   const float sz=(i%5==0)?1.6f:1.0f; const uint32_t c=rgba(255,255,255,(uint32_t)(al*(0.5f+0.5f*float(i%3)/2.f))); quad(x-sz,y-sz,c,x+sz,y+sz,c); }
 }
 auto body=[&](const Vec3&n,float half,const Sprite&sp2,float u0,float v0,float u1,float v1){
  if(!sp2.valid())return; Vec3 pr=normalize(cross(n,Vec3{0,0,1})),pu=cross(pr,n); float px[4],py[4];
  const float sx[4]={-1,1,1,-1},sy2[4]={1,1,-1,-1};
  for(int i=0;i<4;i++){ if(!proj(n+pr*(sx[i]*half)+pu*(sy2[i]*half),px[i],py[i]))return; }
  const float us[4]={u0,u1,u1,u0},vs[4]={v0,v0,v1,v1};
  for(int i=0;i<4;i++)skyVerts_.push_back({px[i],py[i],us[i],vs[i],0xFFFFFFFFu});
 };
 { const Sprite&sn=uiAtlas_.get("environment/celestial/sun"); body(sun,0.30f,sn,sn.u0,sn.v0,sn.u1,sn.v1); }
 { static const char*ph[8]={"full_moon","waning_gibbous","third_quarter","waning_crescent","new_moon","waxing_crescent","first_quarter","waxing_gibbous"};
   const Sprite&mn=uiAtlas_.get(std::string("environment/celestial/moon/")+ph[(worldTime_/24000)%8]); body(Vec3{-sun.x,-sun.y,0.f},0.20f,mn,mn.u0,mn.v0,mn.u1,mn.v1); }
}

void Game::screenDeath(){
 ui_.rect(0,0,ui_.W(),ui_.H(),rgba(90,0,0,140)); const float W=ui_.W(),H=ui_.H();
 ui_.bigText("You Died!",W/2,std::floor(H*0.28f),3.f,COL_WHITE);
 if(ui_.button(std::floor(W/2)-100,std::floor(H*0.5f),200,20,"Respawn"))respawn();
}

// ------------------------------------------------------------------------------------------------ memory limit + cleaner
int Game::rssMB()const{return processRssMB();}
// Gives memory back without stalling a frame: unloads meshes outside the view and shrinks the chunk cache (cheap), and hands free heap pages
// back to the OS on a background thread (that part can take a long time).
void Game::cleanMemory(){
 PerfScope ps("cleanMemory (main thread part)",15);
 if(streamer_)streamer_->trim(); skinCache_.clear();
 static std::atomic<bool> busy{false}; bool exp=false;
 if(busy.compare_exchange_strong(exp,true)) std::thread([]{ releaseFreeMemory(); busy=false; }).detach();
}
void Game::manageMemory(double now){
 if(now-lastMemCheck_<2.0)return; lastMemCheck_=now; rssMB_=rssMB(); const int lim=std::max(256,settings_.ramLimitMB);
 // cleaner modes: when it starts cleaning, how often it cleans routinely, how small the chunk cache gets under pressure
 static const double trigger[3]={0.85,0.70,0.55},calm[3]={0.70,0.55,0.40},routine[3]={120.0,40.0,15.0}; static const int cache[3]={6,5,4};
 const int m=std::clamp(settings_.cleanerMode,0,2);
 if(rssMB_>lim*trigger[m]){
  if(!memPressure_){memPressure_=true; if(streamer_)streamer_->setCacheRadius(cache[m]);}
  if(streamer_)streamer_->setPaused(rssMB_>lim*0.95);
  if(now-lastTrim_>5.0){cleanMemory();lastTrim_=now;}
 } else if(memPressure_&&rssMB_<lim*calm[m]){ memPressure_=false; if(streamer_){streamer_->setCacheRadius(20);streamer_->setPaused(false);} }
 else if(now-lastTrim_>routine[m]){ lastTrim_=now; cleanMemory(); }
}
