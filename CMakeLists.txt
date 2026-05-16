cmake_minimum_required(VERSION 3.30)

include(FetchContent)

macro(set_policy policy state)
    if(POLICY ${policy})
        cmake_policy(SET ${policy} ${state})
    else()
        message(
            WARNING 
            "Policy ${policy} is not"
            "available in this CMake version"    
        )
    endif()
endmacro()

set_policy(CMP0167 NEW)
set_policy(CMP0168 NEW)
set_policy(CMP0135 NEW)
set_policy(CMP0091 NEW)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

cmake_path(
    APPEND CMAKE_CURRENT_SOURCE_DIR 
    "external_libs" 
    OUTPUT_VARIABLE EXTERNAL_LIBS_DIR
)

cmake_path(SET FETCHCONTENT_BASE_DIR ${EXTERNAL_LIBS_DIR})

cmake_path(
    APPEND CMAKE_CURRENT_SOURCE_DIR 
    "data" 
    OUTPUT_VARIABLE DATA_DIR
)

cmake_path(
    APPEND CMAKE_CURRENT_SOURCE_DIR 
    "src" 
    OUTPUT_VARIABLE SRC_DIR
)

if(EXISTS "$ENV{ASSIMP_DIR}")
    cmake_path(SET ASSIMP_DIR "$ENV{ASSIMP_DIR}")    
else()
    message(
        FATAL_ERROR 
        "couldn't find assimp, specify environment variable"
        "ASSIMP_DIR to assimp root directory"
    )
endif() 

cmake_path(
    APPEND ASSIMP_DIR 
    "build"
    OUTPUT_VARIABLE ASSIMP_BUILD_DIR
)

project(
    ASCIIDraw 
    VERSION 1.0.0 
    LANGUAGES CXX
)

block()
    set(CMAKE_FOLDER "ExternalLibs/Assimp")
    set(BUILD_SHARED_LIBS OFF)
    add_subdirectory(${ASSIMP_DIR} ${ASSIMP_BUILD_DIR} SYSTEM)
endblock()

FetchContent_Declare(
    XPERCMakeUtils
    GIT_REPOSITORY https://github.com/glasvizax/XPERCMakeUtils
    GIT_TAG v1.4
)

FetchContent_Declare(
	XPERMath 
	GIT_REPOSITORY https://github.com/glasvizax/XPERMath 
	GIT_TAG v1.2 
	SYSTEM
)

FetchContent_MakeAvailable(
    XPERCMakeUtils 
    XPERMath
)

include(fetch_stb NO_POLICY_SCOPE)

fetch_stb(
    DOWNLOAD_PATH ${EXTERNAL_LIBS_DIR}
    HEADERS 
        stb_image.h
        stb_image_write.h
)

target_compile_definitions(
    stb
    PUBLIC 
        STBI_WINDOWS_UTF8
)

# TODO: fix
file(
    GLOB_RECURSE 
    SRC_FILES 
    CONFIGURE_DEPENDS 
        ${SRC_DIR}/*.h 
        ${SRC_DIR}/*.cpp
)   

file(
    GLOB_RECURSE 
    DATA_FILES 
    CONFIGURE_DEPENDS 
        ${DATA_DIR}/*
)

set_source_files_properties(
    ${DATA_FILES} 
    PROPERTIES
        HEADER_FILE_ONLY ON
)

add_executable(
    Main 
        ${SRC_FILES} 
        ${DATA_FILES}
)

source_group(
    Sources 
    FILES 
       ${SRC_FILES}
)

source_group(
    Data 
    FILES 
        ${DATA_FILES}
)

target_link_libraries(
    Main 
    PRIVATE 
        XPER::Math
        stb
        assimp::assimp
) 

target_compile_definitions(
    Main
    PRIVATE
        $<$<CONFIG:Debug>:DATA_PATH="${DATA_DIR}">
        NOMINMAX
)

include(add_targets_to_folder)

add_targets_to_folder(
    FOLDER ExternalLibs 
    TARGETS 
        XPER::Math
        XPER::CMakeUtils
        stb        
)

include(add_copy_dir_dependency)

add_copy_dir_dependency(
    TARGET Main 
    DIRECTORY ${DATA_DIR} 
    INCLUDE_ROOT
    CONDITION $<CONFIG:Release>
)

set_directory_properties(
    PROPERTIES 
        VS_STARTUP_PROJECT Main
)

set_target_properties(
    Main 
    PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Main>"
)

include(GNUInstallDirs)

install(
    TARGETS Main
    CONFIGURATIONS Release
)

install(
    DIRECTORY ${DATA_DIR}
    TYPE BIN
    CONFIGURATIONS Release
)

# install(IMPORTED_RUNTIME_ARTIFACTS assimp::assimp)

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "glasvizax")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY 
    "${PROJECT_NAME} - console software renderer"
)
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})
set(CPACK_VERBATIM_VARIABLES ON)
set(CPACK_GENERATOR "NSIS;ZIP")

include(CPack)

# TODO : add precompile headers, batching, logging, remove file(GLOB ...), 
#        add more compatibility with single config generators, add unix cpack generator support