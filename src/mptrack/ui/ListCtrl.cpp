// MFC replacement on FLTK. List with columns, per-item data, check boxes and selection.

#include "stdafx.h"
#include "ListCtrl.h"
#include "Scaling.h"

#include <FL/Fl.H>
#include <FL/fl_draw.H>

#include <algorithm>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{


namespace
{

constexpr int kRowHeight = 17;
constexpr int kHeaderHeight = 22;
constexpr int kCheckBoxSize = 12;
constexpr uint32 kStyleSingleSelect = 0x04;

std::string ToUtf8(const mpt::ustring &text)
{
	return mpt::transcode<std::string>(mpt::common_encoding::utf8, text);
}

mpt::ustring FromUtf8(const std::string &text)
{
	return mpt::transcode<mpt::ustring>(mpt::common_encoding::utf8, text);
}

// Uses the current fl_font
std::string FitTextWithEllipsis(const std::string &text, int max_width)
{
	if(fl_width(text.c_str()) <= max_width)
		return text;
	const double ellipsis_width = fl_width("...");
	std::size_t length = text.size();
	while(length > 0)
	{
		--length;
		while(length > 0 && (static_cast<unsigned char>(text[length]) & 0xC0) == 0x80)
			--length;
		if(fl_width(text.c_str(), static_cast<int>(length)) + ellipsis_width <= max_width)
			break;
	}
	return text.substr(0, length) + "...";
}

}  // namespace


ListCtrl::ListCtrl(int x, int y, int width, int height, const char *label)
    : WndT<Fl_Table_Row>(x, y, width, height, label)
{
	rows(0);
	cols(0);
	col_header(1);
	col_header_height(kHeaderHeight);
	color(FL_BACKGROUND2_COLOR);
	col_resize(1);
	type(SELECT_MULTI);
	callback(TableCallback, this);
	when(FL_WHEN_RELEASE);
}


void ListCtrl::ConfigureFromTemplate(const DialogControl &control)
{
	m_isMultiSelect = (control.style & kStyleSingleSelect) == 0;
	type(m_isMultiSelect ? SELECT_MULTI : SELECT_SINGLE);
}


void ListCtrl::RebuildTable()
{
	rows(static_cast<int>(m_items.size()));
	row_height_all(kRowHeight);
	cols(static_cast<int>(m_columns.size()));
	for(int column = 0; column < static_cast<int>(m_columns.size()); ++column)
		col_width(column, m_columns[column].width);
	if(m_isRedrawing)
		redraw();
}


int ListCtrl::InsertColumn(int column, const mpt::ustring &title, uint32 format, int width)
{
	Column entry;
	entry.title = title;
	entry.format = format;
	entry.width = width;
	if(column < 0 || column >= static_cast<int>(m_columns.size()))
	{
		m_columns.push_back(std::move(entry));
		column = static_cast<int>(m_columns.size()) - 1;
	} else
	{
		m_columns.insert(m_columns.begin() + column, std::move(entry));
	}
	RebuildTable();
	return column;
}


void ListCtrl::SetColumnWidth(int column, int width)
{
	if(column < 0 || column >= static_cast<int>(m_columns.size()))
		return;
	if(width < 0)
	{
		// Automatic size: fit to the contents
		fl_font(labelfont(), 12);
		int fitted = static_cast<int>(fl_width(ToUtf8(m_columns[column].title).c_str())) + 16;
		for(const Item &item : m_items)
		{
			if(column < static_cast<int>(item.texts.size()))
				fitted = std::max(fitted, static_cast<int>(fl_width(item.texts[column].c_str())) + 12);
		}
		width = fitted;
	}
	m_columns[column].width = width;
	RebuildTable();
}


bool ListCtrl::GetItemRect(int index, Rect &rect, uint32) const
{
	if(index < 0 || index >= GetItemCount())
		return false;
	rect = Rect(0, kHeaderHeight + index * kRowHeight, w(), kHeaderHeight + (index + 1) * kRowHeight);
	return true;
}


void ListCtrl::GetViewRect(Rect &rect) const
{
	int totalWidth = 0;
	for(const Column &column : m_columns)
		totalWidth += column.width;
	rect = Rect(0, 0, totalWidth, kHeaderHeight + GetItemCount() * kRowHeight);
}


int ListCtrl::GetColumnWidth(int column) const
{
	if(column < 0 || column >= static_cast<int>(m_columns.size()))
		return 0;
	return m_columns[column].width;
}


void ListCtrl::DeleteAllColumns()
{
	m_columns.clear();
	for(Item &item : m_items)
		item.texts.clear();
	RebuildTable();
}


void ListCtrl::DeleteColumn(int column)
{
	if(column < 0 || column >= static_cast<int>(m_columns.size()))
		return;
	m_columns.erase(m_columns.begin() + column);
	for(Item &item : m_items)
	{
		if(column < static_cast<int>(item.texts.size()))
			item.texts.erase(item.texts.begin() + column);
	}
	RebuildTable();
}


int ListCtrl::InsertItem(int index, const mpt::ustring &text, int image)
{
	return InsertItem(index, text, image, 0);
}


int ListCtrl::InsertItem(int index, const mpt::ustring &text, int image, uintptr_t data)
{
	Item item;
	item.texts.push_back(ToUtf8(text));
	item.image = image;
	item.data = data;
	if(index < 0 || index >= static_cast<int>(m_items.size()))
	{
		m_items.push_back(std::move(item));
		index = static_cast<int>(m_items.size()) - 1;
	} else
	{
		m_items.insert(m_items.begin() + index, std::move(item));
	}
	RebuildTable();
	return index;
}


bool ListCtrl::DeleteItem(int index)
{
	if(index < 0 || index >= static_cast<int>(m_items.size()))
		return false;
	m_items.erase(m_items.begin() + index);
	if(m_selectionMark >= static_cast<int>(m_items.size()))
		m_selectionMark = -1;
	RebuildTable();
	return true;
}


bool ListCtrl::DeleteAllItems()
{
	m_items.clear();
	m_selectionMark = -1;
	RebuildTable();
	return true;
}


void ListCtrl::SetItemCount(int count)
{
	if(count > static_cast<int>(m_items.size()))
		m_items.resize(count);
	RebuildTable();
}


void ListCtrl::SetItemText(int index, int column, const mpt::ustring &text)
{
	if(index < 0 || index >= static_cast<int>(m_items.size()) || column < 0)
		return;
	Item &item = m_items[index];
	if(static_cast<int>(item.texts.size()) <= column)
		item.texts.resize(column + 1);
	item.texts[column] = ToUtf8(text);
	if(m_isRedrawing)
		redraw();
}


mpt::ustring ListCtrl::GetItemText(int index, int column) const
{
	if(index < 0 || index >= static_cast<int>(m_items.size()) || column < 0)
		return {};
	const Item &item = m_items[index];
	if(column >= static_cast<int>(item.texts.size()))
		return {};
	return FromUtf8(item.texts[column]);
}


void ListCtrl::SetItemData(int index, uintptr_t data)
{
	if(index >= 0 && index < static_cast<int>(m_items.size()))
		m_items[index].data = data;
}


uintptr_t ListCtrl::GetItemData(int index) const
{
	if(index >= 0 && index < static_cast<int>(m_items.size()))
		return m_items[index].data;
	return 0;
}


void ListCtrl::SetItemImage(int index, int image)
{
	if(index >= 0 && index < static_cast<int>(m_items.size()))
		m_items[index].image = image;
}


uint32 ListCtrl::GetItemState(int index, uint32 mask) const
{
	uint32 state = 0;
	if(index >= 0 && index < static_cast<int>(m_items.size()))
	{
		if(IsRowSelected(index))
			state |= ListItemSelected;
		if(index == m_selectionMark)
			state |= ListItemFocused;
	}
	return state & mask;
}


bool ListCtrl::SetItemState(int index, uint32 state, uint32 mask)
{
	if(index < 0 || index >= static_cast<int>(m_items.size()))
		return false;
	if(mask & ListItemSelected)
		select_row(index, (state & ListItemSelected) ? 1 : 0);
	if((mask & ListItemFocused) && (state & ListItemFocused))
		m_selectionMark = index;
	redraw();
	return true;
}


int ListCtrl::GetNextItem(int startAfter, uint32 flags) const
{
	for(int index = startAfter + 1; index < static_cast<int>(m_items.size()); ++index)
	{
		if(!(flags & ListNextSelected) || IsRowSelected(index))
			return index;
	}
	return -1;
}


void ListCtrl::SetSelectionMark(int index)
{
	m_selectionMark = index;
}


uint32 ListCtrl::GetSelectedCount() const
{
	uint32 count = 0;
	for(int index = 0; index < static_cast<int>(m_items.size()); ++index)
	{
		if(IsRowSelected(index))
			++count;
	}
	return count;
}


void ListCtrl::EnsureVisible(int index, bool)
{
	if(index < 0 || index >= static_cast<int>(m_items.size()))
		return;
	const int first = toprow;
	const int visibleRows = std::max((tih - kHeaderHeight) / kRowHeight, 1);
	if(index < first)
		row_position(index);
	else if(index >= first + visibleRows)
		row_position(index - visibleRows + 1);
}


int ListCtrl::HitTest(Point point, uint32 *flags) const
{
	if(flags)
		*flags = 0;
	const int row = FindRowAt(point.y);
	return row;
}


int ListCtrl::FindRowAt(int y) const
{
	const int yy = y - kHeaderHeight;
	if(yy < 0)
		return -1;
	const int row = toprow + yy / kRowHeight;
	return row < static_cast<int>(m_items.size()) ? row : -1;
}


void ListCtrl::SetCheck(int index, bool isChecked)
{
	if(index >= 0 && index < static_cast<int>(m_items.size()))
	{
		m_items[index].isChecked = isChecked;
		redraw();
	}
}


bool ListCtrl::GetCheck(int index) const
{
	if(index >= 0 && index < static_cast<int>(m_items.size()))
		return m_items[index].isChecked;
	return false;
}


uint32 ListCtrl::SetExtendedStyle(uint32 style)
{
	const uint32 previous = m_style;
	m_style = style;
	redraw();
	return previous;
}


void ListCtrl::SetHeaders(const mpt::span<const Header> &headers)
{
	for(int i = 0; i < static_cast<int>(headers.size()); i++)
	{
		const int width = headers[i].width;
		InsertColumn(i, headers[i].text ? mpt::ustring(headers[i].text) : mpt::ustring(), headers[i].mask, width >= 0 ? ScalePixels(width, this) : 16);
		if(width < 0)
			SetColumnWidth(i, width);
	}
}


void ListCtrl::SetColumnWidths(const mpt::span<const Header> &headers)
{
	for(int i = 0; i < static_cast<int>(headers.size()); i++)
	{
		if(const int width = headers[i].width; width > 0)
			SetColumnWidth(i, ScalePixels(width, this));
	}
}


ColorRef ListCtrl::OnGetCellBkColor(int, int)
{
	return m_backgroundColor;
}


ColorRef ListCtrl::OnGetCellTextColor(int, int)
{
	return m_textColor;
}


void ListCtrl::draw()
{
	WndT<Fl_Table_Row>::draw();
	if(col_header() && table_w < tiw)
	{
		fl_push_clip(wix, wiy, wiw, wih);
		draw_box(FL_UP_BOX, tix + table_w, wiy, tiw - table_w, col_header_height(), FL_BACKGROUND_COLOR);
		fl_pop_clip();
	}
}


void ListCtrl::draw_cell(TableContext context, int row, int column, int x, int y, int width, int height)
{
	switch(context)
	{
	case CONTEXT_STARTPAGE:
		fl_font(FL_HELVETICA, 12);
		return;
	case CONTEXT_COL_HEADER:
	{
		fl_push_clip(x, y, width, height);
		draw_box(FL_UP_BOX, x, y, width, height, FL_BACKGROUND_COLOR);
		if(column < static_cast<int>(m_columns.size()))
		{
			fl_color(FL_FOREGROUND_COLOR);
			fl_draw(ToUtf8(m_columns[column].title).c_str(), x + 4, y, width - 8, height, FL_ALIGN_LEFT | FL_ALIGN_CLIP);
		}
		fl_pop_clip();
		return;
	}
	case CONTEXT_CELL:
	{
		fl_push_clip(x, y, width, height);
		const bool isSelected = IsRowSelected(row) != 0;
		const ColorRef background = OnGetCellBkColor(row, column);
		const ColorRef text = OnGetCellTextColor(row, column);
		if(isSelected)
			fl_color(FL_SELECTION_COLOR);
		else
			fl_color(GetRValue(background), GetGValue(background), GetBValue(background));
		fl_rectf(x, y, width, height);
		int textX = x + 4;
		if(column == 0 && (m_style & ListStyleCheckBoxes))
		{
			fl_color(FL_BLACK);
			fl_rect(textX, y + (height - kCheckBoxSize) / 2, kCheckBoxSize, kCheckBoxSize);
			if(row < static_cast<int>(m_items.size()) && m_items[row].isChecked)
			{
				fl_line(textX + 2, y + height / 2, textX + 5, y + height / 2 + 3);
				fl_line(textX + 5, y + height / 2 + 3, textX + kCheckBoxSize - 2, y + height / 2 - 4);
			}
			textX += kCheckBoxSize + 4;
		}
		if(column == 0 && m_imageList && row < static_cast<int>(m_items.size()) && m_items[row].image >= 0)
		{
			Painter painter;
			m_imageList->Draw(painter, m_items[row].image, Point(textX, y + (height - m_imageList->GetImageHeight()) / 2));
			textX += m_imageList->GetImageWidth() + 4;
		}
		if(isSelected)
			fl_color(FL_WHITE);
		else
			fl_color(GetRValue(text), GetGValue(text), GetBValue(text));
		if(row < static_cast<int>(m_items.size()) && column < static_cast<int>(m_items[row].texts.size()))
		{
			Font cellFont(FL_HELVETICA, 12);
			const bool hasCellFont = onQueryCellFont && onQueryCellFont(row, column, cellFont);
			if(hasCellFont)
				fl_font(cellFont.face, cellFont.size);
			Fl_Align alignment = FL_ALIGN_LEFT;
			if(column < static_cast<int>(m_columns.size()))
			{
				if(m_columns[column].format == ListColumnRight)
					alignment = FL_ALIGN_RIGHT;
				else if(m_columns[column].format == ListColumnCenter)
					alignment = FL_ALIGN_CENTER;
			}
			const int text_width = x + width - textX - 4;
			const std::string fitted_text = FitTextWithEllipsis(m_items[row].texts[column], text_width);
			fl_draw(fitted_text.c_str(), textX, y, text_width, height, alignment | FL_ALIGN_CLIP);
			if(hasCellFont)
				fl_font(FL_HELVETICA, 12);
		}
		if(m_style & ListStyleGridLines)
		{
			fl_color(FL_LIGHT1);
			fl_rect(x, y, width, height);
		}
		fl_pop_clip();
		return;
	}
	default:
		return;
	}
}


namespace
{
class LabelEdit : public Edit
{
public:
	std::function<void()> onLostFocus;

protected:
	void OnKillFocus(Wnd *) override
	{
		if(onLostFocus)
			onLostFocus();
	}
};
}  // namespace


void ListCtrl::SendNotification(uint32 code, void *extra)
{
	if(GetParent())
	{
		NotifyHeader header;
		header.from = this;
		header.id = GetDlgCtrlID();
		header.code = code;
		header.extra = extra;
		RouteNotification(header);
	}
}


Edit *ListCtrl::EditLabel(int index)
{
	Rect rect;
	if(index < 0 || index >= GetItemCount() || !GetItemRect(index, rect))
		return nullptr;
	FinishEdit(false);
	m_editedItem = index;
	auto editor = std::make_unique<LabelEdit>();
	editor->resize(x() + rect.left, y() + rect.top, std::max(GetColumnWidth(0), 80), rect.Height());
	editor->onLostFocus = [this]() { Fl::add_timeout(0.0, [](void *data) { static_cast<ListCtrl *>(data)->FinishEdit(true); }, this); };
	m_editor = std::move(editor);
	add(m_editor.get());
	m_editor->SetWindowText(GetItemText(index, 0));
	m_editor->SetFocus();
	m_editor->SelectAll();
	m_editor->when(FL_WHEN_ENTER_KEY | FL_WHEN_NOT_CHANGED);
	m_editor->callback(
	    [](Fl_Widget *, void *data)
	    {
		    static_cast<ListCtrl *>(data)->FinishEdit(true);
	    },
	    this);
	ListLabelEdit info;
	info.item = index;
	SendNotification(ListBeginLabelEdit, &info);
	return m_editor.get();
}


void ListCtrl::FinishEdit(bool isAccepted)
{
	if(m_editedItem < 0 || m_editor == nullptr)
		return;
	const int index = m_editedItem;
	m_editedItem = -1;
	const mpt::ustring text = m_editor->GetWindowText();
	Fl::remove_timeout([](void *data) { static_cast<ListCtrl *>(data)->FinishEdit(true); }, this);
	remove(*m_editor);
	m_editor.reset();
	ListLabelEdit info;
	info.item = index;
	info.text = isAccepted ? &text : nullptr;
	SendNotification(ListEndLabelEdit, &info);
	redraw();
	Fl::focus(this);
}


bool ListCtrl::OnKeyDown(uint32 key, uint32 repeatCount, uint32 flags)
{
	if(m_editedItem >= 0 && key == Key_ESCAPE)
	{
		FinishEdit(false);
		return true;
	}
	return Wnd::OnKeyDown(key, repeatCount, flags);
}


void ListCtrl::TableCallback(Fl_Widget *widget, void *)
{
	ListCtrl *list = dynamic_cast<ListCtrl *>(widget);
	if(list == nullptr)
		return;
	const int row = list->callback_row();
	if(list->callback_context() == CONTEXT_CELL && row >= 0)
	{
		list->m_selectionMark = row;
		if((list->m_style & ListStyleCheckBoxes) && list->callback_col() == 0 && Fl::event_x() - list->x() < kCheckBoxSize + 8)
		{
			list->m_items[row].isChecked = !list->m_items[row].isChecked;
			list->redraw();
			ListCheckInfo info{row, list->m_items[row].isChecked};
			if(list->GetParent())
			{
				NotifyHeader header;
				header.from = list;
				header.id = list->GetDlgCtrlID();
				header.code = CheckListChange;
				header.extra = &info;
				list->RouteNotification(header);
			}
		}
		ListClickInfo clickInfo;
		clickInfo.item = row;
		clickInfo.column = list->callback_col();
		clickInfo.point = Point(Fl::event_x() - list->x(), Fl::event_y() - list->y());
		const auto notify = [&](uint32 code)
		{
			if(list->GetParent())
			{
				NotifyHeader header;
				header.from = list;
				header.id = list->GetDlgCtrlID();
				header.code = code;
				header.extra = &clickInfo;
				list->RouteNotification(header);
			}
		};
		if(Fl::event_button() == FL_RIGHT_MOUSE)
			notify(ListRClick);
		else if(Fl::event_clicks() > 0)
			notify(ListDblClick);
		else
			notify(ListClick);
		notify(ListItemChanged);
	}
}


}  // namespace ui


OPENMPT_NAMESPACE_END
