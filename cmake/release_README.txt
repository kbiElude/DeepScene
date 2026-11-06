DeepScene - what is in this folder
==================================

VRPlayer.exe                 The program to start: it lists the applications, starts one in the headset, and shows its settings.

runtime\                     What VRPlayer starts. Nothing here is started by hand, except to try one application.
    StereoPresenter_x64.exe    Shows the images of the eyes in the headset (or in a window).
    StereoLauncher_x86.exe     Starts a 32-bit application with the converter in it (_x64 for a 64-bit one).
    StereoConverterDLL_x86.dll The converter: loaded into the application, it draws each frame for both eyes (_x64 for a 64-bit one).

apps\                        The catalog (apps.json) and the recipes: what is known of each application. An application is
                             downloaded into this folder the first time it is launched, so the folder must be writable.

tools\api_dumper\            Captures a program: APIDumper_x86.exe (or _x64) runs it with APIDumperDLL in it and writes a
                             .workload file; APIDumpPrinter_x64.exe turns a .workload into readable calls.

tools\analysis\              Studies a capture and makes the recipe of an application.
    APIDumpAnalysisExtractor_x64.exe    .workload -> .passes (what each frame is made of)
    APIDumpAnalysisQuery_x64.exe        asks questions of a .passes file (passes, draws, overlays, depth)
    APIDumpDependencyExtractor_x64.exe  .workload -> .deps (which frames and textures each frame needs): makes seeks fast
    APIDumpDependencyQuery_x64.exe      asks questions of a .deps file
    APIDumpSlicer_x64.exe               cuts a part of a .workload out into a smaller one
    RecipeBake_x64.exe                  writes into a recipe what the analysis knows (and checks a recipe with --check)

tools\stereo_replay\         Plays a capture again, without the application.
    StereoReplayPlayer_x86.exe              the player; use this one for Direct3D 7 and 8 (StereoReplayPlayer_x64.exe is
                                            for the rest)
    StereoReplayConsistencyChecker_x86.exe  replays and compares, takes screenshots, makes the reports of the overlay probe
                                            (also _x64)
    StereoReplayEngineDLL_x86.dll           the engine both of them load (also _x64)

Every program but VRPlayer has the architecture it is built for in its name, _x64 or _x86: a program which comes in two sizes has
both next to each other, and each looks for its partners next to itself. The source of each part is in the same place in the source
tree as it is here (src\vr_player, src\runtime, src\tools\...), and docs\components.html says what each project is for.
