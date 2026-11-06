# The file of a program or a DLL of this project has the architecture it is built for in its name (StereoLauncher_x64.exe, StereoLauncher_x86.exe), so
# that the 32-bit and the 64-bit one are side by side where one looks for the other (the launcher for the converter, the 64-bit player for the 32-bit
# one), and are named the same in a build folder and in the release. VRPlayer, which is what a person starts, has no suffix.
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCHITECTURE_SUFFIX "_x64")
else()
    set(ARCHITECTURE_SUFFIX "_x86")
endif()

function(set_architecture_name in_target)
    set_target_properties(${in_target} PROPERTIES OUTPUT_NAME "${in_target}${ARCHITECTURE_SUFFIX}")
endfunction()
