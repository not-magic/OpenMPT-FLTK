// Port of openmpt/soundlib/AudioCriticalSection.h, as libopenmpt's CriticalSection is a no-op.
// Engine functions that lock internally in the tracker build must be called with this lock held.

#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include <utility>

OPENMPT_NAMESPACE_BEGIN

namespace mpt
{
class recursive_mutex_with_lock_count;
}  // namespace mpt

namespace Tracker
{
// Implemented in mptrack/Mptrack.cpp
mpt::recursive_mutex_with_lock_count &GetGlobalMutexRef();
}  // namespace Tracker

class TrackerCriticalSection
{
public:
	enum class InitialState
	{
		Locked = 0,
		Unlocked = 1,
	};

	TrackerCriticalSection();
	TrackerCriticalSection(TrackerCriticalSection &&other) noexcept;
	explicit TrackerCriticalSection(InitialState state);
	~TrackerCriticalSection();

	TrackerCriticalSection(const TrackerCriticalSection &) = delete;
	TrackerCriticalSection &operator=(const TrackerCriticalSection &) = delete;

	void Enter();
	void Leave();

private:
	mpt::recursive_mutex_with_lock_count &m_globalMutex;
	bool m_isInSection = false;
};

// For engine functions that take the lock themselves in the tracker build, but not in libopenmpt
template <typename Tfunc>
decltype(auto) CallLocked(Tfunc &&func)
{
	TrackerCriticalSection cs;
	return std::forward<Tfunc>(func)();
}

OPENMPT_NAMESPACE_END
