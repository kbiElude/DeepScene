# What puts an application where it is played: its archive from the network, checked and unpacked, in the directory of the applications.
# miniz unpacks the zip files; only the library of it is built.
add_subdirectory(deps/miniz EXCLUDE_FROM_ALL)

file(GLOB AppInstallerIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/VR/AppInstaller/*.h")
file(GLOB AppInstallerSources CONFIGURE_DEPENDS  "${DemoVRPlayer_SOURCE_DIR}/src/VR/AppInstaller/*.cpp")

# The installing of an application, and what it is made of: the files of the system, the hash, the unpacking and the download.
add_library(AppInstaller STATIC ${AppInstallerIncludes} ${AppInstallerSources})

target_link_libraries(AppInstaller PUBLIC miniz bcrypt winhttp)

source_group ("AppInstaller include files" FILES ${AppInstallerIncludes})
source_group ("AppInstaller source files"  FILES ${AppInstallerSources})
