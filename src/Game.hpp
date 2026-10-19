#pragma once
#include <GLFW/glfw3.h>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "Assets.hpp"
#include "Inventory.hpp"
#include "Registry.hpp"
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
 enum class Screen{Title,WorldSelect,CreateWorld,ConfirmDelete,Options,Controls,Textures,Account,Death,Loading,Pause,Inventory,Creative,Play};
 enum class LoadTask{None,EnterWorld,ReloadPack};
 struct Hit{ bool hit=false; int x=0,y=0,z=0,nx=0,ny=0,nz=0; float hy=0.5f; };

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
 int food_=20; float sat_=5.f,exh_=0.f,healAcc_=0.f,starveAcc_=0.f,eatProg_=0.f; Vec3 lastPos_{}; bool newDebug_=false; void hungerTick(float dt,double now,bool sprinting,bool jumped);
 float health_=20.f,fallPeak_=0.f,eyeH_=1.62f,regenAcc_=0.f; bool dead_=false,wasGround_=true,sneakEdge_=false,inFluidNow_=false,clickGuard_=true,skipTyped_=false,memPressure_=false; double lavaTick_=0,timeAcc_=0,lastMemCheck_=0,lastTrim_=0; int rssMB_=0,sugSel_=0; uint16_t waterId_=0,lavaId_=0;
 std::vector<UiVertex> skyVerts_;
 std::shared_ptr<ChunkStore> store_; std::future<void> saveFut_; bool invJustClosed_=false,f3Down_=false,f3Used_=false; float breakProg_=0.f; int breakX_=0,breakY_=0,breakZ_=0; double breakDelay_=0;
 bool worldDirty_=false; double dirtyAt_=0;
 World world_; std::unique_ptr<ChunkStreamer> streamer_;
 Ui ui_; std::vector<UiVertex> frameVerts_,panoVerts_;
 Screen screen_=Screen::Title,optionsFrom_=Screen::Title; bool inWorld_=false;
 // input
 bool keyPrev_[GLFW_KEY_LAST+1]{}; bool mousePrev_[3]{}; float scroll_=0; std::string typed_; double bsNext_=0; bool bsPrev_=false; bool mouseInit_=false; double lastMX_=0,lastMY_=0;
 double lastTime_=0,fpsAcc_=0,time_=0; int fpsFrames_=0,fpsShown_=0;
 // player
 WorldInfo cur_; Vec3 pos_{0,80,0},vel_{}; float yaw_=0,pitch_=0; bool onGround_=false,flying_=false; double lastJumpPress_=-10; float breakCd_=0,placeCd_=0; Hit hit_;
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
 void tickSounds(float dt);
 void registerCommands();
 // loading
 LoadTask loadTask_=LoadTask::None; int loadStep_=0; float loadProgress_=0; double loadStart_=0; std::string loadMsg_; WorldInfo pendingWorld_; Screen loadReturn_=Screen::Title;
};
