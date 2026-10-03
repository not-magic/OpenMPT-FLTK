// Portable replacement for the tracker-only Util::MultimediaClock in openmpt/misc/mptClock.cpp.

#include "stdafx.h"
#include "mptClockExt.h"

#include <chrono>

OPENMPT_NAMESPACE_BEGIN

namespace Util
{

uint32 MultimediaClock::SetResolution(uint32 resolution_ms)
{
	m_resolution_ms = resolution_ms;
	return m_resolution_ms;
}


uint32 MultimediaClock::GetResolution() const
{
	return m_resolution_ms;
}


uint32 MultimediaClock::Now() const
{
	return static_cast<uint32>(NowNanoseconds() / 1'000'000u);
}


uint64 MultimediaClock::NowNanoseconds() const
{
	const auto since_epoch = std::chrono::steady_clock::now().time_since_epoch();
	return static_cast<uint64>(std::chrono::duration_cast<std::chrono::nanoseconds>(since_epoch).count());
}

}  // namespace Util

OPENMPT_NAMESPACE_END
