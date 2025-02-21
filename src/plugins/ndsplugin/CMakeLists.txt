cmake_minimum_required(VERSION 3.12 )
project(cpp-mods.musicplayer.ndsplugin)

set(SOURCE_FILES
    NDSPlugin.cpp
    nds/vio2sf/vio2sf.c
    nds/vio2sf/desmume/arm_instructions.c
    nds/vio2sf/desmume/armcpu.c
    nds/vio2sf/desmume/bios.c
    nds/vio2sf/desmume/cp15.c
    nds/vio2sf/desmume/FIFO.c
    nds/vio2sf/desmume/GPU.c
    nds/vio2sf/desmume/matrix.c
    nds/vio2sf/desmume/mc.c
    nds/vio2sf/desmume/MMU.c
    nds/vio2sf/desmume/NDSSystem.c
    nds/vio2sf/desmume/SPU.c
    nds/vio2sf/desmume/thumb_instructions.c
)

add_library(nds ${SOURCE_FILES})
target_include_directories(nds PRIVATE nds/vio2sf nds/vio2sf/desmume nds)
target_link_libraries(nds PRIVATE psf coreutils)
if(NOT WIN32)
    target_compile_options(nds PRIVATE -Wno-incompatible-function-pointer-types)
endif()
target_compile_definitions(nds PRIVATE -DLSB_FIRST -DHAVE_STDINT_H)
# -D_strnicmp=strncasecmp)

add_library(ndsplugin OBJECT ndsplugin_register.cpp)
target_link_libraries(ndsplugin PRIVATE nds)
