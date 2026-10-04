// FLTK port of openmpt/mptrack/Globals.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"
#include "DialogBase.h"
#include "Settings.h"
#include "UpdateHints.h"

OPENMPT_NAMESPACE_BEGIN

#define ID_EDIT_MIXPASTE ID_EDIT_PASTE_SPECIAL

class CModControlView;
class CModDoc;
class CTrackerSoundFile;
struct DRAGONDROP;
struct Notification;

// Identifies the views that can be shown in the lower half of a document window
enum class ViewType : uint8
{
	None,
	General,
	Pattern,
	Sample,
	Instrument,
	Comments,
};


class CModControlBar: public ToolBar
{
public:
	bool Init(ImageList &icons, ImageList &disabledIcons);
	void UpdateStyle();
	void OnDPIChanged();
};


class CModControlDlg : public DialogBase
{
protected:
	CModDoc &m_modDoc;
	CTrackerSoundFile &m_sndFile;
	CModControlView &m_parent;
	WindowHandle m_hWndView = nullptr;
	WindowHandle m_lastFocusItem = nullptr;
	int m_nLockCount = 0;
	bool m_initialized = false;

public:
	CModControlDlg(CModControlView &parent, CModDoc &document);
	~CModControlDlg() override;
	
public:
	void SetViewWnd(WindowHandle hwndView) { m_hWndView = hwndView; }
	WindowHandle GetViewWnd() const { return m_hWndView; }
	LResult SendViewMessage(uint32 uMsg, LParam lParam = 0) const;
	bool PostViewMessage(uint32 uMsg, LParam lParam = 0) const;
	void SwitchToView() const;
	// Switch focus to lower view, but only if last interaction with the upper view was through the mouse
	void SwitchToViewIfMouse() const;
	void LockControls() { m_nLockCount++; }
	void UnlockControls();
	bool IsLocked() const { return (m_nLockCount > 0); }
	virtual Setting<int32> &GetSplitPosRef() = 0;

	void SaveLastFocusItem(WindowHandle hwnd);
	void ForgetLastFocusItem() { m_lastFocusItem = nullptr; }
	void RestoreLastFocusItem();

	void OnEditCut();
	void OnEditCopy();
	void OnEditPaste();
	void OnEditMixPaste();
	void OnEditMixPasteITStyle();
	void OnEditPasteFlood();
	void OnEditPushForwardPaste();
	void OnEditFind();
	void OnEditFindNext();
	void OnSwitchToView();

	void OnOK() override {}
	void OnCancel() override {}
	void OnDPIChanged() override { RecalcLayout(); }
	virtual void RecalcLayout() = 0;
	virtual void UpdateView(UpdateHint, HintObject *) = 0;
	virtual ViewType GetAssociatedViewType() { return ViewType::None; }
	virtual LResult OnModCtrlMsg(WParam wParam, LParam lParam);
	virtual void OnActivatePage(LParam) {}
	virtual void OnDeactivatePage() {}
	virtual bool OnDragonDrop(bool /*doDrop*/, const DRAGONDROP &/*dropInfo*/) { return false; }
	void OnSize(uint32 nType, int cx, int cy) override;
	LResult OnUnlockControls(WParam, LParam) { if (m_nLockCount > 0) m_nLockCount--; return 0; }
	LResult OnDragonDropping(WParam doDrop, LParam dropInfo) { return (dropInfo && OnDragonDrop(doDrop != 0, *reinterpret_cast<const DRAGONDROP*>(dropInfo))) ? 1 : 0; }
	UI_DECLARE_MESSAGE_MAP()
};


class CModTabCtrl: public TabCtrl
{
public:
	void OnDPIChanged();
};


class CModControlView: public View
{
public:
	// Note: Page IDs are serialized to module window settings!
	enum class Page
	{
		Unknown = -1,
		First = 0,
		Globals = First,
		Patterns,
		Samples,
		Instruments,
		Comments,
		NumPages
	};

protected:
	CModTabCtrl m_TabCtrl;
	std::array<CModControlDlg *, int(Page::NumPages)> m_Pages = {{}};
	Page m_nActiveDlg = Page::Unknown;
	int m_nInstrumentChanged = -1;
	WindowHandle m_hWndView = nullptr, m_hWndMDI = nullptr;

public:
	CModControlView();
	~CModControlView() override;
	CModDoc *GetDocument() const noexcept;
	void SampleChanged(SAMPLEINDEX smp);
	void InstrumentChanged(int nInstr = -1) { m_nInstrumentChanged = nInstr; }
	int GetInstrumentChange() const { return m_nInstrumentChanged; }
	void SetMDIParentFrame(WindowHandle hwnd) { m_hWndMDI = hwnd; }
	void ForceRefresh();
	CModControlDlg *GetCurrentControlDlg() const;
	// Height that shows all controls of the current page
	int CalcMinHeight() const;

protected:
	void RecalcLayout();
	void UpdateView(UpdateHint hint, HintObject *pHint = nullptr);
	bool SetActivePage(Page page = Page::Unknown, LParam lParam = -1);
public:
	Page GetActivePage() const { return m_nActiveDlg; }

protected:
	void OnInitialUpdate() override;
	void OnUpdate(View* pSender, LParam lHint, HintObject* pHint) override;

protected:
	void OnSetFocus(Wnd *pOldWnd) override;
	void OnSize(uint32 nType, int cx, int cy) override;
	void OnDestroy() override;
	void OnTabSelchange(NotifyHeader* pNMHDR, LResult* pResult);
	void OnEditCut();
	void OnEditCopy();
	void OnEditPaste();
	void OnEditMixPaste();
	void OnEditMixPasteITStyle();
	void OnEditFind();
	void OnEditFindNext();
	void OnSwitchToView();
	LResult OnActivateModView(WParam, LParam);
	LResult OnModCtrlMsg(WParam wParam, LParam lParam);
	UI_DECLARE_MESSAGE_MAP()
};

// Non-client button attributes
#define NCBTNS_MOUSEOVER		0x01
#define NCBTNS_CHECKED			0x02
#define NCBTNS_DISABLED			0x04
#define NCBTNS_PUSHED			0x08


class CModScrollView: public ScrollView
{
protected:
	WindowHandle m_hWndCtrl = nullptr;
	WindowHandle m_lastFocusItem = nullptr;
	int m_nScrollPosX = 0, m_nScrollPosY = 0;
	int m_nScrollPosXfine = 0, m_nScrollPosYfine = 0;
	int m_dpi = ui::kLogicalDpi;  // Cached DPI settings
	bool m_isRestoringFocus = false;

public:
	CModScrollView() = default;
	virtual ~CModScrollView() = default;

public:
	CModDoc *GetDocument() const noexcept;
	void SendCtrlCommand(int id) const;
	LResult SendCtrlMessage(uint32 uMsg, LParam lParam = 0) const;
	bool PostCtrlMessage(uint32 uMsg, LParam lParam = 0) const;
	void UpdateIndicator(const mpt::uchar * lpszText = nullptr);
	void UpdateIndicator(const mpt::ustring &text) { UpdateIndicator(text.c_str()); }
	// The view is the target of this window class
	bool PreTranslateMessage(int event) override;

public:
	void OnInitialUpdate() override;
	void OnUpdate(View *pSender, LParam lHint, HintObject *pHint) override;
	virtual void OnDPIChanged() { Invalidate(); }
	virtual void UpdateView(UpdateHint, HintObject *) {}
	virtual LResult OnModViewMsg(WParam wParam, LParam lParam);
	virtual bool OnDragonDrop(bool /*doDrop*/, const DRAGONDROP &/*dropInfo*/) { return false; }
	virtual LResult OnPlayerNotify(Notification *) { return 0; }

	CModControlDlg *GetControlDlg() { return static_cast<CModControlView *>(m_hWndCtrl)->GetCurrentControlDlg(); }

	void SaveLastFocusItem(WindowHandle hwnd);

protected:
	void OnDestroy() override;
	void OnSetFocus(Wnd *pOldWnd) override;
	LResult OnReceiveModViewMsg(WParam wParam, LParam lParam);
	bool OnMouseWheel(uint32 fFlags, int16 zDelta, Point point) override;
	LResult OnDragonDropping(WParam doDrop, LParam dropInfo) { return (dropInfo && OnDragonDrop(doDrop != 0, *reinterpret_cast<const DRAGONDROP*>(dropInfo))) ? 1 : 0; }
	LResult OnUpdatePosition(WParam, LParam);

	// Support for fractional mouse wheel movements
	bool OnScroll(uint32 nScrollCode, uint32 nPos, bool bDoScroll = true) override;
	bool OnScrollBy(Size sizeScroll, bool bDoScroll = true) override;
	void SetScrollSizes(Size sizeTotal, Size sizePage = {}, Size sizeLine = {});
	int SetScrollPos(int nBar, int nPos, bool bRedraw = true);

	UI_DECLARE_MESSAGE_MAP()
};


OPENMPT_NAMESPACE_END
