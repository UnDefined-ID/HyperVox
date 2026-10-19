cmake_minimum_required(VERSION 3.20)
project(hypervox LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(PNG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenAL QUIET)
find_library(VORBISFILE_LIB NAMES vorbisfile libvorbisfile)
find_package(Threads REQUIRED)
find_program(GLSLC glslc HINTS $ENV{VULKAN_SDK}/Bin $ENV{VULKAN_SDK}/bin)
find_program(GLSLANG_VALIDATOR glslangValidator HINTS $ENV{VULKAN_SDK}/Bin $ENV{VULKAN_SDK}/bin)
if(NOT GLSLC AND NOT GLSLANG_VALIDATOR)
  message(FATAL_ERROR "Install a Vulkan shader compiler: glslc or glslangValidator")
endif()

add_executable(minecraft
  src/main.cpp src/Game.cpp src/GameUi.cpp src/Ui.cpp src/UiAtlas.cpp src/Assets.cpp src/Png.cpp src/TexturePack.cpp src/Registry.cpp
  src/World.cpp src/Streamer.cpp src/Settings.cpp src/SaveGame.cpp src/SaveAnvil.cpp src/Anvil.cpp src/VulkanRenderer.cpp src/GlRenderer.cpp src/MemUtil.cpp src/Climate.cpp src/Mining.cpp src/ChunkStore.cpp
  src/Sound.cpp src/Chat.cpp src/GameCmd.cpp)
target_include_directories(minecraft PRIVATE src)
target_link_libraries(minecraft PRIVATE Vulkan::Vulkan glfw PNG::PNG ZLIB::ZLIB Threads::Threads)
if(OpenAL_FOUND OR OPENAL_FOUND)
  target_link_libraries(minecraft PRIVATE OpenAL::OpenAL)
  message(STATUS "OpenAL found - audio enabled at link time (still uses dlopen at runtime)")
endif()
if(VORBISFILE_LIB)
  target_link_libraries(minecraft PRIVATE ${VORBISFILE_LIB})
endif()
if(MSVC)
  target_compile_options(minecraft PRIVATE /O2 /W3 /utf-8)
else()
  target_compile_options(minecraft PRIVATE -O3 -DNDEBUG -march=native -ffast-math -Wall -Wextra)
endif()

# ---- shaders ------------------------------------------------------------------------------------------------------
set(SHADER_OUT ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUT})
set(SHADER_FILES block.vert block.frag ui.vert ui.frag line.vert line.frag)
set(SHADER_SPV "")
foreach(s ${SHADER_FILES})
  if(GLSLC)
    set(cmd ${GLSLC} -O ${CMAKE_SOURCE_DIR}/shaders/${s} -o ${SHADER_OUT}/${s}.spv)
  else()
    set(cmd ${GLSLANG_VALIDATOR} -V -Os ${CMAKE_SOURCE_DIR}/shaders/${s} -o ${SHADER_OUT}/${s}.spv)
  endif()
  add_custom_command(OUTPUT ${SHADER_OUT}/${s}.spv COMMAND ${cmd} DEPENDS ${CMAKE_SOURCE_DIR}/shaders/${s})
  list(APPEND SHADER_SPV ${SHADER_OUT}/${s}.spv)
endforeach()
add_custom_target(shaders DEPENDS ${SHADER_SPV})
add_dependencies(minecraft shaders)
add_custom_command(TARGET minecraft POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADER_OUT} $<TARGET_FILE_DIR:minecraft>/shaders)

# ---- texture pack: assets/minecraft-assets.zip (or an already extracted assets/block folder) -----------------------
set(ASSET_STAMP ${CMAKE_BINARY_DIR}/assets/.extracted)
if(EXISTS ${CMAKE_SOURCE_DIR}/assets/minecraft-assets.zip)
  add_custom_command(OUTPUT ${ASSET_STAMP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/assets
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}/assets ${CMAKE_COMMAND} -E tar xf ${CMAKE_SOURCE_DIR}/assets/minecraft-assets.zip
    COMMAND ${CMAKE_COMMAND} -E touch ${ASSET_STAMP}
    DEPENDS ${CMAKE_SOURCE_DIR}/assets/minecraft-assets.zip)
  add_custom_target(extract_assets ALL DEPENDS ${ASSET_STAMP})
  add_dependencies(minecraft extract_assets)
elseif(EXISTS ${CMAKE_SOURCE_DIR}/assets/block)
  add_custom_target(extract_assets ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/assets)
  add_dependencies(minecraft extract_assets)
else()
  message(WARNING "No texture pack found. Put minecraft-assets.zip into assets/ (the game needs it to start).")
endif()

# ---- sounds -----------------------------------------------------------------------------------
if(EXISTS ${CMAKE_SOURCE_DIR}/assets/minecraft-sounds.zip)
  set(SOUND_STAMP ${CMAKE_BINARY_DIR}/sounds/.extracted)
  add_custom_command(OUTPUT ${SOUND_STAMP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/sounds
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}/sounds ${CMAKE_COMMAND} -E tar xf ${CMAKE_SOURCE_DIR}/assets/minecraft-sounds.zip
    COMMAND ${CMAKE_COMMAND} -E touch ${SOUND_STAMP}
    DEPENDS ${CMAKE_SOURCE_DIR}/assets/minecraft-sounds.zip)
  add_custom_target(extract_sounds ALL DEPENDS ${SOUND_STAMP})
  add_dependencies(minecraft extract_sounds)
  add_custom_command(TARGET minecraft POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/sounds $<TARGET_FILE_DIR:minecraft>/sounds)
else()
  message(STATUS "No minecraft-sounds.zip found in assets/ - game will run silent")
endif()
