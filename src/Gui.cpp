// Block GUIs: chest (27 slots), crafting table (3x3 + recipes), furnace (smelting + fuel).  Contents are saved next to the world.
#include "Game.hpp"
#include <random>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>

namespace{
// ------------------------------------------------------------------ recipes
struct Recipe{ std::vector<std::string> rows; std::map<char,std::string> key; std::vector<std::string> list; std::string out; int count=1; bool shapeless=false; };
bool ends(const std::string&s,const char*e){const size_t l=std::strlen(e);return s.size()>=l&&s.compare(s.size()-l,l,e)==0;}
bool nameIs(const std::string&have,const std::string&want){
 if(want.empty()||want[0]!='#')return have==want;
 if(want=="#planks")return ends(have,"_planks"); if(want=="#wool")return ends(have,"_wool"); if(want=="#coal")return have=="coal"||have=="charcoal";
 if(want=="#stone")return have=="cobblestone"||have=="cobbled_deepslate"||have=="blackstone"; if(want=="#sand")return have=="sand"||have=="red_sand";
 return false;
}
const std::vector<Recipe>& recipes(){
 static std::vector<Recipe> R; if(!R.empty())return R; const Registry&G=Registry::get();
 auto has=[&](const std::string&n){return G.itemByName(n)!=0||G.blockByName(n)!=0;};
 auto shaped=[&](std::vector<std::string> rows,std::map<char,std::string> key,const std::string&out,int count){ if(!has(out))return; for(auto&k:key)if(k.second[0]!='#'&&!has(k.second))return; Recipe r; r.rows=std::move(rows); r.key=std::move(key); r.out=out; r.count=count; R.push_back(std::move(r)); };
 auto less=[&](std::vector<std::string> list,const std::string&out,int count){ if(!has(out))return; for(auto&n:list)if(n[0]!='#'&&!has(n))return; Recipe r; r.shapeless=true; r.list=std::move(list); r.out=out; r.count=count; R.push_back(std::move(r)); };
 shaped({"P","P"},{{'P',"#planks"}},"stick",4); shaped({"PP","PP"},{{'P',"#planks"}},"crafting_table",1); shaped({"PPP","P P","PPP"},{{'P',"#planks"}},"chest",1);
 shaped({"SSS","S S","SSS"},{{'S',"#stone"}},"furnace",1); shaped({"C","S"},{{'C',"#coal"},{'S',"stick"}},"torch",4); shaped({"S S","SSS","S S"},{{'S',"stick"}},"ladder",3);
 shaped({"I I"," I "},{{'I',"iron_ingot"}},"bucket",1); shaped({" I","I "},{{'I',"iron_ingot"}},"shears",1); less({"iron_ingot","flint"},"flint_and_steel",1); shaped({"WWW"},{{'W',"wheat"}},"bread",1);
 shaped({"SS","SS"},{{'S',"#sand"}},"sandstone",1); shaped({"SS","SS"},{{'S',"stone"}},"stone_bricks",4); shaped({"BB","BB"},{{'B',"brick"}},"bricks",1); shaped({"GGG","GGG"},{{'G',"glass"}},"glass_pane",16); shaped({"III","III"},{{'I',"iron_ingot"}},"iron_bars",16);
 struct Mat{const char*pre,*m;}; for(Mat m:{Mat{"wooden","#planks"},Mat{"stone","#stone"},Mat{"iron","iron_ingot"},Mat{"golden","gold_ingot"},Mat{"diamond","diamond"}}){
  const std::string p=m.pre; std::map<char,std::string> k{{'M',m.m},{'S',"stick"}};
  shaped({"MMM"," S "," S "},k,p+"_pickaxe",1); shaped({"MM","MS"," S"},k,p+"_axe",1); shaped({"M","S","S"},k,p+"_shovel",1); shaped({"MM"," S"," S"},k,p+"_hoe",1); shaped({"M","M","S"},k,p+"_sword",1); }
 for(Mat m:{Mat{"leather","leather"},Mat{"iron","iron_ingot"},Mat{"golden","gold_ingot"},Mat{"diamond","diamond"}}){ const std::string p=m.pre; std::map<char,std::string> k{{'M',m.m}};
  shaped({"MMM","M M"},k,p+"_helmet",1); shaped({"M M","MMM","MMM"},k,p+"_chestplate",1); shaped({"MMM","M M","M M"},k,p+"_leggings",1); shaped({"M M","M M"},k,p+"_boots",1); }
 for(auto&pr:std::vector<std::pair<const char*,const char*>>{{"iron_ingot","iron_block"},{"gold_ingot","gold_block"},{"diamond","diamond_block"},{"emerald","emerald_block"},{"coal","coal_block"},{"lapis_lazuli","lapis_block"},{"redstone","redstone_block"},{"copper_ingot","copper_block"}}){
  shaped({"III","III","III"},{{'I',pr.first}},pr.second,1); less({pr.second},pr.first,9); }
 // generated from the block list: slabs, stairs, walls, fences, carpets, stained glass panes
 for(BlockId id=1;id<G.blockCount();id++){ if(G.baseOf(id)!=id)continue; const BlockId mat=G.materialOf(id); if(!mat)continue; const std::string n=G.blockName(id),mn=G.blockName(mat);
  if(ends(n,"_slab"))shaped({"MMM"},{{'M',mn}},n,6); else if(ends(n,"_stairs"))shaped({"M  ","MM ","MMM"},{{'M',mn}},n,4); else if(ends(n,"_wall"))shaped({"MMM","MMM"},{{'M',mn}},n,6);
  else if(ends(n,"_fence")&&n!="nether_brick_fence")shaped({"PSP","PSP"},{{'P',mn},{'S',"stick"}},n,3); else if(ends(n,"_carpet"))shaped({"MM"},{{'M',mn}},n,3); else if(ends(n,"_pane")&&n!="glass_pane")shaped({"MMM","MMM"},{{'M',mn}},n,16); }
 return R;
}
// result of the 3x3 grid (empty slot when nothing matches)
InvSlot craftResult(const InvSlot*g){
 const Registry&Rg=Registry::get(); std::string nm[9]; int cnt=0; for(int i=0;i<9;i++){ if(!g[i].empty()){ nm[i]=Rg.itemName(g[i].id); cnt++; } } if(!cnt)return InvSlot{};
 if(cnt==1){ for(int i=0;i<9;i++)if(!nm[i].empty()){ std::string s=nm[i]; if(s.compare(0,9,"stripped_")==0)s=s.substr(9);   // any log / stem -> 4 planks of that wood
   for(const char*suf:{"_log","_wood","_stem","_hyphae"})if(ends(s,suf)){ const std::string pl=s.substr(0,s.size()-std::strlen(suf))+"_planks"; if(const ItemId id=Rg.itemByName(pl))return InvSlot{id,4}; } } }
 int minx=3,miny=3,maxx=-1,maxy=-1; for(int i=0;i<9;i++)if(!nm[i].empty()){minx=std::min(minx,i%3);maxx=std::max(maxx,i%3);miny=std::min(miny,i/3);maxy=std::max(maxy,i/3);}
 const int w=maxx-minx+1,h=maxy-miny+1;
 for(const Recipe&r:recipes()){
  if(r.shapeless){ if((int)r.list.size()!=cnt)continue; std::vector<bool> used(9,false); bool ok=true; for(auto&want:r.list){ bool f=false; for(int i=0;i<9;i++)if(!used[i]&&!nm[i].empty()&&nameIs(nm[i],want)){used[i]=f=true;break;} if(!f){ok=false;break;} } if(ok)return InvSlot{Rg.itemByName(r.out)?Rg.itemByName(r.out):(ItemId)Rg.blockByName(r.out),r.count}; continue; }
  const int rh=(int)r.rows.size(),rw=(int)r.rows[0].size(); if(rw!=w||rh!=h)continue;
  for(int mirror=0;mirror<2;mirror++){ bool ok=true;
   for(int y=0;y<h&&ok;y++)for(int x=0;x<w&&ok;x++){ const char c=r.rows[y][mirror?w-1-x:x]; const std::string&have=nm[(miny+y)*3+minx+x]; if(c==' '){ if(!have.empty())ok=false; } else { auto it=r.key.find(c); if(it==r.key.end()||have.empty()||!nameIs(have,it->second))ok=false; } }
   if(ok){ const ItemId id=Rg.itemByName(r.out)?Rg.itemByName(r.out):(ItemId)Rg.blockByName(r.out); return InvSlot{id,r.count}; } }
 }
 return InvSlot{};
}
// ------------------------------------------------------------------ furnace
std::string smeltOf(const std::string&n){
 static const std::map<std::string,std::string> t={{"iron_ore","iron_ingot"},{"deepslate_iron_ore","iron_ingot"},{"raw_iron","iron_ingot"},{"gold_ore","gold_ingot"},{"deepslate_gold_ore","gold_ingot"},{"raw_gold","gold_ingot"},{"copper_ore","copper_ingot"},{"deepslate_copper_ore","copper_ingot"},{"raw_copper","copper_ingot"},
  {"diamond_ore","diamond"},{"deepslate_diamond_ore","diamond"},{"coal_ore","coal"},{"deepslate_coal_ore","coal"},{"lapis_ore","lapis_lazuli"},{"deepslate_lapis_ore","lapis_lazuli"},{"redstone_ore","redstone"},{"deepslate_redstone_ore","redstone"},{"emerald_ore","emerald"},
  {"sand","glass"},{"red_sand","glass"},{"cobblestone","stone"},{"stone","smooth_stone"},{"clay_ball","brick"},{"clay","terracotta"},{"netherrack","nether_brick"},{"sandstone","smooth_sandstone"},{"stone_bricks","cracked_stone_bricks"},{"cactus","green_dye"},{"kelp","dried_kelp"},
  {"beef","cooked_beef"},{"porkchop","cooked_porkchop"},{"chicken","cooked_chicken"},{"mutton","cooked_mutton"},{"rabbit","cooked_rabbit"},{"cod","cooked_cod"},{"salmon","cooked_salmon"},{"potato","baked_potato"},{"wet_sponge","sponge"}};
 auto it=t.find(n); if(it!=t.end())return it->second; if(ends(n,"_log")||ends(n,"_wood")||ends(n,"_stem"))return "charcoal"; return std::string();
}
float fuelSeconds(const std::string&n){
 if(n=="coal"||n=="charcoal")return 80.f; if(n=="coal_block")return 800.f; if(n=="lava_bucket")return 1000.f; if(n=="blaze_rod")return 120.f; if(n=="stick"||n.find("sapling")!=std::string::npos)return 5.f;
 if(ends(n,"_planks")||ends(n,"_log")||ends(n,"_wood")||ends(n,"_stem")||n.find("fence")!=std::string::npos||n=="chest"||n=="crafting_table"||n=="bookshelf")return 15.f; if(ends(n,"_slab")&&n.find("stone")==std::string::npos&&n.find("brick")==std::string::npos)return 7.5f;
 if(n.compare(0,7,"wooden_")==0)return 10.f; if(ends(n,"_wool")||ends(n,"_carpet"))return 5.f; return 0.f;
}
}

namespace{
// window layouts of the remaining block GUIs: texture, title, window height, y of the inventory rows, slot positions.  Slots are plain storage.
struct GuiDef{ const char*block,*tex,*title; int h,invY; std::vector<std::pair<int,int>> slots; };
const GuiDef* guiDef(const std::string&n){
 static std::vector<GuiDef> D; if(D.empty()){
  std::vector<std::pair<int,int>> nine; for(int r=0;r<3;r++)for(int q=0;q<3;q++)nine.push_back({62+18*q,17+18*r});
  D.push_back({"dispenser","dispenser","Dispenser",166,84,nine}); D.push_back({"dropper","dispenser","Dropper",166,84,nine});
  D.push_back({"hopper","hopper","Item Hopper",133,51,{{44,20},{62,20},{80,20},{98,20},{116,20}}});
  D.push_back({"enchanting_table","enchanting_table","Enchant",166,84,{{15,47},{35,47}}}); D.push_back({"enchanting_table_block","enchanting_table","Enchant",166,84,{{15,47},{35,47}}});
  for(const char*a:{"anvil","chipped_anvil","damaged_anvil"})D.push_back({a,"anvil","Repair & Name",166,84,{{27,47},{76,47},{134,47}}});
  D.push_back({"brewing_stand","brewing_stand","Brewing Stand",166,84,{{56,51},{79,58},{102,51},{79,17},{17,17}}});
  D.push_back({"grindstone","grindstone","Repair & Disenchant",166,84,{{49,19},{49,40},{129,34}}});
  D.push_back({"loom","loom","Loom",166,84,{{13,26},{33,26},{23,45},{143,57}}});
  D.push_back({"smithing_table","smithing","Upgrade Gear",166,84,{{8,48},{26,48},{44,48},{98,48}}});
  D.push_back({"cartography_table","cartography_table","Cartography Table",166,84,{{15,15},{15,52},{143,57}}});
  D.push_back({"stonecutter","stonecutter","Stonecutter",166,84,{{20,33},{143,33}}}); }
 for(auto&d:D)if(n==d.block)return &d; return nullptr;
}
bool isGenericChest(const std::string&n){ return n=="chest"||n=="trapped_chest"||n=="ender_chest"||ends(n,"copper_chest")||n=="barrel"||n=="shulker_box"||ends(n,"_shulker_box"); }
}
// ------------------------------------------------------------------ open / close
// Chests that came with the generated terrain (villages, dungeons, mineshafts ...) have no contents in the chunk data we receive, and the real loot tables are not
// available here, so they get a generic random loot the first time they are opened (seeded by world seed + position, so it is stable).  Chests you place stay empty.
void Game::markPlayerChest(int x,int y,int z){ containers_[ckey(x,y,z)].kind=0; }
// chests the terrain generator just placed are remembered as "sealed" (kind 4, saved with the containers) until their first opening
void Game::pollLootChests(){ for(const auto&p:world_.takeNewLootChests()){ auto&c=containers_[ckey(p[0],p[1],p[2])]; if(c.kind==0&&!c.s[0].empty())continue; if(c.kind==0)c.kind=4; } }
// Which structure is a chest in?  The terrain data does not say, so it is worked out from the blocks around it (workstations, rails, sandstone, mossy cobblestone ...).
static std::string classifyChest(const World&w,int x,int y,int z){
 const Registry&R=Registry::get(); std::map<std::string,int> n; int doors=0,planks=0,sandy=0,sandstone=0,deep=0;
 for(int dy=-4;dy<=5;dy++)for(int dz=-7;dz<=7;dz++)for(int dx=-7;dx<=7;dx++){ const uint16_t id=w.get(x+dx,y+dy,z+dz); if(!id)continue; const std::string&nm=R.blockName(R.baseOf(id)); n[nm]++;
  if(nm.size()>5&&nm.compare(nm.size()-5,5,"_door")==0)doors++; if(nm.find("_planks")!=std::string::npos)planks++; if(nm=="sand"||nm=="gravel")sandy++; if(nm.find("sandstone")!=std::string::npos)sandstone++; if(nm.find("deepslate")!=std::string::npos)deep++; }
 auto c=[&](const char*k){ auto it=n.find(k); return it==n.end()?0:it->second; };
 if(c("trial_spawner")||(c("tuff_bricks")>8&&c("copper_bulb")+c("copper_grate")>0))return "trial_chambers";
 if(c("sculk")+c("sculk_vein")+c("sculk_catalyst")>3||(deep>40&&c("soul_lantern")>0))return "ancient_city";
 if(c("spawner")&&c("mossy_cobblestone")>10)return "dungeon";
 if(c("rail")+c("powered_rail")>2||(c("cobweb")>1&&c("oak_fence")>2))return "mineshaft";
 if(sandstone>25&&(c("orange_terracotta")+c("blue_terracotta")+c("chiseled_sandstone")>2))return "desert_pyramid";
 if(c("mossy_cobblestone")>14&&(c("cobblestone")>10||c("chiseled_stone_bricks")>0))return "jungle_temple";
 if(c("purpur_block")+c("purpur_pillar")+c("end_rod")>6)return "end_city";
 if(c("nether_bricks")>8)return "nether_fortress"; if(c("blackstone")+c("polished_blackstone_bricks")>20)return "bastion";
 if(c("obsidian")+c("crying_obsidian")>2||(c("netherrack")>10&&c("magma_block")+c("gold_block")>0))return "ruined_portal";
 if(c("dark_oak_planks")>30&&c("cobblestone")>25)return "woodland_mansion";
 if(c("stone_bricks")+c("mossy_stone_bricks")+c("cracked_stone_bricks")>30)return "stronghold";
 if(c("snow_block")>12&&(c("white_carpet")>0||c("ice")+c("packed_ice")>3))return "igloo";
 if(c("water")>25&&planks>8)return "shipwreck";
 if(c("dark_oak_log")>8&&c("cobblestone")>6)return "pillager_outpost";
 // villages: the workstation next to the chest says which house it is
 if(c("grindstone"))return "village_weaponsmith"; if(c("smithing_table"))return "village_toolsmith"; if(c("blast_furnace"))return "village_armorer"; if(c("smoker"))return "village_butcher";
 if(c("stonecutter"))return "village_mason"; if(c("loom"))return "village_shepherd"; if(c("fletching_table"))return "village_fletcher"; if(c("cartography_table"))return "village_cartographer";
 if(c("brewing_stand"))return "village_temple"; if(c("cauldron"))return "village_tannery"; if(c("barrel")&&c("water")>0)return "village_fisher"; if(c("composter")||c("hay_block")||c("farmland"))return "village_farmer";
 if(doors>0||c("bell")||c("lectern")||c("white_bed")+c("red_bed")>0||(c("dirt_path")>3&&planks>6))return "village_house";
 if(sandy>40&&planks==0&&c("water")<40)return "buried_treasure";
 if(c("cobblestone")+c("mossy_cobblestone")>25&&planks==0)return "dungeon";
 return "";   // not part of any structure we recognise: no loot
}
void Game::fillChestLoot(Container&c,int x,int y,int z){
 struct L{const char*item;int lo,hi,w;}; struct T{const char*key;int r0,r1;std::vector<L> v;};
 static const std::vector<T> tables={
  {"dungeon",2,4,{{"saddle",1,1,20},{"golden_apple",1,1,15},{"enchanted_golden_apple",1,1,2},{"music_disc_13",1,1,15},{"music_disc_cat",1,1,15},{"name_tag",1,1,20},{"golden_horse_armor",1,1,10},{"iron_horse_armor",1,1,15},{"diamond_horse_armor",1,1,5},{"book",1,1,10},{"bucket",1,1,10},{"redstone",1,4,10},{"coal",1,4,15},{"bread",1,1,20},{"wheat",1,4,20},{"bone",1,8,10},{"gunpowder",1,4,10},{"string",1,4,10},{"melon_seeds",2,4,10},{"pumpkin_seeds",2,4,10},{"beetroot_seeds",2,4,10}}},
  {"village_house",3,8,{{"dandelion",1,1,5},{"poppy",1,1,5},{"potato",1,7,10},{"bread",1,4,4},{"apple",1,5,10},{"book",1,1,1},{"feather",1,1,1},{"emerald",1,4,2},{"wheat_seeds",1,5,5},{"oak_sapling",1,2,5},{"iron_ingot",1,3,2},{"coal",1,3,2}}},
  {"village_weaponsmith",3,8,{{"diamond",1,3,3},{"iron_ingot",1,5,10},{"gold_ingot",1,3,5},{"bread",1,3,15},{"apple",1,3,15},{"iron_pickaxe",1,1,5},{"iron_sword",1,1,5},{"iron_chestplate",1,1,1},{"iron_helmet",1,1,1},{"iron_leggings",1,1,1},{"iron_boots",1,1,1},{"obsidian",3,7,5},{"oak_sapling",3,7,5},{"saddle",1,1,3},{"iron_horse_armor",1,1,1},{"golden_horse_armor",1,1,1},{"diamond_horse_armor",1,1,1}}},
  {"village_toolsmith",3,8,{{"diamond",1,3,1},{"iron_ingot",1,5,5},{"gold_ingot",1,3,1},{"bread",1,3,15},{"iron_pickaxe",1,1,5},{"coal",1,3,1},{"stick",1,3,20},{"iron_shovel",1,1,5}}},
  {"village_armorer",1,5,{{"iron_ingot",1,3,2},{"bread",1,4,4},{"iron_helmet",1,1,1},{"emerald",1,1,1}}},
  {"village_butcher",3,8,{{"emerald",1,4,1},{"porkchop",1,3,6},{"wheat",1,3,3},{"beef",1,3,3},{"mutton",1,3,1},{"coal",1,3,3}}},
  {"village_mason",3,8,{{"clay_ball",1,3,1},{"flower_pot",1,1,1},{"stone",1,1,2},{"stone_bricks",1,2,2},{"yellow_dye",1,1,2},{"smooth_stone",1,1,1},{"emerald",1,1,1}}},
  {"village_shepherd",3,8,{{"white_wool",1,8,6},{"black_wool",1,3,1},{"gray_wool",1,3,2},{"brown_wool",1,3,2},{"light_gray_wool",1,3,2},{"emerald",1,1,1},{"shears",1,1,1},{"wheat",1,6,6}}},
  {"village_fletcher",3,8,{{"arrow",1,3,2},{"feather",1,3,6},{"egg",1,3,2},{"flint",1,6,6},{"stick",1,6,6},{"emerald",1,1,1}}},
  {"village_cartographer",1,5,{{"paper",1,5,15},{"compass",1,1,5},{"bread",1,4,15},{"stick",1,2,5}}},
  {"village_temple",3,8,{{"rotten_flesh",1,4,7},{"lapis_lazuli",1,10,1},{"redstone",1,4,1},{"gold_ingot",1,4,1},{"emerald",1,4,1},{"bread",1,4,7}}},
  {"village_tannery",3,8,{{"leather",1,3,6},{"leather_chestplate",1,1,2},{"leather_boots",1,1,2},{"leather_helmet",1,1,2},{"leather_leggings",1,1,2},{"saddle",1,1,1},{"bread",1,4,5}}},
  {"village_farmer",3,8,{{"wheat",1,5,10},{"potato",1,5,5},{"carrot",1,5,5},{"beetroot",1,5,5},{"bread",1,4,5},{"emerald",1,1,1}}},
  {"village_fisher",3,8,{{"cod",1,5,5},{"salmon",1,5,2},{"water_bucket",1,1,1},{"coal",1,3,2},{"emerald",1,1,1}}},
  {"mineshaft",3,6,{{"golden_apple",1,1,20},{"enchanted_golden_apple",1,1,1},{"name_tag",1,1,30},{"book",1,1,10},{"iron_ingot",1,5,10},{"gold_ingot",1,3,5},{"redstone",4,9,5},{"lapis_lazuli",4,9,5},{"diamond",1,2,3},{"coal",3,8,10},{"bread",1,3,15},{"melon_seeds",2,4,10},{"pumpkin_seeds",2,4,10},{"beetroot_seeds",2,4,10},{"rail",4,8,20},{"powered_rail",1,2,5},{"detector_rail",1,2,5},{"activator_rail",1,2,5},{"torch",1,16,15},{"glow_berries",3,6,10}}},
  {"desert_pyramid",3,6,{{"diamond",1,3,5},{"iron_ingot",1,5,15},{"gold_ingot",2,7,15},{"emerald",1,3,15},{"bone",4,6,25},{"spider_eye",1,3,25},{"rotten_flesh",3,7,25},{"saddle",1,1,20},{"iron_horse_armor",1,1,15},{"golden_horse_armor",1,1,10},{"diamond_horse_armor",1,1,5},{"book",1,1,20},{"golden_apple",1,1,20},{"enchanted_golden_apple",1,1,2}}},
  {"jungle_temple",2,6,{{"diamond",1,3,3},{"iron_ingot",1,5,10},{"gold_ingot",2,7,15},{"bamboo",1,3,15},{"emerald",1,3,2},{"bone",4,6,20},{"rotten_flesh",3,7,16},{"saddle",1,1,3},{"iron_horse_armor",1,1,1},{"golden_horse_armor",1,1,1},{"diamond_horse_armor",1,1,1},{"book",1,1,1}}},
  {"stronghold",2,3,{{"ender_pearl",1,1,10},{"iron_ingot",1,5,50},{"gold_ingot",1,3,5},{"redstone",4,9,5},{"bread",1,3,30},{"apple",1,3,15},{"iron_pickaxe",1,1,5},{"iron_sword",1,1,5},{"iron_chestplate",1,1,5},{"iron_helmet",1,1,5},{"iron_leggings",1,1,5},{"iron_boots",1,1,5},{"golden_apple",1,1,1},{"saddle",1,1,1},{"iron_horse_armor",1,1,1},{"book",1,1,1}}},
  {"shipwreck",3,10,{{"paper",1,12,8},{"potato",1,6,7},{"moss_block",1,4,7},{"poisonous_potato",1,6,7},{"carrot",4,8,7},{"wheat",8,21,7},{"coal",2,8,6},{"rotten_flesh",5,24,5},{"gunpowder",1,3,3},{"tnt",1,2,3},{"leather_helmet",1,1,3},{"leather_chestplate",1,1,3},{"leather_leggings",1,1,3},{"leather_boots",1,1,3},{"book",1,1,1},{"iron_nugget",1,10,2},{"gold_nugget",1,10,2},{"emerald",1,1,1},{"bamboo",1,5,2}}},
  {"buried_treasure",3,5,{{"iron_ingot",1,4,20},{"gold_ingot",1,4,10},{"tnt",1,2,5},{"diamond",1,2,5},{"prismarine_crystals",1,5,5},{"leather_chestplate",1,1,5},{"iron_sword",1,1,5},{"cooked_cod",2,4,15},{"cooked_salmon",2,4,15}}},
  {"ruined_portal",4,8,{{"obsidian",1,2,40},{"flint",1,4,40},{"iron_nugget",9,18,40},{"flint_and_steel",1,1,40},{"fire_charge",1,1,40},{"golden_apple",1,1,15},{"gold_nugget",4,24,15},{"golden_sword",1,1,15},{"golden_axe",1,1,15},{"golden_hoe",1,1,15},{"golden_shovel",1,1,15},{"golden_pickaxe",1,1,15},{"golden_boots",1,1,15},{"golden_chestplate",1,1,15},{"golden_helmet",1,1,15},{"golden_leggings",1,1,15},{"gold_ingot",2,8,5},{"clock",1,1,5},{"bell",1,1,5},{"enchanted_golden_apple",1,1,1},{"gold_block",1,2,1}}},
  {"woodland_mansion",1,3,{{"lead",1,1,20},{"golden_apple",1,1,15},{"enchanted_golden_apple",1,1,2},{"music_disc_13",1,1,15},{"music_disc_cat",1,1,15},{"name_tag",1,1,20},{"chainmail_chestplate",1,1,10},{"diamond_hoe",1,1,15},{"diamond_chestplate",1,1,5},{"book",1,1,10},{"bread",1,1,20},{"wheat",1,4,20},{"bucket",1,1,10},{"redstone",1,4,15},{"coal",1,4,15},{"melon_seeds",2,4,10},{"pumpkin_seeds",2,4,10},{"beetroot_seeds",2,4,10},{"bone",1,8,10},{"gunpowder",1,8,10},{"rotten_flesh",1,8,10},{"string",1,8,10}}},
  {"igloo",2,8,{{"apple",1,3,15},{"coal",1,4,15},{"gold_nugget",1,3,10},{"stone_axe",1,1,2},{"rotten_flesh",1,1,10},{"emerald",1,1,1},{"wheat",2,3,10}}},
  {"nether_fortress",2,4,{{"diamond",1,3,5},{"iron_ingot",1,5,5},{"gold_ingot",1,3,15},{"golden_sword",1,1,5},{"golden_chestplate",1,1,5},{"flint_and_steel",1,1,5},{"nether_wart",3,7,5},{"saddle",1,1,10},{"golden_horse_armor",1,1,8},{"iron_horse_armor",1,1,5},{"diamond_horse_armor",1,1,3},{"obsidian",2,4,2}}},
  {"bastion",3,4,{{"gold_ingot",3,9,10},{"iron_nugget",5,10,5},{"ancient_debris",1,2,2},{"netherite_scrap",1,1,1},{"crying_obsidian",3,8,5},{"golden_axe",1,1,6},{"arrow",5,17,10},{"spectral_arrow",10,25,5},{"gilded_blackstone",5,12,6},{"cooked_porkchop",2,5,5},{"golden_carrot",6,17,4},{"diamond",1,3,1}}},
  {"end_city",2,6,{{"diamond",2,7,5},{"iron_ingot",4,8,8},{"gold_ingot",2,7,3},{"emerald",2,6,2},{"beetroot_seeds",1,10,5},{"saddle",1,1,3},{"iron_horse_armor",1,1,1},{"golden_horse_armor",1,1,1},{"diamond_horse_armor",1,1,1},{"diamond_sword",1,1,3},{"diamond_boots",1,1,3},{"diamond_chestplate",1,1,3},{"diamond_leggings",1,1,3},{"diamond_helmet",1,1,3},{"diamond_pickaxe",1,1,3},{"diamond_shovel",1,1,3},{"iron_sword",1,1,3},{"iron_chestplate",1,1,3}}},
  {"ancient_city",5,10,{{"enchanted_golden_apple",1,1,1},{"music_disc_otherside",1,1,4},{"compass",1,1,2},{"sculk_catalyst",1,2,2},{"note_block",1,1,2},{"bone",1,15,4},{"soul_torch",1,15,3},{"coal",6,15,7},{"diamond",1,1,3},{"diamond_hoe",1,1,1},{"iron_leggings",1,1,3},{"golden_apple",1,1,2},{"lead",1,1,2},{"experience_bottle",1,3,5}}},
  {"trial_chambers",2,3,{{"arrow",2,8,5},{"baked_potato",1,2,5},{"bread",1,2,5},{"emerald",1,3,3},{"iron_ingot",1,3,3},{"golden_apple",1,1,1},{"diamond",1,1,1},{"bone",1,3,3}}},
  {"pillager_outpost",2,3,{{"wheat",3,5,7},{"potato",2,5,5},{"carrot",3,5,5},{"dark_oak_log",1,3,2},{"crossbow",1,1,1},{"experience_bottle",1,3,3},{"iron_ingot",1,3,2}}} };
 const Registry&R=Registry::get(); const std::string key=classifyChest(world_,x,y,z); if(key.empty())return; const T*tb=&tables[0]; for(const T&t:tables)if(key==t.key){tb=&t;break;}
 std::mt19937_64 rng(cur_.seed*0x9E3779B97F4A7C15ull^(uint64_t(uint32_t(x))<<40)^(uint64_t(uint32_t(z))<<16)^uint64_t(uint32_t(y)));
 auto put=[&](ItemId id,int cnt){ for(int tries=0;tries<40;tries++){ const int slot=int(rng()%27); if(c.s[slot].empty()){ c.s[slot]=InvSlot{id,cnt}; return; } } };
 auto itemOf=[&](const char*nm){ return R.itemByName(nm)?R.itemByName(nm):(ItemId)R.blockByName(nm); };
 if(key=="buried_treasure"){ if(const ItemId h=itemOf("heart_of_the_sea"))put(h,1); }
 int total=0; for(auto&e:tb->v)total+=e.w; const int rolls=tb->r0+int(rng()%uint64_t(tb->r1-tb->r0+1));
 for(int i=0;i<rolls;i++){ int r=int(rng()%uint64_t(total)); const L*pick=&tb->v[0]; for(auto&e:tb->v){ if(r<e.w){pick=&e;break;} r-=e.w; } const ItemId id=itemOf(pick->item); if(!id)continue; put(id,pick->lo+int(rng()%uint64_t(pick->hi-pick->lo+1))); }
}
bool Game::tryOpenGui(int x,int y,int z){
 const uint16_t id=world_.get(x,y,z); if(!id)return false; const std::string n=Registry::get().blockName(Registry::get().baseOf(id)); Screen s;
 if(n=="ender_chest"){ s=Screen::Chest; }   // shared per-player inventory, see guiContainer()
 else if(isGenericChest(n)){ s=Screen::Chest; const uint64_t ck=ckey(x,y,z); pollLootChests(); auto it=containers_.find(ck); if(it!=containers_.end()&&it->second.kind==4){ it->second.kind=0; fillChestLoot(it->second,x,y,z); } else containers_[ck].kind=0; }   // loot only for terrain chests that were generated and never opened (kind 4)
 else if(n=="crafting_table")s=Screen::Crafting;
 else if(n=="furnace"||n=="blast_furnace"||n=="smoker"){ s=Screen::Furnace; auto&c=containers_[ckey(x,y,z)]; c.kind=1; }
 else if(guiDef(n)){ s=Screen::Generic; auto&c=containers_[ckey(x,y,z)]; c.kind=2; }
 else return false;
 guiKey_=(n=="ender_chest")?kEnderKey:ckey(x,y,z); guiBlock_=n; screen_=s; captureMouse(false); carried_.clear(); playSound(isGenericChest(n)?"random.chestopen":"random.click",0.8f,1.f); return true;
}
void Game::closeGui(){
 for(auto&g:craft_)if(!g.empty()){ if(cur_.inv.add(g.id,g.count)>0){} g.clear(); }   // crafting grid goes back to your inventory
 if(!carried_.empty()&&cur_.mode!=MODE_CREATIVE)cur_.inv.add(carried_.id,carried_.count);
 carried_.clear(); if(isGenericChest(guiBlock_))playSound("random.chestclosed",0.8f,1.f); screen_=Screen::Play; captureMouse(true); markWorldDirty();
}
void Game::dropContainer(int x,int y,int z){
 auto it=containers_.find(ckey(x,y,z)); if(it==containers_.end())return;
 for(auto&s:it->second.s)if(!s.empty())cur_.inv.add(s.id,s.count);   // no item entities yet: the contents go straight into your inventory
 containers_.erase(it); markWorldDirty();
}
// ------------------------------------------------------------------ shared slot UI
void Game::guiPlayerSlots(float px,float py,float y0,std::vector<std::pair<InvSlot*,std::pair<float,float>>>&out){
 for(int i=0;i<36;i++){ const float x=px+8+(i<9?i:(i-9)%9)*18,y=i<9?py+y0+58:py+y0+((i-9)/9)*18; out.push_back({&cur_.inv.s[i],{x,y}}); }
}
// draws the slots, handles clicks (outputIdx = take-only slot, craftOut = that slot is the crafting result)
void Game::guiSlotsUi(std::vector<std::pair<InvSlot*,std::pair<float,float>>>&slots,int outputIdx,bool craftOut){
 const UiInput&in=ui_.input(); int hov=-1;
 for(size_t i=0;i<slots.size();i++){ const float x=slots[i].second.first,y=slots[i].second.second; if(ui_.hover(x,y,16,16))hov=(int)i; }
 for(size_t i=0;i<slots.size();i++){ const float x=slots[i].second.first,y=slots[i].second.second; if((int)i==hov)ui_.sprite(uiAtlas_.get("gui/sprites/container/slot_highlight_back"),x-4,y-4,24,24); drawSlot(*slots[i].first,x,y); if((int)i==hov)ui_.sprite(uiAtlas_.get("gui/sprites/container/slot_highlight_front"),x-4,y-4,24,24); }
 // ---- drag: hold a button with a stack on the cursor and sweep over slots (left = spread evenly, right = one each); released over a single slot it is a normal click
 auto compat=[&](int i){ InvSlot*p=slots[i].first; return i!=outputIdx&&(p->empty()||p->id==carried_.id); };
 if(dragBtn_){
  const bool held=dragBtn_==1?in.ldown:in.rdown;
  if(held&&!carried_.empty()){
   if(hov>=0&&compat(hov)){ InvSlot*p=slots[hov].first; if(std::find(dragPtrs_.begin(),dragPtrs_.end(),p)==dragPtrs_.end()&&(int)dragPtrs_.size()<carried_.count)dragPtrs_.push_back(p); }
   for(size_t i=0;i<slots.size();i++)if(std::find(dragPtrs_.begin(),dragPtrs_.end(),slots[i].first)!=dragPtrs_.end())ui_.rect(slots[i].second.first,slots[i].second.second,16,16,rgba(255,255,255,96));
  } else {
   if(dragPtrs_.size()==1){ clickSlot(*dragPtrs_[0],dragBtn_==1,dragBtn_==2); }
   else if(dragPtrs_.size()>1&&!carried_.empty()){
    const int n=(int)dragPtrs_.size(),mx=Registry::get().maxStack(carried_.id),per=dragBtn_==1?std::max(1,carried_.count/n):1;
    for(InvSlot*p:dragPtrs_){ if(carried_.count<=0)break; const int have=p->empty()?0:p->count; const int give=std::min({per,mx-have,carried_.count}); if(give<=0)continue; if(p->empty())*p=InvSlot{carried_.id,give}; else p->count+=give; carried_.count-=give; }
    if(carried_.count<=0)carried_.clear(); playSound("random.pop",0.4f,1.f);
   }
   markWorldDirty(); dragBtn_=0; dragPtrs_.clear();
  }
 }
 if(!dragBtn_&&hov>=0&&(in.lclick||in.rclick)&&!in.shift&&hov!=outputIdx&&!carried_.empty()&&compat(hov)){
  if(in.lclick&&in.time-lastClickT_<0.25&&hov==lastClickHov_){   // double click: gather every matching item onto the cursor
   const int mx=Registry::get().maxStack(carried_.id);
   for(size_t i=0;i<slots.size();i++){ if((int)i==outputIdx)continue; InvSlot*p=slots[i].first; if(p->empty()||p->id!=carried_.id)continue; const int m=std::min(p->count,mx-carried_.count); if(m<=0)continue; carried_.count+=m; p->count-=m; if(p->count<=0)p->clear(); }
   lastClickT_=-1; markWorldDirty();
  } else { dragBtn_=in.lclick?1:2; dragPtrs_.assign(1,slots[hov].first); lastClickT_=in.time; lastClickHov_=hov; }
 } else if(hov>=0&&(in.lclick||in.rclick)){
  if(in.lclick){ lastClickT_=in.time; lastClickHov_=hov; }
  InvSlot&s=*slots[hov].first;
  if(hov==outputIdx){   // take-only
   if(!s.empty()&&(carried_.empty()||(carried_.id==s.id&&carried_.count+s.count<=Registry::get().maxStack(s.id)))){
    if(carried_.empty())carried_=s; else carried_.count+=s.count;
    if(craftOut){ for(auto&g:craft_)if(!g.empty()&&--g.count<=0)g.clear(); } else s.clear();
    playSound("random.pop",0.5f,1.f); markWorldDirty(); }
  } else if(in.shift&&in.lclick&&!s.empty()){   // quick move between the container and your inventory
   const bool fromPlayer=slots[hov].first>=&cur_.inv.s[0]&&slots[hov].first<=&cur_.inv.s[35];
   if(fromPlayer){ for(size_t i=0;i<slots.size()&&!s.empty();i++){ InvSlot*d=slots[i].first; if(d>=&cur_.inv.s[0]&&d<=&cur_.inv.s[35])continue; if((int)i==outputIdx)continue; if(d->empty()){*d=s;s.clear();} else if(d->id==s.id){const int m=std::min(s.count,Registry::get().maxStack(s.id)-d->count);d->count+=m;s.count-=m;if(s.count<=0)s.clear();} } }
   else{ const int left=cur_.inv.add(s.id,s.count); if(left<=0)s.clear(); else s.count=left; }
   markWorldDirty();
  } else { clickSlot(s,in.lclick,in.rclick); markWorldDirty(); }
 } else if((in.lclick||in.rclick)&&!carried_.empty()){ /* click outside: keep holding */ }
 if(hov>=0&&carried_.empty()&&!slots[hov].first->empty())ui_.tooltip(Registry::get().item(slots[hov].first->id).display);
 if(!carried_.empty())drawSlot(carried_,ui_.mx()-8,ui_.my()-8);
}
// ------------------------------------------------------------------ toasts + recipe book
namespace{
int countOf(const Inventory&inv,const std::string&want){ int n=0; const Registry&R=Registry::get(); for(auto&s:inv.s)if(!s.empty()&&nameIs(R.itemName(s.id),want))n+=s.count; return n; }
bool canCraftRecipe(const Inventory&inv,const Recipe&r){
 std::map<std::string,int> need; if(r.shapeless){ for(auto&n:r.list)need[n]++; } else for(auto&row:r.rows)for(char c:row)if(c!=' '){ auto it=r.key.find(c); if(it!=r.key.end())need[it->second]++; }
 for(auto&kv:need)if(countOf(inv,kv.first)<kv.second)return false; return true; }
ItemId outItem(const Recipe&r){ const Registry&R=Registry::get(); return R.itemByName(r.out)?R.itemByName(r.out):(ItemId)R.blockByName(r.out); }
}
void Game::pushToast(const std::string&title,const std::string&line,ItemId icon,int kind){ toasts_.push_back(Toast{title,line,icon,kind,time_}); if(toasts_.size()>4)toasts_.pop_front(); }
void Game::drawToasts(){
 while(!toasts_.empty()&&time_-toasts_.front().t0>5.4)toasts_.pop_front();
 float y=4; const float W=ui_.W();
 for(auto&t:toasts_){ const double age=time_-t.t0; float slide=age<0.35?float(1.0-age/0.35):(age>5.0?float((age-5.0)/0.4):0.f); slide=slide*slide;
  const float x=W-160+slide*164; const char*sp=t.kind==0?"gui/sprites/toast/recipe":t.kind==1?"gui/sprites/toast/advancement":"gui/sprites/toast/system";
  const Sprite&s=uiAtlas_.get(sp); if(s.valid())ui_.sprite(s,x,y,160,32); else { ui_.rect(x,y,160,32,0xFF1A1A1Au); ui_.rect(x,y,160,1,0xFFA0A0A0u); ui_.rect(x,y+31,160,1,0xFFA0A0A0u); }
  const bool icon=t.icon!=0; const float tx=x+(icon?30.f:18.f);
  if(t.kind==2){ ui_.text(t.title,tx,y+7,rgba(255,255,85)); ui_.text(t.line,tx,y+18,COL_WHITE); }
  else { ui_.text(t.title,tx,y+7,t.kind==1?rgba(255,255,85):rgba(160,80,160),false); ui_.text(t.line,tx,y+18,t.kind==1?COL_WHITE:rgba(0,0,0),false); }
  if(icon)ui_.icon(t.icon,x+8,y+8); y+=32; }
}
void Game::loadRecipeBook(const std::string&folder){
 unlocked_.clear(); unlockedInit_=false; const auto f=saves_.regionDir(folder).parent_path()/"hypervox_recipes.txt"; std::ifstream in(f); std::string l; bool any=false; while(std::getline(in,l)){ if(!l.empty()){unlocked_.insert(l);any=true;} } unlockedInit_=any; bookOpen_=false; bookScroll_=0; nextUnlockScan_=0; toasts_.clear();
}
void Game::saveRecipeBook(){ if(cur_.folder.empty())return; const auto f=saves_.regionDir(cur_.folder).parent_path()/"hypervox_recipes.txt"; std::ofstream o(f,std::ios::trunc); for(auto&n:unlocked_)o<<n<<"\n"; }
// a recipe unlocks as soon as you hold one of its ingredients; the first scan of a world is silent for what you already carry
void Game::updateRecipeUnlocks(){
 if(time_<nextUnlockScan_)return; nextUnlockScan_=time_+0.6; const Registry&R=Registry::get(); std::vector<std::string> fresh;
 std::set<std::string> have; for(auto&s:cur_.inv.s)if(!s.empty())have.insert(R.itemName(s.id));
 for(const Recipe&r:recipes()){ if(unlocked_.count(r.out))continue; bool hit=false;
  auto test=[&](const std::string&w){ for(auto&h:have)if(nameIs(h,w)){hit=true;break;} };
  if(r.shapeless)for(auto&n:r.list)test(n); else for(auto&kv:r.key)test(kv.second);
  if(hit)fresh.push_back(r.out); }
 if(fresh.empty()){ unlockedInit_=true; return; }
 for(auto&n:fresh)unlocked_.insert(n);
 if(unlockedInit_){ const ItemId ic=R.itemByName(fresh[0])?R.itemByName(fresh[0]):(ItemId)R.blockByName(fresh[0]); std::string disp=ic?R.item(ic).display:fresh[0]; if(fresh.size()>1)disp+=" +"+std::to_string(fresh.size()-1); pushToast("Recipe Unlocked!",disp,ic,0); }
 unlockedInit_=true; saveRecipeBook();
}
// button next to the grid + a panel (left of the window) with every unlocked recipe; click one to fill the grid from your inventory
void Game::drawRecipeBook(float px,float py,bool big){
 const UiInput&in=ui_.input(); const Registry&R=Registry::get();
 const float bx=px+(big?5.f:104.f),by=py+(big?35.f:61.f); const bool hov=ui_.hover(bx,by,20,18);
 { const Sprite&s=uiAtlas_.get(hov?"gui/sprites/recipe_book/button_highlighted":"gui/sprites/recipe_book/button"); if(s.valid())ui_.sprite(s,bx,by,20,18); else { ui_.rect(bx,by,20,18,hov?0xFF8B8B8Bu:0xFF555555u); ui_.text("R",bx+7,by+5); } }
 if(hov&&in.lclick){ bookOpen_=!bookOpen_; playSound("random.click",0.6f,1.f); }
 if(!bookOpen_)return;
 const float qx=px-148,qy=py; const Sprite&bg=uiAtlas_.get("gui/recipe_book");
 if(bg.valid())ui_.region(bg,1,1,147,166,qx,qy,147,166); else { ui_.rect(qx,qy,147,166,0xFFC6C6C6u); ui_.rect(qx+2,qy+2,143,162,0xFF8B8B8Bu); }
 ui_.text("Recipes",qx+10,qy+8,COL_WHITE,false);
 std::vector<const Recipe*> list; for(const Recipe&r:recipes())if(unlocked_.count(r.out)&&(big||(r.shapeless?r.list.size()<=4:(r.rows.size()<=2&&r.rows[0].size()<=2))))list.push_back(&r);
 std::stable_sort(list.begin(),list.end(),[&](const Recipe*a,const Recipe*b){return canCraftRecipe(cur_.inv,*a)>canCraftRecipe(cur_.inv,*b);});   // craftable first
 const int cols=5,rows=4,total=(int)list.size(),maxScroll=std::max(0,(total+cols-1)/cols-rows); if(in.wheel!=0.f&&ui_.hover(qx,qy,147,166))bookScroll_=std::clamp(bookScroll_-(in.wheel>0?1:-1),0,maxScroll); bookScroll_=std::clamp(bookScroll_,0,maxScroll);
 if(list.empty()){ ui_.text("Pick up items to",qx+14,qy+60,COL_DARK,false); ui_.text("unlock recipes",qx+14,qy+72,COL_DARK,false); }
 std::string tip; bool tipMissing=false;
 for(int i=bookScroll_*cols;i<total&&i<(bookScroll_+rows)*cols;i++){ const int k=i-bookScroll_*cols; const float x=qx+11+(k%cols)*25,y=qy+31+(k/cols)*25; const Recipe&r=*list[i]; const bool can=canCraftRecipe(cur_.inv,r);
  const Sprite&sl=uiAtlas_.get(can?"gui/sprites/recipe_book/slot_craftable":"gui/sprites/recipe_book/slot_uncraftable"); if(sl.valid())ui_.sprite(sl,x,y,25,25); else ui_.rect(x,y,25,25,can?rgba(106,143,85):rgba(143,85,85));
  const ItemId ic=outItem(r); if(ic)ui_.icon(ic,x+5,y+5);
  if(ui_.hover(x,y,25,25)){ tip=ic?R.item(ic).display:r.out; tipMissing=!can;
   if(in.lclick&&can){   // fill the grid: ingredients come out of your inventory
    for(auto&g:craft_)if(!g.empty()){ cur_.inv.add(g.id,g.count); g.clear(); }
    auto take=[&](const std::string&want)->InvSlot{ for(auto&s:cur_.inv.s)if(!s.empty()&&nameIs(R.itemName(s.id),want)){ InvSlot o{s.id,1}; if(--s.count<=0)s.clear(); return o; } return InvSlot{}; };
    if(r.shapeless){ int n=0; static const int cells[9]={0,1,3,4,2,5,6,7,8}; for(auto&w:r.list)craft_[cells[n++%9]]=take(w); }
    else for(size_t yy=0;yy<r.rows.size();yy++)for(size_t xx=0;xx<r.rows[yy].size();xx++){ const char c=r.rows[yy][xx]; if(c==' ')continue; auto it=r.key.find(c); if(it!=r.key.end())craft_[yy*3+xx]=take(it->second); }
    markWorldDirty(); playSound("random.click",0.6f,1.f); }
   } }
 if(!tip.empty())ui_.tooltip(tipMissing?tip+" (missing ingredients)":tip);   // after every slot, so nothing is drawn over it
}

// ------------------------------------------------------------------ screens
// 2x2 crafting grid of the survival inventory (top-left corner of the 3x3 grid, so every recipe that fits 2x2 works) + its result slot
void Game::guiInventoryCraft(float px,float py){
 static const int cell[4]={0,1,3,4}; InvSlot result=craftResult(craft_); std::vector<std::pair<InvSlot*,std::pair<float,float>>> slots;
 for(int i=0;i<4;i++)slots.push_back({&craft_[cell[i]],{px+98+(i%2)*18,py+18+(i/2)*18}});
 slots.push_back({&result,{px+154,py+28}}); slots.push_back({&cur_.inv.off,{px+77,py+62}}); guiSlotsUi(slots,4,true);
}
void Game::screenChest(){
 drawBackground(true); const float W=ui_.W(),H=ui_.H(),px=std::floor((W-176)/2),py=std::floor((H-168)/2);
 const Sprite&sp=uiAtlas_.get("gui/container/generic_54"); ui_.region(sp,0,0,176,71,px,py,176,71); ui_.region(sp,0,126,176,96,px,py+71,176,96);
 ui_.text(guiBlock_=="ender_chest"?"Ender Chest":guiBlock_=="barrel"?"Barrel":guiBlock_.find("shulker")!=std::string::npos?"Shulker Box":"Chest",px+8,py+6,COL_DARK,false); ui_.text("Inventory",px+8,py+74,COL_DARK,false);
 auto&c=guiContainer(); std::vector<std::pair<InvSlot*,std::pair<float,float>>> slots;
 for(int i=0;i<27;i++)slots.push_back({&c.s[i],{px+8+(i%9)*18,py+18+(i/9)*18}}); guiPlayerSlots(px,py,85,slots);
 guiSlotsUi(slots,-1,false);
}
void Game::screenCrafting(){
 drawBackground(true); const float W=ui_.W(),H=ui_.H(),px=std::floor((W-176)/2),py=std::floor((H-166)/2);
 ui_.region(uiAtlas_.get("gui/container/crafting_table"),0,0,176,166,px,py,176,166); ui_.text("Crafting",px+28,py+6,COL_DARK,false); ui_.text("Inventory",px+8,py+72,COL_DARK,false);
 InvSlot result=craftResult(craft_); std::vector<std::pair<InvSlot*,std::pair<float,float>>> slots;
 for(int i=0;i<9;i++)slots.push_back({&craft_[i],{px+30+(i%3)*18,py+17+(i/3)*18}});
 slots.push_back({&result,{px+124,py+35}}); const int outIdx=(int)slots.size()-1; guiPlayerSlots(px,py,84,slots);
 guiSlotsUi(slots,outIdx,true); drawRecipeBook(px,py,true);
}
void Game::screenGeneric(){
 const GuiDef*d=guiDef(guiBlock_); if(!d){closeGui();return;}
 drawBackground(true); const float W=ui_.W(),H=ui_.H(),px=std::floor((W-176)/2),py=std::floor((H-float(d->h))/2);
 ui_.region(uiAtlas_.get(std::string("gui/container/")+d->tex),0,0,176,d->h,px,py,176,d->h); ui_.text(d->title,px+8,py+6,COL_DARK,false); ui_.text("Inventory",px+8,py+float(d->invY)-10,COL_DARK,false);
 auto&c=guiContainer(); std::vector<std::pair<InvSlot*,std::pair<float,float>>> slots;
 for(size_t i=0;i<d->slots.size()&&i<27;i++)slots.push_back({&c.s[i],{px+float(d->slots[i].first),py+float(d->slots[i].second)}}); guiPlayerSlots(px,py,float(d->invY),slots);
 guiSlotsUi(slots,-1,false);
}
void Game::screenFurnace(){
 drawBackground(true); const float W=ui_.W(),H=ui_.H(),px=std::floor((W-176)/2),py=std::floor((H-166)/2);
 const char*tex=guiBlock_=="blast_furnace"?"gui/container/blast_furnace":guiBlock_=="smoker"?"gui/container/smoker":"gui/container/furnace";
 ui_.region(uiAtlas_.get(tex),0,0,176,166,px,py,176,166); ui_.text(guiBlock_=="blast_furnace"?"Blast Furnace":guiBlock_=="smoker"?"Smoker":"Furnace",px+60,py+6,COL_DARK,false);
 auto&c=guiContainer();
 if(c.burnMax>0.f&&c.burn>0.f){ const Sprite&fl=uiAtlas_.get("gui/sprites/container/furnace/lit_progress"); const float f=c.burn/c.burnMax,hgt=std::ceil(14.f*f); ui_.region(fl,0,14.f-hgt,14,hgt,px+56,py+36+14-hgt,14,hgt); }
 if(c.cook>0.f){ const Sprite&ar=uiAtlas_.get("gui/sprites/container/furnace/burn_progress"); const float f=std::min(1.f,c.cook/10.f); ui_.region(ar,0,0,24.f*f,16,px+79,py+34,24.f*f,16); }
 std::vector<std::pair<InvSlot*,std::pair<float,float>>> slots;
 slots.push_back({&c.s[0],{px+56,py+17}}); slots.push_back({&c.s[1],{px+56,py+53}}); slots.push_back({&c.s[2],{px+116,py+35}}); guiPlayerSlots(px,py,84,slots);
 guiSlotsUi(slots,2,false);
}
// furnaces keep smelting while you are away from the screen
void Game::tickFurnaces(float dt){
 const Registry&R=Registry::get();
 for(auto&kv:containers_){ Container&c=kv.second; if(c.kind!=1)continue;
  const std::string in=c.s[0].empty()?std::string():R.itemName(c.s[0].id); const std::string outName=in.empty()?std::string():smeltOf(in); const ItemId outId=outName.empty()?0:R.itemByName(outName);
  const bool canSmelt=outId&&(c.s[2].empty()||(c.s[2].id==outId&&c.s[2].count<R.maxStack(outId)));
  if(c.burn<=0.f&&canSmelt&&!c.s[1].empty()){ const float f=fuelSeconds(R.itemName(c.s[1].id)); if(f>0.f){ c.burn=c.burnMax=f; const ItemId fid=c.s[1].id; if(R.itemName(fid)=="lava_bucket"){c.s[1].id=R.itemByName("bucket");} else if(--c.s[1].count<=0)c.s[1].clear(); } }
  if(c.burn>0.f){ c.burn-=dt; if(canSmelt){ c.cook+=dt; if(c.cook>=10.f){ c.cook=0.f; if(c.s[2].empty())c.s[2]=InvSlot{outId,1}; else c.s[2].count++; if(--c.s[0].count<=0)c.s[0].clear(); } } else c.cook=std::max(0.f,c.cook-dt*2.f); }
  else c.cook=std::max(0.f,c.cook-dt*2.f);
  if(c.burn<0.f)c.burn=0.f; }
}
// ------------------------------------------------------------------ persistence (text file next to the world)
void Game::saveContainers(const std::unordered_map<uint64_t,Container>&m,const std::filesystem::path&file)const{
 std::ofstream f(file,std::ios::trunc); if(!f)return; const Registry&R=Registry::get();
 for(auto&kv:m){ const Container&c=kv.second; int n=0; for(auto&s:c.s)if(!s.empty())n++; if(!n&&c.burn<=0.f&&c.kind!=4)continue;
  f<<kv.first<<' '<<c.kind<<' '<<c.burn<<' '<<c.burnMax<<' '<<c.cook<<' '<<n; for(int i=0;i<27;i++)if(!c.s[i].empty())f<<' '<<i<<' '<<R.itemName(c.s[i].id)<<' '<<c.s[i].count; f<<'\n'; }
}
void Game::loadContainers(const std::string&folder,int dim){
 { std::ifstream ef(saves_.regionDir(folder).parent_path()/"hypervox_enderchest.txt"); if(ef){ const Registry&R0=Registry::get(); std::string ln; if(std::getline(ef,ln)){ std::istringstream is(ln); uint64_t k; Container c; int n=0; if(is>>k>>c.kind>>c.burn>>c.burnMax>>c.cook>>n){ for(int i=0;i<n;i++){ int slot,count; std::string name; if(!(is>>slot>>name>>count))break; ItemId id=R0.itemByName(name); if(!id)id=(ItemId)R0.blockByName(name); if(id&&slot>=0&&slot<27)c.s[slot]=InvSlot{id,count}; } c.kind=0; ender_=c; } } } else if(dim==0&&ender_.s[0].empty()){ /* new world: stays empty */ } }
 std::ifstream f(saves_.regionDir(folder).parent_path()/(dim==1?"hypervox_containers_nether.txt":dim==2?"hypervox_containers_end.txt":"hypervox_containers.txt")); if(!f)return; const Registry&R=Registry::get(); std::string line;
 while(std::getline(f,line)){ std::istringstream is(line); uint64_t k; Container c; int n=0; if(!(is>>k>>c.kind>>c.burn>>c.burnMax>>c.cook>>n))continue;
  for(int i=0;i<n;i++){ int slot,count; std::string name; if(!(is>>slot>>name>>count))break; ItemId id=R.itemByName(name); if(!id)id=(ItemId)R.blockByName(name); if(id&&slot>=0&&slot<27)c.s[slot]=InvSlot{id,count}; }
  containers_[k]=c; }
}
