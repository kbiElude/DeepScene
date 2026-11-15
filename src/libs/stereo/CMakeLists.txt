# Stereo: what a converter decides, whatever the API is (Core), what is done to the vertices of a draw whose vertices are in pixels, for an eye (also in Core), how the
# launcher hands a playback over to the converter (Session), and the images which go from the application to the program which shows them
# (Transport).
file(GLOB_RECURSE Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                            "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_library(Stereo STATIC ${Sources})

# The pixel pass of DepthMeasurer (DepthHistogramAvx2) is built for AVX2, and nothing else is: the measurer calls it only on a processor that has it.
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/core/depth_histogram_avx2.cpp" PROPERTIES COMPILE_OPTIONS "/arch:AVX2")

target_include_directories(Stereo PUBLIC "${DemoVRPlayer_SOURCE_DIR}/src/libs")
target_link_libraries     (Stereo PUBLIC Analysis Utils)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${Sources})
