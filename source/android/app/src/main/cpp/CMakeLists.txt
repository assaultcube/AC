cmake_minimum_required(VERSION 3.4.1)
add_definitions(-D__ANDROID__)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")

# The cube1 engine codebase assumes that type char is *signed* for example when parsing data from the network (putint/getint).
# However c++ does not specify signedness of char and leaves this up to the platform. On Android the type char is *unsigned*.
# Therefore we need to pass param -fsigned-char to force type char to be signed.
# See also https://stackoverflow.com/a/9518354/50430
# We pass -fexceptions so that we can can catch some SEGFAULTS caused by GL4ES library -> https://github.com/ptitSeb/gl4es/issues/283
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-rtti -fexceptions -Wall -fsigned-char -fstack-protector")

# the assaultcube source code directory
set(ACSRC "../../../../../")
include_directories(${ACSRC}/include/)

# SDL2 2.0.14
add_library(sdl SHARED IMPORTED)
set_target_properties(sdl PROPERTIES IMPORTED_LOCATION  ${PROJECT_SOURCE_DIR}/lib/sdl2/${ANDROID_ABI}/libSDL2.so )

# SDL_image 2.0.5
add_library(sdl_image SHARED IMPORTED)
set_target_properties(sdl_image PROPERTIES IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/lib/sdl_image/${ANDROID_ABI}/libSDL2_image.so )

# openal
add_library(openal_ SHARED IMPORTED)
set_target_properties(openal_ PROPERTIES IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/lib/openal/${ANDROID_ABI}/libopenal.so )

# openal
add_library(cppshared SHARED IMPORTED)
set_target_properties(cppshared PROPERTIES IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/lib/openal/${ANDROID_ABI}/libc++_shared.so )


# GL4ES 1.1.5
add_library(gl4es STATIC IMPORTED)
set_target_properties(gl4es PROPERTIES IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/lib/gl4es/${ANDROID_ABI}/libGL.a )
include_directories(include/gl4es/) # since GL4ES is an Android dependency only we store the headers locally in the android directory

# oggvorbis
add_library(ogg SHARED IMPORTED)
set_target_properties(ogg PROPERTIES IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/lib/oggvorbis/${ANDROID_ABI}/libogg.so )

add_library(vorbis SHARED IMPORTED)
set_target_properties(vorbis PROPERTIES IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/lib/oggvorbis/${ANDROID_ABI}/libvorbis.so )

#include_directories(${PROJECT_SOURCE_DIR}/lib/oggvorbis/${ANDROID_ABI}/include)


find_library(log-lib log)

# enet
add_library(enet
        STATIC
        ${ACSRC}enet/list.c
        ${ACSRC}enet/callbacks.c
        ${ACSRC}enet/compress.c
        ${ACSRC}enet/host.c
        ${ACSRC}enet/packet.c
        ${ACSRC}enet/peer.c
        ${ACSRC}enet/protocol.c
        ${ACSRC}enet/unix.c
        ${ACSRC}enet/win32.c)
include_directories(${ACSRC}/enet/include/)

# assaultcube
add_library(main SHARED
        AssaultCubeLib.cpp
        ${ACSRC}src/crypto.cpp
        ${ACSRC}src/audiomanager.cpp
        ${ACSRC}src/autodownload.cpp
        ${ACSRC}src/client.cpp
        ${ACSRC}src/clientgame.cpp
        ${ACSRC}src/clients2c.cpp
        ${ACSRC}src/command.cpp
        ${ACSRC}src/console.cpp
        ${ACSRC}src/docs.cpp
        ${ACSRC}src/editing.cpp
        ${ACSRC}src/entities.cpp
        ${ACSRC}src/http.cpp
        ${ACSRC}src/log.cpp
        ${ACSRC}src/main.cpp
        ${ACSRC}src/menus.cpp
        ${ACSRC}src/touch/touch.cpp
        ${ACSRC}src/oggstream.cpp
        ${ACSRC}src/openal.cpp
        ${ACSRC}src/packetqueue.cpp
        ${ACSRC}src/physics.cpp
        ${ACSRC}src/protocol.cpp
        ${ACSRC}src/rendercubes.cpp
        ${ACSRC}src/rendergl.cpp
        ${ACSRC}src/renderhud.cpp
        ${ACSRC}src/rendermodel.cpp
        ${ACSRC}src/renderparticles.cpp
        ${ACSRC}src/rendertext.cpp
        ${ACSRC}src/rndmap.cpp
        ${ACSRC}src/scoreboard.cpp
        ${ACSRC}src/serverms.cpp
        ${ACSRC}src/server.cpp
        ${ACSRC}src/serverbrowser.cpp
        ${ACSRC}src/shadow.cpp
        ${ACSRC}src/soundlocation.cpp
        ${ACSRC}src/soundscheduler.cpp
        ${ACSRC}src/stream.cpp
        ${ACSRC}src/texture.cpp
        ${ACSRC}src/tools.cpp
        ${ACSRC}src/water.cpp
        ${ACSRC}src/weapon.cpp
        ${ACSRC}src/wizard.cpp
        ${ACSRC}src/world.cpp
        ${ACSRC}src/worldio.cpp
        ${ACSRC}src/worldlight.cpp
        ${ACSRC}src/worldocull.cpp
        ${ACSRC}src/worldrender.cpp
        ${ACSRC}src/zip.cpp
        ${ACSRC}src/bot/bot.cpp
        ${ACSRC}src/bot/botmanager.cpp
        ${ACSRC}src/bot/bot_ai.cpp
        ${ACSRC}src/bot/bot_util.cpp
        ${ACSRC}src/bot/bot_waypoint.cpp
        ${ACSRC}src/bot/ac_bot.cpp
        ${ACSRC}src/bot/ac_bot_ai.cpp
        )
include_directories(${ACSRC}/src/)

# required for address sanitizer
# see also https://developer.android.com/ndk/guides/asan#cmake
#target_compile_options(main PUBLIC -fsanitize=address -fno-omit-frame-pointer)
#set_target_properties(main PROPERTIES LINK_FLAGS -fsanitize=address)

# assaultcube client
target_link_libraries(main
            gl4es
            android
            sdl
            sdl_image
            openal_
            enet
            z
            vorbis
            ogg
            GLESv2
            OpenSLES
            ${log-lib}
            EGL
            cppshared
            m)

# assaultcube server - this will be required once we want to implement P2P gaming where one of the mobile clients will host a server
#[[
add_library(server SHARED
        AssaultCubeServer.cpp
        ${ACSRC}src/crypto.cpp
        ${ACSRC}src/log.cpp
        ${ACSRC}src/protocol.cpp
        ${ACSRC}src/serverms.cpp
        ${ACSRC}src/server.cpp
        ${ACSRC}src/stream.cpp
        ${ACSRC}src/tools.cpp
        ${ACSRC}src/wizard.cpp
        )
target_compile_definitions(server PUBLIC STANDALONE)

target_link_libraries(server
        android
        sdl
        sdl_image
        enet
        z
        ${log-lib}
        cppshared
        m)
]]