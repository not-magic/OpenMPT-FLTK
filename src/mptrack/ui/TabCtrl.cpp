/*
 * TabCtrl.cpp
 * -----------
 * Purpose: Row of tabs that selects one of several pages.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "TabCtrl.h"

#include <FL/Fl.H>
#include <FL/fl_draw.H>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{


namespace
{

// The selected tab is drawn larger and overlaps its neighbours, like Windows' tab control
constexpr int kTabStartX = 2;
constexpr int kSelectedTabGrowX = 2;
constexpr int kSelectedTabRaiseY = 2;
constexpr int kFrameWidth = 2;

void DrawTabShape(int left, int top, int right, int bottom)
{
	fl_color(FL_BACKGROUND_COLOR);
	fl_rectf(left, top, right - left + 1, bottom - top + 1);
	fl_color(FL_LIGHT3);
	fl_yxline(left, bottom, top + 2);
	fl_point(left + 1, top + 1);
	fl_xyline(left + 2, top, right - 2);
	fl_color(FL_DARK3);
	fl_point(right - 1, top + 1);
	fl_yxline(right, top + 2, bottom);
	fl_color(FL_DARK1);
	fl_yxline(right - 1, top + 2, bottom);
}

}  // namespace


TabCtrl::TabCtrl(int x, int y, int width, int height, const char *label)
    : WndT<Fl_Widget>(x, y, width, height, label)
{
	m_isFocusable = false;
}


int TabCtrl::InsertItem(int index, const mpt::ustring &text, LParam param, int image)
{
	Tab tab;
	tab.text = mpt::transcode<std::string>(mpt::common_encoding::utf8, text);
	tab.param = param;
	tab.image = image;
	if(index < 0 || index >= static_cast<int>(m_tabs.size()))
	{
		m_tabs.push_back(std::move(tab));
		index = static_cast<int>(m_tabs.size()) - 1;
	} else
	{
		m_tabs.insert(m_tabs.begin() + index, std::move(tab));
	}
	if(m_current < 0)
		m_current = 0;
	redraw();
	return index;
}


bool TabCtrl::DeleteItem(int index)
{
	if(index < 0 || index >= static_cast<int>(m_tabs.size()))
		return false;
	m_tabs.erase(m_tabs.begin() + index);
	m_current = std::min(m_current, static_cast<int>(m_tabs.size()) - 1);
	redraw();
	return true;
}


bool TabCtrl::DeleteAllItems()
{
	m_tabs.clear();
	m_current = -1;
	redraw();
	return true;
}


int TabCtrl::SetCurSel(int index)
{
	const int previous = m_current;
	if(index >= -1 && index < static_cast<int>(m_tabs.size()))
	{
		m_current = index;
		redraw();
	}
	return previous;
}


mpt::ustring TabCtrl::GetItemText(int index) const
{
	if(index < 0 || index >= static_cast<int>(m_tabs.size()))
		return {};
	return mpt::transcode<mpt::ustring>(mpt::common_encoding::utf8, m_tabs[index].text);
}


LParam TabCtrl::GetItemData(int index) const
{
	if(index < 0 || index >= static_cast<int>(m_tabs.size()))
		return 0;
	return m_tabs[index].param;
}


void TabCtrl::AdjustRect(bool isLarger, Rect *rect) const
{
	if(isLarger)
	{
		rect->left -= kFrameWidth;
		rect->top = std::max(rect->top - m_tabHeight - kFrameWidth, 0);
		rect->right += kFrameWidth;
		rect->bottom += kFrameWidth;
	} else
	{
		rect->left += kFrameWidth;
		rect->top += m_tabHeight + kFrameWidth;
		rect->right -= kFrameWidth;
		rect->bottom -= kFrameWidth;
	}
}


int TabCtrl::TabWidth(int index) const
{
	fl_font(FL_HELVETICA, 12);
	return static_cast<int>(fl_width(m_tabs[index].text.c_str())) + 20 + ((m_images && m_tabs[index].image >= 0) ? m_images->GetImageWidth() + 4 : 0);
}


int TabCtrl::FindTabAt(Point point) const
{
	int left = kTabStartX;
	for(int i = 0; i < static_cast<int>(m_tabs.size()); ++i)
	{
		const int width = TabWidth(i);
		if(point.x >= left && point.x < left + width && point.y >= 0 && point.y < m_tabHeight)
			return i;
		left += width;
	}
	return -1;
}


void TabCtrl::draw()
{
	fl_push_clip(x(), y(), w(), h());
	fl_color(FL_BACKGROUND_COLOR);
	fl_rectf(x(), y(), w(), h());
	fl_font(FL_HELVETICA, 12);

	// The page frame starts at the bottom edge of the tabs; the selected tab opens into it
	const int frameTop = y() + m_tabHeight - 1;
	const int right = x() + w() - 1;
	const int bottom = y() + h() - 1;
	fl_color(FL_LIGHT3);
	fl_xyline(x(), frameTop, right);
	if(bottom > frameTop + kFrameWidth)
	{
		fl_yxline(x(), frameTop, bottom);
		fl_color(FL_DARK3);
		fl_xyline(x(), bottom, right);
		fl_yxline(right, frameTop, bottom);
		fl_color(FL_DARK1);
		fl_xyline(x() + 1, bottom - 1, right - 1);
		fl_yxline(right - 1, frameTop + 1, bottom - 1);
	}

	int selectedLeft = 0;
	int left = x() + kTabStartX;
	for(int i = 0; i < static_cast<int>(m_tabs.size()); ++i)
	{
		const int width = TabWidth(i);
		if(i == m_current)
			selectedLeft = left;
		else
			DrawTab(i, left, width, false);
		left += width;
	}
	if(m_current >= 0 && m_current < static_cast<int>(m_tabs.size()))
		DrawTab(m_current, selectedLeft, TabWidth(m_current), true);
	fl_pop_clip();
}


void TabCtrl::DrawTab(int index, int left, int width, bool isSelected)
{
	const int growX = isSelected ? kSelectedTabGrowX : 0;
	const int top = y() + (isSelected ? 0 : kSelectedTabRaiseY);
	// The selected tab covers the frame line below it
	const int bottom = y() + m_tabHeight - (isSelected ? 0 : 2);
	DrawTabShape(left - growX, top, left + width - 1 + growX, bottom);

	const int textOffsetY = isSelected ? 0 : kSelectedTabRaiseY / 2;
	int textLeft = left + 10;
	if(m_images && m_tabs[index].image >= 0)
	{
		Painter painter(Point(x(), y()));
		m_images->Draw(painter, m_tabs[index].image, Point(textLeft - x(), (m_tabHeight - m_images->GetImageHeight()) / 2 + textOffsetY));
		textLeft += m_images->GetImageWidth() + 4;
	}
	fl_font(FL_HELVETICA, 12);
	fl_color(FL_FOREGROUND_COLOR);
	fl_draw(m_tabs[index].text.c_str(), textLeft, y() + m_tabHeight / 2 + fl_height() / 2 - fl_descent() + textOffsetY);
}


void TabCtrl::OnLButtonDown(uint32, Point point)
{
	const int index = FindTabAt(point);
	if(index >= 0 && index != m_current)
	{
		m_current = index;
		redraw();
		NotifyParent(*this, TabSelChange);
	}
}


}  // namespace ui


OPENMPT_NAMESPACE_END
