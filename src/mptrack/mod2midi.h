/*
 * mod2midi.h
 * ----------
 * Purpose: Module to MIDI conversion (dialog + conversion code).
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/mod2midi.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "DialogBase.h"
#include "ProgressDialog.h"
#include "../soundlib/Snd_defs.h"

OPENMPT_NAMESPACE_BEGIN

class CModDoc;
struct SubSong;

namespace MidiExport
{
	struct Mod2MidiInstr
	{
		uint8 channel = MidiMappedChannel; // See enum MidiChannel
		uint8 program = 0;
	};
	using InstrMap = std::vector<Mod2MidiInstr>;
}


class CModToMidi : public CProgressDialog
{
protected:
	ComboBox m_CbnInstrument, m_CbnChannel, m_CbnProgram;
	Spinner m_EditSubSong;
	SpinButton m_SpinInstrument;
	CModDoc &m_modDoc;
	MidiExport::InstrMap m_instrMap;
	std::vector<SubSong> m_subSongs;
	size_t m_selectedSong = 0;
	size_t m_currentInstr = 1;
	bool m_percussion = false;
	bool m_conversionRunning = false;
	bool m_locked = true;
public:
	static bool s_overlappingInstruments;

public:
	CModToMidi(CModDoc &modDoc, Wnd *parent = nullptr);
	~CModToMidi();

protected:
	void Run() override {};
	
	void UpdateSubsongName();
	void DoConversion(const mpt::PathString &fileName);

	void OnOK() override;
	void OnCancel() override;
	bool OnInitDialog() override;
	void DoDataExchange(DataExchange *pDX) override;
	void FillProgramBox(bool percussion);
	void OnVScroll(uint32 nSBCode, uint32 nPos, Wnd * pScrollBar) override;
	void UpdateDialog();
	void OnChannelChanged();
	void OnProgramChanged();
	void OnOverlapChanged();
	void OnSubsongChanged();

	UI_DECLARE_MESSAGE_MAP()
};


OPENMPT_NAMESPACE_END
