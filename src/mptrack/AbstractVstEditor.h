/*
 * AbstractVstEditor.h
 * -------------------
 * Purpose: Common plugin editor interface class. This code is shared between custom and default plugin user interfaces.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "ResizableDialog.h"
#include "Moddoc.h"
#include "../soundlib/Snd_defs.h"

OPENMPT_NAMESPACE_BEGIN

class IMixPlugin;
struct UpdateHint;

class CAbstractVstEditor : public ResizableDialog
{
protected:
	Menu m_Menu;
	static uint32 m_clipboardFormat;
	int32 m_currentPresetMenu = 0;
	int m_nLearnMacro = -1;
	int m_nCurProg = -1;
	INSTRUMENTINDEX m_nInstrument;
	bool m_updateDisplay = false;
	bool m_hasPresetMenu = false;
	CModDoc::NoteToChannelMap m_noteChannel;	// Note -> Preview channel assignment

public:
	IMixPlugin &m_VstPlugin;

	CAbstractVstEditor(IMixPlugin &plugin);
	// IMixPlugin befriends this class, which makes it the only GUI class allowed to move a plugin
	static void SetPluginSlot(IMixPlugin &plugin, PLUGINDEX slot);
	virtual ~CAbstractVstEditor();

	void SetupMenu(bool force = false);
	void SetTitle();
	void SetLearnMacro(int inMacro);
	int GetLearnMacro();

	void SetPreset(int32 preset);
	void UpdatePresetField();

	void OnLoadPreset();
	void OnSavePreset();
	void OnCopyParameters();
	void OnPasteParameters();
	void OnRandomizePreset();
	void OnRenamePlugin();
	void OnSetPreset(uint32 nID);
	void OnBypassPlug();
	void OnRecordAutomation();
	void OnRecordMIDIOut();
	void OnPassKeypressesToPlug();
	void OnSetPreviousVSTPreset();
	void OnSetNextVSTPreset();
	void OnVSTPresetBackwardJump();
	void OnVSTPresetForwardJump();
	void OnVSTPresetRename();
	void OnCreateInstrument();
	void OnSelectPresetBank(uint32 nID);
	LResult OnCustomKeyMsg(WParam, LParam);
	LResult OnMidiMsg(WParam, LParam);
	bool CanDropFiles(Point) const override { return true; }
	void OnDropFiles(const std::vector<mpt::PathString> &files) override;
	void OnMove(int x, int y) override;
	void OnActivate(bool isActive) override;

	// Overridden methods:
	void PostNcDestroy() override;
	void OnOK() override { DoClose(); }
	void OnCancel() override { DoClose(); }

	virtual bool OpenEditor(Wnd *parent);
	virtual void DoClose();
	virtual void UpdateParamDisplays() { if(m_updateDisplay) { SetupMenu(true); m_updateDisplay = false; } }
	virtual void UpdateParam(int32 /*param*/) { }
	virtual void UpdateView(UpdateHint hint);

	virtual bool IsResizable() const = 0;
	virtual bool SetSize(int contentWidth, int contentHeight) = 0;

	void UpdateDisplay() { m_updateDisplay = true; }

	UI_DECLARE_MESSAGE_MAP()

protected:
	bool PreTranslateMessage(int event) override;
	bool HandleKeyMessage(int event, bool handleGlobal = false);
	void UpdatePresetMenu(bool force = false);
	void GeneratePresetMenu(int32 offset, Menu &parent) const;
	void UpdateInputMenu();
	void UpdateOutputMenu();
	void UpdateMacroMenu();
	void UpdateOptionsMenu();
	INSTRUMENTINDEX GetBestInstrumentCandidate() const;
	bool CheckInstrument(INSTRUMENTINDEX ins) const;
	bool ValidateCurrentInstrument();

	void OnInitMenu() override { SetupMenu(); }
	void OnToggleEditor(uint32 nID);
	void OnSetInputInstrument(uint32 nID);
	void PrepareToLearnMacro(uint32 nID);

	void StoreWindowPos();
	void RestoreWindowPos();
};

OPENMPT_NAMESPACE_END
