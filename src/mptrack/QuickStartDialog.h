// FLTK port of openmpt/mptrack/QuickStartDialog.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "CListCtrl.h"
#include "ResizableDialog.h"
#include "../common/mptPathString.h"

OPENMPT_NAMESPACE_BEGIN

class QuickStartDlg : public ResizableDialog
{
public:
	QuickStartDlg(const std::vector<mpt::PathString> &templates, const std::vector<mpt::PathString> &examples, Wnd *parent);
	~QuickStartDlg() override { DestroyWindow(); }

	void UpdateHeight();

protected:
	void DoDataExchange(DataExchange *pDX) override;
	bool OnInitDialog() override;
	void OnDPIChanged() override;
	bool PreTranslateMessage(int event) override;
	void OnOK() override;
	void OnCancel() override { DestroyWindow(); }

	void UpdateFileList(LParam highlight = LParam(-1), mpt::ustring filter = {});
	size_t GetItemIndex(int index) const { return static_cast<size_t>(m_list.GetItemData(index) & 0x00FF'FFFF); }
	int GetItemGroup(int index) const { return static_cast<int>(m_list.GetItemData(index) >> 24); }

	void OnNew();
	void OnOpen();
	void OnRemoveMRUItem();
	void OnRemoveAllMRUItems();
	void OnUpdateFilter();
	void OnOpenFile(NotifyHeader *, LResult *);
	void OnRightClickFile(NotifyHeader *, LResult *);
	void OnItemChanged(NotifyHeader *, LResult *);
	void OnDestroy();

	UI_DECLARE_MESSAGE_MAP()

	CListCtrlEx m_list;
	Edit m_find;
	Button m_newButton, m_openButton;
	ui::Bitmap m_bmpNew, m_bmpOpen;
	std::array<std::vector<mpt::PathString>, 3> m_paths;
	int m_prevDPI = 0;
	bool m_groupsEnabled = false;
};

OPENMPT_NAMESPACE_END
