/*
 * KeyConfigDlg.h
 * --------------
 * Purpose: Implementation of OpenMPT's keyboard configuration dialog.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/KeyConfigDlg.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"
#include "CListCtrl.h"
#include "CommandSet.h"

OPENMPT_NAMESPACE_BEGIN

class COptionsKeyboard;

class CommandCategory
{
public:
	CommandCategory(const mpt::uchar *n, InputTargetContext ctx) : name{n}, id{ctx} {}

	const mpt::ustring name;
	const InputTargetContext id;
	
	struct Range
	{
		Range(CommandID f, CommandID l, const mpt::uchar *n) : first{f}, last{l}, name{n} {}
		const CommandID first, last;
		const mpt::ustring name;
	};
	std::vector<Range> commandRanges;
};


class CCustEdit: public Edit
{
protected:
	COptionsKeyboard *m_pOptKeyDlg = nullptr;
	bool m_isFocussed = false;
	bool m_bypassed = false;

public:
	FlagSet<Modifiers> mod = ModNone;
	uint32 code = 0;

	void SetOwner(COptionsKeyboard &dlg) { m_pOptKeyDlg = &dlg; }
	void SetKey(FlagSet<Modifiers> mod, uint32 code);
	bool HasKey() const noexcept { return mod || code; }

	void Bypass(bool bypass) { m_bypassed = bypass; EnableWindow(bypass ? false : true); }
	bool IsBypassed() const { return m_bypassed; }
	
protected:
	bool PreTranslateMessage(int event) override;
	
	void OnSetFocus(Wnd *pOldWnd);
	void OnKillFocus(Wnd *pNewWnd);
	LResult OnMidiMsg(WParam, LParam);
	
	UI_DECLARE_MESSAGE_MAP()
};

class COptionsKeyboard: public PropertyPage
{
	friend class CCustEdit;

protected:
	ListBox m_lbnHotKeys;
	CListCtrlEx m_lbnCommandKeys;
	ComboBox m_cmbKeyChoice;
	ComboBox m_cmbCategory;
	Button m_bKeyDown, m_bKeyHold, m_bKeyUp;
	Button m_bnReset;
	CCustEdit m_eCustHotKey, m_eFindHotKey;
	Edit m_eFind;
	Edit m_eChordWaitTime;
	Button m_restoreDefaultButton;
	Static m_warnIconCtl, m_warnText;
	ui::Bitmap m_infoIcon, m_warnIcon;
	
	mpt::ustring m_lastWarning;
	std::vector<CommandCategory> commandCategories;
	std::unique_ptr<CCommandSet> m_localCmdSet;
	mpt::PathString m_fullPathName;
	CommandID m_curCommand = kcNull;
	int m_curCategory = -1, m_curKeyChoice = -1;
	int m_lockCount = 0;
	bool m_forceUpdate = false;

public:
	COptionsKeyboard();

protected:
	bool OnInitDialog() override;
	void OnOK() override;
	bool OnSetActive() override;
	void DoDataExchange(DataExchange* pDX) override;

	void DefineCommandCategories();
	void ForceUpdateGUI(bool updateAllKeys = false);
	void UpdateNoteRepeatCheckbox();
	void UpdateShortcutList(int category = -1);
	void UpdateCategory();
	int GetCategoryFromCommandID(CommandID command) const;
	void OnCancelKeyChoice(const Wnd *source);
	void OnSetKeyChoice(const Wnd *source);

	void LockControls() { m_lockCount++; }
	void UnlockControls() { m_lockCount--; MPT_ASSERT(m_lockCount >= 0); }
	bool IsLocked() const noexcept { return m_lockCount != 0; }

	void EnableKeyChoice(bool enable);

	void UpdateWarning(mpt::ustring text = {}, bool notify = false);
	void ShowRestoreKeymapMenu();
	void RestoreKeymap(KeyboardPreset preset);

	void UpdateDialog();
	void OnKeyboardChanged();
	void OnKeyChoiceSelect();
	void OnCommandKeySelChanged(NotifyHeader *pNMHDR = nullptr, LResult *pResult = nullptr);
	void OnListenForKeysFromList(NotifyHeader *pNMHDR, LResult *pResult);
	void OnCategorySelChanged();
	void OnSearchTermChanged();
	void OnChordWaitTimeChanged();
	void OnSettingsChanged() { SetModified(true); }
	void OnCheck() { OnSetKeyChoice(&m_eCustHotKey); };
	void OnToggleNotesRepeat();
	void OnListenForKeys();
	void OnDeleteKeyChoice();
	void OnRestoreKeyChoice();
	void OnLoad();
	void OnSave();
	void OnRestoreDefaultKeymap();
	void OnRestoreKeymapDropdown(NotifyHeader *, LResult *result);
	void OnRestoreMPTKeymap() { RestoreKeymap(KeyboardPreset::MPT); }
	void OnRestoreITKeymap() { RestoreKeymap(KeyboardPreset::IT); }
	void OnRestoreFT2Keymap() { RestoreKeymap(KeyboardPreset::FT2); }
	void OnClearHotKey();
	void OnClearSearch();
	void OnEnableFindHotKey();
	void OnFindHotKey();
	void OnLButtonDblClk(uint32 flags, Point point);
	void OnDestroy();

	UI_DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
