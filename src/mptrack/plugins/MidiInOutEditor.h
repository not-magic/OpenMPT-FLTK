/*
 * MidiInOutEditor.h
 * -----------------
 * Purpose: Editor interface for the MidiInOut plugin.
 * Notes  : (currently none)
 * Authors: Johannes Schultz (OpenMPT Devs)
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/plugins/MidiInOutEditor.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "../ui/Ui.h"


#include "MidiInOut.h"
#include "../AbstractVstEditor.h"
#include "../CDecimalSupport.h"

OPENMPT_NAMESPACE_BEGIN

class MidiInOutEditor : public CAbstractVstEditor
{
protected:
	ComboBox m_inputCombo, m_outputCombo, m_paramCombo;
	Spinner m_latencySpin;
	Edit m_dumpEdit, m_paramEdit;
	std::string m_formattedDump;
	bool m_locked = true;

public:

	MidiInOutEditor(MidiInOut &plugin);

	// Refresh current input / output device in GUI
	void SetCurrentDevice(bool asInputDevice, MidiDevice::ID device)
	{
		ComboBox &combo = asInputDevice ? m_inputCombo : m_outputCombo;
		SetCurrentDevice(combo, device);
	}

	bool OpenEditor(Wnd *parent) override;
	void UpdateView(UpdateHint hint) override;
	bool IsResizable() const override { return false; }
	bool SetSize(int, int) override { return false; }

protected:

	// Update lists of available input / output devices
	static void PopulateList(ComboBox &combo, RtMidi &rtDevice, MidiDevice &midiDevice, bool isInput);
	void UpdateOutputPlugin();
	// Refresh current input / output device in GUI
	void SetCurrentDevice(ComboBox &combo, MidiDevice::ID device);

	void UpdateMidiDump();

	void DoDataExchange(DataExchange *pDX) override;

	void OnInputChanged();
	void OnOutputChanged();
	void OnLatencyChanged();
	void OnMidiDumpChanged();
	void OnTimingMessagesChanged();
	void OnAlwaysSendDumpChanged();
	void OnLoadMidiDump();
	void OnSendDumpNow();
	void OnParamChanged();
	void OnMacroChanged();

	UI_DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END

