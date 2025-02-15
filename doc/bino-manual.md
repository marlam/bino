---
title: Bino
header: Version 2.5
date: Feburary 15, 2025
section: 1
---

# Overview

Bino is a video player with a focus on 3D and Virtual Reality:

- Support for stereoscopic 3D videos in various formats

- Support for 360° and 180° surround videos, with and without stereoscopic 3D

- Support for Virtual Reality environments, including SteamVR,
  CAVEs, powerwalls, and other multi-display / multi-GPU / multi-host systems

# Invocation

`bino` [*options*] *URL*...

- `-h`, `--help`
  
  Displays help on command line options.

- `--help-all`

  Displays help including Qt specific options.

- `-v`, `--version`

  Displays version information.

- `--log-level` *level*

  Set log level (fatal, warning, info, debug, firehose).

- `--log-file` *file*

  Set log file.

- `--read-commands` *script*

  Read commands from a script file. See [Scripting].

- `--opengles`

  Use OpenGL ES instead of Desktop OpenGL.

- `--stereo`

  Use OpenGL quad-buffered stereo in GUI mode.

- `--vr`

  Start in Virtual Reality mode instead of GUI mode. See [Virtual Reality].

- `--vr-screen` *screen*
  
  Set VR screen geometry, either as the special values 'united' or 'intersected', or as a comma-separated list of nine
  values representing three 3D coordinates that define a planar screen (bottom left, bottom right, top left), or as a an
  aspect ratio followed by the name of an OBJ file that contains the screen geometry with texture coordinates (example:
  '16:9,myscreen.obj').

- `--capture`

  Capture audio/video input from microphone and camera/screen/window.

- `--list-audio-outputs`

  List audio outputs.

- `--list-audio-inputs`

  List audio inputs.

- `--list-video-inputs`

  List video inputs.

- `--list-screen-inputs`

  List screen inputs.

- `--list-window-inputs`

  List window inputs.

- `--audio-output` *ao*

  Choose audio output via its index.
  
- `--audio-input` *ai*

  Choose audio input via its index. Can be empty.

- `--video-input` *vi*

  Choose video input via its index.

- `--screen-input` *si*

  Choose screen input via its index.

- `--window-input` *wi*

  Choose window input via its index.

- `--list-tracks`

  List all video, audio and subtitle tracks in the media.

- `--preferred-audio` *lang*

  Set preferred audio track language (en, de, fr, ...).

- `--preferred-subtitle` *lang*

  Set preferred subtitle track language (en, de, fr, ...). Can be empty.

- `--video-track` *track*

  Choose video track via its index.

- `--audio-track` *track*

  Choose audio track via its index.

- `--subtitle-track` *track*

  Choose subtitle track via its index. Can be empty.

- `-p`, `--playlist` *file*

  Load playlist.

- `-l`, `--loop` *mode*

  Set loop mode (off, one, all).

- `-w`, `--wait` *mode*

  Set wait mode (off, on).

- `-i`, `--input` *mode*

  Set input mode (mono, top-bottom, top-bottom-half, bottom-top,
  bottom-top-half, left-right, left-right-half, right-left, right-left-half,
  alternating-left-right, alternating-right-left).

- `-o`, `--output` *mode*

  Set output mode (left, right, stereo, alternating, hdmi-frame-pack,
  left-right, left-right-half, right-left, right-left-half, top-bottom,
  top-bottom-half, bottom-top, bottom-top-half, even-odd-rows,
  even-odd-columns, checkerboard, red-cyan-dubois, red-cyan-full-color,
  red-cyan-half-color, red-cyan-monochrome, green-magenta-dubois,
  green-magenta-full-color, green-magenta-half-color, green-magenta-monochrome,
  amber-blue-dubois, amber-blue-full-color, amber-blue-half-color,
  amber-blue-monochrome, red-green-monochrome, red-blue-monochrome).

- `--surround` *mode*

  Set surround mode (360, 180, off).

- `--surround-vfov` *degrees*

  Set surround vertical field of view (default 50, range 5-115).

- `-S`, `--swap-eyes`

  Swap left/right eye.

- `-f`, `--fullscreen`

  Start in fullscreen mode.

# Output modes

Most output modes should be self explanatory, but there are some exceptions:

- `stereo` requires OpenGL quad-buffered stereo support, typically limited to
  high-end graphics cards.
- `alternating` tries to mimic stereo mode by displaying the left and right
  frames alternating, ideally at display speed. This is unreliable since Bino
  has no way of making sure that its output frames actually correspond to
  display output frames, but it might work, depending on your hardware and
  system setup.
- `hdmi-frame-pack` is a special mode supported by some 3D TVs via HDMI 1.4a,
  where the left view is placed in the top part of a frame and the right view
  in the bottom part, and both parts are separated by a blank area that takes
  1/49 of the vertical space. To use this mode, force your display output
  resolution into either 1280x1470 (720p 3D: 720+30+720=1470; 1470/49=30) or
  1920x2205 (1080p 3D: 1080+45+1080=2205; 2205/49=45).
- `even-odd-rows`, `even-odd-columns` and `checkerboard` are for (older) 3D
  TVs.

# File Name Conventions

Bino currently cannot detect the stereoscopic layout or the surround video mode
from metadata because Qt does not provide that information. It therefore has to
guess.

Bino recognizes the following hints at the last part of the file name, just
before the file name extension (.ext):

- `*-tb.ext`, `*-ab.ext`: Input mode `top-bottom`
- `*-tbh.ext`, `*-abq.ext`: Input mode `top-bottom-half`
- `*-bt.ext`, `*-ba.ext`: Input mode `bottom-top`
- `*-bth.ext`, `*-baq.ext`: Input mode `bottom-top-half`
- `*-lr.ext`: Input mode `left-right`
- `*-lrh.ext`, `*-lrq.ext`: Input mode `left-right-half`
- `*-rl.ext`: Input mode `right-left`
- `*-rlh.ext`, `*-rlq.ext`: Input mode `right-left-half`
- `*-2d.ext`: Input mode `mono`

Additionally, if the number `180` or `360` is part of the file name and separated
by neighboring digits or letters by other characters, then the corresponding surround
mode is assumed.

# Scripting

Bino can read commands from a script file and execute them via the option
`--read-commands` *scriptfile*. This works both in GUI mode and in Virtual
Reality mode.

The script file can also be a named pipe so that you can have arbitrary remote
control interfaces write commands into it as they come in.

Empty lines and comment lines (which begin with `#`) are ignored.
The following commands are supported:

- `open` `[--input` *mode*`]` `[--surround` *mode*`]` `[--video-track` *vt*`]` `[--audio-track` *at*`]` `[--subtitle-track` *st*`]` *URL*
  
  Open the URL and start playing. The options have the same meaning as the corresponding command line options.

- `capture` `[--audio-input` *ai*`]` `[--video-input` *vi*`]` `[--screen-input` *si*`]` `[--window-input` *wi*`]`

  Start capturing camera and microphone. The options have the same meaning as the corresponding command line options.

- `play`

  Start playing.

- `pause`

  Pause.

- `toggle-pause`

  Switch between pause and play.

- `stop`

  Stop playing.

- `playlist-load` *playlist.m3u*

  Load the playlist.

- `playlist-next`

  Switch to next playlist entry.

- `playlist-prev`

  Switch to previous playlist entry.

- `playlist-wait` *mode*

  Set wait mode (off, on).

- `playlist-loop` *mode*

  Set loop mode (off, one, all).

- `quit`

  Quit Bino.

- `set-position` *p*

  Set the video position to *p*, where *p*=0 is the beginning and *p*=1 is the end.

- `seek` *seconds*

  Seek the given amounts of seconds forward or, if the number of seconds is negative, backwards.

- `wait` `stop`|*seconds*

  Wait until the video stops, or wait for the given number of seconds, before executing the next command.

- `set-mute` `on`|`off`
  
  Set the volume mute status.

- `toggle-mute`

  Switch between mute and unmute.

- `set-volume` *vol*

  Set the volume level to *vol* (between 0 and 1).

- `adjust-volume` *offset*

  Adjust the volume by the given amount (the final volume is clamped between 0 and 1).

- `set-output-mode` *mode*

  Set the given output mode. See the command line option `--output` for a list of modes.

- `set-surround-vfov` *degrees*

  Set surround vertical field of view (default 50, range 5-115).

- `set-swap-eyes` `on`|`off`

  Set left/right eye swap.

- `toggle-swap-eyes`

  Toggle left/right eye swap.

- `set-fullscreen` `on`|`off`

  Set fullscreen mode.

- `toggle-fullscreen`

  Toggle fullscreen mode.

# Slideshows

You can play slideshows of images (or videos) simply by making a playlist and
switching on its *wait* status. This is the default whenever one or more of the
files you open are images instead of videos; this works from the command line
as well as from the GUI.

With *wait* enabled, the next media in the playlist will only be displayed after
you press the N key, or choose Playlist/Next from the menu.

For automatic media switching based on predefined presentation times, use the
scripting mode as in the following example:
```
set-fullscreen on
playlist-load my-slideshow.m3u
playlist-loop on
playlist-wait on
playlist-next
wait 4
playlist-next
wait 7
playlist-next
wait 5
quit
```

# Virtual Reality

Bino supports all sorts of Virtual Reality environments via [QVR](https://marlam.de/qvr):

- When QVR is compiled just with Qt6, CAVEs and powerwalls and similar
  multi-display setups are supported, including multi-GPU and multi-host
  rendering.

- When QVR is compiled with [VRPN](https://github.com/vrpn/vrpn),
  all sorts of tracking and interaction hardware for such systems are
  additionally supported.

- When QVR is compiled with [OpenVR](https://github.com/ValveSoftware/openvr),
  SteamVR is additionally supported and automatically detected (e.g. HTC Vive).

To start Bino in VR mode, use the option `--vr`.
Bino will then display a screen in the virtual world, and the video will be
displayed on that screen, unless the input is a surround video (360° or 180°),
which will of course be displayed all around the viewer.

The default is a 16:9 screen in a few meters distance from the viewer, but you
can use the `--vr-screen` option to either define arbitrary planar screens via
their bottom left, bottom right and top left corners, or to load arbitrary
screen geometry from an OBJ file. The latter case is useful e.g. if you want
Bino's virtual screen to coincide with a curved physical screen.

The `--vr-screen` option also accepts the special values `united` and
`intersected`. This will unite (or intersect) the 2D geometries of all VR windows
at runtime. For example, use `--vr-screen=united --qvr-config=two-screen-stereo.qvr`
for a two-screen stereo setup, where the left view goes on the first screen and
the right view goes on the second screen.

Bino uses QVRs default navigation, which may be based on autodetected
controllers such as the HTC Vive controllers, or on tracking and interaction
hardware configured via QVR for your VR system, or on the mouse and WASDQE keys
if nothing else is available.

Additional interaction in VR mode is currently limited to the same keyboard
shortcuts that also work in GUI mode. That means you currently must specify the
video to play on the command line, and have no way to pause, skip or seek with
VR controllers. This will be added in a future version.
