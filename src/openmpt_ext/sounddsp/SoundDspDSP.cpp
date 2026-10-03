/*
 * SoundDspDSP.cpp
 * ---------------
 * Purpose: Builds upstream's sounddsp/DSP.cpp, which libopenmpt compiles out.
 * Notes  : See SoundDspExt.h.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "SoundDspExt.h"

#undef NO_DSP
#undef NO_EQ
#undef NO_AGC
#include "sounddsp/DSP.cpp"
