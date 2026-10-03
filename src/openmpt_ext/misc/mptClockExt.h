// Portable replacement for the tracker-only Util::MultimediaClock in openmpt/misc/mptClock.h.

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
