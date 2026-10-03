// MFC replacement on FLTK. Tree of items with text, icons and expandable branches.

#include "stdafx.h"
#include "TreeCtrl.h"

#include <FL/Fl.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <cstdlib>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{


namespace
{

TreeItem sentinelFirst;
TreeItem sentinelLast;
TreeItem sentinelSort;

constexpr int kIndent = 16;
constexpr int kScrollBarWidth = 14;
constexpr int kTextPadding = 4;
constexpr int kDragThreshold = 4;

std::string ToUtf8(const mpt::ustring &text)
{
	return mpt::transcode<std::string>(mpt::common_encoding::utf8, text);
}

}  // namespace


TreeItem *const TreeFirst = &sentinelFirst;
TreeItem *const TreeLast = &sentinelLast;
TreeItem *const TreeSort = &sentinelSort;


TreeCtrl::TreeCtrl(int x, int y, int width, int height, const char *label)
    : WndT<Fl_Group>(x, y, width, height, label)
    , m_root(std::make_unique<TreeItem>())
{
	m_isFocusable = true;
	m_isCustomPaint = false;
	m_root->isExpanded = true;
	begin();
	m_scrollBar = new Fl_Scrollbar(x + width - kScrollBarWidth, y, kScrollBarWidth, height);
	m_scrollBar->callback(ScrollCallback, this);
	m_scrollBar->hide();
	end();
}


void TreeCtrl::ConfigureFromTemplate(const DialogControl &control)
{
	m_style = 0;
	// Style bits of the dialog templates are the same as the style flags
	m_style = control.style & (TreeStyleHasButtons | TreeStyleHasLines | TreeStyleLinesAtRoot | TreeStyleEditLabels | TreeStyleShowSelectionAlways | TreeStyleSingleExpand);
}


TreeItemHandle TreeCtrl::InsertItem(const mpt::ustring &text, int image, int selectedImage, TreeItemHandle parent, TreeItemHandle after)
{
	return InsertItem(TreeItemText | TreeItemImage | TreeItemSelectedImage, text, image, selectedImage, 0, 0, 0, parent, after);
}


TreeItemHandle TreeCtrl::InsertItem(uint32 mask, const mpt::ustring &text, int image, int selectedImage, uint32 state, uint32 stateMask, LParam param, TreeItemHandle parent, TreeItemHandle after)
{
	TreeItem *parentItem = parent ? parent : m_root.get();
	auto item = std::make_unique<TreeItem>();
	item->parent = parentItem;
	if(mask & TreeItemText)
		item->text = text;
	if(mask & TreeItemImage)
		item->image = image;
	if(mask & TreeItemSelectedImage)
		item->selectedImage = selectedImage;
	if(mask & TreeItemParam)
		item->param = param;
	if(mask & TreeItemState)
		item->state = (state & stateMask);
	TreeItem *result = item.get();
	auto &siblings = parentItem->children;
	if(after == TreeFirst)
	{
		siblings.insert(siblings.begin(), std::move(item));
	} else if(after == TreeSort)
	{
		const std::string key = ToUtf8(result->text);
		auto it = siblings.begin();
		while(it != siblings.end() && ToUtf8((*it)->text) <= key)
			++it;
		siblings.insert(it, std::move(item));
	} else if(after == TreeLast || after == nullptr)
	{
		siblings.push_back(std::move(item));
	} else
	{
		auto it = std::find_if(siblings.begin(), siblings.end(), [after](const auto &entry) { return entry.get() == after; });
		if(it == siblings.end())
			siblings.push_back(std::move(item));
		else
			siblings.insert(it + 1, std::move(item));
	}
	m_isRowsDirty = true;
	UpdateScrollBar();
	return result;
}


bool TreeCtrl::SetItem(TreeItemHandle item, uint32 mask, const mpt::ustring &text, int image, int selectedImage, uint32 state, uint32 stateMask, LParam param)
{
	if(item == nullptr)
		return false;
	if(mask & TreeItemText)
		item->text = text;
	if(mask & TreeItemImage)
		item->image = image;
	if(mask & TreeItemSelectedImage)
		item->selectedImage = selectedImage;
	if(mask & TreeItemParam)
		item->param = param;
	if(mask & TreeItemState)
		item->state = (item->state & ~stateMask) | (state & stateMask);
	if(m_isRedrawing)
		redraw();
	return true;
}


bool TreeCtrl::GetItem(TreeItemInfo *info) const
{
	if(info == nullptr || info->item == nullptr)
		return false;
	const TreeItem *item = info->item;
	if(info->mask & TreeItemText)
		info->text = item->text;
	if(info->mask & TreeItemImage)
		info->image = item->image;
	if(info->mask & TreeItemSelectedImage)
		info->selectedImage = item->selectedImage;
	if(info->mask & TreeItemParam)
		info->param = item->param;
	if(info->mask & TreeItemState)
		info->state = item->state & info->stateMask;
	return true;
}


bool TreeCtrl::IsAncestor(const TreeItem *ancestor, const TreeItem *item) const
{
	for(const TreeItem *walker = item; walker != nullptr; walker = walker->parent)
	{
		if(walker == ancestor)
			return true;
	}
	return false;
}


bool TreeCtrl::DeleteItem(TreeItemHandle item)
{
	if(item == nullptr || item->parent == nullptr)
		return false;
	if(m_selected && IsAncestor(item, m_selected))
		m_selected = item->parent == m_root.get() ? nullptr : item->parent;
	if(m_dropTarget && IsAncestor(item, m_dropTarget))
		m_dropTarget = nullptr;
	if(m_dragItem && IsAncestor(item, m_dragItem))
		m_dragItem = nullptr;
	if(m_editedItem && IsAncestor(item, m_editedItem))
		FinishEdit(false);
	SendNotification(TreeDeleteItem, item);
	auto &siblings = item->parent->children;
	siblings.erase(std::find_if(siblings.begin(), siblings.end(), [item](const auto &entry) { return entry.get() == item; }));
	m_isRowsDirty = true;
	UpdateScrollBar();
	return true;
}


bool TreeCtrl::DeleteAllItems()
{
	m_selected = nullptr;
	m_dropTarget = nullptr;
	m_dragItem = nullptr;
	if(m_editedItem)
		FinishEdit(false);
	m_root->children.clear();
	m_isRowsDirty = true;
	m_firstRow = 0;
	UpdateScrollBar();
	return true;
}


mpt::ustring TreeCtrl::GetItemText(TreeItemHandle item) const
{
	return item ? item->text : mpt::ustring();
}


bool TreeCtrl::SetItemText(TreeItemHandle item, const mpt::ustring &text)
{
	if(item == nullptr)
		return false;
	item->text = text;
	if(m_isRedrawing)
		redraw();
	return true;
}


LParam TreeCtrl::GetItemData(TreeItemHandle item) const
{
	return item ? item->param : 0;
}


bool TreeCtrl::SetItemData(TreeItemHandle item, LParam data)
{
	if(item == nullptr)
		return false;
	item->param = data;
	return true;
}


bool TreeCtrl::SetItemImage(TreeItemHandle item, int image, int selectedImage)
{
	if(item == nullptr)
		return false;
	item->image = image;
	item->selectedImage = selectedImage;
	if(m_isRedrawing)
		redraw();
	return true;
}


bool TreeCtrl::GetItemImage(TreeItemHandle item, int &image, int &selectedImage) const
{
	if(item == nullptr)
		return false;
	image = item->image;
	selectedImage = item->selectedImage;
	return true;
}


uint32 TreeCtrl::GetItemState(TreeItemHandle item, uint32 mask) const
{
	if(item == nullptr)
		return 0;
	uint32 state = item->state;
	if(item->isExpanded)
		state |= TreeStateExpanded;
	if(item == m_selected)
		state |= TreeStateSelected;
	if(item == m_dropTarget)
		state |= TreeStateDropHighlighted;
	return state & mask;
}


bool TreeCtrl::SetItemState(TreeItemHandle item, uint32 state, uint32 mask)
{
	if(item == nullptr)
		return false;
	if(mask & TreeStateExpanded)
		Expand(item, (state & TreeStateExpanded) ? TreeExpand : TreeCollapse);
	item->state = (item->state & ~(mask & ~TreeStateExpanded)) | (state & mask & ~TreeStateExpanded);
	if(m_isRedrawing)
		redraw();
	return true;
}


TreeItemHandle TreeCtrl::GetRootItem() const
{
	return m_root->children.empty() ? nullptr : m_root->children.front().get();
}


TreeItemHandle TreeCtrl::GetChildItem(TreeItemHandle item) const
{
	const TreeItem *parent = item ? item : m_root.get();
	return parent->children.empty() ? nullptr : parent->children.front().get();
}


TreeItemHandle TreeCtrl::GetNextSiblingItem(TreeItemHandle item) const
{
	if(item == nullptr || item->parent == nullptr)
		return nullptr;
	const auto &siblings = item->parent->children;
	for(std::size_t i = 0; i + 1 < siblings.size(); ++i)
	{
		if(siblings[i].get() == item)
			return siblings[i + 1].get();
	}
	return nullptr;
}


TreeItemHandle TreeCtrl::GetPrevSiblingItem(TreeItemHandle item) const
{
	if(item == nullptr || item->parent == nullptr)
		return nullptr;
	const auto &siblings = item->parent->children;
	for(std::size_t i = 1; i < siblings.size(); ++i)
	{
		if(siblings[i].get() == item)
			return siblings[i - 1].get();
	}
	return nullptr;
}


TreeItemHandle TreeCtrl::GetParentItem(TreeItemHandle item) const
{
	if(item == nullptr || item->parent == m_root.get())
		return nullptr;
	return item->parent;
}


void TreeCtrl::BuildRows() const
{
	if(!m_isRowsDirty)
		return;
	m_rows.clear();
	std::vector<std::pair<TreeItem *, int>> stack;
	for(auto it = m_root->children.rbegin(); it != m_root->children.rend(); ++it)
		stack.emplace_back(it->get(), 0);
	while(!stack.empty())
	{
		const auto [item, depth] = stack.back();
		stack.pop_back();
		m_rows.push_back(VisibleRow{item, depth});
		if(item->isExpanded)
		{
			for(auto it = item->children.rbegin(); it != item->children.rend(); ++it)
				stack.emplace_back(it->get(), depth + 1);
		}
	}
	m_isRowsDirty = false;
}


TreeItemHandle TreeCtrl::GetFirstVisibleItem() const
{
	BuildRows();
	if(m_firstRow >= 0 && m_firstRow < static_cast<int>(m_rows.size()))
		return m_rows[m_firstRow].item;
	return nullptr;
}


TreeItemHandle TreeCtrl::GetNextVisibleItem(TreeItemHandle item) const
{
	BuildRows();
	for(std::size_t i = 0; i + 1 < m_rows.size(); ++i)
	{
		if(m_rows[i].item == item)
			return m_rows[i + 1].item;
	}
	return nullptr;
}


TreeItemHandle TreeCtrl::GetNextItem(TreeItemHandle item, uint32 code) const
{
	switch(code)
	{
	case TreeNextSibling: return GetNextSiblingItem(item);
	case TreePreviousSibling: return GetPrevSiblingItem(item);
	case TreeParent: return GetParentItem(item);
	case TreeChild: return GetChildItem(item);
	case TreeFirstVisible: return GetFirstVisibleItem();
	case TreeNextVisible: return GetNextVisibleItem(item);
	case TreePreviousVisible:
	{
		BuildRows();
		for(std::size_t i = 1; i < m_rows.size(); ++i)
		{
			if(m_rows[i].item == item)
				return m_rows[i - 1].item;
		}
		return nullptr;
	}
	case TreeCaret: return m_selected;
	case TreeRootItem: return GetRootItem();
	default: break;
	}
	return nullptr;
}


bool TreeCtrl::ItemHasChildren(TreeItemHandle item) const
{
	return item && !item->children.empty();
}


uint32 TreeCtrl::GetCount() const
{
	uint32 count = 0;
	std::vector<const TreeItem *> stack{m_root.get()};
	while(!stack.empty())
	{
		const TreeItem *item = stack.back();
		stack.pop_back();
		for(const auto &child : item->children)
		{
			++count;
			stack.push_back(child.get());
		}
	}
	return count;
}


bool TreeCtrl::SelectItem(TreeItemHandle item)
{
	if(item == m_selected)
		return true;
	TreeItem *previous = m_selected;
	m_selected = item;
	// Make sure the item is shown
	for(TreeItem *parent = item ? item->parent : nullptr; parent && parent != m_root.get(); parent = parent->parent)
	{
		if(!parent->isExpanded)
		{
			parent->isExpanded = true;
			m_isRowsDirty = true;
		}
	}
	EnsureVisible(item);
	redraw();
	if(GetParent())
	{
		TreeNotification notification;
		notification.item = item;
		notification.oldItem = previous;
		NotifyHeader header;
		header.from = this;
		header.id = GetDlgCtrlID();
		header.code = TreeSelChanged;
		header.extra = &notification;
		RouteNotification(header);
	}
	return true;
}


bool TreeCtrl::SetFirstVisibleItem(TreeItemHandle item)
{
	BuildRows();
	for(int i = 0; i < static_cast<int>(m_rows.size()); ++i)
	{
		if(m_rows[i].item == item)
		{
			m_firstRow = i;
			UpdateScrollBar();
			redraw();
			return true;
		}
	}
	return false;
}


TreeItemHandle TreeCtrl::SelectDropTarget(TreeItemHandle item)
{
	TreeItem *previous = m_dropTarget;
	m_dropTarget = item;
	redraw();
	return previous;
}


bool TreeCtrl::Expand(TreeItemHandle item, uint32 action)
{
	if(item == nullptr)
		return false;
	const bool wasExpanded = item->isExpanded;
	switch(action)
	{
	case TreeCollapse: item->isExpanded = false; break;
	case TreeExpand: item->isExpanded = true; break;
	case TreeToggle: item->isExpanded = !item->isExpanded; break;
	default: return false;
	}
	if(item->isExpanded != wasExpanded)
	{
		m_isRowsDirty = true;
		UpdateScrollBar();
		OnItemExpanded(item);
	}
	return true;
}


void TreeCtrl::OnItemExpanded(TreeItemHandle item)
{
	SendNotification(TreeItemExpanded, item);
}


int TreeCtrl::RowHeight() const
{
	return std::max(18, (m_images ? m_images->GetImageHeight() + 2 : 0));
}


bool TreeCtrl::EnsureVisible(TreeItemHandle item)
{
	BuildRows();
	for(std::size_t i = 0; i < m_rows.size(); ++i)
	{
		if(m_rows[i].item == item)
		{
			const int visibleRows = std::max(h() / RowHeight(), 1);
			if(static_cast<int>(i) < m_firstRow)
				m_firstRow = static_cast<int>(i);
			else if(static_cast<int>(i) >= m_firstRow + visibleRows)
				m_firstRow = static_cast<int>(i) - visibleRows + 1;
			UpdateScrollBar();
			return true;
		}
	}
	return false;
}


void TreeCtrl::UpdateScrollBar()
{
	if(!m_isRedrawing)
		return;
	BuildRows();
	const int visibleRows = std::max(h() / std::max(RowHeight(), 1), 1);
	const int rowCount = static_cast<int>(m_rows.size());
	m_firstRow = std::clamp(m_firstRow, 0, std::max(rowCount - visibleRows, 0));
	if(rowCount > visibleRows)
	{
		m_scrollBar->resize(x() + w() - kScrollBarWidth, y(), kScrollBarWidth, h());
		m_scrollBar->scrollvalue(m_firstRow, visibleRows, 0, rowCount);
		m_scrollBar->show();
	} else
	{
		m_scrollBar->hide();
	}
	redraw();
}


void TreeCtrl::ScrollCallback(Fl_Widget *, void *data)
{
	TreeCtrl *tree = static_cast<TreeCtrl *>(data);
	tree->m_firstRow = tree->m_scrollBar->value();
	tree->redraw();
}


void TreeCtrl::resize(int x, int y, int width, int height)
{
	WndT<Fl_Group>::resize(x, y, width, height);
	UpdateScrollBar();
}


TreeItemHandle TreeCtrl::HitTest(Point point, uint32 *flags) const
{
	BuildRows();
	const int rowHeight = RowHeight();
	const int index = m_firstRow + point.y / rowHeight;
	if(flags)
		*flags = TreeHitNowhere;
	if(point.y < 0 || index < 0 || index >= static_cast<int>(m_rows.size()))
		return nullptr;
	const VisibleRow &row = m_rows[index];
	const int buttonLeft = 2 + row.depth * kIndent;
	const int iconLeft = buttonLeft + kIndent;
	const int iconWidth = m_images ? m_images->GetImageWidth() + 4 : 0;
	if(flags)
	{
		if(point.x < iconLeft)
			*flags = TreeHitOnButton;
		else if(point.x < iconLeft + iconWidth)
			*flags = TreeHitOnIcon;
		else
			*flags = TreeHitOnLabel;
	}
	return row.item;
}


bool TreeCtrl::GetItemRect(TreeItemHandle item, Rect &rect, bool isTextOnly) const
{
	BuildRows();
	const int rowHeight = RowHeight();
	for(int i = 0; i < static_cast<int>(m_rows.size()); ++i)
	{
		if(m_rows[i].item != item)
			continue;
		if(i < m_firstRow)
			return false;
		const int top = (i - m_firstRow) * rowHeight;
		if(top >= h())
			return false;
		const int left = 2 + m_rows[i].depth * kIndent + kIndent;
		int textLeft = left + (m_images ? m_images->GetImageWidth() + 4 : 0);
		fl_font(labelfont(), 12);
		const int textWidth = static_cast<int>(fl_width(ToUtf8(item->text).c_str())) + 2 * kTextPadding;
		rect = isTextOnly ? Rect(textLeft, top, textLeft + textWidth, top + rowHeight) : Rect(left, top, textLeft + textWidth, top + rowHeight);
		return true;
	}
	return false;
}


void TreeCtrl::draw()
{
	fl_push_clip(x(), y(), w(), h());
	fl_color(GetRValue(m_backgroundColor), GetGValue(m_backgroundColor), GetBValue(m_backgroundColor));
	fl_rectf(x(), y(), w(), h());
	BuildRows();
	const int rowHeight = RowHeight();
	const int visibleRows = h() / rowHeight + 2;
	Painter painter(Point(x(), y()));
	for(int i = 0; i < visibleRows && m_firstRow + i < static_cast<int>(m_rows.size()); ++i)
	{
		const VisibleRow &row = m_rows[m_firstRow + i];
		TreeItem *item = row.item;
		const int top = i * rowHeight;
		const int buttonLeft = 2 + row.depth * kIndent;
		if(m_style & TreeStyleHasButtons && !item->children.empty())
		{
			const int cx = buttonLeft + kIndent / 2;
			const int cy = top + rowHeight / 2;
			painter.FrameRect(Rect(cx - 4, cy - 4, cx + 5, cy + 5), RGB(128, 128, 128));
			painter.DrawLine(cx - 2, cy, cx + 3, cy);
			if(!item->isExpanded)
				painter.DrawLine(cx, cy - 2, cx, cy + 3);
		}
		int left = buttonLeft + kIndent;
		const bool isSelected = (item == m_selected);
		const int imageIndex = isSelected ? item->selectedImage : item->image;
		if(m_images)
		{
			m_images->Draw(painter, imageIndex, Point(left, top + (rowHeight - m_images->GetImageHeight()) / 2));
			left += m_images->GetImageWidth() + 4;
		}
		const std::string text = ToUtf8(item->text);
		fl_font((item->state & TreeStateBold) ? FL_HELVETICA_BOLD : FL_HELVETICA, 12);
		const int textWidth = static_cast<int>(fl_width(text.c_str())) + 2 * kTextPadding;
		const bool isDropTarget = (item == m_dropTarget);
		if(isSelected || isDropTarget)
		{
			const bool isActive = (Fl::focus() == this) || isDropTarget;
			painter.FillSolidRect(Rect(left, top, left + textWidth, top + rowHeight), GetSystemColor(isActive ? SysColor::Highlight : SysColor::ButtonShadow));
			fl_color(FL_WHITE);
		} else
		{
			fl_color((item->state & TreeStateCut) ? FL_INACTIVE_COLOR : FL_FOREGROUND_COLOR);
		}
		fl_draw(text.c_str(), x() + left + kTextPadding, y() + top + rowHeight / 2 + fl_height() / 2 - fl_descent());
	}
	fl_pop_clip();
	draw_children();
}


void TreeCtrl::SendNotification(uint32 code, TreeItemHandle item)
{
	if(GetParent())
	{
		TreeNotification notification;
		notification.item = item;
		NotifyHeader header;
		header.from = this;
		header.id = GetDlgCtrlID();
		header.code = code;
		header.extra = &notification;
		RouteNotification(header);
	}
}


void TreeCtrl::OnLButtonDown(uint32, Point point)
{
	if(m_editedItem)
		FinishEdit(true);
	uint32 hit = 0;
	TreeItem *item = HitTest(point, &hit);
	if(item == nullptr)
		return;
	if(hit == TreeHitOnButton && !item->children.empty())
	{
		Expand(item, TreeToggle);
		return;
	}
	SelectItem(item);
	SendNotification(TreeClick, item);
	m_dragItem = item;
	m_pressPoint = point;
	m_isDragCandidate = true;
	m_dragButton = FL_LEFT_MOUSE;
}


void TreeCtrl::OnLButtonUp(uint32, Point)
{
	m_isDragCandidate = false;
}


void TreeCtrl::OnLButtonDblClk(uint32, Point point)
{
	uint32 hit = 0;
	TreeItem *item = HitTest(point, &hit);
	if(item == nullptr)
		return;
	SendNotification(TreeDblClick, item);
	if(!item->children.empty() && hit != TreeHitNowhere)
		Expand(item, TreeToggle);
}


void TreeCtrl::OnRButtonDown(uint32, Point point)
{
	TreeItem *item = HitTest(point);
	if(item)
	{
		SelectItem(item);
		m_dragItem = item;
		m_pressPoint = point;
		m_isDragCandidate = true;
		m_dragButton = FL_RIGHT_MOUSE;
	}
}


void TreeCtrl::OnRButtonUp(uint32, Point point)
{
	const bool wasDragCandidate = m_isDragCandidate;
	m_isDragCandidate = false;
	if(wasDragCandidate || HitTest(point))
		SendNotification(TreeRClick, HitTest(point));
}


void TreeCtrl::OnMouseMove(uint32, Point point)
{
	if(m_isDragCandidate && m_dragItem && (std::abs(point.x - m_pressPoint.x) > kDragThreshold || std::abs(point.y - m_pressPoint.y) > kDragThreshold))
	{
		m_isDragCandidate = false;
		SendNotification(m_dragButton == FL_LEFT_MOUSE ? TreeBeginDrag : TreeBeginRDrag, m_dragItem);
	}
}


bool TreeCtrl::OnMouseWheel(uint32, int16 delta, Point)
{
	BuildRows();
	m_firstRow -= delta / 120 * 3;
	UpdateScrollBar();
	return true;
}


bool TreeCtrl::OnKeyDown(uint32 key, uint32, uint32)
{
	BuildRows();
	if(m_rows.empty())
		return false;
	int index = -1;
	for(int i = 0; i < static_cast<int>(m_rows.size()); ++i)
	{
		if(m_rows[i].item == m_selected)
		{
			index = i;
			break;
		}
	}
	switch(key)
	{
	case Key_UP:
		if(index > 0)
			SelectItem(m_rows[index - 1].item);
		else if(index < 0)
			SelectItem(m_rows.front().item);
		return true;
	case Key_DOWN:
		if(index + 1 < static_cast<int>(m_rows.size()))
			SelectItem(m_rows[index + 1].item);
		return true;
	case Key_HOME:
		SelectItem(m_rows.front().item);
		return true;
	case Key_END:
		SelectItem(m_rows.back().item);
		return true;
	case Key_RETURN:
		if(m_selected)
			SendNotification(TreeReturn, m_selected);
		return true;
	case Key_RIGHT:
		if(m_selected && !m_selected->children.empty())
		{
			if(!m_selected->isExpanded)
				Expand(m_selected, TreeExpand);
			else
				SelectItem(m_selected->children.front().get());
		}
		return true;
	case Key_LEFT:
		if(m_selected)
		{
			if(m_selected->isExpanded && !m_selected->children.empty())
				Expand(m_selected, TreeCollapse);
			else if(m_selected->parent && m_selected->parent != m_root.get())
				SelectItem(m_selected->parent);
		}
		return true;
	case Key_PRIOR:
		if(index >= 0)
			SelectItem(m_rows[std::max(index - h() / RowHeight(), 0)].item);
		return true;
	case Key_NEXT:
		if(index >= 0)
			SelectItem(m_rows[std::min(index + h() / RowHeight(), static_cast<int>(m_rows.size()) - 1)].item);
		return true;
	default:
		break;
	}
	return false;
}


Edit *TreeCtrl::EditLabel(TreeItemHandle item)
{
	Rect rect;
	if(item == nullptr || !GetItemRect(item, rect, true))
		return nullptr;
	FinishEdit(false);
	m_editedItem = item;
	m_editor = std::make_unique<Edit>(x() + rect.left, y() + rect.top, std::max(rect.Width() + 20, 80), rect.Height());
	add(m_editor.get());
	m_editor->SetWindowText(item->text);
	m_editor->SetFocus();
	m_editor->SelectAll();
	m_editor->when(FL_WHEN_ENTER_KEY | FL_WHEN_NOT_CHANGED);
	m_editor->callback(
	    [](Fl_Widget *, void *data)
	    {
		    static_cast<TreeCtrl *>(data)->FinishEdit(true);
	    },
	    this);
	if(GetParent())
	{
		TreeNotification notification;
		notification.item = item;
		NotifyHeader header;
		header.from = this;
		header.id = GetDlgCtrlID();
		header.code = TreeBeginLabelEdit;
		header.extra = &notification;
		LResult result = 0;
		RouteNotification(header, &result);
		if(result != 0)
		{
			// Editing was refused
			m_editedItem = nullptr;
			remove(*m_editor);
			m_editor.reset();
			redraw();
			return nullptr;
		}
	}
	return m_editor.get();
}


void TreeCtrl::FinishEdit(bool isAccepted)
{
	if(m_editedItem == nullptr || m_editor == nullptr)
		return;
	TreeItem *item = m_editedItem;
	m_editedItem = nullptr;
	const mpt::ustring text = m_editor->GetWindowText();
	remove(m_editor.get());
	m_editor.reset();
	{
		if(GetParent())
		{
			TreeNotification notification;
			notification.item = item;
			notification.text = isAccepted ? &text : nullptr;
			NotifyHeader header;
			header.from = this;
			header.id = GetDlgCtrlID();
			header.code = TreeEndLabelEdit;
			header.extra = &notification;
			LResult result = 0;
			RouteNotification(header, &result);
			if(result != 0 && isAccepted)
				item->text = text;
		}
	}
	redraw();
}


bool TreeCtrl::SortChildren(TreeItemHandle item)
{
	TreeItem *parent = item ? item : m_root.get();
	std::stable_sort(parent->children.begin(), parent->children.end(), [](const auto &a, const auto &b) { return ToUtf8(a->text) < ToUtf8(b->text); });
	m_isRowsDirty = true;
	redraw();
	return true;
}


}  // namespace ui


OPENMPT_NAMESPACE_END
