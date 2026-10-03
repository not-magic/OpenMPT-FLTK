// Builds openmpt/sounddsp/AGC.cpp, which libopenmpt compiles out. See SoundDspExt.h.

#include "stdafx.h"
#include "SoundDspExt.h"

#undef NO_DSP
#undef NO_EQ
#undef NO_AGC
#include "sounddsp/AGC.cpp"
