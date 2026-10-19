#include "Registry.hpp"
#include <algorithm>
#include "TexturePack.hpp"
#include <cmath>
#include <cctype>
#include <cstring>
#include <iostream>

namespace {
enum{F_T=1,F_S=2,F_L=4,F_U=8};
struct BlockRow{const char*name,*display,*flags,*top,*bottom,*side,*front;int tab;};
const BlockRow kBlocks[]={
#include "BlockList.inc"
};
struct ItemRow{const char*name,*icon,*display;int tab;};
const ItemRow kItems[]={
#include "ItemList.inc"
};
int parseFlags(const char*f){int r=0;for(;*f;f++){if(*f=='T')r|=F_T;else if(*f=='S')r|=F_S;else if(*f=='L')r|=F_L;else if(*f=='U')r|=F_U;}return r;}
const char* kTabNames[Registry::kTabCount]={"Building Blocks","Natural Blocks","Colored Blocks","Functional Blocks","Tools & Armor","Food & Drinks","Ingredients","Redstone Blocks","Survival Inventory"};
const char* kTabIcons[Registry::kTabCount]={"bricks","grass_block","red_wool","crafting_table","iron_pickaxe","apple","iron_ingot","redstone","chest"};
}

Registry& Registry::get(){static Registry r;return r;}

void Registry::init(const TexturePack&pack){
 m_blocks.clear();m_blockNames.clear();m_blockItems.clear();m_items.clear();m_blockIds.clear();m_itemIds.clear();
 for(auto&t:m_tabItems)t.clear();
 // id 0 = air
 m_blocks.push_back(BlockInfo{false,true,false,false,false,{0,0,0,0,0,0}}); m_blockNames.push_back("air"); m_blockItems.push_back(ItemInfo{});
 for(const auto&r:kBlocks){
  const char*tex[4]={r.top,r.bottom,r.side,r.front};
  bool ok=true; for(int i=0;i<4;i++)if(tex[i][0]&&!pack.has(tex[i]))ok=false;
  if(!ok){std::cerr<<"Registry: skipping block '"<<r.name<<"' (missing texture)\n";continue;}
  const int fl=parseFlags(r.flags);
  BlockInfo b; b.transparent=(fl&F_T)!=0; b.cullSame=(fl&F_S)!=0; b.fluid=(fl&F_L)!=0; b.solid=!b.fluid; b.breakable=!(fl&F_U)&&!b.fluid;
  { const std::string nm=r.name; auto ends=[&](const char*e){const size_t l=std::strlen(e);return nm.size()>=l&&nm.compare(nm.size()-l,l,e)==0;};
    b.translucent=nm=="water"||nm=="tinted_glass"||ends("_stained_glass")||nm=="ice"||nm=="slime_block"||nm=="honey_block";
    if(nm=="enchanting_table_block"){b.height16=12;b.transparent=true;} }       // 12/16 tall like vanilla
  const uint16_t top=pack.id(r.top),bot=pack.id(r.bottom),side=pack.id(r.side),front=r.front[0]?pack.id(r.front):side;
  b.texture[0]=bot;b.texture[1]=top;b.texture[2]=side;b.texture[3]=side;b.texture[4]=side;b.texture[5]=front;
  const BlockId id=(BlockId)m_blocks.size();
  m_blocks.push_back(b); m_blockNames.push_back(r.name); m_blockIds[r.name]=id;
  ItemInfo it; it.name=r.name; it.display=r.display; it.isBlock=true; it.tab=r.tab; m_blockItems.push_back(it);
  m_itemIds[r.name]=id; m_tabItems[r.tab].push_back(id);
 }
 // ---- rotated variants (ids are appended after all normal blocks, so normal ids never change)
 { const size_t baseCount=m_blocks.size(); m_base.assign(baseCount,0); for(size_t i=0;i<baseCount;i++)m_base[i]=(BlockId)i; m_rotKind.assign(baseCount,0); m_variants.assign(baseCount,std::array<BlockId,8>{}); m_material.assign(baseCount,0); m_mcNameOf.assign(baseCount,std::string()); m_shapes.clear(); m_mcHead.clear();
   auto ends=[](const std::string&s,const char*e){const size_t l=std::strlen(e);return s.size()>=l&&s.compare(s.size()-l,l,e)==0;};
   for(size_t i=1;i<baseCount;i++){
    const std::string nm=m_blockNames[i]; int kind=0;
    if(ends(nm,"_log")||ends(nm,"_wood")||ends(nm,"_stem")||ends(nm,"_hyphae")||nm=="hay_block"||nm=="bone_block"||nm=="basalt"||nm=="polished_basalt"||nm=="quartz_pillar"||nm=="purpur_pillar"||nm=="deepslate"||nm=="muddy_mangrove_roots"||ends(nm,"_froglight")||nm=="infested_deepslate"||nm=="ancient_debris"&&false)kind=1;
    else if(nm=="furnace"||nm=="blast_furnace"||nm=="smoker"||nm=="carved_pumpkin"||nm=="jack_o_lantern"||nm=="loom"||nm=="stonecutter"||nm=="beehive"||nm=="bee_nest"||ends(nm,"_glazed_terracotta")||nm=="chiseled_bookshelf"||nm=="lectern"||nm=="grindstone"||nm=="anvil"||nm=="bell")kind=2;
    else if(nm=="dispenser"||nm=="dropper"||nm=="observer"||nm=="piston"||nm=="sticky_piston"||nm=="barrel"||nm=="command_block"||nm=="chain_command_block"||nm=="repeating_command_block"||nm=="shulker_box"||ends(nm,"_shulker_box")||nm=="crafter")kind=3;
    if(!kind)continue;
    const BlockInfo b0=m_blocks[i]; const uint16_t T=b0.texture[1],Bt=b0.texture[0],S=b0.texture[2],F=b0.texture[5];
    m_rotKind[i]=(uint8_t)kind; m_variants[i][0]=(BlockId)i;
    auto add=[&](int slot,const char*suffix,const uint16_t tx[6],uint16_t rot){
     BlockInfo b=b0; for(int f=0;f<6;f++)b.texture[f]=tx[f]; b.rotq=rot; const BlockId id=(BlockId)m_blocks.size();
     m_blocks.push_back(b); m_blockNames.push_back(nm+suffix); m_blockItems.push_back(m_blockItems[i]); m_base.push_back((BlockId)i); m_rotKind.push_back(0); m_variants.push_back(std::array<BlockId,8>{}); m_material.push_back(0); m_mcNameOf.push_back(std::string()); m_variants[i][slot]=id; };
    if(kind==1){ const uint16_t X[6]={S,S,T,T,S,S},Z[6]={S,S,S,S,T,T}; add(1,"@x",X,uint16_t(1<<0|1<<2|1<<8|1<<10)); add(2,"@z",Z,uint16_t(1<<4|1<<6)); }   // grain follows the axis
    else{   // facing: 0 south (front on +Z), 1 north, 2 east, 3 west, 4 up, 5 down
     const bool cap=(nm=="piston"||nm=="sticky_piston"||nm=="barrel");           // these have a head (their "top" texture) and a different back
     uint16_t Fr=F!=S?F:T,Bk=S,Sd=S,Tp=T,Bm=Bt;                                    // front, back, sides, top/bottom when facing sideways
     if(cap){ Fr=T; Bk=Bt; Tp=S; Bm=S; }
     else if(nm=="observer"&&pack.has("observer_back")){ Bk=pack.id("observer_back"); }
     const bool hasBack=cap||nm=="observer"; const bool glazed=ends(nm,"_glazed_terracotta"); auto turnsFor=[&](int f)->uint16_t{ if(!glazed)return 0; const int tq=f==4?0:f==3?1:f==5?2:3; return uint16_t(tq<<0|tq<<2); };
     auto mk=[&](int slot,const char*suffix,int f){ uint16_t t[6]={Bm,Tp,Sd,Sd,Sd,Sd}; t[f]=Fr; static const int opp[6]={1,0,3,2,5,4}; if(hasBack)t[opp[f]]=Bk; add(slot,suffix,t,glazed?turnsFor(f):uint16_t(0)); };
     mk(1,"@north",4); mk(2,"@east",3); mk(3,"@west",2);
     if(glazed)m_blocks[i].rotq=turnsFor(5);
     if(kind==3){
      mk(0,"@south",5);                                                             // the base block keeps the original textures (inventory icon); this is its real "facing south" state
      uint16_t Up[6]={Bk,Fr,Sd,Sd,Sd,Sd},Dn[6]={Fr,Bk,Sd,Sd,Sd,Sd}; if(!hasBack){Up[0]=Tp;Dn[1]=Tp;}
      add(4,"@up",Up,0); add(5,"@down",Dn,0);
     }
    }
   }
 }

 // ---- partial blocks: slabs, stairs, carpets, torches, chests and flowers / grass.  Shapes are lists of boxes (1/16 units).
 { auto shapeId=[&](std::vector<Box> bx)->uint8_t{ m_shapes.push_back(std::move(bx)); return (uint8_t)m_shapes.size(); };
   auto pretty=[](std::string s){ bool up=true; for(auto&ch:s){ if(ch=='_'){ch=' ';up=true;} else if(up){ch=(char)std::toupper((unsigned char)ch);up=false;} } return s; };
   // creates a new block (+ its inventory item) and keeps every parallel vector in step
   auto addBlk=[&](const std::string&name,const std::string&mc,const BlockInfo&bi,int tab,bool item,BlockId headOf,const std::string&display)->BlockId{
    const BlockId id=(BlockId)m_blocks.size(); m_blocks.push_back(bi); m_blockNames.push_back(name);
    ItemInfo it; it.name=name; it.display=display; it.isBlock=true; it.tab=tab; m_blockItems.push_back(it);
    m_base.push_back(headOf?headOf:id); m_rotKind.push_back(0); m_variants.push_back(std::array<BlockId,8>{}); m_material.push_back(0); m_mcNameOf.push_back(mc);
    if(!headOf){ m_blockIds[name]=id; if(item){m_itemIds[name]=id; m_tabItems[tab].push_back(id);} if(!mc.empty())m_mcHead[mc]=id; }
    return id; };
   // puts a new inventory item into the creative tab directly behind another one (or at the end of its tab)
   auto insertAfter=[&](BlockId after,BlockId id){ const int tab=std::clamp((int)m_blockItems[after].tab,0,kTabCount-1); auto&v=m_tabItems[tab]; auto p=std::find(v.begin(),v.end(),(ItemId)after); v.insert(p==v.end()?v.end():p+1,(ItemId)id); m_itemIds[m_blockItems[id].name]=id; };
   const uint8_t shSlabB=shapeId({{0,0,0,16,8,16}}),shSlabT=shapeId({{0,8,0,16,16,16}});
   // stairs: 0..3 bottom (north south east west), 4..7 upside down.  facing = the way you look; the tall part is on that side.
   uint8_t shSt[8]; for(int half=0;half<2;half++)for(int f=0;f<4;f++){ Box lo=half?Box{0,8,0,16,16,16}:Box{0,0,0,16,8,16}; const int y0=half?0:8,y1=half?8:16; Box st=f==0?Box{0,(uint8_t)y0,0,16,(uint8_t)y1,8}:f==1?Box{0,(uint8_t)y0,8,16,(uint8_t)y1,16}:f==2?Box{8,(uint8_t)y0,0,16,(uint8_t)y1,16}:Box{0,(uint8_t)y0,0,8,(uint8_t)y1,16}; shSt[half*4+f]=shapeId({lo,st}); }
   static const char*sm[][2]={{"stone","stone"},{"cobblestone","cobblestone"},{"mossy_cobblestone","mossy_cobblestone"},{"stone_brick","stone_bricks"},{"mossy_stone_brick","mossy_stone_bricks"},{"brick","bricks"},{"sandstone","sandstone"},{"smooth_sandstone","smooth_sandstone"},{"cut_sandstone","cut_sandstone"},{"red_sandstone","red_sandstone"},{"smooth_red_sandstone","smooth_red_sandstone"},{"cut_red_sandstone","cut_red_sandstone"},{"quartz","quartz_block"},{"smooth_quartz","smooth_quartz"},{"purpur","purpur_block"},{"andesite","andesite"},{"polished_andesite","polished_andesite"},{"diorite","diorite"},{"polished_diorite","polished_diorite"},{"granite","granite"},{"polished_granite","polished_granite"},{"prismarine","prismarine"},{"prismarine_brick","prismarine_bricks"},{"dark_prismarine","dark_prismarine"},{"nether_brick","nether_bricks"},{"red_nether_brick","red_nether_bricks"},{"end_stone_brick","end_stone_bricks"},{"blackstone","blackstone"},{"polished_blackstone","polished_blackstone"},{"polished_blackstone_brick","polished_blackstone_bricks"},{"cobbled_deepslate","cobbled_deepslate"},{"polished_deepslate","polished_deepslate"},{"deepslate_brick","deepslate_bricks"},{"deepslate_tile","deepslate_tiles"},{"smooth_stone","smooth_stone"},{"tuff","tuff"},{"polished_tuff","polished_tuff"},{"tuff_brick","tuff_bricks"},{"mud_brick","mud_bricks"},{"cut_copper","cut_copper"},{"oak","oak_planks"},{"spruce","spruce_planks"},{"birch","birch_planks"},{"jungle","jungle_planks"},{"acacia","acacia_planks"},{"dark_oak","dark_oak_planks"},{"mangrove","mangrove_planks"},{"cherry","cherry_planks"},{"bamboo","bamboo_planks"},{"crimson","crimson_planks"},{"warped","warped_planks"},{"pale_oak","pale_oak_planks"}};
   for(auto&m:sm){ const auto it=m_blockIds.find(m[1]); if(it==m_blockIds.end())continue; const BlockId mat=it->second; const BlockInfo b0=m_blocks[mat]; if(!b0.solid)continue;
    BlockInfo sb=b0; sb.transparent=true; sb.shape=shSlabB; sb.height16=8; const std::string stem=m[0];
    const BlockId slab=addBlk(stem+"_slab",stem+"_slab",sb,0,false,0,pretty(stem)+" Slab"); insertAfter(mat,slab); m_material[slab]=mat; m_rotKind[slab]=4; m_variants[slab][0]=slab;
    BlockInfo st=sb; st.shape=shSlabT; st.height16=16; const BlockId top=addBlk(stem+"_slab@top","",st,0,false,slab,""); m_variants[slab][1]=top;
    BlockInfo sti=b0; sti.transparent=true; sti.shape=shSt[1]; sti.height16=16; const BlockId stairs=addBlk(stem+"_stairs",stem+"_stairs",sti,0,false,0,pretty(stem)+" Stairs"); insertAfter(slab,stairs); m_material[stairs]=mat; m_rotKind[stairs]=5;
    // variant index order matches withState(): north south east west, then the same upside down; the base (index 1) is the "south" state
    static const char*fn[4]={"north","south","east","west"};
    for(int half=0;half<2;half++)for(int f=0;f<4;f++){ const int idx=half*4+f; if(idx==1){m_variants[stairs][1]=stairs;m_blocks[stairs].shape=shSt[1];continue;} BlockInfo v=sti; v.shape=shSt[idx]; m_variants[stairs][idx]=addBlk(stem+"_stairs@"+std::string(half?"top_":"")+fn[f],"",v,0,false,stairs,""); }
    m_blocks[slab].height16=8;
   }
   // carpets: 1/16 thick, made from the wool colours
   const uint8_t shCarpet=shapeId({{0,0,0,16,1,16}});
   for(const char*col:{"white","orange","magenta","light_blue","yellow","lime","pink","gray","light_gray","cyan","purple","blue","brown","green","red","black"}){ const auto it=m_blockIds.find(std::string(col)+"_wool"); if(it==m_blockIds.end())continue; BlockInfo b=m_blocks[it->second]; b.transparent=true; b.shape=shCarpet; b.height16=1; const BlockId id=addBlk(std::string(col)+"_carpet",std::string(col)+"_carpet",b,2,false,0,pretty(col)+" Carpet"); insertAfter(it->second,id); m_material[id]=it->second; }
   // chest (planks texture on a 14x14x14 box), torches (2x10x2 with the torch sprite cropped), crossed plants
   { const auto it=m_blockIds.find("oak_planks"); if(it!=m_blockIds.end()){ BlockInfo b=m_blocks[it->second]; b.transparent=true; b.shape=shapeId({{1,0,1,15,14,15}}); b.height16=14; const BlockId id=addBlk("chest","chest",b,3,true,0,"Chest"); m_material[id]=it->second; m_mcHead["trapped_chest"]=id; m_mcHead["ender_chest"]=id; } }
   for(const char*tn:{"torch","soul_torch","redstone_torch","copper_torch"}){ if(!pack.has(tn))continue; BlockInfo b; b.solid=false; b.transparent=true; b.shape=shapeId({{7,0,7,9,10,9}}); b.flatIcon=true; for(int f=0;f<6;f++)b.texture[f]=pack.id(tn); b.height16=10; addBlk(tn,tn,b,3,true,0,pretty(tn)); }
   for(const char*pn:{"short_grass","fern","dead_bush","dandelion","poppy","blue_orchid","allium","azure_bluet","red_tulip","orange_tulip","white_tulip","pink_tulip","oxeye_daisy","cornflower","lily_of_the_valley","oak_sapling","spruce_sapling","birch_sapling","jungle_sapling","acacia_sapling","dark_oak_sapling","cherry_sapling","brown_mushroom","red_mushroom","sugar_cane"}){
    if(!pack.has(pn))continue; BlockInfo b; b.solid=false; b.transparent=true; b.shape=255; b.flatIcon=true; for(int f=0;f<6;f++)b.texture[f]=pack.id(pn); b.height16=16; addBlk(pn,pn,b,1,true,0,pretty(pn)); }
   for(auto&al:std::initializer_list<std::pair<const char*,const char*>>{{"grass","short_grass"},{"tall_grass","short_grass"},{"large_fern","fern"},{"wall_torch","torch"},{"soul_wall_torch","soul_torch"},{"redstone_wall_torch","redstone_torch"},{"sunflower","dandelion"},{"rose_bush","poppy"},{"lilac","allium"},{"peony","pink_tulip"}}){ auto it=m_mcHead.find(al.second); if(it!=m_mcHead.end())m_mcHead[al.first]=it->second; }
 }
 if(pack.has("missing")){   // MISSING: not in creative, not in /give
  BlockInfo b; b.texture[0]=b.texture[1]=b.texture[2]=b.texture[3]=b.texture[4]=b.texture[5]=pack.id("missing");
  const BlockId id=(BlockId)m_blocks.size(); m_blocks.push_back(b); m_blockNames.push_back("missing"); m_blockIds["missing"]=id; m_base.push_back(id); m_rotKind.push_back(0); m_variants.push_back(std::array<BlockId,8>{}); m_material.push_back(0); m_mcNameOf.push_back(std::string());
  ItemInfo it; it.name="missing"; it.display="MISSING"; it.isBlock=true; it.tab=0; m_blockItems.push_back(it);
 }
 for(const auto&r:kItems){
  if(!std::filesystem::exists(pack.assetRoot()/"item"/(std::string(r.icon)+".png")))continue;
  ItemInfo it; it.name=r.name; it.display=r.display; it.iconName=r.icon; it.tab=r.tab;
  const ItemId id=(ItemId)(kItemBase+m_items.size()); m_items.push_back(it); m_itemIds[r.name]=id; m_tabItems[r.tab].push_back(id);
 }

 // every other item texture in the pack becomes an item too (so nothing the pack ships is missing from the creative menu)
 { namespace fs=std::filesystem; std::error_code ec; std::vector<std::string> names; for(auto&e:fs::directory_iterator(pack.assetRoot()/"item",ec)){ if(e.path().extension()==".png")names.push_back(e.path().stem().string()); } std::sort(names.begin(),names.end());
   auto has=[](const std::string&s,const char*p){return s.find(p)!=std::string::npos;}; auto ends=[](const std::string&s,const char*e){const size_t l=std::strlen(e);return s.size()>=l&&s.compare(s.size()-l,l,e)==0;};
   auto pretty=[](std::string s){ bool up=true; for(auto&ch:s){ if(ch=='_'){ch=' ';up=true;} else if(up){ch=(char)std::toupper((unsigned char)ch);up=false;} } return s; };
   for(const std::string&n:names){
    if(m_itemIds.count(n)||m_blockIds.count(n))continue;
    if(ends(n,"_overlay")||has(n,"spawn_egg")||has(n,"debug")||has(n,"template")||has(n,"empty_slot")||has(n,"empty_armor")||n=="air"||has(n,"_trim")&&false)continue;
    int tab=6;
    if(ends(n,"_sword")||ends(n,"_pickaxe")||ends(n,"_axe")||ends(n,"_shovel")||ends(n,"_hoe")||ends(n,"_helmet")||ends(n,"_chestplate")||ends(n,"_leggings")||ends(n,"_boots")||ends(n,"_horse_armor")||ends(n,"_spear")||n=="bow"||n=="crossbow"||n=="shield"||n=="fishing_rod"||n=="shears"||n=="flint_and_steel"||n=="trident"||n=="mace"||n=="elytra"||n=="spyglass"||n=="brush"||n=="arrow"||n=="carrot_on_a_stick"||n=="warped_fungus_on_a_stick"||n=="totem_of_undying")tab=4;
    else if(has(n,"apple")||has(n,"bread")||has(n,"beef")||has(n,"porkchop")||has(n,"chicken")||has(n,"mutton")||has(n,"rabbit")||has(n,"cod")||has(n,"salmon")||has(n,"potato")||has(n,"carrot")||has(n,"stew")||has(n,"soup")||has(n,"cookie")||has(n,"pie")||has(n,"melon_slice")||has(n,"berries")||has(n,"dried_kelp")||has(n,"honey_bottle")||has(n,"rotten_flesh")||has(n,"spider_eye")||has(n,"chorus_fruit")||has(n,"beetroot")||has(n,"potion")||n=="milk_bucket"||n=="tropical_fish"||n=="pufferfish")tab=5;
    else if(has(n,"redstone")||n=="repeater"||n=="comparator"||n=="lever"||n=="hopper"||n=="observer"||has(n,"minecart"))tab=7;
    else if(has(n,"door")||has(n,"sign")||has(n,"_bed")||has(n,"boat")||has(n,"raft")||n=="ladder"||n=="painting"||n=="item_frame"||n=="glow_item_frame"||n=="armor_stand"||has(n,"bucket")||n=="bell"||n=="lantern"||n=="soul_lantern"||n=="campfire"||n=="cauldron"||n=="flower_pot"||n=="compass"||n=="clock"||n=="map"||n=="lead"||n=="name_tag"||n=="saddle"||has(n,"banner")||has(n,"shulker_box"))tab=3;
    ItemInfo it; it.name=n; it.display=pretty(n); it.iconName=n; it.tab=tab; const ItemId id=(ItemId)(kItemBase+m_items.size()); m_items.push_back(it); m_itemIds[n]=id; m_tabItems[tab].push_back(id);
   } }
 for(const char*n:{"redstone","redstone_block","redstone_lamp","redstone_torch","repeater","comparator","observer","piston","sticky_piston","dispenser","dropper","hopper","tnt","note_block","target","lever","daylight_detector","lightning_rod","slime_block","honey_block","rail","powered_rail","detector_rail","activator_rail","oak_pressure_plate","stone_pressure_plate","oak_button","stone_button","tripwire_hook","trapped_chest","iron_door","oak_door","oak_trapdoor","iron_trapdoor","oak_fence_gate","jukebox"}){
  const ItemId it=itemByName(n); if(it)m_tabItems[7].push_back(it); }
 m_tabItems[8].clear();
 for(auto&tab:m_tabItems)tab.erase(std::remove_if(tab.begin(),tab.end(),[&](ItemId i){const std::string n=itemName(i);return n=="water"||n=="lava";}),tab.end());   // use the buckets instead
 for(int t=0;t<kTabCount;t++){m_tabIcon[t]=itemByName(kTabIcons[t]); if(!m_tabIcon[t]&&!m_tabItems[t].empty())m_tabIcon[t]=m_tabItems[t][0];}
 missing=blockByName("missing"); stone=blockByName("stone");dirt=blockByName("dirt");grass=blockByName("grass_block");sand=blockByName("sand");water=blockByName("water");
 log=blockByName("oak_log");leaves=blockByName("oak_leaves");bedrock=blockByName("bedrock");gravel=blockByName("gravel");cobble=blockByName("cobblestone");sandstone=blockByName("sandstone");
}
BlockId Registry::blockByName(const std::string&n)const{auto it=m_blockIds.find(n);return it==m_blockIds.end()?0:it->second;}
bool Registry::validItem(ItemId id)const{ if(id==0)return false; if(id<kItemBase)return id<m_blockItems.size(); return (size_t)(id-kItemBase)<m_items.size(); }
const ItemInfo& Registry::item(ItemId id)const{ static const ItemInfo none; if(!validItem(id))return none; return id<kItemBase?m_blockItems[id]:m_items[id-kItemBase]; }
ItemId Registry::itemByName(const std::string&n)const{auto it=m_itemIds.find(n);return it==m_itemIds.end()?0:it->second;}
std::string Registry::itemName(ItemId id)const{return validItem(id)?item(id).name:std::string();}
ItemId Registry::dropFor(BlockId b)const{
 b=baseOf(b);
 if(b==grass)return dirt;
 if(b==stone&&cobble)return cobble;
 const std::string&n=blockName(b);
 if(n.size()>7&&n.compare(n.size()-7,7,"_leaves")==0)return 0;
 if(n=="glass"||n=="tinted_glass"||(n.size()>14&&n.compare(n.size()-14,14,"_stained_glass")==0))return 0;
 return b;
}
const char* Registry::tabName(int tab)const{return (tab>=0&&tab<kTabCount)?kTabNames[tab]:"";}

namespace{ bool endsWith(const std::string&s,const char*e){const size_t l=std::strlen(e);return s.size()>=l&&s.compare(s.size()-l,l,e)==0;} bool hasPart(const std::string&s,const char*p){return s.find(p)!=std::string::npos;} }
int Registry::armorSlot(ItemId id)const{
 const std::string n=itemName(id);
 if(endsWith(n,"_helmet")||n=="turtle_helmet")return 0; if(endsWith(n,"_chestplate"))return 1; if(endsWith(n,"_leggings"))return 2; if(endsWith(n,"_boots"))return 3; return -1;
}
int Registry::armorPoints(ItemId id)const{
 const int s=armorSlot(id); if(s<0)return 0; const std::string n=itemName(id);
 static const int T[6][4]={{1,3,2,1},{2,5,3,1},{2,5,4,1},{2,6,5,2},{3,8,6,3},{3,8,6,3}};   // leather, gold, chainmail/copper, iron, diamond, netherite
 int m=3; if(hasPart(n,"leather"))m=0; else if(hasPart(n,"golden"))m=1; else if(hasPart(n,"chainmail")||hasPart(n,"copper"))m=2; else if(hasPart(n,"iron"))m=3; else if(hasPart(n,"diamond"))m=4; else if(hasPart(n,"netherite"))m=5; else if(n=="turtle_helmet")return 2;
 return T[m][s];
}
int Registry::maxStack(ItemId id)const{
 if(id<kItemBase)return 64;
 const std::string n=itemName(id);
 if(armorSlot(id)>=0)return 1;
 for(const char*e:{"_sword","_pickaxe","_axe","_shovel","_hoe","_horse_armor","_boat","_bed","_potion","_stew"})if(endsWith(n,e))return 1;
 for(const char*e:{"shield","bow","crossbow","trident","fishing_rod","flint_and_steel","shears","elytra","carrot_on_a_stick","warped_fungus_on_a_stick","brush","spyglass","saddle","totem_of_undying","enchanted_book","written_book","writable_book","cake","mace","mushroom_stew","rabbit_stew","beetroot_soup","potion","splash_potion","lingering_potion","wolf_armor"})if(n==e)return 1;
 if(hasPart(n,"minecart")||hasPart(n,"music_disc")||hasPart(n,"_bucket"))return 1;      // filled buckets (water/lava/milk/fish ...)
 if(n=="bucket"||n=="ender_pearl"||n=="snowball"||n=="egg"||n=="armor_stand"||n=="honey_bottle"||endsWith(n,"_sign")||endsWith(n,"_banner")||n=="bucket")return 16;
 return 64;
}
std::vector<std::string> Registry::allItemNames()const{ std::vector<std::string> o; for(size_t i=1;i<m_blockItems.size();i++)if(baseOf((BlockId)i)==i&&m_blockItems[i].name!="missing"&&!m_blockItems[i].name.empty())o.push_back(m_blockItems[i].name); for(auto&it:m_items)o.push_back(it.name); return o; }

BlockId Registry::placed(BlockId base,int nx,int ny,int nz,float yaw,float pitch,float hy)const{
 base=baseOf(base); if(base>=m_rotKind.size()||!m_rotKind[base])return base; const auto&v=m_variants[base]; const uint8_t k=m_rotKind[base];
 if(k==1){ const int s=nx?1:(nz?2:0); return v[s]?v[s]:base; }
 const float fx=std::sin(yaw),fz=-std::cos(yaw);          // where the player looks
 if(k==4){ const bool top=(ny<0)||(ny==0&&hy>0.5f); return top&&v[1]?v[1]:base; }   // slab: upper half / clicked from below
 if(k==5){ const bool top=(ny<0)||(ny==0&&hy>0.5f); int f=std::fabs(fx)>std::fabs(fz)?(fx>0?2:3):(fz<0?0:1); return v[(top?4:0)+f]?v[(top?4:0)+f]:base; }   // tall side = the way you look
 int s; if(k==3&&pitch>0.9f)s=5; else if(k==3&&pitch<-0.9f)s=4;
 else if(std::fabs(fx)>std::fabs(fz))s=fx>0?3:2; else s=fz<0?0:1;                       // the front faces the player
 return v[s]?v[s]:base;
}
int Registry::slabKind(BlockId b)const{ const BlockId base=baseOf(b); if(base>=m_rotKind.size()||m_rotKind[base]!=4)return 0; return b==base?1:2; }
BlockId Registry::shapeHead(const std::string&mc)const{ auto it=m_mcHead.find(mc); return it==m_mcHead.end()?0:it->second; }
bool Registry::stateOf(BlockId b,std::string&mcName,std::vector<std::pair<std::string,std::string>>&props)const{
 const BlockId base=baseOf(b); if(base>=m_rotKind.size())return false; const uint8_t k=m_rotKind[base];
 props.clear();
 if(!m_mcNameOf[base].empty()){ mcName=m_mcNameOf[base]; }
 else if(!k)return false; else mcName=m_blockNames[base];
 if(!k)return true;
 int s=0; for(int i=0;i<8;i++)if(m_variants[base][i]==b)s=i; if(b==base&&k!=5)s=0;
 if(k==1)props.push_back({"axis",s==1?"x":(s==2?"z":"y")});
 else if(k==4)props.push_back({"type",s==1?"top":"bottom"});
 else if(k==5){ static const char*f[4]={"north","south","east","west"}; props.push_back({"facing",f[s&3]}); props.push_back({"half",s>=4?"top":"bottom"}); props.push_back({"shape","straight"}); }
 else { static const char*f[6]={"south","north","east","west","up","down"}; props.push_back({"facing",f[s]}); }
 return true;
}
BlockId Registry::withState(BlockId base,const std::string&axis,const std::string&facing,const std::string&half,const std::string&type)const{
 base=baseOf(base); if(base>=m_rotKind.size()||!m_rotKind[base])return base; const auto&v=m_variants[base]; const uint8_t k=m_rotKind[base]; int s=0;
 if(k==1){ if(axis=="x")s=1; else if(axis=="z")s=2; }
 else if(k==4){ if(type=="double")return m_material[base]?m_material[base]:base; s=type=="top"?1:0; }
 else if(k==5){ s=(facing=="north"?0:facing=="east"?2:facing=="west"?3:1)+(half=="top"?4:0); }
 else{ if(facing=="north")s=1; else if(facing=="east")s=2; else if(facing=="west")s=3; else if(facing=="up")s=4; else if(facing=="down")s=5; }
 return v[s]?v[s]:base;
}
