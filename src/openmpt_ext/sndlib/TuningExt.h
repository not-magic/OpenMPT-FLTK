/*
 * TuningExt.h
 * -----------
 * Purpose: Tuning export functions that upstream only compiles into the tracker build.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../common/mptPathString.h"

#include <iosfwd>

OPENMPT_NAMESPACE_BEGIN

namespace Tuning
{

class CTuning;
class CTuningCollection;

// Writes the tuning as a Scala .scl file
bool WriteSCL(const CTuning &tuning, std::ostream &f, const mpt::PathString &filename);
// Writes every tuning of the collection to its own file starting with prefix
bool UnpackTuningCollection(const CTuningCollection &tc, const mpt::PathString &prefix);

}  // namespace Tuning

OPENMPT_NAMESPACE_END
