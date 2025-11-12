Version 2.6:
- Improved portability to Mac OS
- Improved and fixed OpenGL quad-buffered stereo support
- The GUI now remembers the last 3D output mode and the last directory

Version 2.5:
- Improved meta data detection via ffprobe (if available).

Version 2.4:
- Added support for HDR video input.
- Added support for zooming in surround videos.
- Increased minimum Qt version to 6.6.

Version 2.3:
- Added support for slideshow mode.
- Improved support for image file formats, including MPO.
- Added new capabilities to `--vr-screen` for easy configuration of two-screen
  stereo setups.
- Made `--opengles` the default on ARM platforms.
- Lowered minimum Qt version to 6.4.
- Lowered minimum OpenGL ES requirements.

Version 2.2:
- Added support for capturing windows and screens.
- Added support for HighDPI output.
- Added support for 3D HDMI frame-pack output mode.
- Improved performance in multi-host/multi-GPU Virtual Reality setups.
- Improved compatibility for various OpenGL implementations.

Version 2.1:
- Added support for loading/saving/editing play lists, with loop modes
  off/one/all.
- Added support for 180° videos, with and without 3D.
- Added 3D output modes left-right(-half), right-left(-half),
  top-bottom(-half), bottom-top(-half).
- Added support for rendering controller models in VR mode.
- Improved GUI menu structure.
- Fixed color space conversion for several pixel formats, mostly used by image
  file formats.
- Fixed various scripting mode problems.

Version 2.0:
- This is a complete rewrite of Bino. All code is new and based on Qt6:
  - Builds with CMake instead of autoconf
  - Media input is handled by Qt Multimedia instead of FFmpeg and libass
  - Media output is handled by Qt abstractions instead of OpenAL, GLEW and
    plain OpenGL
  - Virtual Reality support is handled by QVR instead of Equalizer
  - Translations use Qt mechanisms instead of gettext
- New features:
  - Support for 360° videos, with and without 3D
  - Simpler and more powerful VR mode, with autodetection of SteamVR
  - Support for both desktop OpenGL and OpenGL ES
  - GPU accelerated video decoding
- Changed features:
  - Scripting commands have changed, see the manual
  - Multi-screen fullscreen now always requires VR mode, but is simple to
    set up with QVR.
  - LIRC support is not built in anymore but can be achieved by writing
    scripting commands to a named pipe
- Removed features that might be added back if there is enough interest:
  - Cropping and zooming
  - Parallax adjustment
  - Crosstalk ghostbusting
- Removed features that depend on support being added to Qt Multimedia:
  - Advanced subtitle formats
  - Separate left/right video tracks

Version 1.6.8:
- Improved support for using multiple cameras
- New options and script commands to set a vertical pixel shift

Version 1.6.7:
- Fixed a loop mode problem where the beginning of a video was skipped.
- Fixed building with latest FFmpeg
- OpenGL stereo output mode is re-enabled with Qt >= 5.10

Version 1.6.6:
- Fixed the preferences dialog for multi-display fullscreen mode.
- Work with plain libglew again, without libglewmx.

Version 1.6.5:
- Fixed a bug that prevented Bino from working correctly in some languages.

Version 1.6.4:
- Current FFmpeg libraries are supported.
- Build problems were fixed.
- A few bugs were fixed.
- Current Qt versions are supported, but "OpenGL stereo" support is disabled
  with Qt >= 5.7 until a proper fix is found.

Version 1.6.3:
- Build problems were fixed.

Version 1.6.2:
- The preferences dialogues were restructured.
- Qt5 is now the default instead of Qt4.

Version 1.6.0:
- Support for Qt5 (but Qt4 remains the default for now).
- New option to force a source aspect ratio.
- Support for newer versions of FFmpeg/Libav and Equalizer.

Version 1.4.0:
- Support for the new output mode "Left/right view alternating", also known
  as "frame sequential". This is intended for 120Hz active stereo projectors
  and displays when "OpenGL stereo" is not available.
- Better support for audio control. Volume, mute, and delay can be adjusted,
  and the output device can be selected.
- Support for scripting via script files or named pipes.
- Much improved support for older graphics cards.
- Support for video output via SDI on NVIDIA Quadro cards.
- Automatic support for high precision color input and output (30 bits per
  pixel).
- Single-frame stepping via the '.' key.
- An adjustable zoom mode for videos that are wider than the screen.
- Support for opening multiple input devices, and for requesting MJPEG data
  from input devices.
- Support for the MPO, JPS, and PNS file formats for stereoscopic images.
- Support for DLP 3-D Ready Sync.
- Various user interface tweaks, including support for multimedia keyboards
  and a "Recent Files" section in the File menu.

Version 1.2.0:
- Flexible fullscreen mode with support for multiple screens.
- Support for remote controls via LIRC.
- Support for camera devices.
- A loop mode.
- User interface improvements.
