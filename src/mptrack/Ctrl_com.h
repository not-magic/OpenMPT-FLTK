/*
 * Ctrl_com.h
 * ----------
 * Purpose: Song comments tab, upper panel.
 * Notes  : (currently none)
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"
#include "Globals.h"

OPENMPT_NAMESPACE_BEGIN

class CCtrlComments final : public CModControlDlg
{
protected:
	Edit m_EditComments;
	int charWidth = 0;
	bool m_Reformatting = false;

public:
	CCtrlComments(CModControlView &parent, CModDoc &document);

	Setting<int32> &GetSplitPosRef() override;
	bool OnInitDialog() override;
	void OnDPIChanged() override;
	void DoDataExchange(DataExchange *pDX) override;  // DDX/DDV support
	void RecalcLayout() override;
	void UpdateView(UpdateHint hint, HintObject *pObj = nullptr) override;
	ViewType GetAssociatedViewType() override;
	void OnActivatePage(LParam) override;
	void OnDeactivatePage() override;
	bool PreTranslateMessage(int event) override;

protected:
	void OnCommentsUpdated();
	void OnCommentsChanged();
	UI_DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
