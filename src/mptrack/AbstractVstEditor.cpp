// FLTK port of openmpt/mptrack/AbstractVstEditor.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "AbstractVstEditor.h"
#include "Clipboard.h"
#include "dlg_misc.h"
#include "Globals.h"
#include "InputHandler.h"
#include "MIDIMacros.h"
#include "Mainfrm.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "resource.h"
#include "VstPresets.h"
#include "Vstplug.h"
#include "WindowMessages.h"
#include "../common/FileReader.h"
#include "../common/mptStringBuffer.h"
#include "../soundlib/Sndfile.h"
#include "../soundlib/mod_specifications.h"
#include "../soundlib/plugins/PlugInterface.h"
#include "../soundlib/plugins/PluginManager.h"
#include "PluginUi.h"
#include "MIDIMacrosExt.h"

#include <sstream>


OPENMPT_NAMESPACE_BEGIN


#define PRESETS_PER_COLUMN 32
#define PRESETS_PER_GROUP 128

namespace
{
constexpr uint32 presetBankCommandBase = ID_PRESET_SET + 200;
}

uint32 CAbstractVstEditor::m_clipboardFormat = ui::RegisterClipboardFormat(UL_("VST Preset Data"));

UI_MESSAGE_MAP_BEGIN(CAbstractVstEditor, ResizableDialog)
	UI_COMMAND(ID_EDIT_COPY,			&CAbstractVstEditor::OnCopyParameters)
	UI_COMMAND(ID_EDIT_PASTE,			&CAbstractVstEditor::OnPasteParameters)
	UI_COMMAND(ID_PRESET_LOAD,			&CAbstractVstEditor::OnLoadPreset)
	UI_COMMAND(ID_PLUG_BYPASS,			&CAbstractVstEditor::OnBypassPlug)
	UI_COMMAND(ID_PLUG_RECORDAUTOMATION,&CAbstractVstEditor::OnRecordAutomation)
	UI_COMMAND(ID_PLUG_RECORD_MIDIOUT,  &CAbstractVstEditor::OnRecordMIDIOut)
	UI_COMMAND(ID_PLUG_PASSKEYS,		&CAbstractVstEditor::OnPassKeypressesToPlug)
	UI_COMMAND(ID_PRESET_SAVE,			&CAbstractVstEditor::OnSavePreset)
	UI_COMMAND(ID_PRESET_RANDOM,		&CAbstractVstEditor::OnRandomizePreset)
	UI_COMMAND(ID_RENAME_PLUGIN,		&CAbstractVstEditor::OnRenamePlugin)
	UI_COMMAND(ID_PREVIOUSVSTPRESET,	&CAbstractVstEditor::OnSetPreviousVSTPreset)
	UI_COMMAND(ID_NEXTVSTPRESET,		&CAbstractVstEditor::OnSetNextVSTPreset)
	UI_COMMAND(ID_VSTPRESETBACKWARDJUMP,&CAbstractVstEditor::OnVSTPresetBackwardJump)
	UI_COMMAND(ID_VSTPRESETFORWARDJUMP,	&CAbstractVstEditor::OnVSTPresetForwardJump)
	UI_COMMAND(ID_VSTPRESETNAME,		&CAbstractVstEditor::OnVSTPresetRename)
	UI_COMMAND(ID_PLUGINTOINSTRUMENT,	&CAbstractVstEditor::OnCreateInstrument)
	UI_COMMAND_RANGE(ID_PRESET_SET, ID_PRESET_SET + PRESETS_PER_GROUP, &CAbstractVstEditor::OnSetPreset)
	UI_COMMAND_RANGE(presetBankCommandBase, presetBankCommandBase + 255, &CAbstractVstEditor::OnSelectPresetBank)
	UI_MESSAGE(MSG_MOD_MIDIMSG,		&CAbstractVstEditor::OnMidiMsg)
	UI_MESSAGE(MSG_MOD_KEYCOMMAND,	&CAbstractVstEditor::OnCustomKeyMsg)
	UI_COMMAND_RANGE(ID_PLUGSELECT, ID_PLUGSELECT + MAX_MIXPLUGINS, &CAbstractVstEditor::OnToggleEditor)
	UI_COMMAND_RANGE(ID_SELECTINST, ID_SELECTINST + MAX_INSTRUMENTS, &CAbstractVstEditor::OnSetInputInstrument)
	UI_COMMAND_RANGE(ID_LEARN_MACRO_FROM_PLUGGUI, ID_LEARN_MACRO_FROM_PLUGGUI + kSFxMacros, &CAbstractVstEditor::PrepareToLearnMacro)
UI_MESSAGE_MAP_END()


CAbstractVstEditor::CAbstractVstEditor(IMixPlugin &plugin)
	: m_VstPlugin(plugin)
{
	m_Menu.LoadMenu(IDR_VSTMENU);
	m_nInstrument = GetBestInstrumentCandidate();
}


CAbstractVstEditor::~CAbstractVstEditor()
{
	PluginUi::OnEditorDestroyed(*this);
}


void CAbstractVstEditor::SetPluginSlot(IMixPlugin &plugin, PLUGINDEX slot)
{
	plugin.m_nSlot = slot;
	plugin.m_pMixStruct = &plugin.GetSoundFile().m_MixPlugins[slot];
}


void CAbstractVstEditor::PostNcDestroy()
{
	ResizableDialog::PostNcDestroy();
	delete this;
}


void CAbstractVstEditor::OnActivate(bool isActive)
{
	if(isActive)
	{
		auto callback = [&plugin = m_VstPlugin](mpt::const_byte_span sysex) { plugin.MidiSend(sysex); };
		CMainFrame::GetMainFrame()->SetMidiRecordWnd(this, callback);
	}
}


LResult CAbstractVstEditor::OnMidiMsg(WParam midiData, LParam sender)
{
	CModDoc *modDoc = PluginUi(m_VstPlugin).GetModDoc();
	if(modDoc != nullptr && sender != reinterpret_cast<LParam>(&m_VstPlugin))
	{
		if(!CheckInstrument(m_nInstrument))
			m_nInstrument = GetBestInstrumentCandidate();
		modDoc->ProcessMIDI((uint32)midiData, 0, m_nInstrument, &m_VstPlugin, kCtxVSTGUI);
		return 1;
	}
	return 0;
}


void CAbstractVstEditor::OnDropFiles(const std::vector<mpt::PathString> &files)
{
	CMainFrame::GetMainFrame()->SetForegroundWindow();
	for(const mpt::PathString &file : files)
		PluginUi(m_VstPlugin).LoadProgram(file);
}


void CAbstractVstEditor::OnLoadPreset()
{
	if(PluginUi(m_VstPlugin).LoadProgram())
	{
		UpdatePresetMenu(true);
		UpdatePresetField();
	}
}


void CAbstractVstEditor::OnSavePreset()
{
	PluginUi(m_VstPlugin).SaveProgram();
}


void CAbstractVstEditor::OnCopyParameters()
{
	if(CMainFrame::GetMainFrame() == nullptr) return;

	BeginWaitCursor();
	std::ostringstream f(std::ios::out | std::ios::binary);
	if(VSTPresets::SaveFile(f, m_VstPlugin, false))
	{
		const std::string data = f.str();
		Clipboard clipboard(m_clipboardFormat, data.length());
		if(auto dst = clipboard.As<char>())
		{
			memcpy(dst, data.data(), data.length());
		}
	}
	EndWaitCursor();
}


void CAbstractVstEditor::OnPasteParameters()
{
	if(CMainFrame::GetMainFrame() == nullptr) return;

	BeginWaitCursor();
	Clipboard clipboard(m_clipboardFormat);
	if(auto data = clipboard.Get(); data.data())
	{
		FileReader file(data);
		VSTPresets::ErrorCode error = VSTPresets::LoadFile(file, m_VstPlugin);
		clipboard.Close();

		if(error == VSTPresets::noError)
		{
			const CTrackerSoundFile &sndFile = TrackerSoundFile(m_VstPlugin.GetSoundFile());
			CModDoc *pModDoc;
			if(sndFile.GetModSpecifications().supportsPlugins && (pModDoc = sndFile.GetpModDoc()) != nullptr)
			{
				pModDoc->SetModified();
			}
					UpdatePresetField();
		} else
		{
			Reporting::Error(VSTPresets::GetErrorMessage(error));
		}
	}
	EndWaitCursor();
}


void CAbstractVstEditor::OnRandomizePreset()
{
	static double randomFactor = 10.0;
	CInputDlg dlg(this, UL_("Input parameter randomization amount (0 = no change, 100 = completely random)"), 0.0, 100.0, randomFactor);
	if(dlg.DoModal() == IDOK)
	{
		randomFactor = dlg.resultAsDouble;
		PlugParamValue factor = PlugParamValue(randomFactor / 100.0);
		PlugParamIndex numParams = m_VstPlugin.GetNumVisibleParameters();
		for(PlugParamIndex p = 0; p < numParams; p++)
		{
			PlugParamValue val = m_VstPlugin.GetParameter(p);
			val += mpt::random(theApp.PRNG(), PlugParamValue(-1.0), PlugParamValue(1.0)) * factor;
			Limit(val, 0.0f, 1.0f);
			m_VstPlugin.SetParameter(p, val);
		}
		UpdateParamDisplays();
	}
}


void CAbstractVstEditor::OnRenamePlugin()
{
	CTrackerSoundFile &sndFile = TrackerSoundFile(m_VstPlugin.GetSoundFile());
	auto &plugin = sndFile.m_MixPlugins[m_VstPlugin.m_nSlot];

	CInputDlg dlg(this, UL_("New name for this plugin instance:"), mpt::ToUnicode(plugin.GetName()), static_cast<int32>(std::size(plugin.Info.szName.buf)));
	if(dlg.DoModal() == IDOK)
	{
		if(dlg.resultAsString != mpt::ToUnicode(plugin.GetName()))
		{
			plugin.Info.szName = mpt::ToCharset(mpt::Charset::Locale, dlg.resultAsString);
			if(auto *modDoc = sndFile.GetpModDoc(); modDoc != nullptr)
			{
				if(sndFile.GetModSpecifications().supportsPlugins)
					modDoc->SetModified();
				modDoc->UpdateAllViews(nullptr, PluginHint(m_VstPlugin.m_nSlot + 1).Info().Names(), this);
			}
			SetTitle();
		}
	}
}


bool CAbstractVstEditor::OpenEditor(Wnd *)
{
	RestoreWindowPos();
	SetTitle();
	SetupMenu();
	ShowWindow(true);
	return true;
}


void CAbstractVstEditor::DoClose()
{
	StoreWindowPos();
	DestroyWindow();
}


void CAbstractVstEditor::SetupMenu(bool force)
{
	SetMenu(&m_Menu);
	UpdatePresetMenu(force);
	UpdateInputMenu();
	UpdateOutputMenu();
	UpdateMacroMenu();
	UpdateOptionsMenu();
	UpdatePresetField();
}


void CAbstractVstEditor::UpdatePresetField()
{
	if(m_VstPlugin.GetNumPrograms() > 0)
	{
		if(m_Menu.GetMenuItemCount() < 5)
		{
			m_Menu.AppendMenu(0, ID_VSTPRESETBACKWARDJUMP, UL_("<<"));
			m_Menu.AppendMenu(0, ID_PREVIOUSVSTPRESET, UL_("<"));
			m_Menu.AppendMenu(0, ID_NEXTVSTPRESET, UL_(">"));
			m_Menu.AppendMenu(0, ID_VSTPRESETFORWARDJUMP, UL_(">>"));
			m_Menu.AppendMenu(ui::MenuItemGrayed, ID_VSTPRESETNAME, UL_(""));
		}

		mpt::ustring programName = PluginUi(m_VstPlugin).GetFormattedProgramName(m_VstPlugin.GetCurrentProgram());
		programName = mpt::replace(programName, U_("&"), U_("&&"));
		m_Menu.ModifyMenu(ID_VSTPRESETNAME, 0, ID_VSTPRESETNAME, programName);
	}

	DrawMenuBar();
}


void CAbstractVstEditor::OnSetPreset(uint32 nID)
{
	SetPreset(nID - ID_PRESET_SET + m_currentPresetMenu * PRESETS_PER_GROUP);
}


void CAbstractVstEditor::OnSetPreviousVSTPreset()
{
	SetPreset(m_VstPlugin.GetCurrentProgram() - 1);
}


void CAbstractVstEditor::OnSetNextVSTPreset()
{
	SetPreset(m_VstPlugin.GetCurrentProgram() + 1);
}


void CAbstractVstEditor::OnVSTPresetBackwardJump()
{
	SetPreset(std::max(0, m_VstPlugin.GetCurrentProgram() - 10));
}


void CAbstractVstEditor::OnVSTPresetForwardJump()
{
	SetPreset(std::min(m_VstPlugin.GetCurrentProgram() + 10, m_VstPlugin.GetNumPrograms() - 1));
}


void CAbstractVstEditor::SetPreset(int32 preset)
{
	if(preset >= 0 && preset < m_VstPlugin.GetNumPrograms())
	{
		m_VstPlugin.SetCurrentProgram(preset);
			UpdatePresetField();

		if(m_VstPlugin.GetSoundFile().GetModSpecifications().supportsPlugins)
		{
			PluginUi(m_VstPlugin).GetModDoc()->SetModified();
		}
	}
}


void CAbstractVstEditor::OnVSTPresetRename()
{
	auto currentName = PluginUi(m_VstPlugin).GetCurrentProgramName();
	CInputDlg dlg(this, UL_("New program name:"), currentName);
	if(dlg.DoModal() == IDOK)
	{
		PluginUi(m_VstPlugin).SetCurrentProgramName(dlg.resultAsString);
		if(PluginUi(m_VstPlugin).GetCurrentProgramName() != currentName)
		{
			PluginUi(m_VstPlugin).SetModified();
					UpdatePresetField();
			UpdatePresetMenu(true);
		}
	}
}


void CAbstractVstEditor::OnBypassPlug()
{
	m_VstPlugin.ToggleBypass();
	if(m_VstPlugin.GetSoundFile().GetModSpecifications().supportsPlugins)
	{
		PluginUi(m_VstPlugin).GetModDoc()->SetModified();
	}
	SetTitle();
}


void CAbstractVstEditor::OnRecordAutomation()
{
	m_VstPlugin.m_recordAutomation = !m_VstPlugin.m_recordAutomation;
}


void CAbstractVstEditor::OnRecordMIDIOut()
{
	m_VstPlugin.m_recordMIDIOut = !m_VstPlugin.m_recordMIDIOut;
}


void CAbstractVstEditor::OnPassKeypressesToPlug()
{
	m_VstPlugin.m_passKeypressesToPlug  = !m_VstPlugin.m_passKeypressesToPlug;
}


bool CAbstractVstEditor::PreTranslateMessage(int event)
{
	if(HandleKeyMessage(event))
		return true;

	return ResizableDialog::PreTranslateMessage(event);
}


bool CAbstractVstEditor::HandleKeyMessage(int event, bool handleGlobal)
{
	if(m_VstPlugin.m_passKeypressesToPlug)
		return false;
	if(event != FL_KEYBOARD && event != FL_KEYUP)
		return false;

	const uint32 key = ui::KeyFromEvent();
	const uint32 flags = (event == FL_KEYUP) ? ui::KeyFlagRelease : 0;
	CInputHandler *ih = CMainFrame::GetInputHandler();
	if(ih->IsKeyPressHandledByTextBox(key, GetFocus()))
		return false;

	const auto keyEvent = ih->Translate(key, 1, flags);

	// If we successfully mapped to a command and plug does not listen for keypresses, no need to pass message on.
	if(ih->KeyEvent(kCtxVSTGUI, keyEvent, this) != kcNull)
		return true;

	if(handleGlobal && HandleGlobalKeyMessage(key, flags))
		return true;

	// Don't forward key repeats if plug does not listen for keypresses
	// (avoids system beeps on note hold)
	if(keyEvent.keyEventType == kKeyEventRepeat)
		return true;

	return false;
}


void CAbstractVstEditor::UpdateView(UpdateHint hint)
{
	if(!hint.GetType()[HINT_PLUGINNAMES | HINT_MIXPLUGINS])
		return;

	PLUGINDEX hintPlug = hint.ToType<PluginHint>().GetPlugin();
	if(hintPlug > 0 && (hintPlug - 1) != PluginUi(m_VstPlugin).GetSlot())
		return;

	SetTitle();
}


void CAbstractVstEditor::SetTitle()
{
	if(m_VstPlugin.m_pMixStruct)
	{
		mpt::ustring title = MPT_UFORMAT("FX {}: ")(mpt::ufmt::dec0<2>(m_VstPlugin.m_nSlot + 1));

		bool hasCustomName = (m_VstPlugin.m_pMixStruct->GetName() != UL_("")) && (m_VstPlugin.m_pMixStruct->GetName() != m_VstPlugin.m_pMixStruct->GetLibraryName());
		if(hasCustomName)
			title += mpt::ToUnicode(m_VstPlugin.m_pMixStruct->GetName()) + UL_(" (");
		title += mpt::ToUnicode(m_VstPlugin.m_pMixStruct->GetLibraryName());
		if(hasCustomName)
			title += UL_(")");

#ifdef MPT_WITH_VST
		const CVstPlugin *vstPlugin = dynamic_cast<CVstPlugin *>(&m_VstPlugin);
		if(vstPlugin != nullptr && vstPlugin->isBridged)
			title += MPT_UFORMAT(" ({} Bridged)")(m_VstPlugin.GetPluginFactory().GetDllArchNameUser());
#endif // MPT_WITH_VST

		if(m_VstPlugin.IsBypassed())
			title += UL_(" - Bypass");

		SetWindowText(title);
	}
}


LResult CAbstractVstEditor::OnCustomKeyMsg(WParam wParam, LParam /*lParam*/)
{
	switch(wParam)
	{
		case kcVSTGUIPrevPreset:			OnSetPreviousVSTPreset(); return wParam;
		case kcVSTGUIPrevPresetJump:		OnVSTPresetBackwardJump(); return wParam;
		case kcVSTGUINextPreset:			OnSetNextVSTPreset(); return wParam;
		case kcVSTGUINextPresetJump:		OnVSTPresetForwardJump(); return wParam;
		case kcVSTGUIRandParams:			OnRandomizePreset() ; return wParam;
		case kcVSTGUIToggleRecordParams:	OnRecordAutomation(); return wParam;
		case kcVSTGUIToggleSendKeysToPlug:	OnPassKeypressesToPlug(); return wParam;
		case kcVSTGUIBypassPlug:			OnBypassPlug(); return wParam;
	}
	if (wParam >= kcVSTGUIStartNotes && wParam <= kcVSTGUIEndNotes)
	{
		if(ValidateCurrentInstrument())
		{
			CModDoc *pModDoc = PluginUi(m_VstPlugin).GetModDoc();
			const ModCommand::NOTE note = pModDoc->GetNoteWithBaseOctave(static_cast<int>(wParam - kcVSTGUIStartNotes), m_nInstrument);
			if(ModCommand::IsNote(note))
			{
				pModDoc->PlayNote(PlayNoteParam(note).Instrument(m_nInstrument), &m_noteChannel);
			}
		}
		return wParam;
	}
	if (wParam >= kcVSTGUIStartNoteStops && wParam <= kcVSTGUIEndNoteStops)
	{
		if(ValidateCurrentInstrument())
		{
			CModDoc *pModDoc = PluginUi(m_VstPlugin).GetModDoc();
			const ModCommand::NOTE note = pModDoc->GetNoteWithBaseOctave(static_cast<int>(wParam - kcVSTGUIStartNoteStops), m_nInstrument);
			if(ModCommand::IsNote(note))
			{
				pModDoc->NoteOff(note, false, m_nInstrument, m_noteChannel[note - NOTE_MIN]);
			}
		}
		return wParam;
	}

	return kcNull;
}


// When trying to play a note using this plugin, but no instrument is assigned to it,
// the user is asked whether a new instrument should be added.
bool CAbstractVstEditor::ValidateCurrentInstrument()
{
	if(!CheckInstrument(m_nInstrument))
		m_nInstrument = GetBestInstrumentCandidate();

	//only show messagebox if plug is able to process notes.
	if(m_nInstrument == INSTRUMENTINDEX_INVALID)
	{
		if(m_VstPlugin.CanRecieveMidiEvents())
		{
			// We might need to steal the focus from the plugin bridge. This is going to work
			// as the plugin bridge will call AllowSetForegroundWindow on key messages.
			SetForegroundWindow();
			if(!m_VstPlugin.IsInstrument() || m_VstPlugin.GetSoundFile().GetModSpecifications().instrumentsMax == 0 ||
				Reporting::Confirm(UL_("You need to assign an instrument to this plugin before you can play notes from here.\nCreate a new instrument and assign this plugin to the instrument?"), false, false, this) == cnfNo)
			{
				return false;
			} else
			{
				OnCreateInstrument();
				// Return true since we don't want to trigger the note for which the instrument has been validated yet.
				// Otherwise, the note might hang forever because the key-up event will go missing.
				return false;
			}
		} else
		{
			// Can't process notes
			return false;
		}
	}
	return true;

}


static int GetNumSubMenus(int32 numProgs) { return (numProgs + (PRESETS_PER_GROUP - 1)) / PRESETS_PER_GROUP; }


namespace
{
void ReplacePopup(Menu &menu, uint32 position, const mpt::ustring &text, const Menu &subMenu)
{
	menu.RemoveMenu(position, true);
	menu.InsertMenu(position, 0, subMenu, text);
}
}


void CAbstractVstEditor::OnSelectPresetBank(uint32 nID)
{
	const int32 bank = static_cast<int32>(nID - presetBankCommandBase);
	Menu bankMenu;
	GeneratePresetMenu(bank * PRESETS_PER_GROUP, bankMenu);
	const uint32 command = bankMenu.TrackPopupMenu(Point(Fl::event_x_root(), Fl::event_y_root()), this);
	if(command >= ID_PRESET_SET && command <= ID_PRESET_SET + PRESETS_PER_GROUP)
		SetPreset(command - ID_PRESET_SET + bank * PRESETS_PER_GROUP);
}


void CAbstractVstEditor::UpdatePresetMenu(bool force)
{
	const int32 numProgs = m_VstPlugin.GetNumPrograms();
	const int32 curProg  = m_VstPlugin.GetCurrentProgram();

	if(m_hasPresetMenu && curProg == m_nCurProg && !force)
		return;

	Menu presetMenu;
	const int numSubMenus = GetNumSubMenus(numProgs);
	if(numSubMenus > 1)
	{
		// Filling a bank can take quite a while (e.g. Synth1), so the presets of a bank are only listed when it is chosen
		for(int bank = 0, prog = 0; bank < numSubMenus; bank++, prog += PRESETS_PER_GROUP)
		{
			const mpt::ustring label = MPT_UFORMAT("Bank {} ({}-{})")(bank + 1, prog + 1, std::min(prog + PRESETS_PER_GROUP, numProgs));
			presetMenu.AppendMenu(curProg >= prog && curProg < prog + PRESETS_PER_GROUP ? ui::MenuItemChecked : 0, presetBankCommandBase + bank, label);
		}
	} else
	{
		GeneratePresetMenu(0, presetMenu);
	}

	if(m_hasPresetMenu)
		m_Menu.RemoveMenu(1, true);
	m_Menu.InsertMenu(1, numProgs ? 0 : ui::MenuItemGrayed, presetMenu, UL_("&Presets"));
	m_hasPresetMenu = true;

	m_currentPresetMenu = 0;
	m_nCurProg = curProg;
}


void CAbstractVstEditor::GeneratePresetMenu(int32 offset, Menu &parent) const
{
	const int32 numProgs = m_VstPlugin.GetNumPrograms();
	const int32 curProg  = m_VstPlugin.GetCurrentProgram();
	const int32 endProg = std::min(offset + PRESETS_PER_GROUP, numProgs);

	for(int32 p = offset, id = 0; p < endProg; p++, id++)
	{
		mpt::ustring programName = PluginUi(m_VstPlugin).GetFormattedProgramName(p);
		programName = mpt::replace(programName, U_("&"), U_("&&"));
		parent.AppendMenu(p == curProg ? ui::MenuItemChecked : 0, ID_PRESET_SET + id, programName);
	}
}


void CAbstractVstEditor::UpdateInputMenu()
{
	Menu *pInfoMenu = m_Menu.GetSubMenu(2);
	const CTrackerSoundFile &sndFile = TrackerSoundFile(m_VstPlugin.GetSoundFile());
	Menu inputMenu;

	std::vector<IMixPlugin *> inputPlugs;
	m_VstPlugin.GetInputPlugList(inputPlugs);
	for(auto plug : inputPlugs)
	{
		mpt::ustring name = MPT_UFORMAT("FX{}: {}")(mpt::ufmt::dec0<2>(plug->m_nSlot + 1), mpt::ToUnicode(plug->m_pMixStruct->GetName()));
		inputMenu.AppendMenu(ui::MenuItemString, ID_PLUGSELECT + plug->m_nSlot, name);
	}

	std::vector<CHANNELINDEX> inputChannels;
	m_VstPlugin.GetInputChannelList(inputChannels);
	bool addSeparator = !inputPlugs.empty();
	for(auto chn : inputChannels)
	{
		if(addSeparator)
		{
			inputMenu.AppendMenu(ui::MenuItemSeparator, 0, {});
			addSeparator = false;
		}
		mpt::ustring name = MPT_UFORMAT("Chn{}: {}")(mpt::ufmt::dec0<2>(chn + 1), mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.ChnSettings[chn].szName));
		inputMenu.AppendMenu(ui::MenuItemString, 0, name);
	}

	std::vector<INSTRUMENTINDEX> inputInstruments;
	m_VstPlugin.GetInputInstrumentList(inputInstruments);
	addSeparator = !inputPlugs.empty() || !inputChannels.empty();
	for(auto ins : inputInstruments)
	{
		if(addSeparator)
		{
			inputMenu.AppendMenu(ui::MenuItemSeparator, 0, {});
			addSeparator = false;
		}
		mpt::ustring name = MPT_UFORMAT("Ins{}: {}")(mpt::ufmt::dec0<2>(ins), mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.GetInstrumentName(ins)));
		inputMenu.AppendMenu(ui::MenuItemString | ((ins == m_nInstrument) ? ui::MenuItemChecked : 0), ID_SELECTINST + ins, name);
	}

	if(inputPlugs.empty() && inputChannels.empty() && inputInstruments.empty())
	{
		inputMenu.AppendMenu(ui::MenuItemString | ui::MenuItemGrayed, 0, UL_("None"));
	}

	ReplacePopup(*pInfoMenu, 0, UL_("I&nputs"), inputMenu);
}


void CAbstractVstEditor::UpdateOutputMenu()
{
	Menu *pInfoMenu = m_Menu.GetSubMenu(2);
	Menu outputMenu;

	std::vector<IMixPlugin *> outputPlugs;
	m_VstPlugin.GetOutputPlugList(outputPlugs);

	for(auto plug : outputPlugs)
	{
		if(plug != nullptr)
		{
			const mpt::ustring name = MPT_UFORMAT("FX{}: {}")(mpt::ufmt::dec0<2>(plug->m_nSlot + 1), mpt::ToUnicode(plug->m_pMixStruct->GetName()));
			outputMenu.AppendMenu(ui::MenuItemString, ID_PLUGSELECT + plug->m_nSlot, name);
		} else
		{
			outputMenu.AppendMenu(ui::MenuItemString | ui::MenuItemGrayed, 0, UL_("Master Output"));
		}
	}
	ReplacePopup(*pInfoMenu, 1, UL_("Ou&tputs"), outputMenu);
}


void CAbstractVstEditor::UpdateMacroMenu()
{
	Menu *pInfoMenu = m_Menu.GetSubMenu(2);
	Menu macroMenu;

	const MIDIMacroConfig &midiCfg = m_VstPlugin.GetSoundFile().m_MidiCfg;
	for(int nMacro = 0; nMacro < kSFxMacros; nMacro++)
	{
		uint32 action = 0;
		uint32 greyed = ui::MenuItemGrayed;
		mpt::ustring macroName;

		const ParameteredMacro macroType = midiCfg.GetParameteredMacroType(nMacro);

		if(macroType == kSFxUnused)
		{
			macroName = UL_("Unused. Learn Param...");
			action = ID_LEARN_MACRO_FROM_PLUGGUI + nMacro;
			greyed = 0;
		} else
		{
			macroName = GetParameteredMacroName(midiCfg, nMacro, &m_VstPlugin);
			if(macroType != kSFxPlugParam || macroName.substr(0, 3) != UL_("N/A"))
			{
				greyed = 0;
			}
		}

		macroMenu.AppendMenu(ui::MenuItemString | greyed, action, MPT_UFORMAT("SF{}: {}")(mpt::ufmt::HEX(nMacro), macroName));
	}

	ReplacePopup(*pInfoMenu, 2, UL_("&Macros"), macroMenu);
}

void CAbstractVstEditor::UpdateOptionsMenu()
{
	CInputHandler *ih = CMainFrame::GetInputHandler();
	Menu optionsMenu;

	optionsMenu.AppendMenu(ui::MenuItemString | (m_VstPlugin.IsBypassed() ? ui::MenuItemChecked : 0),
	                       ID_PLUG_BYPASS, ih->GetKeyTextFromCommand(kcVSTGUIBypassPlug, UL_("&Bypass Plugin")));
	optionsMenu.AppendMenu(ui::MenuItemString | (m_VstPlugin.m_recordAutomation ? ui::MenuItemChecked : 0),
	                       ID_PLUG_RECORDAUTOMATION, ih->GetKeyTextFromCommand(kcVSTGUIToggleRecordParams, UL_("Record &Parameter Changes")));
	optionsMenu.AppendMenu(ui::MenuItemString | (m_VstPlugin.m_recordMIDIOut ? ui::MenuItemChecked : 0),
	                       ID_PLUG_RECORD_MIDIOUT, ih->GetKeyTextFromCommand(kcVSTGUIToggleRecordMIDIOut, UL_("Record &MIDI Out to Pattern Editor")));
	optionsMenu.AppendMenu(ui::MenuItemString | (m_VstPlugin.m_passKeypressesToPlug ? ui::MenuItemChecked : 0),
	                       ID_PLUG_PASSKEYS, ih->GetKeyTextFromCommand(kcVSTGUIToggleSendKeysToPlug, UL_("Pass &Keys to Plugin")));

	ReplacePopup(m_Menu, m_hasPresetMenu ? 3 : 2, UL_("&Options"), optionsMenu);
}


void CAbstractVstEditor::OnToggleEditor(uint32 nID)
{
	CModDoc *pModDoc = PluginUi(m_VstPlugin).GetModDoc();

	if(pModDoc)
	{
		pModDoc->TogglePluginEditor(nID - ID_PLUGSELECT);
	}
}


bool CAbstractVstEditor::CheckInstrument(INSTRUMENTINDEX ins) const
{
	const CTrackerSoundFile &sndFile = TrackerSoundFile(m_VstPlugin.GetSoundFile());

	if(ins != INSTRUMENTINDEX_INVALID && ins < MAX_INSTRUMENTS && sndFile.Instruments[ins] != nullptr)
	{
		return (sndFile.Instruments[ins]->nMixPlug) == (m_VstPlugin.m_nSlot + 1);
	}
	return false;
}


INSTRUMENTINDEX CAbstractVstEditor::GetBestInstrumentCandidate() const
{
	// First try current instrument:
	const CModDoc *modDoc = PluginUi(m_VstPlugin).GetModDoc();
	for(const View *view : modDoc->GetViews())
	{
		const CModControlView *pView = dynamic_cast<const CModControlView *>(view);
		if(pView != nullptr && pView->GetDocument() == modDoc)
		{
			INSTRUMENTINDEX ins = static_cast<INSTRUMENTINDEX>(pView->GetInstrumentChange());
			if(CheckInstrument(ins))
				return ins;
		}
	}

	// Just take the first instrument that points to this plug..
	return modDoc->HasInstrumentForPlugin(m_VstPlugin.m_nSlot);
}


void CAbstractVstEditor::OnSetInputInstrument(uint32 nID)
{
	m_nInstrument = static_cast<INSTRUMENTINDEX>(nID - ID_SELECTINST);
}


void CAbstractVstEditor::OnCreateInstrument()
{
	if(PluginUi(m_VstPlugin).GetModDoc() != nullptr)
	{
		INSTRUMENTINDEX instr = PluginUi(m_VstPlugin).GetModDoc()->InsertInstrumentForPlugin(PluginUi(m_VstPlugin).GetSlot());
		if(instr != INSTRUMENTINDEX_INVALID) m_nInstrument  = instr;
	}
}


void CAbstractVstEditor::PrepareToLearnMacro(uint32 nID)
{
	m_nLearnMacro = (nID-ID_LEARN_MACRO_FROM_PLUGGUI);
	//Now we wait for a param to be touched. We'll get the message from the VST Plug Manager.
	//Then pModDoc->LearnMacro(macro, param) is called
}

void CAbstractVstEditor::SetLearnMacro(int inMacro)
{
	if (inMacro < kSFxMacros)
	{
		m_nLearnMacro=inMacro;
	}
}

int CAbstractVstEditor::GetLearnMacro()
{
	return m_nLearnMacro;
}


void CAbstractVstEditor::OnMove(int, int)
{
	if(IsWindowVisible())
	{
		StoreWindowPos();
	}
}


void CAbstractVstEditor::StoreWindowPos()
{
	if(Fl_Window *frame = GetFrameWindow())
		PluginUi(m_VstPlugin).SetEditorPos(frame->x(), frame->y());
}


void CAbstractVstEditor::RestoreWindowPos()
{
	int32 editorX, editorY;
	PluginUi(m_VstPlugin).GetEditorPos(editorX, editorY);

	if(editorX != int32_min && editorY != int32_min)
	{
		if(Fl_Window *frame = GetFrameWindow())
			frame->position(editorX, editorY);
	}
}


OPENMPT_NAMESPACE_END
