// FLTK port of openmpt/mptrack/plugins/LFOPluginEditor.h

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

	// LFOPlugin befriends this class; upstream implements these in LFOPlugin for the tracker build only
	static std::pair<PlugParamValue, PlugParamValue> FindParamUIRange(const LFOPlugin &plugin, PlugParamIndex param);
	static mpt::ustring FindParamName(const LFOPlugin &plugin, PlugParamIndex param);
	static mpt::ustring FindParamLabel(const LFOPlugin &plugin, PlugParamIndex param);
	static mpt::ustring FindParamDisplay(LFOPlugin &plugin, PlugParamIndex param);

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

	UI_DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
