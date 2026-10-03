/*
 * TrackerCriticalSection.cpp
 * --------------------------
 * Purpose: Lock that serializes CSoundFile access between the GUI and the audio thread.
 * Notes  : Ported from upstream's AudioCriticalSection.cpp.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "TrackerCriticalSection.h"

#include "../misc/mptMutex.h"

OPENMPT_NAMESPACE_BEGIN

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
		m_globalMutex.lock();
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
