// FLTK port of openmpt/mptrack/PatternGotoDialog.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"
#include "DialogBase.h"
#include "../soundlib/Snd_defs.h"

OPENMPT_NAMESPACE_BEGIN

class CTrackerSoundFile;

// CPatternGotoDialog dialog

class CPatternGotoDialog : public DialogBase
{
	CTrackerSoundFile &m_SndFile;
	Spinner m_SpinRow, m_SpinChannel, m_SpinPattern, m_SpinOrder;

public:
	ROWINDEX m_nRow;
	CHANNELINDEX m_nChannel;
	PATTERNINDEX m_nPattern;
	ORDERINDEX m_nOrder, m_nActiveOrder;

public:
	CPatternGotoDialog(Wnd *pParent, ROWINDEX row, CHANNELINDEX chan, PATTERNINDEX pat, ORDERINDEX ord, CTrackerSoundFile &sndFile);
	bool OnInitDialog() override;

protected:
	bool m_controlLock = true;

	inline bool ControlsLocked() const { return m_controlLock; }
	inline void LockControls() { m_controlLock = true; }
	inline void UnlockControls() { m_controlLock = false; }

	void UpdateNumRows();
	void UpdateTime();

	void DoDataExchange(DataExchange* pDX) override;
	void OnOK() override;

	void OnPatternChanged();
	void OnOrderChanged();
	void OnRowChanged();
	void OnTimeChanged();

	UI_DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
