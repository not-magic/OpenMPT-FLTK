// FLTK port of openmpt/mptrack/PSRatioCalc.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "CDecimalSupport.h"
#include "DialogBase.h"

OPENMPT_NAMESPACE_BEGIN

class CTrackerSoundFile;

class CPSRatioCalc : public DialogBase
{
public:
	CPSRatioCalc(const CTrackerSoundFile &sndFile, SAMPLEINDEX sample, double ratio, Wnd *parent = nullptr);

	double m_ratio = 100.0;

protected:
	bool OnInitDialog() override;

	void CalcSamples();
	void CalcMs();
	void CalcRows();

	void LockControls() { m_lockCount++; }
	void UnlockControls() { m_lockCount--; }
	bool IsLocked() const { return m_lockCount != 0; }

	void OnChangeSampleLength();
	void OnChangeDuration();
	void OnChangeSpeed();
	void OnChangeRows();
	void OnChangeRatio();

	CNumberEdit m_EditTempo;
	CNumberEdit m_EditRatio;
	CNumberEdit m_EditDurationOrig, m_EditDurationNew;
	CNumberEdit m_EditRowsOrig, m_EditRowsNew;

	const CTrackerSoundFile &m_sndFile;
	SAMPLEINDEX m_sampleIndex = 0;

	double m_durationOrig = 0, m_durationNew = 0;
	double m_rowsOrig = 0, m_rowsNew = 0;
	SmpLength m_sampleLengthNew = 0;
	uint32 m_speed = 0;
	TEMPO m_tempo;
	int m_lockCount = 0;

	UI_DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
