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
 { const size_t baseCount=m_blocks.size(); m_base.assign(baseCount,0); for(size_t i=0;i<baseCount;i++)m_base[i]=(BlockId)i; m_rotKind.assign(baseCount,0); m_variants.assign(baseCount,std::array<BlockId,16>{}); m_material.assign(baseCount,0); m_mcNameOf.assign(baseCount,std::string()); m_shapes.clear(); m_mcHead.clear();
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
     m_blocks.push_back(b); m_blockNames.push_back(nm+suffix); m_blockItems.push_back(m_blockItems[i]); m_base.push_back((BlockId)i); m_rotKind.push_back(0); m_variants.push_back(std::array<BlockId,16>{}); m_material.push_back(0); m_mcNameOf.push_back(std::string()); m_variants[i][slot]=id; };
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
 { auto shapeId=[&](std::vector<Box> bx)->uint16_t{ m_shapes.push_back(std::move(bx)); return (uint16_t)m_shapes.size(); };
   auto pretty=[](std::string s){ bool up=true; for(auto&ch:s){ if(ch=='_'){ch=' ';up=true;} else if(up){ch=(char)std::toupper((unsigned char)ch);up=false;} } return s; };
   // creates a new block (+ its inventory item) and keeps every parallel vector in step
   auto addBlk=[&](const std::string&name,const std::string&mc,const BlockInfo&bi,int tab,bool item,BlockId headOf,const std::string&display)->BlockId{
    const BlockId id=(BlockId)m_blocks.size(); m_blocks.push_back(bi); m_blockNames.push_back(name);
    ItemInfo it; it.name=name; it.display=display; it.isBlock=true; it.tab=tab; m_blockItems.push_back(it);
    m_base.push_back(headOf?headOf:id); m_rotKind.push_back(0); m_variants.push_back(std::array<BlockId,16>{}); m_material.push_back(0); m_mcNameOf.push_back(mc);
    if(!headOf){ m_blockIds[name]=id; if(item){m_itemIds[name]=id; m_tabItems[tab].push_back(id);} if(!mc.empty())m_mcHead[mc]=id; }
    return id; };
   // puts a new inventory item into the creative tab directly behind another one (or at the end of its tab)
   auto insertAfter=[&](BlockId after,BlockId id){ const int tab=std::clamp((int)m_blockItems[after].tab,0,kTabCount-1); auto&v=m_tabItems[tab]; auto p=std::find(v.begin(),v.end(),(ItemId)after); v.insert(p==v.end()?v.end():p+1,(ItemId)id); m_itemIds[m_blockItems[id].name]=id; };
   const uint16_t shSlabB=shapeId({{0,0,0,16,8,16}}),shSlabT=shapeId({{0,8,0,16,16,16}});
   // stairs: 0..3 bottom (north south east west), 4..7 upside down.  facing = the way you look; the tall part is on that side.
   uint16_t shSt[8]; for(int half=0;half<2;half++)for(int f=0;f<4;f++){ Box lo=half?Box{0,8,0,16,16,16}:Box{0,0,0,16,8,16}; const int y0=half?0:8,y1=half?8:16; Box st=f==0?Box{0,(uint8_t)y0,0,16,(uint8_t)y1,8}:f==1?Box{0,(uint8_t)y0,8,16,(uint8_t)y1,16}:f==2?Box{8,(uint8_t)y0,0,16,(uint8_t)y1,16}:Box{0,(uint8_t)y0,0,8,(uint8_t)y1,16}; shSt[half*4+f]=shapeId({lo,st}); }
   static const char*sm[][2]={{"stone","stone"},{"cobblestone","cobblestone"},{"mossy_cobblestone","mossy_cobblestone"},{"stone_brick","stone_bricks"},{"mossy_stone_brick","mossy_stone_bricks"},{"brick","bricks"},{"sandstone","sandstone"},{"smooth_sandstone","smooth_sandstone"},{"cut_sandstone","cut_sandstone"},{"red_sandstone","red_sandstone"},{"smooth_red_sandstone","smooth_red_sandstone"},{"cut_red_sandstone","cut_red_sandstone"},{"quartz","quartz_block"},{"smooth_quartz","smooth_quartz"},{"purpur","purpur_block"},{"poplar","poplar_planks"},{"cinnabar","cinnabar"},{"polished_cinnabar","polished_cinnabar"},{"cinnabar_brick","cinnabar_bricks"},{"sulfur","sulfur"},{"polished_sulfur","polished_sulfur"},{"sulfur_brick","sulfur_bricks"},{"andesite","andesite"},{"polished_andesite","polished_andesite"},{"diorite","diorite"},{"polished_diorite","polished_diorite"},{"granite","granite"},{"polished_granite","polished_granite"},{"prismarine","prismarine"},{"prismarine_brick","prismarine_bricks"},{"dark_prismarine","dark_prismarine"},{"nether_brick","nether_bricks"},{"red_nether_brick","red_nether_bricks"},{"end_stone_brick","end_stone_bricks"},{"blackstone","blackstone"},{"polished_blackstone","polished_blackstone"},{"polished_blackstone_brick","polished_blackstone_bricks"},{"cobbled_deepslate","cobbled_deepslate"},{"polished_deepslate","polished_deepslate"},{"deepslate_brick","deepslate_bricks"},{"deepslate_tile","deepslate_tiles"},{"smooth_stone","smooth_stone"},{"tuff","tuff"},{"polished_tuff","polished_tuff"},{"tuff_brick","tuff_bricks"},{"mud_brick","mud_bricks"},{"cut_copper","cut_copper"},{"oak","oak_planks"},{"spruce","spruce_planks"},{"birch","birch_planks"},{"jungle","jungle_planks"},{"acacia","acacia_planks"},{"dark_oak","dark_oak_planks"},{"mangrove","mangrove_planks"},{"cherry","cherry_planks"},{"bamboo","bamboo_planks"},{"crimson","crimson_planks"},{"warped","warped_planks"},{"pale_oak","pale_oak_planks"}};
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
   const uint16_t shCarpet=shapeId({{0,0,0,16,1,16}});
   for(const char*col:{"white","orange","magenta","light_blue","yellow","lime","pink","gray","light_gray","cyan","purple","blue","brown","green","red","black"}){ const auto it=m_blockIds.find(std::string(col)+"_wool"); if(it==m_blockIds.end())continue; BlockInfo b=m_blocks[it->second]; b.transparent=true; b.shape=shCarpet; b.height16=1; const BlockId id=addBlk(std::string(col)+"_carpet",std::string(col)+"_carpet",b,2,false,0,pretty(col)+" Carpet"); insertAfter(it->second,id); m_material[id]=it->second; }
   // fences / walls / glass panes / iron bars: shape follows the neighbours (see World::buildMesh and Game::blockBoxes)
   for(const char*wd:{"oak","spruce","birch","jungle","acacia","dark_oak","mangrove","cherry","bamboo","crimson","warped","pale_oak","poplar","nether_brick"}){
    const std::string pl=std::string(wd)+(std::string(wd)=="nether_brick"?"s":"_planks"); const auto it=m_blockIds.find(pl); if(it==m_blockIds.end())continue;
    BlockInfo b=m_blocks[it->second]; b.transparent=true; b.shape=SH_FENCE; b.conn=std::string(wd)=="nether_brick"?2:1; b.height16=16; const std::string nm=std::string(wd)+(std::string(wd)=="nether_brick"?"_fence":"_fence");
    const BlockId id=addBlk(nm,nm,b,3,true,0,pretty(nm)); m_material[id]=it->second; }
   for(const char*wl:{"cobblestone","mossy_cobblestone","stone_brick","mossy_stone_brick","brick","sandstone","red_sandstone","andesite","diorite","granite","prismarine","nether_brick","red_nether_brick","end_stone_brick","blackstone","polished_blackstone","polished_blackstone_brick","cobbled_deepslate","polished_deepslate","deepslate_brick","deepslate_tile","mud_brick","tuff","polished_tuff","tuff_brick","cinnabar","polished_cinnabar","cinnabar_brick","sulfur","polished_sulfur","sulfur_brick"}){
    for(auto&m:sm){ if(std::string(m[0])!=wl)continue; const auto it=m_blockIds.find(m[1]); if(it==m_blockIds.end())break; BlockInfo b=m_blocks[it->second]; b.transparent=true; b.shape=SH_WALL; b.conn=3; b.height16=16;
     const BlockId id=addBlk(std::string(wl)+"_wall",std::string(wl)+"_wall",b,0,true,0,pretty(std::string(wl))+" Wall"); m_material[id]=it->second; break; } }
   { const auto it=m_blockIds.find("glass"); if(it!=m_blockIds.end()){ BlockInfo b=m_blocks[it->second]; b.transparent=true; b.shape=SH_PANE; b.conn=4; b.height16=16; const BlockId id=addBlk("glass_pane","glass_pane",b,0,true,0,"Glass Pane"); m_material[id]=it->second; } }
   for(const char*col:{"white","orange","magenta","light_blue","yellow","lime","pink","gray","light_gray","cyan","purple","blue","brown","green","red","black"}){ const std::string g=std::string(col)+"_stained_glass"; const auto it=m_blockIds.find(g); if(it==m_blockIds.end())continue; BlockInfo b=m_blocks[it->second]; b.transparent=true; b.shape=SH_PANE; b.conn=4; b.height16=16; const BlockId id=addBlk(g+"_pane",g+"_pane",b,2,true,0,pretty(g)+" Pane"); m_material[id]=it->second; }
   if(pack.has("iron_bars")){ BlockInfo b; b.solid=true; b.transparent=true; b.shape=SH_PANE; b.conn=4; b.height16=16; for(int f=0;f<6;f++)b.texture[f]=pack.id("iron_bars"); addBlk("iron_bars","iron_bars",b,3,true,0,"Iron Bars"); }
   // chest (planks texture on a 14x14x14 box), torches (2x10x2 with the torch sprite cropped), crossed plants
   { const auto it=m_blockIds.find("oak_planks"); if(it!=m_blockIds.end()){ BlockInfo b=m_blocks[it->second]; b.transparent=true; b.shape=shapeId({{1,0,1,15,14,15}}); b.height16=14; const BlockId id=addBlk("chest","chest",b,3,true,0,"Chest"); m_material[id]=it->second; m_mcHead["trapped_chest"]=id; m_mcHead["ender_chest"]=id;
     if(pack.has("chest_side")&&pack.has("chest_front")&&pack.has("chest_top")){   // real chest texture, front towards the player
      const uint16_t S=pack.id("chest_side"),F=pack.id("chest_front"),T=pack.id("chest_top"); static const int ff[4]={5,4,3,2}; static const char*cn[4]={"","@north","@east","@west"};   // south, north, east, west
      m_rotKind[id]=2; m_variants[id][0]=id;
      for(int s=0;s<4;s++){ BlockInfo bb=m_blocks[id]; for(int f=0;f<6;f++)bb.texture[f]=S; bb.texture[0]=bb.texture[1]=T; bb.texture[ff[s]]=F;
       if(s==0)m_blocks[id]=bb; else m_variants[id][s]=addBlk(std::string("chest")+cn[s],"",bb,3,false,id,""); } } } }
   // blocks that are not full cubes in vanilla (the generated list makes every block a cube)
   for(auto&sp:std::vector<std::pair<const char*,Box>>{{"cactus",Box{1,0,1,15,16,15}},{"enchanting_table_block",Box{0,0,0,16,12,16}},{"enchanting_table",Box{0,0,0,16,12,16}},{"farmland",Box{0,0,0,16,15,16}},{"dirt_path",Box{0,0,0,16,15,16}}}){
    auto it=m_blockIds.find(sp.first); if(it==m_blockIds.end())continue; BlockInfo&bi=m_blocks[it->second]; bi.transparent=true; bi.shape=shapeId({sp.second}); bi.height16=sp.second.y1; }
   // doors (two blocks tall), trapdoors and fence gates.  8 variants each: facing (north east south west) + 4 * open.  Right click opens / closes them (Game::placeBlock).
   { static const Box DC[4]={Box{0,0,13,16,16,16},Box{0,0,0,3,16,16},Box{0,0,0,16,16,3},Box{13,0,0,16,16,16}};   // closed door panel: on the edge nearest the player
     static const char*fn[4]={"north","east","south","west"};
     auto boxesOf=[&](int kind,int f,bool open,bool hinge)->std::vector<Box>{
      if(kind==0)return {DC[open?(f+(hinge?3:1))&3:f]};   // door: the hinge side decides which way it swings
      if(kind==1)return open?std::vector<Box>{DC[f]}:(hinge?std::vector<Box>{Box{0,13,0,16,16,16}}:std::vector<Box>{Box{0,0,0,16,3,16}});   // trapdoor: bit 8 = placed on the upper half
      const bool ns=(f&1)==0; if(!open)return ns?std::vector<Box>{Box{0,5,7,16,16,9}}:std::vector<Box>{Box{7,5,0,9,16,16}};
      return ns?std::vector<Box>{Box{0,5,7,2,16,9},Box{14,5,7,16,16,9}}:std::vector<Box>{Box{7,5,0,9,16,2},Box{7,5,14,9,16,16}}; };
     // builds the 8 variants of one block; returns the base (= north, closed)
     auto build=[&](const std::string&nm,const std::string&mc,int kind,const BlockInfo&tpl,const std::string&disp,uint8_t rk,bool item,int tab)->BlockId{
      BlockId base=0; const int ns=(kind==0||kind==1)?16:8; for(int s=0;s<ns;s++){ BlockInfo bi=tpl; bi.transparent=true; bi.shape=shapeId(boxesOf(kind,s&3,(s&4)!=0,(s&8)!=0)); if(kind==2&&(s&4))bi.solid=false; if(kind==2)bi.conn=1;   // fence gates: wooden fences connect to them
       if(s==0){ base=addBlk(nm,mc,bi,tab,item,0,disp); m_rotKind[base]=rk; m_variants[base][0]=base; }
       else { const BlockId v=addBlk(nm+"@"+fn[s&3]+((s&4)?"_open":"")+((s&8)?(kind==0?"_r":"_top"):""),"",bi,tab,false,base,""); m_variants[base][s]=v; } }
      return base; };
     for(const char*wd:{"oak","spruce","birch","jungle","acacia","dark_oak","mangrove","cherry","bamboo","crimson","warped","pale_oak","poplar","iron","copper"}){
      const std::string w=wd; const bool metal=w=="iron"||w=="copper"; const auto pl=m_blockIds.find(metal?w+"_block":w+"_planks"); const BlockId mat=pl==m_blockIds.end()?0:pl->second;
      if(pack.has(w+"_door_bottom")&&pack.has(w+"_door_top")){
       BlockInfo lo; lo.flatIcon=true; for(int f=0;f<6;f++)lo.texture[f]=pack.id(w+"_door_bottom"); BlockInfo up=lo; for(int f=0;f<6;f++)up.texture[f]=pack.id(w+"_door_top");
       const BlockId b=build(w+"_door",w+"_door",0,lo,pretty(w)+" Door",8,true,3); const BlockId t=build(w+"_door_top","",0,up,"",9,false,3); m_mcNameOf[t]=w+"_door";
       m_doorTop[b]=t; m_doorBottom[t]=b; if(mat){ m_material[b]=mat; m_material[t]=mat; for(int s=1;s<16;s++){m_material[m_variants[b][s]]=mat;m_material[m_variants[t][s]]=mat;} } }
      if(pack.has(w+"_trapdoor")){ BlockInfo tb; tb.flatIcon=true; for(int f=0;f<6;f++)tb.texture[f]=pack.id(w+"_trapdoor"); const BlockId b=build(w+"_trapdoor",w+"_trapdoor",1,tb,pretty(w)+" Trapdoor",8,true,3); if(mat){m_material[b]=mat;for(int s=1;s<16;s++)m_material[m_variants[b][s]]=mat;} }
      if(!metal&&mat){ BlockInfo gb=m_blocks[mat]; const BlockId b=build(w+"_fence_gate",w+"_fence_gate",2,gb,pretty(w)+" Fence Gate",8,true,3); m_material[b]=mat; for(int s=1;s<8;s++)m_material[m_variants[b][s]]=mat; } } }
   // signs (standing board, front towards you; no text yet) and pressure plates (decoration, no redstone)
   { static const char*fn[4]={"","@north","@east","@west"};
     for(const char*wd:{"oak","spruce","birch","jungle","acacia","dark_oak","mangrove","cherry","bamboo","crimson","warped","pale_oak","poplar"}){
      const std::string w=wd; const auto pl=m_blockIds.find(w+"_planks"); if(pl==m_blockIds.end())continue; const BlockId mat=pl->second;
      { BlockInfo b=m_blocks[mat]; b.transparent=true; b.flatIcon=true; b.height16=16; BlockId base=0;
        for(int s=0;s<4;s++){ BlockInfo bs=b; bs.shape=(s<2)?shapeId({Box{1,8,7,15,16,9},Box{7,0,7,9,8,9}}):shapeId({Box{7,8,1,9,16,15},Box{7,0,7,9,8,9}});
         if(s==0){ base=addBlk(w+"_sign",w+"_sign",bs,3,true,0,pretty(w)+" Sign"); m_rotKind[base]=2; m_variants[base][0]=base; m_material[base]=mat; }
         else { const BlockId v=addBlk(w+"_sign"+fn[s],"",bs,3,false,base,""); m_variants[base][s]=v; m_material[v]=mat; } } }
      { BlockInfo b=m_blocks[mat]; b.transparent=true; b.solid=false; b.shape=shapeId({Box{1,0,1,15,1,15}}); b.height16=1; const BlockId id=addBlk(w+"_pressure_plate",w+"_pressure_plate",b,3,true,0,pretty(w)+" Pressure Plate"); m_material[id]=mat; } }
     for(auto&pr:std::vector<std::pair<const char*,const char*>>{{"stone","stone"},{"polished_blackstone","polished_blackstone"},{"light_weighted","gold_block"},{"heavy_weighted","iron_block"}}){
      const auto it=m_blockIds.find(pr.second); if(it==m_blockIds.end())continue; BlockInfo b=m_blocks[it->second]; b.transparent=true; b.solid=false; b.shape=shapeId({Box{1,0,1,15,1,15}}); b.height16=1;
      const BlockId id=addBlk(std::string(pr.first)+"_pressure_plate",std::string(pr.first)+"_pressure_plate",b,3,true,0,pretty(std::string(pr.first))+" Pressure Plate"); m_material[id]=it->second; } }
   // lanterns, candles, rails, buttons: small static decoration blocks (no light / redstone behaviour, buttons sit on the floor)
   { auto simple=[&](const std::string&nm,const std::string&tex,std::vector<Box> bx,const std::string&disp,bool solid,BlockId mat)->BlockId{
      if(!pack.has(tex))return 0; BlockInfo b; for(int f=0;f<6;f++)b.texture[f]=pack.id(tex); b.transparent=true; b.solid=solid; b.flatIcon=true; b.shape=shapeId(std::move(bx)); b.height16=bx.empty()?16:16;
      const BlockId id=addBlk(nm,nm,b,3,true,0,disp); if(mat)m_material[id]=mat; return id; };
     auto matOf=[&](const std::string&n)->BlockId{ auto it=m_blockIds.find(n); return it==m_blockIds.end()?0:it->second; };
     simple("lantern","lantern",{Box{5,0,5,11,7,11},Box{6,7,6,10,9,10}},"Lantern",false,matOf("iron_block")); simple("soul_lantern","soul_lantern",{Box{5,0,5,11,7,11},Box{6,7,6,10,9,10}},"Soul Lantern",false,matOf("iron_block"));
     simple("candle","candle",{Box{7,0,7,9,6,9}},"Candle",false,matOf("white_wool"));
     for(const char*col:{"white","orange","magenta","light_blue","yellow","lime","pink","gray","light_gray","cyan","purple","blue","brown","green","red","black"})simple(std::string(col)+"_candle",std::string(col)+"_candle",{Box{7,0,7,9,6,9}},pretty(std::string(col))+" Candle",false,matOf(std::string(col)+"_wool"));
     for(const char*rl:{"rail","powered_rail","detector_rail","activator_rail"})simple(rl,rl,{Box{0,0,0,16,1,16}},pretty(std::string(rl)),false,matOf("iron_block"));
     for(const char*wd:{"oak","spruce","birch","jungle","acacia","dark_oak","mangrove","cherry","bamboo","crimson","warped","pale_oak","poplar"}){ const BlockId m=matOf(std::string(wd)+"_planks"); if(!m)continue; BlockInfo b=m_blocks[m]; b.transparent=true; b.solid=false; b.flatIcon=true; b.shape=shapeId({Box{5,0,6,11,2,10}}); b.height16=2; const BlockId id=addBlk(std::string(wd)+"_button",std::string(wd)+"_button",b,3,true,0,pretty(std::string(wd))+" Button"); m_material[id]=m; }
     for(const char*bt:{"stone","polished_blackstone"}){ const BlockId m=matOf(bt); if(!m)continue; BlockInfo b=m_blocks[m]; b.transparent=true; b.solid=false; b.flatIcon=true; b.shape=shapeId({Box{5,0,6,11,2,10}}); b.height16=2; const BlockId id=addBlk(std::string(bt)+"_button",std::string(bt)+"_button",b,3,true,0,pretty(std::string(bt))+" Button"); m_material[id]=m; } }
   for(const char*tn:{"torch","soul_torch","redstone_torch","copper_torch"}){ if(!pack.has(tn))continue; BlockInfo b; b.solid=false; b.transparent=true; b.shape=shapeId({{7,0,7,9,10,9}}); b.flatIcon=true; for(int f=0;f<6;f++)b.texture[f]=pack.id(tn); b.height16=10; const BlockId head=addBlk(tn,tn,b,3,true,0,pretty(tn));
    // wall torches: 1 wall at -x (facing east), 2 wall at +x (west), 3 wall at -z (south), 4 wall at +z (north)
    m_rotKind[head]=7; m_variants[head][0]=head; const Box wb[4]={{2,3,7,4,13,9},{12,3,7,14,13,9},{7,3,2,9,13,4},{7,3,12,9,13,14}}; static const char*wn[4]={"@east","@west","@south","@north"};
    for(int k=0;k<4;k++){ BlockInfo w=b; w.shape=shapeId({wb[k]}); m_variants[head][1+k]=addBlk(std::string(tn)+wn[k],"",w,3,false,head,""); } }
   for(const char*pn:{"short_grass","fern","dead_bush","dandelion","poppy","blue_orchid","allium","azure_bluet","red_tulip","orange_tulip","white_tulip","pink_tulip","oxeye_daisy","cornflower","lily_of_the_valley","oak_sapling","spruce_sapling","birch_sapling","jungle_sapling","acacia_sapling","dark_oak_sapling","cherry_sapling","poplar_sapling","golden_dandelion","red_shrub","brown_mushroom","red_mushroom","sugar_cane"}){
    if(!pack.has(pn))continue; BlockInfo b; b.solid=false; b.transparent=true; b.shape=SH_PLANT; b.flatIcon=true; for(int f=0;f<6;f++)b.texture[f]=pack.id(pn); b.height16=16; addBlk(pn,pn,b,1,true,0,pretty(pn)); }
   for(auto&al:std::initializer_list<std::pair<const char*,const char*>>{{"grass","short_grass"},{"tall_grass","short_grass"},{"large_fern","fern"},{"wall_torch","torch"},{"soul_wall_torch","soul_torch"},{"redstone_wall_torch","redstone_torch"},{"sunflower","dandelion"},{"rose_bush","poppy"},{"lilac","allium"},{"peony","pink_tulip"}}){ auto it=m_mcHead.find(al.second); if(it!=m_mcHead.end())m_mcHead[al.first]=it->second; }
 }
 if(pack.has("missing")){   // MISSING: not in creative, not in /give
  BlockInfo b; b.texture[0]=b.texture[1]=b.texture[2]=b.texture[3]=b.texture[4]=b.texture[5]=pack.id("missing");
  const BlockId id=(BlockId)m_blocks.size(); m_blocks.push_back(b); m_blockNames.push_back("missing"); m_blockIds["missing"]=id; m_base.push_back(id); m_rotKind.push_back(0); m_variants.push_back(std::array<BlockId,16>{}); m_material.push_back(0); m_mcNameOf.push_back(std::string());
  ItemInfo it; it.name="missing"; it.display="MISSING"; it.isBlock=true; it.tab=0; m_blockItems.push_back(it);
 }
 for(const auto&r:kItems){
  if(!std::filesystem::exists(pack.assetRoot()/"item"/(std::string(r.icon)+".png")))continue;
  ItemInfo it; it.name=r.name; it.display=r.display; it.iconName=r.icon; it.tab=r.tab;
  const ItemId id=(ItemId)(kItemBase+m_items.size()); m_items.push_back(it); m_itemIds[r.name]=id; m_tabItems[r.tab].push_back(id);
 }

 m_stairState.assign(m_blocks.size(),-1); for(size_t b=0;b<m_blocks.size();b++){ if(b<m_rotKind.size()&&m_rotKind[b]==5&&m_base[b]==b){ for(int i=0;i<8;i++)if(m_variants[b][i])m_stairState[m_variants[b][i]]=(int8_t)i; } }   // dynamic stair shapes
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
 b=baseOf(b); { auto it=m_doorBottom.find(b); if(it!=m_doorBottom.end())b=it->second; }
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

int Registry::connectBoxes(uint16_t shape,bool n,bool e,bool s,bool w,Box*o,int mode){   // mode 0 = what is drawn, 1 = selection outline (vanilla: arms are full-height boxes), 2 = collision (fences 1.5 blocks tall)
 int k=0; const bool coll=mode==2;
 if(shape==SH_FENCE){ const uint8_t top=coll?24:16; o[k++]={6,0,6,10,top,10};
  if(mode==0){ if(e){o[k++]={10,6,7,16,9,9};o[k++]={10,12,7,16,15,9};} if(w){o[k++]={0,6,7,6,9,9};o[k++]={0,12,7,6,15,9};} if(s){o[k++]={7,6,10,9,9,16};o[k++]={7,12,10,9,15,16};} if(n){o[k++]={7,6,0,9,9,6};o[k++]={7,12,0,9,15,6};} }
  else{ if(e)o[k++]={10,0,7,16,top,9}; if(w)o[k++]={0,0,7,6,top,9}; if(s)o[k++]={7,0,10,9,top,16}; if(n)o[k++]={7,0,0,9,top,6}; } }
 else if(shape==SH_WALL){ const uint8_t ah=coll?24:14; o[k++]={4,0,4,12,coll?24:16,12};
  if(e)o[k++]={12,0,5,16,ah,11}; if(w)o[k++]={0,0,5,4,ah,11}; if(s)o[k++]={5,0,12,11,ah,16}; if(n)o[k++]={5,0,0,11,ah,4}; }
 else { o[k++]={7,0,7,9,16,9}; if(e)o[k++]={9,0,7,16,16,9}; if(w)o[k++]={0,0,7,7,16,9}; if(s)o[k++]={7,0,9,9,16,16}; if(n)o[k++]={7,0,0,9,16,7}; }
 return k;
}
BlockId Registry::placed(BlockId base,int nx,int ny,int nz,float yaw,float pitch,float hy)const{
 base=baseOf(base); if(base>=m_rotKind.size()||!m_rotKind[base])return base; const auto&v=m_variants[base]; const uint8_t k=m_rotKind[base];
 if(k==1){ const int s=nx?1:(nz?2:0); return v[s]?v[s]:base; }
 if(k==8){ const float gx=std::sin(yaw),gz=-std::cos(yaw); const int f=std::fabs(gz)>=std::fabs(gx)?(gz<0?0:2):(gx>0?1:3); const std::string&bn=m_blockNames[base]; const bool trap=bn.size()>=8&&bn.compare(bn.size()-8,8,"trapdoor")==0; const int top=trap&&((ny<0)||(ny==0&&hy>0.5f))?8:0; return v[f+top]?v[f+top]:base; }   // door / trapdoor / gate: the way you look
 if(k==7){ const int s=ny!=0?0:(nx>0?1:nx<0?2:nz>0?3:4); return v[s]?v[s]:base; }   // torch: on the floor, or on the wall you clicked
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
 if(k==8||k==9){ int s=0; for(int i=0;i<16;i++)if(m_variants[base][i]==b)s=i; static const char*f4[4]={"north","east","south","west"}; const bool gate=mcName.size()>=10&&mcName.compare(mcName.size()-10,10,"fence_gate")==0,trap=mcName.size()>=8&&mcName.compare(mcName.size()-8,8,"trapdoor")==0;
  props.push_back({"facing",f4[s&3]}); if(!gate)props.push_back({"half",trap?((s&8)?"top":"bottom"):(k==9?"upper":"lower")}); if(!trap&&!gate)props.push_back({"hinge",(s&8)?"right":"left"}); props.push_back({"open",(s&4)?"true":"false"}); return true; }
 if(k==7){ int s=0; for(int i=0;i<5;i++)if(m_variants[base][i]==b)s=i; if(s==0)return true; static const char*fc[5]={"","east","west","south","north"}; if(mcName=="torch")mcName="wall_torch"; else if(mcName=="soul_torch")mcName="soul_wall_torch"; else if(mcName=="redstone_torch")mcName="redstone_wall_torch"; props.push_back({"facing",fc[s]}); return true; }
 int s=0; for(int i=0;i<8;i++)if(m_variants[base][i]==b)s=i; if(b==base&&k!=5)s=0;
 if(k==1)props.push_back({"axis",s==1?"x":(s==2?"z":"y")});
 else if(k==4)props.push_back({"type",s==1?"top":"bottom"});
 else if(k==5){ static const char*f[4]={"north","south","east","west"}; props.push_back({"facing",f[s&3]}); props.push_back({"half",s>=4?"top":"bottom"}); props.push_back({"shape","straight"}); }
 else { static const char*f[6]={"south","north","east","west","up","down"}; props.push_back({"facing",f[s]}); }
 return true;
}
BlockId Registry::withState(BlockId base,const std::string&axis,const std::string&facing,const std::string&half,const std::string&type)const{
 base=baseOf(base); if(base>=m_rotKind.size()||!m_rotKind[base])return base; const auto&v=m_variants[base]; const uint8_t k=m_rotKind[base]; int s=0;
 if(k==8){ s=(facing=="east"?1:facing=="south"?2:facing=="west"?3:0)+(type.find("open")!=std::string::npos?4:0)+(type.find("right")!=std::string::npos?8:0)+(half=="top"?8:0); BlockId tgt=base; if(half=="upper"){ auto it=m_doorTop.find(base); if(it!=m_doorTop.end())tgt=it->second; } const auto&vv=m_variants[tgt]; return vv[s]?vv[s]:tgt; }
 if(k==1){ if(axis=="x")s=1; else if(axis=="z")s=2; }
 else if(k==7){ s=facing=="east"?1:facing=="west"?2:facing=="south"?3:facing=="north"?4:0; }
 else if(k==4){ if(type=="double")return m_material[base]?m_material[base]:base; s=type=="top"?1:0; }
 else if(k==5){ s=(facing=="north"?0:facing=="east"?2:facing=="west"?3:1)+(half=="top"?4:0); }
 else{ if(facing=="north")s=1; else if(facing=="east")s=2; else if(facing=="west")s=3; else if(facing=="up")s=4; else if(facing=="down")s=5; }
 return v[s]?v[s]:base;
}

int Registry::openKind(BlockId b)const{ const BlockId base=baseOf(b); if(base>=m_rotKind.size())return 0; const uint8_t k=m_rotKind[base]; return k==8?1:(k==9?2:0); }
BlockId Registry::toggled(BlockId b)const{ if(!openKind(b))return b; const BlockId base=baseOf(b); int s=0; for(int i=0;i<16;i++)if(m_variants[base][i]==b)s=i; const BlockId n=m_variants[base][s^4]; return n?n:b; }
BlockId Registry::doorTopFor(BlockId lower)const{ const BlockId base=baseOf(lower); auto it=m_doorTop.find(base); if(it==m_doorTop.end())return 0; int s=0; for(int i=0;i<16;i++)if(m_variants[base][i]==lower)s=i; const BlockId t=m_variants[it->second][s]; return t?t:it->second; }
BlockId Registry::doorBottomOf(BlockId top)const{ auto it=m_doorBottom.find(baseOf(top)); return it==m_doorBottom.end()?0:it->second; }
BlockId Registry::doorWithHinge(BlockId d,bool right)const{ const BlockId base=baseOf(d); if(base>=m_variants.size())return d; int s=0; for(int i=0;i<16;i++)if(m_variants[base][i]==d)s=i; const BlockId n=m_variants[base][(s&7)|(right?8:0)]; return n?n:d; }

// ---- stairs: the shape follows the neighbouring stairs (vanilla StairBlock rules): straight, inner / outer corner, left / right
namespace{
struct V2{int x,z;};
V2 dirOf(int f){ static const V2 d[4]={{0,-1},{0,1},{1,0},{-1,0}}; return d[f]; }          // facing 0..3 = north south east west (as the stair variants are ordered)
int facingOf(V2 v){ return v.z<0?0:v.z>0?1:v.x>0?2:3; }
V2 ccw(V2 v){ return {v.z,-v.x}; } V2 cw(V2 v){ return {-v.z,v.x}; } V2 neg(V2 v){ return {-v.x,-v.z}; }
bool eq(V2 a,V2 b){ return a.x==b.x&&a.z==b.z; }
Box halfBox(V2 d,int y0,int y1){ return d.z<0?Box{0,(uint8_t)y0,0,16,(uint8_t)y1,8}:d.z>0?Box{0,(uint8_t)y0,8,16,(uint8_t)y1,16}:d.x>0?Box{8,(uint8_t)y0,0,16,(uint8_t)y1,16}:Box{0,(uint8_t)y0,0,8,(uint8_t)y1,16}; }
Box quarter(V2 a,V2 b,int y0,int y1){ const Box p=halfBox(a,y0,y1),q=halfBox(b,y0,y1); return Box{std::max(p.x0,q.x0),(uint8_t)y0,std::max(p.z0,q.z0),std::min(p.x1,q.x1),(uint8_t)y1,std::min(p.z1,q.z1)}; }
}
int Registry::stairBoxes(int state,const int nb[4],Box*o){
 const int f=state&3,half=state>>2; const V2 fv=dirOf(f);
 auto at=[&](V2 d){ return nb[facingOf(d)]; };   // nb[] is indexed by direction in the same north south east west order
 auto facingOfState=[&](int s){ return s&3; };
 auto canTake=[&](V2 d){ const int n=at(d); return n<0||facingOfState(n)!=f||(n>>2)!=half; };
 enum{STRAIGHT,OUTER_L,OUTER_R,INNER_L,INNER_R} shape=STRAIGHT;
 { const int n=at(fv); if(n>=0&&(n>>2)==half){ const V2 d=dirOf(facingOfState(n)); if(d.x*fv.x+d.z*fv.z==0&&canTake(neg(d)))shape=eq(d,ccw(fv))?OUTER_L:OUTER_R; } }
 if(shape==STRAIGHT){ const int n=at(neg(fv)); if(n>=0&&(n>>2)==half){ const V2 d=dirOf(facingOfState(n)); if(d.x*fv.x+d.z*fv.z==0&&canTake(d))shape=eq(d,ccw(fv))?INNER_L:INNER_R; } }
 const int by0=half?8:0,by1=half?16:8,ty0=half?0:8,ty1=half?8:16;   // slab layer / the layer with the steps
 int k=0; o[k++]=Box{0,(uint8_t)by0,0,16,(uint8_t)by1,16};
 switch(shape){
  case STRAIGHT: o[k++]=halfBox(fv,ty0,ty1); break;
  case OUTER_L: o[k++]=quarter(fv,ccw(fv),ty0,ty1); break;
  case OUTER_R: o[k++]=quarter(fv,cw(fv),ty0,ty1); break;
  case INNER_L: o[k++]=halfBox(fv,ty0,ty1); o[k++]=quarter(neg(fv),ccw(fv),ty0,ty1); break;
  case INNER_R: o[k++]=halfBox(fv,ty0,ty1); o[k++]=quarter(neg(fv),cw(fv),ty0,ty1); break; }
 return k;
}
int Registry::variantIndex(BlockId b)const{ const BlockId base=baseOf(b); if(base>=m_variants.size())return 0; for(int i=0;i<16;i++)if(m_variants[base][i]==b)return i; return 0; }
