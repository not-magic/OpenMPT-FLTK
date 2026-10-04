// MFC replacement on FLTK. Tree of items with text, icons and expandable branches.

#include "stdafx.h"
#include "TreeCtrl.h"

#include <FL/Fl.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <cstddef>
#include <cstdlib>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{


namespace
{

// The position handles are only compared by address
alignas(TreeItem) std::byte sentinelStorage[3][sizeof(TreeItem)];

constexpr int kTextPadding = 4;
constexpr int kDragThreshold = 4;
constexpr int kFontSize = 12;
constexpr int kLineSpacing = 2;

std::string ToUtf8(const mpt::ustring &text)
{
	return mpt::transcode<std::string>(mpt::common_encoding::utf8, text);
}

void SetItemFont(const TreeItem &item)
{
	fl_font((item.state & TreeStateBold) ? FL_HELVETICA_BOLD : FL_HELVETICA, kFontSize);
}

int FindTextWidth(TreeItem &item)
{
	if(item.textWidth < 0)
	{
		SetItemFont(item);
		item.textWidth = static_cast<int>(fl_width(item.label() ? item.label() : "")) + 2 * kTextPadding;
	}
	return item.textWidth;
}

int CalcIconWidth(const ImageList *images)
{
	return images ? images->GetImageWidth() + 4 : 0;
}

void AssignItemText(TreeItem &item, const mpt::ustring &text)
{
	item.text = text;
	item.label(ToUtf8(text).c_str());
	item.textWidth = -1;
}

TreeItem *ToTreeItem(Fl_Tree_Item *item)
{
	return static_cast<TreeItem *>(item);
}

bool IsAncestor(const Fl_Tree_Item *ancestor, const Fl_Tree_Item *item)
{
	for(const Fl_Tree_Item *walker = item; walker != nullptr; walker = walker->parent())
	{
		if(walker == ancestor)
			return true;
	}
	return false;
}

// Next item in display order, ignoring the hidden root
TreeItem *FindNextVisible(TreeItem *item, const TreeItem *root)
{
	if(item->is_open() && item->has_children())
		return ToTreeItem(item->child(0));
	for(Fl_Tree_Item *walker = item; walker != nullptr && walker != root; walker = walker->parent())
	{
		if(Fl_Tree_Item *sibling = walker->next_sibling())
			return ToTreeItem(sibling);
	}
	return nullptr;
}

TreeItem *FindPrevVisible(TreeItem *item, const TreeItem *root)
{
	Fl_Tree_Item *sibling = item->prev_sibling();
	if(sibling == nullptr)
		return (item->parent() == root) ? nullptr : ToTreeItem(item->parent());
	while(sibling->is_open() && sibling->has_children())
		sibling = sibling->child(sibling->children() - 1);
	return ToTreeItem(sibling);
}

TreeItem *FindFirstVisible(const TreeItem *root)
{
	return root->has_children() ? ToTreeItem(const_cast<TreeItem *>(root)->child(0)) : nullptr;
}

}  // namespace


TreeItem *const TreeFirst = reinterpret_cast<TreeItem *>(sentinelStorage[0]);
TreeItem *const TreeLast = reinterpret_cast<TreeItem *>(sentinelStorage[1]);
TreeItem *const TreeSort = reinterpret_cast<TreeItem *>(sentinelStorage[2]);


int TreeItem::draw_item_content(int render)
{
	const TreeCtrl *treeCtrl = dynamic_cast<const TreeCtrl *>(tree());
	const ImageList *images = treeCtrl ? treeCtrl->GetImageList() : nullptr;
	const int iconWidth = CalcIconWidth(images);
	const int textWidth = FindTextWidth(*this);
	if(render && treeCtrl)
	{
		Painter painter(Point(label_x(), label_y()));
		const bool isSelected = (this == treeCtrl->GetSelectedItem());
		if(images)
			images->Draw(painter, isSelected ? selectedImage : image, Point(0, (label_h() - images->GetImageHeight()) / 2));
		const bool isDropTarget = (this == treeCtrl->GetDropHighlightItem());
		if(isSelected || isDropTarget)
		{
			const bool isActive = (Fl::focus() == treeCtrl) || isDropTarget;
			painter.FillSolidRect(Rect(iconWidth, 0, iconWidth + textWidth, label_h()), GetSystemColor(isActive ? SysColor::Highlight : SysColor::ButtonShadow));
			fl_color(FL_WHITE);
		} else
		{
			fl_color((state & TreeStateCut) ? FL_INACTIVE_COLOR : FL_FOREGROUND_COLOR);
		}
		SetItemFont(*this);
		if(label())
			fl_draw(label(), label_x() + iconWidth + kTextPadding, label_y() + label_h() / 2 + fl_height() / 2 - fl_descent());
	}
	return label_x() + iconWidth + textWidth;
}


TreeCtrl::TreeCtrl(int x, int y, int width, int height, const char *label)
    : WndT<Fl_Tree>(x, y, width, height, label)
{
	m_isFocusable = true;
	m_isCustomPaint = false;
	box(FL_DOWN_BOX);
	color(FL_WHITE);
	selectmode(FL_TREE_SELECT_NONE);
	item_labelfont(FL_HELVETICA);
	item_labelsize(kFontSize);
	linespacing(kLineSpacing);
	m_root = new TreeItem(this);
	root(m_root);
	showroot(0);
	SetStyle(m_style);
}


void TreeCtrl::ConfigureFromTemplate(const DialogControl &control)
{
	// Style bits of the dialog templates are the same as the style flags
	SetStyle(control.style & (TreeStyleHasButtons | TreeStyleHasLines | TreeStyleLinesAtRoot | TreeStyleEditLabels | TreeStyleShowSelectionAlways | TreeStyleSingleExpand));
}


void TreeCtrl::SetStyle(uint32 style)
{
	m_style = style;
	showcollapse((m_style & TreeStyleHasButtons) ? 1 : 0);
	connectorstyle((m_style & TreeStyleHasLines) ? FL_TREE_CONNECTOR_DOTTED : FL_TREE_CONNECTOR_NONE);
	RefreshLayout();
}


void TreeCtrl::SetImageList(ImageList *images, int)
{
	m_images = images;
	RefreshLayout();
}


void TreeCtrl::SetRedraw(bool isRedrawing)
{
	m_isRedrawing = isRedrawing;
	RefreshLayout();
}


void TreeCtrl::SetBackColor(ColorRef color)
{
	this->color(fl_rgb_color(GetRValue(color), GetGValue(color), GetBValue(color)));
	RefreshLayout();
}


void TreeCtrl::RefreshLayout()
{
	if(!m_isRedrawing)
		return;
	recalc_tree();
	redraw();
}


void TreeCtrl::UpdateItemPositions() const
{
	const_cast<TreeCtrl *>(this)->calc_tree();
}


TreeItemHandle TreeCtrl::InsertItem(const mpt::ustring &text, int image, int selectedImage, TreeItemHandle parent, TreeItemHandle after)
{
	return InsertItem(TreeItemText | TreeItemImage | TreeItemSelectedImage, text, image, selectedImage, 0, 0, 0, parent, after);
}


TreeItemHandle TreeCtrl::InsertItem(uint32 mask, const mpt::ustring &text, int image, int selectedImage, uint32 state, uint32 stateMask, LParam param, TreeItemHandle parent, TreeItemHandle after)
{
	TreeItem *parentItem = parent ? parent : m_root;
	TreeItem *item = new TreeItem(this);
	AssignItemText(*item, (mask & TreeItemText) ? text : mpt::ustring());
	if(mask & TreeItemImage)
		item->image = image;
	if(mask & TreeItemSelectedImage)
		item->selectedImage = selectedImage;
	if(mask & TreeItemParam)
		item->param = param;
	if(mask & TreeItemState)
		item->state = (state & stateMask);
	item->close();
	const int childCount = parentItem->children();
	int position = childCount;
	if(after == TreeFirst)
	{
		position = 0;
	} else if(after == TreeSort)
	{
		const std::string key = ToUtf8(item->text);
		position = 0;
		while(position < childCount && ToUtf8(ToTreeItem(parentItem->child(position))->text) <= key)
			++position;
	} else if(after != TreeLast && after != nullptr)
	{
		const int afterIndex = parentItem->find_child(after);
		if(afterIndex >= 0)
			position = afterIndex + 1;
	}
	parentItem->reparent(item, position);
	RefreshLayout();
	return item;
}


bool TreeCtrl::SetItem(TreeItemHandle item, uint32 mask, const mpt::ustring &text, int image, int selectedImage, uint32 state, uint32 stateMask, LParam param)
{
	if(item == nullptr)
		return false;
	if(mask & TreeItemText)
		AssignItemText(*item, text);
	if(mask & TreeItemImage)
		item->image = image;
	if(mask & TreeItemSelectedImage)
		item->selectedImage = selectedImage;
	if(mask & TreeItemParam)
		item->param = param;
	if(mask & TreeItemState)
	{
		item->state = (item->state & ~stateMask) | (state & stateMask);
		item->textWidth = -1;
	}
	RefreshLayout();
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


bool TreeCtrl::DeleteItem(TreeItemHandle item)
{
	// Like TVI_ROOT, the root deletes all items
	if(item == TreeRoot)
	{
		while(m_root->has_children())
			DeleteItem(ToTreeItem(m_root->child(m_root->children() - 1)));
		return true;
	}
	if(item->parent() == nullptr)
		return false;
	if(m_selected && IsAncestor(item, m_selected))
		m_selected = item->parent() == m_root ? nullptr : ToTreeItem(item->parent());
	if(m_dropTarget && IsAncestor(item, m_dropTarget))
		m_dropTarget = nullptr;
	if(m_dragItem && IsAncestor(item, m_dragItem))
		m_dragItem = nullptr;
	if(m_editedItem && IsAncestor(item, m_editedItem))
		FinishEdit(false);
	SendNotification(TreeDeleteItem, item);
	set_item_focus(nullptr);
	remove(item);
	RefreshLayout();
	return true;
}


bool TreeCtrl::DeleteAllItems()
{
	m_selected = nullptr;
	m_dropTarget = nullptr;
	m_dragItem = nullptr;
	if(m_editedItem)
		FinishEdit(false);
	set_item_focus(nullptr);
	clear_children(m_root);
	RefreshLayout();
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
	AssignItemText(*item, text);
	RefreshLayout();
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
	if(item->is_open())
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
	const uint32 oldState = item->state;
	item->state = (item->state & ~(mask & ~TreeStateExpanded)) | (state & mask & ~TreeStateExpanded);
	if((oldState ^ item->state) & TreeStateBold)
	{
		item->textWidth = -1;
		RefreshLayout();
	} else if(m_isRedrawing)
	{
		redraw();
	}
	return true;
}


TreeItemHandle TreeCtrl::GetRootItem() const
{
	return FindFirstVisible(m_root);
}


TreeItemHandle TreeCtrl::GetChildItem(TreeItemHandle item) const
{
	TreeItem *parent = item ? item : m_root;
	return parent->has_children() ? ToTreeItem(parent->child(0)) : nullptr;
}


TreeItemHandle TreeCtrl::GetNextSiblingItem(TreeItemHandle item) const
{
	return item ? ToTreeItem(item->next_sibling()) : nullptr;
}


TreeItemHandle TreeCtrl::GetPrevSiblingItem(TreeItemHandle item) const
{
	return item ? ToTreeItem(item->prev_sibling()) : nullptr;
}


TreeItemHandle TreeCtrl::GetParentItem(TreeItemHandle item) const
{
	if(item == nullptr || item->parent() == m_root)
		return nullptr;
	return ToTreeItem(item->parent());
}


TreeItemHandle TreeCtrl::GetFirstVisibleItem() const
{
	UpdateItemPositions();
	for(TreeItem *item = FindFirstVisible(m_root); item != nullptr; item = FindNextVisible(item, m_root))
	{
		if(item->y() + item->h() > _tiy)
			return item;
	}
	return nullptr;
}


TreeItemHandle TreeCtrl::GetNextVisibleItem(TreeItemHandle item) const
{
	return item ? FindNextVisible(item, m_root) : nullptr;
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
	case TreePreviousVisible: return item ? FindPrevVisible(item, m_root) : nullptr;
	case TreeCaret: return m_selected;
	case TreeRootItem: return GetRootItem();
	default: break;
	}
	return nullptr;
}


bool TreeCtrl::ItemHasChildren(TreeItemHandle item) const
{
	return item && item->has_children();
}


uint32 TreeCtrl::GetCount() const
{
	uint32 count = 0;
	std::vector<Fl_Tree_Item *> stack{m_root};
	while(!stack.empty())
	{
		Fl_Tree_Item *item = stack.back();
		stack.pop_back();
		for(int i = 0; i < item->children(); ++i)
		{
			++count;
			stack.push_back(item->child(i));
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
	if(item == nullptr)
		return false;
	UpdateItemPositions();
	show_item_top(item);
	return true;
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
	const bool wasExpanded = item->is_open();
	switch(action)
	{
	case TreeCollapse: item->close(); break;
	case TreeExpand: item->open(); break;
	case TreeToggle: item->open_toggle(); break;
	default: return false;
	}
	if(item->is_open() != wasExpanded)
	{
		RefreshLayout();
		OnItemExpanded(item);
	}
	return true;
}


void TreeCtrl::OnItemExpanded(TreeItemHandle item)
{
	SendNotification(TreeItemExpanded, item);
}


bool TreeCtrl::EnsureVisible(TreeItemHandle item)
{
	if(item == nullptr)
		return false;
	bool hasOpened = false;
	for(Fl_Tree_Item *parent = item->parent(); parent != nullptr && parent != m_root; parent = parent->parent())
	{
		if(!parent->is_open())
		{
			parent->open();
			hasOpened = true;
		}
	}
	if(hasOpened)
		RefreshLayout();
	UpdateItemPositions();
	show_item(item);
	return true;
}


TreeItemHandle TreeCtrl::HitTest(Point point, uint32 *flags) const
{
	if(flags)
		*flags = TreeHitNowhere;
	UpdateItemPositions();
	const int windowX = x() + point.x;
	const int windowY = y() + point.y;
	if(windowY < _tiy || windowY >= _tiy + _tih || windowX < _tix || windowX >= _tix + _tiw)
		return nullptr;
	for(TreeItem *item = FindFirstVisible(m_root); item != nullptr; item = FindNextVisible(item, m_root))
	{
		if(windowY < item->y() || windowY >= item->y() + item->h() + linespacing())
			continue;
		if(flags)
		{
			if(windowX < item->label_x())
				*flags = TreeHitOnButton;
			else if(windowX < item->label_x() + CalcIconWidth(m_images))
				*flags = TreeHitOnIcon;
			else
				*flags = TreeHitOnLabel;
		}
		return item;
	}
	return nullptr;
}


bool TreeCtrl::GetItemRect(TreeItemHandle item, Rect &rect, bool isTextOnly) const
{
	if(item == nullptr || !item->visible_r())
		return false;
	UpdateItemPositions();
	if(item->y() + item->h() <= _tiy || item->y() >= _tiy + _tih)
		return false;
	const int left = item->label_x() - x();
	const int textLeft = left + CalcIconWidth(m_images);
	const int top = item->y() - y();
	const int textWidth = FindTextWidth(*item);
	rect = Rect(isTextOnly ? textLeft : left, top, textLeft + textWidth, top + item->h());
	return true;
}


int TreeCtrl::handle(int event)
{
	switch(event)
	{
	case FL_PUSH:
		if((_vscroll->visible() && Fl::event_inside(_vscroll)) || (_hscroll->visible() && Fl::event_inside(_hscroll)))
			break;
		m_isMouseCaptured = true;
		if(m_isFocusable && Fl::focus() != this)
			take_focus();
		if(!PreTranslateMessage(event))
			DispatchEvent(event);
		return 1;
	case FL_DRAG:
	case FL_RELEASE:
		if(!m_isMouseCaptured)
			break;
		if(event == FL_RELEASE && Fl::event_buttons() == 0)
			m_isMouseCaptured = false;
		if(!PreTranslateMessage(event))
			DispatchEvent(event);
		return 1;
	case FL_MOVE:
		WndT<Fl_Tree>::handle(event);
		DispatchEvent(event);
		return 1;
	case FL_KEYBOARD:
		if(PreTranslateMessage(event))
			return 1;
		return DispatchEvent(event) ? 1 : 0;
	case FL_FOCUS:
	{
		// Fl_Tree draws a focus box around its keyboard item, but the selection is drawn by the items
		const int result = WndT<Fl_Tree>::handle(event);
		set_item_focus(nullptr);
		return result;
	}
	default:
		break;
	}
	return WndT<Fl_Tree>::handle(event);
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
	if(hit == TreeHitOnButton && item->has_children())
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
	if(item->has_children() && hit != TreeHitNowhere)
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
	TreeItem *item = HitTest(point);
	if(wasDragCandidate || item)
		SendNotification(TreeRClick, item);
}


void TreeCtrl::OnMouseMove(uint32, Point point)
{
	if(m_isDragCandidate && m_dragItem && (std::abs(point.x - m_pressPoint.x) > kDragThreshold || std::abs(point.y - m_pressPoint.y) > kDragThreshold))
	{
		m_isDragCandidate = false;
		SendNotification(m_dragButton == FL_LEFT_MOUSE ? TreeBeginDrag : TreeBeginRDrag, m_dragItem);
	}
}


bool TreeCtrl::OnKeyDown(uint32 key, uint32, uint32)
{
	TreeItem *first = FindFirstVisible(m_root);
	if(first == nullptr)
		return false;
	switch(key)
	{
	case Key_UP:
		if(m_selected == nullptr)
			SelectItem(first);
		else if(TreeItem *previous = FindPrevVisible(m_selected, m_root))
			SelectItem(previous);
		return true;
	case Key_DOWN:
		if(m_selected == nullptr)
			SelectItem(first);
		else if(TreeItem *next = FindNextVisible(m_selected, m_root))
			SelectItem(next);
		return true;
	case Key_HOME:
		SelectItem(first);
		return true;
	case Key_END:
	{
		TreeItem *last = first;
		while(TreeItem *next = FindNextVisible(last, m_root))
			last = next;
		SelectItem(last);
		return true;
	}
	case Key_RETURN:
		if(m_selected)
			SendNotification(TreeReturn, m_selected);
		return true;
	case Key_RIGHT:
		if(m_selected && m_selected->has_children())
		{
			if(!m_selected->is_open())
				Expand(m_selected, TreeExpand);
			else
				SelectItem(ToTreeItem(m_selected->child(0)));
		}
		return true;
	case Key_LEFT:
		if(m_selected)
		{
			if(m_selected->is_open() && m_selected->has_children())
				Expand(m_selected, TreeCollapse);
			else if(TreeItem *parent = GetParentItem(m_selected))
				SelectItem(parent);
		}
		return true;
	case Key_PRIOR:
	case Key_NEXT:
		if(m_selected)
		{
			UpdateItemPositions();
			const int pageRows = std::max(_tih / std::max(m_selected->h() + linespacing(), 1), 1);
			TreeItem *target = m_selected;
			for(int i = 0; i < pageRows; ++i)
			{
				TreeItem *next = (key == Key_PRIOR) ? FindPrevVisible(target, m_root) : FindNextVisible(target, m_root);
				if(next == nullptr)
					break;
				target = next;
			}
			SelectItem(target);
		}
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
	Fl_Group *editorParent = parent();
	if(editorParent == nullptr)
		return nullptr;
	FinishEdit(false);
	m_editedItem = item;
	// Fl_Tree only draws its own items, so the editor lives next to the tree
	m_editor = std::make_unique<Edit>(x() + rect.left, y() + rect.top, std::max(rect.Width() + 20, 80), rect.Height());
	editorParent->add(m_editor.get());
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
			editorParent->remove(m_editor.get());
			m_editor.reset();
			editorParent->redraw();
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
	if(Fl_Group *editorParent = m_editor->parent())
	{
		editorParent->remove(m_editor.get());
		editorParent->redraw();
	}
	m_editor.reset();
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
			AssignItemText(*item, text);
	}
	RefreshLayout();
}


bool TreeCtrl::SortChildren(TreeItemHandle item)
{
	TreeItem *parentItem = item ? item : m_root;
	std::vector<TreeItem *> children;
	while(parentItem->has_children())
		children.push_back(ToTreeItem(parentItem->deparent(parentItem->children() - 1)));
	std::reverse(children.begin(), children.end());
	std::stable_sort(children.begin(), children.end(), [](const TreeItem *a, const TreeItem *b) { return ToUtf8(a->text) < ToUtf8(b->text); });
	for(TreeItem *child : children)
		parentItem->reparent(child, parentItem->children());
	RefreshLayout();
	return true;
}


}  // namespace ui


OPENMPT_NAMESPACE_END
