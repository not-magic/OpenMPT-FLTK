/*
 * StatusBar.h
 * -----------
 * Purpose: Bar at the bottom of the main window that shows several panes of text.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "Wnd.h"

#include <vector>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{

class StatusBar : public WndT<Fl_Widget>
{
public:
	StatusBar(int x = 0, int y = 0, int width = 0, int height = 0);

	// Creates the panes. A width of 0 makes the pane use the space that the other panes leave.
	void SetPanes(const std::vector<std::pair<uint32, int>> &idsAndWidths);
	void SetPaneText(int index, const mpt::ustring &text);
	mpt::ustring GetPaneText(int index) const;
	void SetPaneWidth(int index, int width);
	int CommandToIndex(uint32 id) const;
	int GetPaneCount() const { return static_cast<int>(m_panes.size()); }

	void draw() override;

private:
	struct Pane
	{
		uint32 id = 0;
		int width = 0;
		std::string text;
	};

	std::vector<Pane> m_panes;
};

}  // namespace ui


OPENMPT_NAMESPACE_END
