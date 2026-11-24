// Game side of the entities: hooks (damage, items, sounds, explosions), attacking / riding, dropped item sprites, rendering uploads, boss bar.
#include "Game.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>

namespace{
float heldAttack(const std::string&n){
 auto has=[&](const char*s){ return n.find(s)!=std::string::npos; };
 float tier=has("wooden")||has("gold")?0.f:has("stone")?1.f:has("iron")?2.f:has("diamond")?3.f:has("netherite")?4.f:0.f;
 if(has("_sword"))return 4.f+tier; if(has("_axe"))return has("wooden")||has("gold")?7.f:has("stone")?9.f:has("netherite")?10.f:9.f;
 if(has("_pickaxe"))return 2.f+tier; if(has("_shovel"))return 2.5f+tier; if(has("_hoe"))return 1.f; if(n=="trident")return 9.f; return 1.f;
}
}

std::vector<std::string> Game::entityLines()const{ std::vector<std::string> r; r.reserve(ents_.list.size()); for(const Entity&e:ents_.list){ std::string l; if(EntityMgr::serialize(e,l))r.push_back(l); } return r; }

void Game::entitiesReset(const std::string&folder,int dim){
 ents_.clear(); entUp_.clear(); riding_=false; ents_.riddenId=0; entAcc_=0.f; if(renderer_)renderer_->clearEntities(); dynTile_.clear(); dynOwner_.clear(); dynNext_=0;
 ents_.dragonDefeated=false; const std::filesystem::path root=saves_.regionDir(folder).parent_path();
 { std::ifstream f(root/"hypervox_dragon.txt"); int v=0; if(f&&(f>>v)&&v==1)ents_.dragonDefeated=true; }
 std::ifstream ef(root/(dim==1?"hypervox_entities_nether.txt":dim==2?"hypervox_entities_end.txt":"hypervox_entities.txt")); std::string ln;
 while(ef&&std::getline(ef,ln)){ Entity o; if(!EntityMgr::parse(ln,o))continue; Entity&n=ents_.spawn(o.type,o.pos); n.yaw=o.yaw; n.bodyYaw=o.yaw; n.prevYaw=o.yaw; n.prev=n.pos; n.health=std::min(o.health,n.maxHealth); n.variant=o.variant; n.item=o.item; n.count=o.count; if(o.type==EType::Item)n.pickupDelay=0.5f;
  if(o.type==EType::Slime){ n.variant=std::max(1,o.variant); n.w=n.h=0.52f*n.variant; n.maxHealth=float(n.variant*n.variant); n.health=std::min(n.health,n.maxHealth); }
  if(o.type==EType::Dragon||o.type==EType::Crystal)ents_.dragonSpawned=true; }
}

void Game::entitiesInit(){
 if(entInit_)return; entInit_=true; EntityEnv&E=entEnv_; E.world=&world_;
 E.hurtPlayer=[this](float a,const Vec3&from,float knock){ if(dead_||cur_.mode!=MODE_SURVIVAL)return; damage(a,time_,true); Vec3 d{pos_.x-from.x,0.f,pos_.z-from.z}; const float L=std::max(0.001f,std::sqrt(d.x*d.x+d.z*d.z)); vel_.x+=d.x/L*knock*6.f; vel_.z+=d.z/L*knock*6.f; vel_.y=std::max(vel_.y,4.f*std::min(1.f,knock)); onGround_=false; };
 E.givePlayer=[this](ItemId id,int n)->int{ const int left=cur_.inv.add(id,n); if(left<n)markWorldDirty(); return left; };
 E.sound=[this](const std::string&ev,const Vec3&at,float vol,float pitch){ const float d=std::sqrt((at.x-pos_.x)*(at.x-pos_.x)+(at.y-pos_.y)*(at.y-pos_.y)+(at.z-pos_.z)*(at.z-pos_.z)); const float v=vol*std::max(0.f,1.f-d/28.f); if(v>0.02f)playSound(ev,v,pitch); };
 E.explode=[this](const Vec3&p,float power,bool breakBlocks,uint32_t src){ explodeAt(p,power,breakBlocks,src); };
 E.setBlock=[this](int x,int y,int z,uint16_t id){ int out[3][2]; const int n=world_.set(x,y,z,id,out); markDirtyAround(x,z,n,out); markWorldDirty(); };
 E.notice=[this](const std::string&m){ noticeMsg_=m; noticeUntil_=time_+6.0; };
}

void Game::explodeAt(const Vec3&p,float power,bool breakBlocks,uint32_t src){
 if(power>0.f){ playSound("random.explode",1.f,0.9f+0.2f*float(std::rand()%100)/100.f); }
 if(power>0.f&&breakBlocks){
  const Registry&R=Registry::get(); const int r=(int)std::ceil(power); const int cx=(int)std::floor(p.x),cy=(int)std::floor(p.y),cz=(int)std::floor(p.z);
  for(int dy=-r;dy<=r;dy++)for(int dz=-r;dz<=r;dz++)for(int dx=-r;dx<=r;dx++){
   if(dx*dx+dy*dy+dz*dz>power*power)continue; const int x=cx+dx,y=cy+dy,z=cz+dz; if(y<1||y>=Chunk::HEIGHT-1)continue;
   const uint16_t id=world_.get(x,y,z); if(!id)continue; const BlockInfo&bi=R.info(id); if(!bi.breakable||bi.fluid)continue;
   const std::string&nm=R.blockName(R.baseOf(id)); if(nm=="bedrock"||nm=="obsidian"||nm=="crying_obsidian"||nm=="end_portal"||nm=="end_portal_frame"||nm=="command_block"||nm=="barrier")continue;
   int out[3][2]; dropContainer(x,y,z); const int n=world_.set(x,y,z,0,out); markDirtyAround(x,z,n,out);
   if(std::rand()%100<30){ const ItemId d=R.dropFor(id); if(d)ents_.spawnItem(Vec3{x+0.5f,y+0.3f,z+0.5f},d,1,Vec3{float(std::rand()%100-50)/25.f,3.f,float(std::rand()%100-50)/25.f},1.f); } }
  markWorldDirty(); }
 if(power>0.f){
  const float dp=std::sqrt((p.x-pos_.x)*(p.x-pos_.x)+(p.y-(pos_.y+0.9f))*(p.y-(pos_.y+0.9f))+(p.z-pos_.z)*(p.z-pos_.z));
  if(dp<2.f*power){ const float impact=1.f-dp/(2.f*power); const float dmg=(impact*impact+impact)/2.f*7.f*power+1.f; entEnv_.hurtPlayer(dmg,p,1.2f*impact+0.3f); }
  for(auto&e:ents_.list){ if(e.dead||e.id==src||e.type==EType::Item)continue; const float d=std::sqrt((p.x-e.pos.x)*(p.x-e.pos.x)+(p.y-(e.pos.y+e.h*0.5f))*(p.y-(e.pos.y+e.h*0.5f))+(p.z-e.pos.z)*(p.z-e.pos.z)); if(d<2.f*power){ const float impact=1.f-d/(2.f*power); float dmg=(impact*impact+impact)/2.f*7.f*power+1.f; if(e.type==EType::Dragon)dmg=std::min(dmg,10.f); e.hurt=0; ents_.hurt(e,dmg,p,1.f,entEnv_); } } }
}

uint16_t Game::itemSpriteTile(ItemId id){
 if(id<kItemBase)return 0; auto it=dynTile_.find(id); if(it!=dynTile_.end())return it->second;
 if(!renderer_||!pack_.has("dyn_item_0"))return 0;
 const size_t slot=dynNext_%64; dynNext_++; if(dynOwner_.size()<64)dynOwner_.resize(64,0); if(dynOwner_[slot])dynTile_.erase(dynOwner_[slot]);
 const uint16_t tile=pack_.id("dyn_item_"+std::to_string(slot)); dynOwner_[slot]=id; dynTile_[id]=tile;
 const Sprite&sp=uiAtlas_.plainItem(id-kItemBase); const AtlasImage&im=uiAtlas_.image(); TileUpdate tu; tu.id=tile; std::fill(tu.rgba,tu.rgba+1024,uint8_t(0));
 if(sp.valid()&&im.width>0){ const int x0=(int)std::lround(sp.u0*im.width),y0=(int)std::lround(sp.v0*im.height); for(int y=0;y<16;y++)for(int x=0;x<16;x++){ const int sx=x0+x*sp.w/16,sy=y0+y*sp.h/16; if(sx<0||sy<0||sx>=(int)im.width||sy>=(int)im.height)continue; const uint8_t*s=&im.rgba[((size_t)sy*im.width+sx)*4]; std::copy(s,s+4,&tu.rgba[(y*16+x)*4]); } }
 renderer_->updateTiles(std::vector<TileUpdate>{tu}); return tile;
}

// One game tick (20 per second, driven by Game::onTick so /tick rate and /tick freeze apply): entity AI, physics, spawning.
void Game::entitiesTick(){
 if(!inWorld_)return; entitiesInit(); EntityEnv&E=entEnv_; const double tm0=glfwGetTime();
 E.world=&world_; E.dim=cur_.dim; E.worldTime=(double)worldTime_; E.player=pos_; E.playerYaw=yaw_; E.playerPitch=pitch_; E.playerEye=eyeH_; E.playerAlive=!dead_; E.playerSurvival=cur_.mode==MODE_SURVIVAL; E.playerRiding=riding_;
 E.activeRadius=std::clamp(float(settings_.renderDistance*16)-24.f,48.f,110.f);
 if(cur_.dim==2&&!ents_.dragonSpawned&&!ents_.dragonDefeated&&cur_.mode!=MODE_SPECTATOR)ents_.setupEnd(E);
 if(riding_){
  auto kd=[&](int a){ return keyDown(settings_.keys[a]); };
  if(!ents_.find(ents_.riddenId)){ ents_.dismount(); riding_=false; }
  else ents_.drive((kd(ACT_FWD)?1.f:0.f)-(kd(ACT_BACK)?1.f:0.f),(kd(ACT_RIGHT)?1.f:0.f)-(kd(ACT_LEFT)?1.f:0.f),false,0.05f,E);
 }
 const bool wasDefeated=ents_.dragonDefeated;
 ents_.update(0.05f,E);
 if(ents_.dragonDefeated&&!wasDefeated){ std::ofstream f(saves_.regionDir(cur_.folder).parent_path()/"hypervox_dragon.txt"); f<<1<<"\n"; }
 const float ms=float((glfwGetTime()-tm0)*1000.0); msptEma_+=(ms-msptEma_)*0.1f;
}

// every frame: sneaking out of a vehicle, the rider follows the (interpolated) vehicle, the tick rate for the debug screen
void Game::entitiesUpdate(float dt,double now){
 (void)now; if(!inWorld_)return;
 tickWin_+=dt; if(tickWin_>=1.f){ tps_=float(tickCount_)/tickWin_; tickCount_=0; tickWin_=0.f; }
 if(riding_){
  if(keyPressed(settings_.keys[ACT_DOWN])){ ents_.dismount(); riding_=false; }
  else if(ents_.find(ents_.riddenId)){ pos_=ents_.ridePos(float(std::clamp(tickAcc_,0.0,1.0))); vel_=Vec3{}; onGround_=true; fallPeak_=pos_.y; }
  else riding_=false;
 }
}

void Game::entitiesRender(double now){
 if(!renderer_||!inWorld_)return; std::unordered_set<uint32_t> seen; static ChunkMesh mesh;
 const float alpha=(float)std::clamp(tickAcc_,0.0,1.0);
 for(const Entity&e0:ents_.list){
  if(e0.dead)continue; Entity e=e0; e.pos=Vec3{e0.prev.x+(e0.pos.x-e0.prev.x)*alpha,e0.prev.y+(e0.pos.y-e0.prev.y)*alpha,e0.prev.z+(e0.pos.z-e0.prev.z)*alpha}; { float dy=e0.yaw-e0.prevYaw; while(dy>3.14159f)dy-=6.28318f; while(dy<-3.14159f)dy+=6.28318f; e.yaw=e0.prevYaw+dy*alpha; } e.age=e0.age+alpha*0.05f; const float dx=e.pos.x-pos_.x,dz=e.pos.z-pos_.z; const float d2=dx*dx+dz*dz; const float maxD=e.type==EType::Dragon?220.f:std::min(112.f,float(settings_.renderDistance*16)); if(d2>maxD*maxD)continue;
  uint16_t tile=0; if(e.type==EType::Item)tile=itemSpriteTile(e.item); else if(e.type==EType::Fireball&&pack_.has("fx_fireball"))tile=pack_.id("fx_fireball"); else if(e.type==EType::DragonFireball&&pack_.has("fx_dragonfire"))tile=pack_.id("fx_dragonfire"); else if(e.type==EType::Arrow&&pack_.has("fx_arrow"))tile=pack_.id("fx_arrow");
  if(!buildEntityMesh(e,(float)now,tile,mesh))continue;
  const float rad=e.type==EType::Dragon?26.f:(e.type==EType::Ghast?6.f:std::max(2.f,e.h+1.f));
  if(renderer_->uploadEntity(e.id,e.pos.x-kEntityMeshOffX,e.pos.y-kEntityMeshOffY,e.pos.z-kEntityMeshOffZ,rad,mesh)||entUp_.count(e.id))seen.insert(e.id);   // upload refused this frame (staging busy): keep drawing the last mesh instead of making the mob blink
 }
 // end crystal beams: a glowing line from every crystal to the dragon it heals
 if(cur_.dim==2&&pack_.has("fx_dragonfire")){ const Entity*dr=nullptr; for(const Entity&e:ents_.list)if(e.type==EType::Dragon&&!e.dead)dr=&e; const uint16_t bt=pack_.id("fx_dragonfire");
  if(dr)for(const Entity&c:ents_.list){ if(c.type!=EType::Crystal||c.dead)continue; const Vec3 a{c.pos.x,c.pos.y+1.5f,c.pos.z},b{dr->pos.x,dr->pos.y+2.f,dr->pos.z}; if(std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z))>60.f)continue;
   for(int pc=0;pc<8;pc++){ Vec3 org; if(!buildBeamPiece(a,b,pc,bt,(float)now,mesh,org))break; const uint32_t id=0x40000000u+c.id*8u+(uint32_t)pc; if(renderer_->uploadEntity(id,org.x,org.y,org.z,14.f,mesh)||entUp_.count(id))seen.insert(id); } } }
 for(uint32_t id:entUp_)if(!seen.count(id))renderer_->releaseEntity(id);
 entUp_.swap(seen);
}

void Game::entityPre(const Vec3&eye,const Vec3&dir,bool creative,const UiInput&in,float dt){
 entAttackCd_=std::max(0.f,entAttackCd_-dt); entTarget_=0; if(riding_)return;
 float et=0; Entity*he=ents_.raycast(eye,dir,creative?6.f:3.2f,et); if(!he)return;
 if(hit_.hit){ const float bd=std::sqrt((hit_.x+0.5f-eye.x)*(hit_.x+0.5f-eye.x)+(hit_.y+0.5f-eye.y)*(hit_.y+0.5f-eye.y)+(hit_.z+0.5f-eye.z)*(hit_.z+0.5f-eye.z)); if(bd<et)return; }   // a block in front of it
 entTarget_=he->id; hit_=Hit{};
 if(in.lclick&&entAttackCd_<=0.f){
  const InvSlot&hs=cur_.inv.held(); const float dmg=creative?1000.f:(hs.empty()||hs.id<kItemBase?1.f:heldAttack(Registry::get().itemName(hs.id)));
  if(he->type==EType::Boat||he->type==EType::Minecart){ he->health=0; he->hurt=0; ents_.hurt(*he,1.f,pos_,0.f,entEnv_); if(!he->dead){ he->dead=true; } if(creative){} }
  else { entAttackCd_=0.45f; he->hurt=0.f; ents_.hurt(*he,dmg,pos_,1.f,entEnv_); if(!creative)exh_+=0.1f; }
  playSound("damage.hit",0.6f,1.f); markWorldDirty();
 }
}

bool Game::entityRightClick(const Vec3&eye,const Vec3&dir,bool click){
 (void)eye;(void)dir; if(!click)return entTarget_!=0;
 if(riding_)return true;
 if(entTarget_){ Entity*e=ents_.find(entTarget_); if(e&&(e->type==EType::Boat||e->type==EType::Minecart)&&!e->dead){ ents_.mount(*e); ents_.riddenId=e->id; riding_=true; return true; } return true; }
 const InvSlot&hs=cur_.inv.held(); if(hs.empty()||hs.id<kItemBase||!hit_.hit)return false; const std::string n=Registry::get().itemName(hs.id);
 const bool boat=n.size()>=5&&n.compare(n.size()-5,5,"_boat")==0&&n.find("chest")==std::string::npos; const bool cart=n=="minecart";
 if(boat){ Entity&b=ents_.spawn(EType::Boat,Vec3{hit_.x+0.5f+hit_.nx*0.6f,hit_.y+(Registry::get().info(world_.get(hit_.x,hit_.y,hit_.z)).fluid?0.85f:1.f)+hit_.ny*0.f,hit_.z+0.5f+hit_.nz*0.6f}); b.yaw=yaw_+3.14159f; if(cur_.mode==MODE_SURVIVAL)cur_.inv.consumeHeld(); markWorldDirty(); return true; }
 if(cart){ const uint16_t id=world_.get(hit_.x,hit_.y,hit_.z); const std::string&bn=Registry::get().blockName(Registry::get().baseOf(id)); if(id&&bn.size()>=4&&bn.compare(bn.size()-4,4,"rail")==0){ ents_.spawn(EType::Minecart,Vec3{hit_.x+0.5f,hit_.y+0.1f,hit_.z+0.5f}); if(cur_.mode==MODE_SURVIVAL)cur_.inv.consumeHeld(); markWorldDirty(); return true; } }
 return false;
}

void Game::drawBossBar(){
 if(!inWorld_||cur_.dim!=2||!ents_.dragonAlive())return; const float W=ui_.W(); const float frac=std::clamp(ents_.dragonHealth()/ents_.dragonMax(),0.f,1.f);
 const float w=182.f,x=std::floor(W/2-w/2),y=14.f; ui_.text("Ender Dragon",std::floor(W/2-ui_.textWidth("Ender Dragon")/2),y-11.f,COL_WHITE,true);
 ui_.rect(x,y,w,5.f,rgba(30,10,40,200)); ui_.rect(x,y,w*frac,5.f,rgba(190,40,200,255)); ui_.rect(x,y,w,1.f,rgba(255,255,255,60));
}
