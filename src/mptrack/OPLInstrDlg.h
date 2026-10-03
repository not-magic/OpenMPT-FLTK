/*
 * OPLInstrDlg.h
 * -------------
 * Purpose: Editor for OPL-based synth instruments
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"
#include "DialogBase.h"
#include "../soundlib/Snd_defs.h"

OPENMPT_NAMESPACE_BEGIN

class CSoundFile;

class OPLInstrDlg : public DialogBase
{
	Button m_additive, m_sustain[2], m_scaleEnv[2], m_vibrato[2], m_tremolo[2];
	HSlider m_feedback, m_attackRate[2], m_decayRate[2], m_sustainLevel[2], m_releaseRate[2], m_volume[2], m_levelScaling[2], m_freqMultiplier[2];
	ComboBox m_waveform[2];
	Size m_windowSize;
	Wnd &m_parent;
	WindowHandle m_lastFocusItem = nullptr;
	const CSoundFile &m_sndFile;
	OPLPatch *m_patch;

public:
	OPLInstrDlg(Wnd &parent, const CSoundFile &sndFile);
	~OPLInstrDlg();
	void SetEnabled(bool enabled);
	void SetPatch(OPLPatch &patch);
	Size GetMinimumSize() const { return m_windowSize; }

protected:
	void ParamsChanged();

	void DoDataExchange(DataExchange *pDX) override;
	bool OnInitDialog() override;
	bool PreTranslateMessage(int event) override;
	mpt::ustring GetToolTipText(uint32 id, WindowHandle hwnd) const override;
	void OnOK() override { }
	void OnCancel() override { }
	void OnHScroll(uint32, uint32, Wnd *) override { ParamsChanged(); }
	LResult OnDragonDropping(WParam wParam, LParam lParam);
	UI_DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
