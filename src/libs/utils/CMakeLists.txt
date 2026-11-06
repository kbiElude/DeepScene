# Utils: what no other library owns and any of them could use. It depends on third-party code and the system, and on nothing of this project (a test,
# Layout, checks that). Each part is a folder: images (PNG and GIF files, comparing and putting images together), binary_io (bytes in and out), json,
# hashing (xxHash), parallel (a loop over several threads), statistics (a series of timings), files (a file from the network, checked, unpacked, and
# the file system under them), ui_host (a window drawn with ImGui) and d3d12 (a command list run and waited for, a texture cleared).
file(GLOB_RECURSE Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                            "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_library(Utils STATIC ${Sources})

# hasher.cpp uses the xxHash that ships with Zstandard.
target_include_directories(Utils PRIVATE "${APIInterceptor_SOURCE_DIR}/deps/zstd/lib/common")
target_include_directories(Utils PUBLIC  "${DemoVRPlayer_SOURCE_DIR}/src/libs")
target_link_libraries     (Utils PUBLIC  ImGui miniz bcrypt winhttp d3d12 dxgi)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${Sources})
