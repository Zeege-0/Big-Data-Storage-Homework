
add_executable(hddssd main.cpp)

target_include_directories(hddssd PUBLIC "${FUSE_PATH}/include")
target_link_directories(hddssd PUBLIC "${FUSE_PATH}/lib/x86_64-linux-gnu")
target_link_libraries(hddssd PUBLIC fuse3 pthread rt)
# target_link_options(hddssd PUBLIC "LINKER:-rpath,${FUSE_PATH}/include/")

target_link_libraries(hddssd PUBLIC argparse)
target_link_libraries(hddssd PUBLIC indicators)
