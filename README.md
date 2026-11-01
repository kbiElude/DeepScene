# Demo VR Player

Plays 3D demos in stereo, live, on a monitor or in a VR headset (OpenXR), without changing the demo: a converter is injected
into the running program, renders two eyes from what the program draws, and passes the images on to a window or a headset.

The program is hooked, studied and replayed by [API Interceptor](deps/APIInterceptor), which this project builds on as a
submodule: what a recipe is, how it is made from a capture of a program, and how the draws of the program are told apart is
documented there. This project is what turns a recipe into stereo, live: the converter, the transport of the images, the
headset, and the player which lists the programs.

* `VRPlayer` - the front end: choose a program and where to see it.
* `runtime\` of a release - what the player starts: `StereoPresenter` (the window or the headset),
  `StereoLauncher` and `StereoConverterDLL` (both for 32 and 64 bits).
* `apps\` - the catalog, the recipes and the programs it lists.

## Build

```
git clone --recurse-submodules <this repository>
build.bat                  :: incremental, x86 + x64, outputs in build_final\
build.bat --from-scratch   :: clean, configure and build
build.bat --debug          :: Debug configuration
```

Visual Studio 2026 (the generator is set in `deps\APIInterceptor\build.bat`), CMake 3.15 or newer. The tests are built
with `-DDEMOVRPLAYER_BUILD_TESTS=ON` (64-bit only); they use the test support of API Interceptor.

See `docs\index.html` for what the parts do and how they fit.
