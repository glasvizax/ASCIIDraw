cmake_minimum_required(VERSION 4.0.0)

include(FetchContent)

project(ASCIIDraw VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

FetchContent_Declare(
	XPERMath 
	GIT_REPOSITORY https://github.com/glasvizax/XPERMath 
	GIT_TAG v1.0 
	SYSTEM
)

FetchContent_MakeAvailable(XPERMath)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.h src/*.cpp)

set(SRC_FILES ${SOURCES})

add_executable(Main ${SRC_FILES})

target_link_libraries(Main PRIVATE XPER::Math) 
