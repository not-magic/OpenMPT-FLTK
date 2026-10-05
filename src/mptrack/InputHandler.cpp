/*
 * InputHandler.cpp
 * ----------------
 * Purpose: Implementation of keyboard input handling, keymap loading, ...
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/InputHandler.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "InputHandler.h"
#include "CommandSet.h"
#include "Mainfrm.h"
#include "Mptrack.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "WindowMessages.h"
#include "../soundlib/MIDIEvents.h"


OPENMPT_NAMESPACE_BEGIN


#define TRANSITIONBIT 0x8000
#define REPEATBIT 0x4000

CInputHandler::CInputHandler(Wnd *mainframe)
{
	m_pMainFrm = mainframe;

	//Init CommandSet and Load defaults
	m_activeCommandSet = std::make_unique<CCommandSet>();
	m_lastCommands.fill(kcNull);

	mpt::PathString defaultPath = theApp.GetConfigPath() + P_("Keybindings.mkb");

	const bool noExistingKbdFileSetting = TrackerSettings::Instance().m_szKbdFile.empty();

	// 1. Try to load keybindings from the path saved in the settings.
	// 2. If the setting doesn't exist or the loading fails, try to load from default location.
	// 3. If neither one of these worked, load default keybindings from resources.
	// 4. If there were no keybinding setting already, create a keybinding file to default location
	//    and set its path to settings.

	if(noExistingKbdFileSetting || !m_activeCommandSet->LoadFile(TrackerSettings::Instance().m_szKbdFile))
	{
		if(!FileSystem::IsFile(defaultPath) || !m_activeCommandSet->LoadFile(defaultPath))
		{
			// Load keybindings from resources.
			MPT_LOG_GLOBAL(LogDebug, "InputHandler", UL_("Loading keybindings from resources\n"));
			m_activeCommandSet->LoadDefaultKeymap();
			if (noExistingKbdFileSetting)
				m_activeCommandSet->SaveFile(defaultPath);
		}
	}
	// We will only overwrite the default Keybindings.mkb file from now on.
	TrackerSettings::Instance().m_szKbdFile = defaultPath;

	//Get Keymap
	m_activeCommandSet->GenKeyMap(m_keyMap);
	SetupSpecialKeyInterception(); // Feature: use Windows keys as modifier keys, intercept special keys
}


CommandID CInputHandler::SendCommands(Wnd *wnd, const KeyMapRange &cmd)
{
	CommandID executeCommand = kcNull;
	if(wnd != nullptr)
	{
		// Some commands (e.g. open/close/document switching) may invalidate the key map and thus its iterators.
		// To avoid this problem, copy over the elements we are interested in before sending commands.
		std::vector<KeyMap::value_type> commands;
		commands.reserve(std::distance(cmd.first, cmd.second));
		for(auto i = cmd.first; i != cmd.second; i++)
		{
			commands.push_back(*i);
		}
		for(const auto &i : commands)
		{
			m_lastCommands[m_lastCommandPos] = i.second;
			m_lastCommandPos = (m_lastCommandPos + 1) % m_lastCommands.size();
			if(wnd->SendMessage(MSG_MOD_KEYCOMMAND, i.second, i.first.AsLPARAM()) != kcNull)
			{
				// Command was handled, no need to let the OS handle the key
				executeCommand = i.second;
			}
		}
	}
	return executeCommand;
}


bool CInputHandler::HandleKeyEvent(int event, InputTargetContext context, Wnd *pSourceWnd)
{
	if(event != FL_KEYBOARD && event != FL_KEYUP)
		return false;
	const uint32 flags = (event == FL_KEYUP) ? ui::KeyFlagRelease : 0;
	return KeyEvent(context, Translate(ui::KeyFromEvent(), 1, flags), pSourceWnd) != kcNull;
}


CommandID CInputHandler::KeyEvent(const InputTargetContext context, const KeyboardEvent &event, Wnd *pSourceWnd)
{
	if(IsKeyPressHandledByTextBox(event.key, Wnd::GetFocus()))
		return kcNull;

	KeyMapRange cmd = m_keyMap.equal_range(KeyCombination(context, GetModifierMask(), event.key, event.keyEventType));
	if(cmd.first != cmd.second && InterceptSpecialKeys(event))
		return kcDummyShortcut;

	if(pSourceWnd == nullptr)
		pSourceWnd = m_pMainFrm;	// By default, send command message to main frame.
	return SendCommands(pSourceWnd, cmd);
}


// Feature: use the Windows / Meta keys as modifier keys, intercept special keys
bool CInputHandler::InterceptSpecialKeys(const KeyboardEvent &event)
{
	enum { KeyNonExistent = ui::Key_F24+1 };

	return event.key == KeyNonExistent;
};


void CInputHandler::SetupSpecialKeyInterception()
{
	m_bInterceptWindowsKeys = m_bInterceptNumLock = m_bInterceptCapsLock = m_bInterceptScrollLock = false;
	for(const auto &i : m_keyMap)
	{
		MPT_ASSERT(i.second != kcNull);
		if(i.first.Modifier() == ModWin)
			m_bInterceptWindowsKeys = true;
		if(i.first.KeyCode() == ui::Key_NUMLOCK)
			m_bInterceptNumLock = true;
		if(i.first.KeyCode() == ui::Key_CAPITAL)
			m_bInterceptCapsLock = true;
		if(i.first.KeyCode() == ui::Key_SCROLL)
			m_bInterceptScrollLock = true;
	}
};


// Translate MIDI messages to shortcut commands
CommandID CInputHandler::HandleMIDIMessage(InputTargetContext context, uint32 message)
{
	KeyMapRange cmd = { m_keyMap.end(), m_keyMap.end() };
	auto byte1 = MIDIEvents::GetDataByte1FromEvent(message), byte2 = MIDIEvents::GetDataByte2FromEvent(message);
	switch(MIDIEvents::GetTypeFromEvent(message))
	{
	case MIDIEvents::evControllerChange:
		if(byte2 != 0)
		{
			// Only capture MIDI CCs for now. Some controllers constantly send some MIDI CCs with value 0
			// (e.g. the Roland D-50 sends CC123 whenenver all notes have been released), so we will ignore those.
			cmd = m_keyMap.equal_range(KeyCombination(context, ModMidi, byte1, kKeyEventDown));
			return SendCommands(m_pMainFrm, cmd);
		}
		break;

	case MIDIEvents::evNoteOff:
		byte2 = 0;
		[[fallthrough]];
	case MIDIEvents::evNoteOn:
		if(byte2 != 0)
		{
			cmd = m_keyMap.equal_range(KeyCombination(context, ModMidi, byte1 | 0x80, kKeyEventDown));
			return SendCommands(m_pMainFrm, cmd);
		} else
		{
			cmd = m_keyMap.equal_range(KeyCombination(context, ModMidi, byte1 | 0x80, kKeyEventUp));
			// If the key-down triggered a note, we still want that note to be stopped. So we always pretend that no key was assigned to this event
			SendCommands(m_pMainFrm, cmd);
		}
		break;

	default:
		break;
	}

	return kcNull;
}


int CInputHandler::GetKeyListSize(CommandID cmd) const
{
	return m_activeCommandSet->GetKeyListSize(cmd);
}


CInputHandler::KeyboardEvent CInputHandler::Translate(uint32 key, uint32 repeatCount, uint32 flags)
{
	return {key, static_cast<uint16>(repeatCount), static_cast<uint16>(flags), GetKeyEventType(flags)};
}


KeyEventType CInputHandler::GetKeyEventType(uint32 nFlags)
{
	if (nFlags & TRANSITIONBIT)
	{
		// Key released
		return kKeyEventUp;
	} else if (nFlags & REPEATBIT)
	{
		// Key repeated
		return kKeyEventRepeat;
	} else
	{
		// New key down
		return kKeyEventDown;
	}
}


bool CInputHandler::SelectionPressed() const
{
	int nSelectionKeys = m_activeCommandSet->GetKeyListSize(kcSelect);
	KeyCombination key;

	const auto modifierMask = GetModifierMask();
	for (int k=0; k<nSelectionKeys; k++)
	{
		key = m_activeCommandSet->GetKey(kcSelect, k);
		if(modifierMask & key.Modifier())
			return true;
	}
	return false;
}


bool CInputHandler::ShiftPressed()
{
	return ui::IsKeyDown(ui::Key_SHIFT);
}


bool CInputHandler::CtrlPressed()
{
	return ui::IsKeyDown(ui::Key_CONTROL);
}


bool CInputHandler::AltPressed()
{
	return ui::IsKeyDown(ui::Key_MENU);
}


OrderTransitionMode CInputHandler::ModifierKeysToTransitionMode()
{
	const bool shift = ShiftPressed();
	const bool alt = AltPressed();
	if(shift && alt)
		return OrderTransitionMode::AtRowEnd;
	else if(!shift && alt)
		return OrderTransitionMode::AtBeatEnd;
	else if(shift && !alt)
		return OrderTransitionMode::AtMeasureEnd;
	else
		return OrderTransitionMode::AtPatternEnd;
}


void CInputHandler::Bypass(bool b)
{
	if(b)
		m_bypassCount++;
	else
		m_bypassCount--;
	MPT_ASSERT(m_bypassCount >= 0);
}


bool CInputHandler::IsBypassed() const
{
	return m_bypassCount > 0;
}


FlagSet<Modifiers> CInputHandler::GetModifierMask()
{
	FlagSet<Modifiers> modifierMask = ModNone;
	if(Wnd::GetFocus() == nullptr)
		return modifierMask;

	const bool distinguishModifiers = TrackerSettings::Instance().MiscDistinguishModifiers;
	if(ui::IsKeyDown(ui::Key_LCONTROL))
		modifierMask.set(ModCtrl);
	if(ui::IsKeyDown(ui::Key_RCONTROL))
		modifierMask.set(distinguishModifiers ? ModRCtrl : ModCtrl);
	if(ui::IsKeyDown(ui::Key_LSHIFT))
		modifierMask.set(ModShift);
	if(ui::IsKeyDown(ui::Key_RSHIFT))
		modifierMask.set(distinguishModifiers ? ModRShift : ModShift);
	if(ui::IsKeyDown(ui::Key_LMENU))
		modifierMask.set(ModAlt);
	if(ui::IsKeyDown(ui::Key_RMENU))
		modifierMask.set(distinguishModifiers ? ModRAlt : ModAlt);
	if(ui::IsKeyDown(ui::Key_LWIN))
		modifierMask.set(ModWin);
	if(ui::IsKeyDown(ui::Key_RWIN))
		modifierMask.set(ModWin);
	return modifierMask;
}


mpt::ustring CInputHandler::GetKeyTextFromCommand(CommandID c, const mpt::ustring &prependText) const
{
	mpt::ustring s;
	if(!prependText.empty())
	{
		s = prependText;
		s += UL_("\t");
	}
	s += m_activeCommandSet->GetKeyTextFromCommand(c, 0);
	return s;
}


mpt::ustring CInputHandler::GetMenuText(uint32 id) const
{
	static constexpr std::tuple<uint32, CommandID, const mpt::uchar *> MenuItems[] =
	{
		{ ID_FILE_NEW,                      kcFileNew,           UL_("&New") },
		{ ID_FILE_OPEN,                     kcFileOpen,          UL_("&Open...") },
		{ ID_FILE_OPENTEMPLATE,             kcNull,              UL_("Open &Template") },
		{ ID_FILE_OPENTEMPLATE_LASTINRANGE, kcFileOpenTemplate,  UL_("&Browse...") },
		{ ID_FILE_CLOSE,                    kcFileClose,         UL_("&Close") },
		{ ID_FILE_CLOSEALL,                 kcFileCloseAll,      UL_("C&lose All") },
		{ ID_FILE_APPENDMODULE,             kcFileAppend,        UL_("Appen&d Module...") },
		{ ID_FILE_SAVE,                     kcFileSave,          UL_("&Save") },
		{ ID_FILE_SAVE_AS,                  kcFileSaveAs,        UL_("Save &As...") },
		{ ID_FILE_SAVE_COPY,                kcFileSaveCopy,      UL_("Save Cop&y...") },
		{ ID_FILE_SAVEASTEMPLATE,           kcFileSaveTemplate,  UL_("Sa&ve as Template") },
		{ ID_FILE_SAVEASWAVE,               kcFileSaveAsWave,    UL_("Stream Export (&WAV, FLAC, MP3, etc.)...") },
		{ ID_FILE_SAVEMIDI,                 kcFileSaveMidi,      UL_("Export as M&IDI...") },
		{ ID_FILE_SAVEOPL,                  kcFileSaveOPL,       UL_("Export O&PL Register Dump...") },
		{ ID_FILE_SAVECOMPAT,               kcFileExportCompat,  UL_("Compatibility &Export...") },
		{ ID_IMPORT_MIDILIB,                kcFileImportMidiLib, UL_("Import &MIDI Library...") },
		{ ID_ADD_SOUNDBANK,                 kcFileAddSoundBank,  UL_("Add Sound &Bank...") },

		{ ID_PLAYER_PLAY,          kcPlayPauseSong,      UL_("Pause / &Resume") },
		{ ID_PLAYER_PLAYFROMSTART, kcPlaySongFromStart,  UL_("&Play from Start") },
		{ ID_PLAYER_STOP,          kcStopSong,           UL_("&Stop") },
		{ ID_PLAYER_PAUSE,         kcPauseSong,          UL_("P&ause") },
		{ ID_MIDI_RECORD,          kcMidiRecord,         UL_("&MIDI Record") },
		{ ID_ESTIMATESONGLENGTH,   kcEstimateSongLength, UL_("&Estimate Song Length") },
		{ ID_APPROX_BPM,           kcApproxRealBPM,      UL_("Approximate Real &BPM") },

		{ ID_EDIT_UNDO,                  kcEditUndo,      UL_("&Undo") },
		{ ID_EDIT_REDO,                  kcEditRedo,      UL_("&Redo") },
		{ ID_EDIT_CUT,                   kcEditCut,       UL_("Cu&t") },
		{ ID_EDIT_COPY,                  kcEditCopy,      UL_("&Copy") },
		{ ID_EDIT_PASTE,                 kcEditPaste,     UL_("&Paste") },
		{ ID_EDIT_SELECT_ALL,            kcEditSelectAll, UL_("Select &All") },
		{ ID_EDIT_CLEANUP,               kcNull,          UL_("C&leanup") },
		{ ID_EDIT_FIND,                  kcEditFind,      UL_("&Find / Replace") },
		{ ID_EDIT_FINDNEXT,              kcEditFindNext,  UL_("Find &Next") },
		{ ID_EDIT_GOTO_MENU,             kcPatternGoto,   UL_("&Goto") },
		{ ID_EDIT_SPLITKEYBOARDSETTINGS, kcShowSplitKeyboardSettings, UL_("Split &Keyboard Settings") },
		// "Paste Special" sub menu
		{ ID_EDIT_PASTE_SPECIAL,    kcEditMixPaste,         UL_("&Mix Paste") },
		{ ID_EDIT_MIXPASTE_ITSTYLE, kcEditMixPasteITStyle,  UL_("M&ix Paste (IT Style)") },
		{ ID_EDIT_PASTEFLOOD,       kcEditPasteFlood,       UL_("Paste Fl&ood") },
		{ ID_EDIT_PUSHFORWARDPASTE, kcEditPushForwardPaste, UL_("&Push Forward Paste (Insert)") },

		{ ID_VIEW_GLOBALS,        kcViewGeneral,            UL_("&General") },
		{ ID_VIEW_SAMPLES,        kcViewSamples,            UL_("&Samples") },
		{ ID_VIEW_PATTERNS,       kcViewPattern,            UL_("&Patterns") },
		{ ID_VIEW_INSTRUMENTS,    kcViewInstruments,        UL_("&Instruments") },
		{ ID_VIEW_COMMENTS,       kcViewComments,           UL_("&Comments") },
		{ ID_VIEW_OPTIONS,        kcViewOptions,            UL_("S&etup") },
		{ ID_VIEW_TOOLBAR,        kcViewMain,               UL_("Show &Main Toolbar") },
		{ IDD_TREEVIEW,           kcViewTree,               UL_("Show &Tree View") },
		{ ID_PLUGIN_SETUP,        kcViewAddPlugin,          UL_("Pl&ugin Manager") },
		{ ID_CHANNEL_MANAGER,     kcViewChannelManager,     UL_("Ch&annel Manager") },
		{ ID_CLIPBOARD_MANAGER,   kcToggleClipboardManager, UL_("C&lipboard Manager") },
		{ ID_VIEW_SONGPROPERTIES, kcViewSongProperties,     UL_("Song P&roperties") },
		{ ID_PATTERN_MIDIMACRO,   kcShowMacroConfig,        UL_("&Zxx Macro Configuration") },
		{ ID_VIEW_MIDIMAPPING,    kcViewMIDImapping,        UL_("&MIDI Mapping") },
		{ ID_VIEW_EDITHISTORY,    kcViewEditHistory,        UL_("Edit &History") },
		// Help submenu
		{ ID_HELPSHOW,        kcHelp, UL_("&Help") },
		{ ID_EXAMPLE_MODULES, kcNull, UL_("&Example Modules") },
	};

	for(const auto & [cmdID, command, text] : MenuItems)
	{
		if(id == cmdID)
		{
			if(command != kcNull)
				return GetKeyTextFromCommand(command, text);
			else
				return text;
		}
	}
	MPT_ASSERT_NOTREACHED();
	return UL_("Unknown Item");
}


void CInputHandler::UpdateMainMenu()
{
	ui::MainFrameBase *mainFrame = dynamic_cast<ui::MainFrameBase *>(m_pMainFrm);
	if(!mainFrame) return;
	Menu *pMenu = mainFrame->GetMenu();
	if (!pMenu || !pMenu->GetSubMenu(0)) return;

	// The first item of the file menu is the "New" sub menu
	pMenu->GetSubMenu(0)->GetItems()[0].text = GetMenuText(ID_FILE_NEW);
	static constexpr int MenuItems[] =
	{
		ID_FILE_OPEN,
		ID_FILE_OPENTEMPLATE_LASTINRANGE,
		ID_FILE_APPENDMODULE,
		ID_FILE_CLOSE,
		ID_FILE_CLOSEALL,
		ID_FILE_SAVE,
		ID_FILE_SAVE_AS,
		ID_FILE_SAVEASWAVE,
		ID_FILE_SAVEMIDI,
		ID_FILE_SAVECOMPAT,
		ID_IMPORT_MIDILIB,
		ID_ADD_SOUNDBANK,

		ID_PLAYER_PLAY,
		ID_PLAYER_PLAYFROMSTART,
		ID_PLAYER_STOP,
		ID_PLAYER_PAUSE,
		ID_MIDI_RECORD,
		ID_ESTIMATESONGLENGTH,
		ID_APPROX_BPM,

		ID_EDIT_UNDO,
		ID_EDIT_REDO,
		ID_EDIT_CUT,
		ID_EDIT_COPY,
		ID_EDIT_PASTE,
		ID_EDIT_PASTE_SPECIAL,
		ID_EDIT_MIXPASTE_ITSTYLE,
		ID_EDIT_PASTEFLOOD,
		ID_EDIT_PUSHFORWARDPASTE,
		ID_EDIT_SELECT_ALL,
		ID_EDIT_FIND,
		ID_EDIT_FINDNEXT,
		ID_EDIT_GOTO_MENU,
		ID_EDIT_SPLITKEYBOARDSETTINGS,

		ID_VIEW_GLOBALS,
		ID_VIEW_SAMPLES,
		ID_VIEW_PATTERNS,
		ID_VIEW_INSTRUMENTS,
		ID_VIEW_COMMENTS,
		ID_VIEW_TOOLBAR,
		IDD_TREEVIEW,
		ID_VIEW_OPTIONS,
		ID_PLUGIN_SETUP,
		ID_CHANNEL_MANAGER,
		ID_CLIPBOARD_MANAGER,
		ID_VIEW_SONGPROPERTIES,
		ID_VIEW_SONGPROPERTIES,
		ID_PATTERN_MIDIMACRO,
		ID_VIEW_EDITHISTORY,
		ID_HELPSHOW,
	};
	for(const auto id : MenuItems)
	{
		pMenu->ModifyMenu(id, ui::MenuItemString, id, GetMenuText(id));
	}
}


void CInputHandler::SetNewCommandSet(const CCommandSet &newSet)
{
	m_activeCommandSet->Copy(newSet);
	m_activeCommandSet->GenKeyMap(m_keyMap);
	SetupSpecialKeyInterception(); // Feature: use Windows keys as modifier keys, intercept special keys
	UpdateMainMenu();
}


bool CInputHandler::SetEffectLetters(const CModSpecifications &modSpecs)
{
	MPT_LOG_GLOBAL(LogDebug, "InputHandler", UL_("Changing command set."));
	bool retval = m_activeCommandSet->QuickChange_SetEffects(modSpecs);
	if(retval) m_activeCommandSet->GenKeyMap(m_keyMap);
	return retval;
}


bool CInputHandler::IsKeyPressHandledByTextBox(uint32 key, const Wnd *focusWindow) const
{
	if(focusWindow == nullptr)
		return false;

	const Edit *edit = dynamic_cast<const Edit *>(focusWindow);
	const ComboBox *combo = dynamic_cast<const ComboBox *>(focusWindow);
	const bool textboxHasFocus = (edit != nullptr) || (combo != nullptr && combo->IsEditable());
	if(!textboxHasFocus)
		return false;

	// Alpha-numerics (only shift or no modifier):
	const auto modifierMask = GetModifierMask();
	if(!modifierMask.test_any_except(ModShift))
	{
		if((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9')
		   || (key >= ui::Key_MULTIPLY && key <= ui::Key_DIVIDE) || key == ui::Key_SPACE || key == ui::Key_CAPITAL
		   || (key >= ui::Key_OEM_1 && key <= ui::Key_OEM_3) || (key >= ui::Key_OEM_4 && key <= ui::Key_OEM_8))
			return true;
		if((key >= ui::Key_NUMPAD0 && key <= ui::Key_NUMPAD9) && modifierMask == ModNone)
			return true;
		if(key == ui::Key_RETURN && edit != nullptr && edit->IsMultiline())
			return true;
	}

	// Navigation (any modifier except Alt without any other modifiers):
	if(modifierMask != ModAlt)
	{
		if(key == ui::Key_LEFT || key == ui::Key_RIGHT || key == ui::Key_UP || key == ui::Key_DOWN
		   || key == ui::Key_HOME || key == ui::Key_END || key == ui::Key_DELETE || key == ui::Key_INSERT || key == ui::Key_BACK)
			return true;
	}

	// Copy paste etc..
	if(modifierMask == ModCtrl)
	{
		if(key == 'Y' || key == 'Z' || key == 'X' ||  key == 'C' || key == 'V' || key == 'A')
			return true;
	}

	return false;
}


BypassInputHandler::BypassInputHandler()
{
	if(CMainFrame::GetInputHandler())
	{
		bypassed = true;
		CMainFrame::GetInputHandler()->Bypass(true);
	}
}


BypassInputHandler::~BypassInputHandler()
{
	if(bypassed)
	{
		CMainFrame::GetInputHandler()->Bypass(false);
		bypassed = false;
	}
}

OPENMPT_NAMESPACE_END
