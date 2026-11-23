#pragma once
#include <cstdint>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>
class TexturePack;

using BlockId=uint16_t;   // 0 = air
using ItemId=uint16_t;    // < kItemBase: the block with the same id, >= kItemBase: a plain item
constexpr ItemId kItemBase=4096;

// Face order everywhere: 0=-Y(bottom) 1=+Y(top) 2=-X 3=+X 4=-Z 5=+Z
struct Box{ uint8_t x0,y0,z0,x1,y1,z1; };   // 1/16 block units
// ---- detailed render models (the collision / outline shape stays the simple Box list above).  Units are pixels (1/16 block).
// A face shows the texture rectangle starting at (uo,vo); the rectangle is as big as the face (swapped when rot is odd) unless tw/th > 0, which stretches a tw x th rectangle over it.
// rot = quarter turns of the texture on the face (clockwise seen from outside).  tex < 0: no face.  autoUV: take the rectangle from the face position (like a vanilla cube).
struct MFace{ int16_t tex=-1; float uo=0,vo=0,tw=0,th=0; uint8_t rot=0; bool autoUV=true; };
struct MBox{ float x0=0,y0=0,z0=0,x1=16,y1=16,z1=16; MFace f[6]; bool trans=false,twoSided=false,cullBorder=true; };   // faces: 0 -Y 1 +Y 2 -X 3 +X 4 -Z 5 +Z
struct MQuad{ float p[4][3]; float uv[4][2]; int16_t tex=-1; bool twoSided=true; };   // free quad, corners counter-clockwise seen from the front; uv in pixels
struct Model{ std::vector<MBox> boxes; std::vector<MQuad> quads; };
// Texture coordinate (pixels) of a face corner.  w,h = face size in pixels, du/dv = the corner along the face's two axes (see World's addQuad: u runs along x or z, v along z or y).
inline void faceUV(int face,float w,float h,float du,float dv,const MFace&f,float&u,float&v){
 const float a=(face==3||face==4)?w-du:du, b=(face>=2)?h-dv:dv;
 const bool odd=(f.rot&1)!=0; const float rw=f.tw>0?f.tw:(odd?h:w), rh=f.th>0?f.th:(odd?w:h);
 switch(f.rot&3){
  case 0: u=f.uo+a*(rw/w); v=f.vo+b*(rh/h); break;
  case 1: u=f.uo+b*(rw/h); v=f.vo+(w-a)*(rh/w); break;
  case 2: u=f.uo+(w-a)*(rw/w); v=f.vo+(h-b)*(rh/h); break;
  default: u=f.uo+(h-b)*(rw/h); v=f.vo+a*(rh/w); break; }
}
struct BlockInfo{ bool solid=true,transparent=false,fluid=false,cullSame=false,breakable=true; uint16_t texture[6]{}; bool translucent=false; uint8_t height16=16; uint16_t rotq=0; /* 2 bits per face: texture turned n x 90 degrees (grain of sideways logs, glazed terracotta tops) */ uint8_t conn=0; /* connect group for fences / walls / panes */ uint16_t shape=0; /* 0 = full cube, 1.. = boxes(shape), 255 = crossed plant */ bool flatIcon=false; /* drawn as a flat sprite in the inventory */ uint16_t model=0; /* 0 = none, else index+1 into the render models */ /* bit per face: rotate the texture 90 degrees (grain of sideways logs) */ /* shape height in 1/16 block */ };

struct ItemInfo{ std::string name,display; bool isBlock=false; std::string iconName; /* item/<iconName>.png for plain items */ int tab=0; };

class Registry{
public:
 static constexpr int kTabCount=9;   // 7 normal tabs, 7 = Redstone, 8 = Survival Inventory
 static Registry& get();
 void init(const TexturePack&pack);

 // blocks
 const BlockInfo& info(BlockId id)const{return id<m_blocks.size()?m_blocks[id]:m_blocks[0];}
 size_t blockCount()const{return m_blocks.size();}
 BlockId blockByName(const std::string&n)const;
 const std::string& blockName(BlockId id)const{return id<m_blockNames.size()?m_blockNames[id]:m_blockNames[0];}

 // items (blocks are items too)
 bool validItem(ItemId id)const;
 const ItemInfo& item(ItemId id)const;
 ItemId itemByName(const std::string&n)const;           // 0 if unknown
 std::string itemName(ItemId id)const;
 ItemId dropFor(BlockId b)const;
 std::vector<std::string> allItemNames()const;         // every block + item name (command auto-complete)
 const std::vector<std::string>& blockNames()const{return m_blockNames;}
 // Rotation: every rotated state is its own block id (made at init): logs / pillars have axis y (the base) + x + z, furnaces, pumpkins, glazed
 // terracotta ... face 4 directions, pistons / dispensers / droppers / observers / barrels 6.
 BlockId baseOf(BlockId b)const{return b<m_base.size()?m_base[b]:b;}
 BlockId redstoneWire=0; uint16_t wireTile=0; const Model* wireModel(int mask)const{ return mask>=0&&mask<(int)m_wire.size()?&m_wire[(size_t)mask]:nullptr; }   // redstone dust: 16 connection patterns (n=1 e=2 s=4 w=8)
 const Model* modelOf(BlockId id)const{ const uint16_t m=info(id).model; return (m&&m<=m_models.size())?&m_models[m-1]:nullptr; }
 const std::vector<Box>& boxes(uint16_t shape)const{ static const std::vector<Box> none; return (shape>=1&&shape<=m_shapes.size())?m_shapes[shape-1]:none; }   // safe for the neighbour-dependent shapes 250..252 (they have no fixed boxes)
 static constexpr uint16_t SH_FENCE=65530,SH_WALL=65531,SH_PANE=65532,SH_PLANT=65535;   // shapes that depend on their neighbours
  // mode bit 4 (walls): the block above is a wall too
 static int connectBoxes(uint16_t shape,bool n,bool e,bool s,bool w,Box*out,int mode);   // mode 0 draw, 1 outline, 2 collision; n=-z e=+x s=+z w=-x; returns the box count (max 9)
 BlockId placed(BlockId base,int nx,int ny,int nz,float yaw,float pitch,float hy=0.5f)const;   // hy = where on the block's height you clicked (0..1)
 BlockId materialOf(BlockId b)const{return b<m_material.size()?m_material[b]:0;}          // slab/stair/carpet -> the cube it is made of
 int stairState(BlockId b)const{ return b<m_stairState.size()?m_stairState[b]:-1; }   // -1 not a stair, else 0..7 = half*4 + facing (north south east west)
 static int stairBoxes(int state,const int nb[4],Box*out);          // boxes of a stair given its neighbours' stair states (N,S,E,W; -1 = no stair): straight / inner / outer corners
 BlockId doorWithHinge(BlockId door,bool right)const;

 BlockId blockIdOrZero(const std::string&n)const{ auto it=m_blockIds.find(n); return it==m_blockIds.end()?BlockId(0):it->second; }
 BlockId variantOf(BlockId base,int i)const{ return base<m_variants.size()?m_variants[base][i&15]:BlockId(0); }
 int bedPart(BlockId b)const;         // -1 not a bed, 0 foot, 1 head
 int bedFacing(BlockId b)const;       // 0..3 = north east south west: the way the head points
 BlockId bedHeadFor(BlockId foot)const;
 int endFrameEye(BlockId b)const;           // -1 not an end portal frame, 0 empty, 1 with an eye of ender
 BlockId endFrameWithEye(BlockId b)const;
 bool isNetherPortal(BlockId b)const{ return b&&(b==netherPortalX||b==netherPortalZ); }
 bool isLadder(BlockId b)const{ const BlockId base=baseOf(b); return base<m_rotKind.size()&&m_rotKind[base]==15; }
 int faceMask(BlockId b)const{ auto it=m_faceMask.find(b); return it==m_faceMask.end()?0:it->second; }   // vines / lichen / sculk veins: which faces (north east south west up down = bits 0..5), 0 = not such a block
 BlockId faceVariant(BlockId any,int mask)const{ auto it=m_faceVar.find(baseOf(any)); return it==m_faceVar.end()||mask<=0||mask>63?any:(it->second[mask]?it->second[mask]:any); }
 int variantIndex(BlockId b)const;                                  // position of b among its base block's variants (doors: facing + 4*open + 8*hingeRight)                // same door with the hinge on the other side
 int openKind(BlockId b)const;                       // 0 none, 1 door lower half / trapdoor / fence gate, 2 door upper half
 BlockId toggled(BlockId b)const;                   // the same block opened / closed
 BlockId doorTopFor(BlockId lower)const;            // upper half matching a lower half's facing + open state
 BlockId doorBottomOf(BlockId top)const;
 int slabKind(BlockId b)const;                                                         // 0 none, 1 bottom slab, 2 top slab
 BlockId shapeHead(const std::string&mcName)const;                                      // Minecraft block name -> our block (0 = none)          // state to place for the clicked face + look direction
 bool stateOf(BlockId b,std::string&mcName,std::vector<std::pair<std::string,std::string>>&props)const;   // Minecraft name + block properties of a block
 BlockId withState(BlockId base,const std::string&axis,const std::string&facing,const std::string&half,const std::string&type)const;   // inverse, used when loading Minecraft worlds
 int maxStack(ItemId id)const;                           // 1 for tools / armour / filled buckets ..., 16 for buckets / pearls ..., else 64
 int armorSlot(ItemId id)const;                          // 0 helmet, 1 chestplate, 2 leggings, 3 boots, -1 = not armour
 int armorPoints(ItemId id)const;                        // vanilla defence points of the piece                         // 0 = drops nothing
 size_t plainItemCount()const{return m_items.size();}
 const ItemInfo& plainItem(size_t i)const{return m_items[i];}

 // creative tabs
 const std::vector<ItemId>& tabItems(int tab)const{return m_tabItems[tab];}
 const char* tabName(int tab)const;
 ItemId tabIcon(int tab)const{return m_tabIcon[tab];}

 // blocks referenced by the world generator
 BlockId missing=0;   // purple/black block shown where a world block cannot be read
 BlockId netherPortalX=0,netherPortalZ=0,endPortal=0,stone=0,dirt=0,grass=0,sand=0,water=0,log=0,leaves=0,bedrock=0,gravel=0,cobble=0,sandstone=0;
private:
 std::vector<Model> m_models; std::vector<Model> m_wire; void initModels(const TexturePack&pack); uint16_t addModel(Model m){ m_models.push_back(std::move(m)); return (uint16_t)m_models.size(); }
 std::vector<BlockId> m_base,m_material; std::vector<uint8_t> m_rotKind; std::vector<std::array<BlockId,16>> m_variants; std::vector<int8_t> m_stairState; std::unordered_map<BlockId,std::array<BlockId,64>> m_faceVar; std::unordered_map<BlockId,int> m_faceMask; std::vector<std::string> m_mcNameOf; std::vector<std::vector<Box>> m_shapes; std::unordered_map<std::string,BlockId> m_mcHead; std::unordered_map<BlockId,BlockId> m_doorTop,m_doorBottom;   // by block id: base block, 0 none / 1 axis / 2 facing4 / 3 facing6, state -> id
 std::vector<BlockInfo> m_blocks; std::vector<std::string> m_blockNames; std::vector<ItemInfo> m_blockItems; // index = block id
 std::vector<ItemInfo> m_items;                                                                                // plain items
 std::unordered_map<std::string,BlockId> m_blockIds; std::unordered_map<std::string,ItemId> m_itemIds;
 std::vector<ItemId> m_tabItems[kTabCount]; ItemId m_tabIcon[kTabCount]{};
};
