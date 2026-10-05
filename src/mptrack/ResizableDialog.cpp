/*
 * ResizableDialog.cpp
 * -------------------
 * Purpose: A wrapper for resizable MFC dialogs that fixes the dialog's minimum size,
 *          as MFC does not scale controls properly if the user makes the dialog smaller than it originally was.
 *          It also does not handle DPI changes properly. Because CMFCDynamicLayout is not inheritable in any practical way,
 *          we do a partial re-implementation.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/ResizableDialog.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "ResizableDialog.h"
#include "MPTrackUtil.h"
#include "resource.h"
#include "ui/ResourceTables.h"

OPENMPT_NAMESPACE_BEGIN


UI_MESSAGE_MAP_BEGIN(ResizableDialog, DialogBase)
UI_MESSAGE_MAP_END()


ResizableDialog::ResizableDialog(uint32 nIDTemplate, Wnd *pParentWnd)
	: DialogBase{nIDTemplate, pParentWnd}
{ }


bool ResizableDialog::OnInitDialog()
{
	// Take our original measurements before the layout is loaded
	m_originalClientSize = GetClientRect().Dimensions();
	m_minSize = m_originalClientSize;

	DialogBase::OnInitDialog();
	LoadDynamicLayout();
	SetResizable(true);
	if(Fl_Window *frame = GetFrameWindow())
		frame->size_range(m_minSize.cx, m_minSize.cy);
	return true;
}


void ResizableDialog::PostNcDestroy()
{
	// In case the dialog object gets reused (e.g. pattern clipboard dialog)
	m_dynamicItems.clear();
	m_minSize = {};
	DialogBase::PostNcDestroy();
}


void ResizableDialog::LoadDynamicLayout()
{
	m_dynamicItems.clear();
	const ui::DialogTemplate *dialogTemplate = ui::FindDialogTemplate(m_templateId);
	if(dialogTemplate == nullptr || dialogTemplate->layout == nullptr)
		return;
	Fl_Group *group = GetWidget()->as_group();
	if(group == nullptr)
		return;
	const int childCount = group->children();
	const std::size_t itemCount = dialogTemplate->layoutCount / 4;
	for(int i = 0; i < childCount && static_cast<std::size_t>(i) < itemCount; ++i)
	{
		const int16 *values = dialogTemplate->layout + static_cast<std::size_t>(i) * 4;
		if(values[0] == 0 && values[1] == 0 && values[2] == 0 && values[3] == 0)
			continue;
		Wnd *child = dynamic_cast<Wnd *>(group->child(i));
		if(child == nullptr)
			continue;
		DynamicItem item;
		item.window = child;
		const Rect rect = child->GetRectInParent();
		item.initialPoint = rect.TopLeft();
		item.initialSize = rect.Dimensions();
		item.moveX = values[0];
		item.moveY = values[1];
		item.sizeX = values[2];
		item.sizeY = values[3];
		m_dynamicItems.push_back(item);
	}
}


Rect ResizableDialog::AdjustItemRect(const DynamicItem &item, const Size windowSize) const
{
	const double ratioX = 0.01 * (windowSize.cx - m_originalClientSize.cx);
	const double ratioY = 0.01 * (windowSize.cy - m_originalClientSize.cy);

	Point move;
	move.x = mpt::saturate_trunc<int>(ratioX * item.moveX);
	move.y = mpt::saturate_trunc<int>(ratioY * item.moveY);

	Size size;
	size.cx = mpt::saturate_trunc<int>(ratioX * item.sizeX);
	size.cy = mpt::saturate_trunc<int>(ratioY * item.sizeY);

	const Point itemPoint = item.initialPoint + Size(move.x, move.y);
	const Size itemSize(item.initialSize.cx + size.cx, item.initialSize.cy + size.cy);
	return Rect{itemPoint, itemSize};
}


void ResizableDialog::OnSize(uint32 type, int width, int height)
{
	DialogBase::OnSize(type, width, height);
	ResizeDynamicLayout();
}


void ResizableDialog::ResizeDynamicLayout()
{
	if(m_dynamicItems.empty() || !m_enableAutoLayout)
		return;

	const Size windowSize = GetClientRect().Dimensions();
	for(const auto &item : m_dynamicItems)
	{
		if(item.window != nullptr)
		{
			const Rect rect = AdjustItemRect(item, windowSize);
			item.window->MoveWindow(GetWidget()->x() + rect.left, GetWidget()->y() + rect.top, rect.Width(), rect.Height());
		}
	}
	Invalidate();
}

OPENMPT_NAMESPACE_END
