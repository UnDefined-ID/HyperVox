#include "Mining.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace{
bool has(const std::string&s,const char*p){return s.find(p)!=std::string::npos;}
bool anyOf(const std::string&n,std::initializer_list<const char*> l){for(const char*p:l)if(has(n,p))return true;return false;}
float hardness(const std::string&n){
 static const struct{const char*n;float h;} ex[]={{"stone",1.5f},{"cobblestone",2.f},{"mossy_cobblestone",2.f},{"deepslate",3.f},{"cobbled_deepslate",3.5f},{"bedrock",-1.f},{"obsidian",50.f},{"crying_obsidian",50.f},
  {"netherrack",0.4f},{"end_stone",3.f},{"glass",0.3f},{"dirt",0.5f},{"coarse_dirt",0.5f},{"grass_block",0.6f},{"podzol",0.5f},{"mycelium",0.6f},{"sand",0.5f},{"red_sand",0.5f},{"gravel",0.6f},{"clay",0.6f},
  {"snow_block",0.2f},{"ice",0.5f},{"packed_ice",0.5f},{"blue_ice",2.8f},{"sandstone",0.8f},{"red_sandstone",0.8f},{"bricks",2.f},{"stone_bricks",1.5f},{"bookshelf",1.5f},{"tnt",0.f},{"sponge",0.6f},
  {"iron_block",5.f},{"gold_block",3.f},{"diamond_block",5.f},{"emerald_block",5.f},{"coal_block",5.f},{"lapis_block",3.f},{"redstone_block",5.f},{"copper_block",3.f},{"netherite_block",50.f},{"furnace",3.5f},
  {"crafting_table",2.5f},{"chest",2.5f},{"anvil",5.f},{"enchanting_table_block",5.f},{"water",-1.f},{"lava",-1.f},{"cactus",0.4f},{"hay_block",0.5f},{"melon",1.f},{"pumpkin",1.f},{"missing",1.f}};
 for(auto&e:ex)if(n==e.n)return e.h;
 if(has(n,"planks"))return 2.f; if(has(n,"_log")||has(n,"_wood")||has(n,"stem")||has(n,"hyphae"))return 2.f; if(has(n,"leaves"))return 0.2f; if(has(n,"wool"))return 0.8f;
 if(has(n,"concrete_powder"))return 0.5f; if(has(n,"concrete"))return 1.8f; if(has(n,"terracotta"))return 1.25f;
 if(has(n,"ore"))return has(n,"deepslate")?4.5f:3.f;
 if(has(n,"deepslate"))return 3.f; if(has(n,"basalt"))return 1.25f; if(has(n,"quartz"))return 0.8f; if(has(n,"copper"))return 3.f;
 if(has(n,"glass"))return 0.3f; if(has(n,"sand"))return 0.5f; if(has(n,"dirt")||has(n,"mud"))return 0.5f; if(has(n,"snow"))return 0.2f; if(has(n,"ice"))return 0.5f;
 return 1.5f;
}
enum Tool{T_NONE,T_PICK,T_AXE,T_SHOVEL,T_HOE,T_SHEARS};
Tool bestTool(const std::string&n){
 if(has(n,"concrete_powder")||anyOf(n,{"dirt","grass_block","sand","gravel","clay","soul_sand","soul_soil","snow_block","mud","mycelium","podzol","farmland"})&&!has(n,"sandstone"))return T_SHOVEL;
 if(anyOf(n,{"leaves","hay_block","sponge","sculk","nether_wart_block","shroomlight","moss_block"}))return T_HOE;
 if(anyOf(n,{"planks","_log","_wood","stem","hyphae","bookshelf","chest","crafting_table","pumpkin","melon","barrel","note_block","jukebox","bamboo"}))return T_AXE;
 if(has(n,"concrete_powder"))return T_SHOVEL;
 if(anyOf(n,{"stone","cobble","deepslate","brick","ore","iron","gold","diamond","emerald","lapis","coal_block","redstone_block","copper","netherite","obsidian","netherrack","end_stone","terracotta","concrete","sandstone","ice","prismarine","quartz","basalt","tuff","andesite","diorite","granite","blackstone","furnace","anvil","amethyst","calcite","dripstone","purpur","glazed","enchanting_table"}))return T_PICK;
 return T_NONE;
}
int requiredTier(const std::string&n){    // 0 wood/gold, 1 stone/copper, 2 iron, 3 diamond
 if(anyOf(n,{"obsidian","netherite","ancient_debris"}))return 3;
 if(anyOf(n,{"diamond","gold","redstone","emerald"})&&has(n,"ore"))return 2; if(anyOf(n,{"diamond_block","emerald_block","gold_block","redstone_block"}))return 2;
 if(anyOf(n,{"iron","lapis","copper"})&&(has(n,"ore")||has(n,"_block")))return 1;
 return 0;
}
bool needsTool(Tool t,const std::string&n){ return t==T_PICK&&!has(n,"concrete_powder"); }   // stone-like blocks only drop when mined with a pickaxe
}
MineInfo mineInfo(const Registry&R,BlockId block,ItemId held){
 MineInfo m; if(R.info(block).shape==255||R.info(block).flatIcon){m.seconds=0.f;return m;}   // plants / torches break instantly
 const BlockId mat=R.materialOf(R.baseOf(block)); const std::string n=R.blockName(mat?mat:block); const float h=hardness(n);
 if(h<0.f){m.unbreakable=true;m.seconds=1e9f;m.canHarvest=false;return m;}
 const Tool want=bestTool(n); Tool have=T_NONE; int tier=-1; float speed=1.f;
 if(held){ const std::string t=R.itemName(held); const int tierOf=has(t,"wooden")?0:has(t,"golden")?0:has(t,"stone_")?1:has(t,"copper")?1:has(t,"iron")?2:has(t,"diamond")?3:has(t,"netherite")?4:-1;
  const float sp=has(t,"wooden")?2.f:has(t,"golden")?12.f:has(t,"stone_")?4.f:has(t,"copper")?5.f:has(t,"iron")?6.f:has(t,"diamond")?8.f:has(t,"netherite")?9.f:1.f;
  if(t.size()>=8&&t.compare(t.size()-8,8,"_pickaxe")==0)have=T_PICK; else if(t.size()>4&&t.compare(t.size()-4,4,"_axe")==0)have=T_AXE; else if(t.size()>7&&t.compare(t.size()-7,7,"_shovel")==0)have=T_SHOVEL; else if(t.size()>4&&t.compare(t.size()-4,4,"_hoe")==0)have=T_HOE; else if(t=="shears")have=T_SHEARS;
  if(have!=T_NONE&&have!=T_SHEARS){tier=tierOf;speed=sp;} else if(have==T_SHEARS){ if(has(n,"wool"))speed=5.f; else if(has(n,"leaves"))speed=15.f; else have=T_NONE; } }
 if(want!=T_NONE&&have==want&&have!=T_SHEARS){} else if(!(have==T_SHEARS)) speed=1.f;
 if(has(n,"wool")&&have==T_SHEARS)speed=5.f; if(has(n,"leaves")&&have==T_SHEARS)speed=15.f;
 m.canHarvest=true;
 if(needsTool(want,n)){ m.canHarvest=(have==T_PICK)&&tier>=requiredTier(n); }
 const float perTick=speed/h/(m.canHarvest?30.f:100.f);
 m.seconds=h<=0.f?0.f:std::ceil(1.f/std::max(perTick,1e-6f))/20.f;
 return m;
}
