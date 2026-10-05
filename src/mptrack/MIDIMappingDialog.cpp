/*
 * MIDIMappingDialog.cpp
 * ---------------------
 * Purpose: Implementation of OpenMPT's MIDI mapping dialog, for mapping incoming MIDI messages to plugin parameters.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/MIDIMappingDialog.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "MIDIMappingDialog.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "resource.h"
#include "WindowMessages.h"
#include "../common/mptStringBuffer.h"
#include "../soundlib/MIDIEvents.h"
#include "../soundlib/mod_specifications.h"
#include "../soundlib/plugins/PlugInterface.h"
#include "PluginUi.h"


OPENMPT_NAMESPACE_BEGIN


CMIDIMappingDialog::CMIDIMappingDialog(Wnd *pParent, CTrackerSoundFile &sndfile)
	: ResizableDialog{IDD_MIDIPARAMCONTROL, pParent}
	, m_sndFile{sndfile}
	, m_rMIDIMapper{m_sndFile.GetMIDIMapper()}
{
	CMainFrame::GetInputHandler()->Bypass(true);
	oldMIDIRecondWnd = CMainFrame::GetMainFrame()->GetMidiRecordWnd();
}


CMIDIMappingDialog::~CMIDIMappingDialog()
{
	CMainFrame::GetMainFrame()->SetMidiRecordWnd(oldMIDIRecondWnd);
	CMainFrame::GetInputHandler()->Bypass(false);
}


void CMIDIMappingDialog::DoDataExchange(DataExchange *pDX)
{
	ResizableDialog::DoDataExchange(pDX);
	pDX->BindControl(IDC_COMBO_CONTROLLER, m_ControllerCBox);
	pDX->BindControl(IDC_COMBO_PLUGIN, m_PluginCBox);
	pDX->BindControl(IDC_COMBO_PARAM, m_PlugParamCBox);
	pDX->BindControl(IDC_LIST1, m_List);
	pDX->BindControl(IDC_COMBO_CHANNEL, m_ChannelCBox);
	pDX->BindControl(IDC_COMBO_EVENT, m_EventCBox);
	pDX->BindControl(IDC_SPINMOVEMAPPING, m_SpinMoveMapping);
}


UI_MESSAGE_MAP_BEGIN(CMIDIMappingDialog, ResizableDialog)
	UI_NOTIFY(ui::ListItemChanged, IDC_LIST1, &CMIDIMappingDialog::OnSelectionChanged)
	UI_NOTIFY(ui::CheckListChange, IDC_LIST1, &CMIDIMappingDialog::OnCheckChanged)
	UI_COMMAND(IDC_CHECKACTIVE, &CMIDIMappingDialog::OnBnClickedCheckactive)
	UI_COMMAND(IDC_CHECKCAPTURE, &CMIDIMappingDialog::OnBnClickedCheckCapture)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO_CONTROLLER, &CMIDIMappingDialog::OnCbnSelchangeComboController)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO_CHANNEL, &CMIDIMappingDialog::OnCbnSelchangeComboChannel)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO_PLUGIN, &CMIDIMappingDialog::OnCbnSelchangeComboPlugin)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO_PARAM, &CMIDIMappingDialog::OnCbnSelchangeComboParam)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO_EVENT, &CMIDIMappingDialog::OnCbnSelchangeComboEvent)
	UI_COMMAND(IDC_BUTTON_ADD, &CMIDIMappingDialog::OnBnClickedButtonAdd)
	UI_COMMAND(IDC_BUTTON_REPLACE, &CMIDIMappingDialog::OnBnClickedButtonReplace)
	UI_COMMAND(IDC_BUTTON_REMOVE, &CMIDIMappingDialog::OnBnClickedButtonRemove)
	UI_MESSAGE(MSG_MOD_MIDIMSG,		&CMIDIMappingDialog::OnMidiMsg)
	UI_NOTIFY(ui::SpinDeltaPos, IDC_SPINMOVEMAPPING, &CMIDIMappingDialog::OnDeltaposSpinmovemapping)
	UI_COMMAND(IDC_CHECK_PATRECORD, &CMIDIMappingDialog::OnBnClickedCheckPatRecord)
UI_MESSAGE_MAP_END()


LResult CMIDIMappingDialog::OnMidiMsg(WParam dwMidiDataParam, LParam)
{
	uint32 midiData = static_cast<uint32>(dwMidiDataParam);
	if(IsDlgButtonChecked(IDC_CHECK_MIDILEARN))
	{
		for(int i = 0; i < m_EventCBox.GetCount(); i++)
		{
			if(static_cast<MIDIEvents::EventType>(m_EventCBox.GetItemData(i)) == MIDIEvents::GetTypeFromEvent(midiData))
			{
				m_ChannelCBox.SetCurSel(1 + MIDIEvents::GetChannelFromEvent(midiData));
				m_EventCBox.SetCurSel(i);
				if(MIDIEvents::GetTypeFromEvent(midiData) == MIDIEvents::evControllerChange)
				{
					uint8 cc = MIDIEvents::GetDataByte1FromEvent(midiData);
					if(m_lastCC >= 32 || cc != m_lastCC + 32)
					{
						// Ignore second CC message of 14-bit CC.
						m_ControllerCBox.SetCurSel(cc);
					}
					m_lastCC = cc;
				}
				OnCbnSelchangeComboChannel();
				OnCbnSelchangeComboEvent();
				OnCbnSelchangeComboController();
				break;
			}
		}
	}
	return 1;
}


bool CMIDIMappingDialog::OnInitDialog()
{
	ResizableDialog::OnInitDialog();

	// Add events
	m_EventCBox.SetItemData(m_EventCBox.AddString(UL_("Controller Change")), MIDIEvents::evControllerChange);
	m_EventCBox.SetItemData(m_EventCBox.AddString(UL_("Polyphonic Aftertouch")), MIDIEvents::evPolyAftertouch);
	m_EventCBox.SetItemData(m_EventCBox.AddString(UL_("Channel Aftertouch")), MIDIEvents::evChannelAftertouch);
	
	// Add controller names
	mpt::ustring s;
	for(uint8 i = MIDIEvents::MIDICC_start; i <= MIDIEvents::MIDICC_end; i++)
	{
		s = ui::Format(UL_("%3u "), i);
		s += mpt::ToUnicode(mpt::Charset::UTF8, MIDIEvents::MidiCCNames[i]);
		m_ControllerCBox.AddString(s);
	}

	// Add plugin names
	m_PluginCBox.Update(PluginComboBox::Config{PluginComboBox::ShowEmptySlots}, m_sndFile);

	// Initialize mapping table
	static constexpr CListCtrlEx::Header headers[] =
	{
		{ UL_("Channel"),            58,  ui::ListColumnLeft },
		{ UL_("Event / Controller"), 176, ui::ListColumnLeft },
		{ UL_("Plugin"),             120, ui::ListColumnLeft },
		{ UL_("Parameter"),          120, ui::ListColumnLeft },
		{ UL_("Capture"),            40,  ui::ListColumnLeft },
		{ UL_("Pattern Record"),     40,  ui::ListColumnLeft }
	};
	m_List.SetHeaders(headers);
	m_List.SetExtendedStyle(m_List.GetExtendedStyle() | ui::ListStyleCheckBoxes | ui::ListStyleFullRowSelect);

	// Add directives to list
	for(size_t i = 0; i < m_rMIDIMapper.GetCount(); i++)
	{
		InsertItem(m_rMIDIMapper.GetDirective(i), int(i));
	}

	if(m_rMIDIMapper.GetCount() > 0 && m_Setting.IsDefault())
	{
		SelectItem(0);
		OnSelectionChanged();
	} else
	{
		UpdateDialog();
	}

	GetDlgItem(IDC_CHECK_PATRECORD)->EnableWindow((m_sndFile.GetType() == MOD_TYPE_MPT) ? true : false);

	CMainFrame::GetMainFrame()->SetMidiRecordWnd(this);

	CheckDlgButton(IDC_CHECK_MIDILEARN, ui::CheckOn);

	return true;  // return true unless you set the focus to a control
}


int CMIDIMappingDialog::InsertItem(const CMIDIMappingDirective &m, int insertAt)
{
	mpt::ustring s;
	if(m.GetAnyChannel())
		s = UL_("Any");
	else
		s = ui::Format(UL_("Ch %u"), m.GetChannel());

	insertAt = m_List.InsertItem(insertAt, s);
	if(insertAt == -1)
		return -1;
	m_List.SetCheck(insertAt, m.IsActive() ? true : false);

	switch(m.GetEvent())
	{
	case MIDIEvents::evControllerChange:
		s = ui::Format(UL_("CC %u: "), m.GetController());
		if(m.GetController() <= MIDIEvents::MIDICC_end) s += mpt::ToUnicode(mpt::Charset::UTF8, MIDIEvents::MidiCCNames[m.GetController()]);
		break;
	case MIDIEvents::evPolyAftertouch:
		s = UL_("Polyphonic Aftertouch"); break;
	case MIDIEvents::evChannelAftertouch:
		s = UL_("Channel Aftertouch"); break;
	default:
		s = ui::Format(UL_("0x%02X"), m.GetEvent()); break;
	}
	m_List.SetItemText(insertAt, 1, s);

	const PLUGINDEX plugindex = m.GetPlugIndex();
	if(plugindex > 0 && plugindex < MAX_MIXPLUGINS)
	{
		const SNDMIXPLUGIN &plug = m_sndFile.m_MixPlugins[plugindex - 1];
		s = ui::Format(UL_("FX%u: "), plugindex);
		s += mpt::ToUnicode(plug.GetName());
		m_List.SetItemText(insertAt, 2, s);
		if(plug.pMixPlugin != nullptr)
			s = PluginUi(*plug.pMixPlugin).GetFormattedParamName(m.GetParamIndex());
		else
			s.clear();
		m_List.SetItemText(insertAt, 3, s);
	}
	m_List.SetItemText(insertAt, 4, m.GetCaptureMIDI() ? UL_("Capt") : UL_(""));
	m_List.SetItemText(insertAt, 5, m.GetAllowPatternEdit() ? UL_("Rec") : UL_(""));

	return insertAt;
}


void CMIDIMappingDialog::SelectItem(int i)
{
	m_List.SetItemState(i, ui::ListItemSelected, ui::ListItemSelected);
	m_List.SetSelectionMark(i);
}


void CMIDIMappingDialog::UpdateDialog(int selItem)
{
	CheckDlgButton(IDC_CHECKACTIVE, m_Setting.IsActive() ? ui::CheckOn : ui::CheckOff);
	CheckDlgButton(IDC_CHECKCAPTURE, m_Setting.GetCaptureMIDI() ? ui::CheckOn : ui::CheckOff);
	CheckDlgButton(IDC_CHECK_PATRECORD, m_Setting.GetAllowPatternEdit() ? ui::CheckOn : ui::CheckOff);

	m_ChannelCBox.SetCurSel(m_Setting.GetChannel());

	m_EventCBox.SetCurSel(-1);
	for(int i = 0; i < m_EventCBox.GetCount(); i++)
	{
		if(m_EventCBox.GetItemData(i) == m_Setting.GetEvent())
		{
			m_EventCBox.SetCurSel(i);
			break;
		}
	}

	m_ControllerCBox.SetCurSel(m_Setting.GetController());
	if(PLUGINDEX plug = m_Setting.GetPlugIndex(); plug > 0)
		m_PluginCBox.SetSelection(plug - 1);
	else
		m_PluginCBox.SetSelection(PLUGINDEX_INVALID);
	m_PlugParamCBox.SetCurSel(m_Setting.GetParamIndex());

	UpdateEvent();
	UpdateParameters();

	bool enableMover = selItem >= 0;
	if(enableMover)
	{
		const bool previousEqual = (selItem > 0 && m_rMIDIMapper.AreOrderEqual(selItem - 1, selItem));
		const bool nextEqual = (selItem + 1 < m_List.GetItemCount() && m_rMIDIMapper.AreOrderEqual(selItem, selItem + 1));
		enableMover = previousEqual || nextEqual;
	}
	m_SpinMoveMapping.EnableWindow(enableMover);
}


void CMIDIMappingDialog::UpdateEvent()
{
	m_ControllerCBox.EnableWindow(m_Setting.GetEvent() == MIDIEvents::evControllerChange ? true : false);
	if(m_Setting.GetEvent() != MIDIEvents::evControllerChange)
		m_ControllerCBox.SetCurSel(0);
}


void CMIDIMappingDialog::UpdateParameters()
{
	m_PlugParamCBox.SetRedraw(false);
	m_PlugParamCBox.ResetContent();
	AddPluginParameternamesToCombobox(m_PlugParamCBox, m_sndFile.m_MixPlugins[m_Setting.GetPlugIndex() - 1]);
	m_PlugParamCBox.SetCurSel(m_Setting.GetParamIndex());
	m_PlugParamCBox.SetRedraw(true);
	m_PlugParamCBox.Invalidate();
}


void CMIDIMappingDialog::OnSelectionChanged(NotifyHeader *, LResult * /*pResult*/)
{
	const int i = m_List.GetSelectionMark();
	if(i < 0 || (size_t)i >= m_rMIDIMapper.GetCount()) return;
	m_Setting = m_rMIDIMapper.GetDirective(i);
	UpdateDialog(i);
}


void CMIDIMappingDialog::OnCheckChanged(NotifyHeader *pNMHDR, LResult * /*pResult*/)
{
	if(pNMHDR != nullptr)
	{
		const ui::ListCheckInfo *info = static_cast<const ui::ListCheckInfo *>(pNMHDR->extra);
		CMIDIMappingDirective m = m_rMIDIMapper.GetDirective(info->item);
		m.SetActive(info->isChecked);
		m_rMIDIMapper.SetDirective(info->item, m);
		SetModified();
		if(info->item == m_List.GetSelectionMark())
			CheckDlgButton(IDC_CHECKACTIVE, info->isChecked ? ui::CheckOn : ui::CheckOff);
	}
}


void CMIDIMappingDialog::OnBnClickedCheckactive()
{
	m_Setting.SetActive(IsDlgButtonChecked(IDC_CHECKACTIVE) == ui::CheckOn);
}


void CMIDIMappingDialog::OnBnClickedCheckCapture()
{
	m_Setting.SetCaptureMIDI(IsDlgButtonChecked(IDC_CHECKCAPTURE) == ui::CheckOn);
}


void CMIDIMappingDialog::OnBnClickedCheckPatRecord()
{
	m_Setting.SetAllowPatternEdit(IsDlgButtonChecked(IDC_CHECK_PATRECORD) == ui::CheckOn);
}


void CMIDIMappingDialog::OnCbnSelchangeComboController()
{
	m_Setting.SetController(m_ControllerCBox.GetCurSel());
}


void CMIDIMappingDialog::OnCbnSelchangeComboChannel()
{
	m_Setting.SetChannel(m_ChannelCBox.GetCurSel());
}


void CMIDIMappingDialog::OnCbnSelchangeComboPlugin()
{
	PLUGINDEX i = m_PluginCBox.GetSelection().value_or(PLUGINDEX_INVALID);
	if(i >= MAX_MIXPLUGINS)
		return;
	m_Setting.SetPlugIndex(i + 1);
	UpdateParameters();
}


void CMIDIMappingDialog::OnCbnSelchangeComboParam()
{
	m_Setting.SetParamIndex(m_PlugParamCBox.GetCurSel());
}


void CMIDIMappingDialog::OnCbnSelchangeComboEvent()
{
	m_Setting.SetEvent(static_cast<MIDIEvents::EventType>(m_EventCBox.GetItemData(m_EventCBox.GetCurSel())));
	UpdateEvent();
}


void CMIDIMappingDialog::OnBnClickedButtonAdd()
{
	if(m_sndFile.GetModSpecifications().MIDIMappingDirectivesMax <= m_rMIDIMapper.GetCount())
	{
		Reporting::Information(UL_("Maximum amount of MIDI Mapping directives reached."));
	} else
	{
		const size_t i = m_rMIDIMapper.AddDirective(m_Setting);
		SetModified();

		SelectItem(InsertItem(m_Setting, static_cast<int>(i)));
		OnSelectionChanged(nullptr, nullptr);
	}
}


void CMIDIMappingDialog::OnBnClickedButtonReplace()
{
	const int i = m_List.GetSelectionMark();
	if(i >= 0 && (size_t)i < m_rMIDIMapper.GetCount())
	{
		const size_t newIndex = m_rMIDIMapper.SetDirective(i, m_Setting);
		SetModified();

		m_List.DeleteItem(i);
		SelectItem(InsertItem(m_Setting, static_cast<int>(newIndex)));
		OnSelectionChanged(nullptr, nullptr);
	}
}


void CMIDIMappingDialog::OnBnClickedButtonRemove()
{
	int i = m_List.GetSelectionMark();
	if(i >= 0 && (size_t)i < m_rMIDIMapper.GetCount())
	{
		m_rMIDIMapper.RemoveDirective(i);
		SetModified();

		m_List.DeleteItem(i);
		if(m_List.GetItemCount() > 0)
		{
			if(i < m_List.GetItemCount())
				SelectItem(i);
			else
				SelectItem(i - 1);
		}
		i = m_List.GetSelectionMark();
		if(i >= 0 && (size_t)i < m_rMIDIMapper.GetCount())
			m_Setting = m_rMIDIMapper.GetDirective(i);

		OnSelectionChanged(nullptr, nullptr);
	}
}



void CMIDIMappingDialog::OnDeltaposSpinmovemapping(NotifyHeader *pNMHDR, LResult *pResult)
{
	const int index = m_List.GetSelectionMark();
	if(index < 0 || index >= m_List.GetItemCount()) return;

	const ui::SpinDelta *pNMUpDown = static_cast<const ui::SpinDelta *>(pNMHDR->extra);

	int newIndex = -1;
	if(pNMUpDown->delta < 0) //Up
	{
		if(index - 1 >= 0 && m_rMIDIMapper.AreOrderEqual(index-1, index))
		{
			newIndex = index - 1;
		}
	} else //Down
	{
		if(index + 1 < m_List.GetItemCount() && m_rMIDIMapper.AreOrderEqual(index, index+1))
		{
			newIndex = index + 1;
		}
	}

	if(newIndex != -1)
	{
		m_rMIDIMapper.Swap(size_t(newIndex), size_t(index));
		m_List.DeleteItem(index);
		InsertItem(m_rMIDIMapper.GetDirective(newIndex), newIndex);
		SelectItem(newIndex);
	}
	
	*pResult = 0;
}


mpt::ustring CMIDIMappingDialog::GetToolTipText(uint32 id, WindowHandle) const
{
	const mpt::uchar *text = UL_("");
	switch(id)
	{
	case IDC_CHECKCAPTURE:
		text = UL_("The event is not passed to any further MIDI mappings or recording facilities.");
		break;
	case IDC_CHECKACTIVE:
		text = UL_("The MIDI mapping is enabled and can be processed.");
		break;
	case IDC_CHECK_PATRECORD:
		text = UL_("Parameter changes are recorded into patterns as Parameter Control events.");
		break;
	case IDC_CHECK_MIDILEARN:
		text = UL_("Listens to incoming MIDI data to automatically fill in the appropriate data.");
		break;
	case IDC_SPINMOVEMAPPING:
		text = UL_("Change the processing order of the current selected MIDI mapping.");
		break;
	case IDC_COMBO_CHANNEL:
		text = UL_("The MIDI channel to listen on for this event.");
		break;
	case IDC_COMBO_EVENT:
		text = UL_("The MIDI event to listen for.");
		break;
	case IDC_COMBO_CONTROLLER:
		text = UL_("The MIDI controler to listen for.");
		break;
	}

	return text;
}


void CMIDIMappingDialog::SetModified()
{
	if(m_sndFile.GetpModDoc() != nullptr)
		m_sndFile.GetpModDoc()->SetModified();
}


OPENMPT_NAMESPACE_END
