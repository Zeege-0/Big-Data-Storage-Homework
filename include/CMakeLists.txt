
add_library(argparse INTERFACE)
target_compile_features(argparse INTERFACE cxx_std_17)
target_include_directories(argparse INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(indicators INTERFACE)
target_compile_features(indicators INTERFACE cxx_std_17)
target_include_directories(indicators INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
