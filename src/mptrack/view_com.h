/*
 * view_com.h
 * ----------
 * Purpose: Song comments tab, lower panel.
 * Notes  : (currently none)
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/view_com.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "CListCtrl.h"
#include "Globals.h"
#include "../soundlib/modcommand.h"

OPENMPT_NAMESPACE_BEGIN

class CViewComments: public CModScrollView
{
public:
	CViewComments() = default;

protected:
	CModControlBar m_ToolBar;
	CListCtrlEx m_ItemList;
	ui::Font m_fixedFont;
	int m_nCurrentListId = 0, m_nListId = 0;
	ModCommand::NOTE m_lastNote = NOTE_NONE;
	CHANNELINDEX m_noteChannel = CHANNELINDEX_INVALID;
	INSTRUMENTINDEX m_noteInstr = INSTRUMENTINDEX_INVALID;
	bool m_editLabel = false;

public:
	void RecalcLayout();
	void UpdateButtonState();

public:
	void OnInitialUpdate() override;
	void OnDPIChanged() override;
	bool PreTranslateMessage(int event) override;
	LResult OnModViewMsg(WParam wParam, LParam lParam) override;
	void UpdateView(UpdateHint hint, HintObject *pObject = nullptr) override;

protected:
	bool SwitchToList(int list);

	// cppcheck-suppress duplInheritedMember
	void OnDestroy();
	void OnSize(uint32 nType, int cx, int cy);
	void OnShowSamples();
	void OnShowInstruments();
	void OnShowPatterns();
	void OnEndLabelEdit(NotifyHeader * pnmhdr, LResult *pLResult);
	void OnBeginLabelEdit(NotifyHeader * pnmhdr, LResult *pLResult);
	void OnDblClickListItem(NotifyHeader *, LResult *);
	void OnRClickListItem(NotifyHeader *, LResult *);
	void OnCopyNames();
	void OnPasteNames();
	LResult OnMidiMsg(WParam midiData, LParam);
	LResult OnCustomKeyMsg(WParam, LParam);
	UI_DECLARE_MESSAGE_MAP()
};


OPENMPT_NAMESPACE_END
