#pragma once
#include <unordered_set>
#include <GLFW/glfw3.h>
#include <map>
#include <deque>
#include <set>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include "Assets.hpp"
#include "Inventory.hpp"
#include "Registry.hpp"
#include "Entity.hpp"
#include "SaveGame.hpp"
#include "Settings.hpp"
#include "Streamer.hpp"
#include "TexturePack.hpp"
#include "Ui.hpp"
#include "UiAtlas.hpp"
#include "Renderer.hpp"
#include "ChunkStore.hpp"
#include "Mining.hpp"
#include "Perf.hpp"
#include <future>
#include "World.hpp"
#include "Chat.hpp"
#include "Sound.hpp"

// Owns everything that is not the renderer: settings, saves, textures, the world, the player and every menu screen.
class Game{
public:
 explicit Game(GLFWwindow*w);
 ~Game();
 bool loadResources();                                   // block atlas + GUI atlas + panorama (from the disk cache when possible)
 const AtlasImage& blockAtlas()const{return pack_.atlas();}
 const AtlasImage& uiAtlasImage()const{return uiAtlas_.image();}
 const PanoramaImage& panorama()const{return pano_;}
 const Settings& settings()const{return settings_;}
 void attach(Renderer*r);
 void frame();
 void shutdown();                                        // saves the world and the options
private:
 enum class Screen{Title,WorldSelect,CreateWorld,ConfirmDelete,Options,Controls,Textures,Account,Death,Chest,Crafting,Furnace,Generic,Loading,Pause,Inventory,Creative,Play};
 enum class LoadTask{None,EnterWorld,ReloadPack};
 struct Hit{ bool hit=false; int x=0,y=0,z=0,nx=0,ny=0,nz=0; float hy=0.5f,hx=0.5f,hz=0.5f; };

 // --- game.cpp
 static void charCallback(GLFWwindow*w,unsigned int cp);
 static void dropCallback(GLFWwindow*w,int n,const char**paths);
 bool useItem(); void damage(float amt,double now,bool armor=true); void die(); void respawn(); void screenDeath(); void buildSky(float W,float H); void cleanMemory(); void manageMemory(double now); int rssMB()const; std::vector<std::string> chatSuggest(size_t&tokStart)const; float eyeHeight()const{return eyeH_;}
 void handleDrops(); void computePanoRot(); void markWorldDirty();
 void screenAccount(); void refreshSkins(); const AtlasImage* skinImage(const std::string&id); void drawSkinHead(const std::string&id,float x,float y,float px); void drawSkinFull(const std::string&id,float x,float y,float px);
 static void scrollCallback(GLFWwindow*w,double dx,double dy);
 UiInput gatherInput(int fbw,int fbh,double now);
 bool keyPressed(int k)const; bool keyDown(int k)const; void snapshotKeys();
 void captureMouse(bool on); void handleEscape();
 void updatePlay(float dt,const UiInput&in);
 bool collides(const Vec3&p)const; bool moveAxis(int axis,float delta); void movePlayer(float dt);
 Hit raycast(const Vec3&o,const Vec3&d,float maxDist,bool fluids=false)const;
 int blockBoxes(int x,int y,int z,uint16_t id,float out[10][6],bool collision=false)const;   // the boxes (block fractions) a block is made of: selection outline + precise targeting
 void breakBlock(bool drop=true); void placeBlock(); void pickBlock(); void markDirtyAround(int x,int z,int outCount,int out[3][2]);
 void startLoading(LoadTask t,const WorldInfo*w=nullptr); void updateLoading();
 void enterWorld(const WorldInfo&w); void leaveWorld(); void saveWorld(bool sync=false); void waitSave(); void applyResourcePack();
 void openPause(); void closePause(); void openInventory(); void closeInventory();
 void drawChat();
 void buildPanorama(float W,float H,double t);
 Mat4 projection(float aspect)const; Mat4 viewMatrix()const;

 // --- gameui.cpp
 void buildScreen(); void drawBackground(bool darken); void drawHud();
 void screenTitle(); void screenWorldSelect(); void screenCreateWorld(); void screenConfirmDelete(); void screenOptions(); void screenControls(); void screenTextures();
 void screenLoading(); void screenPause(); void screenInventory(); void screenCreative();
 void clickSlot(InvSlot&s,bool left,bool right); void quickMove(int from); void drawSlot(const InvSlot&s,float x,float y); 
 std::string keyLabel(int k)const;

 GLFWwindow* win_=nullptr; Renderer* renderer_=nullptr;
 Settings settings_; SaveManager saves_; AssetPaths assets_; std::vector<PackInfo> packs_; TexturePack pack_; UiAtlas uiAtlas_; PanoramaImage pano_; bool registryReady_=false; std::filesystem::path cacheDir_,packsDir_;
 std::vector<std::filesystem::path> dropped_; std::string noticeMsg_; double noticeUntil_=0;
 std::vector<std::string> skinIds_; std::vector<std::string> skinNames_; std::map<std::string,AtlasImage> skinCache_; int skinScroll_=0; bool editingName_=false; bool panoRotDone_=false; int panoRot_[2]{0,0};
 int food_=20; float sat_=5.f,exh_=0.f,healAcc_=0.f,starveAcc_=0.f,eatProg_=0.f; Vec3 lastPos_{}; struct Container{ InvSlot s[27]; float burn=0.f,burnMax=0.f,cook=0.f; int kind=0; };   // kind 0 chest, 1 furnace (slots 0 input, 1 fuel, 2 output)
 std::unordered_map<uint64_t,Container> containers_; Container ender_; static constexpr uint64_t kEnderKey=0xFFFFFFFFFFFFFFFFull; Container&guiContainer(){ return guiKey_==kEnderKey?ender_:containers_[guiKey_]; }   // the ender chest is one shared inventory per player
 int dragBtn_=0; std::vector<InvSlot*> dragPtrs_; double lastClickT_=-1; int lastClickHov_=-1;   // inventory drag / double click state
 void pollLootChests();
 // ---- entities (GameEntities.cpp)
 EntityMgr ents_; EntityEnv entEnv_; bool entInit_=false,riding_=false; float entAttackCd_=0; uint32_t entTarget_=0; std::unordered_map<ItemId,uint16_t> dynTile_; std::vector<ItemId> dynOwner_; size_t dynNext_=0; std::unordered_set<uint32_t> entUp_;
 float frameMsAvg_=10.f,drawDist_=0.f; bool adaptiveDist_=true; float entAcc_=0.f,tps_=20.f,msptEma_=0.f,tickWin_=0.f; int tickCount_=0;   // 20 Hz tick loop + adaptive draw distance
 std::vector<std::string> entityLines()const; void entitiesReset(const std::string&folder,int dim); void entitiesInit(); void entitiesUpdate(float dt,double now); void entitiesRender(double now); uint16_t itemSpriteTile(ItemId id);
 void entityPre(const Vec3&eye,const Vec3&dir,bool creative,const UiInput&in,float dt); bool entityRightClick(const Vec3&eye,const Vec3&dir,bool click);
 void explodeAt(const Vec3&p,float power,bool breakBlocks,uint32_t src); void drawBossBar(); uint64_t guiKey_=0; std::string guiBlock_; InvSlot craft_[9];
 // dimensions: nether / end through Steel, portals, travel
 double portalCool_=0,portalT_=0,ambMood_=20,ambAdd_=10; void switchDimension(int dim); void travelTo(int dim); void updatePortals(float dt); bool tryLightNetherPortal(int fx,int fy,int fz); bool tryEyeOnFrame(int x,int y,int z); void buildNetherPortal(int x,int y,int z); bool findPortalNear(int x,int z,int&ox,int&oy,int&oz); int safeY(int x,int z,int lo,int hi); void ensureEndExit();
 void fillChestLoot(Container&c,int x,int y,int z); void markPlayerChest(int x,int y,int z);   // chests that came with the terrain get loot on first opening
 std::future<std::string> locateFut_; double tickAcc_=0; unsigned long long gameTicks_=0; int stepTicks_=0; void onTick();
 static uint64_t ckey(int x,int y,int z){return (uint64_t(uint32_t(x)&0x3FFFFFFu)<<38)|(uint64_t(uint32_t(z)&0x3FFFFFFu)<<12)|uint64_t(uint32_t(y)&0xFFFu);}
 bool tryOpenGui(int x,int y,int z); void closeGui(); void screenChest(); void screenCrafting(); void screenGeneric(); void guiInventoryCraft(float px,float py);
 // toasts (top-right pop-ups) and the recipe book
 struct Toast{ std::string title,line; ItemId icon=0; int kind=0; double t0=0; };
 std::deque<Toast> toasts_; std::set<std::string> unlocked_; bool unlockedInit_=false,bookOpen_=false; double nextUnlockScan_=0; int bookScroll_=0;
 void pushToast(const std::string&title,const std::string&line,ItemId icon,int kind); void drawToasts();
 void updateRecipeUnlocks(); void loadRecipeBook(const std::string&folder); void saveRecipeBook(); void drawRecipeBook(float px,float py,bool big); void screenFurnace(); void tickFurnaces(float dt); void guiPlayerSlots(float px,float py,float y0,std::vector<std::pair<InvSlot*,std::pair<float,float>>>&out); void dropContainer(int x,int y,int z);
 void saveContainers(const std::unordered_map<uint64_t,Container>&m,const std::filesystem::path&file)const; void loadContainers(const std::string&folder,int dim=0); void guiSlotsUi(std::vector<std::pair<InvSlot*,std::pair<float,float>>>&slots,int outputIdx,bool craftOut);
 bool newDebug_=false; int newTerrain_=0; void hungerTick(float dt,double now,bool sprinting,bool jumped);
 float health_=20.f,fallPeak_=0.f,eyeH_=1.62f,regenAcc_=0.f; bool dead_=false,wasGround_=true,sneakEdge_=false,inFluidNow_=false,clickGuard_=true,skipTyped_=false,memPressure_=false; double lavaTick_=0,timeAcc_=0,lastMemCheck_=0,lastTrim_=0; int rssMB_=0,sugSel_=0; uint16_t waterId_=0,lavaId_=0;
 std::vector<UiVertex> skyVerts_;
 std::shared_ptr<ChunkStore> store_,tmpStore_; std::future<void> tmpFut_; std::filesystem::path tmpDir_; void flushGenerated(bool wait); std::future<void> saveFut_; bool invJustClosed_=false,f3Down_=false,f3Used_=false; float breakProg_=0.f; int breakX_=0,breakY_=0,breakZ_=0; double breakDelay_=0;
 double perfLast_=0,perfTotal_=0,perfDraw_=0,perfStream_=0; int perfFrames_=0;
 bool worldDirty_=false; double dirtyAt_=0;
 World world_; std::unique_ptr<ChunkStreamer> streamer_;
 Ui ui_; std::vector<UiVertex> frameVerts_,panoVerts_;
 Screen screen_=Screen::Title,optionsFrom_=Screen::Title; bool inWorld_=false;
 // input
 bool keyPrev_[GLFW_KEY_LAST+1]{}; bool mousePrev_[3]{}; float scroll_=0; std::string typed_; double bsNext_=0; bool bsPrev_=false; bool mouseInit_=false; double lastMX_=0,lastMY_=0;
 double lastTime_=0,fpsAcc_=0,time_=0; int fpsFrames_=0,fpsShown_=0;
 // player
 WorldInfo cur_; Vec3 pos_{0,80,0},vel_{}; float yaw_=0,pitch_=0; bool onGround_=false,flying_=false; float specSpeed_=1.f; double lastGrow_=-100.0; double lastJumpPress_=-10; float breakCd_=0,placeCd_=0; Hit hit_;
 double itemNameUntil_=0; int lastSelected_=-1; double lastSave_=0;
 // gui state
 Chat chat_; double chatFadeUntil_=0;
 int rebinding_=-1,focus_=-1; std::vector<WorldInfo> worlds_; int selWorld_=-1; float worldScroll_=0; std::string newName_,newSeed_; int newMode_=MODE_CREATIVE;
 int packSel_=0; float packScroll_=0; InvSlot carried_; int creaTab_=0,creaRow_=0; bool creaDrag_=false; std::string errorMsg_;
 // gamerules
 float tickRate_=20.f; bool doDaylight_=true; bool doWeather_=true;
 long long worldTime_=0; // in game ticks
 // sound
 bool soundsLoaded_=false;
 void initSounds();
 void playSound(const std::string& ev,float vol=1.f,float pitch=1.f);
 std::string soundGroup(BlockId b)const; void blockSound(BlockId b,int kind);   // kind: 0 break, 1 place, 2 step, 3 land
 Vec3 lastSndPos_{}; float stepDist_=0.f; bool sndWasGround_=true,sndWasWater_=false;
 void tickSounds(float dt);
 void registerCommands();
 // loading
 LoadTask loadTask_=LoadTask::None; int loadStep_=0; float loadProgress_=0; double loadStart_=0; std::string loadMsg_; WorldInfo pendingWorld_; Screen loadReturn_=Screen::Title;
};
