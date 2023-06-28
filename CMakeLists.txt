cmake_minimum_required(VERSION 3.15)
project(HDDSSD VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_FLAGS "-Wall -fno-rtti -Wzero-as-null-pointer-constant")
set(FUSE_PATH "${CMAKE_HOME_DIRECTORY}/fuse")

add_subdirectory(include)
add_subdirectory(src)
