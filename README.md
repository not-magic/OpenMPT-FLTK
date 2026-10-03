# OpenMPT-FLTK

A port of the OpenMPT Tracker to FLTK, with the intention of having something that works natively on Linux. This is a heavily AI-assisted port with a design chosen to minimize how much code needs to be converted. openmpt and fltk are submodules, with only the GUI code and an 'extension' wrapper around tracker-specific internals has been ported. The actual playback works by linking against libOpenMPT as-is.

Not everything works yet, it's basically proof of concept that the UI can be converted. It can load and play a song, and the custom drawing widgets are converted. Things like exporting does not work yet.

FLTK was chosen because it has that old shool computer vibe and is very lightweight.


## Screenshots

General tab:

![General tab](pictures/general.png)

Pattern editor:

![Pattern editor](pictures/patterns.png)
