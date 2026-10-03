/*
 * dlg_misc.cpp
 * ------------
 * Purpose: Implementation of various OpenMPT dialogs.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "ui/Ui.h"
#include "dlg_misc.h"
#include "MPTrackUtil.h"
#include "Childfrm.h"
#include "DlsBankExt.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "resource.h"
#include "TempoSwingDialog.h"
#include "TrackerSettings.h"
#include "WindowMessages.h"
#include "../common/mptStringBuffer.h"
#include "../common/version.h"
#include "../soundlib/mod_specifications.h"
#include "../soundlib/plugins/PlugInterface.h"

#include <ctime>

#if MPT_WINNT_AT_LEAST(MPT_WIN_VISTA) && defined(UNICODE)
#include <afxtaskdialog.h>
#endif


OPENMPT_NAMESPACE_BEGIN

namespace
{

// libopenmpt has no local timezone, so dates stored in local time are converted here
int64 FindUnixSecondsFromLocal(const mpt::Date::AnyGregorian &date)
{
	std::tm localTime{};
	localTime.tm_year = date.year - 1900;
	localTime.tm_mon = static_cast<int>(date.month) - 1;
	localTime.tm_mday = static_cast<int>(date.day);
	localTime.tm_hour = date.hours;
	localTime.tm_min = date.minutes;
	localTime.tm_sec = date.seconds;
	localTime.tm_isdst = -1;
	return static_cast<int64>(std::mktime(&localTime));
}

}  // namespace



///////////////////////////////////////////////////////////////////////
// CModTypeDlg


UI_MESSAGE_MAP_BEGIN(CModTypeDlg, DialogBase)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO1,			&CModTypeDlg::UpdateDialog)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO_TEMPOMODE,	&CModTypeDlg::OnTempoModeChanged)
	UI_COMMAND(IDC_CHECK_PT1X,				&CModTypeDlg::OnPTModeChanged)
	UI_COMMAND(IDC_BUTTON1,					&CModTypeDlg::OnTempoSwing)
	UI_COMMAND(IDC_BUTTON2,					&CModTypeDlg::OnLegacyPlaybackSettings)
	UI_COMMAND(IDC_BUTTON3,					&CModTypeDlg::OnDefaultBehaviour)

UI_MESSAGE_MAP_END()


void CModTypeDlg::DoDataExchange(DataExchange* pDX)
{
	DialogBase::DoDataExchange(pDX);
	pDX->BindControl(IDC_COMBO1, m_TypeBox);
	pDX->BindControl(IDC_COMBO2, m_ChannelsBox);
	pDX->BindControl(IDC_COMBO_TEMPOMODE, m_TempoModeBox);
	pDX->BindControl(IDC_COMBO_MIXLEVELS, m_PlugMixBox);

	pDX->BindControl(IDC_CHECK1, m_CheckBox1);
	pDX->BindControl(IDC_CHECK2, m_CheckBox2);
	pDX->BindControl(IDC_CHECK3, m_CheckBox3);
	pDX->BindControl(IDC_CHECK4, m_CheckBox4);
	pDX->BindControl(IDC_CHECK5, m_CheckBox5);
	pDX->BindControl(IDC_CHECK_PT1X, m_CheckBoxPT1x);
	pDX->BindControl(IDC_CHECK_AMIGALIMITS, m_CheckBoxAmigaLimits);

}


CModTypeDlg::CModTypeDlg(CTrackerSoundFile &sf, Wnd *parent)
	: DialogBase{IDD_MODDOC_MODTYPE, parent}
	, sndFile{sf}
{
}


bool CModTypeDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();
	m_nType = sndFile.GetBestSaveFormat();
	m_nChannels = sndFile.GetNumChannels();
	m_tempoSwing = sndFile.m_tempoSwing;
	m_playBehaviour = sndFile.m_playBehaviour;
	m_initialized = false;

	// Mod types

	m_TypeBox.SetItemData(m_TypeBox.AddString(UL_("ProTracker MOD")), MOD_TYPE_MOD);
	m_TypeBox.SetItemData(m_TypeBox.AddString(UL_("Scream Tracker S3M")), MOD_TYPE_S3M);
	m_TypeBox.SetItemData(m_TypeBox.AddString(UL_("FastTracker XM")), MOD_TYPE_XM);
	m_TypeBox.SetItemData(m_TypeBox.AddString(UL_("Impulse Tracker IT")), MOD_TYPE_IT);
	m_TypeBox.SetItemData(m_TypeBox.AddString(UL_("OpenMPT MPTM")), MOD_TYPE_MPT);
	switch(m_nType)
	{
	case MOD_TYPE_S3M:	m_TypeBox.SetCurSel(1); break;
	case MOD_TYPE_XM:	m_TypeBox.SetCurSel(2); break;
	case MOD_TYPE_IT:	m_TypeBox.SetCurSel(3); break;
	case MOD_TYPE_MPT:	m_TypeBox.SetCurSel(4); break;
	default:			m_TypeBox.SetCurSel(0); break;
	}

	// Time signature information

	SetDlgItemInt(IDC_ROWSPERBEAT, sndFile.m_nDefaultRowsPerBeat);
	SetDlgItemInt(IDC_ROWSPERMEASURE, sndFile.m_nDefaultRowsPerMeasure);

	// Version information

	if(sndFile.m_dwCreatedWithVersion) SetDlgItemText(IDC_EDIT_CREATEDWITH, UL_("OpenMPT ") + FormatVersionNumber(sndFile.m_dwCreatedWithVersion));
	SetDlgItemText(IDC_EDIT_SAVEDWITH, mpt::ToUnicode(sndFile.m_modFormat.madeWithTracker.empty() ? sndFile.m_modFormat.formatName : sndFile.m_modFormat.madeWithTracker));

	OnDPIChanged();
	UpdateDialog();

	m_initialized = true;
	return true;
}


void CModTypeDlg::OnDPIChanged()
{
	DialogBase::OnDPIChanged();

	m_warnIcon = ui::CreateWarningIcon(ui::ScalePixels(32, this));

	if(m_showWarning)
		static_cast<Static *>(GetDlgItem(IDC_STATIC1))->SetBitmap(&m_warnIcon);
}


mpt::ustring CModTypeDlg::FormatVersionNumber(Version version)
{
	return mpt::ToUnicode(version.ToUString() + (version.IsTestVersion() ? UL_(" (test build)") : UL_("")));
}


void CModTypeDlg::UpdateChannelCBox()
{
	const MODTYPE type = static_cast<MODTYPE>(m_TypeBox.GetItemData(m_TypeBox.GetCurSel()));
	CHANNELINDEX currChanSel = static_cast<CHANNELINDEX>(m_ChannelsBox.GetItemData(m_ChannelsBox.GetCurSel()));
	const CHANNELINDEX minChans = CSoundFile::GetModSpecifications(type).channelsMin;
	const CHANNELINDEX maxChans = CSoundFile::GetModSpecifications(type).channelsMax;

	if(m_ChannelsBox.GetCount() < 1
		|| m_ChannelsBox.GetItemData(0) != minChans
		|| m_ChannelsBox.GetItemData(m_ChannelsBox.GetCount() - 1) != maxChans)
	{
		// Update channel list if number of supported channels has changed.
		if(m_ChannelsBox.GetCount() < 1) currChanSel = m_nChannels;
		m_ChannelsBox.ResetContent();

		mpt::ustring s;
		for(CHANNELINDEX i = minChans; i <= maxChans; i++)
		{
			s = ui::Format(UL_("%u Channel%s"), i, (i != 1) ? UL_("s") : UL_(""));
			m_ChannelsBox.SetItemData(m_ChannelsBox.AddString(s), i);
		}

		Limit(currChanSel, minChans, maxChans);
		m_ChannelsBox.SetCurSel(currChanSel - minChans);
	}
}


void CModTypeDlg::UpdateDialog()
{
	m_nType = static_cast<MODTYPE>(m_TypeBox.GetItemData(m_TypeBox.GetCurSel()));

	UpdateChannelCBox();

	m_CheckBox1.SetCheck(sndFile.m_SongFlags[SONG_LINEARSLIDES] ? ui::CheckOn : ui::CheckOff);
	m_CheckBox2.SetCheck(sndFile.m_SongFlags[SONG_FASTVOLSLIDES] ? ui::CheckOn : ui::CheckOff);
	m_CheckBox3.SetCheck(sndFile.m_SongFlags[SONG_ITOLDEFFECTS] ? ui::CheckOn : ui::CheckOff);
	m_CheckBox4.SetCheck(sndFile.m_SongFlags[SONG_ITCOMPATGXX] ? ui::CheckOn : ui::CheckOff);
	m_CheckBox5.SetCheck(sndFile.m_SongFlags[SONG_EXFILTERRANGE] ? ui::CheckOn : ui::CheckOff);
	m_CheckBoxPT1x.SetCheck(sndFile.m_SongFlags[SONG_PT_MODE] ? ui::CheckOn : ui::CheckOff);
	m_CheckBoxAmigaLimits.SetCheck(sndFile.m_SongFlags[SONG_AMIGALIMITS] ? ui::CheckOn : ui::CheckOff);

	const FlagSet<SongFlags> allowedFlags(sndFile.GetModSpecifications(m_nType).songFlags);
	m_CheckBox1.EnableWindow(allowedFlags[SONG_LINEARSLIDES]);
	m_CheckBox2.EnableWindow(allowedFlags[SONG_FASTVOLSLIDES]);
	m_CheckBox3.EnableWindow(allowedFlags[SONG_ITOLDEFFECTS]);
	m_CheckBox4.EnableWindow(allowedFlags[SONG_ITCOMPATGXX]);
	m_CheckBox5.EnableWindow(allowedFlags[SONG_EXFILTERRANGE]);
	m_CheckBoxPT1x.EnableWindow(allowedFlags[SONG_PT_MODE]);
	m_CheckBoxAmigaLimits.EnableWindow(allowedFlags[SONG_AMIGALIMITS]);

	// These two checkboxes are mutually exclusive and share the same screen space
	m_CheckBoxPT1x.ShowWindow(m_nType == MOD_TYPE_MOD ? true : false);
	m_CheckBox5.ShowWindow(m_nType != MOD_TYPE_MOD ? true : false);
	if(allowedFlags[SONG_PT_MODE]) OnPTModeChanged();

	// Tempo modes
	const TempoMode oldTempoMode = m_initialized ? static_cast<TempoMode>(m_TempoModeBox.GetItemData(m_TempoModeBox.GetCurSel())) : sndFile.m_nTempoMode;
	m_TempoModeBox.ResetContent();

	m_TempoModeBox.SetItemData(m_TempoModeBox.AddString(UL_("Classic")), static_cast<uintptr_t>(TempoMode::Classic));
	if(m_nType == MOD_TYPE_MPT || (sndFile.GetType() != MOD_TYPE_MPT && sndFile.m_nTempoMode == TempoMode::Alternative))
		m_TempoModeBox.SetItemData(m_TempoModeBox.AddString(UL_("Alternative")), static_cast<uintptr_t>(TempoMode::Alternative));
	if(m_nType == MOD_TYPE_MPT || (sndFile.GetType() != MOD_TYPE_MPT && sndFile.m_nTempoMode == TempoMode::Modern))
		m_TempoModeBox.SetItemData(m_TempoModeBox.AddString(UL_("Modern (accurate)")), static_cast<uintptr_t>(TempoMode::Modern));
	m_TempoModeBox.SetCurSel(0);
	for(int i = m_TempoModeBox.GetCount(); i > 0; i--)
	{
		if(static_cast<TempoMode>(m_TempoModeBox.GetItemData(i)) == oldTempoMode)
		{
			m_TempoModeBox.SetCurSel(i);
			break;
		}
	}
	OnTempoModeChanged();

	// Mix levels
	const MixLevels oldMixLevels = m_initialized ? static_cast<MixLevels>(m_PlugMixBox.GetItemData(m_PlugMixBox.GetCurSel())) : sndFile.GetMixLevels();
	m_PlugMixBox.ResetContent();
	if(m_nType == MOD_TYPE_MPT || sndFile.GetMixLevels() == MixLevels::v1_17RC3) // In XM/IT, this is only shown for backwards compatibility with existing tunes
		m_PlugMixBox.SetItemData(m_PlugMixBox.AddString(UL_("OpenMPT 1.17RC3")), static_cast<uintptr_t>(MixLevels::v1_17RC3));
	if(sndFile.GetMixLevels() == MixLevels::v1_17RC2) // Only shown for backwards compatibility with existing tunes
		m_PlugMixBox.SetItemData(m_PlugMixBox.AddString(UL_("OpenMPT 1.17RC2")), static_cast<uintptr_t>(MixLevels::v1_17RC2));
	if(sndFile.GetMixLevels() == MixLevels::v1_17RC1) // Ditto
		m_PlugMixBox.SetItemData(m_PlugMixBox.AddString(UL_("OpenMPT 1.17RC1")), static_cast<uintptr_t>(MixLevels::v1_17RC1));
	if(sndFile.GetMixLevels() == MixLevels::Original) // Ditto
		m_PlugMixBox.SetItemData(m_PlugMixBox.AddString(UL_("Original (MPT 1.16)")), static_cast<uintptr_t>(MixLevels::Original));
	int compatMixMode = m_PlugMixBox.AddString(UL_("Compatible"));
	m_PlugMixBox.SetItemData(compatMixMode, static_cast<uintptr_t>(MixLevels::Compatible));
	if(m_nType == MOD_TYPE_XM)
		m_PlugMixBox.SetItemData(m_PlugMixBox.AddString(UL_("Compatible (FT2 Pan Law)")), static_cast<uintptr_t>(MixLevels::CompatibleFT2));

	// Default to compatible mix mode
	m_PlugMixBox.SetCurSel(compatMixMode);
	int mixCount = m_PlugMixBox.GetCount();
	for(int i = 0; i < mixCount; i++)
	{
		if(static_cast<MixLevels>(m_PlugMixBox.GetItemData(i)) == oldMixLevels)
		{
			m_PlugMixBox.SetCurSel(i);
			break;
		}
	}

	const bool XMorITorMPT = (m_nType & (MOD_TYPE_XM | MOD_TYPE_IT | MOD_TYPE_MPT));
	const bool isMPTM = (m_nType == MOD_TYPE_MPT);

	// Mixmode Box
	GetDlgItem(IDC_TEXT_MIXMODE)->EnableWindow(XMorITorMPT);
	m_PlugMixBox.EnableWindow(XMorITorMPT);

	// Tempo mode box
	m_TempoModeBox.EnableWindow(XMorITorMPT);
	GetDlgItem(IDC_ROWSPERBEAT)->EnableWindow(XMorITorMPT);
	GetDlgItem(IDC_ROWSPERMEASURE)->EnableWindow(XMorITorMPT);
	GetDlgItem(IDC_TEXT_ROWSPERBEAT)->EnableWindow(XMorITorMPT);
	GetDlgItem(IDC_TEXT_ROWSPERMEASURE)->EnableWindow(XMorITorMPT);
	GetDlgItem(IDC_TEXT_TEMPOMODE)->EnableWindow(XMorITorMPT);
	GetDlgItem(IDC_FRAME_TEMPOMODE)->EnableWindow(XMorITorMPT);

	// Compatibility settings
	const PlayBehaviourSet defaultBehaviour = CSoundFile::GetDefaultPlaybackBehaviour(m_nType);
	const PlayBehaviourSet supportedBehaviour = CSoundFile::GetSupportedPlaybackBehaviour(m_nType);
	bool enableSetDefaults = false;
	m_showWarning = false;
	if(m_nType & (MOD_TYPE_MPT | MOD_TYPE_IT | MOD_TYPE_XM))
	{
		for(size_t i = 0; i < m_playBehaviour.size(); i++)
		{
			// Some flags are not really important for "default" behaviour.
			if(defaultBehaviour[i] != m_playBehaviour[i]
				&& i != MSF_COMPATIBLE_PLAY
				&& i != kFT2VolumeRamping)
			{
				enableSetDefaults = true;
				if(!isMPTM)
				{
					m_showWarning = true;
					break;
				}
			}
			if(isMPTM && m_playBehaviour[i] && !supportedBehaviour[i])
			{

				enableSetDefaults = true;
				m_showWarning = true;
				break;
			}
		}
	}
	static_cast<Static *>(GetDlgItem(IDC_STATIC1))->SetBitmap(m_showWarning ? &m_warnIcon : nullptr);
	GetDlgItem(IDC_STATIC2)->SetWindowText(m_showWarning
		? UL_("Playback settings have been set to legacy compatibility mode. Click \"Set Defaults\" to use the recommended settings instead.")
		: UL_("Compatibility settings are currently optimal. It is advised to not edit them."));
	GetDlgItem(IDC_BUTTON3)->EnableWindow(enableSetDefaults ? true : false);
}


void CModTypeDlg::OnPTModeChanged()
{
	// PT1/2 mode enforces Amiga limits
	const bool ptMode = IsDlgButtonChecked(IDC_CHECK_PT1X) != ui::CheckOff;
	m_CheckBoxAmigaLimits.EnableWindow(!ptMode);
	if(ptMode) m_CheckBoxAmigaLimits.SetCheck(ui::CheckOn);
}


void CModTypeDlg::OnTempoModeChanged()
{
	GetDlgItem(IDC_BUTTON1)->EnableWindow(static_cast<TempoMode>(m_TempoModeBox.GetItemData(m_TempoModeBox.GetCurSel())) == TempoMode::Modern);
}


void CModTypeDlg::OnTempoSwing()
{
	const ROWINDEX oldRPB = sndFile.m_nDefaultRowsPerBeat;
	const ROWINDEX oldRPM = sndFile.m_nDefaultRowsPerMeasure;
	const TempoMode oldMode = sndFile.m_nTempoMode;

	// Temporarily apply new tempo signature for preview
	const ROWINDEX newRPB = std::clamp(static_cast<ROWINDEX>(GetDlgItemInt(IDC_ROWSPERBEAT)), ROWINDEX(1), MAX_ROWS_PER_BEAT);
	const ROWINDEX newRPM = std::clamp(static_cast<ROWINDEX>(GetDlgItemInt(IDC_ROWSPERMEASURE)), newRPB, MAX_ROWS_PER_BEAT);
	sndFile.m_nDefaultRowsPerBeat = newRPB;
	sndFile.m_nDefaultRowsPerMeasure = newRPM;
	sndFile.m_nTempoMode = TempoMode::Modern;

	m_tempoSwing.resize(newRPB, TempoSwing::Unity);
	CTempoSwingDlg dlg(this, m_tempoSwing, sndFile);
	if(dlg.DoModal() == IDOK)
	{
		m_tempoSwing = dlg.m_tempoSwing;
	}
	sndFile.m_nDefaultRowsPerBeat = oldRPB;
	sndFile.m_nDefaultRowsPerMeasure = oldRPM;
	sndFile.m_nTempoMode = oldMode;
}


void CModTypeDlg::OnLegacyPlaybackSettings()
{
	CLegacyPlaybackSettingsDlg dlg(this, m_playBehaviour, m_nType);
	if(dlg.DoModal() == IDOK)
	{
		m_playBehaviour = dlg.GetPlayBehaviour();
	}
	UpdateDialog();
}


void CModTypeDlg::OnDefaultBehaviour()
{
	m_playBehaviour = CSoundFile::GetDefaultPlaybackBehaviour(m_nType);
	UpdateDialog();
}


bool CModTypeDlg::VerifyData()
{
	const int newRPB = GetDlgItemInt(IDC_ROWSPERBEAT);
	const int newRPM = GetDlgItemInt(IDC_ROWSPERMEASURE);
	if(newRPB > newRPM)
	{
		Reporting::Warning("Error: Rows per measure must be greater than or equal to rows per beat.");
		GotoDlgCtrl(GetDlgItem(IDC_ROWSPERMEASURE));
		return false;
	}
	if(newRPB == 0 && static_cast<TempoMode>(m_TempoModeBox.GetItemData(m_TempoModeBox.GetCurSel())) == TempoMode::Modern)
	{
		Reporting::Warning("Error: Rows per beat must be greater than 0 in modern tempo mode.");
		GotoDlgCtrl(GetDlgItem(IDC_ROWSPERBEAT));
		return false;
	}

	int sel = static_cast<int>(m_ChannelsBox.GetItemData(m_ChannelsBox.GetCurSel()));
	MODTYPE type = static_cast<MODTYPE>(m_TypeBox.GetItemData(m_TypeBox.GetCurSel()));

	CHANNELINDEX maxChans = CSoundFile::GetModSpecifications(type).channelsMax;

	if(sel > maxChans)
	{
		mpt::ustring error;
		error = ui::Format(UL_("Error: Maximum number of channels for this module type is %u."), maxChans);
		Reporting::Warning(error);
		return false;
	}

	if(maxChans < sndFile.GetNumChannels())
	{
		if(Reporting::Confirm("New module type supports less channels than currently used, and reducing channel number is required. Continue?") != cnfYes)
			return false;
	}

	return true;
}


void CModTypeDlg::OnOK()
{
	if (!VerifyData())
		return;

	int sel = m_TypeBox.GetCurSel();
	if (sel >= 0)
	{
		m_nType = static_cast<MODTYPE>(m_TypeBox.GetItemData(sel));
	}
	const auto &newModSpecs = sndFile.GetModSpecifications(m_nType);

	sndFile.m_SongFlags.set(SONG_LINEARSLIDES, m_CheckBox1.GetCheck() != ui::CheckOff);
	sndFile.m_SongFlags.set(SONG_FASTVOLSLIDES, m_CheckBox2.GetCheck() != ui::CheckOff);
	sndFile.m_SongFlags.set(SONG_ITOLDEFFECTS, m_CheckBox3.GetCheck() != ui::CheckOff);
	sndFile.m_SongFlags.set(SONG_ITCOMPATGXX, m_CheckBox4.GetCheck() != ui::CheckOff);
	sndFile.m_SongFlags.set(SONG_EXFILTERRANGE, m_CheckBox5.GetCheck() != ui::CheckOff);
	sndFile.m_SongFlags.set(SONG_PT_MODE, m_CheckBoxPT1x.GetCheck() != ui::CheckOff);
	sndFile.m_SongFlags.set(SONG_AMIGALIMITS, m_CheckBoxAmigaLimits.GetCheck() != ui::CheckOff);

	sel = m_ChannelsBox.GetCurSel();
	if (sel >= 0)
	{
		m_nChannels = static_cast<CHANNELINDEX>(m_ChannelsBox.GetItemData(sel));
	}

	sndFile.m_nDefaultRowsPerBeat    = std::min(static_cast<ROWINDEX>(GetDlgItemInt(IDC_ROWSPERBEAT)), MAX_ROWS_PER_BEAT);
	sndFile.m_nDefaultRowsPerMeasure = std::min(static_cast<ROWINDEX>(GetDlgItemInt(IDC_ROWSPERMEASURE)), MAX_ROWS_PER_BEAT);
	sndFile.m_PlayState.UpdateTimeSignature(sndFile);

	sel = m_TempoModeBox.GetCurSel();
	if(sel >= 0)
	{
		const auto oldMode = sndFile.m_nTempoMode;
		sndFile.m_nTempoMode = static_cast<TempoMode>(m_TempoModeBox.GetItemData(sel));
		if(oldMode == TempoMode::Modern && sndFile.m_nTempoMode != TempoMode::Modern)
		{
			for(auto &order : sndFile.Order)
			{
				double newTempo = order.GetDefaultTempo().ToDouble() * (order.GetDefaultSpeed() * sndFile.m_nDefaultRowsPerBeat) / ((sndFile.m_nTempoMode == TempoMode::Classic) ? 24 : 60);
				if(!newModSpecs.hasFractionalTempo)
					newTempo = std::round(newTempo);
				order.SetDefaultTempo(Clamp(TEMPO(newTempo), newModSpecs.GetTempoMin(), newModSpecs.GetTempoMax()));
			}
		}
	}
	if(sndFile.m_nTempoMode == TempoMode::Modern)
	{
		sndFile.m_tempoSwing = m_tempoSwing;
		if(!sndFile.m_tempoSwing.empty())
			sndFile.m_tempoSwing.resize(sndFile.m_nDefaultRowsPerBeat);
	} else
	{
		sndFile.m_tempoSwing.clear();
	}

	sel = m_PlugMixBox.GetCurSel();
	if(sel >= 0)
	{
		sndFile.SetMixLevels(static_cast<MixLevels>(m_PlugMixBox.GetItemData(sel)));
	}

	PlayBehaviourSet allowedFlags = CSoundFile::GetSupportedPlaybackBehaviour(m_nType);
	for(size_t i = 0; i < kMaxPlayBehaviours; i++)
	{
		// Only set those flags which are supported by the new format or were already enabled previously
		sndFile.m_playBehaviour.set(i, m_playBehaviour[i] && (allowedFlags[i] || (sndFile.m_playBehaviour[i] && sndFile.GetType() == m_nType)));
	}

	DialogBase::OnOK();
}


mpt::ustring CModTypeDlg::GetToolTipText(uint32 id, WindowHandle) const
{
	mpt::ustring text;
	switch(id)
	{
	case IDC_CHECK1:
		text = UL_("Note slides always slide the same amount, not depending on the sample frequency.");
		break;
	case IDC_CHECK2:
		text = UL_("Old Scream Tracker 3 volume slide behaviour (not recommended).");
		break;
	case IDC_CHECK3:
		text = UL_("Play some effects like in early versions of Impulse Tracker (not recommended).");
		break;
	case IDC_CHECK4:
		text = UL_("Gxx and Exx/Fxx won't share effect memory. Gxx resets instrument envelopes.");
		break;
	case IDC_CHECK5:
		text = UL_("The resonant filter's frequency range is increased from about 5kHz to 10kHz.");
		break;
	case IDC_CHECK_PT1X:
		text = UL_("Enforce Amiga frequency limits, ProTracker offset bug emulation.");
		break;
	case IDC_COMBO_MIXLEVELS:
		text = UL_("Mixing method of sample and instrument plugin levels.");
		break;
	case IDC_BUTTON1:
		if(!GetDlgItem(IDC_BUTTON1)->IsWindowEnabled())
		{
			text = UL_("Tempo swing is only available in modern tempo mode.");
		} else
		{
			text = UL_("Swing setting: ");
			if(m_tempoSwing.empty())
			{
				text += UL_("Default");
			} else
			{
				for(size_t i = 0; i < m_tempoSwing.size(); i++)
				{
					if(i > 0)
						text += UL_(" / ");
					text += MPT_UFORMAT("{}%")(Util::muldivr(m_tempoSwing[i], 100, TempoSwing::Unity));
				}
			}
		}
	}

	return text;
}


//////////////////////////////////////////////////////////////////////////////
// Legacy Playback Settings dialog

UI_MESSAGE_MAP_BEGIN(CLegacyPlaybackSettingsDlg, ResizableDialog)
	UI_COMMAND(IDC_BUTTON1,      &CLegacyPlaybackSettingsDlg::OnSelectDefaults)
	UI_NOTIFY(ui::EditUpdate, IDC_EDIT1,      &CLegacyPlaybackSettingsDlg::OnFilterStringChanged)
	UI_NOTIFY(ui::CheckListChange, IDC_LIST1, &CLegacyPlaybackSettingsDlg::UpdateSelectDefaults)
UI_MESSAGE_MAP_END()


void CLegacyPlaybackSettingsDlg::DoDataExchange(DataExchange* pDX)
{
	ResizableDialog::DoDataExchange(pDX);
	pDX->BindControl(IDC_LIST1, m_CheckList);
}


CLegacyPlaybackSettingsDlg::CLegacyPlaybackSettingsDlg(Wnd *parent, PlayBehaviourSet &playBehaviour, MODTYPE modType)
	: ResizableDialog{IDD_LEGACY_PLAYBACK, parent}
	, m_playBehaviour{playBehaviour}
	, m_modType{modType}
{
}


bool CLegacyPlaybackSettingsDlg::OnInitDialog()
{
	ResizableDialog::OnInitDialog();
	m_CheckList.SetItemHeight(0, 0);  // Workaround to force MFC to correctly compute the height of the first list item, in particular on high-DPI setups
	OnFilterStringChanged();
	UpdateSelectDefaults();
	return true;
}


void CLegacyPlaybackSettingsDlg::OnSelectDefaults()
{
	const int count = m_CheckList.GetCount();
	m_playBehaviour = CSoundFile::GetDefaultPlaybackBehaviour(m_modType);
	for(int i = 0; i < count; i++)
	{
		m_CheckList.SetCheck(i, m_playBehaviour[m_CheckList.GetItemData(i)] ? ui::CheckOn : ui::CheckOff);
	}
}


void CLegacyPlaybackSettingsDlg::UpdateSelectDefaults()
{
	const int count = m_CheckList.GetCount();
	for(int i = 0; i < count; i++)
	{
		m_playBehaviour.set(m_CheckList.GetItemData(i),  m_CheckList.GetCheck(i) != ui::CheckOff);
	}
	const auto defaults = CSoundFile::GetDefaultPlaybackBehaviour(m_modType);
	GetDlgItem(IDC_BUTTON1)->EnableWindow(m_playBehaviour != defaults ? true : false);
}


void CLegacyPlaybackSettingsDlg::OnFilterStringChanged()
{
	mpt::ustring s;
	GetDlgItemText(IDC_EDIT1, s);
	const bool filterActive = !s.empty();
	s = mpt::ToLowerCaseLocale(s);

	m_CheckList.SetRedraw(false);
	m_CheckList.ResetContent();

	const auto allowedFlags = CSoundFile::GetSupportedPlaybackBehaviour(m_modType);
	for(size_t i = 0; i < kMaxPlayBehaviours; i++)
	{
		const mpt::uchar *desc = UL_("");
		switch(i)
		{
		case MSF_COMPATIBLE_PLAY: continue;

		case kMPTOldSwingBehaviour: desc = UL_("OpenMPT 1.17 compatible random variation behaviour for instruments"); break;
		case kMIDICCBugEmulation: desc = UL_("Plugin volume MIDI CC bug emulation"); break;
		case kOldMIDIPitchBends: desc = UL_("Old Pitch Wheel behaviour for instrument plugins"); break;
		case kFT2VolumeRamping: desc = UL_("Use smooth Fasttracker 2 volume ramping"); break;
		case kMODVBlankTiming: desc = UL_("VBlank timing: F20 and above sets speed instead of tempo"); break;

		case kSlidesAtSpeed1: desc = UL_("Execute regular portamento slides at speed 1"); break;
		case kPeriodsAreHertz: desc = UL_("Compute note frequency in Hertz rather than periods"); break;
		case kTempoClamp: desc = UL_("Clamp tempo to 32-255 range"); break;
		case kPerChannelGlobalVolSlide: desc = UL_("Global volume slide memory is per-channel"); break;
		case kPanOverride: desc = UL_("Panning commands override surround and random pan variation"); break;

		case kITInstrWithoutNote: desc = UL_("Retrigger instrument envelopes on instrument change"); break;
		case kITVolColFinePortamento: desc = UL_("Volume column portamento never does fine portamento"); break;
		case kITArpeggio: desc = UL_("IT arpeggio algorithm"); break;
		case kITOutOfRangeDelay: desc = UL_("Out-of-range delay commands queue new instrument"); break;
		case kITPortaMemoryShare: desc = UL_("Gxx shares memory with Exx and Fxx"); break;
		case kITPatternLoopTargetReset: desc = UL_("After finishing a pattern loop, set the pattern loop target to the next row"); break;
		case kITFT2PatternLoop: desc = UL_("Nested pattern loop behaviour"); break;
		case kITPingPongNoReset: desc = UL_("Do not reset ping pong direction with instrument numbers"); break;
		case kITEnvelopeReset: desc = UL_("IT envelope reset behaviour"); break;
		case kITClearOldNoteAfterCut: desc = UL_("Forget the previous note after cutting it"); break;
		case kITVibratoTremoloPanbrello: desc = UL_("More IT-like Vibrato, Tremolo and Panbrello handling"); break;
		case kITTremor: desc = UL_("Ixx behaves like in IT"); break;
		case kITRetrigger: desc = UL_("Qxx behaves like in IT"); break;
		case kITMultiSampleBehaviour: desc = UL_("Properly update C-5 frequency when changing note in multisampled instrument"); break;
		case kITPortaTargetReached: desc = UL_("Clear portamento target after it has been reached"); break;
		case kITPatternLoopBreak: desc = UL_("Do not reset loop count on pattern break"); break;
		case kITOffset: desc = UL_("Offset after sample end is treated like in IT"); break;
		case kITSwingBehaviour: desc = UL_("Volume and panning random variation work more like in IT"); break;
		case kITNNAReset: desc = UL_("NNA is reset on every note change, not every instrument change"); break;
		case kITSCxStopsSample: desc = UL_("SCx really stops the sample and does not just mute it"); break;
		case kITEnvelopePositionHandling: desc = UL_("IT-style envelope position advance + enable/disable behaviour"); break;
		case kITPortamentoInstrument: desc = UL_("More compatible instrument change + portamento"); break;
		case kITPingPongMode: desc = UL_("Do not repeat last sample point in ping pong loop, like IT's software mixer"); break;
		case kITRealNoteMapping: desc = UL_("Use triggered note rather than translated note for PPS and DNA note check"); break;
		case kITHighOffsetNoRetrig: desc = UL_("SAx does not apply an offset effect to a note next to it"); break;
		case kITFilterBehaviour: desc = UL_("User IT's filter coefficients (unless extended filter range is used) and behaviour"); break;
		case kITNoSurroundPan: desc = UL_("Panning modulation is disabled on surround channels"); break;
		case kITShortSampleRetrig: desc = UL_("Do not retrigger already stopped channels"); break;
		case kITPortaNoNote: desc = UL_("Do not apply any portamento if no previous note is playing"); break;
		case kITFT2DontResetNoteOffOnPorta:
			if(m_modType == MOD_TYPE_XM)
				desc = UL_("Reset note-off on portamento if there is an instrument number");
			else
				desc = UL_("Reset note-off on portamento if there is an instrument number in Compatible Gxx mode");
			break;
		case kITVolColMemory: desc = UL_("Volume column effects share their memory with the effect column"); break;
		case kITPortamentoSwapResetsPos: desc = UL_("Portamento with sample swap plays the new sample from the beginning"); break;
		case kITEmptyNoteMapSlot: desc = UL_("Ignore instrument note map entries with no note completely"); break;
		case kITFirstTickHandling: desc = UL_("IT-style first tick handling"); break;
		case kITSampleAndHoldPanbrello: desc = UL_("IT-style sample&hold panbrello waveform"); break;
		case kITClearPortaTarget: desc = UL_("New notes reset portamento target in IT"); break;
		case kITPanbrelloHold: desc = UL_("Do not reset panbrello effect until next note or panning effect"); break;
		case kITPanningReset: desc = UL_("Sample and instrument panning is only applied on note change, not instrument change"); break;
		case kITPatternLoopWithJumpsOld: desc = UL_("Bxx on the same row as SBx terminates the loop in IT"); break;
		case kITInstrWithNoteOff: desc = UL_("Instrument number with note-off recalls default volume"); break;
		case kFT2Arpeggio: desc = UL_("FT2 arpeggio algorithm"); break;
		case kFT2Retrigger: desc = UL_("Rxx behaves like in FT2"); break;
		case kFT2VolColVibrato: desc = UL_("Vibrato speed in volume column does not actually execute the vibrato effect"); break;
		case kFT2PortaNoNote: desc = UL_("Do not play portamento-ed note if no previous note is playing"); break;
		case kFT2KeyOff: desc = UL_("FT2-style Kxx handling"); break;
		case kFT2PanSlide: desc = UL_("Volume-column pan slides are finer"); break;
		case kFT2ST3OffsetOutOfRange: desc = UL_("Offset past sample end stops the note"); break;
		case kFT2RestrictXCommand: desc = UL_("Do not allow ModPlug extensions to X command"); break;
		case kFT2RetrigWithNoteDelay: desc = UL_("Retrigger envelopes if there is a note delay with no note"); break;
		case kFT2SetPanEnvPos: desc = UL_("Lxx only sets the pan envelope position if the volume envelope's sustain flag is set"); break;
		case kFT2PortaIgnoreInstr: desc = UL_("Portamento with instrument number applies volume settings of new sample, but not the new sample itself"); break;
		case kFT2VolColMemory: desc = UL_("No volume column memory"); break;
		case kFT2LoopE60Restart: desc = UL_("Next pattern starts on the same row as the last E60 command"); break;
		case kFT2ProcessSilentChannels: desc = UL_("Keep processing faded channels for later portamento pickup"); break;
		case kFT2ReloadSampleSettings: desc = UL_("Reload sample settings even if a note-off is placed next to an instrument number"); break;
		case kFT2PortaDelay: desc = UL_("Portamento with note delay next to it is ignored"); break;
		case kFT2Transpose: desc = UL_("Ignore out-of-range transposed notes"); break;
		case kFT2PatternLoopWithJumps: desc = UL_("Bxx or Dxx on the same row as E6x terminates the loop"); break;
		case kFT2PortaTargetNoReset: desc = UL_("Portamento target is not reset with new notes"); break;
		case kFT2EnvelopeEscape: desc = UL_("Sustain point at end of envelope loop stops the loop after release"); break;
		case kFT2Tremor: desc = UL_("Txx behaves like in FT2"); break;
		case kFT2OutOfRangeDelay: desc = UL_("Do not trigger notes with out-of-range note delay"); break;
		case kFT2Periods: desc = UL_("Use FT2's broken period handling"); break;
		case kFT2PanWithDelayedNoteOff: desc = UL_("Panning command with delayed note-off is ignored"); break;
		case kFT2VolColDelay: desc = UL_("FT2-style volume column handling if there is a note delay"); break;
		case kFT2FinetunePrecision: desc = UL_("Round sample finetune to multiples of 8"); break;
		case kFT2NoteOffFlags: desc = UL_("Fade instrument on note-off when there is no volume envelope; instrument numbers reset note-off status"); break;
		case kITMultiSampleInstrumentNumber: desc = UL_("Lone instrument number after portamento within multi-sampled instrument sets the target sample's settings"); break;
		case kRowDelayWithNoteDelay: desc = UL_("Note delays next to a row delay are repeated on every row repetition"); break;
		case kFT2MODTremoloRampWaveform: desc = UL_("Emulate FT2/ProTracker tremolo ramp down / triangle waveform"); break;
		case kFT2PortaUpDownMemory: desc = UL_("Portamento Up and Down have separate effect memory"); break;
		case kST3NoMutedChannels: desc = UL_("Do not process any effects on muted S3M channels"); break;
		case kST3EffectMemory: desc = UL_("Most effects share the same memory"); break;
		case kST3PortaSampleChange: desc = UL_("Portamento with instrument number applies volume settings of new sample, but not the new sample itself (GUS)"); break;
		case kST3VibratoMemory: desc = UL_("Do not remember vibrato type in effect memory"); break;
		case kST3LimitPeriod: desc = UL_("Stop note when reaching the format's maximum note frequency"); break;
		case KST3PortaAfterArpeggio: desc = UL_("Portamento immediately following an arpeggio effect continues at the last arpeggiated note"); break;
		case kMODOneShotLoops: desc = UL_("ProTracker one-shot loops"); break;
		case kMODIgnorePanning: desc = UL_("Ignore panning commands"); break;
		case kMODSampleSwap: desc = UL_("Enable on-the-fly sample swapping"); break;
		case kMODOutOfRangeNoteDelay: desc = UL_("Out-of-range note delay is played on next row"); break;
		case kMODTempoOnSecondTick: desc = UL_("Tempo changes are handled on second tick instead of first"); break;
		case kFT2PanSustainRelease: desc = UL_("If the sustain point of the panning envelope is reached before key-off, it is never released"); break;
		case kLegacyReleaseNode: desc = UL_("Old volume envelope release node scaling behaviour"); break;
		case kOPLBeatingOscillators: desc = UL_("Beating OPL oscillators"); break;
		case kST3OffsetWithoutInstrument: desc = UL_("Notes without instrument use the previous note's sample offset"); break;
		case kReleaseNodePastSustainBug: desc = UL_("Broken release node after sustain end behaviour"); break;
		case kFT2NoteDelayWithoutInstr: desc = UL_("Delayed instrument-less notes should not recall volume and panning"); break;
		case kOPLFlexibleNoteOff: desc = UL_("Full control over OPL notes after note-off"); break;
		case kITInstrWithNoteOffOldEffects: desc = UL_("Instrument number with note-off retriggers envelopes with Old Effects enabled"); break;
		case kMIDIVolumeOnNoteOffBug: desc = UL_("Reset VST volume on note-off"); break;
		case kITDoNotOverrideChannelPan: desc = UL_("Instruments / samples with forced panning do not override channel panning for following instruments / samples"); break;
		case kITPatternLoopWithJumps: desc = UL_("Bxx right of SBx terminates the loop in IT"); break;
		case kITDCTBehaviour: desc = UL_("Duplicate Sample Check requires same instrument, Duplicate Note Check uses pattern notes for comparison"); break;
		case kOPLwithNNA: desc = UL_("New Note Action / Duplicate Note Action set to Note Off and Note Fade affect OPL notes like samples"); break;
		case kST3RetrigAfterNoteCut: desc = UL_("Notes cannot be retriggered after they have been cut"); break;
		case kST3SampleSwap: desc = UL_("Enable on-the-fly sample swapping (SoundBlaster driver)"); break;
		case kOPLRealRetrig: desc = UL_("Retrigger (Qxy) affects OPL notes"); break;
		case kOPLNoResetAtEnvelopeEnd: desc = UL_("Do not reset OPL channel status at end of envelopes"); break;
		case kOPLNoteStopWith0Hz: desc = UL_("OPL key-off sets note frequency to 0 Hz"); break;
		case kOPLNoteOffOnNoteChange: desc = UL_("Send OPL key-off when triggering notes"); break;
		case kFT2PortaResetDirection: desc = UL_("Tone Portamento direction resets after reaching portamento target from below"); break;
		case kApplyUpperPeriodLimit: desc = UL_("Apply lower frequency limit"); break;
		case kApplyOffsetWithoutNote: desc = UL_("Offset commands work without a note next to them"); break;
		case kITPitchPanSeparation: desc = UL_("Pitch / Pan Separation can be overridden by panning commands"); break;
		case kImprecisePingPongLoops: desc = UL_("Use old imprecise ping-pong loop end calculation"); break;
		case kPluginIgnoreTonePortamento:
			if(m_modType == MOD_TYPE_XM)
				desc = UL_("Ignore tone portamento and fine pitch slides for instrument plugins");
			else
				desc = UL_("Ignore tone portamento for instrument plugins");
			break;
		case kST3TonePortaWithAdlibNote: desc = UL_("OPL notes with Tone Portamento are delayed until the next row"); break;
		case kITResetFilterOnPortaSmpChange: desc = UL_("Reset filter on portamento if new note plays a different sample"); break;
		case kITInitialNoteMemory: desc = UL_("Initial Last Note Memory of each channel is C-0 instead of No Note"); break;
		case kPluginDefaultProgramAndBank1: desc = UL_("Assume initial plugin MIDI program and bank number is 1"); break;
		case kITNoSustainOnPortamento: desc = UL_("Portamento after note-off does not re-enable sample sustain loop"); break;
		case kITEmptyNoteMapSlotIgnoreCell: desc = UL_("Ignore pattern cell completely when trying to play unmapped instrument note"); break;
		case kITOffsetWithInstrNumber: desc = UL_("Offset command with instrument number recalls offset with last note"); break;
		case kContinueSampleWithoutInstr: desc = UL_("New note without instrument number does not play looped samples from the start"); break;
		case kMIDINotesFromChannelPlugin: desc = UL_("MIDI notes can be sent to channel plugins"); break;
		case kITDoublePortamentoSlides: desc = UL_("Parameters of conflicting volume and effect column portamento commands may overwrite each other"); break;
		case kS3MIgnoreCombinedFineSlides: desc =UL_("Ignore combined fine slides (Kxy / Lxy)"); break;
		case kFT2AutoVibratoAbortSweep: desc = UL_("Key-off before auto-vibrato sweep-in is complete resets auto-vibrato depth"); break;
		case kLegacyPPQpos: desc = UL_("Report inaccurate PPQ position to VST plugins (like OpenMPT 1.31 and older)"); break;
		case kLegacyPluginNNABehaviour: desc = UL_("Plugin notes with New Note Action set to Continue are affected by note-offs (like OpenMPT 1.31 and older)"); break;
		case kITCarryAfterNoteOff: desc = UL_("Note-Off status does not influence Envelope Carry behaviour"); break;
		case kFT2OffsetMemoryRequiresNote: desc = UL_("Offset effect memory is only updated when the command is next to a note"); break;
		case kITNoteCutWithPorta: desc = UL_("Note Cut (SCx) resets note pitch and interacts with tone portamento + row delay"); break;
		case kITVolColNoSlidePropagation: desc = UL_("Do not propagate volume column volume slide memory to regular effect column"); break;
		case kITStoppedFilterEnvAtStart: desc = UL_("Stopped filter envelope is still applied even if its first tick has not been processed yet"); break;
		case kITCompatGxxCarryPortaWithIns: desc = UL_("Envelope Carry quirk in Compatible Gxx mode with portamento and instrument number"); break;

		default: MPT_ASSERT_NOTREACHED();
		}

		if(filterActive && mpt::ToLowerCaseLocale(mpt::ustring{desc}).find(s) == mpt::ustring::npos)
			continue;

		if(m_playBehaviour[i] || allowedFlags[i])
		{
			int item = m_CheckList.AddString(desc);
			m_CheckList.SetItemData(item, i);
			int check = m_playBehaviour[i] ? ui::CheckOn : ui::CheckOff;
			if(!allowedFlags[i])
				check = ui::CheckMixed;  // Is checked but not supported by format -> grey out
			m_CheckList.SetCheck(item, check);
		}
	}
	m_CheckList.SetRedraw(true);
}


///////////////////////////////////////////////////////////
// CRemoveChannelsDlg

void CRemoveChannelsDlg::DoDataExchange(DataExchange* pDX)
{
	DialogBase::DoDataExchange(pDX);
	pDX->BindControl(IDC_REMCHANSLIST, m_RemChansList);
}


UI_MESSAGE_MAP_BEGIN(CRemoveChannelsDlg, DialogBase)
	UI_NOTIFY(ui::ListSelChange, IDC_REMCHANSLIST,		&CRemoveChannelsDlg::OnChannelChanged)
UI_MESSAGE_MAP_END()


CRemoveChannelsDlg::CRemoveChannelsDlg(CTrackerSoundFile &sf, CHANNELINDEX toRemove, bool showCancel, Wnd *parent)
	: DialogBase{IDD_REMOVECHANNELS, parent}
	, sndFile{sf}
	, m_bKeepMask(sf.GetNumChannels(), true)
	, m_nRemove{toRemove}
	, m_ShowCancel{showCancel}
{
}


bool CRemoveChannelsDlg::OnInitDialog()
{
	mpt::ustring s;
	DialogBase::OnInitDialog();
	const CHANNELINDEX numChannels = sndFile.GetNumChannels();
	for(CHANNELINDEX n = 0; n < numChannels; n++)
	{
		s = MPT_UFORMAT("Channel {}")(n + 1);
		if(sndFile.ChnSettings[n].szName[0] >= 0x20)
		{
			s += UL_(": ");
			s += mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.ChnSettings[n].szName);
		}
		m_RemChansList.SetItemData(m_RemChansList.AddString(s), n);
		if (!m_bKeepMask[n]) m_RemChansList.SetSel(n);
	}

	if (m_nRemove > 0)
		s = MPT_UFORMAT("Select {} channel{} to remove:")(m_nRemove, (m_nRemove != 1) ? mpt::ustring(UL_("s")) : mpt::ustring(UL_("")));
	else
		s = MPT_UFORMAT("Select channels to remove (the minimum number of remaining channels is {})")(sndFile.GetModSpecifications().channelsMin);

	SetDlgItemText(IDC_QUESTION1, s);
	if(GetDlgItem(IDCANCEL)) GetDlgItem(IDCANCEL)->ShowWindow(m_ShowCancel);

	OnChannelChanged();
	return true;
}


void CRemoveChannelsDlg::OnOK()
{
	int selCount = m_RemChansList.GetSelCount();
	std::vector<int> selected(selCount);
	m_RemChansList.GetSelItems(selCount, selected.data());

	m_bKeepMask.assign(sndFile.GetNumChannels(), true);
	for (const auto sel : selected)
	{
		m_bKeepMask[sel] = false;
	}
	if ((static_cast<CHANNELINDEX>(selCount) == m_nRemove && selCount > 0)
		|| (m_nRemove == 0 && (sndFile.GetNumChannels() >= selCount + sndFile.GetModSpecifications().channelsMin)))
		DialogBase::OnOK();
	else
		DialogBase::OnCancel();
}


void CRemoveChannelsDlg::OnChannelChanged()
{
	const uint32 selCount = m_RemChansList.GetSelCount();
	GetDlgItem(IDOK)->EnableWindow(((selCount == m_nRemove && selCount > 0)  || (m_nRemove == 0 && (sndFile.GetNumChannels() >= selCount + sndFile.GetModSpecifications().channelsMin) && selCount > 0)) ? true : false);
}


InfoDialog::InfoDialog(Wnd *parent)
	: ResizableDialog(IDD_INFO_BOX, parent)
{ }

bool InfoDialog::OnInitDialog()
{
	ResizableDialog::OnInitDialog();
	SetWindowText(m_caption.c_str());
	SetDlgItemText(IDC_EDIT1, m_content.c_str());
	return true;
}

void InfoDialog::SetContent(mpt::ustring content)
{
	m_content = std::move(content);
}

void InfoDialog::SetCaption(mpt::ustring caption)
{
	m_caption = std::move(caption);
}

////////////////////////////////////////////////////////////////////////////////
// Sound Bank Information

CSoundBankProperties::CSoundBankProperties(const CDLSBank &bank, Wnd *parent)
	: InfoDialog(parent)
{
	const SOUNDBANKINFO &bi = bank.GetBankInfo();
	std::string info;
	info.reserve(128 + bi.szBankName.size() + bi.szDescription.size() + bi.szCopyRight.size() + bi.szEngineer.size() + bi.szSoftware.size() + bi.szComments.size());
	info = "Type:\t" + std::string((bank.GetBankType() & SOUNDBANK_TYPE_SF2) ? "Sound Font (SF2)" : "Downloadable Sound (DLS)");
	if (bi.szBankName.size())
		info += "\r\nName:\t" + bi.szBankName;
	if (bi.szDescription.size())
		info += "\r\n\t" + bi.szDescription;
	if (bi.szCopyRight.size())
		info += "\r\nCopyright:\t" + bi.szCopyRight;
	if (bi.szEngineer.size())
		info += "\r\nAuthor:\t" + bi.szEngineer;
	if (bi.szSoftware.size())
		info += "\r\nSoftware:\t" + bi.szSoftware;
	if (bi.szComments.size())
		info += "\r\n\r\nComments:\r\n" + bi.szComments;
	SetCaption(bank.GetFileName().ToUnicode() + UL_(" - Sound Bank Information"));
	SetContent(mpt::ToUnicode(mpt::Charset::Locale, info));
}


////////////////////////////////////////////////////////////////////////////////////////////
// Keyboard Control

static constexpr uint8 whitetab[7] = {0,2,4,5,7,9,11};
static constexpr uint8 blacktab[7] = {0xff,1,3,0xff,6,8,10};

UI_MESSAGE_MAP_BEGIN(CKeyboardControl, Wnd)
	UI_MESSAGE_MAP_END()


void CKeyboardControl::Init(Wnd *parent, int octaves, bool cursorNotify)
{
	m_parent = parent;
	m_nOctaves = std::max(1, octaves);
	m_cursorNotify = cursorNotify;
	KeyFlags.fill(KeyFlag::Normal);
	m_sampleNum.fill(0);
	OnDPIChanged();
}


void CKeyboardControl::OnDPIChanged()
{
	// Point size to pixels
	m_font = ui::Font(FL_HELVETICA, MulDiv(60, ui::kLogicalDpi, 720));
}


void CKeyboardControl::DrawKey(ui::Painter &dc, const Rect rect, int key, bool black) const
{
	const bool selected = (key == m_nSelection);
	ColorRef color = black ? RGB(20, 20, 20) : RGB(255, 255, 255);
	if(m_mouseDown && selected)
		color = black ? RGB(104, 104, 104) : RGB(212, 212, 212);
	else if(selected)
		color = black ? RGB(130, 130, 130) : RGB(228, 228, 228);
	dc.SetDCBrushColor(color);
	dc.Rectangle(&rect);

	if(static_cast<size_t>(key) < std::size(KeyFlags) && KeyFlags[key] != KeyFlag::Normal)
	{
		const int margin = black ? 0 : 2;
		Rect ellipseRect(rect.left + margin, rect.bottom - rect.Width() + margin, rect.right - margin, rect.bottom - margin);
		dc.SetDCBrushColor((KeyFlags[key] & KeyFlag::BrightDot) ? RGB(255, 192, 192) : RGB(255, 0, 0));
		dc.Ellipse(ellipseRect);
		if(m_sampleNum[key] != 0)
		{
			dc.SetTextColor((KeyFlags[key] & KeyFlag::BrightDot) ? RGB(0, 0, 0) : RGB(255, 255, 255));
			dc.DrawText(ui::Format(UL_("%u"), m_sampleNum[key]), ellipseRect, ui::TextCenter | ui::TextSingleLine | ui::TextVCenter);
		}

		if(KeyFlags[key] == (KeyFlag::RedDot | KeyFlag::BrightDot))
		{
			// Both flags set: Draw second dot
			ellipseRect.MoveToY(ellipseRect.top - ellipseRect.Height() - 2);
			dc.SetDCBrushColor(RGB(255, 0, 0));
			dc.Ellipse(ellipseRect);
		}
	}
}


void CKeyboardControl::OnPaint(ui::Painter &dc)
{
	Rect rect;
	const Rect rcClient = GetClientRect();

	dc.SetBkTransparent(true);
	rect = rcClient;
	dc.SetFont(m_font);

	// Rectangle outline
	dc.SetDCPenColor(RGB(50, 50, 50));

	// White notes
	for(int note = 0; note < m_nOctaves * 7; note++)
	{
		rect.right = ((note + 1) * rcClient.Width()) / (m_nOctaves * 7);
		int val = (note / 7) * 12 + whitetab[note % 7];

		DrawKey(dc, rect, val, false);

		rect.left = rect.right - 1;
	}

	// Black notes
	rect = rcClient;
	rect.bottom -= rcClient.Height() / 3;
	for(int note = 0; note < m_nOctaves * 7; note++)
	{
		switch(note % 7)
		{
		case 1:
		case 2:
		case 4:
		case 5:
		case 6:
		{
			rect.left = (note * rcClient.Width()) / (m_nOctaves * 7);
			rect.right = rect.left;
			int delta = rcClient.Width() / (m_nOctaves * 7 * 3);
			rect.left -= delta;
			rect.right += delta;
			int val = (note / 7) * 12 + blacktab[note % 7];

			DrawKey(dc, rect, val, true);
			break;
		}
		}
	}

}


void CKeyboardControl::OnMouseMove(uint32 flags, Point point)
{
	Rect rcClient, rect;
	GetClientRect(&rcClient);
	rect = rcClient;
	int xmin = rcClient.right;
	int xmax = rcClient.left;
	int sel = -1;
	// White notes
	for(int note = 0; note < m_nOctaves * 7; note++)
	{
		int val = (note / 7) * 12 + whitetab[note % 7];
		rect.right = ((note + 1) * rcClient.Width()) / (m_nOctaves * 7);
		if (val == m_nSelection)
		{
			if (rect.left < xmin) xmin = rect.left;
			if (rect.right > xmax) xmax = rect.right;
		}
		if (rect.PtInRect(point))
		{
			sel = val;
			if (rect.left < xmin) xmin = rect.left;
			if (rect.right > xmax) xmax = rect.right;
		}
		rect.left = rect.right - 1;
	}
	// Black notes
	rect = rcClient;
	rect.bottom -= rcClient.Height() / 3;
	for(int note = 0; note < m_nOctaves * 7; note++)
	{
		switch(note % 7)
		{
		case 1:
		case 2:
		case 4:
		case 5:
		case 6:
		{
			int val = (note / 7) * 12 + blacktab[note % 7];
			rect.left = (note * rcClient.Width()) / (m_nOctaves * 7);
			rect.right = rect.left;
			int delta = rcClient.Width() / (m_nOctaves * 7 * 3);
			rect.left -= delta;
			rect.right += delta;
			if(val == m_nSelection)
			{
				if(rect.left < xmin)
					xmin = rect.left;
				if(rect.right > xmax)
					xmax = rect.right;
			}
			if(rect.PtInRect(point))
			{
				sel = val;
				if(rect.left < xmin)
					xmin = rect.left;
				if(rect.right > xmax)
					xmax = rect.right;
			}
			break;
		}
		}
	}
	// Check for selection change
	if(sel != m_nSelection)
	{
		m_nSelection = sel;
		rcClient.left = xmin;
		rcClient.right = xmax;
		InvalidateRect(&rcClient, false);
		if(m_cursorNotify && m_parent)
		{
			m_parent->PostMessage(MSG_MOD_KBDNOTIFY, KBDNOTIFY_MOUSEMOVE, m_nSelection);
			if(flags & ui::MouseLeft)
				m_parent->SendMessage(MSG_MOD_KBDNOTIFY, KBDNOTIFY_LBUTTONDOWN, m_nSelection);
		}
	}
	if(sel >= 0)
	{
		if(!m_mouseCapture)
		{
			m_mouseCapture = true;
			SetCapture();
		}
	} else
	{
		if(m_mouseCapture)
		{
			m_mouseCapture = false;
			ReleaseCapture();
		}
	}
}


void CKeyboardControl::OnLButtonDown(uint32, Point)
{
	m_mouseDown = true;
	InvalidateRect(nullptr, false);
	if(m_parent)
		m_parent->SendMessage(MSG_MOD_KBDNOTIFY, KBDNOTIFY_LBUTTONDOWN, m_nSelection);
}


void CKeyboardControl::OnLButtonUp(uint32, Point)
{
	m_mouseDown = false;
	InvalidateRect(nullptr, false);
	if(m_parent)
		m_parent->SendMessage(MSG_MOD_KBDNOTIFY, KBDNOTIFY_LBUTTONUP, m_nSelection);
}


////////////////////////////////////////////////////////////////////////////////
//
// Sample Map
//

UI_MESSAGE_MAP_BEGIN(CSampleMapDlg, ResizableDialog)
	UI_MESSAGE(MSG_MOD_KBDNOTIFY, &CSampleMapDlg::OnKeyboardNotify)
	UI_COMMAND(IDC_CHECK1,       &CSampleMapDlg::OnUpdateSamples)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO1, &CSampleMapDlg::OnSampleChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO2, &CSampleMapDlg::UpdateRegionStartEndSelection)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO3, &CSampleMapDlg::OnRegionBoundaryChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO4, &CSampleMapDlg::OnRegionBoundaryChanged)
UI_MESSAGE_MAP_END()

void CSampleMapDlg::DoDataExchange(DataExchange* pDX)
{
	ResizableDialog::DoDataExchange(pDX);
	pDX->BindControl(IDC_KEYBOARD1, m_Keyboard);
	pDX->BindControl(IDC_COMBO1, m_CbnSample);
	pDX->BindControl(IDC_COMBO2, m_CbnRegion);
	pDX->BindControl(IDC_COMBO3, m_CbnRegionStart);
	pDX->BindControl(IDC_COMBO4, m_CbnRegionEnd);
	pDX->BindControl(IDC_SLIDER1, m_SbOctave);
}


CSampleMapDlg::CSampleMapDlg(CTrackerSoundFile &sf, INSTRUMENTINDEX instr, Wnd *parent)
	: ResizableDialog{IDD_EDITSAMPLEMAP, parent}
	, m_sndFile{sf}
	, m_nInstrument{instr}
	, m_minNote{static_cast<ModCommand::NOTE>((sf.GetType() == MOD_TYPE_XM) ? NOTE_MIN + 12 : NOTE_MIN)}
	, m_maxNote{static_cast<ModCommand::NOTE>((sf.GetType() == MOD_TYPE_XM) ? NOTE_MIN + 107 : (NOTE_MIN + 119))}
{
	ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	if(pIns)
		KeyboardMap = pIns->Keyboard;
}


bool CSampleMapDlg::OnInitDialog()
{
	ResizableDialog::OnInitDialog();
	m_Keyboard.Init(this, 3, true);
	m_SbOctave.SetRange((m_minNote - NOTE_MIN) / 12, (m_maxNote - NOTE_MIN) / 12 - 2);
	m_SbOctave.SetPos(4);
	AppendNotesToControlEx(m_CbnRegionStart, m_sndFile, m_nInstrument, m_minNote, m_maxNote);
	AppendNotesToControlEx(m_CbnRegionEnd, m_sndFile, m_nInstrument, m_minNote, m_maxNote);
	OnUpdateSamples();
	OnUpdateOctave();
	UpdateRegionControls();
	return true;
}


void CSampleMapDlg::OnDPIChanged()
{
	ResizableDialog::OnDPIChanged();
	m_Keyboard.OnDPIChanged();
}


void CSampleMapDlg::OnHScroll(uint32 nCode, uint32 nPos, Wnd *pBar)
{
	ResizableDialog::OnHScroll(nCode, nPos, pBar);
	OnUpdateKeyboard();
	OnUpdateOctave();
}


void CSampleMapDlg::OnSampleChanged()
{
	UpdateRegionControls();
	OnUpdateKeyboard();
}


void CSampleMapDlg::OnUpdateSamples()
{
	uint32 oldPos = 0;
	uint32 newPos = 0;

	if(m_nInstrument >= MAX_INSTRUMENTS)
		return;
	if(m_CbnSample.GetCount() > 0)
		oldPos = static_cast<uint32>(m_CbnSample.GetItemData(m_CbnSample.GetCurSel()));
	m_CbnSample.SetRedraw(false);
	m_CbnSample.ResetContent();
	const bool showAll = (IsDlgButtonChecked(IDC_CHECK1) != false) || (*std::max_element(std::begin(KeyboardMap), std::end(KeyboardMap)) == 0);

	uint32 insertPos = m_CbnSample.AddString(UL_("0: No sample"));
	m_CbnSample.SetItemData(insertPos, 0);

	for(SAMPLEINDEX i = 1; i <= m_sndFile.GetNumSamples(); i++)
	{
		bool isUsed = showAll || mpt::contains(KeyboardMap, i);
		if(isUsed)
		{
			mpt::ustring sampleName;
			sampleName = ui::Format(UL_("%d: %s"), i, mpt::ToUnicode(m_sndFile.GetCharsetInternal(), m_sndFile.GetSampleName(i)).c_str());
			insertPos = m_CbnSample.AddString(sampleName);

			m_CbnSample.SetItemData(insertPos, i);
			if(i == oldPos)
				newPos = insertPos;
		}
	}
	m_CbnSample.SetRedraw(true);
	m_CbnSample.SetCurSel(newPos);
	OnUpdateKeyboard();
}


void CSampleMapDlg::OnUpdateOctave()
{
	mpt::uchar s[64];
	const uint32 baseOctave = m_SbOctave.GetPos() & 7;
	wsprintf(s, UL_("Octaves %u-%u"), baseOctave, baseOctave + 2);
	SetDlgItemText(IDC_TEXT1, s);
}


void CSampleMapDlg::OnUpdateKeyboard()
{
	SAMPLEINDEX nSample = static_cast<SAMPLEINDEX>(m_CbnSample.GetItemData(m_CbnSample.GetCurSel()));
	const uint32 baseOctave = m_SbOctave.GetPos() & 7;
	bool redraw = false;
	for(uint32 iNote = 0; iNote < 3 * 12; iNote++)
	{
		const auto oldFlags = m_Keyboard.GetFlags(iNote);
		const SAMPLEINDEX oldSmp = m_Keyboard.GetSample(iNote);
		uint32 ndx = baseOctave * 12 + iNote;
		FlagSet<CKeyboardControl::KeyFlag> newFlags = CKeyboardControl::KeyFlag::Normal;
		if(KeyboardMap[ndx] == nSample)
			newFlags = CKeyboardControl::KeyFlag::RedDot;
		else if(KeyboardMap[ndx] != 0)
			newFlags = CKeyboardControl::KeyFlag::BrightDot;
		if(newFlags != oldFlags || oldSmp != KeyboardMap[ndx])
		{
			m_Keyboard.SetFlags(iNote, newFlags);
			m_Keyboard.SetSample(iNote, KeyboardMap[ndx]);
			redraw = true;
		}
	}
	if(redraw)
		m_Keyboard.InvalidateRect(NULL, false);
}


LResult CSampleMapDlg::OnKeyboardNotify(WParam wParam, LParam lParam)
{
	mpt::ustring s;
	if((lParam >= 0) && (lParam < 3 * 12))
	{
		const SAMPLEINDEX sample = static_cast<SAMPLEINDEX>(m_CbnSample.GetItemData(m_CbnSample.GetCurSel()));
		const uint32 baseOctave = m_SbOctave.GetPos() & 7;

		s = mpt::ToUnicode(m_sndFile.GetNoteName(static_cast<ModCommand::NOTE>(lParam + 1 + 12 * baseOctave), m_nInstrument));
		ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
		if((wParam == KBDNOTIFY_LBUTTONDOWN) && (sample < MAX_SAMPLES) && pIns)
		{
			const uint32 note = static_cast<uint32>(baseOctave * 12 + lParam);

			if(m_mouseAction == MouseAction::Unknown)
			{
				// Mouse down -> decide if we are going to set or remove notes
				m_mouseAction = MouseAction::Set;
				if(KeyboardMap[note] == sample)
				{
					m_mouseAction = (KeyboardMap[note] == pIns->Keyboard[note]) ? MouseAction::Zero : MouseAction::Unset;
				}
			}

			switch(m_mouseAction)
			{
			case MouseAction::Unknown:
			case MouseAction::Set:
				KeyboardMap[note] = sample;
				break;
			case MouseAction::Unset:
				KeyboardMap[note] = pIns->Keyboard[note];
				break;
			case MouseAction::Zero:
				if(KeyboardMap[note] == sample)
					KeyboardMap[note] = 0;
				break;
			}
			OnUpdateKeyboard();
			UpdateRegionControls(static_cast<ModCommand::NOTE>(baseOctave * 12 + lParam + 1));
		}
	} else
	{
		s = UL_("--");
	}

	if(wParam == KBDNOTIFY_LBUTTONUP)
		m_mouseAction = MouseAction::Unknown;

	SetDlgItemText(IDC_TEXT2, s);
	return 0;
}


void CSampleMapDlg::RecalcSampleRegions()
{
	m_regions.clear();
	const SAMPLEINDEX selectedSample = static_cast<SAMPLEINDEX>(m_CbnSample.GetItemData(m_CbnSample.GetCurSel()));
	ModCommand::NOTE regionStart = NOTE_NONE;
	for(ModCommand::NOTE note = m_minNote; note <= m_maxNote; note++)
	{
		const SAMPLEINDEX currentSample = KeyboardMap[note - NOTE_MIN];
		if(currentSample == selectedSample)
		{
			if(regionStart == NOTE_NONE)
				regionStart = note;
		} else if(regionStart != NOTE_NONE)
		{
			m_regions.push_back({regionStart, static_cast<ModCommand::NOTE>(note - 1)});
			regionStart = NOTE_NONE;
		}
	}
	if(regionStart != NOTE_NONE)
		m_regions.push_back({regionStart, m_maxNote});
}


void CSampleMapDlg::UpdateRegionControls(ModCommand::NOTE modifiedNote)
{
	// Find the currently selected region to keep the selection active
	SampleRegion lastSelectedRegion{};
	if(modifiedNote == NOTE_NONE && m_CbnRegion.GetCurSel() >= 0)
	{
		const int lastRegionIndex = m_CbnRegion.GetCurSel();
		if(lastRegionIndex >= 0 && lastRegionIndex < static_cast<int>(m_regions.size()))
			lastSelectedRegion = m_regions[lastRegionIndex];
	}

	RecalcSampleRegions();

	m_CbnRegion.SetRedraw(false);
	m_CbnRegion.ResetContent();
	int regionToSelect = 0;
	for(size_t i = 0; i < m_regions.size(); i++)
	{
		const auto &region = m_regions[i];
		const auto startNoteName = m_sndFile.GetNoteName(region.startNote, m_nInstrument);
		const auto endNoteName = m_sndFile.GetNoteName(region.endNote, m_nInstrument);
		const int insertPos = m_CbnRegion.AddString(MPT_UFORMAT("Region {} to {} ({} of {})")(startNoteName, endNoteName, i + 1, m_regions.size()));
		if(mpt::is_in_range(modifiedNote, region.startNote, region.endNote))
		{
			// If a specific note was modified, select the region containing that note
			regionToSelect = insertPos;
		} else if(modifiedNote == NOTE_NONE && lastSelectedRegion.startNote != NOTE_NONE)
		{
			// If no note was modified, fall back to a region intersecting with the last selected region
			if(region.startNote <= lastSelectedRegion.endNote && region.endNote >= lastSelectedRegion.startNote)
				regionToSelect = insertPos;
		}
	}

	if(m_regions.empty())
		m_CbnRegion.AddString(UL_("No active regions"));

	const bool enable = m_regions.empty() ? false : true;
	m_CbnRegion.EnableWindow(enable);
	m_CbnRegionStart.EnableWindow(enable);
	m_CbnRegionEnd.EnableWindow(enable);
	m_CbnRegion.SetCurSel(regionToSelect);
	m_CbnRegion.SetRedraw(true);
	m_CbnRegion.Invalidate(false);

	UpdateRegionStartEndSelection();
}


void CSampleMapDlg::UpdateRegionStartEndSelection()
{
	const int regionIndex = m_CbnRegion.GetCurSel();
	if(regionIndex < 0 || regionIndex >= static_cast<int>(m_regions.size()))
		return;
	const auto &region = m_regions[regionIndex];
	m_CbnRegionStart.SetCurSel(region.startNote - m_minNote);
	m_CbnRegionEnd.SetCurSel(region.endNote - m_minNote);
}


void CSampleMapDlg::OnRegionBoundaryChanged()
{
	const SAMPLEINDEX selectedSample = static_cast<SAMPLEINDEX>(m_CbnSample.GetItemData(m_CbnSample.GetCurSel()));
	const int regionIndex = m_CbnRegion.GetCurSel();
	if(regionIndex < 0 || regionIndex >= static_cast<int>(m_regions.size()))
		return;

	const auto &region = m_regions[regionIndex];
	const int startSelection = m_CbnRegionStart.GetCurSel();
	const int endSelection = m_CbnRegionEnd.GetCurSel();
	if(startSelection < 0 || endSelection < 0)
		return;

	ModCommand::NOTE newStartNote = static_cast<ModCommand::NOTE>(m_CbnRegionStart.GetItemData(startSelection));
	ModCommand::NOTE newEndNote = static_cast<ModCommand::NOTE>(m_CbnRegionEnd.GetItemData(endSelection));
	if(newStartNote > newEndNote)
		std::swap(newStartNote, newEndNote);

	// Clear notes that are in the old region but not in the new region (extend neighbouring regions)
	const ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	for(ModCommand::NOTE note = region.startNote; note <= region.endNote; note++)
	{
		if(KeyboardMap[note - NOTE_MIN] == selectedSample)
		{
			if(note < newStartNote && region.startNote > m_minNote)
				KeyboardMap[note - NOTE_MIN] = KeyboardMap[region.startNote - 1 - NOTE_MIN];
			else if(note > newEndNote && region.endNote < m_maxNote)
				KeyboardMap[note - NOTE_MIN] = KeyboardMap[region.endNote + 1 - NOTE_MIN];
			else if(note < newStartNote || note > newEndNote)
				KeyboardMap[note - NOTE_MIN] = pIns ? pIns->Keyboard[note - NOTE_MIN] : 0;
		}
	}

	// Set notes that are in the new region but not in the old region
	for(ModCommand::NOTE note = newStartNote; note <= newEndNote; note++)
	{
		if(note < region.startNote || note > region.endNote)
			KeyboardMap[note - NOTE_MIN] = selectedSample;
	}

	OnUpdateKeyboard();
	UpdateRegionControls(newStartNote);
	UpdateRegionStartEndSelection();  // In case regions were merged or split due to this change
	m_Keyboard.Invalidate(false);
}


void CSampleMapDlg::OnOK()
{
	ModInstrument *pIns = m_sndFile.Instruments[m_nInstrument];
	if(pIns)
	{
		bool modified = false;
		for(ModCommand::NOTE i = m_minNote - NOTE_MIN; i <= m_maxNote - NOTE_MIN; i++)
		{
			if(KeyboardMap[i] != pIns->Keyboard[i])
			{
				pIns->Keyboard[i] = KeyboardMap[i];
				modified = true;
			}
		}
		if(modified)
		{
			ResizableDialog::OnOK();
			return;
		}
	}
	ResizableDialog::OnCancel();
}


////////////////////////////////////////////////////////////////////////////////////////////
// Edit history dialog

UI_MESSAGE_MAP_BEGIN(CEditHistoryDlg, ResizableDialog)
	UI_COMMAND(IDC_BTN_CLEAR,	&CEditHistoryDlg::OnClearHistory)
UI_MESSAGE_MAP_END()


CEditHistoryDlg::CEditHistoryDlg(Wnd *parent, CModDoc &modDoc)
	: ResizableDialog{IDD_EDITHISTORY, parent}
	, m_modDoc{modDoc}
{
}


bool CEditHistoryDlg::OnInitDialog()
{
	ResizableDialog::OnInitDialog();

	mpt::ustring s;
	uint64 totalTime = 0;
	const auto &editHistory = m_modDoc.GetSoundFile().GetFileHistory();
	const bool isEmpty = editHistory.empty();

	for(const auto &entry : editHistory)
	{
		totalTime += entry.openTime;
		// Date
		mpt::ustring sDate = mpt::ustring(UL_("<unknown date>"));
		if(entry.HasValidDate())
		{
			const int64 unixSeconds = (m_modDoc.GetSoundFile().GetTimezoneInternal() == mpt::Date::LogicalTimezone::UTC)
				? mpt::chrono::default_system_clock::to_unix_seconds(mpt::Date::default_from_UTC(mpt::Date::interpret_as_timezone<mpt::Date::LogicalTimezone::UTC>(entry.loadDate)))
				: FindUnixSecondsFromLocal(entry.loadDate);
			sDate = Util::FormatLocalTime(unixSeconds, "%d %b %Y, %H:%M:%S");
		}
		// Time + stuff
		uint32 duration = mpt::saturate_round<uint32>(entry.openTime / HISTORY_TIMER_PRECISION);
		s += MPT_UFORMAT("Loaded {}, open for {}h {}m {}s\r\n")(
			sDate, mpt::ufmt::dec(duration / 3600), mpt::ufmt::dec0<2>((duration / 60) % 60), mpt::ufmt::dec0<2>(duration % 60));
	}
	if(isEmpty)
	{
		s = UL_("No information available about the previous edit history of this module.");
	}
	SetDlgItemText(IDC_EDIT_HISTORY, s);

	// Total edit time
	s.clear();
	if(totalTime)
	{
		totalTime = mpt::saturate_round<uint64>(totalTime / HISTORY_TIMER_PRECISION);

		s = ui::Format(UL_("Total edit time: %lluh %02llum %02llus (%zu session%s)"), totalTime / 3600, (totalTime / 60) % 60, totalTime % 60, editHistory.size(), (editHistory.size() != 1) ? UL_("s") : UL_(""));
		SetDlgItemText(IDC_TOTAL_EDIT_TIME, s);
		// Window title
		s = ui::Format(UL_("Edit History for %s"), m_modDoc.GetTitle().c_str());
		SetWindowText(s);
	}
	// Enable or disable Clear button
	GetDlgItem(IDC_BTN_CLEAR)->EnableWindow(isEmpty ? false : true);

	return true;
}


void CEditHistoryDlg::OnClearHistory()
{
	if(!m_modDoc.GetSoundFile().GetFileHistory().empty())
	{
		m_modDoc.GetSoundFile().GetFileHistory().clear();
		m_modDoc.SetModified();
		OnInitDialog();
	}
}


/////////////////////////////////////////////////////////////////////////
// Generic input dialog

void CInputDlg::DoDataExchange(DataExchange* pDX)
{
	DialogBase::DoDataExchange(pDX);
	if(m_minValueInt == m_maxValueInt && m_minValueDbl == m_maxValueDbl)
		pDX->BindControl(IDC_EDIT1, m_edit);
	else
		pDX->BindControl(IDC_EDIT1, m_spinner);
}


CInputDlg::CInputDlg(Wnd *parent, const mpt::ustring &desc, const mpt::ustring &defaultString, int32 maxLength, double minValDbl, double maxValDbl, double defaultDbl, int32 minValInt, int32 maxValInt, int32 defaultInt)
	: DialogBase{IDD_INPUT, parent}
	, m_description{desc}
	, m_minValueDbl{minValDbl}
	, m_maxValueDbl{maxValDbl}
	, m_minValueInt{minValInt}
	, m_maxValueInt{maxValInt}
	, m_maxLength{maxLength}
	, resultAsInt{defaultInt}
	, resultAsDouble{defaultDbl}
	, resultAsString{defaultString}
{
}


bool CInputDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();
	SetDlgItemText(IDC_PROMPT, m_description);

	// Get all current control sizes and positions
	Rect windowRect, labelRect, inputRect, okRect, cancelRect;
	GetWindowRect(windowRect);
	GetDlgItem(IDC_PROMPT)->GetWindowRect(labelRect);
	GetDlgItem(IDC_EDIT1)->GetWindowRect(inputRect);
	GetDlgItem(IDOK)->GetWindowRect(okRect);
	GetDlgItem(IDCANCEL)->GetWindowRect(cancelRect);
	ScreenToClient(labelRect);
	ScreenToClient(inputRect);
	ScreenToClient(okRect);
	ScreenToClient(cancelRect);

	// Find out how big our label shall be
	Rect textRect(0,0,0,0);
	{
		ui::Painter dc;
		dc.SetFont(GetFont());
		dc.DrawText(m_description, textRect, ui::TextCalcRect);
	}
	if(textRect.right < 320) textRect.right = 320;
	const int windowWidth = windowRect.Width() - labelRect.Width() + textRect.right;
	const int windowHeight = windowRect.Height() - labelRect.Height() + textRect.bottom;

	// Resize and move all controls
	GetDlgItem(IDC_PROMPT)->SetWindowPos(nullptr, 0, 0, textRect.right, textRect.bottom, ui::PosNoMove | ui::PosNoZOrder);
	GetDlgItem(IDC_EDIT1)->SetWindowPos(nullptr, inputRect.left, labelRect.top + textRect.bottom + (inputRect.top - labelRect.bottom), textRect.right, inputRect.Height(), ui::PosNoZOrder);
	GetDlgItem(IDOK)->SetWindowPos(nullptr, windowWidth - (windowRect.Width() - okRect.left), windowHeight - (windowRect.Height() - okRect.top), 0, 0, ui::PosNoSize | ui::PosNoZOrder);
	GetDlgItem(IDCANCEL)->SetWindowPos(nullptr, windowWidth - (windowRect.Width() - cancelRect.left), windowHeight - (windowRect.Height() - cancelRect.top), 0, 0, ui::PosNoSize | ui::PosNoZOrder);
	SetWindowPos(nullptr, 0, 0, windowWidth, windowHeight, ui::PosNoMove | ui::PosNoZOrder);

	if(m_minValueInt != m_maxValueInt)
	{
		// Numeric (int)
		m_spinner.SetRange32(m_minValueInt, m_maxValueInt);
		m_spinner.SetPos(resultAsInt);
	} else if(m_minValueDbl != m_maxValueDbl)
	{
		// Numeric (double)
		m_spinner.range(m_minValueDbl, m_maxValueDbl);
		m_spinner.SetAllowFractions(true);
		m_spinner.SetValue(resultAsDouble);
	} else
	{
		// Text
		if(m_maxLength > 0)
			m_edit.LimitText(m_maxLength);
		SetDlgItemText(IDC_EDIT1, resultAsString);
	}

	return true;
}


void CInputDlg::OnOK()
{
	DialogBase::OnOK();
	GetDlgItemText(IDC_EDIT1, resultAsString);
	resultAsInt = static_cast<int32>(GetDlgItemInt(IDC_EDIT1));
	Limit(resultAsInt, m_minValueInt, m_maxValueInt);
	if(m_minValueDbl != m_maxValueDbl)
		resultAsDouble = m_spinner.GetValue();
	Limit(resultAsDouble, m_minValueDbl, m_maxValueDbl);
}


///////////////////////////////////////////////////////////////////////////////////////
// Messagebox with 'don't show again'-option.

struct MsgBoxHidableMessage
{
	const mpt::uchar *mainTitle;
	const mpt::uchar *message;
	const uint32 mask;
	const bool defaultDontShowAgainStatus; // true for don't show again, false for show again.
};

static constexpr MsgBoxHidableMessage HidableMessages[] =
{
	{ UL_("Compatibility Notice"), UL_("The first two bytes of oneshot samples are silenced for ProTracker compatibility."), 1, true },
	{ UL_("Compatibility Hint"), UL_("To create IT files without OpenMPT-specific extensions included, try compatibility export from File menu."), 1 << 1, true },
	{ UL_("Compatibility Hint"), UL_("To create XM files without OpenMPT-specific extensions included, try compatibility export from File menu."), 1 << 3, true },
	{ UL_("Compatibility Notice"), UL_("The exported file will not contain any of OpenMPT's file format hacks."), 1 << 4, true },
};

static_assert(mpt::array_size<decltype(HidableMessages)>::size == enMsgBoxHidableMessage_count);

// Messagebox with 'don't show this again'-checkbox. Uses parameter 'enMsg'
// to get the needed information from message array, and updates the variable that
// controls the show/don't show-flags.
void MsgBoxHidable(enMsgBoxHidableMessage enMsg)
{
	const auto &msg = HidableMessages[enMsg];
	if((TrackerSettings::Instance().gnMsgBoxVisiblityFlags & msg.mask) == 0)
		return;

#if MPT_WINNT_AT_LEAST(MPT_WIN_VISTA) && defined(UNICODE)
	if(CTaskDialog::IsSupported()
	   && !(mpt::OS::Windows::IsWine() && theApp.GetWineVersion()->IsBefore(mpt::osinfo::windows::wine::version{3, 13, 0})))
	{
		CTaskDialog taskDialog(msg.message, msg.mainTitle ? mpt::ustring{msg.mainTitle} : mpt::ustring{}, AfxGetAppName(), TDCBF_OK_BUTTON);
		taskDialog.SetVerificationCheckboxText(UL_("Do not show this message again"));
		taskDialog.SetVerificationCheckbox(msg.defaultDontShowAgainStatus);
		taskDialog.DoModal();

		if(taskDialog.GetVerificationCheckboxState())
			TrackerSettings::Instance().gnMsgBoxVisiblityFlags &= ~msg.mask;
		else
			TrackerSettings::Instance().gnMsgBoxVisiblityFlags |= msg.mask;
	} else
#endif
	{
		if(Reporting::Confirm(msg.message + mpt::ustring(UL_("\n\nShow this message again?")), msg.mainTitle ? mpt::ustring{msg.mainTitle} : mpt::ustring{}, msg.defaultDontShowAgainStatus) == cnfNo)
			TrackerSettings::Instance().gnMsgBoxVisiblityFlags &= ~msg.mask;
		else
			TrackerSettings::Instance().gnMsgBoxVisiblityFlags |= msg.mask;
	}
}


/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////


void AppendNotesToControl(ComboBox& combobox, ModCommand::NOTE noteStart, ModCommand::NOTE noteEnd)
{
	const ModCommand::NOTE upperLimit = std::min(ModCommand::NOTE(NOTE_MAX), noteEnd);
	for(ModCommand::NOTE note = noteStart; note <= upperLimit; note++)
		combobox.SetItemData(combobox.AddString(mpt::ToUnicode(CTrackerSoundFile::GetNoteName(note, CTrackerSoundFile::GetDefaultNoteNames()))), note);
}


void AppendNotesToControlEx(ComboBox& combobox, const CTrackerSoundFile &sndFile, INSTRUMENTINDEX nInstr, ModCommand::NOTE noteStart, ModCommand::NOTE noteEnd)
{
	bool addSpecial = noteStart == noteEnd;
	if(noteStart == noteEnd)
	{
		noteStart = sndFile.GetModSpecifications().noteMin;
		noteEnd = sndFile.GetModSpecifications().noteMax;
	}
	for(ModCommand::NOTE note = noteStart; note <= noteEnd; note++)
	{
		combobox.SetItemData(combobox.AddString(mpt::ToUnicode(sndFile.GetNoteName(note, nInstr))), note);
	}
	if(addSpecial)
	{
		for(ModCommand::NOTE note = NOTE_MIN_SPECIAL - 1; note++ < NOTE_MAX_SPECIAL;)
		{
			if(sndFile.GetModSpecifications().HasNote(note))
				combobox.SetItemData(combobox.AddString(szSpecialNoteNamesMPT[note - NOTE_MIN_SPECIAL]), note);
		}
	}
}


OPENMPT_NAMESPACE_END
