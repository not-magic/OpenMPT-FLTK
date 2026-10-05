/*
 * ResizableDialog.h
 * -----------------
 * Purpose: A wrapper for resizable MFC dialogs that fixes various issues.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/ResizableDialog.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"
#include "DialogBase.h"

OPENMPT_NAMESPACE_BEGIN

class ResizableDialog : public DialogBase
{
public:
	struct DynamicItem
	{
		Wnd *window = nullptr;
		Point initialPoint;
		Size initialSize;
		// How much of the window growth is passed on to the control, in percent
		int moveX = 0, moveY = 0, sizeX = 0, sizeY = 0;
	};

	ResizableDialog() = default;
	explicit ResizableDialog(uint32 nIDTemplate, Wnd *pParentWnd = nullptr);

protected:
	bool OnInitDialog() override;
	void PostNcDestroy() override;
	void OnSize(uint32 type, int width, int height) override;
	virtual void ResizeDynamicLayout();

	void EnableAutoLayout(bool enable) { m_enableAutoLayout = enable; }

	UI_DECLARE_MESSAGE_MAP()

private:
	void LoadDynamicLayout();
	Rect AdjustItemRect(const DynamicItem &item, const Size windowSize) const;

	std::vector<DynamicItem> m_dynamicItems;
	Size m_minSize;
	Size m_originalClientSize;
	bool m_enableAutoLayout = true;
};

OPENMPT_NAMESPACE_END
