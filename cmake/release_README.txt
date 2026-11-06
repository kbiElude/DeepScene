DeepScene - what is in this folder
==================================

VRPlayer.exe                 The program to start: it lists the applications, starts one in the headset, and shows its settings.

runtime\                     What VRPlayer starts. Nothing here is started by hand, except to try one application.
    StereoPresenter.exe        Shows the images of the eyes in the headset (or in a window).
    StereoLauncher_x86.exe     Starts a 32-bit application with the converter in it (_x64 for a 64-bit one).
    StereoConverterDLL_x86.dll The converter: loaded into the application, it draws each frame for both eyes.

apps\                        The catalog (apps.json) and the recipes: what is known of each application. An application is
                             downloaded into this folder the first time it is launched, so the folder must be writable.

tools\APIDumper\             Captures a program: APIDumper_x86.exe (or _x64) runs it with APIDumperDLL in it and writes a
                             .workload file; APIDumpPrinter.exe turns a .workload into readable calls.

tools\Analysis\              Studies a capture and makes the recipe of an application.
    APIDumpAnalysisExtractor.exe    .workload -> .passes (what each frame is made of)
    APIDumpAnalysisQuery.exe        asks questions of a .passes file (passes, draws, overlays, depth)
    APIDumpDependencyExtractor.exe  .workload -> .deps (which frames and textures each frame needs): makes seeks fast
    APIDumpDependencyQuery.exe      asks questions of a .deps file
    APIDumpSlicer.exe               cuts a part of a .workload out into a smaller one
    RecipeBake.exe                  writes into a recipe what the analysis knows (and checks a recipe with --check)

tools\Replay\                Plays a capture again, without the application.
    StereoReplayPlayer_x86.exe          the player; use this one for Direct3D 7 and 8 (the 64-bit StereoReplayPlayer.exe is
                                        for the rest)
    StereoReplayConsistencyChecker_x86.exe  replays and compares, takes screenshots, makes the reports of the overlay probe
                                        (also in 64 bits, without the suffix)
    StereoReplayEngineDLL_x86.dll       the engine both of them load (the 64-bit one without the suffix)

A program which comes in two sizes has both next to each other, the 64-bit one with the suffix _x64 and the 32-bit one with
_x86: each looks for its partners next to itself. The source of each part is in the same place in the source tree as it is here
(src\VRPlayer, src\runtime, src\tools\...), and docs\components.html says what each project is for.
