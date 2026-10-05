/*
 * AdvancedConfigDlg.h
 * -------------------
 * Purpose: Implementation of the advanced settings dialog.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/AdvancedConfigDlg.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "CListCtrl.h"
#include <vector>

OPENMPT_NAMESPACE_BEGIN

class SettingPath;

class CAdvancedSettingsList : public ListCtrl
{
private:
	std::vector<SettingPath> & m_indexToPath;
public:
	CAdvancedSettingsList(std::vector<SettingPath> & indexToPath) : m_indexToPath(indexToPath) {}
	ColorRef OnGetCellBkColor(int nRow, int nColumn) override;
	ColorRef OnGetCellTextColor(int nRow, int nColumn) override;
};


class COptionsAdvanced: public PropertyPage
{
protected:
	CAdvancedSettingsList m_List;
	std::vector<SettingPath> m_indexToPath;

public:
	COptionsAdvanced();
	~COptionsAdvanced();

protected:
	bool OnInitDialog() override;
	void OnOK() override;
	bool OnSetActive() override;
	void DoDataExchange(DataExchange* pDX) override;
	bool PreTranslateMessage(int event) override;

	void OnOptionDblClick(NotifyHeader *, LResult *);
	void OnSettingsChanged() { SetModified(true); }
	void OnFindStringChanged() { ReInit(); }
	void OnSaveNow();

	void ReInit();

	UI_DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
