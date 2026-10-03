/*
 * ChannelManagerDlg.cpp
 * ---------------------
 * Purpose: Dialog class for moving, removing, managing channels
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "ui/Ui.h"
#include "ChannelManagerDlg.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "PatternEditorDialogs.h"
#include "resource.h"
#include "UpdateHints.h"
#include "../common/mptStringBuffer.h"

#include <functional>

OPENMPT_NAMESPACE_BEGIN

#define CM_NB_COLS		8
#define CM_BT_HEIGHT	22

///////////////////////////////////////////////////////////
// CChannelManagerDlg

UI_MESSAGE_MAP_BEGIN(CChannelManagerDlg, DialogBase)

	UI_COMMAND(IDC_BUTTON1, &CChannelManagerDlg::OnApply)
	UI_COMMAND(IDC_BUTTON2, &CChannelManagerDlg::OnClose)
	UI_COMMAND(IDC_BUTTON3, &CChannelManagerDlg::OnSelectAll)
	UI_COMMAND(IDC_BUTTON4, &CChannelManagerDlg::OnInvert)
	UI_COMMAND(IDC_BUTTON5, &CChannelManagerDlg::OnAction1)
	UI_COMMAND(IDC_BUTTON6, &CChannelManagerDlg::OnAction2)
	UI_COMMAND(IDC_BUTTON7, &CChannelManagerDlg::OnStore)
	UI_COMMAND(IDC_BUTTON8, &CChannelManagerDlg::OnRestore)

	UI_NOTIFY(ui::TabSelChange, IDC_TAB1, &CChannelManagerDlg::OnTabSelchange)
UI_MESSAGE_MAP_END()

CChannelManagerDlg * CChannelManagerDlg::sharedInstance_ = nullptr;

CChannelManagerDlg * CChannelManagerDlg::sharedInstanceCreate()
{
	try
	{
		if(sharedInstance_ == nullptr)
			sharedInstance_ = new CChannelManagerDlg();
	} catch(mpt::out_of_memory e)
	{
		mpt::delete_out_of_memory(e);
	}
	return sharedInstance_;
}

void CChannelManagerDlg::SetDocument(CModDoc *modDoc)
{
	if(modDoc == m_ModDoc)
		return;
	
	m_ModDoc = modDoc;
	ResetState(true, true, true);
	if(m_show)
	{
		if(m_ModDoc)
		{
			ResizeWindow();
			ShowWindow(true);  // In case the window was hidden because no module was loaded
			InvalidateRect(&m_drawableArea, false);
		} else
		{
			ShowWindow(false);
		}
	}
}

bool CChannelManagerDlg::IsDisplayed() const
{
	return m_show;
}

void CChannelManagerDlg::Update(UpdateHint hint, HintObject *pHint)
{
	if(!IsWindow() || !m_show)
		return;
	if(!hint.ToType<GeneralHint>().GetType()[HINT_MODCHANNELS | HINT_MODGENERAL | HINT_MODTYPE | HINT_MPTOPTIONS])
		return;

	const size_t oldNumChannels = m_states.size();
	m_states.resize(m_ModDoc->GetNumChannels());
	for(size_t chn = oldNumChannels; chn < m_states.size(); chn++)
	{
		m_states[chn].sourceChn = static_cast<CHANNELINDEX>(chn);
	}

	ResizeWindow();
	InvalidateRect(nullptr, false);
	if(hint.ToType<GeneralHint>().GetType()[HINT_MODCHANNELS] && m_quickChannelProperties && pHint != m_quickChannelProperties.get())
		m_quickChannelProperties->UpdateDisplay();
}

void CChannelManagerDlg::Show()
{
	if(!IsWindow())
	{
		Create(IDD_CHANNELMANAGER, nullptr);
	}
	ResizeWindow();
	ShowWindow(true);
	m_show = true;
}

void CChannelManagerDlg::Hide()
{
	if(IsWindow() && m_show)
	{
		ResetState(true, true, true);
		ShowWindow(false);
		m_show = false;
	}
}


CChannelManagerDlg::CChannelManagerDlg()
	: m_quickChannelProperties{std::make_unique<QuickChannelProperties>()}
	, m_buttonHeight{CM_BT_HEIGHT}
{
}

CChannelManagerDlg::~CChannelManagerDlg()
{
	if(this == sharedInstance_)
		sharedInstance_ = nullptr;
	DestroyWindow();
}

bool CChannelManagerDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();

	m_tabCtrl.InsertItem(kSoloMute, UL_("Solo/Mute"));
	m_tabCtrl.InsertItem(kRecordSelect, UL_("Record Select"));
	m_tabCtrl.InsertItem(kPluginState, UL_("Plugins"));
	m_tabCtrl.InsertItem(kReorderRemove, UL_("Reorder/Remove"));
	m_currentTab = kSoloMute;

	m_buttonHeight = ui::ScalePixels(CM_BT_HEIGHT, this);
	GetDlgItem(IDC_BUTTON1)->ShowWindow(false);

	ResetState(true, true, true, true);

	return true;
}


void CChannelManagerDlg::DoDataExchange(DataExchange *pDX)
{
	DialogBase::DoDataExchange(pDX);
	pDX->BindControl(IDC_TAB1, m_tabCtrl);
}


void CChannelManagerDlg::OnDPIChanged()
{
	ResizeWindow();
	DialogBase::OnDPIChanged();
}


bool CChannelManagerDlg::FindToolTip(Point point, Rect &area, mpt::ustring &text) const
{
	const CHANNELINDEX chn = ButtonHit(point, &area);
	if(chn == CHANNELINDEX_INVALID)
		return false;
	text = GetChannelToolTip(chn);
	return !text.empty();
}


mpt::ustring CChannelManagerDlg::GetChannelToolTip(uint32 id) const
{
	mpt::ustring text;
	if(!m_ModDoc || id >= m_states.size())
		return text;

	const CHANNELINDEX chn = m_states[id].sourceChn;
	const auto &chnSettings = m_ModDoc->GetSoundFile().ChnSettings[chn];
	if(!chnSettings.szName.empty())
		text = MPT_UFORMAT("{}: {}")(chn + 1, mpt::ToUnicode(m_ModDoc->GetSoundFile().GetCharsetInternal(), chnSettings.szName));
	else
		text = MPT_UFORMAT("Channel {}")(chn + 1);

	switch(m_currentTab)
	{
	case kSoloMute:
		text += chnSettings.dwFlags[CHN_MUTE] ? UL_(" (Muted)") : UL_(" (Unmuted)");
		break;
	case kRecordSelect:
		switch(m_ModDoc->GetChannelRecordGroup(chn))
		{
		case RecordGroup::NoGroup: text += UL_(" (No Record Group)"); break;
		case RecordGroup::Group1: text += UL_(" (Record Group 1)"); break;
		case RecordGroup::Group2: text += UL_(" (Record Group 2)"); break;
		}
		break;
	case kPluginState:
		text += chnSettings.dwFlags[CHN_NOFX] ? UL_(" (Plugins Bypassed)") : UL_(" (Plugins Enabled)");
		break;
	case kReorderRemove:
		if(m_states[id].removed)
			text += UL_(" (Marked for Removal)");
		break;
	case kNumTabs:
		MPT_ASSERT_NOTREACHED();
		break;
	}
	return text;
}


void CChannelManagerDlg::OnApply()
{
	if(!m_ModDoc) return;

	std::vector<State> newStates;
	std::vector<CHANNELINDEX> newChnOrder;
	newStates.reserve(m_ModDoc->GetNumChannels());
	newChnOrder.reserve(m_ModDoc->GetNumChannels());

	// Count new number of channels, copy pattern pointers & manager internal store memory
	for(const auto &state : m_states)
	{
		if(!state.removed)
		{
			newStates.push_back(state);
			newChnOrder.push_back(state.sourceChn);
		}
	}

	BeginWaitCursor();

	TrackerCriticalSection cs;
	if(m_ModDoc->ReArrangeChannels(newChnOrder) != newChnOrder.size())
	{
		cs.Leave();
		EndWaitCursor();
		return;
	}

	// Update manager internal store memory
	m_states = std::move(newStates);
	for(CHANNELINDEX chn = 0; chn < m_states.size(); chn++)
	{
		m_states[chn].sourceChn = chn;
	}

	cs.Leave();
	EndWaitCursor();

	ResetState(true, true, true, true);

	// Update document & windows
	m_ModDoc->SetModified();
	m_ModDoc->UpdateAllViews(nullptr, GeneralHint().Channels().ModType(), this); //refresh channel headers

	// Redraw channel manager window
	ResizeWindow();
	InvalidateRect(nullptr, false);
}

void CChannelManagerDlg::OnClose()
{
	ResetState(true, true, true, true);
	m_bkgnd = ui::Bitmap();
	m_show = false;

	DialogBase::OnCancel();
}

void CChannelManagerDlg::OnSelectAll()
{
	for(auto &state : m_states)
		state.select = true;
	InvalidateRect(&m_drawableArea, false);
}

void CChannelManagerDlg::OnInvert()
{
	for(auto &state : m_states)
		state.select = !state.select;
	InvalidateRect(&m_drawableArea, false);
}

void CChannelManagerDlg::OnAction(uint8 action)
{
	if(!m_show || m_ModDoc == nullptr)
		return;

	int numSelectedAndMatching = 0, numSelected = 0;
	const auto recordGroup = (action == 1 ? RecordGroup::Group1 : RecordGroup::Group2);

	switch(m_currentTab)
	{
		case kSoloMute:
			for(CHANNELINDEX chn = 0; chn < m_ModDoc->GetNumChannels(); chn++)
			{
				CHANNELINDEX sourceChn = m_states[chn].sourceChn;
				if(action == 1)
					m_ModDoc->MuteChannel(sourceChn, !m_states[chn].select);
				else if(m_states[chn].select)
					m_ModDoc->MuteChannel(sourceChn, !m_ModDoc->IsChannelMuted(sourceChn));
			}
			break;
		case kRecordSelect:
			for(CHANNELINDEX chn = 0; chn < m_ModDoc->GetNumChannels(); chn++)
			{
				CHANNELINDEX sourceChn = m_states[chn].sourceChn;
				if(m_states[chn].select)
					numSelected++;
				if(m_states[chn].select && m_ModDoc->GetChannelRecordGroup(sourceChn) == recordGroup)
					numSelectedAndMatching++;
			}
			for(CHANNELINDEX chn = 0; chn < m_ModDoc->GetNumChannels(); chn++)
			{
				if(m_states[chn].select)
				{
					CHANNELINDEX sourceChn = m_states[chn].sourceChn;
					if(numSelected != numSelectedAndMatching && m_ModDoc->GetChannelRecordGroup(sourceChn) != recordGroup)
						m_ModDoc->SetChannelRecordGroup(sourceChn, recordGroup);
					else if(numSelected == numSelectedAndMatching)
						m_ModDoc->SetChannelRecordGroup(sourceChn, RecordGroup::NoGroup);
				}
			}
			break;
		case kPluginState:
			for(CHANNELINDEX chn = 0; chn < m_ModDoc->GetNumChannels(); chn++)
			{
				CHANNELINDEX sourceChn = m_states[chn].sourceChn;
				if(m_states[chn].select)
					m_ModDoc->NoFxChannel(sourceChn, action == 2);
			}
			break;
		case kReorderRemove:
			if(action == 1)
			{
				for(CHANNELINDEX chn = 0; chn < m_ModDoc->GetNumChannels(); chn++)
				{
					if(m_states[chn].select)
						m_states[chn].removed = !m_states[chn].removed;
				}
			} else
			{
				ResetState(false, false, false, true);
			}
			break;
		case kNumTabs:
			MPT_ASSERT_NOTREACHED();
			break;
	}

	ResetState();

	m_ModDoc->UpdateAllViews(nullptr, GeneralHint().Channels(), this);
	InvalidateRect(&m_drawableArea, false);
}


void CChannelManagerDlg::OnStore()
{
	if(!m_show || m_ModDoc == nullptr)
		return;

	switch(m_currentTab)
	{
		case kSoloMute:
			for(auto &state : m_states)
				state.memoryMute = m_ModDoc->IsChannelMuted(state.sourceChn);
			break;
		case kRecordSelect:
			for(auto &state : m_states)
				state.memoryRecordGroup = static_cast<uint8>(m_ModDoc->GetChannelRecordGroup(state.sourceChn));
			break;
		case kPluginState:
			for(auto &state : m_states)
				state.memoryNoFx = m_ModDoc->IsChannelNoFx(state.sourceChn);
			break;
		case kReorderRemove:
		default:
			break;
	}
}

void CChannelManagerDlg::OnRestore()
{
	if(!m_show || m_ModDoc == nullptr)
		return;

	switch(m_currentTab)
	{
		case kSoloMute:
			for(auto &state : m_states)
				m_ModDoc->MuteChannel(state.sourceChn, state.memoryMute);
			break;
		case kRecordSelect:
			m_ModDoc->ReinitRecordState();
			for(auto &state : m_states)
				m_ModDoc->SetChannelRecordGroup(state.sourceChn, static_cast<RecordGroup>(state.memoryRecordGroup));
			break;
		case kPluginState:
			for(auto &state : m_states)
				m_ModDoc->NoFxChannel(state.sourceChn, state.memoryNoFx);
			break;
		case kReorderRemove:
			for(CHANNELINDEX chn = 0; chn < m_ModDoc->GetNumChannels(); chn++)
				m_states[chn].sourceChn = chn;
			ResetState(false, false, false, true);
			break;
		default:
			break;
	}

	if(m_currentTab != kReorderRemove)
		ResetState();

	m_ModDoc->UpdateAllViews(nullptr, GeneralHint().Channels(), this);
	InvalidateRect(&m_drawableArea, false);
}

void CChannelManagerDlg::OnTabSelchange(NotifyHeader* /*header*/, LResult* /*pResult*/)
{
	if(!m_show) return;

	m_currentTab = static_cast<Tab>(m_tabCtrl.GetCurSel());

	switch(m_currentTab)
	{
		case kSoloMute:
			SetDlgItemText(IDC_BUTTON5, UL_("S&olo"));
			SetDlgItemText(IDC_BUTTON6, UL_("M&ute"));
			GetDlgItem(IDC_BUTTON5)->ShowWindow(true);
			GetDlgItem(IDC_BUTTON6)->ShowWindow(true);
			GetDlgItem(IDC_BUTTON1)->ShowWindow(false);
			break;
		case kRecordSelect:
			SetDlgItemText(IDC_BUTTON5, UL_("Instrument &1"));
			SetDlgItemText(IDC_BUTTON6, UL_("Instrument &2"));
			GetDlgItem(IDC_BUTTON5)->ShowWindow(true);
			GetDlgItem(IDC_BUTTON6)->ShowWindow(true);
			GetDlgItem(IDC_BUTTON1)->ShowWindow(false);
			break;
		case kPluginState:
			SetDlgItemText(IDC_BUTTON5, UL_("&Enable FX"));
			SetDlgItemText(IDC_BUTTON6, UL_("&Disable FX"));
			GetDlgItem(IDC_BUTTON5)->ShowWindow(true);
			GetDlgItem(IDC_BUTTON6)->ShowWindow(true);
			GetDlgItem(IDC_BUTTON1)->ShowWindow(false);
			break;
		case kReorderRemove:
			SetDlgItemText(IDC_BUTTON5, UL_("R&emove"));
			SetDlgItemText(IDC_BUTTON6, UL_("Ca&ncel All"));
			GetDlgItem(IDC_BUTTON5)->ShowWindow(true);
			GetDlgItem(IDC_BUTTON6)->ShowWindow(true);
			GetDlgItem(IDC_BUTTON1)->ShowWindow(true);
			break;
		default:
			break;
	}

	InvalidateRect(&m_drawableArea, false);
}


void CChannelManagerDlg::ResizeWindow()
{
	if(!IsWindow() || !m_ModDoc)
		return;

	const int dpi = ui::GetDpiForWindow(this);

	const auto oldButtonHeight = m_buttonHeight;
	m_buttonHeight = MulDiv(CM_BT_HEIGHT, dpi, 96);

	CHANNELINDEX channels = m_ModDoc->GetNumChannels();
	int lines = channels / CM_NB_COLS + (channels % CM_NB_COLS ? 1 : 0);

	Rect window;
	GetWindowRect(window);

	Rect client;
	GetClientRect(client);
	m_drawableArea = client;
	m_drawableArea.DeflateRect(MulDiv(10, dpi, 96), MulDiv(38, dpi, 96), MulDiv(8, dpi, 96), MulDiv(30, dpi, 96));

	int chnSizeY = m_drawableArea.Height() / lines;

	if(chnSizeY != m_buttonHeight || oldButtonHeight != m_buttonHeight)
	{
		SetWindowPos(nullptr, 0, 0, window.Width(), window.Height() + (m_buttonHeight - chnSizeY) * lines, ui::PosNoMove | ui::PosNoZOrder | ui::PosNoRedraw);

		GetClientRect(client);

		// Move butttons to bottom of the window
		for(auto id : {IDC_BUTTON1, IDC_BUTTON2, IDC_BUTTON3, IDC_BUTTON4, IDC_BUTTON5, IDC_BUTTON6})
		{
			Wnd *button = GetDlgItem(id);
			if(button != nullptr)
			{
				Rect btn;
				button->GetWindowRect(btn);
				ScreenToClient(&btn);
				button->SetWindowPos(nullptr, btn.left, client.Height() - btn.Height() - MulDiv(3, dpi, 96), 0, 0, ui::PosNoSize | ui::PosNoZOrder | ui::PosNoActivate);
			}
		}

		m_bkgnd = ui::Bitmap();

		m_drawableArea = client;
		m_drawableArea.DeflateRect(MulDiv(10, dpi, 96), MulDiv(38, dpi, 96), MulDiv(8, dpi, 96), MulDiv(30, dpi, 96));
		Invalidate(false);
	}
}


void CChannelManagerDlg::OnPaint(ui::Painter &dc)
{
	if(!IsWindow() || !m_show || m_ModDoc == nullptr)
	{
		ShowWindow(false);
		return;
	}

	const Rect clipBox = dc.GetClipBox();
	if(m_currentTab == kReorderRemove && m_moveRect)
	{
		if(!m_bkgnd.IsValid())
		{
			ui::OffscreenBuffer buffer(w(), h());
			buffer.Begin();
			{
				ui::Painter bufferDC;
				DrawChannels(bufferDC, Rect(0, 0, w(), h()));
			}
			m_bkgnd = buffer.ReadPixels();
			buffer.End();
		}
		dc.DrawBitmap(m_bkgnd, 0, 0, 0, 0, m_bkgnd.GetWidth(), m_bkgnd.GetHeight());

		// Dragged channels are shown semi-transparent at the mouse position
		for(const auto &state : m_states)
		{
			if(!state.select)
				continue;
			Rect btn = state.move;
			btn.DeflateRect(3, 3, 0, 0);
			if(btn.Width() <= 0 || btn.Height() <= 0)
				continue;
			ui::Bitmap ghost(btn.Width(), btn.Height());
			ghost.SetHasAlpha(true);
			for(int y = 0; y < btn.Height(); ++y)
			{
				for(int x = 0; x < btn.Width(); ++x)
					ghost.GetPixels()[y * btn.Width() + x] = (m_bkgnd.GetPixels()[(btn.top + y) * m_bkgnd.GetWidth() + btn.left + x] & 0xFFFFFF) | (192u << 24);
			}
			dc.DrawBitmap(ghost, btn.left + m_moveX - m_downX, btn.top + m_moveY - m_downY, 0, 0, btn.Width(), btn.Height());
		}
		return;
	}

	m_bkgnd = ui::Bitmap();
	DrawChannels(dc, clipBox);
}


void CChannelManagerDlg::DrawChannels(ui::Painter &dc, const Rect &rcPaint)
{
	dc.SetFont(ui::GetGuiFont());

	const int chnSizeX = m_drawableArea.Width() / CM_NB_COLS;
	const int chnSizeY = m_buttonHeight;

	Rect client;
	GetClientRect(&client);
	const int dpi = ui::GetDpiForWindow(this);
	client.SetRect(client.left + MulDiv(2, dpi, 96), client.top + MulDiv(32, dpi, 96), client.right - MulDiv(2, dpi, 96), client.bottom - MulDiv(24, dpi, 96));
	// Draw background
	{
		Rect bgIntersected;
		bgIntersected.IntersectRect(client, rcPaint);
		dc.FillSolidRect(rcPaint, ui::GetSystemColor(ui::SysColor::ButtonFace));
		dc.FillSolidRect(bgIntersected, ui::GetSystemColor(ui::SysColor::Highlight));
		dc.FrameRect(client, RGB(20, 20, 20));
	}

	client.SetRect(client.left + 8, client.top + 6, client.right - 6, client.bottom - 6);

	const ColorRef highlight = ui::GetSystemColor(ui::SysColor::Highlight), red = RGB(192, 96, 96), green = RGB(96, 192, 96), redBright = RGB(218, 163, 163), greenBright = RGB(163, 218, 163);
	const ColorRef brushColors[] = { highlight, green, red };
	const ColorRef brushColorsBright[] = { highlight, greenBright, redBright };
	const auto buttonFaceColor = ui::GetSystemColor(ui::SysColor::ButtonFace), windowColor = ui::GetSystemColor(ui::SysColor::Window);

	uint32 col = 0, row = 0;
	const CTrackerSoundFile &sndFile = m_ModDoc->GetSoundFile();
	mpt::ustring s;
	for(const auto &state : m_states)
	{
		const CHANNELINDEX sourceChn = state.sourceChn;
		const auto &chnSettings = sndFile.ChnSettings[sourceChn];

		if(!chnSettings.szName.empty())
			s = MPT_UFORMAT("{}: {}")(sourceChn + 1, mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.ChnSettings[sourceChn].szName));
		else
			s = MPT_UFORMAT("Channel {}")(sourceChn + 1);

		const int border = ui::ScalePixels(3, this);
		Rect btn;
		btn.left = client.left + col * chnSizeX + border;
		btn.right = btn.left + chnSizeX - border;
		btn.top = client.top + row * chnSizeY + border;
		btn.bottom = btn.top + chnSizeY - border;
		col++;
		if(col >= CM_NB_COLS)
		{
			col = 0;
			row++;
		}

		if(!Rect{}.IntersectRect(rcPaint, btn))
			continue;

		// Button
		const bool activate = state.select;
		const bool enable = !state.removed;
		const Rect btnAdjusted = dc.DrawBevel(btn, enable, true);  // Without border
		if(activate)
			dc.FillSolidRect(btnAdjusted, windowColor);

		if(sndFile.HasChannelColor(sourceChn))
		{
			// Channel color
			const auto startColor = sndFile.GetChannelColor(sourceChn);
			const auto endColor = activate ? windowColor : buttonFaceColor;
			const auto width = btnAdjusted.Width() / 2;
			auto rect = btnAdjusted;
			rect.left = rect.right - width;
			rect.right = rect.left + 1;
			for(int i = 0; i < width; i++)
			{
				auto blend = static_cast<double>(i) / width, blendInv = 1.0 - blend;
				auto blendColor = RGB(mpt::saturate_round<uint8>(GetRValue(startColor) * blend + GetRValue(endColor) * blendInv),
				                      mpt::saturate_round<uint8>(GetGValue(startColor) * blend + GetGValue(endColor) * blendInv),
				                      mpt::saturate_round<uint8>(GetBValue(startColor) * blend + GetBValue(endColor) * blendInv));
				dc.FillSolidRect(rect, blendColor);
				rect.left++;
				rect.right++;
			}
		}

		// Text
		{
			auto rect = btnAdjusted;
			rect.left += ui::ScalePixels(9, this);
			rect.right -= ui::ScalePixels(3, this);

			dc.SetBkTransparent(true);
			dc.SetTextColor(ui::GetSystemColor(enable || activate ? ui::SysColor::ButtonText : ui::SysColor::GrayText));
			dc.DrawText(s, rect, ui::TextRight | ui::TextVCenter | ui::TextSingleLine | ui::TextNoPrefix);
		}

		// Draw red/green markers
		{
			const int margin = ui::ScalePixels(1, this);
			auto rect = btnAdjusted;
			rect.DeflateRect(margin, margin);
			rect.right = rect.left + ui::ScalePixels(7, this);
			const auto &brushes = activate ? brushColorsBright : brushColors;
			const auto redBrush = brushes[2], greenBrush = brushes[1];
			ColorRef color = 0;
			switch(m_currentTab)
			{
			case kSoloMute:
				color = chnSettings.dwFlags[CHN_MUTE] ? redBrush : greenBrush;
				break;
			case kRecordSelect:
				color = brushColors[static_cast<size_t>(m_ModDoc->GetChannelRecordGroup(sourceChn)) % std::size(brushColors)];
				break;
			case kPluginState:
				color = chnSettings.dwFlags[CHN_NOFX] ? redBrush : greenBrush;
				break;
			case kReorderRemove:
				color = state.removed ? redBrush : greenBrush;
				break;
			case kNumTabs:
				MPT_ASSERT_NOTREACHED();
				break;
			}
			dc.FillSolidRect(rect, color);
			// Draw border around marker
			dc.FrameRect(rect, RGB(20, 20, 20));
		}
	}
}


CHANNELINDEX CChannelManagerDlg::ButtonHit(Point point, Rect *invalidate) const
{
	const Rect &client = m_drawableArea;

	if(client.PtInRect(point) && m_ModDoc != nullptr)
	{
		uint32 nColns = CM_NB_COLS;

		int x = point.x - client.left;
		int y = point.y - client.top;

		int dx = client.Width() / (int)nColns;
		int dy = m_buttonHeight;

		x = x / dx;
		y = y / dy;
		CHANNELINDEX n = static_cast<CHANNELINDEX>(y * nColns + x);
		if(n < m_ModDoc->GetNumChannels())
		{
			if(invalidate)
			{
				invalidate->left = client.left + x * dx;
				invalidate->right = invalidate->left + dx;
				invalidate->top = client.top + y * dy;
				invalidate->bottom = invalidate->top + dy;
			}
			return n;
		}
	}
	return CHANNELINDEX_INVALID;
}


void CChannelManagerDlg::ResetState(bool bSelection, bool bMove, bool bInternal, bool bOrder)
{
	size_t oldSize = m_states.size();
	m_states.resize(m_ModDoc ? m_ModDoc->GetNumChannels() : 0);
	for(CHANNELINDEX chn = 0; chn < m_states.size(); chn++)
	{
		if(bSelection)
			m_states[chn].select = false;
		if(bOrder || chn > oldSize)
		{
			m_states[chn].sourceChn = chn;
			m_states[chn].removed = false;
		}
	}
	if(bMove || bInternal)
	{
		m_leftButton = false;
		m_rightButton = false;
	}
	if(bMove)
		m_moveRect = false;
}


void CChannelManagerDlg::OnMouseMove(uint32 nFlags,Point point)
{
	if(!IsWindow() || m_show == false) return;

	if(!m_leftButton && !m_rightButton)
	{
		m_moveX = point.x;
		m_moveY = point.y;
		return;
	}
	MouseEvent(nFlags, point, m_moveRect ? CM_BT_NONE : (m_leftButton ? CM_BT_LEFT : CM_BT_RIGHT));
}

void CChannelManagerDlg::OnLButtonUp(uint32 /*nFlags*/,Point point)
{
	ReleaseCapture();
	if(!IsWindow() || !m_show)
		return;

	if(m_moveRect && m_ModDoc)
	{
		Rect dropRect;
		CHANNELINDEX dropChn = ButtonHit(point, &dropRect);
		if(dropChn != CHANNELINDEX_INVALID)
		{
			// Rearrange channels
			const auto numChannels = m_ModDoc->GetNumChannels();
			if(point.x > dropRect.left + dropRect.Width() / 2 && dropChn < numChannels)
				dropChn++;

			std::vector<State> states;
			CHANNELINDEX selectedBeforeDropChn = 0;
			for(CHANNELINDEX chn = 0; chn < numChannels; chn++)
			{
				if(m_states[chn].select)
				{
					states.push_back(m_states[chn]);
					states.back().select = false;
					if(chn < dropChn)
						selectedBeforeDropChn++;
				}
			}

			// Remove all selected channels from the order
			const auto IsSelected = [](const State &state) { return state.select; };
			m_states.erase(std::remove_if(m_states.begin(), m_states.end(), IsSelected), m_states.end());
			// Then insert them at the drop position
			m_states.insert(m_states.begin() + dropChn - selectedBeforeDropChn, states.begin(), states.end());
		} else
		{
			ResetState(true, false, false, false);
		}

		m_moveRect = false;
		InvalidateRect(&m_drawableArea, false);
		if(m_ModDoc) m_ModDoc->UpdateAllViews(nullptr, GeneralHint().Channels(), this);
	}

	m_leftButton = false;
}

void CChannelManagerDlg::OnLButtonDown(uint32 nFlags,Point point)
{
	if(!IsWindow() || m_show == false) return;
	SetCapture();

	if(ButtonHit(point) == CHANNELINDEX_INVALID)
		ResetState(true, false, false);

	m_leftButton = true;
	m_buttonAction = kUndetermined;
	MouseEvent(nFlags,point,CM_BT_LEFT);
	m_downX = point.x;
	m_downY = point.y;
}

void CChannelManagerDlg::OnRButtonUp(uint32 /*nFlags*/,Point /*point*/)
{
	ReleaseCapture();
	m_rightButton = false;
}

void CChannelManagerDlg::OnRButtonDown(uint32 nFlags,Point point)
{
	if(!IsWindow() || m_show == false) return;
	SetCapture();

	m_rightButton = true;
	m_buttonAction = kUndetermined;
	if(m_moveRect)
	{
		ResetState(true, true, false, false);
		InvalidateRect(&m_drawableArea, false);
	} else
	{
		MouseEvent(nFlags, point, CM_BT_RIGHT);
		m_downX = point.x;
		m_downY = point.y;
	}
}

void CChannelManagerDlg::OnMButtonDown(uint32 /*nFlags*/, Point point)
{
	Rect rect;
	CHANNELINDEX chn = ButtonHit(point, &rect);
	if(m_ModDoc != nullptr && chn != CHANNELINDEX_INVALID)
	{
		ClientToScreen(&point);
		m_quickChannelProperties->Show(m_ModDoc, m_states[chn].sourceChn, point);
	}
}

void CChannelManagerDlg::MouseEvent(uint32 nFlags,Point point, MouseButton button)
{
	if(!m_ModDoc)
		return;

	m_moveX = point.x;
	m_moveY = point.y;

	Rect client, invalidate;
	const CHANNELINDEX n = ButtonHit(point, &invalidate);
	if(n != CHANNELINDEX_INVALID && button != CM_BT_NONE)
	{
		const CHANNELINDEX sourceChn = m_states[n].sourceChn;
		if(nFlags & ui::MouseControl)
		{
			if(button == CM_BT_LEFT)
			{
				if(!m_states[n].select && !m_states[n].removed)
					m_states[n].move = invalidate;
				m_states[n].select = true;
			} else if(button == CM_BT_RIGHT)
			{
				m_states[n].select = false;
			}
		} else
		{
			switch(m_currentTab)
			{
			case kSoloMute:
				if(button == CM_BT_LEFT)
				{
					if(m_buttonAction == kUndetermined)
					{
						bool isAlreadySolo = true;
						for(CHANNELINDEX chn = 0; chn < m_ModDoc->GetNumChannels(); chn++)
						{
							if((chn == sourceChn) == m_ModDoc->IsChannelMuted(chn))
							{
								isAlreadySolo = false;
								break;
							}
						}
						m_buttonAction = isAlreadySolo ? kAction2 : kAction1;
						for(CHANNELINDEX chn = 0; chn < m_ModDoc->GetNumChannels(); chn++)
						{
							m_ModDoc->MuteChannel(chn, m_buttonAction == kAction1);
						}
					}
					if(m_buttonAction == kAction1)
						m_ModDoc->MuteChannel(sourceChn, false);
					invalidate = client = m_drawableArea;
				} else
				{
					if(m_buttonAction == kUndetermined)
						m_buttonAction = m_ModDoc->IsChannelMuted(sourceChn) ? kAction1 : kAction2;
					m_ModDoc->MuteChannel(sourceChn, m_buttonAction == kAction2);
				}
				m_ModDoc->SetModified();
				m_ModDoc->UpdateAllViews(nullptr, GeneralHint(sourceChn).Channels(), this);
				break;
			case kRecordSelect:
				if(m_buttonAction == kUndetermined)
				{
					auto rec = m_ModDoc->GetChannelRecordGroup(sourceChn);
					m_buttonAction = (rec == RecordGroup::NoGroup || rec != (button == CM_BT_LEFT ? RecordGroup::Group1 : RecordGroup::Group2)) ? kAction1 : kAction2;
				}

				if(m_buttonAction == kAction1 && button == CM_BT_LEFT)
					m_ModDoc->SetChannelRecordGroup(sourceChn, RecordGroup::Group1);
				else if(m_buttonAction == kAction1 && button == CM_BT_RIGHT)
					m_ModDoc->SetChannelRecordGroup(sourceChn, RecordGroup::Group2);
				else
					m_ModDoc->SetChannelRecordGroup(sourceChn, RecordGroup::NoGroup);
				m_ModDoc->UpdateAllViews(nullptr, GeneralHint(sourceChn).Channels(), this);
				break;
			case kPluginState:
				m_ModDoc->NoFxChannel(sourceChn, (button != CM_BT_LEFT));
				m_ModDoc->SetModified();
				m_ModDoc->UpdateAllViews(nullptr, GeneralHint(sourceChn).Channels(), this);
				break;
			case kReorderRemove:
				if(button == CM_BT_LEFT)
				{
					m_states[n].move = invalidate;
					m_states[n].select = true;
				} else if(button == CM_BT_RIGHT)
				{
					if(m_buttonAction == kUndetermined)
						m_buttonAction = m_states[n].removed ? kAction1 : kAction2;
					m_states[n].select = false;
					m_states[n].removed = (m_buttonAction == kAction2);
				}

				if(m_states[n].select || button == CM_BT_NONE)
				{
					m_moveRect = true;
				}
				break;
			case kNumTabs:
				MPT_ASSERT_NOTREACHED();
				break;
			}
		}

		InvalidateRect(m_moveRect ? &m_drawableArea : &invalidate, false);
	} else
	{
		InvalidateRect(&m_drawableArea, false);
	}
}


void CChannelManagerDlg::OnLButtonDblClk(uint32 nFlags, Point point)
{
	OnLButtonDown(nFlags, point);
	DialogBase::OnLButtonDblClk(nFlags, point);
}


OPENMPT_NAMESPACE_END
