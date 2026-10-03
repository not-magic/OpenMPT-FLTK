// Definitions from openmpt/common/BuildSettings.h and openmpt/src/openmpt/base/Types.hpp
// that only reach tracker code in the tracker build.

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
