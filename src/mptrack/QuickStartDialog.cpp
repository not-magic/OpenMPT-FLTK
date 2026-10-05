/*
 * QuickStartDialog.cpp
 * --------------------
 * Purpose: Dialog to show inside the MDI client area when no modules are loaded.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/QuickStartDialog.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "QuickStartDialog.h"
#include "FolderScanner.h"
#include "Image.h"
#include "ImageLists.h"
#include "Mainfrm.h"
#include "Mptrack.h"
#include "MPTrackUtil.h"
#include "resource.h"
#include "TrackerSettings.h"


OPENMPT_NAMESPACE_BEGIN

UI_MESSAGE_MAP_BEGIN(QuickStartDlg, ResizableDialog)

	UI_COMMAND(IDC_BUTTON1,   &QuickStartDlg::OnNew)
	UI_COMMAND(IDC_BUTTON2,   &QuickStartDlg::OnOpen)
	UI_COMMAND(ID_REMOVE,     &QuickStartDlg::OnRemoveMRUItem)
	UI_COMMAND(ID_REMOVE_ALL, &QuickStartDlg::OnRemoveAllMRUItems)

	UI_NOTIFY(ui::EditChange, IDC_EDIT1, &QuickStartDlg::OnUpdateFilter)
	
	UI_NOTIFY(ui::ListDblClick, IDC_LIST1, &QuickStartDlg::OnOpenFile)
	UI_NOTIFY(ui::ListRClick, IDC_LIST1, &QuickStartDlg::OnRightClickFile)
	UI_NOTIFY(ui::ListItemChanged, IDC_LIST1, &QuickStartDlg::OnItemChanged)
UI_MESSAGE_MAP_END()


void QuickStartDlg::DoDataExchange(DataExchange *pDX)
{
	ResizableDialog::DoDataExchange(pDX);
	pDX->BindControl(IDC_BUTTON1, m_newButton);
	pDX->BindControl(IDC_BUTTON2, m_openButton);
	pDX->BindControl(IDC_LIST1, m_list);
	pDX->BindControl(IDC_EDIT1, m_find);
}


QuickStartDlg::QuickStartDlg(const std::vector<mpt::PathString> &templates, const std::vector<mpt::PathString> &examples, Wnd *parent)
{
	m_newButton.SetAccessibleText(UL_("New Module"));
	m_openButton.SetAccessibleText(UL_("Open Module"));
	Create(IDD_QUICKSTART, parent, true);

	m_groupsEnabled = m_list.EnableGroupView();
	m_list.SetRedraw(false);
	m_list.SetExtendedStyle(m_list.GetExtendedStyle() | ui::ListStyleFullRowSelect);
	m_list.InsertColumn(0, UL_("File"), ui::ListColumnLeft);
	m_list.InsertColumn(1, UL_("Location"), ui::ListColumnLeft);
	
	const std::pair<const std::vector<mpt::PathString> &, const mpt::uchar *> PathGroups[] =
	{
		{TrackerSettings::Instance().mruFiles, UL_("Recent Files")},
		{templates, UL_("Templates")},
		{examples, UL_("Example Modules")},
	};
	static_assert(mpt::array_size<decltype(PathGroups)>::size == mpt::array_size<decltype(m_paths)>::size);
	for(size_t groupId = 0; groupId < std::size(PathGroups); groupId++)
	{
		m_paths[groupId] = PathGroups[groupId].first;
	}
	UpdateFileList();
	m_list.SetColumnWidth(0, LVSCW_AUTOSIZE);
	m_list.SetColumnWidth(1, LVSCW_AUTOSIZE_USEHEADER);
	m_list.SetRedraw(true);
	m_list.Invalidate(false);
	OnItemChanged(nullptr, nullptr);
	UpdateHeight();
}


bool QuickStartDlg::OnInitDialog()
{
	ResizableDialog::OnInitDialog();
	OnDPIChanged();
	return true;
}


void QuickStartDlg::OnDPIChanged()
{
	const double scaling = GetDPI() / 96.0;

	m_list.SetImageList(&CMainFrame::GetMainFrame()->m_PatternIcons);
	m_list.SetColumnWidth(0, ui::LVSCW_AUTOSIZE);
	m_list.SetColumnWidth(1, ui::LVSCW_AUTOSIZE_USEHEADER);

	std::unique_ptr<RawImage> bitmapNew, bitmapOpen;
	try
	{
		bitmapNew = LoadPixelImage(GetResource(IDB_NEW_BIG), scaling);
		bitmapOpen = LoadPixelImage(GetResource(IDB_OPEN_BIG), scaling);
	} catch(...)
	{
		return;
	}
	m_bmpNew = ToBitmap(*bitmapNew);
	m_bmpOpen = ToBitmap(*bitmapOpen);
	m_newButton.SetBitmap(&m_bmpNew);
	m_openButton.SetBitmap(&m_bmpOpen);
}


bool QuickStartDlg::PreTranslateMessage(int event)
{
	// Use up/down keys to navigate in list, even if search field is focussed. This also skips the group headers during navigation
	const uint32 key = (event == FL_KEYBOARD) ? ui::KeyFromEvent() : 0;
	if((key == ui::Key_UP || key == ui::Key_DOWN) && Wnd::GetFocus() == &m_find)
	{
		const int curSel = m_list.GetSelectionMark();
		const int selItem = std::clamp(curSel + (key == ui::Key_UP ? -1 : 1), 0, m_list.GetItemCount() - 1);
		if(curSel != selItem)
		{
			if(curSel != -1)
				m_list.SetItemState(curSel, 0, ui::ListItemSelected);
			m_list.SetItemState(selItem, ui::ListItemSelected, ui::ListItemSelected);
			m_list.SetSelectionMark(selItem);
		}
		return true;
	}

	return ResizableDialog::PreTranslateMessage(event);
}


void QuickStartDlg::UpdateHeight()
{
	// Try to make the view tall enough to view the entire list contents, but only up to 90% of the screen
	Rect listRect, viewRect, itemRect;
	m_list.GetClientRect(listRect);
	m_list.GetViewRect(viewRect);
	m_list.GetItemRect(0, itemRect);

	Rect windowRect;
	GetClientRect(windowRect);
	if(viewRect.bottom > listRect.bottom)
		windowRect.bottom += viewRect.bottom - listRect.bottom;
	const int maxHeight = std::max(ui::ScalePixels(154, this), Util::muldiv(ui::GetScreenSize().cy, 9, 10));
	LimitMax(windowRect.bottom, maxHeight);
	SetWindowPos(nullptr, 0, 0, windowRect.Width(), windowRect.Height(), ui::PosNoZOrder | ui::PosNoActivate | ui::PosNoMove);
}


void QuickStartDlg::OnNew()
{
	theApp.NewDocument();
}


void QuickStartDlg::OnOpen()
{
	CMainFrame::GetMainFrame()->SendCommand(ID_FILE_OPEN);
}


void QuickStartDlg::OnOK()
{
	OnOpenFile(nullptr, nullptr);
}


void QuickStartDlg::OnRemoveMRUItem()
{
	auto &mruFiles = TrackerSettings::Instance().mruFiles;
	int i = -1;
	while((i = m_list.GetNextItem(i, ui::ListNextSelected | ui::ListNextAll)) != -1)
	{
		if(GetItemGroup(i) != 0)
			continue;
		auto &path = m_paths[0][GetItemIndex(i)];
		if(auto it = std::find(mruFiles.begin(), mruFiles.end(), path); it != mruFiles.end())
			mruFiles.erase(it);
		path = {};
		m_list.DeleteItem(i);
		i--;
	}
	CMainFrame::GetMainFrame()->UpdateMRUList();
}


void QuickStartDlg::OnRemoveAllMRUItems()
{
	TrackerSettings::Instance().mruFiles.clear();
	m_paths[0].clear();
	for(int i = m_list.GetItemCount() - 1; i >= 0; i--)
	{
		if(GetItemGroup(i) == 0)
			m_list.DeleteItem(i);
	}
	CMainFrame::GetMainFrame()->UpdateMRUList();
}


void QuickStartDlg::OnUpdateFilter()
{
	m_list.SetRedraw(false);
	LParam highlight = LParam(-1);
	if(int sel = m_list.GetSelectionMark(); sel != -1)
		highlight = m_list.GetItemData(sel);
	m_list.DeleteAllItems();
	mpt::ustring filter;
	m_find.GetWindowText(filter);
	UpdateFileList(highlight, filter);
	m_list.SetRedraw(true);
}


void QuickStartDlg::UpdateFileList(LParam highlight, mpt::ustring filter)
{
	const bool applyFilter = !filter.empty();
	if(applyFilter)
		filter = UL_("*") + filter + UL_("*");

	bool highlightFound = false;
	int itemId = -1;
	for(size_t groupId = 0; groupId < m_paths.size(); groupId++)
	{
		for(size_t i = 0; i < m_paths[groupId].size(); i++)
		{
			if(m_paths[groupId][i].empty() || (applyFilter && !MatchFileSpec(m_paths[groupId][i].ToUnicode(), filter)))
				continue;
			const auto filename = m_paths[groupId][i].GetFilename().ToUnicode();
			const LParam itemData = static_cast<LParam>(i | (groupId << 24));
			++itemId;
			m_list.InsertItem(itemId, filename, TIMAGE_MODULE_FILE, static_cast<uintptr_t>(itemData));
			m_list.SetItemText(itemId, 1, m_paths[groupId][i].GetDirectoryWithDrive().ToUnicode());
			if(itemData == highlight)
			{
				m_list.SetItemState(itemId, ui::ListItemSelected, ui::ListItemSelected);
				m_list.SetSelectionMark(itemId);
				highlightFound = true;
			}
		}
	}
	if(!highlightFound)
	{
		m_list.SetItemState(0, ui::ListItemSelected, ui::ListItemSelected);
		m_list.SetSelectionMark(0);
	}
}


void QuickStartDlg::OnOpenFile(NotifyHeader *, LResult *)
{
	struct OpenItem
	{
		mpt::PathString path;  // Must create copy because dialog will get destroyed after successfully loading the first file
		int item = 0;
		int group = 0;
	};
	std::vector<OpenItem> files;
	int i = -1;
	while((i = m_list.GetNextItem(i, ui::ListNextSelected | ui::ListNextAll)) != -1)
	{
		const int group = GetItemGroup(i);
		const size_t index = GetItemIndex(i);
		files.push_back({std::move(m_paths[group][index]), i, group});
	}
	bool success = false;
	for(const auto &item : files)
	{
		if(item.group != 1)
			success |= (theApp.OpenDocumentFile(item.path) != nullptr);
		else
			success |= (theApp.OpenTemplateFile(item.path) != nullptr);
	}

	if(!success)
	{
		// If at least one item managed to load, the dialog will now be destroyed, and there are no items to delete from the list
		for(auto it = files.rbegin(); it != files.rend(); it++)
		{
			m_list.DeleteItem(it->item);
		}
	}
}


void QuickStartDlg::OnRightClickFile(NotifyHeader *nmhdr, LResult *)
{
	const ui::ListClickInfo *item = static_cast<const ui::ListClickInfo *>(nmhdr->extra);
	if(item->item < 0)
		return;
	Point point = item->point;
	m_list.ClientToScreen(&point);
	// Can only remove MRU items
	if(GetItemGroup(item->item) != 0)
		return;
	Menu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(ui::MenuItemString, ID_REMOVE, UL_("&Remove from Recent File List"));
	menu.AppendMenu(ui::MenuItemString, ID_REMOVE_ALL, UL_("&Clear Recent File List"));
	menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
	menu.DestroyMenu();
}


void QuickStartDlg::OnItemChanged(NotifyHeader *, LResult *)
{
	GetDlgItem(IDOK)->EnableWindow(m_list.GetSelectedCount() != 0 ? true : false);
}


void QuickStartDlg::OnDestroy()
{
	ResizableDialog::OnDestroy();
}

OPENMPT_NAMESPACE_END
