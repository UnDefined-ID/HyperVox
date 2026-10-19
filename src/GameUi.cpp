#include "Game.hpp"
#include <algorithm>
#include <cmath>
#include <ctime>

namespace{
const char* kActionNames[ACT_COUNT]={"Forward","Back","Left","Right","Jump / Fly Up","Sneak / Fly Down","Sprint","(unused)","Inventory"};
const char* modeName(int m){return m==MODE_CREATIVE?"Creative":"Survival";}
}

std::string Game::keyLabel(int k)const{
 if(k>=GLFW_KEY_A&&k<=GLFW_KEY_Z)return std::string(1,char('A'+k-GLFW_KEY_A));
 if(k>=GLFW_KEY_0&&k<=GLFW_KEY_9)return std::string(1,char('0'+k-GLFW_KEY_0));
 switch(k){case GLFW_KEY_SPACE:return "Space";case GLFW_KEY_LEFT_SHIFT:return "L-Shift";case GLFW_KEY_RIGHT_SHIFT:return "R-Shift";case GLFW_KEY_LEFT_CONTROL:return "L-Ctrl";case GLFW_KEY_RIGHT_CONTROL:return "R-Ctrl";
  case GLFW_KEY_LEFT_ALT:return "L-Alt";case GLFW_KEY_TAB:return "Tab";case GLFW_KEY_UP:return "Up";case GLFW_KEY_DOWN:return "Down";case GLFW_KEY_LEFT:return "Left";case GLFW_KEY_RIGHT:return "Right";default:break;}
 return "Key "+std::to_string(k);
}

void Game::drawBackground(bool darken){
 if(inWorld_)ui_.rect(0,0,ui_.W(),ui_.H(),rgba(0,0,0,darken?150:110));
 else if(pano_.valid())ui_.rect(0,0,ui_.W(),ui_.H(),rgba(0,0,0,darken?110:40));
 else ui_.tiled(ui_.input().time>=0?uiAtlas_.get("gui/menu_background"):uiAtlas_.white(),0,0,ui_.W(),ui_.H(),32,rgba(90,90,90));
}


// ------------------------------------------------------------------ chat (vanilla look: translucent lines, one fade, command suggestions)
std::vector<std::string> Game::chatSuggest(size_t&tokStart)const{
 std::vector<std::string> out; const std::string&in=chat_.input(); tokStart=0; if(in.empty()||in[0]!='/')return out;
 const size_t sp=in.find_last_of(' '); tokStart=sp==std::string::npos?1:sp+1;
 auto lower=[](std::string s){for(auto&c:s)c=(char)std::tolower((unsigned char)c);return s;};
 const std::string cur=lower(in.substr(tokStart)); const int argIdx=(int)std::count(in.begin(),in.end(),' ');
 std::string cmd; if(argIdx>0){ const size_t e=in.find(' '); cmd=lower(in.substr(1,e-1)); }
 std::vector<std::string> pool; const Registry&R=Registry::get();
 if(argIdx==0)pool={"clear","fly","gamemode","gamerule","gc","give","help","kill","seed","setblock","tick","time","tp"};
 else if(cmd=="gamemode"&&argIdx==1)pool={"survival","creative","spectator"};
 else if(cmd=="gamerule"){ if(argIdx==1)pool={"tickRate","doDaylightCycle","doWeatherCycle"}; else if(argIdx==2)pool={"true","false"}; }
 else if(cmd=="time"){ if(argIdx==1)pool={"set","add","query"}; else if(argIdx==2)pool={"day","noon","sunset","night","midnight"}; }
 else if(cmd=="tick"&&argIdx==1)pool={"freeze","unfreeze","step","rate"};
 else if(cmd=="give"&&argIdx==1)pool=R.allItemNames();
 else if(cmd=="setblock"&&argIdx==4)for(auto&bn:R.blockNames())if(bn.find('@')==std::string::npos&&bn!="missing")pool.push_back(bn);
 for(auto&p:pool){ if(p.empty())continue; if(lower(p).rfind(cur,0)==0&&lower(p)!=cur)out.push_back(p); }
 std::sort(out.begin(),out.end()); if(out.size()>10)out.resize(10);
 return out;
}
void Game::drawChat(){
 const double now=ui_.input().time; const float W=ui_.W(),H=ui_.H(); const bool open=chat_.isOpen();
 const float cw=std::min(W-8.f,320.f),lh=9.f,bottom=H-(open?16.f:36.f)+(open?0.f:0.f); const int maxLines=open?20:10;
 int row=0;
 for(auto& l:chat_.lines()){
  if(row>=maxLines)break;
  float f=1.f;
  if(!open){ const float age=float(now-l.time); f=std::clamp((1.f-age/10.f)*10.f,0.f,1.f); f*=f; if(f<=0.003f)continue; }   // fully visible ~9 s, then one 1 s fade
  // wrap long lines like vanilla
  std::vector<std::string> parts; std::string cur;
  for(char ch:l.text){ cur+=ch; if(ui_.textWidth(cur)>cw-6.f){ const size_t sp=cur.find_last_of(' '); if(sp!=std::string::npos&&sp>0){parts.push_back(cur.substr(0,sp));cur=cur.substr(sp+1);} else {parts.push_back(cur);cur.clear();} } }
  if(!cur.empty()||parts.empty())parts.push_back(cur);
  for(int p=(int)parts.size()-1;p>=0&&row<maxLines;p--,row++){
   const float y=bottom-(row+1)*lh; const uint32_t a=(uint32_t)(f*255.f);
   ui_.rect(2,y,cw+4,lh,rgba(0,0,0,(uint32_t)(f*128.f)));
   ui_.text(parts[p],4,y+1,rgba(255,255,255,a),false);
  }
 }
 if(open){
  const float by=H-14.f; ui_.rect(2,by,W-4,12,rgba(0,0,0,128));
  const std::string in=chat_.input(); ui_.text(in,4,by+2,COL_WHITE,false); const float tw=ui_.textWidth(in);
  size_t tok=0; const auto sug=chatSuggest(tok); const int ns=(int)sug.size(); const int sel=ns?std::clamp(sugSel_,0,ns-1):0;
  if(ns){   // grey completion text + the vanilla suggestion list
   const std::string rest=sug[sel].substr(std::min(sug[sel].size(),in.size()-tok)); ui_.text(rest,4+tw,by+2,rgba(160,160,160),false);
   float mw=0; for(auto&s:sug)mw=std::max(mw,ui_.textWidth(s)); const float x=4+ui_.textWidth(in.substr(0,tok))-1,h=12.f*ns,y=by-h-1;
   ui_.rect(x,y,mw+2,h,rgba(0,0,0,208));
   for(int i=0;i<ns;i++)ui_.text(sug[ns-1-i],x+1,y+12.f*i+2,(ns-1-i)==sel?rgba(255,255,85):rgba(170,170,170),false);
  } else if(fmod(now,1.0)<0.5)ui_.rect(4+tw,by+2,5,1,COL_WHITE);
  if(ns&&fmod(now,1.0)<0.5){}
 }
}

void Game::buildScreen(){
 switch(screen_){
  case Screen::Title: screenTitle(); break;
  case Screen::WorldSelect: screenWorldSelect(); break;
  case Screen::CreateWorld: screenCreateWorld(); break;
  case Screen::ConfirmDelete: screenConfirmDelete(); break;
  case Screen::Options: screenOptions(); break;
  case Screen::Controls: screenControls(); break;
  case Screen::Textures: screenTextures(); break;
  case Screen::Account: screenAccount(); break;
  case Screen::Death: drawHud(); screenDeath(); break;
  case Screen::Loading: screenLoading(); break;
  case Screen::Pause: drawHud(); screenPause(); drawChat(); break;
  case Screen::Inventory: drawHud(); screenInventory(); drawChat(); break;
  case Screen::Creative: drawHud(); screenCreative(); drawChat(); break;
  case Screen::Play: drawHud(); drawChat(); break;
 }
 if(time_<noticeUntil_&&!noticeMsg_.empty())ui_.textCentered(noticeMsg_,ui_.W()/2,ui_.H()-24,rgba(85,255,85));
 if(!errorMsg_.empty()&&screen_!=Screen::Play){ ui_.textCentered(errorMsg_,ui_.W()/2,ui_.H()-12,rgba(255,85,85)); if(ui_.input().lclick)errorMsg_.clear(); }
}

// ------------------------------------------------------------------ HUD
void Game::drawSlot(const InvSlot&s,float x,float y){
 if(s.empty())return; ui_.icon(s.id,x,y);
 if(s.count>1){const std::string c=std::to_string(s.count); ui_.text(c,x+17-ui_.textWidth(c),y+9);}
}
void Game::drawHud(){
 const float W=ui_.W(),H=ui_.H(),cx=std::floor(W/2),cy=std::floor(H/2);
 if(screen_==Screen::Play){   // crosshair
  ui_.rect(cx-5,cy-1,11,3,rgba(0,0,0,150)); ui_.rect(cx-1,cy-5,3,11,rgba(0,0,0,150)); ui_.rect(cx-4,cy,9,1,COL_WHITE); ui_.rect(cx,cy-4,1,9,COL_WHITE);
 }
 const float hx=std::floor((W-182)/2),hy=H-22;
 ui_.sprite(uiAtlas_.get("gui/sprites/hud/hotbar"),hx,hy,182,22);
 for(int i=0;i<9;i++)drawSlot(cur_.inv.s[i],hx+3+20*i,hy+3);
 ui_.sprite(uiAtlas_.get("gui/sprites/hud/hotbar_selection"),hx-1+20*cur_.inv.selected,hy-1,24,23);
 if(cur_.mode==MODE_SURVIVAL&&screen_==Screen::Play&&eatProg_>0.f){ const float bw=20.f,bx=std::floor(ui_.W()/2-bw/2),by=std::floor(ui_.H()/2)+10; ui_.rect(bx-1,by-1,bw+2,4,rgba(0,0,0,170)); ui_.rect(bx,by,bw*std::min(1.f,eatProg_/1.6f),2,rgba(255,200,80)); }
 if(cur_.mode==MODE_SURVIVAL&&screen_==Screen::Play&&breakProg_>0.f){ const float bw=20.f,bx=std::floor(ui_.W()/2-bw/2),by=std::floor(ui_.H()/2)+10; ui_.rect(bx-1,by-1,bw+2,4,rgba(0,0,0,170)); ui_.rect(bx,by,bw*std::min(1.f,breakProg_),2,rgba(235,235,235)); }
 if(cur_.mode==MODE_SURVIVAL&&(screen_==Screen::Play||screen_==Screen::Pause||screen_==Screen::Death)){   // hearts + armour bar
  const Sprite&hc=uiAtlas_.get("gui/sprites/hud/heart/container"),&hf=uiAtlas_.get("gui/sprites/hud/heart/full"),&hh=uiAtlas_.get("gui/sprites/hud/heart/half");
  const float hy2=hy-10; const int hp=(int)std::ceil(health_);
  for(int i=0;i<10;i++){ const float x=hx+i*8; ui_.sprite(hc,x,hy2,9,9); if(hp>=i*2+2)ui_.sprite(hf,x,hy2,9,9); else if(hp==i*2+1)ui_.sprite(hh,x,hy2,9,9); }
  { const Sprite&fe=uiAtlas_.get("gui/sprites/hud/food_empty"),&ff=uiAtlas_.get("gui/sprites/hud/food_full"),&fh=uiAtlas_.get("gui/sprites/hud/food_half");   // hunger: right side, filling from the right
   for(int i=0;i<10;i++){ const float x=hx+172.f-float(i)*8.f; ui_.sprite(fe,x,hy2,9,9); if(food_>=i*2+2)ui_.sprite(ff,x,hy2,9,9); else if(food_==i*2+1)ui_.sprite(fh,x,hy2,9,9); } }
  const int ap=armorTotal(cur_.inv);
  if(ap>0){ const Sprite&ae=uiAtlas_.get("gui/sprites/hud/armor_empty"),&af=uiAtlas_.get("gui/sprites/hud/armor_full"),&ah=uiAtlas_.get("gui/sprites/hud/armor_half");
   for(int i=0;i<10;i++){ const float x=hx+i*8,y=hy2-10; ui_.sprite(ae,x,y,9,9); if(ap>=i*2+2)ui_.sprite(af,x,y,9,9); else if(ap==i*2+1)ui_.sprite(ah,x,y,9,9); } }
 }
 if(screen_==Screen::Play&&time_<itemNameUntil_&&!cur_.inv.held().empty())ui_.textCentered(Registry::get().item(cur_.inv.held().id).display,W/2,hy-16);
 if(settings_.showDebug&&screen_==Screen::Play){
  char b[160]; float y=3;
  auto line=[&](const char*s){ui_.text(s,3,y,COL_WHITE);y+=10;};
  std::snprintf(b,sizeof b,"hypervox  %d fps",fpsShown_); line(b);
  std::snprintf(b,sizeof b,"XYZ: %.1f / %.1f / %.1f",pos_.x,pos_.y,pos_.z); line(b);
  std::snprintf(b,sizeof b,"Chunks: %u drawn, %zu loaded, render distance %d%s",renderer_?renderer_->drawnChunks():0,streamer_?streamer_->meshedCount():size_t(0),settings_.renderDistance,streamer_&&streamer_->gpuFull()?" (GPU memory full)":""); line(b);
  std::snprintf(b,sizeof b,"%s mode%s  seed %lld",modeName(cur_.mode),flying_?" (flying)":"",(long long)(int64_t)cur_.seed); line(b);
  if(renderer_){ std::snprintf(b,sizeof b,"GPU (%s): chunks resident %zu, drawn %u, vertex memory %.0f%%",renderer_->name(),renderer_->chunkCount(),renderer_->drawnChunks(),100.0*double(renderer_->vertexUsed())/double(std::max(1u,renderer_->vertexCapacity()))); line(b); }
  std::snprintf(b,sizeof b,"Memory: %d MB / %d MB limit%s  HP %.0f  armour %d",rssMB_,settings_.ramLimitMB,memPressure_?" (cleaning)":"",health_,armorTotal(cur_.inv)); line(b);
  if(hit_.hit){std::snprintf(b,sizeof b,"Looking at: %s (%d %d %d)",Registry::get().blockName((BlockId)world_.get(hit_.x,hit_.y,hit_.z)).c_str(),hit_.x,hit_.y,hit_.z);line(b);}
 }
}

// ------------------------------------------------------------------ title / worlds
void Game::screenTitle(){
 drawBackground(false); const float W=ui_.W(),H=ui_.H();
 const Sprite&logo=uiAtlas_.get("gui/title/minecraft"),&ed=uiAtlas_.get("gui/title/edition");
 float ly=std::floor(std::min(24.f,H*0.08f));
 if(logo.valid()){                                        // minecraft.png (256 gui px wide) with edition.png tucked underneath, like the original
  const float lw=256.f,lh=lw*float(logo.h)/float(logo.w); ui_.sprite(logo,std::floor(W/2-lw/2),ly,lw,lh);
  if(ed.valid()){ const float ew=128.f,eh=ew*float(ed.h)/float(ed.w); ui_.sprite(ed,std::floor(W/2-ew/2),ly+42,ew,eh); }
 } else { ui_.bigText("hypervox",W/2,std::floor(H*0.16f),3.f,COL_WHITE); }
 const float bx=std::floor(W/2)-100,by=std::max(std::floor(H*0.42f),ly+88);
 if(ui_.button(bx,by,200,20,"Singleplayer")){worlds_=saves_.list();selWorld_=worlds_.empty()?-1:0;worldScroll_=0;screen_=Screen::WorldSelect;}
 if(ui_.button(bx,by+24,200,20,"Options...")){optionsFrom_=Screen::Title;screen_=Screen::Options;}
 if(ui_.button(bx,by+48,200,20,"Quit Game"))glfwSetWindowShouldClose(win_,GLFW_TRUE);
 // account: face + user name in the corner, click to open the account screen
 { const float ax=4,ay=4,aw=118,ah=24; const bool hv=ui_.hover(ax,ay,aw,ah);
   ui_.rect(ax,ay,aw,ah,hv?rgba(40,40,40,230):rgba(0,0,0,170)); ui_.rect(ax,ay,aw,1,rgba(200,200,200,hv?255:120)); ui_.rect(ax,ay+ah-1,aw,1,rgba(200,200,200,hv?255:120));
   drawSkinHead(settings_.skin,ax+4,ay+4,2.0f); ui_.text(settings_.username,ax+26,ay+4,COL_WHITE); ui_.text("Account",ax+26,ay+14,COL_GRAY);
   if(hv&&ui_.input().lclick){ refreshSkins(); skinScroll_=0; editingName_=false; focus_=-1; screen_=Screen::Account; } }
 ui_.text("Resource pack: "+(assets_.packName=="default"?std::string("Default"):assets_.packName),3,H-11,COL_GRAY);
 ui_.text(std::string("Renderer: ")+(renderer_?renderer_->name():"?"),W-3-ui_.textWidth(std::string("Renderer: ")+(renderer_?renderer_->name():"?")),H-11,COL_GRAY);
}

// ------------------------------------------------------------------ account / skins
void Game::refreshSkins(){
 namespace fs=std::filesystem; std::error_code ec; skinIds_.clear(); skinNames_.clear();
 auto cap=[](std::string s){ if(!s.empty())s[0]=(char)std::toupper((unsigned char)s[0]); return s; };
 for(const char*kind:{"wide","slim"}){
  std::vector<fs::path> f; for(auto&e:fs::directory_iterator(assets_.base/"entity"/"player"/kind,ec))if(e.path().extension()==".png")f.push_back(e.path()); std::sort(f.begin(),f.end());
  for(auto&p:f){ skinIds_.push_back(std::string("default:")+kind+"/"+p.stem().string()); skinNames_.push_back(cap(p.stem().string())+(std::string(kind)=="slim"?" (Slim)":"")); }
 }
 std::vector<fs::path> cu; for(auto&e:fs::directory_iterator(Settings::skinsDir(),ec)){ std::string x=e.path().extension().string(); for(auto&ch:x)ch=(char)std::tolower((unsigned char)ch); if(x==".png")cu.push_back(e.path()); } std::sort(cu.begin(),cu.end());
 for(auto&p:cu){ skinIds_.push_back("custom:"+p.filename().string()); skinNames_.push_back(p.stem().string()); skinCache_.erase("custom:"+p.filename().string()); }
}
const AtlasImage* Game::skinImage(const std::string&id){
 auto it=skinCache_.find(id); if(it!=skinCache_.end())return it->second.width?&it->second:nullptr;
 AtlasImage img; std::filesystem::path p;
 if(id.rfind("default:",0)==0)p=assets_.resolve("entity/player/"+id.substr(8)+".png"); else if(id.rfind("custom:",0)==0)p=Settings::skinsDir()/id.substr(7);
 if(p.empty()||!readPng(p,img)||img.width<64||img.height<32)img=AtlasImage{};
 AtlasImage&slot=skinCache_[id]; slot=std::move(img); return slot.width?&slot:nullptr;
}
void Game::drawSkinHead(const std::string&id,float x,float y,float p){
 const AtlasImage*s=skinImage(id); if(!s){ui_.rect(x,y,8*p,8*p,rgba(120,120,120));return;}
 const int k=(int)s->width/64;
 for(int layer=0;layer<2;layer++)for(int j=0;j<8;j++)for(int i=0;i<8;i++){
  const uint8_t*q=&s->rgba[((size_t)((layer?8:8)*k+j*k+k/2)*s->width+(size_t)((layer?40:8)*k+i*k+k/2))*4]; if(q[3]<8)continue;
  ui_.rect(x+i*p,y+j*p,p,p,rgba(q[0],q[1],q[2],255)); }
}
void Game::drawSkinFull(const std::string&id,float x,float y,float p){
 const AtlasImage*s=skinImage(id); if(!s){return;}
 const int k=(int)s->width/64; const bool modern=s->height>=(uint32_t)(64*k);
 bool slim=id.find("/slim/")!=std::string::npos||id.find("slim/")!=std::string::npos;
 if(id.rfind("custom:",0)==0&&modern){ const uint8_t*q=&s->rgba[((size_t)(20*k+k/2)*s->width+(size_t)(55*k+k/2))*4]; slim=q[3]==0; }
 auto blit=[&](int sx,int sy,int w,int h,float dx,float dy,bool flip){
  for(int j=0;j<h;j++)for(int i=0;i<w;i++){ const int si=flip?w-1-i:i; const uint8_t*q=&s->rgba[((size_t)(sy*k+j*k+k/2)*s->width+(size_t)(sx*k+si*k+k/2))*4]; if(q[3]<8)continue; ui_.rect(x+dx+i*p,y+dy+j*p,p,p,rgba(q[0],q[1],q[2],255)); } };
 const int aw=slim?3:4; const float rax=slim?1*p:0.f;
 for(int layer=0;layer<2;layer++){
  blit(layer?40:8,8,8,8,4*p,0,false);                                               // head (+ hat)
  blit(20,layer?36:20,8,12,4*p,8*p,false);                                          // body (+ jacket)
  blit(44,layer?36:20,aw,12,rax,8*p,false);                                         // right arm (+ sleeve)
  if(modern)blit(layer?52:36,layer?52:52,aw,12,12*p,8*p,false); else if(!layer)blit(44,20,aw,12,12*p,8*p,true);   // left arm
  blit(4,layer?36:20,4,12,4*p,20*p,false);                                          // right leg (+ pants)
  if(modern)blit(layer?4:20,52,4,12,8*p,20*p,false); else if(!layer)blit(4,20,4,12,8*p,20*p,true);
 }
}
void Game::screenAccount(){
 drawBackground(true); const float W=ui_.W(),H=ui_.H(); const int n=(int)skinIds_.size();
 ui_.textCentered("Account",W/2,12);
 const float cx=std::floor(W/2);
 ui_.text("Username",cx-100,26,COL_GRAY);
 if(ui_.textField(3,cx-100,36,200,20,settings_.username,16,focus_)){}
 if(settings_.username.empty()&&focus_!=3)settings_.username="Player";
 // big preview of the selected skin on the left, grid of skins on the right
 const float top=64,bot=H-64; ui_.textCentered("Skin",W/2,top-10,COL_GRAY);
 const float cell=38,cols=std::max(1.f,std::floor((W-150)/cell)); const float gx=W-10-cols*cell>140?W-10-cols*cell:140; const int vis=std::max(1,(int)((bot-top)/cell));
 const int rowsTotal=((n+(int)cols-1)/(int)cols); const int maxScroll=std::max(0,rowsTotal-vis); skinScroll_=std::clamp(skinScroll_-(int)ui_.input().wheel,0,maxScroll);
 ui_.rect(10,top,110,bot-top,rgba(0,0,0,150)); drawSkinFull(settings_.skin,10+(110-16*3.f)/2,top+(bot-top-32*3.f)/2,3.f);
 std::string selName; for(int i=0;i<n;i++)if(skinIds_[i]==settings_.skin)selName=skinNames_[i]; ui_.textCentered(selName.empty()?settings_.skin:selName,65,bot-10,COL_WHITE);
 std::string tip;
 for(int r=0;r<vis;r++)for(int c=0;c<(int)cols;c++){
  const int i=(r+skinScroll_)*(int)cols+c; if(i>=n)break; const float x=gx+c*cell,y=top+r*cell; const bool sel=skinIds_[i]==settings_.skin,hv=ui_.hover(x,y,cell-2,cell-2);
  ui_.rect(x,y,cell-2,cell-2,sel?rgba(255,255,255):(hv?rgba(160,160,160):rgba(70,70,70))); ui_.rect(x+1,y+1,cell-4,cell-4,rgba(20,20,20));
  drawSkinHead(skinIds_[i],x+(cell-2-4*8)/2,y+(cell-2-4*8)/2,4.f);
  if(hv){tip=skinNames_[i]; if(ui_.input().lclick){settings_.skin=skinIds_[i];settings_.save();}}
 }
 if(!tip.empty())ui_.tooltip(tip);
 if(ui_.button(cx-154,H-52,150,20,"Add Skin...")){ openFolder(Settings::skinsDir()); noticeMsg_="Drop a skin .png here or copy it into the folder, then reopen"; noticeUntil_=time_+6.0; }
 if(ui_.button(cx+4,H-52,150,20,"Refresh"))refreshSkins();
 ui_.textCentered("Tip: drag & drop a skin (.png) onto the window to add it",W/2,bot+4,COL_GRAY);
 if(ui_.button(W/2-100,H-28,200,20,"Done")){ focus_=-1; if(settings_.username.empty())settings_.username="Player"; settings_.save(); screen_=Screen::Title; }
}

void Game::screenWorldSelect(){
 drawBackground(true); const float W=ui_.W(),H=ui_.H();
 ui_.textCentered("Select World",W/2,12);
 const float top=32,bot=H-64,rowH=36,x=std::floor(W/2)-150; const int vis=std::max(1,(int)((bot-top)/rowH));
 ui_.tiled(uiAtlas_.get("gui/menu_list_background"),0,top,W,bot-top,32,rgba(70,70,70,200));
 ui_.tiled(uiAtlas_.get("gui/header_separator"),0,top-2,W,2,32); ui_.tiled(uiAtlas_.get("gui/footer_separator"),0,bot,W,2,32);
 const int n=(int)worlds_.size(); const int maxScroll=std::max(0,n-vis);
 worldScroll_=std::clamp(worldScroll_-ui_.input().wheel,0.f,float(maxScroll)); const int first=(int)worldScroll_;
 static double lastClick=0; static int lastRow=-1; bool play=false;
 if(n==0)ui_.textCentered("No worlds yet - create one!",W/2,top+20,COL_GRAY);
 for(int r=0;r<vis&&first+r<n;r++){
  const int i=first+r; const float y=top+2+r*rowH; const WorldInfo&w=worlds_[i]; const bool hv=ui_.hover(x,y,300,rowH)&&ui_.my()>=top&&ui_.my()<bot;
  if(i==selWorld_){ ui_.rect(x-1,y-1,302,rowH+1,rgba(200,200,200)); ui_.rect(x,y,300,rowH-1,rgba(0,0,0,255)); }
  ui_.sprite(uiAtlas_.get(hv?"gui/sprites/world_list/join_highlighted":"gui/sprites/world_list/join"),x+2,y+2,32,32);
  ui_.text(w.name,x+38,y+3);
  char date[32]="";{time_t t=(time_t)w.lastPlayed;std::tm tmv{};
#ifdef _WIN32
   localtime_s(&tmv,&t);
#else
   localtime_r(&t,&tmv);
#endif
   std::strftime(date,sizeof date,"%Y-%m-%d %H:%M",&tmv);}
  ui_.text(w.folder+" ("+date+")",x+38,y+14,COL_GRAY);
  ui_.text(std::string(modeName(w.mode))+" Mode, Seed: "+std::to_string((long long)(int64_t)w.seed),x+38,y+25,COL_GRAY);
  if(hv&&ui_.input().lclick){ if(i==lastRow&&ui_.input().time-lastClick<0.4)play=true; selWorld_=i; lastRow=i; lastClick=ui_.input().time; }
 }
 const bool has=selWorld_>=0&&selWorld_<n; const float bx=std::floor(W/2)-154;
 if(ui_.button(bx,H-52,150,20,"Play Selected World",has)||play){ if(has)startLoading(LoadTask::EnterWorld,&worlds_[selWorld_]); }
 else if(ui_.button(bx+158,H-52,150,20,"Create New World")){newName_="New World";newSeed_.clear();focus_=1;screen_=Screen::CreateWorld;}
 else if(ui_.button(bx,H-28,150,20,"Delete",has))screen_=Screen::ConfirmDelete;
 else if(ui_.button(bx+158,H-28,150,20,"Cancel"))screen_=Screen::Title;
}

void Game::screenCreateWorld(){
 drawBackground(true); const float W=ui_.W(),H=ui_.H(),x=std::floor(W/2)-100;
 ui_.textCentered("Create New World",W/2,14);
 ui_.text("World Name",x,40,COL_GRAY); ui_.textField(1,x,52,200,20,newName_,32,focus_);
 if(ui_.button(x,82,200,20,std::string("Game Mode: ")+modeName(newMode_)))newMode_=newMode_==MODE_CREATIVE?MODE_SURVIVAL:MODE_CREATIVE;
 ui_.text(newMode_==MODE_CREATIVE?"Unlimited blocks, flying, instant building":"Collect blocks from the world",x,106,COL_GRAY);
 ui_.text("Seed (leave empty for random)",x,126,COL_GRAY); ui_.textField(2,x,138,200,20,newSeed_,24,focus_);
 if(ui_.button(x,164,200,20,std::string("World Type: ")+(newDebug_?"Debug (every block)":"Default")))newDebug_=!newDebug_;
 if(ui_.button(x,H-28,98,20,"Create World")){
  WorldInfo info; if(saves_.create(newName_.empty()?"New World":newName_,newDebug_?MODE_CREATIVE:newMode_,SaveManager::seedFromString(newSeed_),info)){ if(newDebug_){info.debug=true;info.flying=true;saves_.saveLevel(info);} focus_=-1;startLoading(LoadTask::EnterWorld,&info);} else errorMsg_="Could not create the world folder";
 } else if(ui_.button(x+102,H-28,98,20,"Cancel")){focus_=-1;screen_=Screen::WorldSelect;}
}

void Game::screenConfirmDelete(){
 drawBackground(true); const float W=ui_.W(),H=ui_.H();
 const std::string name=(selWorld_>=0&&selWorld_<(int)worlds_.size())?worlds_[selWorld_].name:"";
 ui_.textCentered("Are you sure you want to delete this world?",W/2,H/2-30);
 ui_.textCentered("'"+name+"' will be lost forever!",W/2,H/2-16,rgba(255,85,85));
 if(ui_.button(W/2-102,H/2+6,100,20,"Delete")){ if(selWorld_>=0&&selWorld_<(int)worlds_.size())saves_.remove(worlds_[selWorld_].folder); worlds_=saves_.list(); selWorld_=worlds_.empty()?-1:0; screen_=Screen::WorldSelect; }
 else if(ui_.button(W/2+2,H/2+6,100,20,"Cancel"))screen_=Screen::WorldSelect;
}

// ------------------------------------------------------------------ options / controls / textures
void Game::screenOptions(){
 drawBackground(true); const float W=ui_.W(),H=ui_.H(),lx=std::floor(W/2)-155,rx=std::floor(W/2)+5,y0=std::max(34.f,H/2-80);
 ui_.textCentered("Options",W/2,14); bool changed=false;
 float rd=float(settings_.renderDistance-Settings::kMinRender)/float(Settings::kMaxSlider-Settings::kMinRender); rd=std::clamp(rd,0.f,1.f);
 if(ui_.slider(1,lx,y0,150,20,"Render Distance: "+std::to_string(settings_.renderDistance)+" chunks",rd)){settings_.renderDistance=Settings::kMinRender+(int)std::lround(rd*float(Settings::kMaxSlider-Settings::kMinRender));}
 float fv=float(settings_.fov-30)/80.f; if(ui_.slider(2,rx,y0,150,20,"FOV: "+std::to_string(settings_.fov),fv))settings_.fov=30+(int)std::lround(fv*80.f);
 if(ui_.slider(3,lx,y0+24,150,20,"Sensitivity: "+std::to_string((int)std::lround(settings_.mouseSens*100))+"%",settings_.mouseSens)){}
 if(ui_.button(rx,y0+24,150,20,std::string("VSync: ")+(settings_.vsync?"ON":"OFF"))){settings_.vsync=!settings_.vsync;if(renderer_)renderer_->setVsync(settings_.vsync);changed=true;}
 if(ui_.button(lx,y0+48,150,20,std::string("Debug Info: ")+(settings_.showDebug?"ON":"OFF"))){settings_.showDebug=!settings_.showDebug;changed=true;}
 if(ui_.button(rx,y0+48,150,20,"Controls..."))screen_=Screen::Controls;
 if(ui_.button(lx,y0+72,150,20,"Textures...")){packs_=listResourcePacks(assets_,Settings::packDirs(),cacheDir_/"packs");packSel_=0;for(size_t i=0;i<packs_.size();i++)if(!packs_[i].builtin&&packs_[i].name==settings_.resourcePack)packSel_=(int)i;packScroll_=0;screen_=Screen::Textures;}
 { int fw=0,fh=0; glfwGetFramebufferSize(win_,&fw,&fh); const int ms=Ui::maxScale(float(fw),float(fh)); const std::string sc=settings_.guiScale<=0?"Auto":std::to_string(settings_.guiScale);
   if(ui_.button(lx,y0+96,150,20,"Menu Scale: "+sc)){ settings_.guiScale=(settings_.guiScale+1)%(ms+1); changed=true; } }
 if(ui_.button(rx,y0+96,150,20,std::string("Renderer: ")+(settings_.renderer?"OpenGL":"Vulkan"))){ settings_.renderer=!settings_.renderer; changed=true; noticeMsg_="Renderer changes after you restart the game"; noticeUntil_=time_+4.0; }
 { // memory limit (up / down in 256 MB steps, minimum 256) + cleaner
   if(ui_.button(lx,y0+120,78,20,"Limit Up")){ settings_.ramLimitMB+=256; changed=true; }
   if(ui_.button(lx+72,y0+120,78,20,"Limit Down")){ settings_.ramLimitMB=std::max(256,settings_.ramLimitMB-256); changed=true; }
   if(ui_.button(rx,y0+120,150,20,"Clean Memory Now")){ cleanMemory(); noticeMsg_="Memory cleaned"; noticeUntil_=time_+2.0; }
   if(ui_.button(rx,y0+168,150,20,std::string("Vulkan mipmaps: ")+(settings_.mipmaps?"On":"Off")+" (restart)")){ settings_.mipmaps=!settings_.mipmaps; changed=true; }
   { static const char*cm[3]={"Normal","Aggressive","Very Aggressive"}; if(ui_.button(lx,y0+144,150,20,std::string("Cleaner: ")+cm[std::clamp(settings_.cleanerMode,0,2)])){ settings_.cleanerMode=(settings_.cleanerMode+1)%3; changed=true; } }
   ui_.text("RAM limit: "+std::to_string(settings_.ramLimitMB)+" MB  (using "+std::to_string(rssMB_)+" MB)",rx,y0+148,COL_GRAY); }
 ui_.text("Settings are saved automatically",rx,y0+78,COL_GRAY);
 if(ui_.sliderReleased()||changed)settings_.save();
 if(ui_.button(W/2-100,H-28,200,20,"Done")){settings_.save();screen_=optionsFrom_;}
}

void Game::screenControls(){
 drawBackground(true); const float W=ui_.W(),H=ui_.H(),y0=std::max(34.f,H/2-80);
 ui_.textCentered("Controls",W/2,14);
 if(rebinding_>=0){ for(int k=32;k<=GLFW_KEY_LAST;k++)if(k!=GLFW_KEY_ESCAPE&&keyPressed(k)){settings_.keys[rebinding_]=k;rebinding_=-1;settings_.save();break;} }
 int slot=0;
 for(int a=0;a<ACT_COUNT;a++){
  if(a==ACT_FLY)continue;   // flying is toggled by double-tapping Jump
  const float x=std::floor(W/2)-155+(slot%2)*160,y=y0+(slot/2)*24; slot++;
  const std::string label=std::string(kActionNames[a])+": "+(rebinding_==a?"> ? <":keyLabel(settings_.keys[a]));
  if(ui_.button(x,y,150,20,label)&&rebinding_<0)rebinding_=a;
 }
 if(ui_.button(std::floor(W/2)-100,y0+5*24+4,200,20,"Reset to Defaults")){settings_.resetKeys();settings_.save();}
 if(ui_.button(W/2-100,H-28,200,20,"Done")){rebinding_=-1;settings_.save();screen_=Screen::Options;}
}

void Game::screenTextures(){
 drawBackground(true); const float W=ui_.W(),H=ui_.H(),top=32,bot=H-76,rowH=36,x=std::floor(W/2)-150; const int vis=std::max(1,(int)((bot-top)/rowH));
 ui_.textCentered("Resource Packs",W/2,12);
 ui_.tiled(uiAtlas_.get("gui/menu_list_background"),0,top,W,bot-top,32,rgba(70,70,70,200));
 ui_.tiled(uiAtlas_.get("gui/header_separator"),0,top-2,W,2,32); ui_.tiled(uiAtlas_.get("gui/footer_separator"),0,bot,W,2,32);
 const int n=(int)packs_.size(),maxScroll=std::max(0,n-vis); packScroll_=std::clamp(packScroll_-ui_.input().wheel,0.f,float(maxScroll));
 for(int r=0;r<vis&&(int)packScroll_+r<n;r++){
  const int i=(int)packScroll_+r; const float y=top+2+r*rowH; const PackInfo&p=packs_[i]; const bool hv=ui_.hover(x,y,300,rowH)&&ui_.my()>=top&&ui_.my()<bot;
  if(i==packSel_){ui_.rect(x-1,y-1,302,rowH+1,rgba(200,200,200));ui_.rect(x,y,300,rowH-1,rgba(0,0,0,255));}
  ui_.sprite(uiAtlas_.get("misc/unknown_pack"),x+2,y+2,32,32);
  ui_.sprite(uiAtlas_.get(i==packSel_?(hv?"gui/sprites/transferable_list/unselect_highlighted":"gui/sprites/transferable_list/unselect"):(hv?"gui/sprites/transferable_list/select_highlighted":"gui/sprites/transferable_list/select")),x+270,y+2,32,32);
  ui_.text(p.name,x+38,y+4); ui_.text(p.description,x+38,y+16,COL_GRAY); if(i==packSel_)ui_.text("Active",x+38,y+26,rgba(85,255,85));
  if(hv&&ui_.input().lclick)packSel_=i;
 }
 ui_.textCentered("Drop .zip packs on the window or use Add Texture Pack  ("+packsDir_.string()+")",W/2,bot+5,COL_GRAY);
 if(ui_.button(std::floor(W/2)-100,H-52,200,20,"Add Texture Pack")){ openFolder(packsDir_); noticeMsg_="Copy a .zip / folder into the opened folder, then press Refresh"; noticeUntil_=time_+6.0; }
 if(ui_.button(std::floor(W/2)+104,H-52,60,20,"Refresh")){ packs_=listResourcePacks(assets_,Settings::packDirs(),cacheDir_/"packs"); packSel_=std::min(packSel_,std::max(0,(int)packs_.size()-1)); }
 const std::string cur=assets_.packName=="default"?"Default":assets_.packName; const bool same=packs_.empty()||packs_[std::min(packSel_,n-1)].name==cur;
 if(ui_.button(W/2-100,H-28,98,20,"Done")){
  if(n>0&&!same){settings_.resourcePack=packs_[packSel_].builtin?"":packs_[packSel_].name;settings_.save();startLoading(LoadTask::ReloadPack);} else screen_=Screen::Options;
 } else if(ui_.button(W/2+2,H-28,98,20,"Cancel"))screen_=Screen::Options;
}

void Game::screenLoading(){
 const float W=ui_.W(),H=ui_.H();
 ui_.tiled(uiAtlas_.get("gui/menu_background"),0,0,W,H,32,rgba(110,110,110));
 ui_.textCentered(loadMsg_,W/2,H/2-14);
 const float bw=200,bx=std::floor(W/2-bw/2),by=H/2+2; ui_.rect(bx-1,by-1,bw+2,10,rgba(255,255,255)); ui_.rect(bx,by,bw,8,rgba(0,0,0));
 ui_.rect(bx,by,bw*std::clamp(loadProgress_,0.f,1.f),8,rgba(120,220,90));
 if(loadTask_==LoadTask::ReloadPack)ui_.textCentered("Rebuilding the texture atlas (it is cached afterwards)",W/2,by+16,COL_GRAY);
}

// ------------------------------------------------------------------ in-game menus
void Game::screenPause(){
 drawBackground(true); const float W=ui_.W(),H=ui_.H(),x=std::floor(W/2)-100,y=std::floor(H/2)-40;
 ui_.textCentered("Game Menu",W/2,y-24);
 if(ui_.button(x,y,200,20,"Back to Game"))closePause();
 else if(ui_.button(x,y+24,200,20,"Options...")){optionsFrom_=Screen::Pause;screen_=Screen::Options;}
 else if(ui_.button(x,y+48,200,20,"Save and Quit to Title"))leaveWorld();
}

void Game::clickSlot(InvSlot&s,bool left,bool right){
 if(left){
  if(carried_.empty()){ if(!s.empty()){carried_=s;s.clear();} }
  else if(s.empty()){s=carried_;carried_.clear();}
  else if(s.id==carried_.id){const int m=std::min(carried_.count,Registry::get().maxStack(s.id)-s.count);s.count+=m;carried_.count-=m;if(carried_.count<=0)carried_.clear();}
  else std::swap(s,carried_);
 }else if(right){
  if(carried_.empty()){ if(!s.empty()){const int h=(s.count+1)/2;carried_=InvSlot{s.id,h};s.count-=h;if(s.count<=0)s.clear();} }
  else if(s.empty()){s=InvSlot{carried_.id,1};if(--carried_.count<=0)carried_.clear();}
  else if(s.id==carried_.id&&s.count<Registry::get().maxStack(s.id)){s.count++;if(--carried_.count<=0)carried_.clear();}
  else std::swap(s,carried_);
 }
}
void Game::quickMove(int from){
 InvSlot&src=cur_.inv.s[from]; if(src.empty())return; const int lo=from<9?9:0,hi=from<9?36:9;
 for(int pass=0;pass<2&&!src.empty();pass++)for(int i=lo;i<hi&&!src.empty();i++){
  InvSlot&d=cur_.inv.s[i];
  if(pass==0&&d.id==src.id&&d.count<Registry::get().maxStack(d.id)){const int m=std::min(src.count,Registry::get().maxStack(d.id)-d.count);d.count+=m;src.count-=m;if(src.count<=0)src.clear();}
  else if(pass==1&&d.empty()){d=src;src.clear();}
 }
}

void Game::screenInventory(){
 drawBackground(true); const UiInput&in=ui_.input(); const float W=ui_.W(),H=ui_.H(),px=std::floor((W-176)/2),py=std::floor((H-166)/2);
 ui_.region(uiAtlas_.get("gui/container/inventory"),0,0,176,166,px,py,176,166);
 ui_.text("Crafting",px+97,py+8,COL_DARK,false);
 const int hoverIdx=[&]{for(int i=0;i<36;i++){const float x=px+8+(i<9?i:(i-9)%9)*18,y=i<9?py+142:py+84+((i-9)/9)*18;if(ui_.hover(x,y,16,16))return i;}return -1;}();
 for(int i=0;i<36;i++){
  const float x=px+8+(i<9?i:(i-9)%9)*18,y=i<9?py+142:py+84+((i-9)/9)*18;
  if(i==hoverIdx)ui_.sprite(uiAtlas_.get("gui/sprites/container/slot_highlight_back"),x-4,y-4,24,24);
  drawSlot(cur_.inv.s[i],x,y);
  if(i==hoverIdx)ui_.sprite(uiAtlas_.get("gui/sprites/container/slot_highlight_front"),x-4,y-4,24,24);
 }
 const Registry&RR=Registry::get(); int armorHover=-1;
 for(int i=0;i<4;i++){ const float x=px+8,y=py+8+18*i; const bool hv=ui_.hover(x,y,16,16); if(hv)armorHover=i;
  if(hv)ui_.sprite(uiAtlas_.get("gui/sprites/container/slot_highlight_back"),x-4,y-4,24,24); drawSlot(cur_.inv.armor[i],x,y); if(hv)ui_.sprite(uiAtlas_.get("gui/sprites/container/slot_highlight_front"),x-4,y-4,24,24); }
 if(armorHover>=0&&(in.lclick||in.rclick)){ InvSlot&a=cur_.inv.armor[armorHover]; if(carried_.empty())std::swap(a,carried_); else if(RR.armorSlot(carried_.id)==armorHover)std::swap(a,carried_); markWorldDirty(); }
 else if(armorHover>=0&&carried_.empty()&&!cur_.inv.armor[armorHover].empty())ui_.tooltip(RR.item(cur_.inv.armor[armorHover].id).display);
 if(hoverIdx>=0&&(in.lclick||in.rclick)){ if(in.shift&&in.lclick)quickMove(hoverIdx); else clickSlot(cur_.inv.s[hoverIdx],in.lclick,in.rclick); }
 else if((in.lclick||in.rclick)&&!ui_.hover(px,py,176,166))carried_.clear();
 if(hoverIdx>=0&&carried_.empty()&&!cur_.inv.s[hoverIdx].empty())ui_.tooltip(Registry::get().item(cur_.inv.s[hoverIdx].id).display);
 if(!carried_.empty())drawSlot(carried_,ui_.mx()-8,ui_.my()-8);
}

void Game::screenCreative(){
 drawBackground(true); const UiInput&in=ui_.input(); const float W=ui_.W(),H=ui_.H(),px=std::floor((W-195)/2),py=std::floor((H-136)/2)+8;
 const Registry&R=Registry::get(); const int T=Registry::kTabCount;
 // tabs: 0-6 top row, 7 (Redstone) bottom-left, 8 (Survival Inventory) bottom-right
 auto tabPos=[&](int i,float&tx,float&ty,bool&bottom,int&col){ bottom=i>=7; col=i<7?i:(i==7?0:6); tx=px+27*col; ty=bottom?py+132:py-28; };
 auto tabSprite=[&](int i,bool sel){ bool b;float tx,ty;int col; tabPos(i,tx,ty,b,col); return std::string("gui/sprites/container/creative_inventory/tab_")+(b?"bottom_":"top_")+(sel?"selected_":"unselected_")+std::to_string(col+1); };
 std::string tip;
 for(int pass=0;pass<2;pass++)for(int i=0;i<T;i++){
  const bool sel=i==creaTab_; if((pass==1)!=sel)continue; float tx,ty;bool bt;int col; tabPos(i,tx,ty,bt,col);
  ui_.sprite(uiAtlas_.get(tabSprite(i,sel)),tx,ty,26,32);
  ui_.icon(R.tabIcon(i),tx+5,bt?ty+7:ty+9);
  if(ui_.hover(tx,ty+(bt?4:0),26,28)){tip=R.tabName(i);if(in.lclick){creaTab_=i;creaRow_=0;carried_.clear();}}
 }
 ui_.region(uiAtlas_.get(creaTab_==8?"gui/container/creative_inventory/tab_inventory":"gui/container/creative_inventory/tab_items"),0,0,195,136,px,py,195,136);
 // redraw selected tab over the panel border
 { float tx,ty;bool bt;int col; tabPos(creaTab_,tx,ty,bt,col); ui_.sprite(uiAtlas_.get(tabSprite(creaTab_,true)),tx,ty,26,32); ui_.icon(R.tabIcon(creaTab_),tx+5,bt?ty+7:ty+9); }
 if(creaTab_!=8)ui_.text(R.tabName(creaTab_),px+8,py+6,COL_DARK,false);
 const std::vector<ItemId>&items=R.tabItems(creaTab_); const int rows=((int)items.size()+8)/9,maxRow=std::max(0,rows-5);
 if(in.wheel!=0.f)creaRow_-=in.wheel>0?1:-1; creaRow_=std::clamp(creaRow_,0,maxRow);
 // scrollbar
 const float tx0=px+175,ty0=py+18; const bool canScroll=maxRow>0;
 if(canScroll&&in.lclick&&ui_.hover(tx0,ty0,14,112))creaDrag_=true; if(!in.ldown)creaDrag_=false;
 if(creaDrag_&&canScroll)creaRow_=std::clamp((int)std::lround(std::clamp((ui_.my()-ty0-7.5f)/97.f,0.f,1.f)*float(maxRow)),0,maxRow);
 if(creaTab_!=8)ui_.sprite(uiAtlas_.get(canScroll?"gui/sprites/container/creative_inventory/scroller":"gui/sprites/container/creative_inventory/scroller_disabled"),tx0+1,ty0+(canScroll?std::floor(97.f*float(creaRow_)/float(maxRow)):0),12,15);
 // item grid
 for(int r=0;r<5;r++)for(int c=0;c<9;c++){
  const int idx=(creaRow_+r)*9+c; if(idx>=(int)items.size())continue; const float x=px+9+18*c,y=py+18+18*r; const bool hv=ui_.hover(x,y,16,16);
  if(hv)ui_.sprite(uiAtlas_.get("gui/sprites/container/slot_highlight_back"),x-4,y-4,24,24);
  ui_.icon(items[idx],x,y);
  if(hv){ ui_.sprite(uiAtlas_.get("gui/sprites/container/slot_highlight_front"),x-4,y-4,24,24); tip=R.item(items[idx]).display;
   if(in.lclick||in.rclick){ if(!carried_.empty())carried_.clear(); else if(in.shift&&in.lclick)cur_.inv.add(items[idx],Registry::get().maxStack(items[idx])); else carried_=InvSlot{items[idx],in.rclick?1:kStackMax}; } }
 }
 // hotbar row
 for(int j=0;j<9;j++){
  const float x=px+9+18*j,y=py+112; const bool hv=ui_.hover(x,y,16,16);
  if(hv)ui_.sprite(uiAtlas_.get("gui/sprites/container/slot_highlight_back"),x-4,y-4,24,24);
  drawSlot(cur_.inv.s[j],x,y);
  if(hv){ ui_.sprite(uiAtlas_.get("gui/sprites/container/slot_highlight_front"),x-4,y-4,24,24);
   if(in.lclick||in.rclick)clickSlot(cur_.inv.s[j],in.lclick,in.rclick); else if(carried_.empty()&&!cur_.inv.s[j].empty())tip=R.item(cur_.inv.s[j].id).display; }
 }
 if(creaTab_==8){   // survival inventory: the 27 storage slots + a delete slot (put an item on it to destroy it)
  for(int r=0;r<3;r++)for(int c2=0;c2<9;c2++){
   const int idx=9+r*9+c2; const float x=px+9+18*c2,y=py+54+18*r; const bool hv=ui_.hover(x,y,16,16);
   if(hv)ui_.sprite(uiAtlas_.get("gui/sprites/container/slot_highlight_back"),x-4,y-4,24,24);
   drawSlot(cur_.inv.s[idx],x,y);
   if(hv){ ui_.sprite(uiAtlas_.get("gui/sprites/container/slot_highlight_front"),x-4,y-4,24,24);
    if(in.lclick||in.rclick)clickSlot(cur_.inv.s[idx],in.lclick,in.rclick); else if(carried_.empty()&&!cur_.inv.s[idx].empty())tip=R.item(cur_.inv.s[idx].id).display; }
  }
  for(int i=0;i<4;i++){ const float x=px+(i<2?54:108),y=py+(i%2?33:6); const bool hv=ui_.hover(x,y,16,16); drawSlot(cur_.inv.armor[i],x,y);
   if(hv){ ui_.sprite(uiAtlas_.get("gui/sprites/container/slot_highlight_front"),x-4,y-4,24,24); if(in.lclick||in.rclick){ InvSlot&a=cur_.inv.armor[i]; if(carried_.empty())std::swap(a,carried_); else if(R.armorSlot(carried_.id)==i)std::swap(a,carried_); markWorldDirty(); } } }
  if(ui_.hover(px+173,py+112,16,16)){ ui_.sprite(uiAtlas_.get("gui/sprites/container/slot_highlight_back"),px+169,py+108,24,24); tip="Delete Item"; if((in.lclick||in.rclick)&&!carried_.empty()){carried_.clear();markWorldDirty();} }
 }
 if((in.lclick||in.rclick)&&!carried_.empty()&&!ui_.hover(px,py,195,136)&&!ui_.hover(px,py-28,189,28))carried_.clear();
 if(carried_.empty()&&!tip.empty())ui_.tooltip(tip);
 if(!carried_.empty())drawSlot(carried_,ui_.mx()-8,ui_.my()-8);
}
