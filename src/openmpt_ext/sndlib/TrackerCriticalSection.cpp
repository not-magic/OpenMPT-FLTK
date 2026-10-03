// Port of openmpt/soundlib/AudioCriticalSection.cpp, as libopenmpt's CriticalSection is a no-op.

#include "stdafx.h"
#include "TrackerCriticalSection.h"

#include "../misc/mptMutex.h"

#include <atomic>
#include <thread>

OPENMPT_NAMESPACE_BEGIN

namespace
{

std::atomic<int> g_lockWaiterCount{0};

}  // namespace


void Tracker::YieldToLockWaiters()
{
	while(g_lockWaiterCount.load(std::memory_order_acquire) > 0)
		std::this_thread::yield();
}


TrackerCriticalSection::TrackerCriticalSection()
	: m_globalMutex(Tracker::GetGlobalMutexRef())
{
	Enter();
}


TrackerCriticalSection::TrackerCriticalSection(TrackerCriticalSection &&other) noexcept
	: m_globalMutex(other.m_globalMutex)
	, m_isInSection(other.m_isInSection)
{
	other.m_isInSection = false;
}


TrackerCriticalSection::TrackerCriticalSection(InitialState state)
	: m_globalMutex(Tracker::GetGlobalMutexRef())
{
	if(state == InitialState::Locked)
		Enter();
}


TrackerCriticalSection::~TrackerCriticalSection()
{
	Leave();
}


void TrackerCriticalSection::Enter()
{
	if(!m_isInSection)
	{
		m_isInSection = true;
		g_lockWaiterCount.fetch_add(1, std::memory_order_acq_rel);
		m_globalMutex.lock();
		g_lockWaiterCount.fetch_sub(1, std::memory_order_acq_rel);
	}
}


void TrackerCriticalSection::Leave()
{
	if(m_isInSection)
	{
		m_isInSection = false;
		m_globalMutex.unlock();
	}
}

OPENMPT_NAMESPACE_END
