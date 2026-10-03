/*
 * Childfrm.cpp
 * ------------
 * Purpose: Implementation of the MDI document child windows.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "ui/Ui.h"
#include "Childfrm.h"
#include "ChannelManagerDlg.h"
#include "Ctrl_ins.h"
#include "Ctrl_pat.h"
#include "Ctrl_smp.h"
#include "Globals.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "resource.h"
#include "view_com.h"
#include "View_gen.h"
#include "View_ins.h"
#include "View_pat.h"
#include "View_smp.h"
#include "WindowMessages.h"
#include "../common/FileReader.h"
#include "mpt/io/io.hpp"
#include "mpt/io/io_stdstream.hpp"

#include <sstream>


OPENMPT_NAMESPACE_BEGIN


/////////////////////////////////////////////////////////////////////////////
// CChildFrame


UI_MESSAGE_MAP_BEGIN(CChildFrame, ChildFrameBase)
	UI_MESSAGE(MSG_MOD_CHANGEVIEWCLASS,    &CChildFrame::OnChangeViewClass)
	UI_MESSAGE(MSG_MOD_INSTRSELECTED,      &CChildFrame::OnInstrumentSelected)
UI_MESSAGE_MAP_END()

CChildFrame *CChildFrame::m_lastActiveFrame = nullptr;
int CChildFrame::glMdiOpenCount = 0;

/////////////////////////////////////////////////////////////////////////////
// CChildFrame construction/destruction

CChildFrame::CChildFrame()
{
	glMdiOpenCount++;
}


CChildFrame::~CChildFrame()
{
	if ((--glMdiOpenCount) == 0)
	{
		TrackerSettings::Instance().gbMdiMaximize = m_maxWhenClosed;
	}
}


namespace
{
constexpr int splitterBarHeight = 4;
constexpr int minPaneHeight = 15;

View *CreateViewForType(ViewType viewType)
{
	switch(viewType)
	{
	case ViewType::General: return new CViewGlobals();
	case ViewType::Pattern: return new CViewPattern();
	case ViewType::Sample: return new CViewSample();
	case ViewType::Instrument: return new CViewInstrument();
	case ViewType::Comments: return new CViewComments();
	case ViewType::None: break;
	}
	return nullptr;
}

int ClampSplitHeight(int splitHeight, const CModControlView *controlView, int frameHeight)
{
	const int minSplitHeight = std::max(minPaneHeight, controlView ? controlView->CalcMinHeight() : 0);
	return std::max(minSplitHeight, std::min(splitHeight, frameHeight - splitterBarHeight - minPaneHeight));
}
}


bool CChildFrame::CreateViews(int initialHeight)
{
	begin();
	m_controlView = new CModControlView();
	end();
	m_controlView->SetDocument(GetActiveDocument());
	m_hWndCtrl = m_controlView;
	m_controlView->SetMDIParentFrame(this);
	m_dpi = 96;

	m_splitHeight = ui::WindowsPixelsToPixelsY(TrackerSettings::Instance().glGeneralWindowHeight);
	if(m_splitHeight <= 1)
		m_splitHeight = (initialHeight * 2) / 3;
	return ChangeViewType(ViewType::General);
}


void CChildFrame::SetSplitterHeight(int cy)
{
	if(cy <= 1)
		cy = 188;
	m_splitHeight = std::max(ui::WindowsPixelsToPixelsY(cy), m_controlView ? m_controlView->CalcMinHeight() : 0);
	LayoutViews();
}


void CChildFrame::LayoutViews()
{
	const int frameWidth = w(), frameHeight = h();
	const int splitHeight = ClampSplitHeight(m_splitHeight, m_controlView, frameHeight);
	if(m_controlView)
		m_controlView->GetWidget()->resize(x(), y(), frameWidth, splitHeight);
	if(m_bottomView)
	{
		const int bottomTop = splitHeight + splitterBarHeight;
		m_bottomView->GetWidget()->resize(x(), y() + bottomTop, frameWidth, std::max(0, frameHeight - bottomTop));
	}
	redraw();
}


void CChildFrame::resize(int x, int y, int width, int height)
{
	ChildFrameBase::resize(x, y, width, height);
	LayoutViews();
}


int CChildFrame::handle(int event)
{
	const int splitterTop = y() + ClampSplitHeight(m_splitHeight, m_controlView, h());
	const bool isOverSplitter = Fl::event_y() >= splitterTop && Fl::event_y() < splitterTop + splitterBarHeight;
	switch(event)
	{
	case FL_ENTER:
	case FL_MOVE:
		SetCursorShape(isOverSplitter ? FL_CURSOR_NS : FL_CURSOR_DEFAULT);
		break;
	case FL_PUSH:
		if(isOverSplitter)
		{
			m_isDraggingSplitter = true;
			return 1;
		}
		break;
	case FL_DRAG:
		if(m_isDraggingSplitter)
		{
			m_splitHeight = ClampSplitHeight(Fl::event_y() - y(), m_controlView, h());
			LayoutViews();
			return 1;
		}
		break;
	case FL_RELEASE:
		if(m_isDraggingSplitter)
		{
			m_isDraggingSplitter = false;
			return 1;
		}
		break;
	default:
		break;
	}
	return ChildFrameBase::handle(event);
}


void CChildFrame::OnMDIActivate(bool bActivate, Wnd *pActivateWnd, Wnd *pDeactivateWnd)
{
	if(bActivate && m_hWndView)
		CMainFrame::GetMainFrame()->SetMidiRecordWnd(m_hWndView);
	if(m_hWndCtrl)
		m_hWndCtrl->SendMessage(bActivate ? MSG_MOD_MDIACTIVATE : MSG_MOD_MDIDEACTIVATE, 0, 0);
	if(m_hWndView)
		m_hWndView->SendMessage(bActivate ? MSG_MOD_MDIACTIVATE : MSG_MOD_MDIDEACTIVATE, 0, 0);

	if(bActivate)
	{
		CMainFrame::GetMainFrame()->UpdateEffectKeys(static_cast<CModDoc *>(GetActiveDocument()));
		m_lastActiveFrame = this;
	}

	auto instance = CChannelManagerDlg::sharedInstance();
	if(instance != nullptr)
	{
		if(!bActivate && pActivateWnd == nullptr)
			instance->SetDocument(nullptr);
		else if(bActivate)
			instance->SetDocument(static_cast<CModDoc *>(GetActiveDocument()));
	}
	MPT_UNUSED(pDeactivateWnd);
}


void CChildFrame::ActivateFrame(int nCmdShow)
{
	ChildFrameBase::ActivateFrame(nCmdShow);

	// When song first loads, initialise patternViewState to point to start of song.
	View *pView = GetActiveView();
	CModDoc *pModDoc = nullptr;
	if (pView) pModDoc = (CModDoc *)pView->GetDocument();
	if ((m_hWndCtrl) && (pModDoc))
	{
		if (m_initialActivation && m_ViewPatterns.nPattern == 0)
		{
			if(!pModDoc->GetSoundFile().Order().empty())
				m_ViewPatterns.nPattern = pModDoc->GetSoundFile().Order()[0];
			m_initialActivation = false;
		}
	}
}


void CChildFrame::OnUpdateFrameTitle(bool bAddToTitle)
{
	GetMDIFrame()->OnUpdateFrameTitle(bAddToTitle);
	if(!bAddToTitle)
		return;
	const Document *pDocument = GetActiveDocument();
	if(pDocument == nullptr)
		return;
	mpt::ustring szText = pDocument->GetTitle();
	if(pDocument->IsModified())
		szText += UL_("*");
	SetTitle(szText);
	GetMDIFrame()->GetWidget()->redraw();
}


bool CChildFrame::ChangeViewType(ViewType newViewType)
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if(newViewType == m_currentViewType)
		return true;
	if(m_bottomView)
	{
		if(pMainFrm && pMainFrm->GetMidiRecordWnd() == m_hWndView)
			pMainFrm->SetMidiRecordWnd(nullptr);
		if(GetActiveView() == m_bottomView)
			SetActiveView(m_controlView, false);
		Fl_Widget *oldWidget = m_bottomView->GetWidget();
		m_bottomView->DestroyWindow();
		Fl::delete_widget(oldWidget);
		m_bottomView = nullptr;
	}
	m_hWndView = nullptr;
	m_currentViewType = ViewType::None;

	begin();
	m_bottomView = CreateViewForType(newViewType);
	end();
	if(!m_bottomView)
		return false;
	m_bottomView->SetDocument(GetActiveDocument());
	m_hWndView = m_bottomView;
	m_currentViewType = newViewType;
	// As with MFC, views get their initial update before their first size notification
	m_bottomView->InitialUpdate();
	LayoutViews();
	if(m_hWndCtrl)
	{
		m_hWndView->PostMessage(MSG_MOD_VIEWMSG, VIEWMSG_SETCTRLWND, reinterpret_cast<LParam>(m_hWndCtrl));
		// Sent, not posted: a queued pointer would dangle if the view is replaced again before delivery
		m_hWndCtrl->SendMessage(MSG_MOD_CTRLMSG, CTRLMSG_SETVIEWWND, reinterpret_cast<LParam>(m_hWndView));
		if(pMainFrm)
			pMainFrm->SetMidiRecordWnd(m_hWndView);
	}
	return true;
}

void CChildFrame::ForceRefresh()
{
	CModControlView *pModView;
	if ((pModView = GetModControlView()) != nullptr)
	{
		pModView->ForceRefresh();
	}

	return;
}

void CChildFrame::SavePosition(bool force)
{
	m_maxWhenClosed = false;
	if(force)
		TrackerSettings::Instance().gbMdiMaximize = m_maxWhenClosed;
	if(!m_controlView || w() == 0)
		return;
	const int l = ui::PixelsToWindowsPixelsY(m_splitHeight);
	switch(m_currentViewType)
	{
	case ViewType::General: TrackerSettings::Instance().glGeneralWindowHeight = l; break;
	case ViewType::Pattern: TrackerSettings::Instance().glPatternWindowHeight = l; break;
	case ViewType::Sample: TrackerSettings::Instance().glSampleWindowHeight = l; break;
	case ViewType::Instrument: TrackerSettings::Instance().glInstrumentWindowHeight = l; break;
	case ViewType::Comments: TrackerSettings::Instance().glCommentsWindowHeight = l; break;
	case ViewType::None: break;
	}
}


int CChildFrame::GetSplitterHeight()
{
	if(m_controlView)
		return ui::PixelsToWindowsPixelsY(m_splitHeight);
	return 15;
}


LResult CChildFrame::SendCtrlMessage(uint32 uMsg, LParam lParam) const
{
	if(m_hWndCtrl)
		return m_hWndCtrl->SendMessage(MSG_MOD_CTRLMSG, uMsg, lParam);
	return 0;
}


LResult CChildFrame::SendViewMessage(uint32 uMsg, LParam lParam) const
{
	if(m_hWndView)
		return m_hWndView->SendMessage(MSG_MOD_VIEWMSG, uMsg, lParam);
	return 0;
}


LResult CChildFrame::ActivateView(uint32 nId, LParam lParam) { return m_hWndCtrl->SendMessage(MSG_MOD_ACTIVATEVIEW, nId, lParam); }


LResult CChildFrame::OnInstrumentSelected(WParam wParam, LParam lParam)
{
	View *pView = GetActiveView();
	CModDoc *pModDoc = NULL;
	if (pView) pModDoc = (CModDoc *)pView->GetDocument();
	if ((m_hWndCtrl) && (pModDoc))
	{
		auto nIns = lParam;

		if ((!wParam) && (pModDoc->GetNumInstruments() > 0))
		{
			nIns = pModDoc->FindSampleParent(static_cast<SAMPLEINDEX>(nIns));
			if(nIns == INSTRUMENTINDEX_INVALID)
			{
				nIns = 0;
			}
		}
		m_hWndCtrl->SendMessage(MSG_MOD_CTRLMSG, CTRLMSG_PAT_SETINSTRUMENT, nIns);
	}
	return 0;
}


/////////////////////////////////////////////////////////////////////////////
// CChildFrame message handlers

void CChildFrame::OnDestroy()
{
	SavePosition();
	if(m_lastActiveFrame == this)
		m_lastActiveFrame = nullptr;
	ChildFrameBase::OnDestroy();
}


LResult CChildFrame::OnChangeViewClass(WParam wParam, LParam lParam)
{
	CModControlDlg *pDlg = (CModControlDlg *)lParam;
	if (pDlg)
	{
		const ViewType newViewType = pDlg->GetAssociatedViewType();
		if(newViewType != ViewType::None)
			ChangeViewType(newViewType);
		m_hWndCtrl->PostMessage(MSG_MOD_CTRLMSG, CTRLMSG_ACTIVATEPAGE, static_cast<LParam>(wParam));
	}
	return 0;
}


bool CChildFrame::IsPatternView() const
{
	return m_currentViewType == ViewType::Pattern;
}


void CChildFrame::SaveAllViewStates()
{
	void *ptr = nullptr;
	switch(m_currentViewType)
	{
	case ViewType::Pattern: ptr = &m_ViewPatterns; break;
	case ViewType::Sample: ptr = &m_ViewSamples; break;
	case ViewType::Instrument: ptr = &m_ViewInstruments; break;
	case ViewType::General: ptr = &m_ViewGeneral; break;
	case ViewType::Comments: ptr = &m_ViewComments; break;
	case ViewType::None: break;
	}
	if(m_hWndView)
		m_hWndView->SendMessage(MSG_MOD_VIEWMSG, VIEWMSG_SAVESTATE, reinterpret_cast<LParam>(ptr));
}


std::string CChildFrame::SerializeView()
{
	SaveAllViewStates();

	std::ostringstream f(std::ios::out | std::ios::binary);
	// Version
	mpt::IO::WriteVarInt(f, 1u);
	// Current page
	mpt::IO::WriteVarInt(f, static_cast<uint8>(GetModControlView()->GetActivePage()));

	const auto Serialize = [](std::ostringstream &f, CModControlView::Page page, const std::string &s)
	{
		mpt::IO::WriteVarInt(f, static_cast<uint8>(page));
		mpt::IO::WriteVarInt(f, s.size());
		mpt::IO::WriteRaw(f, s.data(), s.size());
	};
	Serialize(f, CModControlView::Page::Patterns, m_ViewPatterns.Serialize());
	Serialize(f, CModControlView::Page::Samples, m_ViewSamples.Serialize());
	Serialize(f, CModControlView::Page::Instruments, m_ViewInstruments.Serialize());

	return std::move(f).str();
}


void CChildFrame::DeserializeView(FileReader &file)
{
	uint32 version, page;
	if(!file.ReadVarInt(version) || version > 1)
		return;
	if(!file.ReadVarInt(page) || page >= static_cast<uint32>(CModControlView::Page::NumPages))
		return;

	uint32 pageDlg = 0;
	switch(static_cast<CModControlView::Page>(page))
	{
	case CModControlView::Page::Globals:
		pageDlg = IDD_CONTROL_GLOBALS;
		break;
	case CModControlView::Page::Patterns:
		pageDlg = IDD_CONTROL_PATTERNS;
		if(version == 0)
			m_ViewPatterns.Deserialize(file);
		break;
	case CModControlView::Page::Samples:
		pageDlg = IDD_CONTROL_SAMPLES;
		if(version == 0)
			m_ViewSamples.Deserialize(file);
		break;
	case CModControlView::Page::Instruments:
		pageDlg = IDD_CONTROL_INSTRUMENTS;
		if(version == 0)
			m_ViewInstruments.Deserialize(file);
		break;
	case CModControlView::Page::Comments:
		pageDlg = IDD_CONTROL_COMMENTS;
		break;
	case CModControlView::Page::Unknown:
	case CModControlView::Page::NumPages:
		break;
	}

	// Version 1 extensions: Deserialize all views, not just current view
	while(file.CanRead(3))
	{
		uint32 size = 0;
		file.ReadVarInt(page);
		file.ReadVarInt(size);
		FileReader chunk = file.ReadChunk(size);
		if(page >= static_cast<uint32>(CModControlView::Page::NumPages) || !chunk.IsValid())
			continue;

		switch(static_cast<CModControlView::Page>(page))
		{
		case CModControlView::Page::Globals:     m_ViewGeneral.Deserialize(chunk); break;
		case CModControlView::Page::Patterns:    m_ViewPatterns.Deserialize(chunk); break;
		case CModControlView::Page::Samples:     m_ViewSamples.Deserialize(chunk); break;
		case CModControlView::Page::Instruments: m_ViewInstruments.Deserialize(chunk); break;
		case CModControlView::Page::Comments:    m_ViewComments.Deserialize(chunk); break;
		case CModControlView::Page::Unknown:
		case CModControlView::Page::NumPages:
			break;
		}
	}

	GetModControlView()->PostMessage(MSG_MOD_ACTIVATEVIEW, pageDlg, LParam(-1));
}


namespace
{
bool IsFocusInside(const Wnd *parent)
{
	if(!parent)
		return false;
	for(const Fl_Widget *focus = Fl::focus(); focus != nullptr; focus = focus->parent())
	{
		if(focus == parent->GetWidget())
			return true;
	}
	return false;
}
}


void CChildFrame::ToggleViews()
{
	if(IsFocusInside(GetHwndView()))
		SendCtrlMessage(CTRLMSG_SETFOCUS);
	else if(IsFocusInside(GetHwndCtrl()))
		SendViewMessage(VIEWMSG_SETFOCUS);
}


std::string PatternViewState::Serialize() const
{
	std::ostringstream f(std::ios::out | std::ios::binary);
	mpt::IO::WriteVarInt(f, initialized ? nOrder : initialOrder);
	mpt::IO::WriteVarInt(f, visibleColumns.to_ulong());
	return std::move(f).str();
}


void PatternViewState::Deserialize(FileReader &f)
{
	f.ReadVarInt(initialOrder);
	unsigned long columns = 0;
	if(f.ReadVarInt(columns))
		visibleColumns = columns;
}


std::string SampleViewState::Serialize() const
{
	std::ostringstream f(std::ios::out | std::ios::binary);
	mpt::IO::WriteVarInt(f, nSample ? nSample : initialSample);
	return std::move(f).str();
}


void SampleViewState::Deserialize(FileReader &f)
{
	f.ReadVarInt(initialSample);
}


std::string InstrumentViewState::Serialize() const
{
	std::ostringstream f(std::ios::out | std::ios::binary);
	mpt::IO::WriteVarInt(f, instrument ? instrument : initialInstrument);
	return std::move(f).str();
}


void InstrumentViewState::Deserialize(FileReader &f)
{
	f.ReadVarInt(initialInstrument);
}


OPENMPT_NAMESPACE_END
