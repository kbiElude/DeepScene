# Demo VR Player

A set of tools that turns a plain 3D demo into stereo 3D, live, on a monitor or in a VR headset (OpenXR), without changing the demo.

It works in three steps:

1. **Capture** the program: every graphics API call is recorded, with its data, into a `.workload` file. This is done by [API Interceptor](deps/APIInterceptor), which this project has as a submodule.
2. **Process** the capture: the analysis tells what each draw of the program is (the scene, the interface, the post-processing), and a **recipe** says how each draw is to be treated for stereo.
3. **Play** it in 3D: a converter is injected into the running program, renders the two eyes from what the program draws, and passes the images on to a window or a headset. `VRPlayer` is the front end that lists the programs and starts them.

Direct3D 8 is the first API supported, and the demos are what drove the work.

## What is in here

| Part | What it is |
| --- | --- |
| `APIDumpAnalysis*`, `Recipe*` | The analysis of a capture (the `.passes` file, queries) and the recipes: reading, baking and applying them. |
| `StereoReplayEngineDLL`, `StereoReplayPlayer`, `StereoReplayConsistencyChecker` | Play a workload again for both eyes, to try a recipe offline and to check that a frame can be drawn twice. |
| `StereoConverterDLL`, `StereoLauncher` | The converter that goes into the running program, and the launcher that puts it there. Built for 32 and 64 bits, to match the program. |
| `StereoTransport`, `StereoD3D12`, `StereoXR`, `StereoPresenter` | Carry the images of the eyes to the screen: through shared memory, onto the GPU, into a headset or a preview window. |
| `VRPlayer`, `AppCatalog`, `AppInstaller`, `VRDevices`, `UIHost` | The player: the list of programs, the list of devices, and the window. A program which is not on the computer is downloaded the first time it is launched. |
| `apps\` | The catalog and the recipes. The programs the catalog lists are downloaded into it. |

Each part is described in [the documentation](docs/index.html) (open it in a browser).

## Build

```
git clone --recurse-submodules <this repository>
build.bat                  :: incremental, x86 + x64, outputs in build_final\
build.bat --from-scratch   :: clean, configure and build
build.bat --debug          :: Debug configuration
```

Visual Studio 2026 (the generator is set in `deps\APIInterceptor\build.bat`), CMake 3.15 or newer. The tests are built with `-DDEMOVRPLAYER_BUILD_TESTS=ON` (64-bit only).

`build_final\` has `VRPlayer.exe`, `runtime\` (what it starts), `apps\`, and `tools\`: `APIDumper` (capture), `Analysis` and `Replay`.

The first time a program of the catalog is launched, `VRPlayer` downloads it (with its hash checked) into `apps\<id>\` next to itself, so its folder has to be one you can write to. Nothing is written anywhere else.

## License

MIT; see [LICENSE.txt](LICENSE.txt).

---

Built by Dominik Witczak in his spare time, with help from the Sonnet AI model. Dominik steers, reviews and makes the calls.
