/*
 * GeneralConfigDlg.h
 * ------------------
 * Purpose: Implementation of the general settings dialog.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

OPENMPT_NAMESPACE_BEGIN

class COptionsGeneral: public PropertyPage
{
protected:
	Edit m_defaultArtist;
	ComboBox m_defaultTemplate, m_defaultFormat;
	ListBox m_CheckList;
	Button m_templateBrowseButton;

public:
	COptionsGeneral();

protected:
	bool OnInitDialog() override;
	void OnOK() override;
	bool OnSetActive() override;
	void DoDataExchange(DataExchange* pDX) override;

	void OnOptionSelChanged();
	void OnSettingsChanged() { SetModified(true); }
	void OnBrowseTemplate();
	void OnDefaultTypeChanged();
	void OnTemplateChanged();

	UI_DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
