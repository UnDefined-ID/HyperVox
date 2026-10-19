// level.dat + region/*.mca in the real Minecraft 1.21.11 format (DataVersion 4671)
#include "SaveGame.hpp"
#include "Anvil.hpp"
#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
namespace fs=std::filesystem;
static constexpr int kDataVersion=4671;
static std::string mcName(const std::string&n){return "minecraft:"+(n=="enchanting_table_block"?std::string("enchanting_table"):n);}
static std::string ourName(std::string n){ if(n.compare(0,10,"minecraft:")==0)n=n.substr(10); if(n=="enchanting_table")n="enchanting_table_block"; return n; }
static const float kPi=3.14159265f;

bool SaveManager::readLevel(const fs::path&dir,WorldInfo&w) const{
 std::vector<uint8_t> raw,nbt; Nbt root; if(!readFile((dir/"level.dat").string(),raw)||!inflateAny(raw.data(),raw.size(),nbt)||!nbtRead(nbt.data(),nbt.size(),root))return false;
 const Nbt*d=root.get("Data"); if(!d)return false;
 w=WorldInfo{}; w.folder=dir.filename().string();
 if(auto*n=d->get("LevelName"))w.name=n->s;
 const Nbt*gs=d->get("WorldGenSettings"); if(!gs)gs=d->get("world_gen_settings");   // 1.21.x / 26.x renamed the compound
 if(auto*g=gs){ if(auto*sd=g->get("seed"))w.seed=(uint64_t)sd->i; } else if(auto*sd=d->get("RandomSeed"))w.seed=(uint64_t)sd->i;
 if(auto*n=d->get("GameType"))w.mode=n->i==1?MODE_CREATIVE:(n->i==3?MODE_SPECTATOR:MODE_SURVIVAL);
 if(auto*n=d->get("LastPlayed"))w.lastPlayed=n->i/1000;
 if(auto*fv=d->get("FastVoxel")){ if(auto*n=fv->get("Created"))w.created=n->i; if(auto*n=fv->get("Flying"))w.flying=n->i!=0; if(auto*n=fv->get("Debug"))w.debug=n->i!=0; }
 if(auto*p=d->get("Player")){
  if(auto*n=p->get("Health"))w.health=(float)n->d; if(auto*n=p->get("foodLevel"))w.food=(int)n->i; if(auto*n=p->get("foodSaturationLevel"))w.sat=(float)n->d; if(auto*n=p->get("foodExhaustionLevel"))w.exh=(float)n->d;
  const Nbt*pos=p->get("Pos"),*rot=p->get("Rotation");
  if(pos&&pos->list.size()==3){w.px=(float)pos->list[0].d;w.py=(float)pos->list[1].d+float(Chunk::Y_OFFSET);w.pz=(float)pos->list[2].d;w.hasPlayer=true;}
  if(rot&&rot->list.size()==2){w.yaw=(float)rot->list[0].d*kPi/180.f+kPi;w.pitch=(float)rot->list[1].d*kPi/180.f;}
  if(auto*n=p->get("SelectedItemSlot"))w.inv.selected=std::clamp((int)n->i,0,8);
  if(auto*inv=p->get("Inventory"))for(auto&e:inv->list){
   const Nbt*sl=e.get("Slot"),*id=e.get("id"),*ct=e.get("count"); if(!ct)ct=e.get("Count"); if(!sl||!id||!ct)continue;
   const int idx=(int)sl->i; const ItemId it=Registry::get().itemByName(ourName(id->s)); if(it&&ct->i>0){ InvSlot*dst=(idx>=0&&idx<36)?&w.inv.s[idx]:(idx>=100&&idx<104?&w.inv.armor[103-idx]:nullptr); if(dst){dst->id=it;dst->count=std::min((int)ct->i,Registry::get().maxStack(it));} }
  }
 }
 if(w.name.empty())w.name=w.folder;
 if(!w.created)w.created=w.lastPlayed;
 return true;
}

bool SaveManager::saveLevel(const WorldInfo&w) const{
 const fs::path dir=m_root/w.folder; std::error_code ec; fs::create_directories(dir/"region",ec);
 const bool cr=w.mode==MODE_CREATIVE; Nbt data=Nbt::Comp_();
 data.set("LevelName",Nbt::Str_(w.name)); data.set("DataVersion",Nbt::Int_(kDataVersion)); data.set("version",Nbt::Int_(19133));
 { Nbt v=Nbt::Comp_(); v.set("Id",Nbt::Int_(kDataVersion)); v.set("Name",Nbt::Str_("1.21.11")); v.set("Series",Nbt::Str_("main")); v.set("Snapshot",Nbt::Byte_(0)); data.set("Version",std::move(v)); }
 data.set("GameType",Nbt::Int_(w.mode==MODE_SPECTATOR?3:(cr?1:0))); data.set("Difficulty",Nbt::Byte_(2)); data.set("hardcore",Nbt::Byte_(0)); data.set("allowCommands",Nbt::Byte_(cr?1:0)); data.set("initialized",Nbt::Byte_(1));
 data.set("LastPlayed",Nbt::Long_(w.lastPlayed*1000)); data.set("Time",Nbt::Long_(0)); data.set("DayTime",Nbt::Long_(1000));
 data.set("SpawnX",Nbt::Int_(0)); data.set("SpawnY",Nbt::Int_(80)); data.set("SpawnZ",Nbt::Int_(0)); data.set("SpawnAngle",Nbt::Float_(0));
 data.set("raining",Nbt::Byte_(0)); data.set("thundering",Nbt::Byte_(0));
 { Nbt g=Nbt::Comp_(); g.set("seed",Nbt::Long_((int64_t)w.seed)); g.set("generate_features",Nbt::Byte_(1)); g.set("bonus_chest",Nbt::Byte_(0));
   Nbt dims=Nbt::Comp_(),ow=Nbt::Comp_(),gen=Nbt::Comp_(),bs=Nbt::Comp_();
   bs.set("type",Nbt::Str_("minecraft:multi_noise")); bs.set("preset",Nbt::Str_("minecraft:overworld"));
   gen.set("type",Nbt::Str_("minecraft:noise")); gen.set("settings",Nbt::Str_("minecraft:overworld")); gen.set("biome_source",std::move(bs));
   ow.set("type",Nbt::Str_("minecraft:overworld")); ow.set("generator",std::move(gen)); dims.set("minecraft:overworld",std::move(ow)); g.set("dimensions",std::move(dims)); data.set("WorldGenSettings",std::move(g)); }
 { Nbt fv=Nbt::Comp_(); fv.set("Created",Nbt::Long_(w.created)); fv.set("Flying",Nbt::Byte_(w.flying?1:0)); fv.set("Debug",Nbt::Byte_(w.debug?1:0)); data.set("FastVoxel",std::move(fv)); }
 if(w.hasPlayer){
  Nbt p=Nbt::Comp_(); Nbt pos=Nbt::List_(Nbt::Double); pos.list={Nbt::Double_(w.px),Nbt::Double_(w.py-double(Chunk::Y_OFFSET)),Nbt::Double_(w.pz)}; p.set("Pos",std::move(pos));
  Nbt rot=Nbt::List_(Nbt::Float); rot.list={Nbt::Float_((w.yaw-kPi)*180.f/kPi),Nbt::Float_(w.pitch*180.f/kPi)}; p.set("Rotation",std::move(rot));
  Nbt mot=Nbt::List_(Nbt::Double); mot.list={Nbt::Double_(0),Nbt::Double_(0),Nbt::Double_(0)}; p.set("Motion",std::move(mot));
  p.set("Health",Nbt::Float_(w.health)); p.set("foodLevel",Nbt::Int_(w.food)); p.set("foodSaturationLevel",Nbt::Float_(w.sat)); p.set("foodExhaustionLevel",Nbt::Float_(w.exh)); p.set("playerGameType",Nbt::Int_(w.mode==MODE_SPECTATOR?3:(cr?1:0))); p.set("SelectedItemSlot",Nbt::Int_(w.inv.selected)); p.set("Dimension",Nbt::Str_("minecraft:overworld")); p.set("Health",Nbt::Float_(20)); p.set("foodLevel",Nbt::Int_(20)); p.set("DataVersion",Nbt::Int_(kDataVersion));
  Nbt inv=Nbt::List_(Nbt::Comp);
  for(int i=0;i<36;i++)if(!w.inv.s[i].empty()){Nbt e=Nbt::Comp_();e.set("Slot",Nbt::Byte_(i));e.set("id",Nbt::Str_(mcName(Registry::get().itemName(w.inv.s[i].id))));e.set("count",Nbt::Int_(w.inv.s[i].count));inv.list.push_back(std::move(e));}
  for(int i=0;i<4;i++)if(!w.inv.armor[i].empty()){Nbt e=Nbt::Comp_();e.set("Slot",Nbt::Byte_(103-i));e.set("id",Nbt::Str_(mcName(Registry::get().itemName(w.inv.armor[i].id))));e.set("count",Nbt::Int_(w.inv.armor[i].count));inv.list.push_back(std::move(e));}
  p.set("Inventory",std::move(inv));
  Nbt ab=Nbt::Comp_(); ab.set("flying",Nbt::Byte_(w.flying?1:0)); ab.set("mayfly",Nbt::Byte_(cr?1:0)); ab.set("instabuild",Nbt::Byte_(cr?1:0)); ab.set("invulnerable",Nbt::Byte_(cr?1:0)); p.set("abilities",std::move(ab));
  data.set("Player",std::move(p));
 }
 Nbt root=Nbt::Comp_(); root.set("Data",std::move(data));
 std::vector<uint8_t> raw,gz; nbtWrite(raw,root); if(!gzipCompress(raw,gz,true))return false;
 return writeFileAtomic((dir/"level.dat").string(),gz);
}

