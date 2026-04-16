cmake_minimum_required(VERSION 4.0.0)

include(FetchContent)

project(ASCIIDraw VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EXTERNAL_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.external_libs")
set(FETCHCONTENT_BASE_DIR "${EXTERNAL_LIBS_DIR}/fetch_content_libs")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_POLICY_DEFAULT_CMP0168 NEW)

FetchContent_Declare(
    XPERCMakeUtils
    GIT_REPOSITORY https://github.com/glasvizax/XPERCMakeUtils
    GIT_TAG v1.1
)

FetchContent_Declare(
	XPERMath 
	GIT_REPOSITORY https://github.com/glasvizax/XPERMath 
	GIT_TAG v1.0 
	SYSTEM
)

FetchContent_MakeAvailable(XPERCMakeUtils XPERMath)

list(APPEND CMAKE_MODULE_PATH "${XPERCMakeUtils_SOURCE_DIR}")

include(fetch_stb)

fetch_stb(HEADERS stb_image.h stb_image_write.h SRC_PATH ${EXTERNAL_LIBS_DIR})

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS src/*.h src/*.cpp)

add_executable(Main ${SRC_FILES})

target_link_libraries(
    Main 
    PRIVATE 
        XPER::Math
        stb
) 

file(GLOB_RECURSE DATA CONFIGURE_DEPENDS data/*)

target_sources(
    Main
    PRIVATE
        ${DATA}
)

target_compile_definitions(
    Main
    PRIVATE
        DATA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/"
)

set_target_properties(
    stb 
    PROPERTIES 
        FOLDER ExternalLibs
)