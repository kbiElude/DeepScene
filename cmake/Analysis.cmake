# The analysis of a capture, the recipes, and the programs which make them and ask them. Everything here reads a workload through the loader
# of the interceptor (APIDumpLoader).

file(GLOB APIDumpAnalysisIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/Analysis/APIDumpAnalysis/*.h")
file(GLOB APIDumpAnalysisSources CONFIGURE_DEPENDS  "${DemoVRPlayer_SOURCE_DIR}/src/Analysis/APIDumpAnalysis/*.cpp")

add_library(APIDumpAnalysis STATIC ${APIDumpAnalysisIncludes} ${APIDumpAnalysisSources})

target_link_libraries(APIDumpAnalysis PUBLIC APIDumpLoader)

# hasher.cpp uses the xxHash that ships with Zstandard.
target_include_directories(APIDumpAnalysis PRIVATE "${APIInterceptor_SOURCE_DIR}/deps/zstd/lib/common")

source_group ("APIDump Analysis include files" FILES ${APIDumpAnalysisIncludes})
source_group ("APIDump Analysis source files"  FILES ${APIDumpAnalysisSources})
