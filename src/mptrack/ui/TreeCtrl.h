// MFC replacement on FLTK. Tree of items with text, icons and expandable branches.

#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "Controls.h"
#include "ImageList.h"

#include <FL/Fl_Tree.H>
#include <FL/Fl_Tree_Item.H>

#include <memory>
#include <string>
#include <vector>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{

struct TreeItem;
using TreeItemHandle = TreeItem *;

// Special handles for positions
inline TreeItemHandle const TreeRoot = nullptr;
extern TreeItem *const TreeFirst;
extern TreeItem *const TreeLast;
extern TreeItem *const TreeSort;

enum TreeItemMask : uint32
{
	TreeItemText = 0x01,
	TreeItemImage = 0x02,
	TreeItemParam = 0x04,
	TreeItemState = 0x08,
	TreeItemHandleMask = 0x10,
	TreeItemSelectedImage = 0x20,
};

enum TreeItemStateFlags : uint32
{
	TreeStateBold = 0x01,
	TreeStateExpanded = 0x02,
	TreeStateCut = 0x04,
	TreeStateSelected = 0x08,
	TreeStateDropHighlighted = 0x10,
};

enum TreeExpandAction : uint32
{
	TreeCollapse = 1,
	TreeExpand = 2,
	TreeToggle = 3,
};

enum TreeNextCode : uint32
{
	TreeNextSibling = 1,
	TreePreviousSibling = 2,
	TreeParent = 3,
	TreeChild = 4,
	TreeFirstVisible = 5,
	TreeNextVisible = 6,
	TreePreviousVisible = 7,
	TreeRootItem = 0,
	TreeCaret = 9,
};

enum TreeStyle : uint32
{
	TreeStyleHasButtons = 0x01,
	TreeStyleHasLines = 0x02,
	TreeStyleLinesAtRoot = 0x04,
	TreeStyleEditLabels = 0x08,
	TreeStyleShowSelectionAlways = 0x20,
	TreeStyleSingleExpand = 0x400,
};

enum TreeHitFlags : uint32
{
	TreeHitNowhere = 0x01,
	TreeHitOnIcon = 0x02,
	TreeHitOnLabel = 0x04,
	TreeHitOnButton = 0x10,
	TreeHitOnItem = TreeHitOnIcon | TreeHitOnLabel,
};

// Item attributes in the style of the platform tree control
struct TreeItemInfo
{
	uint32 mask = 0;
	TreeItemHandle item = nullptr;
	mpt::ustring text;
	int image = 0;
	int selectedImage = 0;
	uint32 state = 0;
	uint32 stateMask = 0;
	LParam param = 0;
};


struct TreeItem : public Fl_Tree_Item
{
	explicit TreeItem(Fl_Tree *tree) : Fl_Tree_Item(tree) { }
	int draw_item_content(int render) override;

	mpt::ustring text;
	LParam param = 0;
	uint32 state = 0;
	int image = 0;
	int selectedImage = 0;
	// Cached label width including padding; negative until measured
	int textWidth = -1;
};


// Fl_Tree lays out and scrolls the items; mouse and keyboard input goes through the Wnd handlers like a platform tree control
class TreeCtrl : public WndT<Fl_Tree>
{
public:
	TreeCtrl(int x = 0, int y = 0, int width = 0, int height = 0, const char *label = nullptr);

	void ConfigureFromTemplate(const DialogControl &control) override;

	TreeItemHandle InsertItem(const mpt::ustring &text, int image, int selectedImage, TreeItemHandle parent = TreeRoot, TreeItemHandle after = TreeLast);
	TreeItemHandle InsertItem(const mpt::ustring &text, TreeItemHandle parent = TreeRoot, TreeItemHandle after = TreeLast) { return InsertItem(text, 0, 0, parent, after); }
	TreeItemHandle InsertItem(uint32 mask, const mpt::ustring &text, int image, int selectedImage, uint32 state, uint32 stateMask, LParam param, TreeItemHandle parent, TreeItemHandle after);
	bool SetItem(TreeItemHandle item, uint32 mask, const mpt::ustring &text, int image, int selectedImage, uint32 state, uint32 stateMask, LParam param);
	bool GetItem(TreeItemInfo *info) const;
	bool DeleteItem(TreeItemHandle item);
	bool DeleteAllItems();

	mpt::ustring GetItemText(TreeItemHandle item) const;
	bool SetItemText(TreeItemHandle item, const mpt::ustring &text);
	LParam GetItemData(TreeItemHandle item) const;
	bool SetItemData(TreeItemHandle item, LParam data);
	bool SetItemImage(TreeItemHandle item, int image, int selectedImage);
	bool GetItemImage(TreeItemHandle item, int &image, int &selectedImage) const;
	uint32 GetItemState(TreeItemHandle item, uint32 mask) const;
	bool SetItemState(TreeItemHandle item, uint32 state, uint32 mask);

	TreeItemHandle GetRootItem() const;
	TreeItemHandle GetChildItem(TreeItemHandle item) const;
	TreeItemHandle GetNextSiblingItem(TreeItemHandle item) const;
	TreeItemHandle GetPrevSiblingItem(TreeItemHandle item) const;
	TreeItemHandle GetParentItem(TreeItemHandle item) const;
	TreeItemHandle GetSelectedItem() const noexcept { return m_selected; }
	TreeItemHandle GetFirstVisibleItem() const;
	TreeItemHandle GetNextItem(TreeItemHandle item, uint32 code) const;
	TreeItemHandle GetNextVisibleItem(TreeItemHandle item) const;
	bool ItemHasChildren(TreeItemHandle item) const;
	uint32 GetCount() const;

	bool SelectItem(TreeItemHandle item);
	bool Select(TreeItemHandle item, uint32 = 0) { return SelectItem(item); }
	TreeItemHandle SelectDropTarget(TreeItemHandle item);
	TreeItemHandle GetDropHighlightItem() const noexcept { return m_dropTarget; }
	// Scrolls so that the item is the first one that is shown
	bool SetFirstVisibleItem(TreeItemHandle item);
	bool Expand(TreeItemHandle item, uint32 action);
	bool EnsureVisible(TreeItemHandle item);
	TreeItemHandle HitTest(Point point, uint32 *flags = nullptr) const;
	bool GetItemRect(TreeItemHandle item, Rect &rect, bool isTextOnly) const;
	Edit *EditLabel(TreeItemHandle item);
	Edit *GetEditControl() { return m_editor.get(); }
	void EndEditLabel(bool isCancelled) { FinishEdit(!isCancelled); }
	ImageList *GetImageList() const noexcept { return m_images; }
	void SetImageList(ImageList *images, int = 0);
	void SetRedraw(bool isRedrawing);
	uint32 GetStyle() const noexcept { return m_style; }
	void SetStyle(uint32 style);
	void ModifyStyle(uint32 remove, uint32 add) { SetStyle((m_style & ~remove) | add); }
	void SetIndent(int) { }
	// Sorts the children of an item by their text
	bool SortChildren(TreeItemHandle item);
	void SetBackColor(ColorRef color);

	int handle(int event) override;

protected:
	void OnLButtonDown(uint32 flags, Point point) override;
	void OnLButtonUp(uint32 flags, Point point) override;
	void OnLButtonDblClk(uint32 flags, Point point) override;
	void OnRButtonDown(uint32 flags, Point point) override;
	void OnRButtonUp(uint32 flags, Point point) override;
	void OnMouseMove(uint32 flags, Point point) override;
	bool OnKeyDown(uint32 key, uint32 repeatCount, uint32 flags) override;

	// Notifications; handlers can veto by overriding
	virtual void OnItemExpanded(TreeItemHandle item);

	// Item positions are only updated when Fl_Tree draws; queries in between need them recalculated
	void UpdateItemPositions() const;
	// Recalculates the tree layout and redraws, unless redrawing is disabled
	void RefreshLayout();
	void SendNotification(uint32 code, TreeItemHandle item);
	void FinishEdit(bool isAccepted);

	TreeItem *m_root = nullptr;
	TreeItem *m_selected = nullptr;
	TreeItem *m_dropTarget = nullptr;
	TreeItem *m_dragItem = nullptr;
	TreeItem *m_editedItem = nullptr;
	ImageList *m_images = nullptr;
	std::unique_ptr<Edit> m_editor;
	Point m_pressPoint;
	uint32 m_style = TreeStyleHasButtons | TreeStyleHasLines | TreeStyleLinesAtRoot;
	int m_dragButton = 0;
	bool m_isRedrawing = true;
	bool m_isDragCandidate = false;
	// The tree handles the mouse button itself instead of Fl_Tree (which only gets its scroll bars)
	bool m_isMouseCaptured = false;
};


// Info passed with the tree notifications through NotifyHeader::extra
struct TreeNotification
{
	TreeItemHandle item = nullptr;
	TreeItemHandle oldItem = nullptr;
	Point point;
	// Text of TreeEndLabelEdit; null if the editing was cancelled
	const mpt::ustring *text = nullptr;
};

}  // namespace ui


OPENMPT_NAMESPACE_END
