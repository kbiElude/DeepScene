# DeepScene

A set of tools that turns a plain 3D demo into stereo 3D, live, on a monitor or in a VR headset (OpenXR), without changing the demo.

It works in three steps:

1. **Capture** the program: every graphics API call is recorded, with its data, into a `.workload` file. This is done by [API Interceptor](deps/APIInterceptor), which this project has as a submodule.
2. **Process** the capture: the analysis tells what each draw of the program is (the scene, the interface, the post-processing), and a **recipe** says how each draw is to be treated for stereo.
3. **Play** it in 3D: a converter is injected into the running program, renders the two eyes from what the program draws, and passes the images on to a window or a headset. `VRPlayer` is the front end that lists the programs and starts them.

Direct3D 8 is the first API supported, and the demos are what drove the work.

## What is in here

| Part | What it is |
| --- | --- |
| `libs/analysis`, `tools/analysis` | The analysis of a capture (the `.passes` file, queries) and the recipes: reading, baking and applying them. |
| `tools/stereo_replay` (`StereoReplayEngineDLL`, `StereoReplayPlayer`, `StereoReplayConsistencyChecker`) | Play a workload again for both eyes, to try a recipe offline and to check that a frame can be drawn twice. |
| `runtime/converter_dll`, `runtime/launcher` | The converter that goes into the running program, and the launcher that puts it there. Built for 32 and 64 bits, to match the program. |
| `libs/stereo` (Transport), `libs/headset`, `runtime/presenter` | Carry the images of the eyes to the screen: through shared memory, onto the GPU, into a headset or a preview window. |
| `vr_player`, `libs/vr`, `libs/headset` (vr_devices) | The player: the list of programs, the list of devices, and the window. A program which is not on the computer is downloaded the first time it is launched. |
| `libs/utils` | What is not about any of the above and any of it may use: image and JSON files, hashing, downloading and unpacking, an ImGui window, Direct3D 12 plumbing. |
| `apps\` | The catalog and the recipes. The programs the catalog lists are downloaded into it. |

Each part is described in [the documentation](docs/index.html) (open it in a browser). The folders of `src` are the folders of the release (`build_final`): see `docs/components.html`.

## Build

```
git clone --recurse-submodules <this repository>
build.bat                  :: incremental, x86 + x64, outputs in build_final\
build.bat --from-scratch   :: clean, configure and build
build.bat --debug          :: Debug configuration
```

Visual Studio 2026 (the generator is set in `deps\APIInterceptor\build.bat`), CMake 3.15 or newer. The tests are built with `-DDEMOVRPLAYER_BUILD_TESTS=ON` (64-bit only).

`build_final\` has `VRPlayer.exe`, `runtime\` (what it starts), `apps\`, and `tools\`: `api_dumper` (capture), `analysis` and `stereo_replay`.

The first time a program of the catalog is launched, `VRPlayer` downloads it (with its hash checked) into `apps\<id>\` next to itself, so its folder has to be one you can write to. Nothing is written anywhere else.

## Run

Start `VRPlayer.exe` (from `build_final\`) and you are done: it is the front end, and nothing else has to be started by hand.

1. Pick a program from the list. The first time, a program that is not on the computer yet is downloaded first, so you need internet that time.
2. Pick the device: your headset, or just the preview window on the monitor.
3. Set the depth and, for a headset, the image quality.
4. Press **Launch**. The program starts with the converter inside it, and the two eyes go to the headset (or the window). The depth can be changed while it runs.

For a headset, the PC has to see it through an active OpenXR runtime. Without one, the preview window still works.

Only the Oculus Quest 3 has been tested so far. Other headsets that work through OpenXR might be fine, but nobody has tried them yet.

## License

MIT; see [LICENSE.txt](LICENSE.txt).

---

Built by Dominik Witczak in his spare time, with help from the Sonnet AI model. Dominik steers, reviews and makes the calls.
