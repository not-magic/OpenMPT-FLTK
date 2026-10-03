/*
 * BuildSettingsExt.h
 * ------------------
 * Purpose: Settings that upstream's BuildSettings.h only provides for the tracker build.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "mpt/base/float.hpp"
#include "mpt/base/namespace.hpp"

#ifndef OPENMPT_BUILD_VARIANT
#define OPENMPT_BUILD_VARIANT "Unknown"
#endif
#ifndef OPENMPT_BUILD_VARIANT_MONIKER
#define OPENMPT_BUILD_VARIANT_MONIKER ""
#endif

namespace mpt
{
inline namespace MPT_INLINE_NS
{
// Types.hpp makes these visible to mpt headers only when there is no OpenMPT namespace
using namespace float_literals;
}  // namespace MPT_INLINE_NS
}  // namespace mpt
