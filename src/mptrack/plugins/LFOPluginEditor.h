/*
 * LFOPluginEditor.h
 * -----------------
 * Purpose: Editor interface for the LFO plugin.
 * Notes  : (currently none)
 * Authors: Johannes Schultz (OpenMPT Devs)
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "../ui/Ui.h"

#include "../AbstractVstEditor.h"
#include "../PluginComboBox.h"
#include "../../soundlib/plugins/LFOPlugin.h"

OPENMPT_NAMESPACE_BEGIN

struct UpdateHint;

class LFOPluginEditor : public CAbstractVstEditor
{
protected:
	ComboBox m_plugParam, m_midiCC;
	PluginComboBox m_outPlug;
	HSlider m_amplitudeSlider, m_offsetSlider, m_frequencySlider;
	Spinner m_midiChnSpin;
	LFOPlugin &m_lfoPlugin;
	bool m_locked : 1;
	static constexpr int SLIDER_GRANULARITY = 1000;

public:

	LFOPluginEditor(LFOPlugin &plugin);

	bool OpenEditor(Wnd *parent) override;
	bool IsResizable() const override { return false; }
	bool SetSize(int, int) override { return false; }

	void UpdateParamDisplays() override;
	void UpdateParam(int32 param) override;
	void UpdateView(UpdateHint hint) override;

protected:
	void DoDataExchange(DataExchange* pDX) override;
	void OnHScroll(uint32 nCode, uint32 nPos, ScrollBar *pSB);

	void InitSlider(HSlider &slider, LFOPlugin::Parameters param);
	void SetSliderText(LFOPlugin::Parameters param);
	void SetSliderValue(HSlider &slider, float value);
	float GetSliderValue(HSlider &slider);

	void OnPolarityChanged();
	void OnTempoSyncChanged();
	void OnBypassChanged();
	void OnLoopModeChanged();
	void OnWaveformChanged(uint32 nID);
	void OnPlugParameterChanged();
	void OnMidiCCChanged();
	void OnParameterChanged();
	void OnOutputPlugChanged();
	void OnPluginEditor();
	LResult OnUpdateParam(WParam wParam, LParam lParam);

	UI_DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
