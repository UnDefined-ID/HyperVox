#pragma once
// Mobs and other entities: items on the ground, cow / pig / chicken, creeper, enderman, blaze, ender dragon, end crystal, boat, minecart.
// The logic lives in Entity.cpp, the box models (vanilla layout, baked into the block atlas as 16x16 tiles) and their mesh builder in EntityModel.cpp.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "Math.hpp"
#include "Inventory.hpp"
#include "World.hpp"
#include "Registry.hpp"
#include "EntityModel.hpp"

enum class EType : uint8_t { Item, Cow, Pig, Chicken, Creeper, Enderman, Blaze, Dragon, Crystal, Boat, Minecart, Fireball, DragonFireball, Zombie, Skeleton, Spider, Sheep, Ghast, Slime, Wolf, Villager, Arrow, Count };

struct Entity{
 uint32_t id=0; EType type=EType::Item;
 Vec3 pos{},vel{},prev{}; float yaw=0,pitch=0,bodyYaw=0,prevYaw=0;           // pos = feet centre; yaw in radians, forward = (-sin yaw, -cos yaw)
 float w=0.6f,h=1.0f;                                      // collision box: width (x and z) and height
 float health=10,maxHealth=10; bool dead=false,onGround=false,inWater=false,inLava=false,wall=false;
 float walk=0,walkSpeed=0,hurt=0,age=0;                    // animation phase, speed, red flash timer, seconds alive
 float t0=0,t1=0,t2=0,t3=0,amb=0; int s0=0,s1=0;                 // per type scratch (timers / states)
 Vec3 target{};                                            // wander / charge target
 uint32_t rider=0;                                         // boat / minecart: 1 when the player sits in it
 ItemId item=0; int count=1; float pickupDelay=0;          // dropped items
 uint16_t carried=0;                                       // enderman: carried block
 int variant=0;                                            // boat wood type / fireball owner flags
 float box_h()const{return h;}
};

// What the entities need from the game (implemented by Game).
struct EntityEnv{
 World* world=nullptr; int dim=0; double worldTime=0;       // worldTime in ticks (24000 per day, 13000..23000 is night)
 float activeRadius=64.f;                                  // entities further away than this are frozen (their chunks may not be loaded)
 Vec3 player{}; float playerYaw=0,playerPitch=0,playerEye=1.62f; bool playerAlive=true,playerSurvival=true,playerRiding=false;
 std::function<void(float amount,const Vec3&from,float knock)> hurtPlayer;
 std::function<int(ItemId,int)> givePlayer;                 // returns how many did not fit
 std::function<void(const std::string&ev,const Vec3&at,float vol,float pitch)> sound;
 std::function<void(const Vec3&at,float power,bool breakBlocks,uint32_t source)> explode;   // blocks + damage to the player and entities
 std::function<void(int x,int y,int z,uint16_t id)> setBlock;
 std::function<void(const std::string&msg)> notice;
};

class EntityMgr{
public:
 std::vector<Entity> list; uint32_t nextId=1;
 bool dragonDefeated=false; bool dragonSpawned=false; float bossFlash=0;
 Entity* find(uint32_t id);
 Entity& spawn(EType t,const Vec3&pos);
 void spawnItem(const Vec3&pos,ItemId it,int count,const Vec3&vel={0,0,0},float pickupDelay=0.5f);
 void clear(){ list.clear(); dragonSpawned=false; }
 void update(float dt,EntityEnv&env);                       // physics + AI + spawning + pickups
 void hurt(Entity&e,float amount,const Vec3&from,float knock,EntityEnv&env);   // damage from the player or an explosion
 int index(uint32_t id)const;
 // closest entity hit by the ray (the entity's box), its distance in t
 Entity* raycast(const Vec3&o,const Vec3&d,float maxDist,float&tOut);
 // mounting
 Entity* ridden=nullptr; uint32_t riddenId=0;
 void mount(Entity&e){ riddenId=e.id; e.rider=1; }
 void dismount(){ if(Entity*e=find(riddenId))e->rider=0; riddenId=0; }
 // driving the ridden vehicle (forward -1..1, turn -1..1)
 void drive(float forward,float turn,bool jump,float dt,EntityEnv&env);
 Vec3 ridePos(float alpha=1.f)const;                                       // where the rider sits
 // End: crystals on the obsidian pillars + the dragon
 void setupEnd(EntityEnv&env);
 // block physics helpers
 static bool solidAt(const World&w,int x,int y,int z);
 static float surfaceY(const World&w,int x,int z,int fromY);   // y of the first free block above ground, searching down from fromY (-1 = none)
 static bool serialize(const Entity&e,std::string&line);   // one text line per saved entity (hypervox_entities*.txt)
 static bool parse(const std::string&line,Entity&out);
 float dragonHealth()const; bool dragonAlive()const;
 float dragonMax()const{return 200.f;}
};

// ---- rendering (EntityModel.cpp)
// Builds the mesh of one entity in the block vertex format.  Local coordinates are offset by (16,2,16) blocks: the draw origin is pos - (16,2,16).
// `itemTile` = atlas tile of a sprite for dropped non-block items (0 = use the block cube); returns false if nothing should be drawn.
bool buildEntityMesh(const Entity&e,float time,uint16_t itemTile,ChunkMesh&out);
// A glowing beam between two points (end crystal -> dragon); split into pieces of at most 12 blocks.  `piece` 0..n-1, returns false when there is no such piece.  Mesh origin = from - (16,2,16) + direction offset: see out parameters.
bool buildBeamPiece(const Vec3&from,const Vec3&to,int piece,uint16_t tile,float time,ChunkMesh&out,Vec3&origin);
constexpr float kEntityMeshOffX=16.f,kEntityMeshOffY=2.f,kEntityMeshOffZ=16.f;
