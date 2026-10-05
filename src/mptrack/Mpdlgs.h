/*
 * MPDlgs.h
 * --------
 * Purpose: Implementation of various player setup dialogs.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/Mpdlgs.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "TrackerSettings.h"
#include "openmpt_ext/sounddsp/SoundDspExt.h"
#include "openmpt/sounddevice/SoundDevice.hpp"

OPENMPT_NAMESPACE_BEGIN

class CTrackerSoundFile;
class CMainFrame;

#define NUM_CHANNELCOMBOBOXES	4

class COptionsSoundcard: public PropertyPage
{
protected:
	ComboBox m_CbnDevice;
	ComboBox m_CbnLatencyMS, m_CbnUpdateIntervalMS, m_CbnMixingFreq, m_CbnChannels, m_CbnSampleFormat, m_CbnDither, m_CbnRecordingChannels, m_CbnRecordingSource;
	Edit m_EditStatistics;
	Button m_BtnDriverPanel;

	ComboBox m_CbnStoppedMode;

	ComboBox m_CbnChannelMapping[NUM_CHANNELCOMBOBOXES];

	SoundDevice::Identifier m_InitialDeviceIdentifier;

	void SetInitialDevice();

	void SetDevice(SoundDevice::Identifier dev, bool forceReload=false);
	SoundDevice::Info m_CurrentDeviceInfo;
	SoundDevice::Caps m_CurrentDeviceCaps;
	SoundDevice::DynamicCaps m_CurrentDeviceDynamicCaps;
	SoundDevice::Settings m_Settings;

public:
	COptionsSoundcard(SoundDevice::Identifier deviceIdentifier);

	void UpdateStatistics();

private:
	void UpdateEverything();
	void UpdateDevice();
	void UpdateGeneral();
	void UpdateLatency();
	void UpdateUpdateInterval();
	void UpdateSampleRates();
	void UpdateChannels();
	void UpdateSampleFormat();
	void UpdateDither();
	void UpdateChannelMapping();
	void UpdateRecording();
	void UpdateControls();

protected:
	bool OnInitDialog() override;
	void OnOK() override;
	bool OnSetActive() override;
	void DoDataExchange(DataExchange* pDX) override;
	void UpdateStereoSep();

	void OnDeviceChanged();
	void OnSettingsChanged() { SetModified(true); }
	void OnExclusiveModeChanged();
	void OnChannelsChanged();
	void OnSampleFormatChanged();
	void OnRecordingChanged();
	void OnSoundCardShowAll();
	void OnSoundCardRescan();
	void OnSoundCardDriverPanel();

	void OnChannelChanged(int channel);
	void OnChannel1Changed() { OnChannelChanged(0); };
	void OnChannel2Changed() { OnChannelChanged(1); };
	void OnChannel3Changed() { OnChannelChanged(2); };
	void OnChannel4Changed() { OnChannelChanged(3); };

	UI_DECLARE_MESSAGE_MAP()
};


class COptionsMixer: public PropertyPage
{
protected:

	ComboBox m_CbnResampling, m_CbnAmigaType;

	Spinner m_CEditRampUp;
	Spinner m_CEditRampDown;
	Edit m_CInfoRampUp;
	Edit m_CInfoRampDown;

	HSlider m_SliderStereoSep;

	// check box soft pan

	HSlider m_SliderPreAmp;

	bool m_initialized = false;

public:
	COptionsMixer();

protected:
	void UpdateRamping();
	void UpdateStereoSep();

	bool OnInitDialog() override;
	void OnOK() override;
	bool OnSetActive() override;
	void DoDataExchange(DataExchange* pDX) override;

	void OnSettingsChanged() { SetModified(true); }
	void OnAmigaChanged();
	void OnRampingChanged();
	void OnDefaultRampSettings();

	void OnHScroll(uint32 n, uint32 pos, Wnd *p) override;

	UI_DECLARE_MESSAGE_MAP()

};



class CEQSlider: public VSlider
{
public:
	Wnd *m_pParent;
	uint32 m_nSliderNo;
	short int m_x, m_y;
public:
	CEQSlider() = default;
	void Init(uint32 nID, uint32 n, Wnd *parent);
	bool PreTranslateMessage(int event);
};



class COptionsPlayer: public PropertyPage
{
protected:
	ComboBox m_CbnReverbPreset;
	HSlider m_SbXBassDepth, m_SbXBassRange;
	HSlider m_SbSurroundDepth, m_SbSurroundDelay;
	HSlider m_SbReverbDepth;
	HSlider m_SbBitCrushBits;

	CEQSlider m_Sliders[MAX_EQ_BANDS];
	EQPreset &m_EQPreset;
	uint32 m_nSliderMenu;

public:
	COptionsPlayer();

protected:
	bool OnInitDialog() override;
	void OnOK() override;
	bool OnSetActive() override;
	void DoDataExchange(DataExchange* pDX) override;
	void OnHScroll(uint32, uint32, Wnd *) override;
	void OnSettingsChanged() { SetModified(true); }

	void OnVScroll(uint32 nSBCode, uint32 nPos, Wnd * pScrollBar) override;
	void OnEqUser1() { LoadEQPreset(TrackerSettings::Instance().m_EqUserPresets[0]); };
	void OnEqUser2() { LoadEQPreset(TrackerSettings::Instance().m_EqUserPresets[1]); };
	void OnEqUser3() { LoadEQPreset(TrackerSettings::Instance().m_EqUserPresets[2]); };
	void OnEqUser4() { LoadEQPreset(TrackerSettings::Instance().m_EqUserPresets[3]); };
	void OnSavePreset();
	void OnSliderMenu(uint32);
	void OnSliderFreq(uint32);

	void UpdateDialog();
	void UpdateEQ(bool bReset);
	void LoadEQPreset(const EQPreset &preset);

	UI_DECLARE_MESSAGE_MAP()
};


class CMidiSetupDlg: public PropertyPage
{
public:
	FlagSet<MidiSetup> m_midiSetup;
	uint32 m_nMidiDevice;

public:
	CMidiSetupDlg(FlagSet<MidiSetup> flags, uint32 device);

protected:
	bool OnInitDialog() override;
	void OnOK() override;
	bool OnSetActive() override;
	void DoDataExchange(DataExchange* pDX) override;
	void RefreshDeviceList(uint32 currentDevice);
	void OnRenameDevice();
	void OnSettingsChanged() { SetModified(true); }
	UI_DECLARE_MESSAGE_MAP()

protected:
	Spinner m_SpinSpd, m_SpinPat, m_SpinAmp;
	ComboBox m_InputDevice, m_ATBehaviour, m_Quantize, m_ContinueMode, m_RecordPitchBend;
};



class COptionsWine: public PropertyPage
{

protected:
	ComboBox m_CbnPulseAudio;
	ComboBox m_CbnPortAudio;
	ComboBox m_CbnRtAudio;

public:
	COptionsWine();

protected:
	bool OnInitDialog() override;
	void OnOK() override;
	bool OnSetActive() override;
	void DoDataExchange(DataExchange* pDX) override;

	void OnSettingsChanged();
	
	UI_DECLARE_MESSAGE_MAP()
};



OPENMPT_NAMESPACE_END
