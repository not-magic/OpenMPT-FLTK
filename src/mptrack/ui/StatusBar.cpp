// MFC replacement on FLTK. Bar at the bottom of the main window that shows several panes of text.

#include "stdafx.h"
#include "StatusBar.h"

#include <FL/fl_draw.H>

#include <algorithm>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{


StatusBar::StatusBar(int x, int y, int width, int height)
    : WndT<Fl_Widget>(x, y, width, height)
{
}


void StatusBar::SetPanes(const std::vector<std::pair<uint32, int>> &idsAndWidths)
{
	m_panes.clear();
	for(const auto &[id, width] : idsAndWidths)
		m_panes.push_back(Pane{id, width, {}});
	redraw();
}


void StatusBar::SetPaneText(int index, const mpt::ustring &text)
{
	if(index < 0 || index >= static_cast<int>(m_panes.size()))
		return;
	const std::string utf8 = mpt::transcode<std::string>(mpt::common_encoding::utf8, text);
	if(m_panes[index].text != utf8)
	{
		m_panes[index].text = utf8;
		redraw();
	}
}


mpt::ustring StatusBar::GetPaneText(int index) const
{
	if(index < 0 || index >= static_cast<int>(m_panes.size()))
		return {};
	return mpt::transcode<mpt::ustring>(mpt::common_encoding::utf8, m_panes[index].text);
}


void StatusBar::SetPaneWidth(int index, int width)
{
	if(index >= 0 && index < static_cast<int>(m_panes.size()))
	{
		m_panes[index].width = width;
		redraw();
	}
}


int StatusBar::CommandToIndex(uint32 id) const
{
	for(int i = 0; i < static_cast<int>(m_panes.size()); ++i)
	{
		if(m_panes[i].id == id)
			return i;
	}
	return -1;
}


void StatusBar::draw()
{
	fl_push_clip(x(), y(), w(), h());
	fl_color(FL_BACKGROUND_COLOR);
	fl_rectf(x(), y(), w(), h());
	int fixedWidth = 0;
	int flexibleCount = 0;
	for(const Pane &pane : m_panes)
	{
		if(pane.width > 0)
			fixedWidth += pane.width;
		else
			++flexibleCount;
	}
	const int flexibleWidth = flexibleCount ? std::max((w() - fixedWidth) / flexibleCount, 20) : 0;
	fl_font(FL_HELVETICA, 12);
	int left = x();
	for(const Pane &pane : m_panes)
	{
		const int width = pane.width > 0 ? pane.width : flexibleWidth;
		draw_box(FL_THIN_DOWN_BOX, left, y() + 1, width - 1, h() - 2, FL_BACKGROUND_COLOR);
		fl_color(FL_FOREGROUND_COLOR);
		fl_draw(pane.text.c_str(), left + 4, y() + 1, width - 8, h() - 2, FL_ALIGN_LEFT | FL_ALIGN_CLIP);
		left += width;
	}
	fl_pop_clip();
}


}  // namespace ui


OPENMPT_NAMESPACE_END
