/*
 * KeyConfigDlg.cpp
 * ----------------
 * Purpose: Implementation of OpenMPT's keyboard configuration dialog.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/KeyConfigDlg.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "KeyConfigDlg.h"
#include "CListCtrl.h"
#include "FileDialog.h"
#include "Mainfrm.h"
#include "Mptrack.h"
#include "InputHandler.h"
#include "Reporting.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "WindowMessages.h"
#include "../soundlib/mod_specifications.h"
#include "../soundlib/MIDIEvents.h"


OPENMPT_NAMESPACE_BEGIN


//***************************************************************************************//
// CCustEdit: customised Edit control to catch keypresses.
// (does what CHotKeyCtrl does,but better)
//***************************************************************************************//

UI_MESSAGE_MAP_BEGIN(CCustEdit, Edit)
	UI_MESSAGE(MSG_MOD_MIDIMSG, &CCustEdit::OnMidiMsg)
UI_MESSAGE_MAP_END()


LResult CCustEdit::OnMidiMsg(WParam dwMidiDataParam, LParam)
{
	if(!m_isFocussed)
		return 1;

	uint32 midiData = static_cast<uint32>(dwMidiDataParam);
	const auto byte1 = MIDIEvents::GetDataByte1FromEvent(midiData), byte2 = MIDIEvents::GetDataByte2FromEvent(midiData);
	switch(MIDIEvents::GetTypeFromEvent(midiData))
	{
	case MIDIEvents::evControllerChange:
		if(byte2 != 0)
		{
			SetKey(ModMidi, byte1);
			m_pOptKeyDlg->OnSetKeyChoice(this);
		}
		break;

	case MIDIEvents::evNoteOn:
	case MIDIEvents::evNoteOff:
		SetKey(ModMidi, byte1 | 0x80);
		m_pOptKeyDlg->OnSetKeyChoice(this);
		break;

	default:
		break;
	}

	return 1;
}


bool CCustEdit::PreTranslateMessage(int event)
{
	if(!m_bypassed)
	{
		if(event == FL_KEYBOARD)
		{
			SetKey(CInputHandler::GetModifierMask(), ui::KeyFromEvent());
			return true;  // Keypress handled, don't pass on message.
		} else if(event == FL_KEYUP)
		{
			//if a key has been released but custom edit box is empty, we have probably just
			//navigated into the box with TAB or SHIFT-TAB. No need to set keychoice.
			if(code != 0)
				m_pOptKeyDlg->OnSetKeyChoice(this);
		}
	}
	return Edit::PreTranslateMessage(event);
}


void CCustEdit::SetKey(FlagSet<Modifiers> inMod, uint32 inCode)
{
	mod = inMod;
	code = inCode;
	//Setup display
	SetWindowText(KeyCombination::GetKeyText(mod, code));
}


void CCustEdit::OnSetFocus(Wnd *pOldWnd)
{
	Edit::OnSetFocus(pOldWnd);
	// Lock the input handler
	CMainFrame::GetInputHandler()->Bypass(true);
	// Accept MIDI input
	CMainFrame::GetMainFrame()->SetMidiRecordWnd(this);

	m_isFocussed = true;
}


void CCustEdit::OnKillFocus(Wnd *pNewWnd)
{
	Edit::OnKillFocus(pNewWnd);
	//unlock the input handler
	CMainFrame::GetInputHandler()->Bypass(false);
	m_isFocussed = false;
	m_pOptKeyDlg->OnCancelKeyChoice(this);
}


//***************************************************************************************//
// COptionsKeyboard:
//
//***************************************************************************************//

static constexpr CListCtrlEx::Header KeyListHeaders[] =
{
	{UL_("Shortcut"),      274, ui::ListColumnLeft},
	{UL_("Assigned Keys"), 174, ui::ListColumnLeft},
};

UI_MESSAGE_MAP_BEGIN(COptionsKeyboard, PropertyPage)
	UI_NOTIFY(ui::ListSelChange, IDC_CHOICECOMBO,     &COptionsKeyboard::OnKeyChoiceSelect)
	UI_NOTIFY(ui::ListSelChange, IDC_KEYCATEGORY,     &COptionsKeyboard::OnCategorySelChanged)
	UI_NOTIFY(ui::EditUpdate, IDC_CHORDDETECTWAITTIME, &COptionsKeyboard::OnChordWaitTimeChanged)
	UI_COMMAND(IDC_BUTTON3,               &COptionsKeyboard::OnClearSearch)
	UI_COMMAND(IDC_BUTTON2,               &COptionsKeyboard::OnEnableFindHotKey)
	UI_COMMAND(IDC_BUTTON1,               &COptionsKeyboard::OnListenForKeys)
	UI_COMMAND(IDC_DELETE,                &COptionsKeyboard::OnDeleteKeyChoice)
	UI_COMMAND(IDC_RESTORE,               &COptionsKeyboard::OnRestoreKeyChoice)
	UI_COMMAND(IDC_LOAD,                  &COptionsKeyboard::OnLoad)
	UI_COMMAND(IDC_SAVE,                  &COptionsKeyboard::OnSave)
	UI_COMMAND(IDC_CHECKKEYDOWN,          &COptionsKeyboard::OnCheck)
	UI_COMMAND(IDC_CHECKKEYHOLD,          &COptionsKeyboard::OnCheck)
	UI_COMMAND(IDC_CHECKKEYUP,            &COptionsKeyboard::OnCheck)
	UI_COMMAND(IDC_NOTESREPEAT,           &COptionsKeyboard::OnToggleNotesRepeat)
	UI_COMMAND(IDC_RESTORE_KEYMAP,        &COptionsKeyboard::OnRestoreDefaultKeymap)
	UI_COMMAND(ID_KEYPRESET_MPT,          &COptionsKeyboard::OnRestoreMPTKeymap)
	UI_COMMAND(ID_KEYPRESET_IT,           &COptionsKeyboard::OnRestoreITKeymap)
	UI_COMMAND(ID_KEYPRESET_FT2,          &COptionsKeyboard::OnRestoreFT2Keymap)
	UI_NOTIFY(ui::EditChange, IDC_FIND,                &COptionsKeyboard::OnSearchTermChanged)
	UI_NOTIFY(ui::EditSetFocus, IDC_FINDHOTKEY,        &COptionsKeyboard::OnClearHotKey)
	UI_NOTIFY(ui::ListItemChanged, IDC_COMMAND_LIST, &COptionsKeyboard::OnCommandKeySelChanged)
	UI_NOTIFY(ui::ListDblClick, IDC_COMMAND_LIST, &COptionsKeyboard::OnListenForKeysFromList)
#if MPT_WINNT_AT_LEAST(MPT_WIN_VISTA)
	UI_NOTIFY(BCN_DROPDOWN, IDC_RESTORE_KEYMAP, &COptionsKeyboard::OnRestoreKeymapDropdown)
#endif
UI_MESSAGE_MAP_END()


void COptionsKeyboard::DoDataExchange(DataExchange *pDX)
{
	PropertyPage::DoDataExchange(pDX);
	pDX->BindControl(IDC_KEYCATEGORY, m_cmbCategory);
	pDX->BindControl(IDC_COMMAND_LIST, m_lbnCommandKeys);
	pDX->BindControl(IDC_CHOICECOMBO, m_cmbKeyChoice);
	pDX->BindControl(IDC_CHORDDETECTWAITTIME, m_eChordWaitTime);
	pDX->BindControl(IDC_CUSTHOTKEY, m_eCustHotKey);
	pDX->BindControl(IDC_FINDHOTKEY, m_eFindHotKey);
	pDX->BindControl(IDC_CHECKKEYDOWN, m_bKeyDown);
	pDX->BindControl(IDC_CHECKKEYHOLD, m_bKeyHold);
	pDX->BindControl(IDC_CHECKKEYUP, m_bKeyUp);
	pDX->BindControl(IDC_FIND, m_eFind);
	pDX->BindControl(IDC_STATIC1, m_warnIconCtl);
	pDX->BindControl(IDC_KEYREPORT, m_warnText);
	pDX->BindControl(IDC_RESTORE_KEYMAP, m_restoreDefaultButton);
}


COptionsKeyboard::COptionsKeyboard() : PropertyPage{IDD_OPTIONS_KEYBOARD} { }


bool COptionsKeyboard::OnSetActive()
{
	CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_KEYBOARD;
	return PropertyPage::OnSetActive();
}


bool COptionsKeyboard::OnInitDialog()
{
	PropertyPage::OnInitDialog();
	m_fullPathName = TrackerSettings::Instance().m_szKbdFile;

	m_localCmdSet = std::make_unique<CCommandSet>();
	m_localCmdSet->Copy(*CMainFrame::GetInputHandler()->m_activeCommandSet);

	m_lbnCommandKeys.SetExtendedStyle(m_lbnCommandKeys.GetExtendedStyle() | ui::ListStyleFullRowSelect);
	m_lbnCommandKeys.SetHeaders(KeyListHeaders);

	//Fill category combo and automatically selects first category
	DefineCommandCategories();
	for(size_t c = 0; c < commandCategories.size(); c++)
	{
		if(!commandCategories[c].name.empty() && !commandCategories[c].commandRanges.empty())
			m_cmbCategory.SetItemData(m_cmbCategory.AddString(commandCategories[c].name), c);
	}
	m_cmbCategory.SetCurSel(0);
	UpdateDialog();

	m_eCustHotKey.SetOwner(*this);
	m_eFindHotKey.SetOwner(*this);

	EnableKeyChoice(false);

	m_eChordWaitTime.SetWindowText(mpt::ufmt::val(TrackerSettings::Instance().gnAutoChordWaitTime.Get()));
	return true;
}


void COptionsKeyboard::DefineCommandCategories()
{
	{
		auto &commands = commandCategories.emplace_back(UL_("Global"), kCtxAllContexts).commandRanges;
		commands.emplace_back(kcStartFile, kcEndFile, UL_("File"));
		commands.emplace_back(kcStartPlayCommands, kcEndPlayCommands, UL_("Player"));
		commands.emplace_back(kcStartEditCommands, kcEndEditCommands, UL_("Edit"));
		commands.emplace_back(kcStartView, kcEndView, UL_("View"));
		commands.emplace_back(kcStartMisc, kcEndMisc, UL_("Miscellaneous"));
		commands.emplace_back(kcDummyShortcut, kcDummyShortcut, UL_(""));
	}

	commandCategories.emplace_back(UL_("  General [Top]"), kCtxCtrlGeneral);
	commandCategories.emplace_back(UL_("  General [Bottom]"), kCtxViewGeneral);
	commandCategories.emplace_back(UL_("  Pattern Editor [Top]"), kCtxCtrlPatterns);

	{
		auto &commands = commandCategories.emplace_back(UL_("  Pattern Editor - Order List"), kCtxCtrlOrderlist).commandRanges;
		commands.emplace_back(kcStartOrderlistEdit, kcEndOrderlistEdit, UL_("Edit"));
		commands.emplace_back(kcStartOrderlistNavigation, kcEndOrderlistNavigation, UL_("Navigation"));
		commands.emplace_back(kcStartOrderlistNum, kcEndOrderlistNum, UL_("Pattern Entry"));
		commands.emplace_back(kcStartOrderlistMisc, kcEndOrderlistMisc, UL_("Miscellaneous"));
	}

	{
		auto &commands = commandCategories.emplace_back(UL_("  Pattern Editor - Quick Channel Settings"), kCtxChannelSettings).commandRanges;
		commands.emplace_back(kcStartChnSettingsCommands, kcEndChnSettingsCommands, UL_(""));
	}

	{
		auto &commands = commandCategories.emplace_back(UL_("    Pattern Editor - General"), kCtxViewPatterns).commandRanges;
		commands.emplace_back(kcStartPlainNavigate, kcEndPlainNavigate, UL_("Navigation"));
		commands.emplace_back(kcStartJumpSnap, kcEndJumpSnap, UL_("Jump"));
		commands.emplace_back(kcStartHomeEnd, kcEndHomeEnd, UL_("Go To"));
		commands.emplace_back(kcStartGotoColumn, kcEndGotoColumn, UL_("Go To Column"));
		commands.emplace_back(kcPrevPattern, kcNextSequence, UL_("Order List Navigation"));
		commands.emplace_back(kcStartPatternScrolling, kcEndPatternScrolling, UL_("Scrolling"));
		commands.emplace_back(kcStartSelect, kcEndSelect, UL_("Selection"));
		commands.emplace_back(kcStartPatternClipboard, kcEndPatternClipboard, UL_("Clipboard"));
		commands.emplace_back(kcClearRow, kcInsertWholeRowGlobal, UL_("Clear / Insert"));
		commands.emplace_back(kcStartChannelKeys, kcEndChannelKeys, UL_("Channels"));
		commands.emplace_back(kcBeginTranspose, kcEndTranspose, UL_("Transpose"));
		commands.emplace_back(kcStartPatternEditMisc, kcEndPatternEditMisc, UL_("Edit"));
		commands.emplace_back(kcStartPatternMisc, kcEndPatternMisc, UL_("Miscellaneous"));
	}

	{
		auto &commands = commandCategories.emplace_back(UL_("        Pattern Editor - Note Column"), kCtxViewPatternsNote).commandRanges;
		commands.emplace_back(kcVPStartNotes, kcVPEndNotes, UL_("Note Entry"));
		commands.emplace_back(kcSetOctave0, kcSetOctave9, UL_("Octave Entry"));
		commands.emplace_back(kcStartNoteMisc, kcEndNoteMisc, UL_("Miscellaneous"));
	}

	{
		auto &commands = commandCategories.emplace_back(UL_("        Pattern Editor - Instrument Column"), kCtxViewPatternsIns).commandRanges;
		commands.emplace_back(kcSetIns0, kcSetIns9, UL_("Instrument Entry"));
	}

	{
		auto &commands = commandCategories.emplace_back(UL_("        Pattern Editor - Volume Column"), kCtxViewPatternsVol).commandRanges;
		commands.emplace_back(kcStartVolumeDigits, kcEndVolumeDigits, UL_("Volume Entry"));
		commands.emplace_back(kcStartVolumeCommands, kcEndVolumeCommands, UL_("Volume Command Entry"));
	}

	{
		auto &commands = commandCategories.emplace_back(UL_("        Pattern Editor - Effect Column"), kCtxViewPatternsFX).commandRanges;
		commands.emplace_back(kcSetFXStart, kcSetFXEnd, UL_("Effect Command Entry"));
	}

	{
		auto &commands = commandCategories.emplace_back(UL_("        Pattern Editor - Effect Parameter Column"), kCtxViewPatternsFXparam).commandRanges;
		commands.emplace_back(kcSetFXParam0, kcSetFXParamF, UL_("Parameter Digit Entry"));
	}

	commandCategories.emplace_back(UL_("  Sample [Top]"), kCtxCtrlSamples);

	{
		auto &commands = commandCategories.emplace_back(UL_("    Sample Editor"), kCtxViewSamples).commandRanges;
		commands.emplace_back(kcStartSampleEditing, kcEndSampleEditing, UL_("Edit"));
		commands.emplace_back(kcStartSampleMisc, kcEndSampleMisc, UL_("Miscellaneous"));
		commands.emplace_back(kcStartSampleCues, kcEndSampleCueGroup, UL_("Sample Cues"));
	}

	{
		auto &commands = commandCategories.emplace_back(UL_("  Instrument Editor"), kCtxCtrlInstruments).commandRanges;
		commands.emplace_back(kcStartInstrumentCtrl, kcEndInstrumentCtrl, UL_("Miscellaneous"));
		commands.emplace_back(kcStartInsNoteMap, kcEndInsNoteMap, UL_("Note Map"));
	}

	{
		auto &commands = commandCategories.emplace_back(UL_("    Envelope Editor"), kCtxViewInstruments).commandRanges;
		commands.emplace_back(kcStartInsEnvelopeEdit, kcEndInsEnvelopeEdit, UL_(""));
	}

	commandCategories.emplace_back(UL_("  Comments [Top]"), kCtxCtrlComments);

	{
		auto &commands = commandCategories.emplace_back(UL_("  Comments [Bottom]"), kCtxViewComments).commandRanges;
		commands.emplace_back(kcStartCommentsCommands, kcEndCommentsCommands, UL_(""));
	}

	{
		auto &commands = commandCategories.emplace_back(UL_("  Plugin Editor"), kCtxVSTGUI).commandRanges;
		commands.emplace_back(kcStartVSTGUICommands, kcEndVSTGUICommands, UL_(""));
	}

	{
		auto &commands = commandCategories.emplace_back(UL_("  Tree View"), kCtxViewTree).commandRanges;
		commands.emplace_back(kcStartTreeViewCommands, kcEndTreeViewCommands, UL_(""));
	}
}


// Pure GUI methods

void COptionsKeyboard::UpdateDialog()
{
	OnCategorySelChanged();    // Fills command list and automatically selects first command.
	OnCommandKeySelChanged();  // Fills command key choice list for that command and automatically selects first choice.
}


void COptionsKeyboard::OnKeyboardChanged()
{
	OnSettingsChanged();
	UpdateDialog();
}


void COptionsKeyboard::OnCategorySelChanged()
{
	LockControls();
	int cat = static_cast<int>(m_cmbCategory.GetItemData(m_cmbCategory.GetCurSel()));
	if(cat < 0)
		return;

	const bool refresh = cat != m_curCategory || m_eFind.GetWindowTextLength() > 0 || m_eFindHotKey.GetWindowTextLength() > 0;
	m_eFind.SetWindowText(UL_(""));
	OnClearHotKey();
	if(refresh)
	{
		// Changed category
		UpdateShortcutList(cat);
	}
	UnlockControls();
}


// Force last active category to be selected in dropdown menu.
void COptionsKeyboard::UpdateCategory()
{
	for(int i = 0; i < m_cmbCategory.GetCount(); i++)
	{
		if(static_cast<int>(m_cmbCategory.GetItemData(i)) == m_curCategory)
		{
			m_cmbCategory.SetCurSel(i);
			break;
		}
	}
}

void COptionsKeyboard::OnSearchTermChanged()
{
	if(IsLocked())
		return;

	mpt::ustring findString;
	m_eFind.GetWindowText(findString);

	if(findString.empty())
	{
		UpdateCategory();
	}
	UpdateShortcutList(findString.empty() ? m_curCategory : -1);
}


void COptionsKeyboard::OnClearSearch()
{
	m_eFindHotKey.SetKey(ModNone, 0);
	m_eFind.SetWindowText(UL_(""));
}


void COptionsKeyboard::OnEnableFindHotKey()
{
	OnClearHotKey();
	GetDlgItem(IDC_BUTTON2)->ShowWindow(false);
	GetDlgItem(IDC_FINDHOTKEY_LABEL)->ShowWindow(true);
	GetDlgItem(IDC_FINDHOTKEY)->ShowWindow(true);
	GetDlgItem(IDC_FINDHOTKEY)->SetFocus();
}


void COptionsKeyboard::OnFindHotKey()
{
	GetDlgItem(IDC_BUTTON2)->ShowWindow(true);
	GetDlgItem(IDC_FINDHOTKEY_LABEL)->ShowWindow(false);
	GetDlgItem(IDC_FINDHOTKEY)->ShowWindow(false);
	const bool hasKey = m_eFindHotKey.HasKey();
	if(!hasKey)
		UpdateCategory();
	UpdateShortcutList(hasKey ? -1 : m_curCategory);
	GotoDlgCtrl(&m_lbnCommandKeys);
}


void COptionsKeyboard::OnClearHotKey()
{
	// Focus key search: Clear input
	m_eFindHotKey.SetKey(ModNone, 0);
}


// Fills command list and automatically selects first command.
void COptionsKeyboard::UpdateShortcutList(int category)
{
	mpt::ustring findString;
	m_eFind.GetWindowText(findString);
	findString = mpt::ToLowerCaseLocale(findString);

	const bool searchByName = !findString.empty(), searchByKey = m_eFindHotKey.HasKey();
	const bool doSearch = (searchByName || searchByKey);

	int firstCat = category, lastCat = category;
	if(category == -1)
	{
		// We will search in all categories
		firstCat = 0;
		lastCat = static_cast<int>(commandCategories.size()) - 1;
	}

	const auto curSelection = m_lbnCommandKeys.GetSelectionMark();
	const CommandID curCommand = (curSelection >= 0) ? static_cast<CommandID>(m_lbnCommandKeys.GetItemData(curSelection)) : kcNull;
	m_lbnCommandKeys.SetRedraw(false);
	m_lbnCommandKeys.DeleteAllItems();
	int itemID = -1;
	int itemToSelect = -1;

	for(int cat = firstCat; cat <= lastCat; cat++)
	{
		// When searching, we also add the category names to the list.
		bool addCategoryName = (firstCat != lastCat);

		for(const auto &range : commandCategories[cat].commandRanges)
		{
			for(CommandID com = range.first; com <= range.last; com = static_cast<CommandID>(com + 1))
			{
				mpt::ustring cmdText = m_localCmdSet->GetCommandText(com);
				bool addKey = true;

				if(searchByKey)
				{
					addKey = false;
					for(const KeyCombination &kc : m_localCmdSet->GetKeyChoices(com))
					{
						if(kc.KeyCode() == m_eFindHotKey.code && kc.Modifier() == m_eFindHotKey.mod)
						{
							addKey = true;
							break;
						}
					}
				}
				if(searchByName && addKey)
				{
					addKey = (mpt::ToLowerCaseLocale(cmdText).find(findString) != mpt::ustring::npos);
				}

				if(!addKey)
					continue;

				m_curCategory = cat;

				if(com == range.first && !doSearch)
				{
					mpt::ustring catName;
					if(range.name.empty())
						catName = UL_("------------------------------------------------------");
					else
						catName = UL_("------ ") + mpt::ustring{range.name} + UL_(" ------");
					m_lbnCommandKeys.InsertItem(++itemID, catName, -1, static_cast<uintptr_t>(kcNull));
					if(itemToSelect == -1)
						itemToSelect = itemID;
				}

				if(!m_localCmdSet->IsHidden(com))
				{
					if(doSearch && addCategoryName)
					{
						const mpt::ustring catName = UL_("------ ") + mpt::trim_left(mpt::ustring{commandCategories[cat].name}) + UL_(" ------");
						m_lbnCommandKeys.InsertItem(++itemID, catName, -1, static_cast<uintptr_t>(kcNull));
						addCategoryName = false;
					}

					const mpt::ustring text = m_localCmdSet->GetCommandText(com);
					m_lbnCommandKeys.InsertItem(++itemID, text, -1, static_cast<uintptr_t>(com));
					m_lbnCommandKeys.SetItemText(itemID, 1, m_localCmdSet->GetKeyTextFromCommand(com));
					if(itemToSelect == -1)
						itemToSelect = itemID;

					if(curCommand == com)
					{
						// Keep selection on previously selected string
						itemToSelect = itemID;
					}
				}
			}
		}
	}

	if(itemToSelect != -1)
	{
		m_lbnCommandKeys.SetSelectionMark(itemToSelect);
		m_lbnCommandKeys.SetItemState(itemToSelect, ui::ListItemSelected, ui::ListItemSelected);
	}
	m_lbnCommandKeys.SetRedraw(true);
	OnCommandKeySelChanged();
}


// Fills  key choice list and automatically selects first key choice
void COptionsKeyboard::OnCommandKeySelChanged(NotifyHeader *pNMHDR, LResult *)
{
	const int selectedItem = m_lbnCommandKeys.GetSelectionMark();

	const CommandID cmd = (selectedItem >= 0) ? static_cast<CommandID>(m_lbnCommandKeys.GetItemData(selectedItem)) : kcNull;
	mpt::ustring str;

	EnableKeyChoice(false);

	bool enableButton = (cmd != kcNull) ? true : false;
	bool enableCheckBoxes = (cmd != kcNull && m_localCmdSet->GetKeyListSize(cmd) > 0) ? true : false;
	GetDlgItem(IDC_BUTTON1)->EnableWindow(enableButton);
	GetDlgItem(IDC_DELETE)->EnableWindow(enableButton);
	GetDlgItem(IDC_RESTORE)->EnableWindow(enableButton);
	m_cmbKeyChoice.EnableWindow(enableButton);
	m_bKeyDown.EnableWindow(enableCheckBoxes);
	m_bKeyHold.EnableWindow(enableCheckBoxes);
	m_bKeyUp.EnableWindow(enableCheckBoxes);

	//Separator
	if(cmd == kcNull)
	{
		m_cmbKeyChoice.SetWindowText(UL_(""));
		m_eCustHotKey.SetWindowText(UL_(""));
		m_bKeyDown.SetCheck(ui::CheckOff);
		m_bKeyHold.SetCheck(ui::CheckOff);
		m_bKeyUp.SetCheck(ui::CheckOff);
		m_curCommand = kcNull;

		GetDlgItem(IDC_GROUPBOX_KEYSETUP)->SetWindowText(UL_("&Key setup for selected command"));
		UpdateWarning();
	}

	//Fill "choice" list
	else if((cmd >= kcFirst && cmd != m_curCommand) || m_forceUpdate)  // Have we changed command?
	{
		GetDlgItem(IDC_GROUPBOX_KEYSETUP)->SetWindowText(UL_("&Key setup for ") + m_localCmdSet->GetCommandText(cmd));

		m_forceUpdate = false;

		m_curCommand = cmd;
		m_curCategory = GetCategoryFromCommandID(cmd);

		m_cmbKeyChoice.ResetContent();
		int numChoices = m_localCmdSet->GetKeyListSize(cmd);
		if(cmd >= kcFirst && cmd < kcNumCommands && numChoices > 0)
		{
			for(int i = 0; i < numChoices; i++)
			{
				mpt::ustring s = MPT_UFORMAT("Choice {} (of {})")(i + 1, numChoices);
				m_cmbKeyChoice.SetItemData(m_cmbKeyChoice.AddString(s), i);
			}
		}
		m_cmbKeyChoice.SetItemData(m_cmbKeyChoice.AddString(UL_("<new>")), numChoices);
		m_cmbKeyChoice.SetCurSel(0);
		m_curKeyChoice = -1;
		OnKeyChoiceSelect();
	}
}


void COptionsKeyboard::OnListenForKeysFromList(NotifyHeader *pNMHDR, LResult *)
{
	const auto *hdr = static_cast<const ui::ListClickInfo *>(pNMHDR->extra);
	if(m_curCommand != kcNull && hdr->column == 1)
		OnListenForKeys();
}


//Fills or clears key choice info
void COptionsKeyboard::OnKeyChoiceSelect()
{
	EnableKeyChoice(false);

	int choice = static_cast<int>(m_cmbKeyChoice.GetItemData(m_cmbKeyChoice.GetCurSel()));
	CommandID cmd = m_curCommand;

	//If nothing there, clear
	if(cmd == kcNull || choice >= m_localCmdSet->GetKeyListSize(cmd) || choice < 0)
	{
		UpdateWarning();
		m_curKeyChoice = choice;
		m_forceUpdate = true;
		m_eCustHotKey.SetKey(ModNone, 0);
		m_bKeyDown.SetCheck(ui::CheckOff);
		m_bKeyHold.SetCheck(ui::CheckOff);
		m_bKeyUp.SetCheck(ui::CheckOff);
		return;
	}

	//else, if changed, Fill
	if(choice != m_curKeyChoice || m_forceUpdate)
	{
		m_curKeyChoice = choice;
		m_forceUpdate = false;
		KeyCombination kc = m_localCmdSet->GetKey(cmd, choice);
		m_eCustHotKey.SetKey(kc.Modifier(), kc.KeyCode());

		m_bKeyDown.SetCheck((kc.EventType() & kKeyEventDown) ? ui::CheckOn : ui::CheckOff);
		m_bKeyHold.SetCheck((kc.EventType() & kKeyEventRepeat) ? ui::CheckOn : ui::CheckOff);
		m_bKeyUp.SetCheck((kc.EventType() & kKeyEventUp) ? ui::CheckOn : ui::CheckOff);

		if(auto conflictCmd = m_localCmdSet->IsConflicting(kc, cmd, true, false); conflictCmd.first != kcNull
		   && conflictCmd.first != cmd)
		{
			UpdateWarning(m_localCmdSet->FormatConflict(kc, conflictCmd.first, conflictCmd.second));
		} else
		{
			UpdateWarning();
		}
	}
}

void COptionsKeyboard::OnChordWaitTimeChanged()
{
	mpt::ustring s;
	uint32 val;
	m_eChordWaitTime.GetWindowText(s);
	val = _tstoi(s);
	if(val > 5000)
	{
		val = 5000;
		m_eChordWaitTime.SetWindowText(UL_("5000"));
	}
	OnSettingsChanged();
}

// Change handling

void COptionsKeyboard::OnRestoreKeyChoice()
{
	CommandID cmd = m_curCommand;

	CInputHandler *ih = CMainFrame::GetInputHandler();

	// Do nothing if there's nothing to restore
	if(cmd == kcNull || ((m_curKeyChoice < 0 || m_curKeyChoice >= ih->GetKeyListSize(cmd)) && ih->GetKeyListSize(cmd) != 0))
	{
		ui::Beep();
		return;
	}

	if(ih->GetKeyListSize(cmd) == 0)
	{
		// Restore the defaults for this key
		mpt::heap_value<CCommandSet> defaultSet;
		defaultSet->LoadDefaultKeymap();
		for(const KeyCombination &kc : defaultSet->GetKeyChoices(cmd))
		{
			m_localCmdSet->Add(kc, cmd, true, m_curKeyChoice);
		}
	} else
	{
		// Restore current key combination choice for currently selected command.
		KeyCombination kc = ih->m_activeCommandSet->GetKey(cmd, m_curKeyChoice);
		m_localCmdSet->Remove(m_curKeyChoice, cmd);
		UpdateWarning(m_localCmdSet->Add(kc, cmd, true, m_curKeyChoice));
	}

	ForceUpdateGUI();
	return;
}


void COptionsKeyboard::OnLButtonDblClk(uint32 flags, Point point)
{
	ClientToScreen(&point);
	Rect rect;
	m_eCustHotKey.GetWindowRect(rect);
	if(m_curCommand != kcNull && m_eCustHotKey.IsBypassed() && rect.PtInRect(point))
		EnableKeyChoice(true);
	else
		PropertyPage::OnLButtonDblClk(flags, point);
}


void COptionsKeyboard::OnListenForKeys()
{
	EnableKeyChoice(m_eCustHotKey.IsBypassed());
}


void COptionsKeyboard::EnableKeyChoice(bool enable)
{
	if(!enable && GetFocus() == &m_eCustHotKey)
		GotoDlgCtrl(GetDlgItem(IDC_BUTTON1));
	m_eCustHotKey.Bypass(!enable);
	GetDlgItem(IDC_BUTTON1)->SetWindowText(enable ? UL_("Cancel") : UL_("&Set"));
	if(enable)
		GotoDlgCtrl(&m_eCustHotKey);
}


void COptionsKeyboard::OnDeleteKeyChoice()
{
	CommandID cmd = m_curCommand;

	// Do nothing if there's no key defined for this slot.
	if(m_curCommand == kcNull || m_curKeyChoice < 0 || m_curKeyChoice >= m_localCmdSet->GetKeyListSize(cmd))
	{
		ui::Beep();
		return;
	}

	// Delete current key combination choice for currently selected command.
	m_localCmdSet->Remove(m_curKeyChoice, cmd);

	ForceUpdateGUI();
	UpdateWarning();
}


void COptionsKeyboard::OnCancelKeyChoice(const Wnd *source)
{
	if(source == &m_eFindHotKey)
	{
		GetDlgItem(IDC_BUTTON2)->ShowWindow(true);
		GetDlgItem(IDC_FINDHOTKEY_LABEL)->ShowWindow(false);
		GetDlgItem(IDC_FINDHOTKEY)->ShowWindow(false);
	} else
	{
		EnableKeyChoice(false);
	}
}


void COptionsKeyboard::OnSetKeyChoice(const Wnd *source)
{
	if(source == &m_eFindHotKey)
	{
		OnFindHotKey();
		return;
	}

	EnableKeyChoice(false);

	CommandID cmd = m_curCommand;
	if(cmd == kcNull)
	{
		Reporting::Warning("Invalid slot.", "Invalid key data", this);
		return;
	}

	FlagSet<KeyEventType> event = kKeyEventNone;
	if(m_bKeyDown.GetCheck() != ui::CheckOff)
		event |= kKeyEventDown;
	if(m_bKeyHold.GetCheck() != ui::CheckOff)
		event |= kKeyEventRepeat;
	if(m_bKeyUp.GetCheck() != ui::CheckOff)
		event |= kKeyEventUp;
	if(event == kKeyEventNone)
		event = kKeyEventDown;

	KeyCombination kc(CCommandSet::ContextFromCommand(cmd), m_eCustHotKey.mod, m_eCustHotKey.code, event);
	//detect invalid input
	if(!kc.KeyCode())
	{
		Reporting::Warning("You need to say to which key you'd like to map this command to.", "Invalid key data", this);
		return;
	}

	bool add = true, updateAll = false;
	std::pair<CommandID, KeyCombination> conflictCmd;
	if(CCommandSet::MustBeModifierKey(cmd) && !kc.IsModifierCombination())
	{
		KeyCombination origKc = m_localCmdSet->GetKey(cmd, m_curKeyChoice);
		m_eCustHotKey.SetKey(origKc.Modifier(), origKc.KeyCode());
		UpdateWarning(m_localCmdSet->GetCommandText(cmd) + UL_(" must be a modifier (Shift/Ctrl/Alt), but you chose ") + kc.GetKeyText(), true);
		add = false;
	} else if((conflictCmd = m_localCmdSet->IsConflicting(kc, cmd)).first != kcNull && conflictCmd.first != cmd && !m_localCmdSet->IsCrossContextConflict(kc, conflictCmd.second))
	{
		ConfirmAnswer delOld = Reporting::Confirm(UL_("New shortcut (") + kc.GetKeyText() + UL_(") has the same key combination as ") + m_localCmdSet->GetCommandText(conflictCmd.first) + UL_(" in ") + conflictCmd.second.GetContextText() + UL_(".\nDo you want to delete the other shortcut, only keeping the new one?"), UL_("Shortcut Conflict"), true, false, this);
		if(delOld == cnfYes)
		{
			m_localCmdSet->Remove(conflictCmd.second, conflictCmd.first);
			updateAll = true;
		} else if(delOld == cnfCancel)
		{
			// Cancel altogther; restore original choice
			add = false;
			if(m_curKeyChoice >= 0 && m_curKeyChoice < m_localCmdSet->GetKeyListSize(cmd))
			{
				KeyCombination origKc = m_localCmdSet->GetKey(cmd, m_curKeyChoice);
				m_eCustHotKey.SetKey(origKc.Modifier(), origKc.KeyCode());
			} else
			{
				m_eCustHotKey.SetWindowText(UL_(""));
			}
		}
	}
	
	if(add)
	{
		//process valid input
		m_localCmdSet->Remove(m_curKeyChoice, cmd);
		UpdateWarning(m_localCmdSet->Add(kc, cmd, true, m_curKeyChoice), true);
		ForceUpdateGUI(updateAll);
	}
}


void COptionsKeyboard::UpdateWarning(mpt::ustring text, bool notify)
{
	const int iconSize = ui::ScalePixels(16, this);
	const ui::Bitmap *icon = nullptr;
	if(text.empty())
	{
		m_warnText.SetWindowText(UL_("No conflicts found."));
		if(!m_infoIcon.IsValid())
			m_infoIcon = ui::CreateInfoIcon(iconSize);
		icon = &m_infoIcon;
	} else
	{
		if(notify)
			ui::Beep();

		m_warnText.SetWindowText(text);
		if(!m_warnIcon.IsValid())
			m_warnIcon = ui::CreateWarningIcon(iconSize);
		icon = &m_warnIcon;
	}
	m_warnIconCtl.SetWindowPos(nullptr, 0, 0, iconSize, iconSize, ui::PosShowWindow | ui::PosNoZOrder | ui::PosNoRedraw | ui::PosNoMove | ui::PosNoActivate);
	m_warnIconCtl.SetBitmap(icon);
	m_lastWarning = std::move(text);
}


void COptionsKeyboard::OnOK()
{
	CMainFrame::GetInputHandler()->SetNewCommandSet(*m_localCmdSet);

	mpt::ustring cs;
	m_eChordWaitTime.GetWindowText(cs);
	TrackerSettings::Instance().gnAutoChordWaitTime = _tstoi(cs);

	PropertyPage::OnOK();
}


void COptionsKeyboard::OnDestroy()
{
	PropertyPage::OnDestroy();
	m_localCmdSet.reset();
}


void COptionsKeyboard::OnLoad()
{
	auto dlg = OpenFileDialog()
	               .DefaultExtension(UL_("mkb"))
	               .DefaultFilename(m_fullPathName)
	               .ExtensionFilter(UL_("OpenMPT Key Bindings (*.mkb)|*.mkb||"))
	               .AddPlace(theApp.GetInstallPkgPath() + P_("extraKeymaps"))
	               .WorkingDirectory(TrackerSettings::Instance().m_szKbdFile);
	if(!dlg.Show(this))
		return;

	m_fullPathName = dlg.GetFirstFile();
	m_localCmdSet->LoadFile(m_fullPathName);
	ForceUpdateGUI(true);
	UpdateWarning();
}


void COptionsKeyboard::OnSave()
{
	auto dlg = SaveFileDialog()
	               .DefaultExtension(UL_("mkb"))
	               .DefaultFilename(m_fullPathName)
	               .ExtensionFilter(UL_("OpenMPT Key Bindings (*.mkb)|*.mkb||"))
	               .WorkingDirectory(TrackerSettings::Instance().m_szKbdFile);
	if(!dlg.Show(this))
		return;

	m_fullPathName = dlg.GetFirstFile();
	m_localCmdSet->SaveFile(m_fullPathName);
}


void COptionsKeyboard::OnToggleNotesRepeat()
{
	m_localCmdSet->QuickChange_NotesRepeat(IsDlgButtonChecked(IDC_NOTESREPEAT) != ui::CheckOn);
	ForceUpdateGUI(true);
}


void COptionsKeyboard::ForceUpdateGUI(bool updateAllKeys)
{
	m_forceUpdate = true;                  // m_curKeyChoice and m_curCommand haven't changed, yet we still want to update.
	int curChoice = m_curKeyChoice;        // next call will overwrite m_curKeyChoice
	OnCommandKeySelChanged();              // update keychoice list
	m_cmbKeyChoice.SetCurSel(curChoice);   // select fresh keychoice (thus restoring m_curKeyChoice)
	OnKeyChoiceSelect();                   // update key data
	OnSettingsChanged();                   // Enable "apply" button

	if(updateAllKeys)
	{
		const int numItems = m_lbnCommandKeys.GetItemCount();
		for(int i = 0; i < numItems; i++)
		{
			if(const auto cmd = static_cast<CommandID>(m_lbnCommandKeys.GetItemData(i)); cmd != kcNull)
				m_lbnCommandKeys.SetItemText(i, 1, m_localCmdSet->GetKeyTextFromCommand(cmd));
		}
		UpdateNoteRepeatCheckbox();
	} else if(m_curCommand != kcNull)
	{
		m_lbnCommandKeys.SetItemText(m_lbnCommandKeys.GetSelectionMark(), 1, m_localCmdSet->GetKeyTextFromCommand(m_curCommand));
		if(mpt::is_in_range(m_curCommand, kcVPStartNotes, kcVPEndNotes))
			UpdateNoteRepeatCheckbox();
	}
}


void COptionsKeyboard::UpdateNoteRepeatCheckbox()
{
	uint32 state = uint32_max;
	for(CommandID cmd = kcVPStartNotes; cmd <= kcVPEndNotes && state != ui::CheckMixed; cmd = static_cast<CommandID>(cmd + 1))
	{
		for(auto &kc : m_localCmdSet->GetKeyChoices(cmd))
		{
			const bool repeat = (kc.EventType() & kKeyEventRepeat);
			if(repeat && (state == uint32_max || state == ui::CheckOn))
				state = ui::CheckOn;
			else if(!repeat && (state == uint32_max || state == ui::CheckOff))
				state = ui::CheckOff;
			else
				state = ui::CheckMixed;
		}
	}
	CheckDlgButton(IDC_NOTESREPEAT, state);
}


void COptionsKeyboard::OnRestoreKeymapDropdown(NotifyHeader *, LResult *result)
{
	ShowRestoreKeymapMenu();
	*result = 0;
}

void COptionsKeyboard::OnRestoreDefaultKeymap()
{
#if MPT_WINNT_AT_LEAST(MPT_WIN_VISTA)
	if((m_restoreDefaultButton.GetStyle() & BS_SPLITBUTTON) == BS_SPLITBUTTON)
	{
		OnRestoreMPTKeymap();
		return;
	}
#endif
	ShowRestoreKeymapMenu();
}


void COptionsKeyboard::ShowRestoreKeymapMenu()
{
	Rect rect;
	m_restoreDefaultButton.GetWindowRect(rect);

	Menu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(ui::MenuItemString, ID_KEYPRESET_MPT, UL_("&OpenMPT style"));
	menu.AppendMenu(ui::MenuItemString, ID_KEYPRESET_IT, UL_("&Impulse Tracker style"));
	menu.AppendMenu(ui::MenuItemString, ID_KEYPRESET_FT2, UL_("&Fast Tracker style"));
	menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, rect.left, rect.bottom, this);
}


void COptionsKeyboard::RestoreKeymap(KeyboardPreset preset)
{
	if(Reporting::Confirm("Discard all custom changes and restore default key configuration?", false, true, this) == cnfYes)
	{
		m_localCmdSet->LoadDefaultKeymap(preset);
		ForceUpdateGUI(true);
	}
}


int COptionsKeyboard::GetCategoryFromCommandID(CommandID command) const
{
	const InputTargetContext context = CCommandSet::ContextFromCommand(command);
	for(size_t cat = 0; cat < commandCategories.size(); cat++)
	{
		if(commandCategories[cat].id == context)
			return static_cast<int>(cat);
	}
	return -1;
}


OPENMPT_NAMESPACE_END
