# Analysis: what a capture is made of (APIDumpAnalysis, with the front ends of each API), what each frame does with what the frames before it left
# (APIDumpDependencies), the cutting of a capture (APIDumpSlicing), and the recipes (RecipeLoader, RecipeApplier). Everything here reads a
# workload through the loader of the interceptor (APIDumpLoader). The programs which make the analysis and ask it are in src/tools/analysis.
file(GLOB_RECURSE Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                            "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

# The compressor of the slicing is the one of the dumper (a source of its DLL, which a program cannot link).
add_library(Analysis STATIC ${Sources}
                            "${APIInterceptor_SOURCE_DIR}/src/Capture/APIDumperDLL/zstd_file_writer.cpp")

# zstd_file_writer.cpp includes its header by its name.
target_include_directories(Analysis PRIVATE "${APIInterceptor_SOURCE_DIR}/include/Capture/APIDumperDLL")
target_include_directories(Analysis PUBLIC  "${DemoVRPlayer_SOURCE_DIR}/src/libs")
target_link_libraries     (Analysis PUBLIC  APIDumpLoader D3D7Utils D3D8Utils Utils)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${Sources})
