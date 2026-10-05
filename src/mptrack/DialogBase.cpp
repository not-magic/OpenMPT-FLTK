/*
 * DialogBase.cpp
 * --------------
 * Purpose: Base class for dialogs that adds functionality on top of CDialog.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/DialogBase.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "DialogBase.h"
#include "InputHandler.h"
#include "Mainfrm.h"

#include <FL/Fl.H>

OPENMPT_NAMESPACE_BEGIN

UI_MESSAGE_MAP_BEGIN(DialogBase, Dialog)
UI_MESSAGE_MAP_END()


bool DialogBase::OnInitDialog()
{
	UpdateToolTips();
	return Dialog::OnInitDialog();
}


bool DialogBase::PreTranslateMessage(int event)
{
	if(event == FL_PUSH || event == FL_MOVE || event == FL_DRAG)
	{
		m_lastInputDevice = InputDevice::Mouse;
	} else if(event == FL_KEYBOARD || event == FL_KEYUP)
	{
		m_lastInputDevice = InputDevice::Keyboard;
		const uint32 key = ui::KeyFromEvent();
		const uint32 flags = (event == FL_KEYUP) ? ui::KeyFlagRelease : 0;
		if(HandleGlobalKeyMessage(key, flags))
			return true;
	}
	return Dialog::PreTranslateMessage(event);
}


bool DialogBase::HandleGlobalKeyMessage(uint32 key, uint32 flags)
{
	// We handle keypresses before the toolkit has a chance to handle them (for alt etc..)
	if(CInputHandler *ih = CMainFrame::GetInputHandler())
	{
		const auto event = ih->Translate(key, 1, flags);
		if(ih->KeyEvent(kCtxAllContexts, event) != kcNull)
		{
			// Special case: ESC is typically bound to stopping playback, but we also want to allow ESC to close dialogs
			if(!(key == ui::Key_ESCAPE && !(flags & ui::KeyFlagRelease)))
				return true;  // Mapped to a command, no need to pass the key on.
		}
	}

	return false;
}


void DialogBase::SetFocusToFirstControl()
{
	Fl_Group *group = GetWidget()->as_group();
	if(group == nullptr)
		return;
	for(int i = 0; i < group->children(); ++i)
	{
		Fl_Widget *child = group->child(i);
		if(child->visible() && child->active() && child->visible_focus())
		{
			child->take_focus();
			return;
		}
	}
}


void DialogBase::UpdateToolTips()
{
	Fl_Group *group = GetWidget()->as_group();
	if(group == nullptr)
		return;
	for(int i = 0; i < group->children(); ++i)
	{
		Fl_Widget *child = group->child(i);
		Wnd *control = dynamic_cast<Wnd *>(child);
		if(control == nullptr)
			continue;
		if(ui::ToolBar *toolBar = dynamic_cast<ui::ToolBar *>(control))
		{
			toolBar->onToolTip = [this](uint32 buttonId) { return GetToolTipText(buttonId, nullptr); };
			continue;
		}
		const mpt::ustring text = GetToolTipText(control->GetDlgCtrlID(), control);
		if(text.empty())
			continue;
		m_tooltipStorage.push_back(std::make_unique<std::string>(mpt::transcode<std::string>(mpt::common_encoding::utf8, text)));
		child->tooltip(m_tooltipStorage.back()->c_str());
	}
}


OPENMPT_NAMESPACE_END
