cmake_minimum_required(VERSION 3.24)

if (DEFINED ENV{METAMODULE_SDK_DIR})
    set(METAMODULE_SDK_DIR "$ENV{METAMODULE_SDK_DIR}")
elseif("${METAMODULE_SDK_DIR}" STREQUAL "")
    set(METAMODULE_SDK_DIR "${CMAKE_CURRENT_LIST_DIR}/../metamodule-plugin-sdk")
endif()
include(${METAMODULE_SDK_DIR}/plugin.cmake)

project(Bidoo VERSION 2.1.1 DESCRIPTION "Bidoo Plugin Pack" LANGUAGES C CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Define source directories
set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Bidoo)
set(SRC_DIR ${SOURCE_DIR}/src)
set(DEP_DIR ${SRC_DIR}/dep)

add_library(Bidoo STATIC)

target_compile_definitions(Bidoo PRIVATE 
    __V1x 
    __PLUGIN
)

# Add compile options for better control over symbols
target_compile_options(Bidoo PRIVATE
    -fdata-sections
    -ffunction-sections
    -fno-exceptions
    -fno-threadsafe-statics
    -fno-use-cxa-atexit
    $<$<CXX_COMPILER_ID:GNU>:-fvisibility=hidden>
    $<$<CXX_COMPILER_ID:GNU>:-fvisibility-inlines-hidden>
)

# Add linker flags for static linking
target_link_options(Bidoo PRIVATE
    -Wl,--gc-sections
    -static-libgcc
    -static-libstdc++
)

# Add source files
target_sources(Bidoo PRIVATE
    ${SRC_DIR}/dep/quantizer.cpp
    ${SRC_DIR}/dep/freeverb/revmodel.cpp
    ${SRC_DIR}/dep/freeverb/comb.cpp
    ${SRC_DIR}/dep/freeverb/allpass.cpp
    ${SRC_DIR}/ACNE.cpp
    ${SRC_DIR}/BAFIS.cpp
    ${SRC_DIR}/BANCAU.cpp
    ${SRC_DIR}/BAR.cpp
    ${SRC_DIR}/BISTROT.cpp
    ${SRC_DIR}/BORDL.cpp
    ${SRC_DIR}/CHUTE.cpp
    ${SRC_DIR}/DIKTAT.cpp
    ${SRC_DIR}/DILEMO.cpp
    ${SRC_DIR}/DTROY.cpp
    ${SRC_DIR}/DUKE.cpp
    ${SRC_DIR}/DFUZE.cpp
    ${SRC_DIR}/FREIN.cpp
    ${SRC_DIR}/FORK.cpp
    ${SRC_DIR}/HCTIP.cpp
    ${SRC_DIR}/HUITRE.cpp
    ${SRC_DIR}/LAMBDA.cpp
    ${SRC_DIR}/LATE.cpp
    ${SRC_DIR}/LOURDE.cpp
    ${SRC_DIR}/LIMBO.cpp
    ${SRC_DIR}/LOURDE.cpp
    ${SRC_DIR}/MINIBAR.cpp
    ${SRC_DIR}/MS.cpp
    ${SRC_DIR}/plugin.cpp
    ${SRC_DIR}/PERCO.cpp
    ${SRC_DIR}/RABBIT.cpp
    ${SRC_DIR}/REI.cpp
    ${SRC_DIR}/SIGMA.cpp
    ${SRC_DIR}/SPORE.cpp
    ${SRC_DIR}/TIARE.cpp
    ${SRC_DIR}/TOCANTE.cpp
    ${SRC_DIR}/ZINC.cpp
    # Add dependency files as needed
    # ${DEP_DIR}/filters/*.cpp
    # ${DEP_DIR}/freeverb/*.cpp
    # ${DEP_DIR}/gverb/src/*.c
    # ${DEP_DIR}/lodepng/*.cpp
    # ${DEP_DIR}/pffft/*.c
    # ${DEP_DIR}/resampler/*.cpp
)

# Add include directories
target_include_directories(Bidoo PRIVATE 
    ${SOURCE_DIR}
    ${DEP_DIR}
    ${DEP_DIR}/dr_wav
    ${DEP_DIR}/filters
    ${DEP_DIR}/freeverb
    ${DEP_DIR}/gverb/include
    ${DEP_DIR}/minimp3
    ${DEP_DIR}/lodepng
    ${DEP_DIR}/pffft
    ${DEP_DIR}/AudioFile
    ${DEP_DIR}/resampler
)

# Create the plugin file
create_plugin(
    SOURCE_LIB      Bidoo
    PLUGIN_NAME     Bidoo
    PLUGIN_JSON     ${SOURCE_DIR}/plugin.json
    SOURCE_ASSETS   ${CMAKE_CURRENT_LIST_DIR}/assets
    DESTINATION     ${CMAKE_CURRENT_LIST_DIR}/metamodule-plugins
) 