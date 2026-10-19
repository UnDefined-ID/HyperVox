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

        // /tick <rate|freeze|unfreeze|step>
        if(cmd=="tick") {
            if(a.size()<2){chat_.sys("tickRate="+std::to_string((int)tickRate_),now);return true;}
            if(a[1]=="freeze"){tickRate_=0;chat_.sys("Game frozen",now);}
            else if(a[1]=="unfreeze"){tickRate_=20;chat_.sys("Game unpaused at 20 tps",now);}
            else if(a[1]=="step"){worldTime_++;chat_.sys("Stepped 1 tick",now);}
            else if(a[1]=="rate"||a[1]=="query"){
                if(a.size()>=3){float v;if(parseFloat(a[2],v)){tickRate_=std::clamp(v,0.f,1000.f);chat_.sys("Tick rate: "+a[2],now);}else chat_.sys("Invalid",now);}
                else chat_.sys("tickRate="+std::to_string((int)tickRate_),now);
            }
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
            const char* cmds[]={"/tp x y z","/gamemode <survival|creative|spectator>","/gamerule [rule] [val]","/time <set|add|query> [val]","/tick [rate|freeze|unfreeze|step]","/give <item> [count]","/clear","/setblock x y z <block>","/kill","/seed","/fly"};
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
    std::cout<<"[Sound] Put minecraft-sounds.zip into the hypervox data folder and restart to enable audio\n";
}
void Game::playSound(const std::string& ev, float vol, float pitch) {
    if(soundsLoaded_) Sound::get().play(ev,vol,pitch);
}
void Game::tickSounds(float) {
    if(!soundsLoaded_||!inWorld_) return;
    const Registry& R=Registry::get();
    const uint16_t atFeet=world_.get((int)std::floor(pos_.x),(int)std::floor(pos_.y+0.1f),(int)std::floor(pos_.z));
    const bool inWater=atFeet&&R.info(atFeet).fluid;
    if(inWater) Sound::get().loop("ambient.underwater.loop",0.6f);
    else        Sound::get().stopLoop("ambient.underwater.loop");
}
