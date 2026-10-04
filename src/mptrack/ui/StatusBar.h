// MFC replacement on FLTK. Bar at the bottom of the main window that shows several panes of text.

#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "Wnd.h"

#include <vector>

class Fl_Box;


OPENMPT_NAMESPACE_BEGIN


namespace ui
{

class StatusBar : public WndT<Fl_Group>
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

	void resize(int x, int y, int width, int height) override;

private:
	void Layout();

	struct Pane
	{
		uint32 id = 0;
		int width = 0;
		Fl_Box *box = nullptr;
	};

	std::vector<Pane> m_panes;
};

}  // namespace ui


OPENMPT_NAMESPACE_END
