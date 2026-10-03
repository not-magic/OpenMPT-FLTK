// FLTK port of openmpt/mptrack/PlugNotFoundDlg.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "ResizableDialog.h"
#include "CListCtrl.h"

OPENMPT_NAMESPACE_BEGIN

struct VSTPluginLib;

class PlugNotFoundDialog : public ResizableDialog
{
	std::vector<VSTPluginLib *> &m_plugins;
	CListCtrlEx m_List;

public:
	PlugNotFoundDialog(std::vector<VSTPluginLib *> &plugins, Wnd *parent);

	void DoDataExchange(DataExchange *pDX) override;
	bool OnInitDialog() override;

	void OnRemove();

	UI_DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
