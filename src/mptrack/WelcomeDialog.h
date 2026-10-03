/*
 * WelcomeDialog.cpp
 * -----------------
 * Purpose: "First run" OpenMPT welcome dialog
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "DialogBase.h"
#include "../common/mptPathString.h"

OPENMPT_NAMESPACE_BEGIN

class WelcomeDlg : public DialogBase
{
protected:
	mpt::PathString m_vstPath;

public:
	WelcomeDlg(Wnd *parent);

protected:
	bool OnInitDialog() override;
	void OnOK() override;
	void OnCancel() override;
	void PostNcDestroy() override { DialogBase::PostNcDestroy(); delete this; }

	void OnOptions();
	void OnScanPlugins();

	UI_DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
