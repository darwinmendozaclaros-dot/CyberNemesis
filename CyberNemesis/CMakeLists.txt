cmake_minimum_required(VERSION 3.15)
project(CyberNemesis VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(CN_WINDOWS_CONSOLE "Windows: mostrar ventana de consola junto al juego (solo para depurar)" OFF)
option(CN_USE_SYSTEM_RAYLIB "Usar la Raylib instalada en el sistema (puede no tener Wayland)" OFF)
set(CN_WAYLAND "AUTO" CACHE STRING "Soporte Wayland en Linux: AUTO | ON | OFF")
set(CN_X11     "AUTO" CACHE STRING "Soporte X11 en Linux: AUTO | ON | OFF")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-ffile-prefix-map=${CMAKE_SOURCE_DIR}=. -ffile-prefix-map=${CMAKE_BINARY_DIR}=.)
elseif(MSVC)
  file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}" CN_SRC_NATIVE)
  file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}" CN_BIN_NATIVE)
  add_compile_options(/d1trimfile:${CMAKE_SOURCE_DIR}/ /d1trimfile:${CN_SRC_NATIVE}\\
                      /d1trimfile:${CMAKE_BINARY_DIR}/ /d1trimfile:${CN_BIN_NATIVE}\\)
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CN_HAVE_RC OFF)
if(WIN32)
  enable_language(RC)
  set(CN_HAVE_RC ON)
endif()

if(CN_USE_SYSTEM_RAYLIB)
  find_package(raylib 5.0 REQUIRED)
else()
  set(CN_BUILD_WAYLAND ON)
  set(CN_BUILD_X11 ON)

  if(UNIX AND NOT APPLE)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
      pkg_check_modules(CN_WL QUIET wayland-client wayland-cursor wayland-egl xkbcommon
                                    wayland-protocols wayland-scanner)
      pkg_check_modules(CN_X  QUIET x11 xrandr xi xcursor xinerama)

      if(CN_WAYLAND STREQUAL "AUTO")
        if(NOT CN_WL_FOUND)
          set(CN_BUILD_WAYLAND OFF)
          message(WARNING "Wayland desactivado: faltan paquetes de desarrollo "
                          "(wayland, wayland-protocols, libxkbcommon). Ver README.")
        endif()
      endif()
      if(CN_X11 STREQUAL "AUTO")
        if(NOT CN_X_FOUND)
          set(CN_BUILD_X11 OFF)
          message(STATUS "X11 desactivado: faltan paquetes de desarrollo de Xorg.")
        endif()
      endif()
    endif()
    if(CN_WAYLAND STREQUAL "OFF")
      set(CN_BUILD_WAYLAND OFF)
    elseif(CN_WAYLAND STREQUAL "ON")
      set(CN_BUILD_WAYLAND ON)
    endif()
    if(CN_X11 STREQUAL "OFF")
      set(CN_BUILD_X11 OFF)
    elseif(CN_X11 STREQUAL "ON")
      set(CN_BUILD_X11 ON)
    endif()

    if(NOT CN_BUILD_WAYLAND AND NOT CN_BUILD_X11)
      set(CN_BUILD_X11 ON)
    endif()
    message(STATUS "CYBER-NEMESIS backends de ventana -> Wayland: ${CN_BUILD_WAYLAND} | X11: ${CN_BUILD_X11}")
    set(GLFW_BUILD_WAYLAND ${CN_BUILD_WAYLAND} CACHE BOOL "" FORCE)
    set(GLFW_BUILD_X11     ${CN_BUILD_X11}     CACHE BOOL "" FORCE)
  endif()

  include(FetchContent)
  FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW ON)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(raylib)
endif()


set(CN_SOURCES
  src/main.cpp
  src/core/Game.cpp
  src/core/Paths.cpp
  src/entities/Entity.cpp
  src/entities/Arena.cpp
  src/entities/Hazards.cpp
  src/entities/Drones.cpp
  src/entities/Pickups.cpp
  src/entities/Bullets.cpp
  src/entities/Boss.cpp
  src/entities/Hero.cpp
  src/entities/Projectile.cpp
  src/ai/PatternTracker.cpp
  src/ai/AdaptiveBrain.cpp
  src/ai/FinalDirector.cpp
  src/gfx/Sprites.cpp
  src/gfx/Particles.cpp
  src/gfx/Background.cpp
  src/gfx/PostFX.cpp
  src/gfx/HUD.cpp
  src/gfx/ShardField.cpp
  src/gfx/MainMenu.cpp
  src/audio/AudioManager.cpp
)

if(CN_HAVE_RC)
  
  set(CN_ICON_PATH "${CMAKE_SOURCE_DIR}/assets/windows/icon.ico")
  configure_file(assets/windows/app.rc.in ${CMAKE_BINARY_DIR}/app.rc @ONLY)
  list(APPEND CN_SOURCES ${CMAKE_BINARY_DIR}/app.rc)
endif()

add_executable(CyberNemesis ${CN_SOURCES})
target_include_directories(CyberNemesis PRIVATE src)
target_link_libraries(CyberNemesis PRIVATE raylib)

if(MSVC)
  target_compile_options(CyberNemesis PRIVATE /W3 /utf-8)
  target_compile_definitions(CyberNemesis PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(CyberNemesis PRIVATE -Wall -Wno-missing-field-initializers)
endif()

if(WIN32 AND NOT CN_WINDOWS_CONSOLE)
  if(MSVC)
    target_link_options(CyberNemesis PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
  else()
    target_link_options(CyberNemesis PRIVATE -mwindows)
  endif()
endif()

if(MINGW)
  target_link_options(CyberNemesis PRIVATE -static -static-libgcc -static-libstdc++ $<$<CONFIG:Release>:-s>)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT APPLE)
  target_link_options(CyberNemesis PRIVATE -static-libgcc -static-libstdc++ $<$<CONFIG:Release>:-s>)
endif()

add_custom_command(TARGET CyberNemesis POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets/shaders $<TARGET_FILE_DIR:CyberNemesis>/assets/shaders)

if(WIN32)
  set(CN_PKG_NAME "CyberNemesis-windows-x64.zip")
  set(CN_TAR_MODE cf)
  set(CN_TAR_FMT --format=zip)
elseif(APPLE)
  set(CN_PKG_NAME "CyberNemesis-macos.zip")
  set(CN_TAR_MODE cf)
  set(CN_TAR_FMT --format=zip)
else()
  set(CN_PKG_NAME "CyberNemesis-linux-x64.tar.gz")
  set(CN_TAR_MODE czf)
  set(CN_TAR_FMT "")
endif()
set(CN_STAGE "${CMAKE_BINARY_DIR}/dist/CyberNemesis")

add_custom_target(package_game
  COMMAND ${CMAKE_COMMAND} -E remove_directory "${CN_STAGE}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CN_STAGE}"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:CyberNemesis>" "${CN_STAGE}/"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/dist/LEEME.txt" "${CN_STAGE}/"
  COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_BINARY_DIR}/dist"
          ${CMAKE_COMMAND} -E tar ${CN_TAR_MODE} "${CN_PKG_NAME}" ${CN_TAR_FMT} CyberNemesis
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/release"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:CyberNemesis>" "${CMAKE_SOURCE_DIR}/release/"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/dist/LEEME.txt" "${CMAKE_SOURCE_DIR}/release/"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/dist/${CN_PKG_NAME}" "${CMAKE_SOURCE_DIR}/release/"
  DEPENDS CyberNemesis
  COMMENT "Empaquetando ${CN_PKG_NAME}"
  VERBATIM)
