cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_STANDARD 14)

# file(GLOB_RECURSE headers CMAKE_CONFIGURE_DEPENDS *.hpp *.h)
# add_library(modern_wxsqlite3 ${headers})

add_library(modern_wxsqlite3 INTERFACE)

target_include_directories(modern_wxsqlite3 INTERFACE ./headers)