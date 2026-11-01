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

file(GLOB APIDumpAnalysisFrontendsIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/Analysis/APIDumpAnalysis/frontends/*.h")
file(GLOB APIDumpAnalysisFrontendsSources CONFIGURE_DEPENDS  "${DemoVRPlayer_SOURCE_DIR}/src/Analysis/APIDumpAnalysis/frontends/*.cpp")

# The front ends: for each API, what follows its state and says what its calls mean to the model.
add_library(APIDumpAnalysisFrontends STATIC ${APIDumpAnalysisFrontendsIncludes} ${APIDumpAnalysisFrontendsSources})

target_link_libraries(APIDumpAnalysisFrontends PUBLIC APIDumpAnalysis D3D8Utils)

source_group ("APIDump Analysis Frontends include files" FILES ${APIDumpAnalysisFrontendsIncludes})
source_group ("APIDump Analysis Frontends source files"  FILES ${APIDumpAnalysisFrontendsSources})

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

file(GLOB APIDumpAnalysisExtractorSources CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/src/Analysis/APIDumpAnalysisExtractor/*.cpp"
                                                            "${DemoVRPlayer_SOURCE_DIR}/src/Analysis/APIDumpAnalysisExtractor/*.h")

add_executable(APIDumpAnalysisExtractor ${APIDumpAnalysisExtractorSources})

target_link_libraries(APIDumpAnalysisExtractor APIDumpAnalysis APIDumpAnalysisFrontends)

source_group ("APIDumpAnalysisExtractor source files" FILES ${APIDumpAnalysisExtractorSources})

file(GLOB APIDumpAnalysisQuerySources CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/src/Analysis/APIDumpAnalysisQuery/*.cpp"
                                                        "${DemoVRPlayer_SOURCE_DIR}/src/Analysis/APIDumpAnalysisQuery/*.h")

add_executable(APIDumpAnalysisQuery ${APIDumpAnalysisQuerySources})

target_link_libraries(APIDumpAnalysisQuery APIDumpAnalysis)

source_group ("APIDumpAnalysisQuery source files" FILES ${APIDumpAnalysisQuerySources})
