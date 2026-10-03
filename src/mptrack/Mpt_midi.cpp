/*
 * MPT_MIDI.cpp
 * ------------
 * Purpose: MIDI Input handling code.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "ui/Ui.h"
#include "DlsBankExt.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "WindowMessages.h"
#include "../soundlib/MIDIEvents.h"

#include "../include/rtmidi/RtMidi.h"


OPENMPT_NAMESPACE_BEGIN


#ifdef MPT_ALL_LOGGING
#define MPTMIDI_RECORDLOG
#endif

//Get Midi message(dwParam1), apply MIDI settings having effect on volume, and return
//the volume value [0, 256]. In addition value -1 is used as 'use default value'-indicator.
int CMainFrame::ApplyVolumeRelatedSettings(const uint32 &dwParam1, uint8 midiChannelVolume)
{
	int nVol = MIDIEvents::GetDataByte2FromEvent(dwParam1);
	const FlagSet<MidiSetup> midiSetup = TrackerSettings::Instance().midiSetup;
	if(midiSetup[MidiSetup::RecordVelocity])
	{
		if(!midiSetup[MidiSetup::ApplyChannelVolumeToVelocity])
			midiChannelVolume = 127;
		nVol = Util::muldivr_unsigned(CDLSBank::DLSMidiVolumeToLinear(nVol), TrackerSettings::Instance().midiVelocityAmp * midiChannelVolume, 100 * 127 * 256);
		Limit(nVol, 1, 256);
	} else
	{
		// Case: No velocity record.
		if(midiSetup[MidiSetup::ApplyChannelVolumeToVelocity])
			nVol = (midiChannelVolume + 1) * 2;
		else //Use default volume
			nVol = -1;
	}

	return nVol;
}


void ApplyTransposeKeyboardSetting(CMainFrame &rMainFrm, uint32 &midiMsg)
{
	const FlagSet<MidiSetup> midiSetup = TrackerSettings::Instance().midiSetup;
	if(midiSetup[MidiSetup::TransposeKeyboard]
		&& (MIDIEvents::GetChannelFromEvent(midiMsg) != 9))
	{
		int nTranspose = rMainFrm.GetBaseOctave() - 4;
		if (nTranspose)
		{
			int note = MIDIEvents::GetDataByte1FromEvent(midiMsg);
			if (note < 0x80)
			{
				note += nTranspose * 12;
				Limit(note, 0, NOTE_MAX - NOTE_MIN);

				midiMsg &= 0xffff00ff;

				midiMsg |= (note << 8);
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
// Midi Record

namespace
{

void MidiInCallBack(double timestamp, std::vector<unsigned char> *message, void *userData)
{
	CMainFrame *pMainFrm = static_cast<CMainFrame *>(userData);
	if(!pMainFrm || !message || message->empty())
		return;

	const auto &bytes = *message;
	if(bytes[0] == 0xF0)
	{
		// SysEx
		std::lock_guard<mpt::mutex> guard{pMainFrm->midiInData.dataMutex};
		if(auto callback = pMainFrm->GetMidiSysexCallback())
			callback(mpt::const_byte_span{mpt::byte_cast<const std::byte *>(bytes.data()), bytes.size()});
		return;
	}

	uint32 data = bytes[0];
	if(bytes.size() > 1)
		data |= static_cast<uint32>(bytes[1]) << 8;
	if(bytes.size() > 2)
		data |= static_cast<uint32>(bytes[2]) << 16;

#ifdef MPTMIDI_RECORDLOG
	MPT_LOG_GLOBAL(LogDebug, "MIDI", MPT_UFORMAT("time={}s status={} data={}.{}")(timestamp, mpt::ufmt::HEX0<2>(bytes[0]), mpt::ufmt::HEX0<2>((data >> 8) & 0xFF), mpt::ufmt::HEX0<2>((data >> 16) & 0xFF)));
#endif

	WindowHandle hWndMidi = pMainFrm->GetMidiRecordWnd();
	if(hWndMidi != nullptr)
	{
		switch(MIDIEvents::GetTypeFromEvent(data))
		{
		case MIDIEvents::evNoteOff:	// Note Off
		case MIDIEvents::evNoteOn:	// Note On
			ApplyTransposeKeyboardSetting(*pMainFrm, data);
			[[fallthrough]];
		default:
			hWndMidi->PostMessage(MSG_MOD_MIDIMSG, data, static_cast<LParam>(timestamp * 1000.0));
			return;	// Message has been handled
		}
	}
	// Pass MIDI to keyboard handler
	CMainFrame::GetInputHandler()->HandleMIDIMessage(kCtxAllContexts, data);
}

}


std::vector<mpt::ustring> CMainFrame::GetMidiInputDeviceNames()
{
	std::vector<mpt::ustring> names;
	try
	{
		RtMidiIn probe(RtMidi::UNSPECIFIED, "OpenMPT");
		const unsigned int count = probe.getPortCount();
		for(unsigned int i = 0; i < count; ++i)
			names.push_back(mpt::transcode<mpt::ustring>(mpt::common_encoding::utf8, probe.getPortName(i)));
	} catch(const RtMidiError &)
	{
	}
	return names;
}


bool CMainFrame::midiOpenDevice(bool showSettings)
{
	if(midiInData.isOpen)
		return true;

	const auto tryOpen = [this]() -> bool
	{
		try
		{
			auto input = std::make_unique<RtMidiIn>(RtMidi::UNSPECIFIED, "OpenMPT");
			const uint32 device = TrackerSettings::Instance().GetCurrentMIDIDevice();
			if(device >= input->getPortCount())
				return false;
			input->openPort(device, "OpenMPT input");
			input->ignoreTypes(false, true, true);
			input->setCallback(&MidiInCallBack, this);
			midiInData.input = std::move(input);
			midiInData.isOpen = true;
			return true;
		} catch(const RtMidiError &)
		{
			midiInData.input.reset();
			return false;
		}
	};

	if(!tryOpen())
	{
		// Show MIDI configuration on fail.
		if(showSettings)
		{
			CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_MIDI;
			CMainFrame::GetMainFrame()->OnViewOptions();
		}

		// Let's see if the user updated the settings.
		if(!tryOpen())
			return false;
	}
	return true;
}


void CMainFrame::midiCloseDevice()
{
	if(midiInData.isOpen)
	{
		midiInData.isOpen = false;
		std::lock_guard<mpt::mutex> guard{midiInData.dataMutex};
		if(midiInData.input)
		{
			midiInData.input->cancelCallback();
			midiInData.input->closePort();
			midiInData.input.reset();
		}
	}
}


void CMainFrame::OnMidiRecord()
{
	if(midiInData.isOpen)
	{
		midiCloseDevice();
	} else
	{
		midiOpenDevice();
	}
}


void CMainFrame::OnUpdateMidiRecord(CmdUI *pCmdUI)
{
	if (pCmdUI) pCmdUI->SetCheck(midiInData.isOpen ? true : false);
}


OPENMPT_NAMESPACE_END
