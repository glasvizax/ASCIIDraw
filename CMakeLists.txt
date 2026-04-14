cmake_minimum_required(VERSION 4.0.0)

include(FetchContent)

project(ASCIIDraw VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EXTERNAL_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.external_libs")
set(FETCHCONTENT_BASE_DIR "${EXTERNAL_LIBS_DIR}/fetch_content_libs")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

FetchContent_Declare(
	XPERMath 
	GIT_REPOSITORY https://github.com/glasvizax/XPERMath 
	GIT_TAG v1.0 
	SYSTEM
)

FetchContent_MakeAvailable(XPERMath)

set(STB_DIR "${EXTERNAL_LIBS_DIR}/stb")
file(MAKE_DIRECTORY ${STB_DIR})

function(download_stb name)
    if(NOT EXISTS ${STB_DIR}/${name})
        file(DOWNLOAD
            https://raw.githubusercontent.com/nothings/stb/28d546d5eb77d4585506a20480f4de2e706dff4c/${name}
            ${STB_DIR}/${name}
        )
    endif()
endfunction()

download_stb(stb_image.h)
download_stb(stb_image_write.h)

file(GENERATE OUTPUT _stb_impl.cpp
     CONTENT [==[
        #define STB_IMAGE_IMPLEMENTATION
        #include "stb_image.h"

        #define STB_IMAGE_WRITE_IMPLEMENTATION
        #include "stb_image_write.h"
     ]==]
)

add_library(stb STATIC _stb_impl.cpp ${STB_DIR}/stb_image.h ${STB_DIR}/stb_image_write.h)

target_include_directories(stb PUBLIC ${STB_DIR})

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.h src/*.cpp)

set(SRC_FILES ${SOURCES})

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



