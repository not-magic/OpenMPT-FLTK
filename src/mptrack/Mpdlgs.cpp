/*
 * MPDlgs.cpp
 * ----------
 * Purpose: Implementation of various player setup dialogs.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "ui/Ui.h"
#include "Mpdlgs.h"
#include "DialogBase.h"
#include "dlg_misc.h"
#include "ImageLists.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "resource.h"
#include "Sndfile.h"
#include "WindowMessages.h"
#include "../common/mptStringBuffer.h"
#include "../soundlib/mod_specifications.h"
#include "mpt/parse/parse.hpp"
#include "openmpt/sounddevice/SoundDevice.hpp"
#include "openmpt/sounddevice/SoundDeviceManager.hpp"


OPENMPT_NAMESPACE_BEGIN


const mpt::uchar *gszChnCfgNames[3] =
{
	UL_("Mono"),
	UL_("Stereo"),
	UL_("Quad")
};


static double ParseTime(mpt::ustring str)
{
	return mpt::parse<double>(mpt::ToCharset(mpt::Charset::ASCII, str)) / 1000.0;
}


static mpt::ustring PrintTime(double seconds)
{
	int32 microseconds = mpt::saturate_round<int32>(seconds * 1000000.0);
	int precision = 0;
	if(microseconds < 1000)
	{
		precision = 3;
	} else if(microseconds < 10000)
	{
		precision = 2;
	} else if(microseconds < 100000)
	{
		precision = 1;
	} else
	{
		precision = 0;
	}
	return MPT_UFORMAT("{} ms")(mpt::ufmt::fix(seconds * 1000.0, precision));
}


UI_MESSAGE_MAP_BEGIN(COptionsSoundcard, PropertyPage)
	UI_COMMAND(IDC_CHECK4,	&COptionsSoundcard::OnExclusiveModeChanged)
	UI_COMMAND(IDC_CHECK5,	&COptionsSoundcard::OnSettingsChanged)
	UI_COMMAND(IDC_CHECK7,	&COptionsSoundcard::OnSettingsChanged)
	UI_COMMAND(IDC_CHECK9,	&COptionsSoundcard::OnSettingsChanged)
	UI_COMMAND(IDC_CHECK_SOUNDCARD_SHOWALL, &COptionsSoundcard::OnSoundCardShowAll)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO1, &COptionsSoundcard::OnDeviceChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO2, &COptionsSoundcard::OnSettingsChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO_UPDATEINTERVAL, &COptionsSoundcard::OnSettingsChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO3, &COptionsSoundcard::OnSettingsChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO4, &COptionsSoundcard::OnSettingsChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO5, &COptionsSoundcard::OnChannelsChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO6, &COptionsSoundcard::OnSampleFormatChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO10, &COptionsSoundcard::OnSettingsChanged)
	UI_NOTIFY(ui::ComboEditChange, IDC_COMBO2, &COptionsSoundcard::OnSettingsChanged)
	UI_NOTIFY(ui::ComboEditChange, IDC_COMBO_UPDATEINTERVAL, &COptionsSoundcard::OnSettingsChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO11, &COptionsSoundcard::OnSettingsChanged)
	UI_COMMAND(IDC_BUTTON1,	&COptionsSoundcard::OnSoundCardRescan)
	UI_COMMAND(IDC_BUTTON2,	&COptionsSoundcard::OnSoundCardDriverPanel)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO_CHANNEL_FRONTLEFT, &COptionsSoundcard::OnChannel1Changed)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO_CHANNEL_FRONTRIGHT, &COptionsSoundcard::OnChannel2Changed)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO_CHANNEL_REARLEFT, &COptionsSoundcard::OnChannel3Changed)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO_CHANNEL_REARRIGHT, &COptionsSoundcard::OnChannel4Changed)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO_RECORDING_CHANNELS, &COptionsSoundcard::OnRecordingChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO_RECORDING_SOURCE, &COptionsSoundcard::OnSettingsChanged)
UI_MESSAGE_MAP_END()


void COptionsSoundcard::OnSampleFormatChanged()
{
	OnSettingsChanged();
	UpdateDither();
}


void COptionsSoundcard::OnRecordingChanged()
{
	uintptr_t inputChannels = m_CbnRecordingChannels.GetItemData(m_CbnRecordingChannels.GetCurSel());
	m_CbnRecordingSource.EnableWindow((m_CurrentDeviceCaps.HasNamedInputSources && inputChannels > 0) ? true : false);
	OnSettingsChanged();
}


void COptionsSoundcard::DoDataExchange(DataExchange* pDX)
{
	PropertyPage::DoDataExchange(pDX);
	pDX->BindControl(IDC_COMBO1, m_CbnDevice);
	pDX->BindControl(IDC_COMBO2, m_CbnLatencyMS);
	pDX->BindControl(IDC_COMBO_UPDATEINTERVAL, m_CbnUpdateIntervalMS);
	pDX->BindControl(IDC_COMBO3, m_CbnMixingFreq);
	pDX->BindControl(IDC_COMBO5, m_CbnChannels);
	pDX->BindControl(IDC_COMBO6, m_CbnSampleFormat);
	pDX->BindControl(IDC_COMBO10, m_CbnDither);
	pDX->BindControl(IDC_BUTTON2, m_BtnDriverPanel);
	pDX->BindControl(IDC_COMBO6, m_CbnSampleFormat);
	pDX->BindControl(IDC_COMBO11, m_CbnStoppedMode);
	pDX->BindControl(IDC_COMBO_CHANNEL_FRONTLEFT, m_CbnChannelMapping[0]);
	pDX->BindControl(IDC_COMBO_CHANNEL_FRONTRIGHT, m_CbnChannelMapping[1]);
	pDX->BindControl(IDC_COMBO_CHANNEL_REARLEFT, m_CbnChannelMapping[2]);
	pDX->BindControl(IDC_COMBO_CHANNEL_REARRIGHT, m_CbnChannelMapping[3]);
	pDX->BindControl(IDC_COMBO_RECORDING_CHANNELS, m_CbnRecordingChannels);
	pDX->BindControl(IDC_COMBO_RECORDING_SOURCE, m_CbnRecordingSource);
	pDX->BindControl(IDC_EDIT_STATISTICS, m_EditStatistics);
}


COptionsSoundcard::COptionsSoundcard(SoundDevice::Identifier deviceIdentifier)
	: PropertyPage(IDD_OPTIONS_SOUNDCARD)
	, m_InitialDeviceIdentifier(deviceIdentifier)
{
	return;
}


void COptionsSoundcard::SetInitialDevice()
{
	SetDevice(m_InitialDeviceIdentifier, true);
}


void COptionsSoundcard::SetDevice(SoundDevice::Identifier dev, bool forceReload)
{
	SoundDevice::Identifier olddev = m_CurrentDeviceInfo.GetIdentifier();
	SoundDevice::Info newInfo;
	SoundDevice::Caps newCaps;
	SoundDevice::DynamicCaps newDynamicCaps;
	SoundDevice::Settings newSettings;
	newInfo = theApp.GetSoundDevicesManager()->FindDeviceInfo(dev);
	newCaps = theApp.GetSoundDevicesManager()->GetDeviceCaps(dev, CMainFrame::GetMainFrame()->gpSoundDevice);
	newDynamicCaps = theApp.GetSoundDevicesManager()->GetDeviceDynamicCaps(dev, TrackerSettings::Instance().GetSampleRates(), CMainFrame::GetMainFrame(), CMainFrame::GetMainFrame()->gpSoundDevice, true);
	bool deviceChanged = (dev != olddev);
	if(deviceChanged || forceReload)
	{
		newSettings = TrackerSettings::Instance().GetSoundDeviceSettings(dev);
	} else
	{
		newSettings = m_Settings;
	}
	m_CurrentDeviceInfo = newInfo;
	m_CurrentDeviceCaps = newCaps;
	m_CurrentDeviceDynamicCaps = newDynamicCaps;
	m_Settings = newSettings;
}


void COptionsSoundcard::OnSoundCardShowAll()
{
	TrackerSettings::Instance().m_SoundShowDeprecatedDevices = (IsDlgButtonChecked(IDC_CHECK_SOUNDCARD_SHOWALL) == ui::CheckOn);
	SetDevice(m_CurrentDeviceInfo.GetIdentifier(), true);
	UpdateEverything();
}


void COptionsSoundcard::OnSoundCardRescan()
{
	{
		// Close sound device because IDs might change when re-enumerating which could cause all kinds of havoc.
		CMainFrame::GetMainFrame()->audioCloseDevice();
		delete CMainFrame::GetMainFrame()->gpSoundDevice;
		CMainFrame::GetMainFrame()->gpSoundDevice = nullptr;
	}
	theApp.GetSoundDevicesManager()->ReEnumerate();
	SetDevice(m_CurrentDeviceInfo.GetIdentifier(), true);
	UpdateEverything();
}


bool COptionsSoundcard::OnInitDialog()
{
	PropertyPage::OnInitDialog();
	SetInitialDevice();
	UpdateEverything();
	return true;
}


void COptionsSoundcard::UpdateLatency()
{
	{
		GetDlgItem(IDC_STATIC_LATENCY)->EnableWindow(true);
		m_CbnLatencyMS.EnableWindow(true);
	}
	// latency
	{
		static constexpr double latencies [] = {
			0.001,
			0.002,
			0.003,
			0.004,
			0.005,
			0.010,
			0.015,
			0.020,
			0.025,
			0.030,
			0.040,
			0.050,
			0.075,
			0.100,
			0.150,
			0.200,
			0.250
		};
		m_CbnLatencyMS.ResetContent();
		m_CbnLatencyMS.SetWindowText(PrintTime(m_Settings.Latency));
		for(auto lat : latencies)
		{
			if(m_CurrentDeviceCaps.LatencyMin <= lat && lat <= m_CurrentDeviceCaps.LatencyMax)
			{
				m_CbnLatencyMS.AddString(PrintTime(lat));
			}
		}
	}
	if(theApp.GetSoundDevicesManager()->IsDeviceUnavailable(m_CurrentDeviceInfo.GetIdentifier()))
	{
		GetDlgItem(IDC_STATIC_LATENCY)->EnableWindow(false);
		m_CbnLatencyMS.EnableWindow(false);
	}
}


void COptionsSoundcard::UpdateUpdateInterval()
{
	{
		m_CbnUpdateIntervalMS.EnableWindow(true);
	}
	// update interval
	{
		static constexpr double updateIntervals [] = {
			0.001,
			0.002,
			0.005,
			0.010,
			0.015,
			0.020,
			0.025,
			0.050
		};
		m_CbnUpdateIntervalMS.ResetContent();
		m_CbnUpdateIntervalMS.SetWindowText(PrintTime(m_Settings.UpdateInterval));
		for(auto upd : updateIntervals)
		{
			if(m_CurrentDeviceCaps.UpdateIntervalMin <= upd && upd <= m_CurrentDeviceCaps.UpdateIntervalMax)
			{
				m_CbnUpdateIntervalMS.AddString(PrintTime(upd));
			}
		}
	}
	if(theApp.GetSoundDevicesManager()->IsDeviceUnavailable(m_CurrentDeviceInfo.GetIdentifier()) || !m_CurrentDeviceCaps.CanUpdateInterval)
	{
		m_CbnUpdateIntervalMS.EnableWindow(false);
	}
}


void COptionsSoundcard::UpdateGeneral()
{
	// General
	{
		if(m_CurrentDeviceCaps.CanKeepDeviceRunning)
		{
			m_CbnStoppedMode.ResetContent();
			m_CbnStoppedMode.AddString(UL_("Close driver"));
			m_CbnStoppedMode.AddString(UL_("Pause driver"));
			m_CbnStoppedMode.AddString(UL_("Play silence"));
			m_CbnStoppedMode.SetCurSel(TrackerSettings::Instance().m_SoundSettingsStopMode);
		} else
		{
			m_CbnStoppedMode.ResetContent();
			m_CbnStoppedMode.AddString(UL_("Close driver"));
			m_CbnStoppedMode.AddString(UL_("Close driver"));
			m_CbnStoppedMode.AddString(UL_("Close driver"));
			m_CbnStoppedMode.SetCurSel(TrackerSettings::Instance().m_SoundSettingsStopMode);
		}
		CheckDlgButton(IDC_CHECK7, TrackerSettings::Instance().m_SoundSettingsOpenDeviceAtStartup ? ui::CheckOn : ui::CheckOff);
	}
	bool isUnavailble = theApp.GetSoundDevicesManager()->IsDeviceUnavailable(m_CurrentDeviceInfo.GetIdentifier());
	m_CbnStoppedMode.EnableWindow(isUnavailble ? false : (m_CurrentDeviceCaps.CanKeepDeviceRunning ? true : false));
	PropertySheet *sheet = dynamic_cast<PropertySheet *>(GetParent());
	if(sheet) sheet->GetDlgItem(IDOK)->EnableWindow(isUnavailble ? false : true);
}


void COptionsSoundcard::UpdateEverything()
{
	// Sound Device
	{
		if(m_CurrentDeviceInfo.IsDeprecated())
		{
			TrackerSettings::Instance().m_SoundShowDeprecatedDevices = true;
		}
		CheckDlgButton(IDC_CHECK_SOUNDCARD_SHOWALL, TrackerSettings::Instance().m_SoundShowDeprecatedDevices ? ui::CheckOn : ui::CheckOff);

		m_CbnDevice.ResetContent();
		m_CbnDevice.SetImageList(&CMainFrame::GetMainFrame()->m_MiscIcons);

		uint32 iItem = 0;

		for(const auto &it : *theApp.GetSoundDevicesManager())
		{

			if(!TrackerSettings::Instance().m_SoundShowDeprecatedDevices)
			{
				if(it.IsDeprecated())
				{
					continue;
				}
			}

			{
				const SoundDevice::Manager::GlobalID globalId = theApp.GetSoundDevicesManager()->GetGlobalID(it.GetIdentifier());
				int image = 0;
				if(it.type == SoundDevice::TypeWAVEOUT || it.type == SoundDevice::TypePORTAUDIO_WMME)
				{
					image = IMAGE_WAVEOUT;
				} else if(it.type == SoundDevice::TypeDSOUND || it.type == SoundDevice::TypePORTAUDIO_DS || it.type == UL_("RtAudio-ds"))
				{
					image = IMAGE_DIRECTX;
				} else if(it.type == SoundDevice::TypeASIO || it.type == UL_("RtAudio-asio"))
				{
					image = IMAGE_ASIO;
				} else if(it.type == SoundDevice::TypePORTAUDIO_WASAPI || it.type == UL_("RtAudio-wasapi"))
				{
					image = IMAGE_SAMPLEMUTE; // No real image available for now,
				} else if(it.type == SoundDevice::TypePORTAUDIO_WDMKS)
				{
					image = IMAGE_CHIP; // No real image available for now,
				} else if(it.type.find(UL_("Wine-Native-")) == 0)
				{
					image = IMAGE_TUX;
				}
				const int pos = m_CbnDevice.InsertItem(static_cast<int>(iItem), mpt::ToUnicode(it.GetDisplayName()), image, static_cast<uintptr_t>(globalId));
				if(globalId == theApp.GetSoundDevicesManager()->GetGlobalID(m_CurrentDeviceInfo.GetIdentifier()))
				{
					m_CbnDevice.SetCurSel(pos);
				}
				iItem++;
			}
		}
	}

	UpdateDevice();

}


void COptionsSoundcard::UpdateDevice()
{
	GetDlgItem(IDC_CHECK_SOUNDCARD_SHOWALL)->EnableWindow(m_CurrentDeviceInfo.IsDeprecated() ? false : true);
	UpdateGeneral();
	UpdateControls();
	UpdateLatency();
	UpdateUpdateInterval();
	UpdateSampleRates();
	UpdateChannels();
	UpdateSampleFormat();
	UpdateDither();
	UpdateChannelMapping();
	UpdateRecording();
}


void COptionsSoundcard::UpdateChannels()
{
	{
		m_CbnChannels.EnableWindow(true);
	}
	m_CbnChannels.ResetContent();
	int maxChannels = 0;
	if(m_CurrentDeviceDynamicCaps.channelNames.size() > 0)
	{
		maxChannels = static_cast<int>(std::min(std::size_t(4), m_CurrentDeviceDynamicCaps.channelNames.size()));
	} else
	{
		maxChannels = 4;
	}
	int sel = 0;
	for(int channels = maxChannels; channels >= 1; channels /= 2)
	{
		int ndx = m_CbnChannels.AddString(gszChnCfgNames[(channels+2)/2-1]);
		m_CbnChannels.SetItemData(ndx, channels);
		if(channels == m_Settings.Channels)
		{
			sel = ndx;
		}
	}
	m_CbnChannels.SetCurSel(sel);
	if(theApp.GetSoundDevicesManager()->IsDeviceUnavailable(m_CurrentDeviceInfo.GetIdentifier()))
	{
		m_CbnChannels.EnableWindow(false);
	}
}


void COptionsSoundcard::UpdateRecording()
{
	GetDlgItem(IDC_STATIC_RECORDING)->ShowWindow(TrackerSettings::Instance().m_SoundShowRecordingSettings ? true : false);
	m_CbnRecordingChannels.ShowWindow(TrackerSettings::Instance().m_SoundShowRecordingSettings ? true : false);
	m_CbnRecordingSource.ShowWindow(TrackerSettings::Instance().m_SoundShowRecordingSettings ? true : false);
	m_CbnRecordingChannels.ResetContent();
	m_CbnRecordingSource.ResetContent();
	if(m_CurrentDeviceCaps.CanInput && ((m_CurrentDeviceCaps.HasNamedInputSources && m_CurrentDeviceDynamicCaps.inputSourceNames.size() > 0) || !m_CurrentDeviceCaps.HasNamedInputSources))
	{
		GetDlgItem(IDC_STATIC_RECORDING)->EnableWindow(true);
		m_CbnRecordingChannels.EnableWindow(true);
		int sel = 0;
		{
			int ndx = m_CbnRecordingChannels.AddString(UL_("off"));
			m_CbnRecordingChannels.SetItemData(ndx, 0);
			if(0 == m_Settings.InputChannels)
			{
				sel = ndx;
			}
		}
		for(int channels = 4; channels >= 1; channels /= 2)
		{
			int ndx = m_CbnRecordingChannels.AddString(gszChnCfgNames[(channels+2)/2-1]);
			m_CbnRecordingChannels.SetItemData(ndx, channels);
			if(channels == m_Settings.InputChannels)
			{
				sel = ndx;
			}
		}
		m_CbnRecordingChannels.SetCurSel(sel);
		if(m_CurrentDeviceCaps.HasNamedInputSources)
		{
			m_CbnRecordingSource.EnableWindow((m_Settings.InputChannels > 0) ? true : false);
			sel = -1;
			for(size_t ch = 0; ch < m_CurrentDeviceDynamicCaps.inputSourceNames.size(); ch++)
			{
				const int pos = m_CbnRecordingSource.AddString(mpt::ToUnicode(m_CurrentDeviceDynamicCaps.inputSourceNames[ch].second));
				m_CbnRecordingSource.SetItemData(pos, (uintptr_t)m_CurrentDeviceDynamicCaps.inputSourceNames[ch].first);
				if(m_CurrentDeviceDynamicCaps.inputSourceNames[ch].first == m_Settings.InputSourceID)
				{
					sel = pos;
				}
			}
			if(sel == -1 ) sel = 0;
			m_CbnRecordingSource.SetCurSel(sel);
		} else
		{
			m_CbnRecordingSource.EnableWindow(false);
		}
	} else
	{
		GetDlgItem(IDC_STATIC_RECORDING)->EnableWindow(false);
		m_CbnRecordingChannels.EnableWindow(false);
		int ndx = m_CbnRecordingChannels.AddString(UL_("off"));
		m_CbnRecordingChannels.SetItemData(ndx, 0);
		m_CbnRecordingChannels.SetCurSel(ndx);
		m_CbnRecordingSource.EnableWindow(false);
	}
}


void COptionsSoundcard::UpdateSampleFormat()
{
	{
		m_CbnSampleFormat.EnableWindow(true);
	}
	uint32 n = 0;
	m_CbnSampleFormat.ResetContent();
	std::vector<SampleFormat> sampleformats;
	if(IsDlgButtonChecked(IDC_CHECK4))
	{
		sampleformats = m_CurrentDeviceDynamicCaps.supportedExclusiveModeSampleFormats;
	} else
	{
		sampleformats = m_CurrentDeviceDynamicCaps.supportedSampleFormats;
	}
	m_CbnSampleFormat.EnableWindow(m_CurrentDeviceCaps.CanSampleFormat && (sampleformats.size() != 1) ? true : false);
	const std::vector<SampleFormat> allSampleFormats = AllSampleFormats<std::vector<SampleFormat>>();
	for(const auto sampleFormat : allSampleFormats)
	{
		if(!sampleformats.empty() && !mpt::contains(sampleformats, sampleFormat))
		{
			continue;
		}
		mpt::ustring name;
		if(sampleFormat.IsFloat())
		{
			name = MPT_UFORMAT("Float {} bit")(sampleFormat.GetBitsPerSample());
		} else if(sampleFormat.IsUnsigned())
		{
			name = MPT_UFORMAT("{} Bit uint")(sampleFormat.GetBitsPerSample());
		} else
		{
			name = MPT_UFORMAT("{} Bit")(sampleFormat.GetBitsPerSample());
		}
		uint32 ndx = m_CbnSampleFormat.AddString(name);
		m_CbnSampleFormat.SetItemData(ndx, mpt::to_underlying<SampleFormat::Enum>(sampleFormat));
		if(sampleFormat == m_Settings.sampleFormat)
		{
			n = ndx;
		}
	}
	m_CbnSampleFormat.SetCurSel(n);
	if(theApp.GetSoundDevicesManager()->IsDeviceUnavailable(m_CurrentDeviceInfo.GetIdentifier()))
	{
		m_CbnSampleFormat.EnableWindow(false);
	}
}


void COptionsSoundcard::UpdateDither()
{
	{
		m_CbnDither.EnableWindow(true);
	}
	m_CbnDither.ResetContent();
	SampleFormat sampleFormat = SampleFormat::FromInt(static_cast<int>(m_CbnSampleFormat.GetItemData(m_CbnSampleFormat.GetCurSel())));
	if(sampleFormat.IsInt() && sampleFormat.GetBitsPerSample() < 32)
	{
		m_CbnDither.EnableWindow(true);
		for(std::size_t i = 0; i < DithersOpenMPT::GetNumDithers(); ++i)
		{
			m_CbnDither.AddString(mpt::ToUnicode(DithersOpenMPT::GetModeName(i) + UL_(" dither")));
		}
	} else if(m_CurrentDeviceCaps.HasInternalDither)
	{
		m_CbnDither.EnableWindow(true);
		m_CbnDither.AddString(mpt::ToUnicode(DithersOpenMPT::GetModeName(DithersOpenMPT::GetNoDither()) + UL_(" dither")));
		m_CbnDither.AddString(mpt::ToUnicode(DithersOpenMPT::GetModeName(DithersOpenMPT::GetDefaultDither()) + UL_(" dither")));
	} else
	{
		m_CbnDither.EnableWindow(false);
		for(std::size_t i = 0; i < DithersOpenMPT::GetNumDithers(); ++i)
		{
			m_CbnDither.AddString(mpt::ToUnicode(DithersOpenMPT::GetModeName(DithersOpenMPT::GetNoDither()) + UL_(" dither")));
		}
	}
	if(m_Settings.DitherType < 0 || m_Settings.DitherType >= m_CbnDither.GetCount())
	{
		m_CbnDither.SetCurSel(1);
	} else
	{
		m_CbnDither.SetCurSel(m_Settings.DitherType);
	}
	if(theApp.GetSoundDevicesManager()->IsDeviceUnavailable(m_CurrentDeviceInfo.GetIdentifier()))
	{
		m_CbnDither.EnableWindow(false);
	}
}


void COptionsSoundcard::UpdateChannelMapping()
{
	{
		GetDlgItem(IDC_STATIC_CHANNELMAPPING)->EnableWindow(true);
		GetDlgItem(IDC_STATIC_CHANNEL_FRONT)->EnableWindow(true);
		GetDlgItem(IDC_STATIC_CHANNEL_REAR)->EnableWindow(true);
		for(int mch = 0; mch < NUM_CHANNELCOMBOBOXES; mch++)
		{
			ComboBox *combo = &m_CbnChannelMapping[mch];
			combo->EnableWindow(true);
		}
	}
	int usedChannels = static_cast<int>(m_CbnChannels.GetItemData(m_CbnChannels.GetCurSel()));
	if(m_Settings.Channels.GetNumHostChannels() != static_cast<uint32>(usedChannels))
	{
		// If the channel mapping is not valid for the selected number of channels, reset it to default identity mapping.
		m_Settings.Channels = SoundDevice::ChannelMapping(usedChannels);
	}
	GetDlgItem(IDC_STATIC_CHANNELMAPPING)->EnableWindow(m_CurrentDeviceCaps.CanChannelMapping ? true : false);
	if(m_CurrentDeviceCaps.CanChannelMapping && usedChannels > 2)
	{
		GetDlgItem(IDC_STATIC_CHANNEL_FRONT)->EnableWindow(true);
		GetDlgItem(IDC_STATIC_CHANNEL_REAR)->EnableWindow(true);
	} else
	{
		GetDlgItem(IDC_STATIC_CHANNEL_FRONT)->EnableWindow(false);
		GetDlgItem(IDC_STATIC_CHANNEL_REAR)->EnableWindow(false);
	}
	for(int mch = 0; mch < NUM_CHANNELCOMBOBOXES; mch++)	// Host channels
	{
		ComboBox *combo = &m_CbnChannelMapping[mch];
		combo->EnableWindow((m_CurrentDeviceCaps.CanChannelMapping && mch < usedChannels) ? true : false);
		combo->ResetContent();
		if(m_CurrentDeviceCaps.CanChannelMapping)
		{
			combo->SetItemData(combo->AddString(UL_("Unassigned")), (uintptr_t)-1);
			combo->SetCurSel(0);
			if(mch < usedChannels)
			{
				for(size_t dch = 0; dch < m_CurrentDeviceDynamicCaps.channelNames.size(); dch++)	// Device channels
				{
					const int pos = combo->AddString(mpt::ToUnicode(m_CurrentDeviceDynamicCaps.channelNames[dch]));
					combo->SetItemData(pos, (uintptr_t)dch);
					if(static_cast<int32>(dch) == m_Settings.Channels.ToDevice(mch))
					{
						combo->SetCurSel(pos);
					}
				}
			}
		}
	}
	if(theApp.GetSoundDevicesManager()->IsDeviceUnavailable(m_CurrentDeviceInfo.GetIdentifier()))
	{
		GetDlgItem(IDC_STATIC_CHANNELMAPPING)->EnableWindow(false);
		GetDlgItem(IDC_STATIC_CHANNEL_FRONT)->EnableWindow(false);
		GetDlgItem(IDC_STATIC_CHANNEL_REAR)->EnableWindow(false);
		for(int mch = 0; mch < NUM_CHANNELCOMBOBOXES; mch++)
		{
			ComboBox *combo = &m_CbnChannelMapping[mch];
			combo->EnableWindow(false);
		}
	}
}


void COptionsSoundcard::OnDeviceChanged()
{
	int n = m_CbnDevice.GetCurSel();
	if(n >= 0)
	{
		SetDevice(theApp.GetSoundDevicesManager()->FindDeviceInfo(static_cast<SoundDevice::Manager::GlobalID>(m_CbnDevice.GetItemData(n))).GetIdentifier());
		UpdateDevice();
		OnSettingsChanged();
	}
}


void COptionsSoundcard::OnExclusiveModeChanged()
{
	UpdateSampleRates();
	UpdateSampleFormat();
	UpdateDither();
	OnSettingsChanged();
}


void COptionsSoundcard::OnChannelsChanged()
{
	UpdateChannelMapping();
	OnSettingsChanged();
}


void COptionsSoundcard::OnSoundCardDriverPanel()
{
	theApp.GetSoundDevicesManager()->OpenDriverSettings(
		theApp.GetSoundDevicesManager()->FindDeviceInfo(static_cast<SoundDevice::Manager::GlobalID>(m_CbnDevice.GetItemData(m_CbnDevice.GetCurSel()))).GetIdentifier(),
		CMainFrame::GetMainFrame(),
		CMainFrame::GetMainFrame()->gpSoundDevice
		);
}


void COptionsSoundcard::OnChannelChanged(int channel)
{
	ComboBox *combo = &m_CbnChannelMapping[channel];
	const intptr_t newChn = combo->GetItemData(combo->GetCurSel());
	if(newChn == -1)
	{
		return;
	}
	// Ensure that no channel is used twice
	for(int mch = 0; mch < NUM_CHANNELCOMBOBOXES; mch++)	// Host channels
	{
		if(mch != channel)
		{
			combo = &m_CbnChannelMapping[mch];
			if((int)combo->GetItemData(combo->GetCurSel()) == newChn)
			{
				// find an unused channel
				bool found = false;
				int deviceChannel = 0;
				for(; deviceChannel < static_cast<int>(m_CurrentDeviceDynamicCaps.channelNames.size()); ++deviceChannel)
				{
					bool used = false;
					for(int hostChannel = 0; hostChannel < NUM_CHANNELCOMBOBOXES; ++hostChannel)
					{
						if(static_cast<int>(m_CbnChannelMapping[hostChannel].GetItemData(m_CbnChannelMapping[hostChannel].GetCurSel())) == deviceChannel)
						{
							used = true;
							break;
						}
					}
					if(!used)
					{
						found = true;
						break;
					}
				}
				if(found)
				{
					combo->SetCurSel(deviceChannel+1);
				} else
				{
					combo->SetCurSel(0);
				}
				break;
			}
		}
	}
	OnSettingsChanged();
}


// Fill the dropdown box with a list of valid sample rates, depending on the selected sound device.
void COptionsSoundcard::UpdateSampleRates()
{
	{
		GetDlgItem(IDC_STATIC_FORMAT)->EnableWindow(true);
		m_CbnMixingFreq.EnableWindow(true);
	}

	m_CbnMixingFreq.ResetContent();

	std::vector<uint32> samplerates;

	if(IsDlgButtonChecked(IDC_CHECK4))
	{
		samplerates = m_CurrentDeviceDynamicCaps.supportedExclusiveSampleRates;
	} else
	{
		samplerates = m_CurrentDeviceDynamicCaps.supportedSampleRates;
	}

	if(samplerates.empty())
	{
		// We have no valid list of supported playback rates! Assume all rates supported by OpenMPT are possible...
		samplerates = TrackerSettings::Instance().GetSampleRates();
	}

	int n = 0;
	for(size_t i = 0; i < samplerates.size(); i++)
	{
		int pos = m_CbnMixingFreq.AddString(MPT_UFORMAT("{} Hz")(samplerates[i]));
		m_CbnMixingFreq.SetItemData(pos, samplerates[i]);
		if(m_Settings.Samplerate == samplerates[i])
		{
			n = pos;
		}
	}
	m_CbnMixingFreq.SetCurSel(n);
	if(theApp.GetSoundDevicesManager()->IsDeviceUnavailable(m_CurrentDeviceInfo.GetIdentifier()))
	{
		GetDlgItem(IDC_STATIC_FORMAT)->EnableWindow(false);
		m_CbnMixingFreq.EnableWindow(false);
	}
}


void COptionsSoundcard::UpdateControls()
{
	{
		m_BtnDriverPanel.EnableWindow(true);
		GetDlgItem(IDC_CHECK4)->EnableWindow(true);
		GetDlgItem(IDC_CHECK5)->EnableWindow(true);
		GetDlgItem(IDC_CHECK9)->EnableWindow(true);
		GetDlgItem(IDC_STATIC_UPDATEINTERVAL)->EnableWindow(true);
		GetDlgItem(IDC_COMBO_UPDATEINTERVAL)->EnableWindow(true);
	}
	if(!m_CurrentDeviceCaps.CanKeepDeviceRunning)
	{
		m_Settings.KeepDeviceRunning = false;
	}
	m_BtnDriverPanel.EnableWindow(m_CurrentDeviceCaps.CanDriverPanel ? true : false);
	GetDlgItem(IDC_CHECK4)->EnableWindow(m_CurrentDeviceCaps.CanExclusiveMode ? true : false);
	GetDlgItem(IDC_CHECK5)->EnableWindow(m_CurrentDeviceCaps.CanBoostThreadPriority ? true : false);
	GetDlgItem(IDC_CHECK9)->EnableWindow(m_CurrentDeviceCaps.CanUseHardwareTiming ? true : false);
	GetDlgItem(IDC_STATIC_UPDATEINTERVAL)->EnableWindow(m_CurrentDeviceCaps.CanUpdateInterval ? true : false);
	GetDlgItem(IDC_COMBO_UPDATEINTERVAL)->EnableWindow(m_CurrentDeviceCaps.CanUpdateInterval ? true : false);
	GetDlgItem(IDC_CHECK4)->SetWindowText(mpt::ToUnicode(m_CurrentDeviceCaps.ExclusiveModeDescription));
	CheckDlgButton(IDC_CHECK4, m_CurrentDeviceCaps.CanExclusiveMode && m_Settings.ExclusiveMode ? ui::CheckOn : ui::CheckOff);
	CheckDlgButton(IDC_CHECK5, m_CurrentDeviceCaps.CanBoostThreadPriority && m_Settings.BoostThreadPriority ? ui::CheckOn : ui::CheckOff);
	CheckDlgButton(IDC_CHECK9, m_CurrentDeviceCaps.CanUseHardwareTiming && m_Settings.UseHardwareTiming ? ui::CheckOn : ui::CheckOff);
	if(theApp.GetSoundDevicesManager()->IsDeviceUnavailable(m_CurrentDeviceInfo.GetIdentifier()))
	{
		m_BtnDriverPanel.EnableWindow(false);
		GetDlgItem(IDC_CHECK4)->EnableWindow(false);
		GetDlgItem(IDC_CHECK5)->EnableWindow(false);
		GetDlgItem(IDC_CHECK9)->EnableWindow(false);
		GetDlgItem(IDC_STATIC_UPDATEINTERVAL)->EnableWindow(false);
		GetDlgItem(IDC_COMBO_UPDATEINTERVAL)->EnableWindow(false);
	}
}


bool COptionsSoundcard::OnSetActive()
{
	CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_SOUNDCARD;
	return PropertyPage::OnSetActive();
}


void COptionsSoundcard::OnOK()
{
	if(!theApp.GetSoundDevicesManager()->IsDeviceUnavailable(m_CurrentDeviceInfo.GetIdentifier()))
	{

	// General
	{
		TrackerSettings::Instance().m_SoundSettingsOpenDeviceAtStartup = IsDlgButtonChecked(IDC_CHECK7) != ui::CheckOff;
	}
	m_Settings.ExclusiveMode = IsDlgButtonChecked(IDC_CHECK4) != ui::CheckOff;
	m_Settings.BoostThreadPriority = IsDlgButtonChecked(IDC_CHECK5) != ui::CheckOff;
	m_Settings.UseHardwareTiming = IsDlgButtonChecked(IDC_CHECK9) != ui::CheckOff;
	// Mixing Freq
	{
		m_Settings.Samplerate = static_cast<uint32>(m_CbnMixingFreq.GetItemData(m_CbnMixingFreq.GetCurSel()));
	}
	// Channels
	{
		uintptr_t n = m_CbnChannels.GetItemData(m_CbnChannels.GetCurSel());
		m_Settings.Channels = static_cast<int>(n);
		if((m_Settings.Channels != 1) && (m_Settings.Channels != 4))
		{
			m_Settings.Channels = 2;
		}
	}
	// SampleFormat
	{
		uintptr_t n = m_CbnSampleFormat.GetItemData(m_CbnSampleFormat.GetCurSel());
		m_Settings.sampleFormat = SampleFormat::FromInt(static_cast<int>(n));
	}
	// Dither
	{
		m_Settings.DitherType = m_CbnDither.GetCurSel();
	}
	// Latency
	{
		mpt::ustring s;
		m_CbnLatencyMS.GetWindowText(s);
		m_Settings.Latency = ParseTime(s);
		//Check given value.
		if(m_Settings.Latency == 0.0) m_Settings.Latency = m_CurrentDeviceCaps.DefaultSettings.Latency;
		m_Settings.Latency = Clamp(m_Settings.Latency, m_CurrentDeviceCaps.LatencyMin, m_CurrentDeviceCaps.LatencyMax);
		m_CbnLatencyMS.SetWindowText(PrintTime(m_Settings.Latency));
	}
	// Update Interval
	{
		mpt::ustring s;
		m_CbnUpdateIntervalMS.GetWindowText(s);
		m_Settings.UpdateInterval = ParseTime(s);
		//Check given value.
		if(m_Settings.UpdateInterval == 0.0) m_Settings.UpdateInterval = m_CurrentDeviceCaps.DefaultSettings.UpdateInterval;
		m_Settings.UpdateInterval = Clamp(m_Settings.UpdateInterval, m_CurrentDeviceCaps.UpdateIntervalMin, m_CurrentDeviceCaps.UpdateIntervalMax);
		m_CbnUpdateIntervalMS.SetWindowText(PrintTime(m_Settings.UpdateInterval));
	}
	// Channel Mapping
	{
		if(m_CurrentDeviceCaps.CanChannelMapping)
		{
			int numChannels = std::min(static_cast<int>(m_Settings.Channels), static_cast<int>(NUM_CHANNELCOMBOBOXES));
			std::vector<int32> channels(numChannels);
			for(int mch = 0; mch < numChannels; mch++)	// Host channels
			{
				ComboBox *combo = &m_CbnChannelMapping[mch];
				channels[mch] = static_cast<int32>(combo->GetItemData(combo->GetCurSel()));
			}
			m_Settings.Channels = channels;
		}
	}
	// Recording
	{
		if(TrackerSettings::Instance().m_SoundShowRecordingSettings && m_CurrentDeviceCaps.CanInput && ((m_CurrentDeviceCaps.HasNamedInputSources && m_CurrentDeviceDynamicCaps.inputSourceNames.size() > 0) || !m_CurrentDeviceCaps.HasNamedInputSources))
		{
			uintptr_t n = m_CbnRecordingChannels.GetItemData(m_CbnRecordingChannels.GetCurSel());
			m_Settings.InputChannels = static_cast<uint8>(n);
			if((m_Settings.InputChannels != 1) && (m_Settings.InputChannels != 2) && (m_Settings.InputChannels != 4))
			{
				m_Settings.InputChannels = 0;
			}
			if(m_CurrentDeviceCaps.HasNamedInputSources)
			{
				uintptr_t sourceID = m_CbnRecordingSource.GetItemData(m_CbnRecordingSource.GetCurSel());
				m_Settings.InputSourceID = static_cast<uint32>(sourceID);
			} else
			{
				m_Settings.InputSourceID = 0;
			}
		} else
		{
			m_Settings.InputChannels = 0;
			m_Settings.InputSourceID = 0;
		}
	}
	CMainFrame::GetMainFrame()->SetupSoundCard(m_Settings, m_CurrentDeviceInfo.GetIdentifier(), (SoundDeviceStopMode)m_CbnStoppedMode.GetCurSel());
	SetDevice(m_CurrentDeviceInfo.GetIdentifier(), true); // Poll changed ASIO sample format and channel names
	UpdateDevice();
	UpdateStatistics();

	} else
	{

		Reporting::Error("Sound card currently not available.");

	}

	PropertyPage::OnOK();
}


void COptionsSoundcard::UpdateStatistics()
{
	if (m_EditStatistics.GetParent() == nullptr) return;
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if(pMainFrm->gpSoundDevice && pMainFrm->IsPlaying())
	{
		const SoundDevice::BufferAttributes bufferAttributes = pMainFrm->gpSoundDevice->GetEffectiveBufferAttributes();
		const SoundDevice::Statistics stats = pMainFrm->gpSoundDevice->GetStatistics();
		const uint32 samplerate = pMainFrm->gpSoundDevice->GetSettings().Samplerate;
		mpt::ustring s;
		if(bufferAttributes.NumBuffers > 2)
		{
			s += MPT_UFORMAT("Buffer: {}% ({}/{})\r\n")((bufferAttributes.Latency > 0.0) ? mpt::saturate_round<int64>(stats.InstantaneousLatency / bufferAttributes.Latency * 100.0) : 0, (stats.LastUpdateInterval > 0.0) ? mpt::saturate_round<int64>(bufferAttributes.Latency / stats.LastUpdateInterval) : 0, bufferAttributes.NumBuffers);
		} else
		{
			s += MPT_UFORMAT("Buffer: {}%\r\n")((bufferAttributes.Latency > 0.0) ? mpt::saturate_round<int64>(stats.InstantaneousLatency / bufferAttributes.Latency * 100.0) : 0);
		}
		s += MPT_UFORMAT("Latency: {} ms (current: {} ms, {} frames)\r\n")(mpt::ufmt::fix(bufferAttributes.Latency * 1000.0, 1), mpt::ufmt::fix(stats.InstantaneousLatency * 1000.0, 1), mpt::saturate_round<int64>(stats.InstantaneousLatency * samplerate));
		s += MPT_UFORMAT("Period: {} ms (current: {} ms, {} frames)\r\n")(mpt::ufmt::fix(bufferAttributes.UpdateInterval * 1000.0, 1), mpt::ufmt::fix(stats.LastUpdateInterval * 1000.0, 1), mpt::saturate_round<int64>(stats.LastUpdateInterval * samplerate));
		s += stats.text;
		m_EditStatistics.SetWindowText(mpt::ToUnicode(s));
	}	else
	{
		if(theApp.GetSoundDevicesManager()->IsDeviceUnavailable(m_CurrentDeviceInfo.GetIdentifier()))
		{
			m_EditStatistics.SetWindowText(UL_("Device currently unavailable."));
		} else
		{
			m_EditStatistics.SetWindowText(UL_(""));
		}
	}
}


//////////////////
// COptionsMixer

UI_MESSAGE_MAP_BEGIN(COptionsMixer, PropertyPage)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO_FILTER,     &COptionsMixer::OnSettingsChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO_AMIGA_TYPE, &COptionsMixer::OnSettingsChanged)
	UI_NOTIFY(ui::EditUpdate, IDC_RAMPING_IN,           &COptionsMixer::OnRampingChanged)
	UI_NOTIFY(ui::EditUpdate, IDC_RAMPING_OUT,          &COptionsMixer::OnRampingChanged)
	UI_COMMAND(IDC_CHECK_SOFTPAN,          &COptionsMixer::OnSettingsChanged)
	UI_COMMAND(IDC_CHECK1,                 &COptionsMixer::OnAmigaChanged)
	UI_COMMAND(IDC_BUTTON1,                &COptionsMixer::OnDefaultRampSettings)
UI_MESSAGE_MAP_END()


void COptionsMixer::DoDataExchange(DataExchange* pDX)
{
	PropertyPage::DoDataExchange(pDX);
	pDX->BindControl(IDC_COMBO_FILTER, m_CbnResampling);
	pDX->BindControl(IDC_COMBO_AMIGA_TYPE, m_CbnAmigaType);
	pDX->BindControl(IDC_RAMPING_IN, m_CEditRampUp);
	pDX->BindControl(IDC_RAMPING_OUT, m_CEditRampDown);
	pDX->BindControl(IDC_EDIT_VOLRAMP_SAMPLES_UP, m_CInfoRampUp);
	pDX->BindControl(IDC_EDIT_VOLRAMP_SAMPLES_DOWN, m_CInfoRampDown);
	pDX->BindControl(IDC_SLIDER_STEREOSEP, m_SliderStereoSep);
	pDX->BindControl(IDC_SLIDER_PREAMP, m_SliderPreAmp);
}


COptionsMixer::COptionsMixer()
	: PropertyPage{IDD_OPTIONS_MIXER}
{
	m_CEditRampUp.SetAccessibleSuffix(UL_("microseconds up"));
	m_CEditRampDown.SetAccessibleSuffix(UL_("microseconds down"));
}



bool COptionsMixer::OnInitDialog()
{
	PropertyPage::OnInitDialog();

	// Resampling type
	{
		const auto resamplingModes = Resampling::AllModes();
		for(auto mode : resamplingModes)
		{
			int index = m_CbnResampling.AddString(CTrackApp::GetResamplingModeName(mode, 2, true));
			m_CbnResampling.SetItemData(index, mode);
			if(TrackerSettings::Instance().ResamplerMode == mode)
			{
				m_CbnResampling.SetCurSel(index);
			}
		}
	}

	// Amiga Resampler
	const bool enableAmigaResampler = TrackerSettings::Instance().ResamplerEmulateAmiga != Resampling::AmigaFilter::Off;
	CheckDlgButton(IDC_CHECK1, enableAmigaResampler ? ui::CheckOn : ui::CheckOff);
	m_CbnAmigaType.EnableWindow(enableAmigaResampler ? true : false);
	static constexpr std::pair<const mpt::uchar *, Resampling::AmigaFilter> Filters[] =
	{
		{UL_("A500 Filter"),  Resampling::AmigaFilter::A500},
		{UL_("A1200 Filter"), Resampling::AmigaFilter::A1200},
		{UL_("Unfiltered"),   Resampling::AmigaFilter::Unfiltered},
	};
	int sel = 0;
	for(const auto & [name, filter] : Filters)
	{
		const int item = m_CbnAmigaType.AddString(name);
		m_CbnAmigaType.SetItemData(item, static_cast<uintptr_t>(filter));
		if(filter == TrackerSettings::Instance().ResamplerEmulateAmiga)
			sel = item;
	}
	m_CbnAmigaType.SetCurSel(sel);

	// volume ramping
	{
		m_CEditRampUp.SetWindowText(mpt::ToUnicode(mpt::ufmt::val(TrackerSettings::Instance().GetMixerSettings().GetVolumeRampUpMicroseconds())));
		m_CEditRampDown.SetWindowText(mpt::ToUnicode(mpt::ufmt::val(TrackerSettings::Instance().GetMixerSettings().GetVolumeRampDownMicroseconds())));
		m_CEditRampUp.SetRange32(0, int32_max);
		m_CEditRampDown.SetRange32(0, int32_max);
		UpdateRamping();
	}

	// Stereo Separation
	{
		m_SliderStereoSep.SetRange(0, 32);
		m_SliderStereoSep.SetPos(16);
		for (int n = 0; n <= 32; n++)
		{
			if ((int)TrackerSettings::Instance().MixerStereoSeparation <= 8 * n)
			{
				m_SliderStereoSep.SetPos(n);
				break;
			}
		}
		UpdateStereoSep();
	}

	// soft pan
	{
		CheckDlgButton(IDC_CHECK_SOFTPAN, (TrackerSettings::Instance().MixerFlags & SNDMIX_SOFTPANNING) ? ui::CheckOn : ui::CheckOff);
	}

	// Pre-Amplification
	{
		m_SliderPreAmp.SetTicFreq(5);
		m_SliderPreAmp.SetRange(0, 40);
		int n = (TrackerSettings::Instance().MixerPreAmp - 64) / 8;
		if ((n < 0) || (n > 40)) n = 16;
		m_SliderPreAmp.SetPos(n);
	}

	m_initialized = true;

	return true;
}


bool COptionsMixer::OnSetActive()
{
	CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_MIXER;
	return PropertyPage::OnSetActive();
}


void COptionsMixer::OnAmigaChanged()
{
	const bool enableAmigaResampler = IsDlgButtonChecked(IDC_CHECK1) != ui::CheckOff;
	m_CbnAmigaType.EnableWindow(enableAmigaResampler ? true : false);
	OnSettingsChanged();
}


void COptionsMixer::OnRampingChanged()
{
	if(!m_initialized)
		return;
	UpdateRamping();
	OnSettingsChanged();
}


void COptionsMixer::OnDefaultRampSettings()
{
	m_CEditRampUp.SetWindowText(mpt::ToUnicode(mpt::ufmt::val(MixerSettings().GetVolumeRampUpMicroseconds())));
	m_CEditRampDown.SetWindowText(mpt::ToUnicode(mpt::ufmt::val(MixerSettings().GetVolumeRampDownMicroseconds())));
	OnRampingChanged();
}


void COptionsMixer::OnHScroll(uint32 n, uint32 pos, Wnd *p)
{
	PropertyPage::OnHScroll(n, pos, p);
	if(p == (ScrollBar *)&m_SliderStereoSep)
	{
		UpdateStereoSep();
		OnSettingsChanged();
	}
}


void COptionsMixer::UpdateRamping()
{
	MixerSettings settings = TrackerSettings::Instance().GetMixerSettings();
	mpt::ustring s;
	m_CEditRampUp.GetWindowText(s);
	settings.SetVolumeRampUpMicroseconds(mpt::parse<int32>(s));
	m_CEditRampDown.GetWindowText(s);
	settings.SetVolumeRampDownMicroseconds(mpt::parse<int32>(s));
	s = ui::Format(UL_("%i samples at %i Hz"), (int)settings.GetVolumeRampUpSamples(), (int)settings.gdwMixingFreq);
	m_CInfoRampUp.SetWindowText(s);
	s = ui::Format(UL_("%i samples at %i Hz"), (int)settings.GetVolumeRampDownSamples(), (int)settings.gdwMixingFreq);
	m_CInfoRampDown.SetWindowText(s);
}


void COptionsMixer::UpdateStereoSep()
{
	mpt::ustring s;
	s = ui::Format(UL_("%d%%"), ((8 * m_SliderStereoSep.GetPos()) * 100) / 128);
	SetDlgItemText(IDC_TEXT_STEREOSEP, s);
}


void COptionsMixer::OnOK()
{
	// resampler mode
	{
		TrackerSettings::Instance().ResamplerMode = static_cast<ResamplingMode>(m_CbnResampling.GetItemData(m_CbnResampling.GetCurSel()));
	}

	// Amiga Resampler
	if(IsDlgButtonChecked(IDC_CHECK1) == ui::CheckOff)
		TrackerSettings::Instance().ResamplerEmulateAmiga = Resampling::AmigaFilter::Off;
	else
		TrackerSettings::Instance().ResamplerEmulateAmiga = static_cast<Resampling::AmigaFilter>(m_CbnAmigaType.GetItemData(m_CbnAmigaType.GetCurSel()));

	// volume ramping
	{
		MixerSettings settings = TrackerSettings::Instance().GetMixerSettings();
		mpt::ustring s;
		m_CEditRampUp.GetWindowText(s);
		settings.SetVolumeRampUpMicroseconds(mpt::parse<int>(s));
		m_CEditRampDown.GetWindowText(s);
		settings.SetVolumeRampDownMicroseconds(mpt::parse<int>(s));
		TrackerSettings::Instance().SetMixerSettings(settings);
	}

	// stereo sep
	{
		TrackerSettings::Instance().MixerStereoSeparation = 8 * m_SliderStereoSep.GetPos();
	}

	// soft pan
	{
		if(IsDlgButtonChecked(IDC_CHECK_SOFTPAN))
		{
			TrackerSettings::Instance().MixerFlags = TrackerSettings::Instance().MixerFlags | SNDMIX_SOFTPANNING;
		} else
		{
			TrackerSettings::Instance().MixerFlags = TrackerSettings::Instance().MixerFlags & ~SNDMIX_SOFTPANNING;
		}
	}

	// pre amp
	{
		int n = m_SliderPreAmp.GetPos();
		if ((n >= 0) && (n <= 40)) // approximately +/- 10dB
		{
			TrackerSettings::Instance().MixerPreAmp = 64 + (n * 8);
		}
	}

	CMainFrame::GetMainFrame()->SetupPlayer();
	CMainFrame::GetMainFrame()->PostMessage(MSG_MOD_INVALIDATEPATTERNS, HINT_MPTOPTIONS);

	PropertyPage::OnOK();
}


////////////////////////////////////////////////////////////////////////////////
//
// CEQSavePresetDlg
//


class CEQSavePresetDlg : public DialogBase
{
protected:
	EQPreset &m_EQ;

public:
	CEQSavePresetDlg(EQPreset &eq, Wnd *parent = nullptr) : DialogBase(IDD_SAVEPRESET, parent), m_EQ(eq) { }
	bool OnInitDialog();
	void OnOK();
};


bool CEQSavePresetDlg::OnInitDialog()
{
	ComboBox *pCombo = (ComboBox *)GetDlgItem(IDC_COMBO1);
	if (pCombo)
	{
		int ndx = 0;
		for (uint32 i=0; i<4; i++)
		{
			int n = pCombo->AddString(mpt::ToUnicode(mpt::Charset::Locale, TrackerSettings::Instance().m_EqUserPresets[i].szName));
			pCombo->SetItemData( n, i);
			if (mpt::CompareNoCaseAscii(TrackerSettings::Instance().m_EqUserPresets[i].szName, m_EQ.szName) == 0) ndx = n;
		}
		pCombo->SetCurSel(ndx);
	}
	SetDlgItemText(IDC_EDIT1, mpt::ToUnicode(mpt::Charset::Locale, m_EQ.szName));
	return true;
}


void CEQSavePresetDlg::OnOK()
{
	ComboBox *pCombo = (ComboBox *)GetDlgItem(IDC_COMBO1);
	if (pCombo)
	{
		int n = pCombo->GetCurSel();
		if ((n < 0) || (n >= 4)) n = 0;
		mpt::ustring s;
		GetDlgItemText(IDC_EDIT1, s);
		mpt::String::WriteAutoBuf(m_EQ.szName) = mpt::ToCharset(mpt::Charset::Locale, s);
		TrackerSettings::Instance().m_EqUserPresets[n] = m_EQ;
	}
	DialogBase::OnOK();
}


void CEQSlider::Init(uint32 nID, uint32 n, Wnd *parent)
{
	m_nSliderNo = n;
	m_pParent = parent;
	SubclassDlgItem(nID, parent);
}


bool CEQSlider::PreTranslateMessage(int event)
{
	if(event == FL_RELEASE && Fl::event_button() == FL_RIGHT_MOUSE && m_pParent)
	{
		m_x = Fl::event_x() - GetWidget()->x();
		m_y = Fl::event_y() - GetWidget()->y();
		m_pParent->PostCommand(ID_EQSLIDER_BASE + m_nSliderNo);
		return true;
	}
	return VSlider::PreTranslateMessage(event);
}



//////////////////////////////////////////////////////////
// COptionsPlayer - DSP / EQ settings


#define EQ_MAX_FREQS	5

const uint32 gEqBandFreqs[MAX_EQ_BANDS][EQ_MAX_FREQS] =
{
	{ 100, 125, 150, 200, 250 },
	{ 300, 350, 400, 450, 500 },
	{ 600, 700, 800, 900, 1000 },
	{ 1250, 1500, 1750, 2000, 2500 },
	{ 3000, 3500, 4000, 4500, 5000 },
	{ 6000, 7000, 8000, 9000, 10000 },
};

UI_MESSAGE_MAP_BEGIN(COptionsPlayer, PropertyPage)
	// EQ
	UI_COMMAND(IDC_CHECK3,  &COptionsPlayer::OnSettingsChanged)
	UI_COMMAND(IDC_BUTTON1, &COptionsPlayer::OnEqUser1)
	UI_COMMAND(IDC_BUTTON2, &COptionsPlayer::OnEqUser2)
	UI_COMMAND(IDC_BUTTON3, &COptionsPlayer::OnEqUser3)
	UI_COMMAND(IDC_BUTTON4, &COptionsPlayer::OnEqUser4)
	UI_COMMAND(IDC_BUTTON5, &COptionsPlayer::OnSavePreset)
	UI_COMMAND_RANGE(ID_EQSLIDER_BASE, ID_EQSLIDER_BASE + MAX_EQ_BANDS, &COptionsPlayer::OnSliderMenu)
	UI_COMMAND_RANGE(ID_EQMENU_BASE, ID_EQMENU_BASE + EQ_MAX_FREQS,     &COptionsPlayer::OnSliderFreq)

	// DSP
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO2, &COptionsPlayer::OnSettingsChanged)
	UI_COMMAND(IDC_CHECK1,       &COptionsPlayer::OnSettingsChanged)
	UI_COMMAND(IDC_CHECK2,       &COptionsPlayer::OnSettingsChanged)
	UI_COMMAND(IDC_CHECK4,       &COptionsPlayer::OnSettingsChanged)
	UI_COMMAND(IDC_CHECK5,       &COptionsPlayer::OnSettingsChanged)
	UI_COMMAND(IDC_CHECK6,       &COptionsPlayer::OnSettingsChanged)
	UI_COMMAND(IDC_CHECK7,       &COptionsPlayer::OnSettingsChanged)
UI_MESSAGE_MAP_END()


void COptionsPlayer::DoDataExchange(DataExchange* pDX)
{
	PropertyPage::DoDataExchange(pDX);
	pDX->BindControl(IDC_COMBO2, m_CbnReverbPreset);
	pDX->BindControl(IDC_SLIDER1, m_SbXBassDepth);
	pDX->BindControl(IDC_SLIDER2, m_SbXBassRange);
	pDX->BindControl(IDC_SLIDER3, m_SbReverbDepth);
	pDX->BindControl(IDC_SLIDER5, m_SbSurroundDepth);
	pDX->BindControl(IDC_SLIDER6, m_SbSurroundDelay);
	pDX->BindControl(IDC_SLIDER4, m_SbBitCrushBits);
}


COptionsPlayer::COptionsPlayer() : PropertyPage{IDD_OPTIONS_PLAYER}
	, m_EQPreset(TrackerSettings::Instance().m_EqSettings)
{
}


bool COptionsPlayer::OnInitDialog()
{
	PropertyPage::OnInitDialog();

	uint32 dwQuality = TrackerSettings::Instance().MixerDSPMask;

	for (uint32 i = 0; i < MAX_EQ_BANDS; i++)
	{
		m_Sliders[i].Init(IDC_SLIDER7 + i, i, this);
		m_Sliders[i].SetRange(0, 32);
		m_Sliders[i].SetTicFreq(4);
	}

	UpdateDialog();

	if (dwQuality & SNDDSP_EQ) CheckDlgButton(IDC_CHECK3, ui::CheckOn);

	// Effects
	if (dwQuality & SNDDSP_MEGABASS) CheckDlgButton(IDC_CHECK1, ui::CheckOn);
	if (dwQuality & SNDDSP_AGC) CheckDlgButton(IDC_CHECK2, ui::CheckOn);
	if (dwQuality & SNDDSP_SURROUND) CheckDlgButton(IDC_CHECK4, ui::CheckOn);
	if (dwQuality & SNDDSP_BITCRUSH) CheckDlgButton(IDC_CHECK5, ui::CheckOn);

	m_SbBitCrushBits.SetRange(1, 24);
	m_SbBitCrushBits.SetPos(TrackerSettings::Instance().m_BitCrushSettings.m_Bits);

	// Bass Expansion
	m_SbXBassDepth.SetRange(0,4);
	m_SbXBassDepth.SetPos(8-TrackerSettings::Instance().m_MegaBassSettings.m_nXBassDepth);
	m_SbXBassRange.SetRange(0,4);
	m_SbXBassRange.SetPos(4 - (TrackerSettings::Instance().m_MegaBassSettings.m_nXBassRange - 1) / 5);

#ifndef NO_REVERB
	// Reverb
	m_SbReverbDepth.SetRange(1, 16);
	m_SbReverbDepth.SetPos(TrackerSettings::Instance().m_ReverbSettings.m_nReverbDepth);
	uint32 nSel = 0;
	for (uint32 iRvb=0; iRvb<NUM_REVERBTYPES; iRvb++)
	{
		mpt::ustring pszName = mpt::ToUnicode(GetReverbPresetName(iRvb));
		if(!pszName.empty())
		{
			uint32 n = m_CbnReverbPreset.AddString(pszName);
			m_CbnReverbPreset.SetItemData(n, iRvb);
			if (iRvb == TrackerSettings::Instance().m_ReverbSettings.m_nReverbType) nSel = n;
		}
	}
	m_CbnReverbPreset.SetCurSel(nSel);
	if (dwQuality & SNDDSP_REVERB) CheckDlgButton(IDC_CHECK6, ui::CheckOn);
#else
	GetDlgItem(IDC_CHECK6)->EnableWindow(false);
	m_SbReverbDepth.EnableWindow(false);
	m_CbnReverbPreset.EnableWindow(false);
#endif

	// Surround
	{
		uint32 n = TrackerSettings::Instance().m_SurroundSettings.m_nProLogicDepth;
		if (n < 1) n = 1;
		if (n > 16) n = 16;
		m_SbSurroundDepth.SetRange(1, 16);
		m_SbSurroundDepth.SetPos(n);
		m_SbSurroundDelay.SetRange(0, 8);
		m_SbSurroundDelay.SetPos((TrackerSettings::Instance().m_SurroundSettings.m_nProLogicDelay-5)/5);
	}

	return true;
}


bool COptionsPlayer::OnSetActive()
{
	CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_PLAYER;

	SetDlgItemText(IDC_EQ_WARNING,
		UL_("Note: This EQ is applied to any and all of the modules ")
		UL_("that you load in OpenMPT; its settings are stored globally, ")
		UL_("rather than in each file. This means that you should avoid ")
		UL_("using it as part of your production process, and instead only ")
		UL_("use it to correct deficiencies in your audio hardware."));

	return PropertyPage::OnSetActive();
}


void COptionsPlayer::OnHScroll(uint32 nSBCode, uint32, Wnd *psb)
{
	if (nSBCode == ui::ScrollEndScroll) return;
	if ((psb) && (psb == static_cast<Wnd *>(&m_SbReverbDepth)))
	{
#ifndef NO_REVERB
		uint32 n = m_SbReverbDepth.GetPos();
		if ((n) && (n <= 16)) TrackerSettings::Instance().m_ReverbSettings.m_nReverbDepth = n;
		//if ((n) && (n <= 16)) CSoundFile::m_Reverb.m_Settings.m_nReverbDepth = n;
		CMainFrame::GetMainFrame()->SetupPlayer();
#endif
	} else
	{
		OnSettingsChanged();
	}
}


void COptionsPlayer::OnOK()
{
	uint32 dwQuality = 0;

	if (IsDlgButtonChecked(IDC_CHECK1)) dwQuality |= SNDDSP_MEGABASS;
	if (IsDlgButtonChecked(IDC_CHECK2)) dwQuality |= SNDDSP_AGC;
	if (IsDlgButtonChecked(IDC_CHECK3)) dwQuality |= SNDDSP_EQ;
	if (IsDlgButtonChecked(IDC_CHECK4)) dwQuality |= SNDDSP_SURROUND;
#ifndef NO_REVERB
	if (IsDlgButtonChecked(IDC_CHECK6)) dwQuality |= SNDDSP_REVERB;
#endif
	if (IsDlgButtonChecked(IDC_CHECK5)) dwQuality |= SNDDSP_BITCRUSH;

	{
		TrackerSettings::Instance().m_BitCrushSettings.m_Bits = m_SbBitCrushBits.GetPos();
	}

	// Bass Expansion
	{
		uint32 nXBassDepth = 8-m_SbXBassDepth.GetPos();
		if (nXBassDepth < 4) nXBassDepth = 4;
		if (nXBassDepth > 8) nXBassDepth = 8;
		uint32 nXBassRange = (4-m_SbXBassRange.GetPos()) * 5 + 1;
		if (nXBassRange < 5) nXBassRange = 5;
		if (nXBassRange > 21) nXBassRange = 21;
		TrackerSettings::Instance().m_MegaBassSettings.m_nXBassDepth = nXBassDepth;
		TrackerSettings::Instance().m_MegaBassSettings.m_nXBassRange = nXBassRange;
	}
#ifndef NO_REVERB
	// Reverb
	{
		// Reverb depth is dynamically changed
		uint32 nReverbType = static_cast<uint32>(m_CbnReverbPreset.GetItemData(m_CbnReverbPreset.GetCurSel()));
		if (nReverbType < NUM_REVERBTYPES) TrackerSettings::Instance().m_ReverbSettings.m_nReverbType = nReverbType;
	}
#endif
	// Surround
	{
		uint32 nProLogicDepth = m_SbSurroundDepth.GetPos();
		uint32 nProLogicDelay = 5 + (m_SbSurroundDelay.GetPos() * 5);
		TrackerSettings::Instance().m_SurroundSettings.m_nProLogicDepth = nProLogicDepth;
		TrackerSettings::Instance().m_SurroundSettings.m_nProLogicDelay = nProLogicDelay;
	}

	TrackerSettings::Instance().MixerDSPMask = dwQuality;

	CMainFrame::GetMainFrame()->SetupPlayer();
	PropertyPage::OnOK();
}



void COptionsPlayer::UpdateEQ(bool bReset)
{
	TrackerCriticalSection cs;
	if(CMainFrame::GetMainFrame()->GetSoundFilePlaying())
		CMainFrame::GetMainFrame()->GetSoundFilePlaying()->SetEQGains(m_EQPreset.Gains, m_EQPreset.Freqs, bReset);
}


void COptionsPlayer::OnVScroll(uint32 nSBCode, uint32 nPos, Wnd *pScrollBar)
{
	PropertyPage::OnVScroll(nSBCode, nPos, pScrollBar);
	for (uint32 i=0; i<MAX_EQ_BANDS; i++)
	{
		int n = 32 - m_Sliders[i].GetPos();
		if ((n >= 0) && (n <= 32)) m_EQPreset.Gains[i] = n;
	}
	UpdateEQ(false);
}


void COptionsPlayer::LoadEQPreset(const EQPreset &preset)
{
	m_EQPreset = preset;
	UpdateEQ(true);
	UpdateDialog();
}


void COptionsPlayer::OnSavePreset()
{
	CEQSavePresetDlg dlg(m_EQPreset, this);
	if (dlg.DoModal() == IDOK)
	{
		UpdateDialog();
	}
}


static mpt::ustring f2s(uint32 f)
{
	if (f < 1000)
	{
		return MPT_UFORMAT("{}Hz")(f);
	} else
	{
		uint32 fHi = f / 1000u;
		uint32 fLo = f % 1000u;
		if(fLo)
		{
			return MPT_UFORMAT("{}.{}kHz")(fHi, mpt::ufmt::dec0<1>(fLo/100));
		} else
		{
			return MPT_UFORMAT("{}kHz")(fHi);
		}
	}
}


void COptionsPlayer::UpdateDialog()
{
	for (uint32 i=0; i<MAX_EQ_BANDS; i++)
	{
		int n = 32 - m_EQPreset.Gains[i];
		if (n < 0) n = 0;
		if (n > 32) n = 32;
		if (n != (m_Sliders[i].GetPos() & 0xFFFF)) m_Sliders[i].SetPos(n);
		SetDlgItemText(IDC_TEXT1 + i, f2s(m_EQPreset.Freqs[i]));
	}
	for(unsigned int i = 0; i < std::size(TrackerSettings::Instance().m_EqUserPresets); i++)
	{
		SetDlgItemText(IDC_BUTTON1 + i, mpt::ToUnicode(mpt::Charset::Locale, TrackerSettings::Instance().m_EqUserPresets[i].szName));
	}
}


void COptionsPlayer::OnSliderMenu(uint32 nID)
{
	uint32 n = nID - ID_EQSLIDER_BASE;
	if (n < MAX_EQ_BANDS)
	{
		HMENU hMenu = ui::CreatePopupMenu();
		m_nSliderMenu = n;
		if (!hMenu) return;
		const uint32 *pFreqs = gEqBandFreqs[m_nSliderMenu];
		for (uint32 i = 0; i < EQ_MAX_FREQS; i++)
		{
			uint32 d = ui::MenuItemString;
			if (m_EQPreset.Freqs[m_nSliderMenu] == pFreqs[i]) d |= ui::MenuItemChecked;
			ui::AppendMenu(hMenu, d, ID_EQMENU_BASE+i, f2s(pFreqs[i]));
		}
		Point pt(m_Sliders[m_nSliderMenu].m_x, m_Sliders[m_nSliderMenu].m_y);
		m_Sliders[m_nSliderMenu].ClientToScreen(&pt);
		ui::TrackPopupMenu(hMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, pt.x, pt.y, 0, this, NULL);
		ui::DestroyMenu(hMenu);
	}
}


void COptionsPlayer::OnSliderFreq(uint32 nID)
{
	uint32 n = nID - ID_EQMENU_BASE;
	if ((m_nSliderMenu < MAX_EQ_BANDS) && (n < EQ_MAX_FREQS))
	{
		uint32 f = gEqBandFreqs[m_nSliderMenu][n];
		if (f != m_EQPreset.Freqs[m_nSliderMenu])
		{
			m_EQPreset.Freqs[m_nSliderMenu] = f;
			UpdateEQ(true);
			UpdateDialog();
		}
	}
}



/////////////////////////////////////////////////////////////
// CMidiSetupDlg

UI_MESSAGE_MAP_BEGIN(CMidiSetupDlg, PropertyPage)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO1,            &CMidiSetupDlg::OnSettingsChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO2,            &CMidiSetupDlg::OnSettingsChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO3,            &CMidiSetupDlg::OnSettingsChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO4,            &CMidiSetupDlg::OnSettingsChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO5,            &CMidiSetupDlg::OnSettingsChanged)
	UI_COMMAND(IDC_BUTTON1,                 &CMidiSetupDlg::OnRenameDevice)
	UI_COMMAND(IDC_CHECK1,                  &CMidiSetupDlg::OnSettingsChanged)
	UI_COMMAND(IDC_CHECK2,                  &CMidiSetupDlg::OnSettingsChanged)
	UI_COMMAND(IDC_CHECK3,                  &CMidiSetupDlg::OnSettingsChanged)
	UI_COMMAND(IDC_CHECK4,                  &CMidiSetupDlg::OnSettingsChanged)
	UI_COMMAND(IDC_CHECK5,                  &CMidiSetupDlg::OnSettingsChanged)
	UI_COMMAND(IDC_MIDI_TO_PLUGIN,          &CMidiSetupDlg::OnSettingsChanged)
	UI_COMMAND(IDC_MIDI_MACRO_CONTROL,      &CMidiSetupDlg::OnSettingsChanged)
	UI_COMMAND(IDC_MIDIVOL_TO_NOTEVOL,      &CMidiSetupDlg::OnSettingsChanged)
	UI_COMMAND(IDC_MIDIPLAYCONTROL,         &CMidiSetupDlg::OnSettingsChanged)
	UI_COMMAND(IDC_MIDIPLAYPATTERNONMIDIIN, &CMidiSetupDlg::OnSettingsChanged)
	UI_NOTIFY(ui::EditChange, IDC_EDIT1,                 &CMidiSetupDlg::OnSettingsChanged)
	UI_NOTIFY(ui::EditChange, IDC_EDIT2,                 &CMidiSetupDlg::OnSettingsChanged)
	UI_NOTIFY(ui::EditChange, IDC_EDIT3,                 &CMidiSetupDlg::OnSettingsChanged)
	UI_NOTIFY(ui::EditChange, IDC_EDIT4,                 &CMidiSetupDlg::OnSettingsChanged)
UI_MESSAGE_MAP_END()


void CMidiSetupDlg::DoDataExchange(DataExchange* pDX)
{
	PropertyPage::DoDataExchange(pDX);
	pDX->BindControl(IDC_EDIT1, m_SpinSpd);
	pDX->BindControl(IDC_EDIT2, m_SpinPat);
	pDX->BindControl(IDC_EDIT3, m_SpinAmp);
	pDX->BindControl(IDC_COMBO1, m_InputDevice);
	pDX->BindControl(IDC_COMBO2, m_ATBehaviour);
	pDX->BindControl(IDC_COMBO3, m_Quantize);
	pDX->BindControl(IDC_COMBO4, m_ContinueMode);
	pDX->BindControl(IDC_COMBO5, m_RecordPitchBend);
}


CMidiSetupDlg::CMidiSetupDlg(FlagSet<MidiSetup> flags, uint32 device)
	: PropertyPage{IDD_OPTIONS_MIDI}
	, m_midiSetup{flags}
	, m_nMidiDevice{device}
{
	m_SpinAmp.SetAccessibleSuffix(UL_("%"));
}


bool CMidiSetupDlg::OnInitDialog()
{
	PropertyPage::OnInitDialog();
	// Flags
	if(m_midiSetup[MidiSetup::RecordVelocity]) CheckDlgButton(IDC_CHECK1, ui::CheckOn);
	if(m_midiSetup[MidiSetup::RecordNoteOff]) CheckDlgButton(IDC_CHECK2, ui::CheckOn);
	if(m_midiSetup[MidiSetup::EnableMidiInOnStartup]) CheckDlgButton(IDC_CHECK3, ui::CheckOn);
	if(m_midiSetup[MidiSetup::TransposeKeyboard]) CheckDlgButton(IDC_CHECK4, ui::CheckOn);
	if(m_midiSetup[MidiSetup::SendMidiToPlugins]) CheckDlgButton(IDC_MIDI_TO_PLUGIN, ui::CheckOn);
	if(m_midiSetup[MidiSetup::RecordCCsAsMacros]) CheckDlgButton(IDC_MIDI_MACRO_CONTROL, ui::CheckOn);
	if(m_midiSetup[MidiSetup::ApplyChannelVolumeToVelocity]) CheckDlgButton(IDC_MIDIVOL_TO_NOTEVOL, ui::CheckOn);
	if(m_midiSetup[MidiSetup::RespondToPlayControl]) CheckDlgButton(IDC_MIDIPLAYCONTROL, ui::CheckOn);
	if(m_midiSetup[MidiSetup::PlayPatternOnMidiNote]) CheckDlgButton(IDC_MIDIPLAYPATTERNONMIDIIN, ui::CheckOn);

	// Midi In Device
	RefreshDeviceList(m_nMidiDevice);

	// Continue behaviour
	m_ContinueMode.AddString(UL_("From Cursor Position"));
	m_ContinueMode.AddString(UL_("From Start of Pattern"));
	m_ContinueMode.SetCurSel(m_midiSetup[MidiSetup::PlayPatternFromStart] ? 1 : 0);

	// Aftertouch behaviour
	m_ATBehaviour.ResetContent();
	static constexpr std::pair<const mpt::uchar *, RecordAftertouch> aftertouchOptions[] =
	{
		{ UL_("Do not record Aftertouch"), RecordAftertouch::DoNotRecord },
		{ UL_("Record as Volume Commands"), RecordAftertouch::RecordAsVolume },
		{ UL_("Record as MIDI Macros"), RecordAftertouch::RecordAsMacro },
	};

	for(const auto & [str, value] : aftertouchOptions)
	{
		int item = m_ATBehaviour.AddString(str);
		m_ATBehaviour.SetItemData(item, static_cast<uintptr_t>(value));
		if(value == TrackerSettings::Instance().aftertouchBehaviour)
		{
			m_ATBehaviour.SetCurSel(item);
		}
	}

	// Pitch Bend behaviour
	m_RecordPitchBend.ResetContent();
	static constexpr std::pair<const mpt::uchar *, RecordPitchBend> pitchBendOptions[] =
	{
		{ UL_("Do not record Pitch Bends"), RecordPitchBend::DoNotRecord },
		{ UL_("Record only as MIDI Macros"), RecordPitchBend::RecordAsMacro },
		{ UL_("Record as Finetune or MIDI Macros"), RecordPitchBend::RecordAsFinetuneOrMacro },
		{ UL_("Record only as Finetune"), RecordPitchBend::RecordAsFinetune },
	};

	for (const auto & [str, value] : pitchBendOptions)
	{
		int item = m_RecordPitchBend.AddString(str);
		m_RecordPitchBend.SetItemData(item, static_cast<uintptr_t>(value));
		if (value == TrackerSettings::Instance().pitchBendBehaviour)
		{
			m_RecordPitchBend.SetCurSel(item);
		}
	}

	// Note Velocity amp
	SetDlgItemInt(IDC_EDIT3, TrackerSettings::Instance().midiVelocityAmp);
	m_SpinAmp.SetRange(1, 10000);

	SetDlgItemText(IDC_EDIT4, mpt::ToUnicode(IgnoredCCsToString(TrackerSettings::Instance().midiIgnoreCCs)));

	// Midi Import settings
	SetDlgItemInt(IDC_EDIT1, TrackerSettings::Instance().midiImportTicks);
	SetDlgItemInt(IDC_EDIT2, TrackerSettings::Instance().midiImportPatternLen);

	// Note quantization
	m_Quantize.ResetContent();
	static constexpr std::pair<const mpt::uchar *, uint32> quantizeOptions[] =
	{
		{ UL_("1/4th Notes"),  4 },  { UL_("1/6th Notes"),  6 },
		{ UL_("1/8th Notes"),  8 },  { UL_("1/12th Notes"), 12 },
		{ UL_("1/16th Notes"), 16 }, { UL_("1/24th Notes"), 24 },
		{ UL_("1/32nd Notes"), 32 }, { UL_("1/48th Notes"), 48 },
		{ UL_("1/64th Notes"), 64 }, { UL_("1/96th Notes"), 96 },
	};

	for(const auto & [str, value]: quantizeOptions)
	{
		int item = m_Quantize.AddString(str);
		m_Quantize.SetItemData(item, value);
		if(value == TrackerSettings::Instance().midiImportQuantize)
		{
			m_Quantize.SetCurSel(item);
		}
	}
	m_SpinSpd.SetRange32(2, 16);
	m_SpinPat.SetRange32(ModSpecs::mptm.patternRowsMin, ModSpecs::mptm.patternRowsMax);
	return true;
}


void CMidiSetupDlg::RefreshDeviceList(uint32 currentDevice)
{
	m_InputDevice.SetRedraw(false);
	m_InputDevice.ResetContent();
	const auto deviceNames = CMainFrame::GetMidiInputDeviceNames();
	for(uint32 i = 0; i < deviceNames.size(); i++)
	{
		int item = m_InputDevice.AddString(theApp.GetFriendlyMIDIPortName(deviceNames[i], true));
		m_InputDevice.SetItemData(item, i);
		if(i == currentDevice)
		{
			m_InputDevice.SetCurSel(item);
		}
	}
	m_InputDevice.SetRedraw(true);
	m_InputDevice.Invalidate(false);
}


void CMidiSetupDlg::OnRenameDevice()
{
	int n = m_InputDevice.GetCurSel();
	if(n >= 0)
	{
		uint32 device = static_cast<uint32>(m_InputDevice.GetItemData(n));
		const auto deviceNames = CMainFrame::GetMidiInputDeviceNames();
		mpt::ustring name = (device < deviceNames.size()) ? deviceNames[device] : mpt::ustring();
		mpt::ustring friendlyName = theApp.GetSettings().Read(UL_("MIDI Input Ports"), mpt::ToUnicode(name), name);
		CInputDlg dlg(this, UL_("New name for ") + name + UL_(":"), friendlyName);
		if(dlg.DoModal() == IDOK)
		{
			if(dlg.resultAsString.empty() || dlg.resultAsString == name)
				theApp.GetSettings().Remove(UL_("MIDI Input Ports"), mpt::ToUnicode(name));
			else
				theApp.GetSettings().Write(UL_("MIDI Input Ports"), mpt::ToUnicode(name), dlg.resultAsString);
			RefreshDeviceList(device);
		}
	}
}


void CMidiSetupDlg::OnOK()
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	m_midiSetup.set(MidiSetup::RecordVelocity, IsDlgButtonChecked(IDC_CHECK1) != ui::CheckOff);
	m_midiSetup.set(MidiSetup::RecordNoteOff, IsDlgButtonChecked(IDC_CHECK2) != ui::CheckOff);
	m_midiSetup.set(MidiSetup::EnableMidiInOnStartup, IsDlgButtonChecked(IDC_CHECK3)  != ui::CheckOff);
	m_midiSetup.set(MidiSetup::TransposeKeyboard, IsDlgButtonChecked(IDC_CHECK4) != ui::CheckOff);
	m_midiSetup.set(MidiSetup::SendMidiToPlugins, IsDlgButtonChecked(IDC_MIDI_TO_PLUGIN) != ui::CheckOff);
	m_midiSetup.set(MidiSetup::RecordCCsAsMacros, IsDlgButtonChecked(IDC_MIDI_MACRO_CONTROL) != ui::CheckOff);
	m_midiSetup.set(MidiSetup::ApplyChannelVolumeToVelocity, IsDlgButtonChecked(IDC_MIDIVOL_TO_NOTEVOL) != ui::CheckOff);
	m_midiSetup.set(MidiSetup::RespondToPlayControl, IsDlgButtonChecked(IDC_MIDIPLAYCONTROL) != ui::CheckOff);
	m_midiSetup.set(MidiSetup::PlayPatternOnMidiNote, IsDlgButtonChecked(IDC_MIDIPLAYPATTERNONMIDIIN) != ui::CheckOff);
	m_midiSetup.set(MidiSetup::PlayPatternFromStart, m_ContinueMode.GetCurSel() == 1);

	int n = m_InputDevice.GetCurSel();
	if(n >= 0)
		m_nMidiDevice = static_cast<uint32>(m_InputDevice.GetItemData(n));
	else
		m_nMidiDevice = 0;

	TrackerSettings::Instance().aftertouchBehaviour = static_cast<RecordAftertouch>(m_ATBehaviour.GetItemData(m_ATBehaviour.GetCurSel()));
	TrackerSettings::Instance().pitchBendBehaviour = static_cast<RecordPitchBend>(m_RecordPitchBend.GetItemData(m_RecordPitchBend.GetCurSel()));

	TrackerSettings::Instance().midiVelocityAmp = static_cast<uint16>(Clamp(GetDlgItemInt(IDC_EDIT3), 1u, 10000u));

	mpt::ustring cc;
	GetDlgItemText(IDC_EDIT4, cc);
	TrackerSettings::Instance().midiIgnoreCCs = StringToIgnoredCCs(mpt::ToUnicode(cc));

	int minVal, maxVal;
	m_SpinSpd.GetRange32(minVal, maxVal);
	TrackerSettings::Instance().midiImportTicks = static_cast<uint8>(Clamp(static_cast<int>(GetDlgItemInt(IDC_EDIT1)), minVal, maxVal));
	m_SpinPat.GetRange32(minVal, maxVal);
	TrackerSettings::Instance().midiImportPatternLen = static_cast<ROWINDEX>(Clamp(static_cast<int>(GetDlgItemInt(IDC_EDIT2)), minVal, maxVal));
	if(m_Quantize.GetCurSel() != -1)
	{
		TrackerSettings::Instance().midiImportQuantize = static_cast<uint32>(m_Quantize.GetItemData(m_Quantize.GetCurSel()));
	}

	if(pMainFrm)
		pMainFrm->SetupMidi(m_midiSetup, m_nMidiDevice);
	PropertyPage::OnOK();
}


bool CMidiSetupDlg::OnSetActive()
{
	CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_MIDI;
	return PropertyPage::OnSetActive();
}


// Wine


UI_MESSAGE_MAP_BEGIN(COptionsWine, PropertyPage)
	UI_COMMAND(IDC_CHECK_WINE_ENABLE, &COptionsWine::OnSettingsChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO_WINE_PULSEAUDIO, &COptionsWine::OnSettingsChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO_WINE_PORTAUDIO, &COptionsWine::OnSettingsChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO_WINE_RTAUDIO, &COptionsWine::OnSettingsChanged)
UI_MESSAGE_MAP_END()


COptionsWine::COptionsWine()
	: PropertyPage(IDD_OPTIONS_WINE)
{
	return;
}


void COptionsWine::DoDataExchange(DataExchange* pDX)
{
	PropertyPage::DoDataExchange(pDX);
	pDX->BindControl(IDC_COMBO_WINE_PULSEAUDIO, m_CbnPulseAudio);
	pDX->BindControl(IDC_COMBO_WINE_PORTAUDIO, m_CbnPortAudio);
	pDX->BindControl(IDC_COMBO_WINE_RTAUDIO, m_CbnRtAudio);
}


bool COptionsWine::OnInitDialog()
{
	PropertyPage::OnInitDialog();
	GetDlgItem(IDC_CHECK_WINE_ENABLE)->EnableWindow(false);
	CheckDlgButton(IDC_CHECK_WINE_ENABLE, TrackerSettings::Instance().WineSupportEnabled ? ui::CheckOn : ui::CheckOff);
	int index;
	index = m_CbnPulseAudio.AddString(UL_("Auto"    )); m_CbnPulseAudio.SetItemData(index, 1);
	index = m_CbnPulseAudio.AddString(UL_("Enabled" )); m_CbnPulseAudio.SetItemData(index, 2);
	index = m_CbnPulseAudio.AddString(UL_("Disabled")); m_CbnPulseAudio.SetItemData(index, 0);
	m_CbnPulseAudio.SetCurSel(0);
	for(index = 0; index < 3; ++index)
	{
		if(m_CbnPulseAudio.GetItemData(index) == static_cast<uint32>(TrackerSettings::Instance().WineSupportEnablePulseAudio))
		{
			m_CbnPulseAudio.SetCurSel(index);
		}
	}
	index = m_CbnPortAudio.AddString(UL_("Auto"    )); m_CbnPortAudio.SetItemData(index, 1);
	index = m_CbnPortAudio.AddString(UL_("Enabled" )); m_CbnPortAudio.SetItemData(index, 2);
	index = m_CbnPortAudio.AddString(UL_("Disabled")); m_CbnPortAudio.SetItemData(index, 0);
	for(index = 0; index < 3; ++index)
	{
		if(m_CbnPortAudio.GetItemData(index) == static_cast<uint32>(TrackerSettings::Instance().WineSupportEnablePortAudio))
		{
			m_CbnPortAudio.SetCurSel(index);
		}
	}
	index = m_CbnRtAudio.AddString(UL_("Auto"    )); m_CbnRtAudio.SetItemData(index, 1);
	index = m_CbnRtAudio.AddString(UL_("Enabled" )); m_CbnRtAudio.SetItemData(index, 2);
	index = m_CbnRtAudio.AddString(UL_("Disabled")); m_CbnRtAudio.SetItemData(index, 0);
	for(index = 0; index < 3; ++index)
	{
		if(m_CbnRtAudio.GetItemData(index) == static_cast<uint32>(TrackerSettings::Instance().WineSupportEnableRtAudio))
		{
			m_CbnRtAudio.SetCurSel(index);
		}
	}
	return true;
}


void COptionsWine::OnSettingsChanged()
{
	SetModified(true);
}


void COptionsWine::OnOK()
{
	TrackerSettings::Instance().WineSupportEnabled = IsDlgButtonChecked(IDC_CHECK_WINE_ENABLE) ? true : false;
	TrackerSettings::Instance().WineSupportEnablePulseAudio = static_cast<int32>(m_CbnPulseAudio.GetItemData(m_CbnPulseAudio.GetCurSel()));
	TrackerSettings::Instance().WineSupportEnablePortAudio = static_cast<int32>(m_CbnPortAudio.GetItemData(m_CbnPortAudio.GetCurSel()));
	TrackerSettings::Instance().WineSupportEnableRtAudio = static_cast<int32>(m_CbnRtAudio.GetItemData(m_CbnRtAudio.GetCurSel()));
	PropertyPage::OnOK();
}


bool COptionsWine::OnSetActive()
{
	CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_WINE;
	return PropertyPage::OnSetActive();
}


OPENMPT_NAMESPACE_END
