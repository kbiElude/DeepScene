# The analysis of a capture and the recipes. Everything here reads a workload through the loader of the interceptor (APIDumpLoader). The
# programs which make the analysis and ask it are in src/tools/Analysis.

file(GLOB APIDumpAnalysisIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/Analysis/APIDumpAnalysis/*.h")
file(GLOB APIDumpAnalysisSources CONFIGURE_DEPENDS  "${DemoVRPlayer_SOURCE_DIR}/src/Analysis/APIDumpAnalysis/*.cpp")

add_library(APIDumpAnalysis STATIC ${APIDumpAnalysisIncludes} ${APIDumpAnalysisSources})

target_link_libraries(APIDumpAnalysis PUBLIC APIDumpLoader)

# hasher.cpp uses the xxHash that ships with Zstandard.
target_include_directories(APIDumpAnalysis PRIVATE "${APIInterceptor_SOURCE_DIR}/deps/zstd/lib/common")

source_group ("APIDump Analysis include files" FILES ${APIDumpAnalysisIncludes})
source_group ("APIDump Analysis source files"  FILES ${APIDumpAnalysisSources})

file(GLOB APIDumpAnalysisFrontendsIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/Analysis/APIDumpAnalysis/frontends/*.h")
file(GLOB APIDumpAnalysisFrontendsSources CONFIGURE_DEPENDS  "${DemoVRPlayer_SOURCE_DIR}/src/Analysis/APIDumpAnalysis/frontends/*.cpp")

# The front ends: for each API, what follows its state and says what its calls mean to the model.
add_library(APIDumpAnalysisFrontends STATIC ${APIDumpAnalysisFrontendsIncludes} ${APIDumpAnalysisFrontendsSources})

target_link_libraries(APIDumpAnalysisFrontends PUBLIC APIDumpAnalysis D3D7Utils D3D8Utils)

source_group ("APIDump Analysis Frontends include files" FILES ${APIDumpAnalysisFrontendsIncludes})
source_group ("APIDump Analysis Frontends source files"  FILES ${APIDumpAnalysisFrontendsSources})

file(GLOB APIDumpDependenciesIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/Analysis/APIDumpDependencies/*.h")
file(GLOB APIDumpDependenciesSources CONFIGURE_DEPENDS  "${DemoVRPlayer_SOURCE_DIR}/src/Analysis/APIDumpDependencies/*.cpp")

# What each frame of a workload does with what the frames before it left in the resources: made of the accesses that the model builder tells.
add_library(APIDumpDependencies STATIC ${APIDumpDependenciesIncludes} ${APIDumpDependenciesSources})

target_link_libraries(APIDumpDependencies PUBLIC APIDumpAnalysis)

source_group ("APIDump Dependencies include files" FILES ${APIDumpDependenciesIncludes})
source_group ("APIDump Dependencies source files"  FILES ${APIDumpDependenciesSources})

file(GLOB RecipeLoaderIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/Analysis/RecipeLoader/*.h")
file(GLOB RecipeLoaderSources CONFIGURE_DEPENDS  "${DemoVRPlayer_SOURCE_DIR}/src/Analysis/RecipeLoader/*.cpp")

add_library(RecipeLoader STATIC ${RecipeLoaderIncludes} ${RecipeLoaderSources})

target_link_libraries(RecipeLoader PUBLIC APIDumpAnalysis)

source_group ("Recipe Loader include files" FILES ${RecipeLoaderIncludes})
source_group ("Recipe Loader source files"  FILES ${RecipeLoaderSources})

file(GLOB RecipeApplierIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/Analysis/RecipeApplier/*.h")
file(GLOB RecipeApplierSources CONFIGURE_DEPENDS  "${DemoVRPlayer_SOURCE_DIR}/src/Analysis/RecipeApplier/*.cpp")

add_library(RecipeApplier STATIC ${RecipeApplierIncludes} ${RecipeApplierSources})

target_link_libraries(RecipeApplier PUBLIC RecipeLoader)

source_group ("Recipe Applier include files" FILES ${RecipeApplierIncludes})
source_group ("Recipe Applier source files"  FILES ${RecipeApplierSources})

# Writes a workload, and cuts the first frames of one out of another. The compressor is the one of the dumper (a source of its DLL, which a program cannot link).
file(GLOB APIDumpSlicingIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/Analysis/APIDumpSlicing/*.h")
file(GLOB APIDumpSlicingSources CONFIGURE_DEPENDS  "${DemoVRPlayer_SOURCE_DIR}/src/Analysis/APIDumpSlicing/*.cpp")

add_library(APIDumpSlicing STATIC ${APIDumpSlicingIncludes} ${APIDumpSlicingSources} "${APIInterceptor_SOURCE_DIR}/src/Capture/APIDumperDLL/zstd_file_writer.cpp")

# zstd_file_writer.cpp includes its header by its name.
target_include_directories(APIDumpSlicing PRIVATE "${APIInterceptor_SOURCE_DIR}/include/Capture/APIDumperDLL")
target_link_libraries(APIDumpSlicing PUBLIC APIDumpLoader)

source_group ("APIDump Slicing include files" FILES ${APIDumpSlicingIncludes})
source_group ("APIDump Slicing source files"  FILES ${APIDumpSlicingSources})
