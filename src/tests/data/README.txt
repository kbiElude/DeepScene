fr08_slice.workload

Frames 0, 1, 2, 3, 100, 101, 14364, 14365, 219679 and 219680 of a capture of fr08v101.exe (farbrausch, fr-08 .the
.product), 5 minutes of its animation: 250771 frames, 1.77 GB. Frames 0-3 are the setup (the loader runs between the
presents of frames 1 and 2). Frame 100 has one pass, 14364 has three passes into the back buffer, 219679 renders the
scene to a texture and post-processes it in six passes before the final composite.

Made with a throwaway tool which keeps the calls of the chosen frames and empties every data chunk bigger than 1 KB
(an empty chunk is valid in a workload file), so the file is small. Timestamps are those of the capture.
