cmake_minimum_required(VERSION 3.30)

include(FetchContent)

project(ASCIIDraw VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

cmake_path(APPEND CMAKE_CURRENT_SOURCE_DIR ".external_libs" OUTPUT_VARIABLE EXTERNAL_LIBS_DIR)
cmake_path(APPEND FETCHCONTENT_BASE_DIR ${EXTERNAL_LIBS_DIR} "fetch_content_libs")

cmake_path(APPEND CMAKE_CURRENT_SOURCE_DIR "data" OUTPUT_VARIABLE DATA_DIR)
cmake_path(APPEND CMAKE_CURRENT_SOURCE_DIR "src" OUTPUT_VARIABLE SRC_DIR)

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
    GIT_TAG v1.3
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

list(APPEND CMAKE_MODULE_PATH "${XPERCMakeUtils_SOURCE_DIR}")

include(fetch_stb)

fetch_stb(
    SRC_PATH ${EXTERNAL_LIBS_DIR}
    HEADERS 
        stb_image.h 
        stb_image_write.h 
)

# TODO: fix
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS 
    ${SRC_DIR}/*.h 
    ${SRC_DIR}/*.cpp
)   

add_executable(Main ${SRC_FILES})

target_link_libraries(
    Main 
    PRIVATE 
        XPER::Math
        stb
) 

file(GLOB_RECURSE DATA_FILES CONFIGURE_DEPENDS 
    ${DATA_DIR}/*
)

target_sources(
    Main
    PRIVATE
        ${DATA_FILES}
)

target_compile_definitions(
    Main
    PRIVATE
        $<$<CONFIG:Debug>:DATA_PATH="${DATA_DIR}">
        NOMINMAX
)

set_directory_properties(
    PROPERTIES 
        VS_STARTUP_PROJECT Main
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

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "glasvizax")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_NAME} - console software renderer")
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})
set(CPACK_VERBATIM_VARIABLES ON)
set(CPACK_GENERATOR NSIS)

include(CPack)
