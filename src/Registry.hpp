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
struct BlockInfo{ bool solid=true,transparent=false,fluid=false,cullSame=false,breakable=true; uint16_t texture[6]{}; bool translucent=false; uint8_t height16=16; uint16_t rotq=0; /* 2 bits per face: texture turned n x 90 degrees (grain of sideways logs, glazed terracotta tops) */ uint8_t shape=0; /* 0 = full cube, 1.. = boxes(shape), 255 = crossed plant */ bool flatIcon=false; /* drawn as a flat sprite in the inventory */ /* bit per face: rotate the texture 90 degrees (grain of sideways logs) */ /* shape height in 1/16 block */ };

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
 const std::vector<Box>& boxes(uint8_t shape)const{return m_shapes[shape-1];}
 BlockId placed(BlockId base,int nx,int ny,int nz,float yaw,float pitch,float hy=0.5f)const;   // hy = where on the block's height you clicked (0..1)
 BlockId materialOf(BlockId b)const{return b<m_material.size()?m_material[b]:0;}          // slab/stair/carpet -> the cube it is made of
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
 BlockId stone=0,dirt=0,grass=0,sand=0,water=0,log=0,leaves=0,bedrock=0,gravel=0,cobble=0,sandstone=0;
private:
 std::vector<BlockId> m_base,m_material; std::vector<uint8_t> m_rotKind; std::vector<std::array<BlockId,8>> m_variants; std::vector<std::string> m_mcNameOf; std::vector<std::vector<Box>> m_shapes; std::unordered_map<std::string,BlockId> m_mcHead;   // by block id: base block, 0 none / 1 axis / 2 facing4 / 3 facing6, state -> id
 std::vector<BlockInfo> m_blocks; std::vector<std::string> m_blockNames; std::vector<ItemInfo> m_blockItems; // index = block id
 std::vector<ItemInfo> m_items;                                                                                // plain items
 std::unordered_map<std::string,BlockId> m_blockIds; std::unordered_map<std::string,ItemId> m_itemIds;
 std::vector<ItemId> m_tabItems[kTabCount]; ItemId m_tabIcon[kTabCount]{};
};
