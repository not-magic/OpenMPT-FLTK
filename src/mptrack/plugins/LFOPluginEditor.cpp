/*
 * LFOPluginEditor.cpp
 * -------------------
 * Purpose: Editor interface for the LFO plugin.
 * Notes  : (currently none)
 * Authors: Johannes Schultz (OpenMPT Devs)
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "stdafx.h"
#include "../ui/Ui.h"

#include "LFOPluginEditor.h"
#include "../UpdateHints.h"
#include "../../soundlib/Sndfile.h"
#include "../../soundlib/MIDIEvents.h"
#include "../../mptrack/resource.h"

OPENMPT_NAMESPACE_BEGIN

UI_MESSAGE_MAP_BEGIN(LFOPluginEditor, CAbstractVstEditor)
	UI_COMMAND(IDC_BUTTON1, &LFOPluginEditor::OnPluginEditor)
	UI_COMMAND(IDC_CHECK1,	&LFOPluginEditor::OnPolarityChanged)
	UI_COMMAND(IDC_CHECK2,	&LFOPluginEditor::OnTempoSyncChanged)
	UI_COMMAND(IDC_CHECK3,	&LFOPluginEditor::OnBypassChanged)
	UI_COMMAND(IDC_CHECK4,	&LFOPluginEditor::OnLoopModeChanged)
	UI_COMMAND_RANGE(IDC_RADIO1, IDC_RADIO1 + LFOPlugin::kNumWaveforms - 1, &LFOPluginEditor::OnWaveformChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO1, &LFOPluginEditor::OnPlugParameterChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO2, &LFOPluginEditor::OnOutputPlugChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO3, &LFOPluginEditor::OnMidiCCChanged)
	UI_COMMAND(IDC_RADIO7, &LFOPluginEditor::OnParameterChanged)
	UI_COMMAND(IDC_RADIO8, &LFOPluginEditor::OnParameterChanged)
	UI_NOTIFY(ui::EditUpdate, IDC_EDIT1, &LFOPluginEditor::OnParameterChanged)
	UI_MESSAGE(LFOPlugin::WM_PARAM_UDPATE, &LFOPluginEditor::OnUpdateParam)
UI_MESSAGE_MAP_END()


void LFOPluginEditor::DoDataExchange(DataExchange* pDX)
{
	CAbstractVstEditor::DoDataExchange(pDX);
	pDX->BindControl(IDC_COMBO1, m_plugParam);
	pDX->BindControl(IDC_COMBO2, m_outPlug);
	pDX->BindControl(IDC_COMBO3, m_midiCC);
	pDX->BindControl(IDC_SLIDER1, m_amplitudeSlider);
	pDX->BindControl(IDC_SLIDER2, m_offsetSlider);
	pDX->BindControl(IDC_SLIDER3, m_frequencySlider);
	pDX->BindControl(IDC_EDIT1, m_midiChnSpin);
}


LFOPluginEditor::LFOPluginEditor(LFOPlugin &plugin)
	: CAbstractVstEditor(plugin)
	, m_lfoPlugin(plugin)
	, m_locked(true)
{
}


bool LFOPluginEditor::OpenEditor(Wnd *parent)
{
	m_locked = true;
	Create(IDD_LFOPLUGIN, parent);

	m_midiChnSpin.SetRange32(1, 16);
	m_midiCC.SetRedraw(false);
	mpt::ustring s;
	for(unsigned int i = 0; i < 128; i++)
	{
		s = ui::Format(UL_("%3u: "), i);
		s += mpt::ToUnicode(mpt::Charset::UTF8, MIDIEvents::MidiCCNames[i]);
		m_midiCC.AddString(s);
	}
	if(m_lfoPlugin.m_outputToCC && m_lfoPlugin.m_outputParam != LFOPlugin::INVALID_OUTPUT_PARAM)
	{
		m_midiCC.SetCurSel(m_lfoPlugin.m_outputParam & 0x7F);
		SetDlgItemInt(IDC_EDIT1, 1 + ((m_lfoPlugin.m_outputParam & 0xF00) >> 8));
	} else
	{
		SetDlgItemInt(IDC_EDIT1, 1);
	}
	m_midiCC.SetRedraw(true);

	UpdateView(PluginHint().Info().Names());

	for(int32 i = 0; i < LFOPlugin::kLFONumParameters; i++)
	{
		UpdateParam(i);
	}
	UpdateParamDisplays();

	m_locked = false;
	// Avoid weird WM_COMMAND message (following a WM_ACTIVATE message) activating a wrong waveform when closing the plugin editor while the pattern editor is open
	GotoDlgCtrl(GetDlgItem(IDC_RADIO1 + m_lfoPlugin.m_waveForm));
	return CAbstractVstEditor::OpenEditor(parent);
}


void LFOPluginEditor::UpdateParamDisplays()
{
	CAbstractVstEditor::UpdateParamDisplays();
	m_locked = true;
	CheckRadioButton(IDC_RADIO7, IDC_RADIO8, m_lfoPlugin.m_outputToCC ? IDC_RADIO8 : IDC_RADIO7);
	m_plugParam.SetRedraw(false);
	IMixPlugin *outPlug = m_lfoPlugin.GetOutputPlugin();
	if(outPlug != nullptr)
	{
		if(intptr_t(outPlug) != GetWindowLongPtr(m_plugParam, GWLP_USERDATA))
		{
			m_plugParam.ResetContent();
			AddPluginParameternamesToCombobox(m_plugParam, *outPlug);
			if(!m_lfoPlugin.m_outputToCC)
				m_plugParam.SetCurSel(m_lfoPlugin.m_outputParam);
			SetWindowLongPtr(m_plugParam, GWLP_USERDATA, intptr_t(outPlug));
		}
		GetDlgItem(IDC_BUTTON1)->EnableWindow(outPlug ? true :false);
	} else
	{
		m_plugParam.ResetContent();
		SetWindowLongPtr(m_plugParam, GWLP_USERDATA, 0);
	}
	const bool enableWindow = outPlug ? true : false;
	m_midiCC.EnableWindow(enableWindow);
	m_midiChnSpin.EnableWindow(enableWindow);
	GetDlgItem(IDC_RADIO7)->EnableWindow(enableWindow);
	GetDlgItem(IDC_RADIO8)->EnableWindow(enableWindow);

	m_plugParam.SetRedraw(true);
	m_locked = false;
}


void LFOPluginEditor::UpdateParam(int32 p)
{
	LFOPlugin::Parameters param = static_cast<LFOPlugin::Parameters>(p);
	CAbstractVstEditor::UpdateParam(p);
	m_locked = true;
	switch(param)
	{
	case LFOPlugin::kAmplitude:
		InitSlider(m_amplitudeSlider, LFOPlugin::kAmplitude);
		break;
	case LFOPlugin::kOffset:
		InitSlider(m_offsetSlider, LFOPlugin::kOffset);
		break;
	case LFOPlugin::kFrequency:
		InitSlider(m_frequencySlider, LFOPlugin::kFrequency);
		break;
	case LFOPlugin::kTempoSync:
		CheckDlgButton(IDC_CHECK2, m_lfoPlugin.m_tempoSync ? ui::CheckOn : ui::CheckOff);
		InitSlider(m_frequencySlider, LFOPlugin::kFrequency);
		break;
	case LFOPlugin::kWaveform:
		CheckRadioButton(IDC_RADIO1, IDC_RADIO1 + LFOPlugin::kNumWaveforms - 1, IDC_RADIO1 + m_lfoPlugin.m_waveForm);
		break;
	case LFOPlugin::kPolarity:
		CheckDlgButton(IDC_CHECK1, m_lfoPlugin.m_polarity ? ui::CheckOn : ui::CheckOff);
		break;
	case LFOPlugin::kBypassed:
		CheckDlgButton(IDC_CHECK3, m_lfoPlugin.m_bypassed ? ui::CheckOn : ui::CheckOff);
		break;
	case LFOPlugin::kLoopMode:
		CheckDlgButton(IDC_CHECK4, m_lfoPlugin.m_oneshot ? ui::CheckOn : ui::CheckOff);
		break;
	default:
		break;
	}
	m_locked = false;
}


void LFOPluginEditor::UpdateView(UpdateHint hint)
{
	CAbstractVstEditor::UpdateView(hint);
	m_outPlug.Update(PluginComboBox::Config{PluginComboBox::ShowLibraryNames | PluginComboBox::ShowNoPlugin}
		.Hint(hint)
		.CurrentSelection(m_lfoPlugin.m_pMixStruct->GetOutputPlugin())
		.FirstPlugin(m_lfoPlugin.GetSlot() + 1), m_lfoPlugin.GetSoundFile());
}


void LFOPluginEditor::InitSlider(HSlider &slider, LFOPlugin::Parameters param)
{
	slider.SetRange(0, SLIDER_GRANULARITY);
	slider.SetTicFreq(SLIDER_GRANULARITY / 10);
	SetSliderValue(slider, m_lfoPlugin.GetParameter(param));
	SetSliderText(param);
}


void LFOPluginEditor::SetSliderText(LFOPlugin::Parameters param)
{
	mpt::ustring s = m_lfoPlugin.GetParamName(param) + UL_(": ") + m_lfoPlugin.GetFormattedParamValue(param);
	SetDlgItemText(IDC_STATIC1 + param, s);
}


void LFOPluginEditor::SetSliderValue(HSlider &slider, float value)
{
	slider.SetPos(mpt::saturate_round<int>(value * SLIDER_GRANULARITY));
}


float LFOPluginEditor::GetSliderValue(HSlider &slider)
{
	float value = slider.GetPos() / static_cast<float>(SLIDER_GRANULARITY);
	return value;
}


void LFOPluginEditor::OnHScroll(uint32 nCode, uint32 nPos, ScrollBar *pSB)
{
	CAbstractVstEditor::OnHScroll(nCode, nPos, pSB);
	if(!m_locked && nCode != ui::ScrollEndScroll && pSB != nullptr)
	{
		auto slider = reinterpret_cast<HSlider *>(pSB);
		LFOPlugin::Parameters param;
		if(slider == &m_amplitudeSlider)
			param = LFOPlugin::kAmplitude;
		else if(slider == &m_offsetSlider)
			param = LFOPlugin::kOffset;
		else if(slider == &m_frequencySlider)
			param = LFOPlugin::kFrequency;
		else
			return;

		float value = GetSliderValue(*slider);
		m_lfoPlugin.SetParameter(param, value);
		m_lfoPlugin.AutomateParameter(param);
		SetSliderText(param);
	}
}


void LFOPluginEditor::OnPolarityChanged()
{
	if(!m_locked)
	{
		m_lfoPlugin.m_polarity = IsDlgButtonChecked(IDC_CHECK1) != ui::CheckOff;
		m_lfoPlugin.AutomateParameter(LFOPlugin::kPolarity);
	}
}


void LFOPluginEditor::OnTempoSyncChanged()
{
	if(!m_locked)
	{
		m_lfoPlugin.m_tempoSync = IsDlgButtonChecked(IDC_CHECK2) != ui::CheckOff;
		m_lfoPlugin.RecalculateFrequency();
		m_lfoPlugin.AutomateParameter(LFOPlugin::kTempoSync);
		InitSlider(m_frequencySlider, LFOPlugin::kFrequency);
	}
}


void LFOPluginEditor::OnBypassChanged()
{
	if(!m_locked)
	{
		m_lfoPlugin.m_bypassed = IsDlgButtonChecked(IDC_CHECK3) != ui::CheckOff;
		m_lfoPlugin.AutomateParameter(LFOPlugin::kBypassed);
	}
}


void LFOPluginEditor::OnLoopModeChanged()
{
	if(!m_locked)
	{
		m_lfoPlugin.m_oneshot = IsDlgButtonChecked(IDC_CHECK4) != ui::CheckOff;
		m_lfoPlugin.AutomateParameter(LFOPlugin::kLoopMode);
	}
}


void LFOPluginEditor::OnWaveformChanged(uint32 nID)
{
	if(!m_locked)
	{
		m_lfoPlugin.m_waveForm = static_cast<LFOPlugin::LFOWaveform>(nID - IDC_RADIO1);
		m_lfoPlugin.AutomateParameter(LFOPlugin::kWaveform);
	}
}


void LFOPluginEditor::OnPlugParameterChanged()
{
	if(!m_locked)
	{
		CheckRadioButton(IDC_RADIO7, IDC_RADIO8, IDC_RADIO7);
		OnParameterChanged();
	}
}


void LFOPluginEditor::OnMidiCCChanged()
{
	if(!m_locked)
	{
		CheckRadioButton(IDC_RADIO7, IDC_RADIO8, IDC_RADIO8);
		OnParameterChanged();
	}
}


void LFOPluginEditor::OnParameterChanged()
{
	if(!m_locked)
	{
		m_locked = true;
		PlugParamIndex param = 0;
		bool outputToCC = IsDlgButtonChecked(IDC_RADIO8) != ui::CheckOff;
		if(outputToCC)
		{
			param = ((Clamp(GetDlgItemInt(IDC_EDIT1), 1u, 16u) - 1) << 8);
			if(m_lfoPlugin.m_outputToCC || m_midiCC.GetCurSel() >= 0)
				param |= (m_midiCC.GetCurSel() & 0x7F);
		} else
		{
			if(!m_lfoPlugin.m_outputToCC || m_plugParam.GetCurSel() >= 0)
				param = static_cast<PlugParamIndex>(m_plugParam.GetItemData(m_plugParam.GetCurSel()));
		}
		m_lfoPlugin.m_outputToCC = outputToCC;
		m_lfoPlugin.m_outputParam = param;
		m_lfoPlugin.SetModified();
		m_locked = false;
	}
}


void LFOPluginEditor::OnOutputPlugChanged()
{
	if(m_locked)
		return;

	PLUGINDEX plug = m_outPlug.GetSelection().value_or(PLUGINDEX_INVALID);
	m_lfoPlugin.GetSoundFile().m_MixPlugins[m_lfoPlugin.GetSlot()].SetOutputPlugin(plug);
	m_lfoPlugin.SetModified();
	UpdateParamDisplays();
	if(CModDoc *modDoc = m_lfoPlugin.GetSoundFile().GetpModDoc(); modDoc != nullptr)
		modDoc->UpdateAllViews(nullptr, PluginHint(m_lfoPlugin.GetSlot() + 1).Info(), this);
}


void LFOPluginEditor::OnPluginEditor()
{
	std::vector<IMixPlugin *> plug;
	if(m_lfoPlugin.GetOutputPlugList(plug) && plug.front() != nullptr)
	{
		plug.front()->ToggleEditor();
	}
}


LResult LFOPluginEditor::OnUpdateParam(WParam wParam, LParam lParam)
{
	if(wParam == m_lfoPlugin.GetSlot())
	{
		UpdateParam(static_cast<int32>(lParam));
	}
	return 0;
}


OPENMPT_NAMESPACE_END
