---
title: Bino
header: Version 2.8
date: July 18, 2026
section: 1
---

# Overview

Bino is a video player with a focus on 3D and Virtual Reality:

- Support for stereoscopic 3D images and videos in various formats

- Support for 180° and 360° surround images and videos, with and without stereoscopic 3D

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

- `--control-file` *file*

  Get control commands from a file.
  See [Scripting and Remote Control](#scripting-and-remote-control).

- `--control-fifo` *name*

  Get control commands from a named pipe (fifo).
  See [Scripting and Remote Control](#scripting-and-remote-control).

- `--control-uds` *name*

  Get control commands from a Unix Domain Socket.
  See [Scripting and Remote Control](#scripting-and-remote-control).

- `--control-tcp` *[ip|name]:port*

  Get control commands by listening on a TCP port.
  See [Scripting and Remote Control](#scripting-and-remote-control).

- `--stereo`

  Enable OpenGL quad-buffered stereo support. This typically only works
  with legacy proprietary GPU drivers, and autodetection breaks with these
  drivers, so this feature has to be enabled manually.

- `--opengles`

  Use OpenGL ES instead of Desktop OpenGL.

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

- `-s`, `--surround` *mode*

  Set surround mode (360, 180, off).

- `--surround-vfov` *degrees*

  Set surround vertical field of view (default 50, range 5-115).

- `-S`, `--swap-eyes`

  Swap left/right eye.

- `-f`, `--fullscreen`

  Start in fullscreen mode.

# Output Modes

## Single-Screen Output

Most output modes should be self explanatory, but there are some exceptions:

- `stereo` requires OpenGL quad-buffered stereo support, typically limited to
  high-end graphics cards. You must use the `--stereo` option on the command
  line to enable this mode.
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

## Multi-Screen Output

Common stereoscopic display setups have one display for the left view and one
display for the right view.

The easiest way to use such setups is to configure your desktop environment to
allow Bino to use two screens in fullscreen mode, and then pick output mode
left/right or top/bottom.

For example, on KDE Plasma, look for "KWin Scripts" in the System Settings,
enable the "Video Wall" script, and configure it so that it applies to
"org.bino3d.bino".

An alternative is to use [Virtual Reality mode](#virtual-reality), which can
support much more complex multi-screen setups without the need for any VR
hardware (you can still use keyboard and mouse).

# File Name Conventions

Bino tries to autodetect the stereoscopic layout and the surround video mode
from image and video metadata, but that data is often incomplete or unknown.
In such cases, Bino has to guess.

To help with this, the hints listed below can be used as the last part of the
file name, just before the file name extension (.ext).

If you use both stereoscopic and surround mode hints, but the surround hint
first, followed by the stereoscopic hint, e.g. `example-180-tb.mp4`.

## Stereoscopic Layout Hints

- `*-tb.ext`, `*-ab.ext`: Input mode `top-bottom`
- `*-tbh.ext`, `*-abq.ext`: Input mode `top-bottom-half`
- `*-bt.ext`, `*-ba.ext`: Input mode `bottom-top`
- `*-bth.ext`, `*-baq.ext`: Input mode `bottom-top-half`
- `*-lr.ext`: Input mode `left-right`
- `*-lrh.ext`, `*-lrq.ext`: Input mode `left-right-half`
- `*-rl.ext`: Input mode `right-left`
- `*-rlh.ext`, `*-rlq.ext`: Input mode `right-left-half`
- `*-2d.ext`: Input mode `mono`

## Surround Mode Hints

- `*-180-*.ext` or `*-180.ext`: Surround mode 180°
- `*-360-*.ext` or `*-360.ext`: Surround mode 360°

# Virtual Reality

## Overview

Bino supports all sorts of Virtual Reality environments via [QVR](https://marlam.de/qvr):

- When QVR is compiled just with Qt6, CAVEs and powerwalls and similar
  multi-display setups are supported, including multi-GPU and multi-host
  rendering.

- When QVR is compiled with [VRPN](https://github.com/vrpn/vrpn),
  all sorts of tracking and interaction hardware for such systems are
  additionally supported.

- When QVR is compiled with [OpenVR](https://github.com/ValveSoftware/openvr),
  SteamVR is additionally supported and automatically detected (e.g. HTC Vive).

VR mode does not require any VR hardware, it can also be used for multi-screen
output in normal desktop environments, with keyboard/mouse interaction.

Use the option `--vr` to start Bino in VR mode.

## Interaction

You can quit VR mode by pressing the Menu button on your controller, or the Q
key on your keyboard.

Activate the on-screen user interface by pressing a trigger on your controller.
When you release the trigger, the selected action will be performed, and the
on-screen user interface will vanish again. Keyboard shortcuts also still work,
in case you have access to your keyboard in VR mode.

Bino uses QVRs default navigation to move around in the virtual world:
* With autodetected controllers such as the HTC Vive controllers, one
  controller is for left/right/forward/backward movement, the other is
  for up/down and for left/right rotations.
* When keyboard/mouse input is available, the WASD keys will move
  forward/left/backward/right, QE will move up/down, and the mouse will
  change the viewing direction.

## Virtual Screen

Bino will display a video screen in the virtual world for conventional video.
If the input is a surround image or video instead (360° or 180°), it will be
displayed all around the viewer.

The default screen is a 16:9 screen in front of the viewer, but you can use the
`--vr-screen` option to define your own screen via its bottom left, bottom
right and top left corners, or to load screen geometry from an OBJ file. The
latter case is useful e.g. if you want Bino's virtual screen to coincide with a
curved physical screen.

The `--vr-screen` option also accepts the special values `united` and
`intersected`. This will unite (or intersect) the 2D geometries of all VR
windows at runtime, which is useful for multi-screen output configurations.
For example, use `--vr-screen=united --qvr-config=two-screen-stereo.qvr` for a
two-screen stereo setup, where the left view goes on the first screen and the
right view goes on the second screen.

# Scripting and Remote Control

Bino can read commands and execute them. This works both in GUI mode and in
Virtual Reality mode.

The source of the commands can be a text file, a named pipe (fifo), a Unix
Domain Socket (UDS), or a TCP port that Bino listens on. See the various
`--control` options.  Examples:
```
### File:
$ bino --control-file script.bino
### FIFO:
$ mkfifo fifo.bino
$ bino --control-fifo fifo.bino &
$ echo "open myvideo.mp4" > fifo.bino
### Unix Domain Socket:
$ bino --control-uds /tmp/socket.bino &
$ echo "open myvideo.mp4" | nc -q0 -U /tmp/socket.bino
### TCP Socket:
$ bino --control-tcp localhost:63000 &
$ echo "open myvideo.mp4" | nc -q0 localhost 63000
```

This allows pre-scripted media presentations as well as the implementation
of custom remote controls for Bino.

The command interface is line-based. Empty lines and comment lines (which begin
with `#`) are ignored. The following commands are supported:

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

You can play slideshows of images (or videos) simply by making a playlist.
In the GUI, use the Playlist menu, and on the command line, just list multiple
files to play.

By default, the next image/video in the playlist will only be displayed after
you press the N key, or choose Playlist/Next from the menu or the on-screen
user interface.

When the *wait* status of the playlist is switched off instead, the next media
will play as soon as the previous is finished.

For a scripted slideshow with predefined presentation times for each image or
video, use the [scripting mode](#scripting-and-remote-control) as in the
following example:
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
