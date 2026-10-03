// MFC replacement on FLTK. List with columns, per-item data, check boxes and selection.

#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "Controls.h"
#include "ImageList.h"

#include <FL/Fl_Table_Row.H>

#include <functional>
#include <memory>
#include <string>
#include <vector>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{

enum ListStyle : uint32
{
	ListStyleGridLines = 0x01,
	ListStyleCheckBoxes = 0x04,
	ListStyleFullRowSelect = 0x20,
};

enum ListItemState : uint32
{
	ListItemFocused = 0x01,
	ListItemSelected = 0x02,
};

enum ListNextItem : uint32
{
	ListNextAll = 0x00,
	ListNextSelected = 0x02,
};

enum ListColumnFormat : uint32
{
	ListColumnLeft = 0,
	ListColumnRight = 1,
	ListColumnCenter = 2,
};


// Widths of columns that are fitted to their contents
constexpr int LVSCW_AUTOSIZE = -1;
constexpr int LVSCW_AUTOSIZE_USEHEADER = -2;
constexpr uint32 LVIR_BOUNDS = 0;


class ListCtrl : public WndT<Fl_Table_Row>
{
public:
	struct Header
	{
		const mpt::uchar *text = nullptr;
		int width = 0;
		uint32 mask = 0;
	};

	ListCtrl(int x = 0, int y = 0, int width = 0, int height = 0, const char *label = nullptr);

	void ConfigureFromTemplate(const DialogControl &control) override;

	int InsertColumn(int column, const mpt::ustring &title, uint32 format = ListColumnLeft, int width = 100);
	int GetColumnCount() const { return static_cast<int>(m_columns.size()); }
	void SetColumnWidth(int column, int width);
	int GetColumnWidth(int column) const;
	// The rectangle of an item in client coordinates
	bool GetItemRect(int index, Rect &rect, uint32 = 0) const;
	// Size of all items together
	void GetViewRect(Rect &rect) const;
	void DeleteColumn(int column);
	void DeleteAllColumns();

	int InsertItem(int index, const mpt::ustring &text, int image = -1);
	int InsertItem(int index, const mpt::ustring &text, int image, uintptr_t data);
	bool DeleteItem(int index);
	bool DeleteAllItems();
	int GetItemCount() const { return static_cast<int>(m_items.size()); }
	// Reserves space for a given number of items; missing items are created empty
	void SetItemCount(int count);
	void SetItemText(int index, int column, const mpt::ustring &text);
	mpt::ustring GetItemText(int index, int column) const;
	void SetItemData(int index, uintptr_t data);
	uintptr_t GetItemData(int index) const;
	void SetItemImage(int index, int image);
	void SetItemDataPtr(int index, void *value) { SetItemData(index, reinterpret_cast<uintptr_t>(value)); }
	void *GetItemDataPtr(int index) const { return reinterpret_cast<void *>(GetItemData(index)); }

	// Selection
	uint32 GetItemState(int index, uint32 mask) const;
	bool SetItemState(int index, uint32 state, uint32 mask);
	int GetNextItem(int startAfter, uint32 flags) const;
	int GetSelectionMark() const noexcept { return m_selectionMark; }
	void SetSelectionMark(int index);
	uint32 GetSelectedCount() const;
	int GetSelectedItem() const { return GetNextItem(-1, ListNextSelected); }
	void EnsureVisible(int index, bool = false);
	int HitTest(Point point, uint32 *flags = nullptr) const;

	// In-place editing of the text of the first column. ListBeginLabelEdit is sent when the editor
	// opens, ListEndLabelEdit when it closes (with text set unless cancelled).
	Edit *EditLabel(int index);
	Edit *GetEditControl() { return m_editor.get(); }
	void CancelLabelEdit() { FinishEdit(false); }

	// Check boxes (ListStyleCheckBoxes)
	void SetCheck(int index, bool isChecked = true);
	bool GetCheck(int index) const;

	uint32 GetExtendedStyle() const noexcept { return m_style; }
	uint32 SetExtendedStyle(uint32 style);
	bool IsMultiSelect() const noexcept { return m_isMultiSelect; }

	void SetImageList(ImageList *imageList) noexcept { m_imageList = imageList; }
	void SetRedraw(bool isRedrawing) noexcept { m_isRedrawing = isRedrawing; if(isRedrawing) redraw(); }
	ColorRef GetBkColor() const noexcept { return m_backgroundColor; }
	ColorRef GetTextColor() const noexcept { return m_textColor; }
	void SetBkColor(ColorRef color) noexcept { m_backgroundColor = color; }
	void SetTextColor(ColorRef color) noexcept { m_textColor = color; }

	void SetHeaders(const mpt::span<const Header> &headers);
	void SetColumnWidths(const mpt::span<const Header> &headers);
	// Switches to groups of items with a heading; returns false if not supported
	bool EnableGroupView() { return false; }

	// Chooses a different font for a cell; return true after changing font
	std::function<bool(int row, int column, Font &font)> onQueryCellFont;

	void draw() override;
	void draw_cell(TableContext context, int row, int column, int x, int y, int width, int height) override;

	// Per-cell colours; the base implementation returns the default colours
	virtual ColorRef OnGetCellBkColor(int row, int column);
	virtual ColorRef OnGetCellTextColor(int row, int column);

protected:
	struct Column
	{
		mpt::ustring title;
		int width = 100;
		uint32 format = ListColumnLeft;
	};

	struct Item
	{
		std::vector<std::string> texts;
		uintptr_t data = 0;
		int image = -1;
		bool isChecked = false;
	};

	bool OnKeyDown(uint32 key, uint32 repeatCount, uint32 flags) override;
	void FinishEdit(bool isAccepted);
	void SendNotification(uint32 code, void *extra);

	bool IsRowSelected(int row) const { return const_cast<ListCtrl *>(this)->row_selected(row) != 0; }
	void RebuildTable();
	int FindRowAt(int y) const;

	static void TableCallback(Fl_Widget *widget, void *data);

	std::unique_ptr<Edit> m_editor;
	int m_editedItem = -1;
	std::vector<Column> m_columns;
	std::vector<Item> m_items;
	ImageList *m_imageList = nullptr;
	uint32 m_style = 0;
	int m_selectionMark = -1;
	bool m_isMultiSelect = true;
	bool m_isRedrawing = true;
	ColorRef m_backgroundColor = RGB(255, 255, 255);
	ColorRef m_textColor = RGB(0, 0, 0);
};


// Header description for lists that use column definitions given as tables
using ListHeader = ListCtrl::Header;


}  // namespace ui


OPENMPT_NAMESPACE_END
