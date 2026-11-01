# What converts an application to stereo as it runs, and what carries its images to the program which shows them. Each of the programs
# is built for x86 and for x64 (see build.bat), since the launcher has to be of the architecture of the application.

file(GLOB StereoSessionIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/Stereo/StereoSession/*.h")
file(GLOB StereoSessionSources CONFIGURE_DEPENDS  "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/StereoSession/*.cpp")

# How the launcher hands a playback over to the converter in the application: through the environment of the application.
add_library(StereoSession STATIC ${StereoSessionIncludes} ${StereoSessionSources})

source_group ("StereoSession include files" FILES ${StereoSessionIncludes})
source_group ("StereoSession source files"  FILES ${StereoSessionSources})
