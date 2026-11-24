DeepScene
=========

DeepScene plays old 3D demos in stereo, live, without changing them. Pick a demo, press Launch, and
it is shown with a separate picture for each eye: in a VR headset (OpenXR), or side by side in a
window on your monitor.

The demos of the catalog are 2000-2001 productions for Direct3D 7, Direct3D 8 and OpenGL. DeepScene
puts a converter into the running demo, which draws every frame twice, from two eye positions. The
demo itself is not modified: it is downloaded unchanged from scene.org the first time you launch it.

What you need
-------------

  * Windows 10 (version 1607 or newer) or Windows 11, 64-bit.

  * The Microsoft Visual C++ Redistributable, x64 and x86 (both: some of the programs are 32-bit,
    like the demos). It is free from Microsoft:

      x64:  https://aka.ms/vs/17/release/vc_redist.x64.exe
      x86:  https://aka.ms/vs/17/release/vc_redist.x86.exe

    (the page that lists them all: https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist)

  * A graphics card that supports Direct3D 11. For a headset, Direct3D 12.

  * For a headset: an OpenXR runtime that is set as the active one (SteamVR, the Meta Quest / Oculus
    app, or Windows Mixed Reality), with the headset connected. Without one, the picture is shown in
    a window on the monitor ("2D preview"), which also works for trying it out.

  * An internet connection for the first launch of each demo (4-8 MB each, from scene.org). After
    that, nothing is needed from the network.

  * A folder that can be written to: the demos are downloaded into the "apps" folder next to
    VRPlayer.exe, so do not put the program in "Program Files".


How to run it
-------------

  1. Unpack the zip anywhere that can be written to, and keep its folders together.
  2. Start VRPlayer.exe.
  3. Select a demo in the list. In a headset, put it on first. The settings are on the right:
       Device            the headset, or "2D preview" for a window.
       Depth             how deep the stereo is. Natural suits each scene; Reduced is flatter.
       Resolution per eye  lower is lighter on the graphics card.
  4. Press Launch. The first time, the button says "Download & launch".

A demo is closed with its own controls (Escape in most of them), or by closing the preview window.


If something does not work
--------------------------

  * A virus scanner may remove or block a file: DeepScene works by loading a DLL into the demo, which
    is what some scanners dislike. Allow the files of this folder and the "apps" folder, and launch
    again.

  * A demo that does not start: run VRPlayer.exe with --log. It writes a log of each session to
    %TEMP%\apistereo_<session>.log, which says what happened and why a headset was or was not used.


What is in this folder
----------------------

  VRPlayer.exe     The program to start.
  runtime\         What VRPlayer starts to play a demo. Nothing in it is started by hand.
  apps\            The list of demos (apps.json) and what is known of each one. The demos are
                   downloaded here.
  README.txt       This file.


More
----

The source code, the documentation and the way to add a demo are at
https://github.com/kbiElude/DeepScene . MIT licence.

The demos belong to their authors. The pictures in the list are the screenshots of their pages on
scene.org, credited there to pouet.net.

Questions, bugs and ideas are welcome: https://github.com/kbiElude/DeepScene/issues

This project would not have been possible without Sonnet.
