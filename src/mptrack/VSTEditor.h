/*
 * VSTEditor.h
 * -----------
 * Purpose: Implementation of the custom plugin editor window that is used if a plugin provides an own editor GUI.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "AbstractVstEditor.h"

OPENMPT_NAMESPACE_BEGIN

#ifdef MPT_WITH_VST

class CVstPlugin;

class COwnerVstEditor : public CAbstractVstEditor
{
protected:
	Static m_plugWindow;
	int m_width = 0, m_height = 0;

public:
	COwnerVstEditor(CVstPlugin &plugin);
	~COwnerVstEditor() override = default;

	// Plugins may request to change the GUI size.
	bool IsResizable() const override { return true; }
	bool SetSize(int contentWidth, int contentHeight) override;

	void UpdateParamDisplays() override;

	bool OpenEditor(Wnd *parent) override;
	void DoClose() override;

protected:
	bool OnEraseBkgnd(ui::Painter *) { return true; }
	void OnPaint();

	LResult OnPreTranslateKeyDown(WParam wParam, LParam lParam) { return HandlePreTranslateMessage(WM_KEYDOWN, wParam, lParam); }
	LResult OnPreTranslateKeyUp(WParam wParam, LParam lParam) { return HandlePreTranslateMessage(WM_KEYUP, wParam, lParam); }
	LResult OnPreTranslateSysKeyDown(WParam wParam, LParam lParam) { return HandlePreTranslateMessage(WM_SYSKEYDOWN, wParam, lParam); }
	LResult OnPreTranslateSysKeyUp(WParam wParam, LParam lParam) { return HandlePreTranslateMessage(WM_SYSKEYUP, wParam, lParam); }
	LResult HandlePreTranslateMessage(uint32 message, WParam wParam, LParam lParam)
	{
		MSG msg = {m_plugWindow, message, wParam, lParam, 0, {}};
		return HandleKeyMessage(msg, true);
	}

	UI_DECLARE_MESSAGE_MAP()
};

#endif // MPT_WITH_VST

OPENMPT_NAMESPACE_END
