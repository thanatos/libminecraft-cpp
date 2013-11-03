cmake_minimum_required(VERSION 2.8)

project(nbt)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(
	nbt SHARED
	src/reader.cxx
	src/string.cxx
	src/utility.cxx
)
