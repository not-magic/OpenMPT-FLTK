/*
 * ctrl_gen.h
 * ----------
 * Purpose: General tab, upper panel.
 * Notes  : (currently none)
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"
#include "MPTrackUtil.h"

#include "CDecimalSupport.h"
#include "Globals.h"
#include "openmpt_ext/misc/mptClockExt.h"

OPENMPT_NAMESPACE_BEGIN

namespace Util { class MultimediaClock; }

class CVuMeter final : public Panel
{
protected:
	int m_lastDisplayedLevel = -1, m_lastLevel = 0;
	uint32 m_lastVuUpdateTime;

public:
	CVuMeter() { m_lastVuUpdateTime = Util::GetTickCount64(); }
	void SetVuMeter(int level, bool force = false);

protected:
	void DrawVuMeter(ui::Painter &dc);

protected:
	void OnPaint(ui::Painter &dc) override;
	UI_DECLARE_MESSAGE_MAP()
};


class CCtrlGeneral final : public CModControlDlg
{
public:
	Edit m_EditTitle, m_EditArtist;
	Spinner m_SpinTempo, m_SpinSpeed, m_SpinGlobalVol, m_SpinRestartPos, m_SpinSamplePA, m_SpinVSTiVol;
	Button m_BtnModType;
	ComboBox m_CbnResampling;

	VSlider m_SliderTempo, m_SliderSamplePreAmp, m_SliderGlobalVol, m_SliderVSTiVol;
	CVuMeter m_VuMeterLeft, m_VuMeterRight;
	std::unique_ptr<Util::MultimediaClock> m_tapTimer;
	bool m_editsLocked = false;

	TEMPO m_tempoMin, m_tempoMax;

public:
	CCtrlGeneral(CModControlView &parent, CModDoc &document);

private:
	// Determine how the global volume slider should be scaled to actual global volume.
	// Display range for XM / S3M should be 0...64, for other formats it's 0...256.
	uint32 GetGlobalVolumeFactor() const;

	Setting<int32> &GetSplitPosRef() override;
	bool OnInitDialog() override;
	void DoDataExchange(DataExchange *pDX) override;  // DDX/DDV support
	void RecalcLayout() override;
	void UpdateView(UpdateHint hint, HintObject *pObj = nullptr) override;
	ViewType GetAssociatedViewType() override;
	void OnActivatePage(LParam) override;
	void OnDeactivatePage() override;
	mpt::ustring GetToolTipText(uint32 uId, WindowHandle hwnd) const override;

protected:
	static constexpr int MAX_SLIDER_VSTI_VOL = 255;
	static constexpr int MAX_SLIDER_SAMPLE_VOL = 255;

	// At this point, the tempo slider moves in more coarse steps to provide detailed values in the regions where it matters
	static constexpr auto TEMPO_SPLIT_THRESHOLD = TEMPO(256, 0);
	static constexpr int TEMPO_SPLIT_PRECISION = 3;

	TEMPO TempoSliderRange() const;
	TEMPO SliderToTempo(int value) const;
	int TempoToSlider(TEMPO tempo) const;

	LResult OnUpdatePosition(WParam, LParam);
	void OnVScroll(uint32, uint32, Wnd *) override;
	void OnTempoSpinDelta(NotifyHeader *, LResult *);
	void OnTapTempo();
	void OnTitleChanged();
	void OnArtistChanged();
	void OnTempoChanged();
	void OnSpeedChanged();
	void OnGlobalVolChanged();
	void OnVSTiVolChanged();
	void OnSamplePAChanged();
	void OnRestartPosChanged();
	void OnRestartPosDone();
	void OnSongProperties();
	void OnLoopSongChanged();
	void OnEnSetfocusEditSongtitle();
	void OnResamplingChanged();
	UI_DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
