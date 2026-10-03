// FLTK port of openmpt/mptrack/MIDIMacroDialog.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "MIDIMacroDialog.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "resource.h"
#include "../common/mptStringBuffer.h"
#include "../soundlib/MIDIEvents.h"
#include "../soundlib/Sndfile.h"
#include "../soundlib/plugins/PlugInterface.h"
#include "PluginUi.h"
#include "MIDIMacrosExt.h"


OPENMPT_NAMESPACE_BEGIN


UI_MESSAGE_MAP_BEGIN(CMidiMacroSetup, DialogBase)
	UI_COMMAND(IDC_BUTTON1,			&CMidiMacroSetup::OnSetAsDefault)
	UI_COMMAND(IDC_BUTTON2,			&CMidiMacroSetup::OnResetCfg)
	UI_COMMAND(IDC_BUTTON3,			&CMidiMacroSetup::OnMacroHelp)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO1,	&CMidiMacroSetup::OnSFxChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO2,	&CMidiMacroSetup::OnSFxPresetChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO3,	&CMidiMacroSetup::OnZxxPresetChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO4,	&CMidiMacroSetup::UpdateZxxSelection)
	UI_NOTIFY(ui::ComboSelChange, IDC_MACROPLUG, &CMidiMacroSetup::OnPlugChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_MACROPARAM,&CMidiMacroSetup::OnPlugParamChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_MACROCC,	&CMidiMacroSetup::OnCCChanged)
	UI_NOTIFY(ui::EditChange, IDC_EDIT1,			&CMidiMacroSetup::OnSFxEditChanged)
	UI_NOTIFY(ui::EditChange, IDC_EDIT2,			&CMidiMacroSetup::OnZxxEditChanged)
	UI_COMMAND_RANGE(ID_PLUGSELECT, ID_PLUGSELECT + kSFxMacros - 1, &CMidiMacroSetup::OnViewAllParams)
	UI_COMMAND_RANGE(ID_PLUGSELECT + kSFxMacros, ID_PLUGSELECT + kSFxMacros + kSFxMacros - 1, &CMidiMacroSetup::OnSetSFx)
UI_MESSAGE_MAP_END()


void CMidiMacroSetup::DoDataExchange(DataExchange* pDX)
{
	DialogBase::DoDataExchange(pDX);
	pDX->BindControl(IDC_COMBO1, m_CbnSFx);
	pDX->BindControl(IDC_COMBO2, m_CbnSFxPreset);
	pDX->BindControl(IDC_COMBO3, m_CbnZxxPreset);
	pDX->BindControl(IDC_COMBO4, m_CbnZxx);
	pDX->BindControl(IDC_EDIT1, m_EditSFx);
	pDX->BindControl(IDC_EDIT2, m_EditZxx);
	pDX->BindControl(IDC_MACROPLUG, m_CbnMacroPlug);
	pDX->BindControl(IDC_MACROPARAM, m_CbnMacroParam);
	pDX->BindControl(IDC_MACROCC, m_CbnMacroCC);
}


CMidiMacroSetup::CMidiMacroSetup(CTrackerSoundFile &sndFile, Wnd *parent)
	: DialogBase{IDD_MIDIMACRO, parent}
	, m_SndFile{sndFile}
	, m_vMidiCfg{sndFile.m_MidiCfg}
	, m_MidiCfg{*m_vMidiCfg}
{

}

bool CMidiMacroSetup::OnInitDialog()
{
	mpt::ustring s;
	DialogBase::OnInitDialog();
	m_EditSFx.SetLimitText(kMacroLength - 1);
	m_EditZxx.SetLimitText(kMacroLength - 1);

	// Parametered macro selection
	m_CbnSFx.SetRedraw(false);
	for(int i = 0; i < 16; i++)
	{
		s = ui::Format(UL_("%d (SF%X)"), i, i);
		m_CbnSFx.AddString(s);
	}
	m_CbnSFx.SetRedraw(true);
	m_CbnSFx.SetCurSel(0);

	// Parametered macro presets
	m_CbnSFxPreset.SetRedraw(false);
	for(int i = 0; i < kSFxMax; i++)
	{
		m_CbnSFxPreset.SetItemData(m_CbnSFxPreset.AddString(GetParameteredMacroName(m_MidiCfg, static_cast<ParameteredMacro>(i))), i);
	}
	m_CbnSFxPreset.SetRedraw(true);
	OnSFxChanged();

	// MIDI CC selection box
	m_CbnMacroCC.SetRedraw(false);
	for (int cc = MIDIEvents::MIDICC_start; cc <= MIDIEvents::MIDICC_end; cc++)
	{
		s = ui::Format(UL_("CC %02d "), cc);
		s += mpt::ToUnicode(mpt::Charset::UTF8, MIDIEvents::MidiCCNames[cc]);
		m_CbnMacroCC.SetItemData(m_CbnMacroCC.AddString(s), cc);
	}
	m_CbnMacroCC.SetRedraw(true);

	// Z80...ZFF box
	m_CbnZxx.SetRedraw(false);
	for(int zxx = 0x80; zxx <= 0xFF; zxx++)
	{
		s = ui::Format(UL_("Z%02X"), zxx);
		m_CbnZxx.AddString(s);
	}
	m_CbnZxx.SetRedraw(true);
	m_CbnZxx.SetCurSel(0);

	// Fixed macro presets
	m_CbnZxxPreset.SetRedraw(false);
	for(int i = 0; i < kZxxMax; i++)
	{
		m_CbnZxxPreset.SetItemData(m_CbnZxxPreset.AddString(GetFixedMacroName(m_MidiCfg, static_cast<FixedMacro>(i))), i);
	}
	m_CbnZxxPreset.SetRedraw(true);
	m_CbnZxxPreset.SetCurSel(m_MidiCfg.GetFixedMacroType());

	UpdateDialog();

	auto ScalePixels = [&](auto x) { return ui::ScalePixels(x, this); };
	int offsetx = ScalePixels(19), offsety = ScalePixels(30), separatorx = ScalePixels(4), separatory = ScalePixels(2);
	int height = ScalePixels(18), widthMacro = ScalePixels(30), widthVal = ScalePixels(179), widthType = ScalePixels(135), widthBtn = ScalePixels(70);

	for(uint32 m = 0; m < kSFxMacros; m++)
	{
		const int rowTop = offsety + m * (separatory + height);
		m_EditMacro[m].Button.CreateChild(*this, Rect(offsetx, rowTop, offsetx + widthMacro, rowTop + height), ID_PLUGSELECT + kSFxMacros + m);
		m_EditMacro[m].Button.SetFont(GetFont());

		m_EditMacro[m].Type.CreateChild(*this, Rect(offsetx + separatorx + widthMacro, rowTop, offsetx + widthMacro + widthType, rowTop + height), ID_PLUGSELECT + kSFxMacros + m);
		m_EditMacro[m].Type.SetReadOnly(true);
		m_EditMacro[m].Type.SetFont(GetFont());

		m_EditMacro[m].Value.CreateChild(*this, Rect(offsetx + separatorx + widthType + widthMacro, rowTop, offsetx + widthMacro + widthType + widthVal, rowTop + height), ID_PLUGSELECT + kSFxMacros + m);
		m_EditMacro[m].Value.SetReadOnly(true);
		m_EditMacro[m].Value.SetFont(GetFont());

		m_EditMacro[m].ShowAll.CreateChild(*this, Rect(offsetx + separatorx + widthType + widthMacro + widthVal, rowTop, offsetx + widthMacro + widthType + widthVal + widthBtn, rowTop + height), ID_PLUGSELECT + m);
		m_EditMacro[m].ShowAll.SetWindowText(UL_("Show All..."));
		m_EditMacro[m].ShowAll.SetFont(GetFont());
	}
	UpdateMacroList();
	m_CbnMacroPlug.Update(PluginComboBox::Config{PluginComboBox::Flags::ShowLibraryNames}, m_SndFile);
	m_CbnMacroPlug.SetRawSelection(0);
	OnPlugChanged();
	return false;
}


// macro == -1 for updating all macros at once
void CMidiMacroSetup::UpdateMacroList(int macro)
{
	if(m_EditMacro[0].Button.GetParent() == nullptr)
	{
		// GUI not yet initialized
		return;
	}

	int start, end;

	if(macro >= 0 && macro < kSFxMacros)
	{
		start = end = macro;
	} else
	{
		start = 0;
		end = kSFxMacros - 1;
	}

	mpt::ustring s;
	const int selectedMacro = m_CbnSFx.GetCurSel();

	for(int m = start; m <= end; m++)
	{
		// SFx
		s = ui::Format(UL_("SF%X"), static_cast<unsigned int>(m));
		m_EditMacro[m].Button.SetWindowText(s);

		// Macro value:
		m_EditMacro[m].Value.SetWindowText(mpt::ToUnicode(mpt::Charset::ASCII, static_cast<std::string>(m_MidiCfg.SFx[m])));
		m_EditMacro[m].Value.SetBackColor(m == selectedMacro ? RGB(200, 200, 225) : RGB(245, 245, 245));

		// Macro Type:
		const ParameteredMacro macroType = m_MidiCfg.GetParameteredMacroType(m);
		switch(macroType)
		{
		case kSFxPlugParam:
			s = ui::Format(UL_("Control Plugin Param %u"), static_cast<unsigned int>(MacroToPlugParam(m_MidiCfg, m)));
			break;

		default:
			s = GetParameteredMacroName(m_MidiCfg, m);
			break;
		}
		m_EditMacro[m].Type.SetWindowText(s);
		m_EditMacro[m].Type.SetBackColor(m == selectedMacro ? RGB(200,200,225) : RGB(245,245,245));

		// Param details button:
		m_EditMacro[m].ShowAll.ShowWindow((macroType == kSFxPlugParam) ? true : false);
	}
}


void CMidiMacroSetup::UpdateDialog()
{
	uint32 sfx = m_CbnSFx.GetCurSel();
	uint32 sfx_preset = static_cast<uint32>(m_CbnSFxPreset.GetItemData(m_CbnSFxPreset.GetCurSel()));
	if(sfx < m_MidiCfg.SFx.size())
	{
		ToggleBoxes(sfx_preset, sfx);
		m_EditSFx.SetWindowText(mpt::ToUnicode(mpt::Charset::ASCII, static_cast<std::string>(m_MidiCfg.SFx[sfx])));
	}

	UpdateZxxSelection();
	UpdateMacroList();
}


void CMidiMacroSetup::OnSetAsDefault()
{
	theApp.SetDefaultMidiMacro(m_MidiCfg);
}


void CMidiMacroSetup::OnResetCfg()
{
	theApp.GetDefaultMidiMacro(m_MidiCfg);
	m_CbnZxxPreset.SetCurSel(0);
	OnSFxChanged();
}


void CMidiMacroSetup::OnMacroHelp()
{
	Reporting::Information(UL_("Valid characters in macros:\n\n"
		"0-9, A-F - Raw hex data (4-Bit value)\n"
		"c - MIDI channel (4-Bit value)\n"
		"n - Note value\n\n"
		"v - Note velocity\n"
		"u - Computed note volume (including envelopes)\n\n"
		"x - Note panning\n"
		"y - Computed panning (including envelopes)\n\n"
		"a - High byte of bank select\n"
		"b - Low byte of bank select\n"
		"p - Program select\n\n"
		"h - Pattern channel\n"
		"m - Sample loop direction\n"
		"o - Last sample offset (Oxx / 9xx)\n"
		"s - SysEx checksum (Roland)\n\n"
		"z - Zxx parameter (00-7F)\n\n"
		"Macros can be up to 31 characters long and contain multiple MIDI messages. SysEx messages are automatically terminated if not specified by the user."),
		UL_("OpenMPT MIDI Macro quick reference"));
}


void CMidiMacroSetup::OnSFxChanged()
{
	uint32 sfx = m_CbnSFx.GetCurSel();
	if (sfx < 16)
	{
		int preset = m_MidiCfg.GetParameteredMacroType(sfx);
		m_CbnSFxPreset.SetCurSel(preset);
	}
	UpdateDialog();
}


void CMidiMacroSetup::OnSFxPresetChanged()
{
	uint32 sfx = m_CbnSFx.GetCurSel();
	ParameteredMacro sfx_preset = static_cast<ParameteredMacro>(m_CbnSFxPreset.GetItemData(m_CbnSFxPreset.GetCurSel()));

	if (sfx < kSFxMacros)
	{
		if(sfx_preset != kSFxCustom)
		{
			m_MidiCfg.CreateParameteredMacro(sfx, sfx_preset);
		}
		UpdateDialog();
	}
}


void CMidiMacroSetup::OnZxxPresetChanged()
{
	FixedMacro zxxPreset = static_cast<FixedMacro>(m_CbnZxxPreset.GetItemData(m_CbnZxxPreset.GetCurSel()));

	if (zxxPreset != kZxxCustom)
	{
		m_MidiCfg.CreateFixedMacro(zxxPreset);
		UpdateDialog();
	}
}


void CMidiMacroSetup::UpdateZxxSelection()
{
	uint32 zxx = m_CbnZxx.GetCurSel();
	if(zxx < m_MidiCfg.Zxx.size())
	{
		m_EditZxx.SetWindowText(mpt::ToUnicode(mpt::Charset::ASCII, static_cast<std::string>(m_MidiCfg.Zxx[zxx])));
	}
}


void CMidiMacroSetup::OnSFxEditChanged()
{
	uint32 sfx = m_CbnSFx.GetCurSel();
	if(sfx < m_MidiCfg.SFx.size())
	{
		if(ValidateMacroString(m_EditSFx, m_MidiCfg.SFx[sfx], true, true, false))
		{
			mpt::ustring s;
			m_EditSFx.GetWindowText(s);
			m_MidiCfg.SFx[sfx] = mpt::ToCharset(mpt::Charset::ASCII, s);

			int sfx_preset = m_MidiCfg.GetParameteredMacroType(sfx);
			m_CbnSFxPreset.SetCurSel(sfx_preset);
			ToggleBoxes(sfx_preset, sfx);
			UpdateMacroList(sfx);
		}
	}
}


void CMidiMacroSetup::OnZxxEditChanged()
{
	uint32 zxx = m_CbnZxx.GetCurSel();
	if(zxx < m_MidiCfg.Zxx.size())
	{
		if(ValidateMacroString(m_EditZxx, m_MidiCfg.Zxx[zxx], false, true, false))
		{
			mpt::ustring s;
			m_EditZxx.GetWindowText(s);
			m_MidiCfg.Zxx[zxx] = mpt::ToCharset(mpt::Charset::ASCII, s);
			m_CbnZxxPreset.SetCurSel(m_MidiCfg.GetFixedMacroType());
		}
	}
}

void CMidiMacroSetup::OnSetSFx(uint32 id)
{
	m_CbnSFx.SetCurSel(id - (ID_PLUGSELECT + kSFxMacros));
	OnSFxChanged();
}

void CMidiMacroSetup::OnViewAllParams(uint32 id)
{
	mpt::ustring message, plugName;
	int sfx = id - ID_PLUGSELECT;
	PlugParamIndex param = MacroToPlugParam(m_MidiCfg, sfx);
	message = ui::Format(UL_("These are the parameters that can be controlled by macro SF%X:\n\n"), sfx);

	for(PLUGINDEX plug = 0; plug < MAX_MIXPLUGINS; plug++)
	{
		IMixPlugin *pVstPlugin = m_SndFile.m_MixPlugins[plug].pMixPlugin;
		if(pVstPlugin && param < pVstPlugin->GetNumVisibleParameters())
		{
			plugName = mpt::ToUnicode(m_SndFile.m_MixPlugins[plug].GetName());
			message += ui::Format(UL_("FX%d: "), plug + 1);
			message += plugName + UL_("\t") + PluginUi(*pVstPlugin).GetFormattedParamName(param) + UL_("\n");
		}
	}

	Reporting::Notification(message, UL_("Macro -> Parameters"));
}

void CMidiMacroSetup::OnPlugChanged()
{
	PLUGINDEX plug = m_CbnMacroPlug.GetSelection().value_or(PLUGINDEX_INVALID);

	if(plug >= MAX_MIXPLUGINS)
		return;

	IMixPlugin *pVstPlugin = m_SndFile.m_MixPlugins[plug].pMixPlugin;
	if (pVstPlugin != nullptr)
	{
		m_CbnMacroParam.SetRedraw(false);
		m_CbnMacroParam.ResetContent();
		AddPluginParameternamesToCombobox(m_CbnMacroParam, *pVstPlugin);
		m_CbnMacroParam.SetRedraw(true);

		int param = MacroToPlugParam(m_MidiCfg, m_CbnSFx.GetCurSel());
		m_CbnMacroParam.SetCurSel(param);
	}
}

void CMidiMacroSetup::OnPlugParamChanged()
{
	int param = static_cast<int>(m_CbnMacroParam.GetItemData(m_CbnMacroParam.GetCurSel()));

	if(param < 384)
	{
		const std::string macroText = m_MidiCfg.CreateParameteredMacro(kSFxPlugParam, param);
		m_EditSFx.SetWindowText(mpt::ToUnicode(mpt::Charset::ASCII, macroText));
	} else
	{
		Reporting::Notification("Only parameters 0 to 383 can be controlled using MIDI Macros. Use Parameter Control Events to automate higher parameters.");
	}
}

void CMidiMacroSetup::OnCCChanged()
{
	int cc = static_cast<int>(m_CbnMacroCC.GetItemData(m_CbnMacroCC.GetCurSel()));
	const std::string macroText = m_MidiCfg.CreateParameteredMacro(kSFxCC, cc);
	m_EditSFx.SetWindowText(mpt::ToUnicode(mpt::Charset::ASCII, macroText));
}

void CMidiMacroSetup::ToggleBoxes(uint32 sfxPreset, uint32 sfx)
{

	if (sfxPreset == kSFxPlugParam)
	{
		m_CbnMacroCC.ShowWindow(false);
		m_CbnMacroPlug.ShowWindow(true);
		m_CbnMacroParam.ShowWindow(true);
		m_CbnMacroPlug.EnableWindow(true);
		m_CbnMacroParam.EnableWindow(true);
		SetDlgItemText(IDC_GENMACROLABEL, UL_("Plugin/Param"));
		m_CbnMacroParam.SetCurSel(MacroToPlugParam(m_MidiCfg, sfx));
	} else
	{
		m_CbnMacroPlug.EnableWindow(false);
		m_CbnMacroParam.EnableWindow(false);
	}

	if (sfxPreset == kSFxCC)
	{
		m_CbnMacroCC.EnableWindow(true);
		m_CbnMacroCC.ShowWindow(true);
		m_CbnMacroPlug.ShowWindow(false);
		m_CbnMacroParam.ShowWindow(false);
		SetDlgItemText(IDC_GENMACROLABEL, UL_("MIDI CC"));
		m_CbnMacroCC.SetCurSel(MacroToMidiCC(m_MidiCfg, sfx));
	} else
	{
		m_CbnMacroCC.EnableWindow(false);
	}
}


OPENMPT_NAMESPACE_END
