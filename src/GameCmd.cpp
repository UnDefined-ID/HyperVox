// All singleplayer chat commands, sound init, and sound ticks
#include "Game.hpp"
#include "MemUtil.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>

// ─── helpers ──────────────────────────────────────────────────────────────────
static std::string join(const std::vector<std::string>& a, int from=1) {
    std::string r; for(int i=from;i<(int)a.size();i++){if(i>from)r+=' ';r+=a[i];} return r;
}
static bool parseInt(const std::string& s, int& out) {
    try{ out=(int)std::stol(s); return true; }catch(...){ return false; }
}
static bool parseFloat(const std::string& s, float& out) {
    try{ out=std::stof(s); return true; }catch(...){ return false; }
}

// ─── commands ─────────────────────────────────────────────────────────────────
void Game::registerCommands() {
    const double& now = time_;
    chat_.onCommand = [this,&now](const std::vector<std::string>& a) -> bool {
        const std::string& cmd = a[0];

        // /tp [x y z | player]
        if(cmd=="tp"||cmd=="teleport") {
            if(a.size()>=4) {
                float x,y,z;
                if(parseFloat(a[1],x)&&parseFloat(a[2],y)&&parseFloat(a[3],z)) {
                    pos_=Vec3{x,y,z}; vel_=Vec3{}; chat_.sys("Teleported to "+a[1]+" "+a[2]+" "+a[3],now); return true;
                }
            } else if(a.size()==2&&(a[1]=="~"||a[1]=="here")) {
                chat_.sys("Already here: "+std::to_string((int)pos_.x)+" "+std::to_string((int)pos_.y)+" "+std::to_string((int)pos_.z),now); return true;
            }
            chat_.sys("Usage: /tp <x> <y> <z>",now); return true;
        }

        // /gamemode <survival|creative|s|c|0|1>
        if(cmd=="gamemode"||cmd=="gm") {
            if(a.size()<2){chat_.sys("Usage: /gamemode <survival|creative>",now);return true;}
            const std::string& m=a[1];
            if(m=="creative"||m=="c"||m=="1"){cur_.mode=MODE_CREATIVE;flying_=false;chat_.sys("Game mode set to Creative",now);}
            else if(m=="spectator"||m=="sp"||m=="3"){cur_.mode=MODE_SPECTATOR;flying_=true;chat_.sys("Game mode set to Spectator",now);}
            else if(m=="survival"||m=="s"||m=="0"){cur_.mode=MODE_SURVIVAL;flying_=false;chat_.sys("Game mode set to Survival",now);}
            else chat_.sys("Unknown mode: "+m,now);
            return true;
        }

        // /gamerule <rule> [value]
        if(cmd=="gamerule") {
            if(a.size()==1){
                chat_.sys("Rules: tickRate="+std::to_string((int)tickRate_)+" doDaylightCycle="+std::string(doDaylight_?"true":"false")+" doWeatherCycle="+std::string(doWeather_?"true":"false"),now);
                return true;
            }
            const std::string& rule=a[1];
            if(a.size()==2) {
                if(rule=="tickRate") chat_.sys("tickRate = "+std::to_string((int)tickRate_),now);
                else if(rule=="doDaylightCycle") chat_.sys(std::string("doDaylightCycle = ")+(doDaylight_?"true":"false"),now);
                else if(rule=="doWeatherCycle") chat_.sys(std::string("doWeatherCycle = ")+(doWeather_?"true":"false"),now);
                else chat_.sys("Unknown rule: "+rule,now);
                return true;
            }
            const std::string& val=a[2];
            if(rule=="tickRate"){float v;if(parseFloat(val,v)){tickRate_=std::clamp(v,0.001f,1000.f);chat_.sys("tickRate = "+std::to_string((int)tickRate_),now);}else chat_.sys("Invalid value",now);}
            else if(rule=="doDaylightCycle"){doDaylight_=(val=="true");chat_.sys("doDaylightCycle = "+val,now);}
            else if(rule=="doWeatherCycle"){doWeather_=(val=="true");chat_.sys("doWeatherCycle = "+val,now);}
            else chat_.sys("Unknown rule: "+rule,now);
            return true;
        }

        // /time <set|add|query> <value>
        if(cmd=="time") {
            if(a.size()<2){chat_.sys("Usage: /time <set|add|query> [value]",now);return true;}
            if(a[1]=="query"){chat_.sys("Time: "+std::to_string(worldTime_)+" ("+std::to_string(worldTime_%24000)+" ticks into day)",now);return true;}
            if(a.size()<3){chat_.sys("Usage: /time set|add <ticks>",now);return true;}
            int v;
            if(!parseInt(a[2],v)){ // named times
                if(a[2]=="day")v=1000; else if(a[2]=="noon")v=6000; else if(a[2]=="sunset")v=12000; else if(a[2]=="night")v=13000; else if(a[2]=="midnight")v=18000; else if(a[2]=="sunrise")v=23000; else{chat_.sys("Invalid time",now);return true;}
            }
            if(a[1]=="set") worldTime_=(worldTime_/24000)*24000+v;
            else if(a[1]=="add") worldTime_+=v;
            else{chat_.sys("Usage: /time <set|add|query>",now);return true;}
            chat_.sys("Set time to "+std::to_string(worldTime_%24000),now); return true;
        }

        // /tick <query|rate|freeze|unfreeze|step [n]>
        if(cmd=="tick") {
            auto q=[&]{ chat_.sys("Tick rate "+std::to_string((int)tickRate_)+" tps"+(tickRate_<=0?" (frozen)":"")+", tick "+std::to_string(gameTicks_)+", day time "+std::to_string(worldTime_%24000),now); };
            if(a.size()<2||a[1]=="query"){q();return true;}
            if(a[1]=="freeze"){tickRate_=0;chat_.sys("Game frozen (use /tick step)",now);}
            else if(a[1]=="unfreeze"){tickRate_=20;chat_.sys("Game running at 20 tps",now);}
            else if(a[1]=="step"){ int n=1; if(a.size()>=3&&!parseInt(a[2],n))n=1; n=std::clamp(n,1,10000); stepTicks_+=n; chat_.sys("Stepping "+std::to_string(n)+" tick(s)",now); }
            else if(a[1]=="rate"){ float v; if(a.size()>=3&&parseFloat(a[2],v)){tickRate_=std::clamp(v,0.f,10000.f);chat_.sys("Tick rate set to "+a[2],now);} else q(); }
            else chat_.sys("Usage: /tick <query|rate <n>|freeze|unfreeze|step [n]>",now);
            return true;
        }

        // /dimension <overworld|nether|end>   (also: /dim)
        if(cmd=="dimension"||cmd=="dim") {
            if(a.size()<2){chat_.sys("You are in the "+std::string(world_.dimension()==0?"overworld":world_.dimension()==1?"nether":"end")+". Usage: /dimension <overworld|nether|end>",now);return true;}
            const std::string t=a[1]; const int to=(t=="overworld"||t=="0")?0:(t=="nether"||t=="the_nether"||t=="1")?1:(t=="end"||t=="the_end"||t=="2")?2:-1;
            if(to<0){chat_.sys("Unknown dimension: "+t,now);return true;}
            if(to==world_.dimension()){chat_.sys("Already there",now);return true;}
            chat_.sys("Travelling ...",now); travelTo(to); return true;
        }

        // /locate block <name> [radius in chunks]   (searches the terrain around you; structures / biomes need data the terrain helper does not give us)
        if(cmd=="adaptive") { adaptiveDist_=!(a.size()>=2&&a[1]=="off"); if(a.size()>=2&&a[1]=="on")adaptiveDist_=true; chat_.sys(std::string("Adaptive draw distance: ")+(adaptiveDist_?"on (lowers the number of chunks drawn when the frame rate drops)":"off"),now); return true; }
        if(cmd=="summon") {   // /summon <mob> [x y z]
            static const std::vector<std::pair<std::string,EType>> kinds={{"cow",EType::Cow},{"pig",EType::Pig},{"chicken",EType::Chicken},{"creeper",EType::Creeper},{"enderman",EType::Enderman},{"blaze",EType::Blaze},{"ender_dragon",EType::Dragon},{"end_crystal",EType::Crystal},{"boat",EType::Boat},{"oak_boat",EType::Boat},{"minecart",EType::Minecart},{"zombie",EType::Zombie},{"skeleton",EType::Skeleton},{"spider",EType::Spider},{"sheep",EType::Sheep},{"ghast",EType::Ghast},{"slime",EType::Slime},{"wolf",EType::Wolf},{"villager",EType::Villager}};
            if(a.size()<2){chat_.sys("Usage: /summon <cow|pig|chicken|creeper|enderman|blaze|ender_dragon|end_crystal|boat|minecart> [x y z]",now);return true;}
            std::string tn=a[1]; if(tn.compare(0,10,"minecraft:")==0)tn=tn.substr(10); EType ty=EType::Count; for(auto&k:kinds)if(k.first==tn)ty=k.second;
            if(ty==EType::Count){chat_.sys("Unknown entity: "+a[1],now);return true;}
            Vec3 p{pos_.x-std::sin(yaw_)*-2.5f,pos_.y,pos_.z+std::cos(yaw_)*-2.5f}; if(a.size()>=5){ try{ p=Vec3{std::stof(a[2])+0.5f,std::stof(a[3]),std::stof(a[4])+0.5f}; }catch(...){} }
            if(ty==EType::Dragon){ ents_.dragonSpawned=true; p.y+=25.f; } if(ty==EType::Ghast)p.y+=6.f;
            { Entity&ne=ents_.spawn(ty,p); if(ty==EType::Slime){ ne.variant=2; ne.w=ne.h=1.04f; ne.health=ne.maxHealth=4.f; } } chat_.sys("Summoned "+tn,now); return true; }
        if(cmd=="locate") {
            std::vector<std::string> A=a;
            // /locate structure <village|mineshaft|desert_pyramid|jungle_temple|stronghold|nether_fortress|bastion|end_city|ruined_portal|ancient_city|igloo|woodland_mansion|trial_chambers|shipwreck|...>: the terrain helper gives no structure positions,
            // so the nearest tell-tale block of that structure is searched for instead.  Biomes are not available at all.
            if(A.size()>=3&&A[1]=="biome"){chat_.sys("/locate biome is not available: the terrain helper does not expose biome data",now);return true;}
            if(A.size()>=3&&A[1]=="structure"){ static const std::vector<std::pair<std::string,std::string>> tells={{"village","bell"},{"mineshaft","rail"},{"desert_pyramid","chiseled_sandstone"},{"jungle_temple","mossy_cobblestone"},{"stronghold","end_portal_frame"},{"fortress","nether_bricks"},{"nether_fortress","nether_bricks"},{"bastion","polished_blackstone_bricks"},{"bastion_remnant","polished_blackstone_bricks"},{"end_city","purpur_block"},{"ruined_portal","crying_obsidian"},{"ancient_city","sculk_catalyst"},{"igloo","snow_block"},{"mansion","dark_oak_planks"},{"woodland_mansion","dark_oak_planks"},{"trial_chambers","trial_spawner"},{"monument","prismarine_bricks"},{"ocean_monument","prismarine_bricks"},{"pillager_outpost","dark_oak_log"},{"dungeon","spawner"},{"shipwreck","chest"}};
                std::string sn=A[2]; if(sn.compare(0,10,"minecraft:")==0)sn=sn.substr(10); std::string tell; for(auto&t:tells)if(t.first==sn)tell=t.second;
                if(tell.empty()){chat_.sys("Unknown structure: "+A[2],now);return true;}
                A[1]="block"; A[2]=tell; }
            if(A.size()<3||A[1]!="block"){chat_.sys("Usage: /locate block <block> [radius]   or   /locate structure <name> [radius]",now);return true;}
            if(locateFut_.valid()){chat_.sys("Still searching ...",now);return true;}
            const Registry&R=Registry::get(); std::string nm=A[2]; if(nm.compare(0,10,"minecraft:")==0)nm=nm.substr(10); const BlockId want=R.baseOf(R.blockIdOrZero(nm));
            if(!want){chat_.sys("Unknown block: "+A[2],now);return true;}
            int rad=24; if(A.size()>=4)parseInt(A[3],rad); rad=std::clamp(rad,1,96);
            const int pcx=(int)std::floor(pos_.x)>>4,pcz=(int)std::floor(pos_.z)>>4; const float px=pos_.x,pz=pos_.z; World*w=&world_;
            chat_.sys("Searching for "+nm+" within "+std::to_string(rad)+" chunks (terrain is generated as needed) ...",now);
            locateFut_=std::async(std::launch::async,[w,want,rad,pcx,pcz,px,pz,nm]()->std::string{
                const Registry&R=Registry::get();
                for(int r=0;r<=rad;r++)for(int dz=-r;dz<=r;dz++)for(int dx=-r;dx<=r;dx++){ if(std::max(std::abs(dx),std::abs(dz))!=r)continue;
                    const auto c=w->acquire(pcx+dx,pcz+dz,false); const int top=std::min(c->maxY,Chunk::HEIGHT-1); long best=-1; int bx=0,by=0,bz=0;
                    for(int y=0;y<=top;y++)for(int z=0;z<16;z++)for(int x=0;x<16;x++){ const uint16_t id=c->at(x,y,z); if(id&&R.baseOf(id)==want){ const int wx=(pcx+dx)*16+x,wz=(pcz+dz)*16+z; const long d=long(wx-px)*long(wx-px)+long(wz-pz)*long(wz-pz); if(best<0||d<best){best=d;bx=wx;by=y-Chunk::Y_OFFSET;bz=wz;} } }
                    if(best>=0)return "Nearest "+nm+" is at "+std::to_string(bx)+" "+std::to_string(by)+" "+std::to_string(bz)+" ("+std::to_string((int)std::sqrt((double)best))+" blocks away)";
                }
                return "No "+nm+" found within "+std::to_string(rad)+" chunks"; });
            return true;
        }

        // /give <item> [count]
        if(cmd=="give") {
            if(a.size()<2){chat_.sys("Usage: /give <item> [count]",now);return true;}
            const std::string& item=a[1]; int count=a.size()>=3?std::max(1,std::min(64,std::stoi(a[2]))):64;
            const ItemId id=Registry::get().itemByName(item);
            if(!id){chat_.sys("Unknown item: "+item,now);return true;}
            const int left=cur_.inv.add(id,count);
            chat_.sys("Gave "+std::to_string(count-left)+" "+item+(left>0?" (inventory full)":""),now); return true;
        }

        // /clear
        if(cmd=="clear") { for(auto& s:cur_.inv.s) s.clear(); chat_.sys("Cleared inventory",now); return true; }

        // /kill
        if(cmd=="kill") { health_=0; die(); chat_.sys("Killed.",now); return true; }
        if(cmd=="gc"||cmd=="memory") { cleanMemory(); chat_.sys("Memory cleaned ("+std::to_string(processRssMB())+" MB in use)",now); return true; }

        // /setblock <x> <y> <z> <block>
        if(cmd=="setblock") {
            if(a.size()<5){chat_.sys("Usage: /setblock <x> <y> <z> <block>",now);return true;}
            int x,y,z; if(!parseInt(a[1],x)||!parseInt(a[2],y)||!parseInt(a[3],z)){chat_.sys("Invalid coords",now);return true;}
            const std::string& bn=a[4]; const BlockId bid=(bn=="air"||bn=="minecraft:air")?0:Registry::get().blockByName(bn.compare(0,10,"minecraft:")==0?bn.substr(10):bn);
            int out[3][2]; int n=world_.set(x,y,z,bid,out); for(int i=0;i<n;i++) streamer_->markDirty(out[i][0],out[i][1]);
            chat_.sys("Set block at "+a[1]+" "+a[2]+" "+a[3]+" to "+bn,now); return true;
        }

        // /seed
        if(cmd=="seed"){chat_.sys("Seed: "+std::to_string((long long)(int64_t)cur_.seed),now);return true;}

        // /fly
        if(cmd=="fly"||cmd=="flying") {
            flying_=!flying_; vel_.y=0;
            chat_.sys(std::string("Flying: ")+(flying_?"ON":"OFF"),now); return true;
        }

        // /help
        if(cmd=="help"||cmd=="?") {
            const char* cmds[]={"/tp x y z","/gamemode <survival|creative|spectator>","/gamerule [rule] [val]","/time <set|add|query> [val]","/tick [query|rate|freeze|unfreeze|step]","/locate block <name> [radius]","/dimension <overworld|nether|end>","/give <item> [count]","/clear","/setblock x y z <block>","/kill","/seed","/fly"};
            for(auto c:cmds) chat_.sys(c,now); return true;
        }

        return false; // unhandled
    };
}

// ─── sound ────────────────────────────────────────────────────────────────────
void Game::initSounds() {
    { const auto sd=ensureDataFolder("sounds"); if(!sd.empty()){ Sound::get().init(sd); soundsLoaded_=true; return; } }   // data folder first (zip is unpacked on first start)
    // otherwise the old places next to the executable
    for(const char* p:{"sounds","assets/sounds","../sounds"}) {
        if(std::filesystem::exists(p)) { Sound::get().init(p); soundsLoaded_=true; return; }
    }
    // Sounds zip not extracted yet: try to extract if the zip is present
    std::cout<<"[Sound] Put hypervox_sounds.zip into the hypervox data folder and restart to enable audio\n";
}
// sound pack groups: dig.* (break/place) and step.* use cloth coral grass gravel sand snow stone wet_grass wood
std::string Game::soundGroup(BlockId b)const{
 const std::string n=Registry::get().blockName(Registry::get().baseOf(b)); auto has=[&](const char*p){return n.find(p)!=std::string::npos;};
 if(has("wool")||has("carpet")||has("_bed"))return "cloth"; if(has("coral"))return "coral";
 if(has("sand")&&!has("stone"))return "sand"; if(has("snow"))return "snow";
 if(has("gravel")||has("dirt")||has("clay")||has("mud")||has("podzol")||has("farmland")||has("mycelium"))return "gravel";
 if(has("grass")||has("leaves")||has("fern")||has("flower")||has("tulip")||has("dandelion")||has("poppy")||has("moss")||has("vine")||has("sapling")||has("mushroom")||has("sugar_cane")||has("orchid")||has("allium")||has("bluet")||has("daisy")||has("cornflower")||has("lily"))return "grass";
 if(has("planks")||has("_log")||has("_wood")||has("stem")||has("fence")||has("door")||has("chest")||has("crafting")||has("bookshelf")||has("barrel")||has("note_block")||has("jukebox")||has("sign")||has("ladder")||has("_slab")&&has("oak")||has("torch"))return "wood";
 return "stone";
}
void Game::blockSound(BlockId b,int kind){
 if(!soundsLoaded_||!b)return; const std::string n=Registry::get().blockName(Registry::get().baseOf(b)); const std::string g=soundGroup(b);
 if(kind==0&&n.find("glass")!=std::string::npos){ playSound("random.glass",0.9f,1.f); return; }
 if(kind==2||kind==3)playSound("step."+g,kind==3?0.6f:0.35f,kind==3?0.9f:1.f); else playSound("dig."+g,kind==0?0.9f:0.8f,kind==0?0.85f:1.f);
}
void Game::playSound(const std::string& ev, float vol, float pitch) {
    if(soundsLoaded_) Sound::get().play(ev,vol,pitch);
}
void Game::tickSounds(float dt) {
    if(!soundsLoaded_||!inWorld_) return;
    const Registry& R=Registry::get();
    const uint16_t atFeet=world_.get((int)std::floor(pos_.x),(int)std::floor(pos_.y+0.1f),(int)std::floor(pos_.z));
    const bool inWater=atFeet&&R.info(atFeet).fluid;
    if(inWater&&!sndWasWater_)playSound("liquid.splash",0.6f,1.f);
    sndWasWater_=inWater;
    if(!flying_&&cur_.mode!=MODE_SPECTATOR&&!inWater){      // footsteps (every ~1.6 blocks, softer when crouching) and the landing thud
     const uint16_t below=world_.get((int)std::floor(pos_.x),(int)std::floor(pos_.y-0.1f),(int)std::floor(pos_.z));
     if(onGround_){
      if(!sndWasGround_&&below)blockSound(below,3);
      const float dx=pos_.x-lastSndPos_.x,dz=pos_.z-lastSndPos_.z; stepDist_+=std::sqrt(dx*dx+dz*dz);
      if(stepDist_>1.6f){ stepDist_=0.f; if(below&&!glfwGetKey(win_,settings_.keys[ACT_DOWN]))blockSound(below,2); }
     }
     sndWasGround_=onGround_;
    }
    lastSndPos_=pos_;
    if(inWater){ Sound::get().loop("ambient.underwater.loop",0.6f); Sound::get().loop("ambient.underwater.underwater_ambience",0.5f); }
    else { Sound::get().stopLoop("ambient.underwater.loop"); Sound::get().stopLoop("ambient.underwater.underwater_ambience"); }
    if(inWorld_&&world_.dimension()==1){   // nether: low drone + random moods / additions
     Sound::get().loop("ambient.nether.nether_wastes.ambience",0.3f); static unsigned r=12345u; auto rnd=[&](){ r=r*1664525u+1013904223u; return float((r>>8)&0xFFFF)/65535.f; };
     if((ambMood_-=dt)<=0){ Sound::get().play("ambient.nether.nether_wastes.mood",0.7f,1.f); ambMood_=20.0+30.0*rnd(); }
     if((ambAdd_-=dt)<=0){ Sound::get().play("ambient.nether.nether_wastes.addition",0.6f,0.9f+0.2f*rnd()); ambAdd_=8.0+17.0*rnd(); } }
    else Sound::get().stopLoop("ambient.nether.nether_wastes.ambience");
    { const Registry&Rp=Registry::get(); const uint16_t pb=world_.get((int)std::floor(pos_.x),(int)std::floor(pos_.y+0.5f),(int)std::floor(pos_.z)); if(inWorld_&&Rp.isNetherPortal(pb))Sound::get().loop("portal.portal",0.5f); else Sound::get().stopLoop("portal.portal"); }
}
