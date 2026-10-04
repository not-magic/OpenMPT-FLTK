// MFC replacement on FLTK. Bar at the bottom of the main window that shows several panes of text.

#include "stdafx.h"
#include "StatusBar.h"

#include <FL/Fl_Box.H>

#include <algorithm>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{


StatusBar::StatusBar(int x, int y, int width, int height)
    : WndT<Fl_Group>(x, y, width, height)
{
}


void StatusBar::SetPanes(const std::vector<std::pair<uint32, int>> &idsAndWidths)
{
	for(const Pane &pane : m_panes)
	{
		remove(pane.box);
		delete pane.box;
	}
	m_panes.clear();
	begin();
	for(const auto &[id, width] : idsAndWidths)
	{
		Fl_Box *const box = new Fl_Box(x(), y(), 0, h());
		box->box(FL_THIN_DOWN_BOX);
		box->labelsize(12);
		box->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
		m_panes.push_back(Pane{id, width, box});
	}
	end();
	Layout();
	redraw();
}


void StatusBar::SetPaneText(int index, const mpt::ustring &text)
{
	if(index < 0 || index >= static_cast<int>(m_panes.size()))
		return;
	const std::string utf8 = mpt::transcode<std::string>(mpt::common_encoding::utf8, text);
	Fl_Box *const box = m_panes[index].box;
	if(!box->label() || utf8 != box->label())
		box->copy_label(utf8.c_str());
}


mpt::ustring StatusBar::GetPaneText(int index) const
{
	if(index < 0 || index >= static_cast<int>(m_panes.size()) || !m_panes[index].box->label())
		return {};
	return mpt::transcode<mpt::ustring>(mpt::common_encoding::utf8, std::string(m_panes[index].box->label()));
}


void StatusBar::SetPaneWidth(int index, int width)
{
	if(index >= 0 && index < static_cast<int>(m_panes.size()))
	{
		m_panes[index].width = width;
		Layout();
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


void StatusBar::resize(int x, int y, int width, int height)
{
	WndT<Fl_Group>::resize(x, y, width, height);
	Layout();
}


void StatusBar::Layout()
{
	int fixed_width = 0;
	int flexible_total = 0;
	for(const Pane &pane : m_panes)
	{
		if(pane.width > 0)
			fixed_width += pane.width;
		else
			++flexible_total;
	}
	const int flexible_width = flexible_total ? std::max((w() - fixed_width) / flexible_total, 20) : 0;
	int left = x();
	for(const Pane &pane : m_panes)
	{
		const int width = pane.width > 0 ? pane.width : flexible_width;
		pane.box->resize(left, y(), width, h());
		left += width;
	}
	redraw();
}


}  // namespace ui


OPENMPT_NAMESPACE_END
