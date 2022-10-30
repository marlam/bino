# Bino: a 3D video player

Bino is a video player with an emphasis on 3D video:

- Support for stereoscopic 3D videos in various formats

- Support for 360° videos, with and without stereoscopic 3D

- Support for Virtual Reality environments, including Steam VR,
  CAVEs, and multi-host / multi-GPU render clusters

Bino is based on [Qt](https://www.qt.io/). The optional Virtual Reality
and multi-GPU support is based on [QVR](https://marlam.de/qvr/). No other
libraries are required.

See [bino3d.org](https://bino3d.org/).

Note: this is a complete rewrite of Bino and will eventually be released
as Bino 2.0. It has the following advantages over the 1.x versions:

- No libraries required except for Qt

- GPU accelerated video decoding

- Support for OpenGL ES, e.g. for mobile platforms, Raspberry Pi

- Support for 360° videos, with and without 3D

- When used with QVR: much better support for Virtual Reality hardware,
  including SteamVR support (e.g. HTC Vivde) and

- Drastically simplified source code

- Build system based on CMake instead of autotools

Some features of the 1.x versions have not been reimplemented yet, but may be
added if there is interest: output modes even-odd-rows, even-odd-columns and
checkerboard; support for crop and/or zoom; parallax adjustment; crosstalk
ghostbusting. Some other features will only be implemented if Qt adds support
for them in the Qt Multimedia module: advanced subtitle formats; separate
left/right video tracks.
