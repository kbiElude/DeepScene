# What converts an application to stereo as it runs, and what carries its images to the program which shows them. Each of the programs
# is built for x86 and for x64 (see build.bat), since the launcher has to be of the architecture of the application.

file(GLOB StereoSessionIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/Stereo/StereoSession/*.h")
file(GLOB StereoSessionSources CONFIGURE_DEPENDS  "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/StereoSession/*.cpp")

# How the launcher hands a playback over to the converter in the application: through the environment of the application.
add_library(StereoSession STATIC ${StereoSessionIncludes} ${StereoSessionSources})

source_group ("StereoSession include files" FILES ${StereoSessionIncludes})
source_group ("StereoSession source files"  FILES ${StereoSessionSources})

file(GLOB StereoCoreIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/Stereo/StereoCore/*.h")
file(GLOB StereoCoreSources CONFIGURE_DEPENDS  "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/StereoCore/*.cpp")

# What a converter which plays an application as it runs decides, whatever the API is.
add_library(StereoCore STATIC ${StereoCoreIncludes} ${StereoCoreSources})

target_link_libraries(StereoCore PUBLIC RecipeApplier)

source_group ("Stereo Core include files" FILES ${StereoCoreIncludes})
source_group ("Stereo Core source files"  FILES ${StereoCoreSources})
