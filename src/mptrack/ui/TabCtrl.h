// MFC replacement on FLTK. Row of tabs that selects one of several pages.

#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "Controls.h"
#include "ImageList.h"

#include <vector>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{

class TabCtrl : public WndT<Fl_Widget>
{
public:
	TabCtrl(int x = 0, int y = 0, int width = 0, int height = 0, const char *label = nullptr);

	int InsertItem(int index, const mpt::ustring &text, LParam param = 0, int image = -1);
	LParam GetItemData(int index) const;
	void SetImageList(const ImageList *images) noexcept { m_images = images; redraw(); }
	// Computes the area for the page below the tabs (isLarger false) or the area that includes the tabs (isLarger true)
	void AdjustRect(bool isLarger, Rect *rect) const;
	bool DeleteItem(int index);
	bool DeleteAllItems();
	int GetItemCount() const { return static_cast<int>(m_tabs.size()); }
	int GetCurSel() const noexcept { return m_current; }
	int SetCurSel(int index);
	mpt::ustring GetItemText(int index) const;
	// Height of the row of tabs
	int GetTabHeight() const noexcept { return m_tabHeight; }

	void draw() override;

protected:
	void OnLButtonDown(uint32 flags, Point point) override;

	int FindTabAt(Point point) const;
	int TabWidth(int index) const;
	void DrawTab(int index, int left, int width, bool isSelected);

	struct Tab
	{
		std::string text;
		LParam param = 0;
		int image = -1;
	};

	std::vector<Tab> m_tabs;
	const ImageList *m_images = nullptr;
	int m_current = -1;
	int m_tabHeight = 24;
};

}  // namespace ui


OPENMPT_NAMESPACE_END
