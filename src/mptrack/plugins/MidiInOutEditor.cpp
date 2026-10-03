/*
 * MidiInOutEditor.cpp
 * -------------------
 * Purpose: Editor interface for the MidiInOut plugin.
 * Notes  : (currently none)
 * Authors: Johannes Schultz (OpenMPT Devs)
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "stdafx.h"
#include "../ui/Ui.h"

#include "MidiInOutEditor.h"
#include "MidiInOut.h"
#include "../FileDialog.h"
#include "../Mptrack.h"
#include "../Reporting.h"
#include "../resource.h"
#include "../UpdateHints.h"
#include "../../soundlib/MIDIEvents.h"
#include "../../soundlib/MIDIMacroParser.h"
#include "PluginUi.h"
#include <rtmidi/RtMidi.h>


OPENMPT_NAMESPACE_BEGIN


UI_MESSAGE_MAP_BEGIN(MidiInOutEditor, CAbstractVstEditor)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO1, &MidiInOutEditor::OnInputChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO2, &MidiInOutEditor::OnOutputChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO3, &MidiInOutEditor::OnParamChanged)
	UI_NOTIFY(ui::EditChange, IDC_EDIT1,      &MidiInOutEditor::OnLatencyChanged)
	UI_NOTIFY(ui::EditChange, IDC_EDIT2,      &MidiInOutEditor::OnMidiDumpChanged)
	UI_NOTIFY(ui::EditChange, IDC_EDIT3,      &MidiInOutEditor::OnMacroChanged)
	UI_COMMAND(IDC_CHECK1,       &MidiInOutEditor::OnTimingMessagesChanged)
	UI_COMMAND(IDC_CHECK2,       &MidiInOutEditor::OnAlwaysSendDumpChanged)
	UI_COMMAND(IDC_BUTTON1,      &MidiInOutEditor::OnLoadMidiDump)
	UI_COMMAND(IDC_BUTTON2,      &MidiInOutEditor::OnSendDumpNow)
UI_MESSAGE_MAP_END()


void MidiInOutEditor::DoDataExchange(DataExchange* pDX)
{
	CAbstractVstEditor::DoDataExchange(pDX);
	pDX->BindControl(IDC_COMBO1, m_inputCombo);
	pDX->BindControl(IDC_COMBO2, m_outputCombo);
	pDX->BindControl(IDC_COMBO3, m_paramCombo);
	pDX->BindControl(IDC_EDIT1, m_latencySpin);
	pDX->BindControl(IDC_EDIT2, m_dumpEdit);
	pDX->BindControl(IDC_EDIT3, m_paramEdit);
}


MidiInOutEditor::MidiInOutEditor(MidiInOut &plugin)
	: CAbstractVstEditor{plugin}
{
	m_latencySpin.SetAccessibleSuffix(UL_("milliseconds"));
}


bool MidiInOutEditor::OpenEditor(Wnd *parent)
{
	Create(IDD_MIDI_IO_PLUGIN, parent);
	MidiInOut &plugin = static_cast<MidiInOut &>(m_VstPlugin);
	m_latencySpin.SetAllowFractions(true);
	m_latencySpin.SetRange32(mpt::saturate_round<int>(plugin.GetOutputLatency() * -1000.0), int32_max);
	m_latencySpin.SetValue(plugin.m_latency * 1000.0);
	PopulateList(m_inputCombo, plugin.m_midiIn,  plugin.m_inputDevice, true);
	PopulateList(m_outputCombo, plugin.m_midiOut, plugin.m_outputDevice, false);
	UpdateOutputPlugin();
	CheckDlgButton(IDC_CHECK1, plugin.m_sendTimingInfo ? ui::CheckOn : ui::CheckOff);
	CheckDlgButton(IDC_CHECK2, plugin.m_alwaysSendInitialDump ? ui::CheckOff : ui::CheckOn);
	UpdateMidiDump();
	m_paramCombo.SetRedraw(false);
	int numItems = MidiInOut::kMacroParamMax - MidiInOut::kMacroParamMin + 1;
	m_paramCombo.InitStorage(numItems, static_cast<uint32>(numItems * 14 * sizeof(mpt::uchar)));
	for(unsigned int i = MidiInOut::kMacroParamMin; i <= MidiInOut::kMacroParamMax; i++)
	{
		m_paramCombo.AddString(MPT_UFORMAT("Parameter {}")(i));
	}
	m_paramCombo.SetRedraw(true);
	m_paramCombo.SetCurSel(0);
	if(!plugin.m_parameterMacros.empty())
		m_paramEdit.SetWindowText(mpt::ToUnicode(mpt::Charset::ASCII, plugin.m_parameterMacros[0].first));
	m_locked = false;
	return CAbstractVstEditor::OpenEditor(parent);
}


void MidiInOutEditor::UpdateView(UpdateHint hint)
{
	CAbstractVstEditor::UpdateView(hint);
	PluginHint pluginHint = hint.ToType<PluginHint>();
	if(pluginHint.GetType()[HINT_MODTYPE | HINT_PLUGINNAMES] || pluginHint.GetPlugin() == (PluginUi(m_VstPlugin).GetSlot() + 1))
		UpdateOutputPlugin();
}


void MidiInOutEditor::UpdateMidiDump()
{
	MidiInOut &plugin = static_cast<MidiInOut &>(m_VstPlugin);
	m_formattedDump.clear();
	m_formattedDump.reserve(plugin.m_initialMidiDump.size() * 3);
	MIDIMacroParser parser{mpt::as_span(plugin.m_initialMidiDump)};
	mpt::span<uint8> midiMsg;
	bool firstLine = true;
	while(parser.NextMessage(midiMsg, false))
	{
		bool firstChar = true;
		for(uint8 c : midiMsg)
		{
			if(firstChar && !firstLine)
				m_formattedDump += "\r\n";
			else if(!firstChar)
				m_formattedDump += ' ';
			firstChar = firstLine = false;
			m_formattedDump += mpt::afmt::HEX0<2>(c);
		}
	}
	m_dumpEdit.SetWindowText(mpt::ToUnicode(mpt::Charset::ASCII, m_formattedDump));
}


// Update lists of available input / output devices
void MidiInOutEditor::PopulateList(ComboBox &combo, RtMidi &rtDevice, MidiDevice &midiDevice, bool isInput)
{
	combo.SetRedraw(false);
	combo.ResetContent();

	// Add dummy device
	combo.SetItemData(combo.AddString(UL_("<none>")), static_cast<uintptr_t>(MidiInOut::kNoDevice));
	if(!isInput)
	{
		combo.SetItemData(combo.AddString(UL_("Internal OpenMPT Output")), static_cast<uintptr_t>(MidiInOut::kInternalDevice));
	}

	// Go through all RtMidi devices
	auto ports = rtDevice.getPortCount();
	int selectedItem = 0;
	mpt::ustring portName;
	for(unsigned int i = 0; i < ports; i++)
	{
		try
		{
			portName = theApp.GetFriendlyMIDIPortName(mpt::ToUnicode(mpt::Charset::UTF8, midiDevice.GetPortName(i)), isInput);
			int result = combo.AddString(portName);
			combo.SetItemData(result, i);

			if(result != CB_ERR && i == midiDevice.index)
				selectedItem = result;
		} catch(RtMidiError &)
		{
		}
	}

	combo.SetCurSel(selectedItem);
	combo.SetRedraw(true);
}


void MidiInOutEditor::UpdateOutputPlugin()
{
	MPT_ASSERT(m_outputCombo.GetItemData(1) == MidiInOut::kInternalDevice);
	const int sel = m_outputCombo.GetCurSel();
	mpt::ustring outputPlugin;
	if(std::vector<IMixPlugin *> plug; m_VstPlugin.GetOutputPlugList(plug) && plug.front() != nullptr)
		outputPlugin = MPT_UFORMAT("FX{}: {}")(mpt::ufmt::dec0<2>(PluginUi(*plug.front()).GetSlot() + 1), mpt::ToUnicode(m_VstPlugin.GetSoundFile().m_MixPlugins[PluginUi(*plug.front()).GetSlot()].GetName()));
	else
		outputPlugin = UL_("No Plugin");
	m_outputCombo.DeleteString(1);
	m_outputCombo.SetItemData(m_outputCombo.InsertString(1, MPT_UFORMAT("Internal OpenMPT Output ({})")(outputPlugin)), static_cast<uintptr_t>(MidiInOut::kInternalDevice));
	m_outputCombo.SetCurSel(sel);
}


// Refresh current input / output device in GUI
void MidiInOutEditor::SetCurrentDevice(ComboBox &combo, MidiDevice::ID device)
{
	int items = combo.GetCount();
	for(int i = 0; i < items; i++)
	{
		if(static_cast<MidiDevice::ID>(combo.GetItemData(i)) == device)
		{
			combo.SetCurSel(i);
			break;
		}
	}
}


void MidiInOutEditor::OnInputChanged()
{
	MidiDevice::ID newDevice = static_cast<MidiDevice::ID>(m_inputCombo.GetItemData(m_inputCombo.GetCurSel()));
	static_cast<MidiInOut &>(m_VstPlugin).OpenDevice(newDevice, true);
}


void MidiInOutEditor::OnOutputChanged()
{
	MidiDevice::ID newDevice = static_cast<MidiDevice::ID>(m_outputCombo.GetItemData(m_outputCombo.GetCurSel()));
	static_cast<MidiInOut &>(m_VstPlugin).OpenDevice(newDevice, false);
}


void MidiInOutEditor::OnLatencyChanged()
{
	MidiInOut &plugin = static_cast<MidiInOut &>(m_VstPlugin);
	if(!m_locked)
	{
		plugin.m_latency = m_latencySpin.GetValue() * (1.0 / 1000.0);
		PluginUi(plugin).SetModified();
	}
}


void MidiInOutEditor::OnTimingMessagesChanged()
{
	if(!m_locked)
	{
		MidiInOut &plugin = static_cast<MidiInOut &>(m_VstPlugin);
		plugin.m_sendTimingInfo = IsDlgButtonChecked(IDC_CHECK1) != ui::CheckOff;
		PluginUi(plugin).SetModified();
	}
}


void MidiInOutEditor::OnMidiDumpChanged()
{
	if(m_locked)
		return;
	if(ValidateMacroString(m_dumpEdit, m_formattedDump, false, false, true))
	{
		mpt::ustring dumpText;
		m_dumpEdit.GetWindowText(dumpText);
		m_formattedDump = mpt::ToCharset(mpt::Charset::ASCII, dumpText);
		std::vector<uint8> dump;
		dump.reserve(static_cast<size_t>(m_formattedDump.size() / 3));
		bool firstNibble = true;
		uint8 b = 0;
		for(const char c : m_formattedDump)
		{
			if(c >= '0' && c <= '9')
				b |= static_cast<uint8>(c - '0');
			else if(c >= 'A' && c <= 'F')
				b |= static_cast<uint8>(c - 'A' + 0x0A);
			else if(c >= 'a' && c <= 'f')
				b |= static_cast<uint8>(c - 'a' + 0x0A);
			else
				continue;

			if(firstNibble)
			{
				b <<= 4;
			} else
			{
				dump.push_back(b);
				b = 0;
			}
			firstNibble = !firstNibble;
		}
		static_cast<MidiInOut &>(m_VstPlugin).SetInitialMidiDump(std::move(dump));
	}
}


void MidiInOutEditor::OnAlwaysSendDumpChanged()
{
	MidiInOut &plugin = static_cast<MidiInOut &>(m_VstPlugin);
	plugin.m_alwaysSendInitialDump = IsDlgButtonChecked(IDC_CHECK2) == ui::CheckOff;
	PluginUi(plugin).SetModified();
}


void MidiInOutEditor::OnLoadMidiDump()
{
	FileDialog dlg = OpenFileDialog()
						 .DefaultExtension(UL_("syx"))
						 .ExtensionFilter(UL_("SysEx Dumps (*.syx)|*.syx||"));
	if(!dlg.Show(this))
		return;
	mpt::IO::InputFile f(dlg.GetFirstFile());
	if(!f.IsValid())
		return;
	FileReader file = GetFileReader(f);
	std::vector<uint8> dump;
	file.ReadVector(dump, file.GetLength());
	if(!dump.empty() && dump[0] < 0x80)
	{
		if(Reporting::Confirm("This file is most likely not a valid SysEx dump. Load it anyway?") != cnfYes)
			return;
	}
	static_cast<MidiInOut &>(m_VstPlugin).SetInitialMidiDump(std::move(dump));
	m_locked = true;
	UpdateMidiDump();
	m_locked = false;
}


void MidiInOutEditor::OnSendDumpNow()
{
	static_cast<MidiInOut &>(m_VstPlugin).m_initialDumpSent = false;
}


void MidiInOutEditor::OnParamChanged()
{
	int i = m_paramCombo.GetCurSel();
	if(i < 0)
		return;
	MidiInOut &plugin = static_cast<MidiInOut &>(m_VstPlugin);
	m_locked = true;
	if(static_cast<size_t>(i) < plugin.m_parameterMacros.size())
		m_paramEdit.SetWindowText(mpt::ToUnicode(mpt::Charset::ASCII, plugin.m_parameterMacros[i].first));
	else
		m_paramEdit.SetWindowText(UL_(""));
	m_locked = false;
}


void MidiInOutEditor::OnMacroChanged()
{
	int i = m_paramCombo.GetCurSel();
	if(m_locked || i < 0)
		return;
	MidiInOut &plugin = static_cast<MidiInOut &>(m_VstPlugin);
	std::string_view prevMacro;
	if(static_cast<size_t>(i) < plugin.m_parameterMacros.size())
		prevMacro = plugin.m_parameterMacros[i].first;
	if(ValidateMacroString(m_paramEdit, prevMacro, true, true, false))
	{
		mpt::ustring macroText;
		m_paramEdit.GetWindowText(macroText);
		static_cast<MidiInOut &>(m_VstPlugin).SetMacro(static_cast<size_t>(MidiInOut::kMacroParamMin + i), mpt::ToCharset(mpt::Charset::ASCII, macroText));
	}
}


OPENMPT_NAMESPACE_END

