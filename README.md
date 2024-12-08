# Bino: a 3D video player

Bino is a video player with a focus on 3D and Virtual Reality:

- Support for 3D videos in various formats

- Support for 360° and 180° videos, with and without 3D

- Support for 3D displays with various modes

- Support for Virtual Reality environments, including SteamVR, CAVEs,
  powerwalls, and other multi-display / multi-GPU / multi-host systems

Bino is based on [Qt](https://www.qt.io/). The optional Virtual Reality
and multi-GPU support is based on [QVR](https://marlam.de/qvr/). No other
libraries are required.

See [bino3d.org](https://bino3d.org/) and the [manual](https://bino3d.org/bino-manual.html).

## Building from source

To build Bino from source, you need Qt6 dev packages. It will comes with the necessary tools to build Bino (such as MinGW, CMake, etc):

1. Cd into the source directory.
2. `mkdir build`
3. `cd build`
4. `cmake -G "MinGW Makefiles" ..`
5. `cmake --build .`
6. `windeployqt bino.exe`