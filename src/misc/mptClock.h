/*
 * mptClock.h
 * ----------
 * Purpose: Various clock utility functions.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"



OPENMPT_NAMESPACE_BEGIN



#ifdef MODPLUG_TRACKER

namespace Util
{

#if MPT_OS_WINDOWS

// RAII wrapper around timeBeginPeriod/timeEndPeriod/timeGetTime (on Windows).
// This clock is monotonic, even across changing its resolution.
// This is needed to synchronize time in Steinberg APIs (ASIO and VST).
class MultimediaClock
{
private:
	uint32 m_CurrentPeriod;
private:
	void Init();
	void SetPeriod(uint32 ms);
	void Cleanup();
public:
	MultimediaClock();
	MultimediaClock(uint32 ms);
	~MultimediaClock();
public:
	// Sets the desired resolution in milliseconds, returns the obtained resolution in milliseconds.
	// A parameter of 0 causes the resolution to be reset to system defaults.
	// A return value of 0 means the resolution is unknown, but timestamps will still be valid.
	uint32 SetResolution(uint32 ms);
	// Returns obtained resolution in milliseconds.
	// A return value of 0 means the resolution is unknown, but timestamps will still be valid.
	uint32 GetResolution() const; 
	// Returns current instantaneous timestamp in milliseconds.
	// The epoch (offset) of the timestamps is undefined but constant until the next system reboot.
	// The resolution is the value returned from GetResolution().
	uint32 Now() const;
	// Returns current instantaneous timestamp in nanoseconds.
	// The epoch (offset) of the timestamps is undefined but constant until the next system reboot.
	// The resolution is the value returned from GetResolution() in milliseconds.
	uint64 NowNanoseconds() const;
};

#else // !MPT_OS_WINDOWS

// Monotonic clock with the interface of the multimedia timer wrapper of Windows.
// The resolution of the system clock is always better than a millisecond.
class MultimediaClock
{
private:
	uint32 m_CurrentPeriod = 0;
public:
	MultimediaClock() = default;
	explicit MultimediaClock(uint32 ms) : m_CurrentPeriod(ms) { }
public:
	// Remembers the desired resolution in milliseconds and returns it. A parameter of 0 resets it.
	uint32 SetResolution(uint32 ms);
	uint32 GetResolution() const;
	// Returns current instantaneous timestamp in milliseconds.
	uint32 Now() const;
	// Returns current instantaneous timestamp in nanoseconds.
	uint64 NowNanoseconds() const;
};

#endif // MPT_OS_WINDOWS

} // namespace Util

#endif // MODPLUG_TRACKER



OPENMPT_NAMESPACE_END
