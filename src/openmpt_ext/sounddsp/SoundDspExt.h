/*
 * SoundDspExt.h
 * -------------
 * Purpose: Upstream's DSP effects (surround, mega bass, EQ, AGC, bitcrush), which libopenmpt compiles out.
 * Notes  : The NO_* guards are only lifted around these includes, so that no engine type changes layout.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "openmpt/base/Types.hpp"
#include "../soundlib/Snd_defs.h"

#if !MPT_OS_WINDOWS
OPENMPT_NAMESPACE_BEGIN

// Win32 integer types used in upstream's sounddsp declarations
using DWORD = uint32;
using UINT = unsigned int;

OPENMPT_NAMESPACE_END
#endif

#undef NO_DSP
#undef NO_EQ
#undef NO_AGC
#include "../sounddsp/AGC.h"
#include "../sounddsp/DSP.h"
#include "../sounddsp/EQ.h"
#define NO_DSP
#define NO_EQ
#define NO_AGC

// Snd_defs.h only defines these when the effects are compiled into the engine
#define SNDDSP_AGC 0x40
#define SNDDSP_MEGABASS 0x02
#define SNDDSP_SURROUND 0x08
#define SNDDSP_BITCRUSH 0x01
#define SNDDSP_EQ 0x80
