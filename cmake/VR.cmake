# The VR player, and what it lists and shows: the window of the tools, the devices, and the programs. The headset and the programs which show
# the images are 64-bit (see Headset.cmake).

file(GLOB UIHostIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/VR/UIHost/*.h")
file(GLOB UIHostSources CONFIGURE_DEPENDS  "${DemoVRPlayer_SOURCE_DIR}/src/VR/UIHost/*.cpp")

# The window of a tool which is drawn with ImGui: shared by the control window of the replay player and the VR frontend.
add_library(UIHost STATIC ${UIHostIncludes} ${UIHostSources})

target_link_libraries(UIHost PUBLIC ImGui)

source_group ("UIHost include files" FILES ${UIHostIncludes})
source_group ("UIHost source files"  FILES ${UIHostSources})
