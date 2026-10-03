/*
 * mptClockExt.h
 * -------------
 * Purpose: Monotonic clock with the interface of upstream's tracker-only MultimediaClock.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

OPENMPT_NAMESPACE_BEGIN

namespace Util
{

class MultimediaClock
{
public:
	MultimediaClock() = default;
	explicit MultimediaClock(uint32 resolution_ms) : m_resolution_ms(resolution_ms) { }

	// A resolution of 0 resets it
	uint32 SetResolution(uint32 resolution_ms);
	uint32 GetResolution() const;
	uint32 Now() const;
	uint64 NowNanoseconds() const;

private:
	uint32 m_resolution_ms = 0;
};

}  // namespace Util

OPENMPT_NAMESPACE_END
