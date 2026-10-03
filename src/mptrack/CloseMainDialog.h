// FLTK port of openmpt/mptrack/CloseMainDialog.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"
#include "InputHandler.h"
#include "ResizableDialog.h"

OPENMPT_NAMESPACE_BEGIN

class CModDoc;

class CloseMainDialog: public ResizableDialog
{
protected:
	ListBox m_List;
	Point m_minSize;
	BypassInputHandler m_bih;

	static mpt::ustring FormatTitle(const CModDoc &modDoc, bool fullPath);

public:
	CloseMainDialog();

protected:
	void DoDataExchange(DataExchange* pDX) override;
	bool OnInitDialog() override;
	void OnOK() override;

	void OnSaveAll();
	void OnSaveNone();
	void OnSwitchFullPaths();

	UI_DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
