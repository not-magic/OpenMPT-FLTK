// MFC replacement on FLTK. A bar of icon buttons with check and drop-down buttons, separators, and room for
// embedded controls.

#include "stdafx.h"
#include "ToolBar.h"
#include "Controls.h"

#include <FL/Fl.H>
#include <FL/fl_draw.H>

#include <algorithm>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{


namespace
{

constexpr int kSeparatorWidth = 8;
constexpr int kDropDownWidth = 12;

}  // namespace


ToolBar::ToolBar(int x, int y, int width, int height, const char *label)
    : WndT<Fl_Group>(x, y, width, height, label)
{
	m_isFocusable = false;
	// The window text names the toolbar but is not displayed
	labeltype(FL_NO_LABEL);
}


void ToolBar::ConfigureFromTemplate(const DialogControl &)
{
}


bool ToolBar::AddButton(uint32 id, int image, uint32 style, uint32 state)
{
	Button button;
	button.id = id;
	button.image = image;
	button.style = style;
	button.state = state;
	button.width = (style & ToolStyleSeparator) ? kSeparatorWidth : 0;
	m_buttons.push_back(button);
	redraw();
	return true;
}


int ToolBar::CommandToIndex(uint32 id) const
{
	for(int i = 0; i < static_cast<int>(m_buttons.size()); ++i)
	{
		if(!(m_buttons[i].style & ToolStyleSeparator) && m_buttons[i].id == id)
			return i;
	}
	return -1;
}


void ToolBar::EnableButton(uint32 id, bool isEnabled)
{
	const int index = CommandToIndex(id);
	if(index < 0)
		return;
	if(isEnabled)
		m_buttons[index].state |= ToolStateEnabled;
	else
		m_buttons[index].state &= ~static_cast<uint32>(ToolStateEnabled);
	redraw();
}


void ToolBar::CheckButton(uint32 id, bool isChecked)
{
	const int index = CommandToIndex(id);
	if(index < 0)
		return;
	if(isChecked)
		m_buttons[index].state |= ToolStateChecked;
	else
		m_buttons[index].state &= ~static_cast<uint32>(ToolStateChecked);
	redraw();
}


bool ToolBar::IsButtonChecked(uint32 id) const
{
	const int index = CommandToIndex(id);
	return index >= 0 && (m_buttons[index].state & ToolStateChecked) != 0;
}


bool ToolBar::IsButtonEnabled(uint32 id) const
{
	const int index = CommandToIndex(id);
	return index >= 0 && (m_buttons[index].state & ToolStateEnabled) != 0;
}


void ToolBar::HideButton(uint32 id, bool isHidden)
{
	const int index = CommandToIndex(id);
	if(index < 0)
		return;
	if(isHidden)
		m_buttons[index].state |= ToolStateHidden;
	else
		m_buttons[index].state &= ~static_cast<uint32>(ToolStateHidden);
	redraw();
}


bool ToolBar::IsButtonHidden(uint32 id) const
{
	const int index = CommandToIndex(id);
	return index >= 0 && (m_buttons[index].state & ToolStateHidden) != 0;
}


void ToolBar::SetButtonImage(uint32 id, int image)
{
	const int index = CommandToIndex(id);
	if(index >= 0)
	{
		m_buttons[index].image = image;
		redraw();
	}
}


void ToolBar::SetButtonVisibility(int index, bool isVisible)
{
	if(index < 0 || index >= static_cast<int>(m_buttons.size()))
		return;
	if(isVisible)
		m_buttons[index].state &= ~static_cast<uint32>(ToolStateHidden);
	else
		m_buttons[index].state |= ToolStateHidden;
	redraw();
}


void ToolBar::SetButtonInfo(int index, uint32 id, uint32 style, int imageOrWidth)
{
	if(index < 0 || index >= static_cast<int>(m_buttons.size()))
		return;
	Button &button = m_buttons[index];
	button.id = id;
	button.style = style;
	if(style & ToolStyleSeparator)
		button.width = imageOrWidth;
	else
		button.image = imageOrWidth;
	redraw();
}


uint32 ToolBar::GetButtonStyle(int index) const
{
	if(index < 0 || index >= static_cast<int>(m_buttons.size()))
		return 0;
	return m_buttons[index].style;
}


void ToolBar::SetButtonStyle(int index, uint32 style)
{
	if(index >= 0 && index < static_cast<int>(m_buttons.size()))
	{
		m_buttons[index].style = style;
		redraw();
	}
}


uint32 ToolBar::GetButtonState(uint32 id) const
{
	const int index = CommandToIndex(id);
	return index >= 0 ? m_buttons[index].state : 0;
}


void ToolBar::SetButtonState(uint32 id, uint32 state)
{
	const int index = CommandToIndex(id);
	if(index >= 0)
	{
		m_buttons[index].state = state;
		redraw();
	}
}


void ToolBar::UpdateControl(bool isShown, Wnd &control, int index, int id, int height)
{
	if(isShown)
	{
		SetButtonInfo(index, static_cast<uint32>(id), ToolStyleSeparator, control.GetWindowRect().Width());
		Rect rect;
		if(GetItemRect(index, rect))
		{
			if(height)
			{
				const int centered = (rect.bottom + rect.top - height) / 2;
				if(centered > rect.top)
					rect.top = centered;
			}
			control.GetWidget()->position(x() + rect.left, y() + rect.top);
		}
	}
	control.ShowWindow(isShown);
	SetButtonVisibility(index, isShown);
}


void ToolBar::SetSeparatorWidth(int index, int width)
{
	if(index >= 0 && index < static_cast<int>(m_buttons.size()) && (m_buttons[index].style & ToolStyleSeparator))
	{
		m_buttons[index].width = width;
		redraw();
	}
}


int ToolBar::ButtonWidth(const Button &button) const
{
	if(button.style & ToolStyleSeparator)
		return button.width;
	int width = m_buttonSize.cx;
	if((button.style & ToolStyleDropDown) && (m_extendedStyle & ToolExtendedDrawDropDownArrows))
		width += kDropDownWidth;
	return width;
}


bool ToolBar::GetItemRect(int index, Rect &rect) const
{
	if(index < 0 || index >= static_cast<int>(m_buttons.size()))
		return false;
	int left = m_indent;
	for(int i = 0; i < index; ++i)
	{
		if(!(m_buttons[i].state & ToolStateHidden))
			left += ButtonWidth(m_buttons[i]);
	}
	rect = Rect(left, 0, left + ButtonWidth(m_buttons[index]), std::min(h(), m_buttonSize.cy));
	return true;
}


Size ToolBar::CalcFixedSize() const
{
	int width = 0;
	for(const Button &button : m_buttons)
	{
		if(!(button.state & ToolStateHidden))
			width += ButtonWidth(button);
	}
	return Size(width, m_buttonSize.cy);
}


int ToolBar::FindButtonAt(Point point) const
{
	for(int i = 0; i < static_cast<int>(m_buttons.size()); ++i)
	{
		if(m_buttons[i].state & ToolStateHidden)
			continue;
		Rect rect;
		if(GetItemRect(i, rect) && rect.PtInRect(point) && !(m_buttons[i].style & ToolStyleSeparator))
			return i;
	}
	return -1;
}


void ToolBar::draw()
{
	fl_push_clip(x(), y(), w(), h());
	fl_color(FL_BACKGROUND_COLOR);
	fl_rectf(x(), y(), w(), h());
	Painter painter(Point(x(), y()));
	for(int i = 0; i < static_cast<int>(m_buttons.size()); ++i)
	{
		const Button &button = m_buttons[i];
		if(button.state & ToolStateHidden)
			continue;
		Rect rect;
		GetItemRect(i, rect);
		if(button.style & ToolStyleSeparator)
		{
			if(button.width <= kSeparatorWidth)
			{
				painter.FillSolidRect(Rect(rect.left + rect.Width() / 2, rect.top + 4, rect.left + rect.Width() / 2 + 1, rect.bottom - 4), GetSystemColor(SysColor::ButtonShadow));
			}
			continue;
		}
		const bool isEnabled = (button.state & ToolStateEnabled) != 0;
		const bool isChecked = (button.state & ToolStateChecked) != 0;
		const bool isPressed = (i == m_pressedIndex);
		const bool isHot = (i == m_hotIndex) && isEnabled;
		if(isChecked || isPressed)
			painter.Draw3dRect(rect, GetSystemColor(SysColor::ButtonShadow), GetSystemColor(SysColor::ButtonHighlight));
		else if(isHot || !m_isFlat)
			painter.Draw3dRect(rect, GetSystemColor(SysColor::ButtonHighlight), GetSystemColor(SysColor::ButtonShadow));
		const ImageList *images = isEnabled ? m_images : (m_disabledImages ? m_disabledImages : m_images);
		const int iconAreaWidth = rect.Width() - (((button.style & ToolStyleDropDown) && (m_extendedStyle & ToolExtendedDrawDropDownArrows)) ? kDropDownWidth : 0);
		if(images)
		{
			const int offset = (isChecked || isPressed) ? 1 : 0;
			images->Draw(painter, button.image, Point(rect.left + (iconAreaWidth - images->GetImageWidth()) / 2 + offset, rect.top + (rect.Height() - images->GetImageHeight()) / 2 + offset));
		}
		if((button.style & ToolStyleDropDown) && (m_extendedStyle & ToolExtendedDrawDropDownArrows))
		{
			const int cx = rect.right - kDropDownWidth / 2 - 1;
			const int cy = rect.top + rect.Height() / 2;
			painter.SetBrushColor(GetSystemColor(isEnabled ? SysColor::ButtonText : SysColor::GrayText));
			painter.SetPenColor(GetSystemColor(isEnabled ? SysColor::ButtonText : SysColor::GrayText));
			const Point arrow[3] = {{cx - 3, cy - 1}, {cx + 3, cy - 1}, {cx, cy + 2}};
			painter.Polygon(arrow, 3);
		}
	}
	fl_pop_clip();
	draw_children();
}


void ToolBar::OnLButtonDown(uint32, Point point)
{
	const int index = FindButtonAt(point);
	if(index < 0 || !(m_buttons[index].state & ToolStateEnabled))
		return;
	const Button &button = m_buttons[index];
	Rect rect;
	GetItemRect(index, rect);
	if((button.style & ToolStyleDropDown) && (m_extendedStyle & ToolExtendedDrawDropDownArrows) && point.x >= rect.right - kDropDownWidth)
	{
		ToolbarDropDownInfo info;
		info.id = button.id;
		info.rect = rect;
		NotifyHeader header;
		header.from = this;
		header.id = GetDlgCtrlID();
		header.code = ToolbarDropDown;
		header.extra = &info;
		if(Wnd *parent = GetParent())
			parent->RouteCommand(header.id, ToolbarDropDown, &header);
		return;
	}
	m_pressedIndex = index;
	redraw();
}


void ToolBar::OnLButtonUp(uint32, Point point)
{
	const int pressed = m_pressedIndex;
	m_pressedIndex = -1;
	redraw();
	if(pressed < 0 || FindButtonAt(point) != pressed)
		return;
	Button &button = m_buttons[pressed];
	if(button.style & ToolStyleCheck)
		button.state ^= ToolStateChecked;
	if(Wnd *parent = GetParent())
		parent->RouteCommand(button.id, 0, nullptr);
}


mpt::ustring ToolBar::GetButtonToolTip(uint32 id) const
{
	return onToolTip ? onToolTip(id) : mpt::ustring{};
}


void ToolBar::OnMouseMove(uint32, Point point)
{
	const int index = FindButtonAt(point);
	if(index != m_hotIndex)
	{
		m_hotIndex = index;
		redraw();
	}
	if(index >= 0)
	{
		const mpt::ustring text = GetButtonToolTip(m_buttons[index].id);
		Rect rect;
		if(!text.empty() && GetItemRect(index, rect))
			ShowToolTip(rect, text);
	}
}


void ToolBar::OnMouseLeave()
{
	if(m_hotIndex != -1)
	{
		m_hotIndex = -1;
		redraw();
	}
}


}  // namespace ui


OPENMPT_NAMESPACE_END
