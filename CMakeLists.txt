cmake_minimum_required(VERSION 4.0.0)

include(FetchContent)

project(ASCIIDraw VERSION 0.1.0 LANGUAGES CXX C)

FetchContent_Declare(
	XPERMath 
	GIT_REPOSITORY https://github.com/glasvizax/XPERMath 
	GIT_TAG v1.0 
	SYSTEM
)

FetchContent_MakeAvailable(XPERMath)

set(SRC_FILES main.cpp)

add_executable(Main ${SRC_FILES})

target_link_libraries(Main PRIVATE XPER::Math) 
