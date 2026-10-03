// FLTK port of openmpt/mptrack/Globals.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "Globals.h"
#include "Childfrm.h"
#include "Ctrl_com.h"
#include "Ctrl_gen.h"
#include "Ctrl_ins.h"
#include "Ctrl_pat.h"
#include "Ctrl_smp.h"
#include "ImageLists.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "WindowMessages.h"
#include "../soundlib/mod_specifications.h"

OPENMPT_NAMESPACE_BEGIN


static void RestoreLastFocusItem(Wnd &parent, Wnd *&lastFocusItem)
{
	Fl_Group *group = parent.GetWidget()->as_group();
	if(lastFocusItem && lastFocusItem->IsWindowEnabled() && lastFocusItem->IsWindowVisible())
	{
		lastFocusItem->SetFocus();
		return;
	}
	if(group)
	{
		for(int i = 0; i < group->children(); ++i)
		{
			Fl_Widget *child = group->child(i);
			if(child->visible() && child->active() && child->visible_focus())
			{
				if(Wnd *wnd = dynamic_cast<Wnd *>(child))
				{
					lastFocusItem = wnd;
					wnd->SetFocus();
					return;
				}
			}
		}
	}
	parent.SetFocus();
}


/////////////////////////////////////////////////////////////////////////////
// CModControlDlg

UI_MESSAGE_MAP_BEGIN(CModControlDlg, DialogBase)
	UI_MESSAGE(MSG_MOD_UNLOCKCONTROLS, &CModControlDlg::OnUnlockControls)
	UI_MESSAGE(MSG_MOD_DRAGONDROPPING, &CModControlDlg::OnDragonDropping)
UI_MESSAGE_MAP_END()


CModControlDlg::CModControlDlg(CModControlView &parent, CModDoc &document) : m_modDoc(document), m_sndFile(document.GetSoundFile()), m_parent(parent)
{
}


CModControlDlg::~CModControlDlg()
{
}


void CModControlDlg::OnSize(uint32 nType, int cx, int cy)
{
	DialogBase::OnSize(nType, cx, cy);
	if((cx > 0) && (cy > 0))
	{
		RecalcLayout();
	}
}


void CModControlDlg::SaveLastFocusItem(WindowHandle hwnd)
{
	if(hwnd)
		m_lastFocusItem = hwnd;
}


void CModControlDlg::RestoreLastFocusItem()
{
	OPENMPT_NAMESPACE::RestoreLastFocusItem(*this, m_lastFocusItem);
}


void CModControlDlg::OnEditCut() { if(m_hWndView) m_hWndView->SendCommand(ID_EDIT_CUT); }
void CModControlDlg::OnEditCopy() { if(m_hWndView) m_hWndView->SendCommand(ID_EDIT_COPY); }
void CModControlDlg::OnEditPaste() { if(m_hWndView) m_hWndView->SendCommand(ID_EDIT_PASTE); }
void CModControlDlg::OnEditMixPaste() { if(m_hWndView) m_hWndView->SendCommand(ID_EDIT_MIXPASTE); }
void CModControlDlg::OnEditMixPasteITStyle() { if(m_hWndView) m_hWndView->SendCommand(ID_EDIT_MIXPASTE_ITSTYLE); }
void CModControlDlg::OnEditPasteFlood() { if(m_hWndView) m_hWndView->SendCommand(ID_EDIT_PASTEFLOOD); }
void CModControlDlg::OnEditPushForwardPaste() { if(m_hWndView) m_hWndView->SendCommand(ID_EDIT_PUSHFORWARDPASTE); }
void CModControlDlg::OnEditFind() { if(m_hWndView) m_hWndView->SendCommand(ID_EDIT_FIND); }
void CModControlDlg::OnEditFindNext() { if(m_hWndView) m_hWndView->SendCommand(ID_EDIT_FINDNEXT); }
void CModControlDlg::OnSwitchToView() { if(m_hWndView) m_hWndView->PostMessage(MSG_MOD_VIEWMSG, VIEWMSG_SETFOCUS, 0); }


LResult CModControlDlg::OnModCtrlMsg(WParam wParam, LParam lParam)
{
	switch(wParam)
	{
	case CTRLMSG_SETVIEWWND:
		m_hWndView = reinterpret_cast<WindowHandle>(lParam);
		break;

	case CTRLMSG_ACTIVATEPAGE:
		OnActivatePage(lParam);
		break;

	case CTRLMSG_DEACTIVATEPAGE:
		OnDeactivatePage();
		break;

	case CTRLMSG_SETFOCUS:
		if(ChildFrameBase *frame = m_parent.GetParentFrame())
			frame->SetActiveView(&m_parent);
		RestoreLastFocusItem();
		break;
	}
	return 0;
}


LResult CModControlDlg::SendViewMessage(uint32 uMsg, LParam lParam) const
{
	if(m_hWndView)
		return m_hWndView->SendMessage(MSG_MOD_VIEWMSG, uMsg, lParam);
	return 0;
}


bool CModControlDlg::PostViewMessage(uint32 uMsg, LParam lParam) const
{
	if(m_hWndView)
	{
		m_hWndView->PostMessage(MSG_MOD_VIEWMSG, uMsg, lParam);
		return true;
	}
	return false;
}

void CModControlDlg::SwitchToView() const
{
	SendViewMessage(VIEWMSG_SETACTIVE);
}

void CModControlDlg::SwitchToViewIfMouse() const
{
	if(m_lastInputDevice == InputDevice::Mouse)
		SendViewMessage(VIEWMSG_SETACTIVE);
}

void CModControlDlg::UnlockControls()
{
	PostMessage(MSG_MOD_UNLOCKCONTROLS);
}


/////////////////////////////////////////////////////////////////////////////
// CModControlBar

bool CModControlBar::Init(ImageList &icons, ImageList &disabledIcons)
{
	OnDPIChanged();

	// Add bitmaps
	SetImageList(&icons);
	SetDisabledImageList(&disabledIcons);
	UpdateStyle();
	return true;
}


void CModControlBar::UpdateStyle()
{
	SetFlat(TrackerSettings::Instance().patternSetup & PatternSetup::FlatToolbarButtons);
}


void CModControlBar::OnDPIChanged()
{
	const int imgSize = ui::ScalePixels(16, this), btnSizeX = ui::ScalePixels(26, this), btnSizeY = ui::ScalePixels(24, this);
	SetBitmapSize(Size(imgSize, imgSize));
	SetButtonSize(Size(btnSizeX, btnSizeY));
}


/////////////////////////////////////////////////////////////////////////////
// CModTabCtrl

void CModTabCtrl::OnDPIChanged()
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	SetImageList(&pMainFrm->m_MiscIcons);
}


/////////////////////////////////////////////////////////////////////////////////
// CModControlView


UI_MESSAGE_MAP_BEGIN(CModControlView, View)
	UI_NOTIFY(ui::TabSelChange, IDC_TABCTRL1, &CModControlView::OnTabSelchange)
	UI_MESSAGE(MSG_MOD_ACTIVATEVIEW,        &CModControlView::OnActivateModView)
	UI_MESSAGE(MSG_MOD_CTRLMSG,             &CModControlView::OnModCtrlMsg)
	UI_COMMAND(ID_EDIT_CUT,                &CModControlView::OnEditCut)
	UI_COMMAND(ID_EDIT_COPY,               &CModControlView::OnEditCopy)
	UI_COMMAND(ID_EDIT_PASTE,              &CModControlView::OnEditPaste)
	UI_COMMAND(ID_EDIT_MIXPASTE,           &CModControlView::OnEditMixPaste)
	UI_COMMAND(ID_EDIT_MIXPASTE_ITSTYLE,   &CModControlView::OnEditMixPasteITStyle)
	UI_COMMAND(ID_EDIT_FIND,               &CModControlView::OnEditFind)
	UI_COMMAND(ID_EDIT_FINDNEXT,           &CModControlView::OnEditFindNext)
UI_MESSAGE_MAP_END()


CModControlView::CModControlView()
{
	m_TabCtrl.SetDlgCtrlID(IDC_TABCTRL1);
	add(m_TabCtrl.GetWidget());
}


CModControlView::~CModControlView()
{
	for(auto &pDlg : m_Pages)
	{
		if(pDlg)
		{
			remove(pDlg->GetWidget());
			delete pDlg;
			pDlg = nullptr;
		}
	}
}


CModDoc *CModControlView::GetDocument() const noexcept { return static_cast<CModDoc *>(View::GetDocument()); }

void CModControlView::OnInitialUpdate() // called first time after construct
{
	View::OnInitialUpdate();

	CChildFrame *pParentFrame = static_cast<CChildFrame *>(GetParentFrame());
	if (pParentFrame) m_hWndView = pParentFrame->GetHwndView();
	m_TabCtrl.OnDPIChanged();
	UpdateView(UpdateHint().ModType());
	SetActivePage(Page::First);
}


void CModControlView::OnSetFocus(Wnd *pOldWnd)
{
	if(CModControlDlg *activeDlg = GetCurrentControlDlg())
		activeDlg->RestoreLastFocusItem();
	View::OnSetFocus(pOldWnd);
}


void CModControlView::OnSize(uint32 nType, int cx, int cy)
{
	View::OnSize(nType, cx, cy);
	if((cx > 0) && (cy > 0))
	{
		RecalcLayout();
	}
}


void CModControlView::RecalcLayout()
{
	const Rect rcClient = GetClientRect();
	MoveChild(m_TabCtrl, rcClient);
	if(Wnd *pDlg = GetCurrentControlDlg())
	{
		Rect rect = rcClient;
		m_TabCtrl.AdjustRect(false, &rect);
		MoveChild(*pDlg, rect);
	}
}


int CModControlView::CalcMinHeight() const
{
	const CModControlDlg *dlg = GetCurrentControlDlg();
	if(dlg == nullptr)
		return 0;
	const Rect clientRect = GetClientRect();
	Rect pageRect = clientRect;
	m_TabCtrl.AdjustRect(false, &pageRect);
	return dlg->CalcTemplateHeight() + clientRect.Height() - pageRect.Height();
}


void CModControlView::OnUpdate(View *, LParam lHint, HintObject *pHint)
{
	UpdateView(UpdateHint::FromLPARAM(lHint), pHint);
}


void CModControlView::ForceRefresh()
{
	SetActivePage(GetActivePage());
}


CModControlDlg *CModControlView::GetCurrentControlDlg() const
{
	if(m_nActiveDlg >= Page::First && m_nActiveDlg < Page::NumPages)
		return m_Pages[static_cast<size_t>(m_nActiveDlg)];
	else
		return nullptr;
}


bool CModControlView::SetActivePage(Page page, LParam lParam)
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	CModControlDlg *pDlg = nullptr;

	if(page == Page::Unknown)
		page = static_cast<Page>(m_TabCtrl.GetCurSel());

	const uint32 nID = static_cast<uint32>(m_TabCtrl.GetItemData(static_cast<int>(page)));
	if(nID == 0)
		return false;

	switch(nID)
	{
		case IDD_CONTROL_COMMENTS:
			page = Page::Comments;
			break;
		case IDD_CONTROL_GLOBALS:
			page = Page::Globals;
			break;
		case IDD_CONTROL_PATTERNS:
			page = Page::Patterns;
			break;
		case IDD_CONTROL_SAMPLES:
			page = Page::Samples;
			break;
		case IDD_CONTROL_INSTRUMENTS:
			page = Page::Instruments;
			break;
		default:
			return false;
	}

	if(page < Page::First || page >= Page::NumPages || !pMainFrm)
		return false;

	CModControlDlg *oldActiveDlg = GetCurrentControlDlg();
	if(oldActiveDlg)
		oldActiveDlg->GetSplitPosRef() = static_cast<CChildFrame *>(GetParentFrame())->GetSplitterHeight();

	if(page == m_nActiveDlg)
	{
		pDlg = oldActiveDlg;
		PostMessage(MSG_MOD_CTRLMSG, CTRLMSG_ACTIVATEPAGE, lParam);
		return true;
	}
	if(oldActiveDlg)
	{
		OnModCtrlMsg(CTRLMSG_DEACTIVATEPAGE, 0);
		oldActiveDlg->ShowWindow(false);
	}
	if(m_Pages[static_cast<size_t>(page)]) // Ctrl window already created?
	{
		m_nActiveDlg = page;
		pDlg = m_Pages[static_cast<size_t>(page)];
		pDlg->ForgetLastFocusItem();
	} else // Ctrl window is not created yet - creating one.
	{
		m_nActiveDlg = Page::Unknown;
		MPT_ASSERT_ALWAYS(GetDocument() != nullptr);
		switch(nID)
		{
		case IDD_CONTROL_COMMENTS:
			pDlg = new CCtrlComments(*this, *GetDocument());
			break;
		case IDD_CONTROL_GLOBALS:
			pDlg = new CCtrlGeneral(*this, *GetDocument());
			break;
		case IDD_CONTROL_PATTERNS:
			pDlg = new CCtrlPatterns(*this, *GetDocument());
			break;
		case IDD_CONTROL_SAMPLES:
			pDlg = new CCtrlSamples(*this, *GetDocument());
			break;
		case IDD_CONTROL_INSTRUMENTS:
			pDlg = new CCtrlInstruments(*this, *GetDocument());
			break;
		default:
			return false;
		}
		pDlg->SetViewWnd(m_hWndView);
		bool bStatus = pDlg->CreateChild(nID, *this);
		if(bStatus == 0) // Creation failed.
		{
			delete pDlg;
			return false;
		}
		m_nActiveDlg = page;
		m_Pages[static_cast<size_t>(page)] = pDlg;
	}
	RecalcLayout();
	pMainFrm->SetUserText(UL_(""));
	pMainFrm->SetInfoText(UL_(""));
	pMainFrm->SetXInfoText(UL_(""));
	pDlg->ShowWindow(true);
	static_cast<CChildFrame *>(GetParentFrame())->SetSplitterHeight(pDlg->GetSplitPosRef());
	if(m_hWndMDI)
		m_hWndMDI->PostMessage(MSG_MOD_CHANGEVIEWCLASS, (WParam)lParam, (LParam)pDlg);
	return true;
}


void CModControlView::OnDestroy()
{
	m_nActiveDlg = Page::Unknown;
	for(auto &pDlg : m_Pages)
	{
		if(pDlg)
		{
			remove(pDlg->GetWidget());
			delete pDlg;
			pDlg = nullptr;
		}
	}
	View::OnDestroy();
}


void CModControlView::UpdateView(UpdateHint lHint, HintObject *pObject)
{
	Wnd *pActiveDlg = nullptr;
	CModDoc *pDoc = GetDocument();
	if(!pDoc)
		return;
	// Module type changed: update tabs
	if (lHint.GetType()[HINT_MODTYPE])
	{
		uint32 nCount = 4;
		uint32 mask = 1 | 2 | 4 | 16;

		if(pDoc->GetSoundFile().GetModSpecifications().instrumentsMax > 0 || pDoc->GetNumInstruments() > 0)
		{
			mask |= 8;
			nCount++;
		}
		if (nCount != (uint32)m_TabCtrl.GetItemCount())
		{
			uint32 count = 0;
			pActiveDlg = GetCurrentControlDlg();
			if(pActiveDlg)
				pActiveDlg->ShowWindow(false);
			m_TabCtrl.DeleteAllItems();
			if (mask & 1) m_TabCtrl.InsertItem(count++, UL_("General"), IDD_CONTROL_GLOBALS, IMAGE_GENERAL);
			if (mask & 2) m_TabCtrl.InsertItem(count++, UL_("Patterns"), IDD_CONTROL_PATTERNS, IMAGE_PATTERNS);
			if (mask & 4) m_TabCtrl.InsertItem(count++, UL_("Samples"), IDD_CONTROL_SAMPLES, IMAGE_SAMPLES);
			if (mask & 8) m_TabCtrl.InsertItem(count++, UL_("Instruments"), IDD_CONTROL_INSTRUMENTS, IMAGE_INSTRUMENTS);
			if (mask & 16) m_TabCtrl.InsertItem(count++, UL_("Comments"), IDD_CONTROL_COMMENTS, IMAGE_COMMENTS);
		}
	}
	// Update child dialogs
	for (uint32 nIndex=0; nIndex<int(Page::NumPages); nIndex++)
	{
		CModControlDlg *pDlg = m_Pages[nIndex];
		if ((pDlg) && (pObject != pDlg)) pDlg->UpdateView(UpdateHint(lHint), pObject);
	}
	// Restore the displayed child dialog
	if (pActiveDlg) pActiveDlg->ShowWindow(true);
}


void CModControlView::OnTabSelchange(NotifyHeader*, LResult* pResult)
{
	SetActivePage(static_cast<Page>(m_TabCtrl.GetCurSel()));
	if(pResult)
		*pResult = 0;
}


LResult CModControlView::OnActivateModView(WParam nIndex, LParam lParam)
{
	if(Wnd::GetFocus() == nullptr)
	{
		// If we are in a dialog (e.g. Amplify Sample), do not allow to switch to a different tab. Otherwise, watch the tracker crash!
		return 0;
	}

	if (static_cast<Page>(nIndex) < Page::NumPages)
	{
		m_TabCtrl.SetCurSel(static_cast<int>(nIndex));
		SetActivePage(static_cast<Page>(nIndex), lParam);
	} else
	// Might be a dialog id IDD_XXXX
	{
		int nItems = m_TabCtrl.GetItemCount();
		for (int i = 0; i < nItems; i++)
		{
			if (static_cast<WParam>(m_TabCtrl.GetItemData(i)) == nIndex)
			{
				m_TabCtrl.SetCurSel(i);
				SetActivePage(static_cast<Page>(i), lParam);
				break;
			}
		}
	}
	return 0;
}

void CModControlView::OnEditCut() { if(m_hWndView) m_hWndView->SendCommand(ID_EDIT_CUT); }
void CModControlView::OnEditCopy() { if(m_hWndView) m_hWndView->SendCommand(ID_EDIT_COPY); }
void CModControlView::OnEditPaste() { if(m_hWndView) m_hWndView->SendCommand(ID_EDIT_PASTE); }
void CModControlView::OnEditMixPaste() { if(m_hWndView) m_hWndView->SendCommand(ID_EDIT_MIXPASTE); }
void CModControlView::OnEditMixPasteITStyle() { if(m_hWndView) m_hWndView->SendCommand(ID_EDIT_MIXPASTE_ITSTYLE); }
void CModControlView::OnEditFind() { if(m_hWndView) m_hWndView->SendCommand(ID_EDIT_FIND); }
void CModControlView::OnEditFindNext() { if(m_hWndView) m_hWndView->SendCommand(ID_EDIT_FINDNEXT); }
void CModControlView::OnSwitchToView() { if(m_hWndView) m_hWndView->PostMessage(MSG_MOD_VIEWMSG, VIEWMSG_SETFOCUS, 0); }


LResult CModControlView::OnModCtrlMsg(WParam wParam, LParam lParam)
{
	CModControlDlg *pActiveDlg = GetCurrentControlDlg();
	if(!pActiveDlg)
		return 0;
	switch(wParam)
	{
	case CTRLMSG_SETVIEWWND:
		m_hWndView = reinterpret_cast<WindowHandle>(lParam);
		for(CModControlDlg *dlg : m_Pages)
		{
			if(dlg)
				dlg->SetViewWnd(m_hWndView);
		}
		break;
	}
	return pActiveDlg->OnModCtrlMsg(wParam, lParam);
}


void CModControlView::SampleChanged(SAMPLEINDEX smp)
{
	const CModDoc *modDoc = GetDocument();
	if(modDoc && modDoc->GetNumInstruments())
	{
		INSTRUMENTINDEX k = static_cast<INSTRUMENTINDEX>(GetInstrumentChange());
		if(!modDoc->IsChildSample(k, smp))
		{
			INSTRUMENTINDEX nins = modDoc->FindSampleParent(smp);
			if(nins != INSTRUMENTINDEX_INVALID)
			{
				InstrumentChanged(nins);
			}
		}
	} else
	{
		InstrumentChanged(smp);
	}
}


//////////////////////////////////////////////////////////////////
// CModScrollView

UI_MESSAGE_MAP_BEGIN(CModScrollView, ScrollView)
	UI_MESSAGE(MSG_MOD_VIEWMSG,            &CModScrollView::OnReceiveModViewMsg)
	UI_MESSAGE(MSG_MOD_DRAGONDROPPING,     &CModScrollView::OnDragonDropping)
	UI_MESSAGE(MSG_MOD_UPDATEPOSITION,     &CModScrollView::OnUpdatePosition)
UI_MESSAGE_MAP_END()

CModDoc *CModScrollView::GetDocument() const noexcept { return static_cast<CModDoc *>(View::GetDocument()); }

LResult CModScrollView::SendCtrlMessage(uint32 uMsg, LParam lParam) const
{
	if (m_hWndCtrl)	return m_hWndCtrl->SendMessage(MSG_MOD_CTRLMSG, uMsg, lParam);
	return 0;
}


void CModScrollView::SendCtrlCommand(int id) const
{
	m_hWndCtrl->SendCommand(id);
}


bool CModScrollView::PostCtrlMessage(uint32 uMsg, LParam lParam) const
{
	if (m_hWndCtrl)
	{
		m_hWndCtrl->PostMessage(MSG_MOD_CTRLMSG, uMsg, lParam);
		return true;
	}
	return false;
}


LResult CModScrollView::OnReceiveModViewMsg(WParam wParam, LParam lParam)
{
	return OnModViewMsg(wParam, lParam);
}


void CModScrollView::SaveLastFocusItem(WindowHandle hwnd)
{
	if(hwnd)
		m_lastFocusItem = hwnd;
}


void CModScrollView::OnSetFocus(Wnd *pOldWnd)
{
	RestoreLastFocusItem(*this, m_lastFocusItem);
	ScrollView::OnSetFocus(pOldWnd);
}


void CModScrollView::OnUpdate(View *pView, LParam lHint, HintObject *pHint)
{
	if (pView != this) UpdateView(UpdateHint::FromLPARAM(lHint), pHint);
}


LResult CModScrollView::OnModViewMsg(WParam wParam, LParam lParam)
{
	switch(wParam)
	{
	case VIEWMSG_SETCTRLWND:
		m_hWndCtrl = (WindowHandle)lParam;
		break;

	case VIEWMSG_SETFOCUS:
	case VIEWMSG_SETACTIVE:
		if(ChildFrameBase *frame = GetParentFrame())
			frame->SetActiveView(this);
		RestoreLastFocusItem(*this, m_lastFocusItem);
		break;
	}
	return 0;
}


void CModScrollView::OnInitialUpdate()
{
	m_dpi = ui::GetDpiForWindow(this);
}


bool CModScrollView::PreTranslateMessage(int event)
{
	// We handle keypresses before the toolkit has a chance to handle them (for alt etc..)
	if(event == FL_KEYBOARD || event == FL_KEYUP)
	{
		CInputHandler *ih = CMainFrame::GetInputHandler();
		const uint32 flags = (event == FL_KEYUP) ? ui::KeyFlagRelease : 0;
		const auto keyEvent = ih->Translate(ui::KeyFromEvent(), 1, flags);
		if(ih->KeyEvent(kCtxAllContexts, keyEvent) != kcNull)
			return true;  // Mapped to a command, no need to pass the key on.
	}

	return ScrollView::PreTranslateMessage(event);
}


void CModScrollView::UpdateIndicator(const mpt::uchar * lpszText)
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if (pMainFrm) pMainFrm->SetUserText((lpszText) ? lpszText : UL_(""));
}


// Accumulate mouse wheel steps for laptop precision touchpads that emit wheel events < WHEEL_DELTA
static constexpr int kWheelDelta = 120;

static short RoundMouseWheelToWholeStep(int value, int &accum)
{
	accum += value;
	value = mpt::align_down(accum, kWheelDelta);
	accum -= value;
	return mpt::saturate_cast<short>(value);
}


bool CModScrollView::OnMouseWheel(uint32 fFlags, int16 zDelta, Point)
{
	// we don't handle anything but scrolling just now
	if(fFlags & (ui::MouseShift | ui::MouseControl))
		return false;

	const short steps = RoundMouseWheelToWholeStep(zDelta, m_nScrollPosYfine);
	if(steps == 0)
		return true;
	return OnScrollBy(Size(0, -steps * m_line.cy / kWheelDelta * 3), true);
}


void CModScrollView::OnDestroy()
{
	CModDoc *pModDoc = GetDocument();
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if ((pMainFrm) && (pModDoc))
	{
		if (pMainFrm->GetFollowSong(pModDoc) == this)
		{
			pModDoc->SetNotifications(Notification::Default);
			pModDoc->SetFollowWnd(NULL);
		}
		if (pMainFrm->GetMidiRecordWnd() == this)
		{
			pMainFrm->SetMidiRecordWnd(NULL);
		}
	}
	ScrollView::OnDestroy();
}


LResult CModScrollView::OnUpdatePosition(WParam, LParam lParam)
{
	Notification *pnotify = (Notification *)lParam;
	if (pnotify) return OnPlayerNotify(pnotify);
	return 0;
}


bool CModScrollView::OnScroll(uint32 nScrollCode, uint32 nPos, bool bDoScroll)
{
	if((nScrollCode & 0xFF) == ui::ScrollThumbTrack)
		m_nScrollPosX = nPos;
	else if(((nScrollCode >> 8) & 0xFF) == ui::ScrollThumbTrack)
		m_nScrollPosY = nPos;
	if(bDoScroll)
		m_nScrollPosXfine = m_nScrollPosYfine = 0;
	return ScrollView::OnScroll(nScrollCode, nPos, bDoScroll);
}


bool CModScrollView::OnScrollBy(Size sizeScroll, bool bDoScroll)
{
	bool ret = ScrollView::OnScrollBy(sizeScroll, bDoScroll);
	if(ret)
	{
		if(sizeScroll.cx)
			m_nScrollPosX = GetScrollPos(0);
		if(sizeScroll.cy)
			m_nScrollPosY = GetScrollPos(1);
		if(bDoScroll)
			m_nScrollPosXfine = m_nScrollPosYfine = 0;
	}
	return ret;
}


int CModScrollView::SetScrollPos(int nBar, int nPos, bool bRedraw)
{
	if(nBar == 0)
		m_nScrollPosX = nPos;
	else if(nBar == 1)
		m_nScrollPosY = nPos;
	return ScrollView::SetScrollPos(nBar, nPos, bRedraw);
}


void CModScrollView::SetScrollSizes(Size sizeTotal, Size sizePage, Size sizeLine)
{
	ScrollView::SetScrollSizes(sizeTotal, sizePage, sizeLine);
	// Fix scroll positions
	m_nScrollPosX = GetScrollPos(0);
	m_nScrollPosY = GetScrollPos(1);
}


OPENMPT_NAMESPACE_END
