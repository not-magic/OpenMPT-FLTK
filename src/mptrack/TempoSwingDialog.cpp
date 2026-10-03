// FLTK port of openmpt/mptrack/TempoSwingDialog.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "TempoSwingDialog.h"
#include "Mainfrm.h"
#include "resource.h"


OPENMPT_NAMESPACE_BEGIN

void CTempoSwingDlg::RowCtls::SetValue(TempoSwing::value_type v)
{
	int32 val = Util::muldivr(static_cast<int32>(v) - TempoSwing::Unity, CTempoSwingDlg::SliderUnity, TempoSwing::Unity);
	valueSlider.SetPos(val);
}


TempoSwing::value_type CTempoSwingDlg::RowCtls::GetValue() const
{
	return Util::muldivr(valueSlider.GetPos(), TempoSwing::Unity, SliderUnity) + TempoSwing::Unity;
}


struct TempoSwingMeasurements
{
	enum
	{
		edRowLabelWidth = 64,                       // Label "Row 999:"
		edSliderWidth = 220,                        // Setting slider
		edSliderHeight = 20,                        // Setting slider
		edValueLabelWidth = 64,                     // Label "100%"
		edPaddingX = 8,                             // Spacing between elements
		edPaddingY = 4,                             // Spacing between elements
		edPaddingTop = 64,                          // Spacing from top of dialog
		edRowHeight = edSliderHeight + edPaddingY,  // Height of one set of controls
		edFooterHeight = 32,                        // Buttons
		edScrollbarWidth = 16,                      // Width of optional scrollbar
	};

	const int rowLabelWidth;
	const int sliderWidth;
	const int sliderHeight;
	const int valueLabelWidth;
	const int paddingX;
	const int paddingY;
	const int paddingTop;
	const int rowHeight;
	const int footerHeight;
	const int scrollbarWidth;

	TempoSwingMeasurements(WindowHandle hWnd)
		: rowLabelWidth(ui::ScalePixels(edRowLabelWidth, hWnd))
		, sliderWidth(ui::ScalePixels(edSliderWidth, hWnd))
		, sliderHeight(ui::ScalePixels(edSliderHeight, hWnd))
		, valueLabelWidth(ui::ScalePixels(edValueLabelWidth, hWnd))
		, paddingX(ui::ScalePixels(edPaddingX, hWnd))
		, paddingY(ui::ScalePixels(edPaddingY, hWnd))
		, paddingTop(ui::ScalePixels(edPaddingTop, hWnd))
		, rowHeight(ui::ScalePixels(edRowHeight, hWnd))
		, footerHeight(ui::ScalePixels(edFooterHeight, hWnd))
		, scrollbarWidth(ui::ScalePixels(edScrollbarWidth, hWnd))
	{
	}


	Rect RowLabelRect(const Rect rect) const noexcept
	{
		return Rect{rect.left, rect.top, rect.right, rect.top + rowHeight};
	}

	Rect ValueLabelRect(const Rect rect) const noexcept
	{
		return Rect{rect.right - valueLabelWidth, rect.top, rect.right, rect.top + sliderHeight};
	}

	Rect ValueSliderRect(const Rect rect) const noexcept
	{
		return Rect{rect.left + rowLabelWidth, rect.top, rect.right - valueLabelWidth, rect.top + sliderHeight};
	}
};



UI_MESSAGE_MAP_BEGIN(CTempoSwingDlg, DialogBase)
	UI_COMMAND(IDC_BUTTON1, &CTempoSwingDlg::OnReset)
	UI_COMMAND(IDC_BUTTON2, &CTempoSwingDlg::OnUseGlobal)
	UI_COMMAND(IDC_CHECK1,  &CTempoSwingDlg::OnToggleGroup)
	UI_NOTIFY(ui::EditChange, IDC_EDIT1, &CTempoSwingDlg::OnGroupChanged)
UI_MESSAGE_MAP_END()

int CTempoSwingDlg::m_groupSize = 1;

CTempoSwingDlg::CTempoSwingDlg(Wnd *parent, const TempoSwing &currentTempoSwing, CTrackerSoundFile &sndFile, PATTERNINDEX pattern)
	: DialogBase(IDD_TEMPO_SWING, parent)
	, m_container(*this)
	, m_scrollPos(0)
	, m_tempoSwing(currentTempoSwing)
	, m_origTempoSwing(pattern == PATTERNINDEX_INVALID ? sndFile.m_tempoSwing : sndFile.Patterns[pattern].GetTempoSwing())
	, m_sndFile(sndFile)
	, m_pattern(pattern)
{
	m_groupSize = std::min(m_groupSize, static_cast<int>(m_tempoSwing.size()));
}


void CTempoSwingDlg::DoDataExchange(DataExchange* pDX)
{
	DialogBase::DoDataExchange(pDX);
	pDX->BindControl(IDC_CHECK1, m_checkGroup);
	pDX->BindControl(IDC_SCROLLBAR1, m_scrollBar);
	pDX->BindControl(IDC_CONTAINER, m_container);
}


bool CTempoSwingDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();
	
	TempoSwingMeasurements m{this};
	Rect windowRect, rect;
	GetWindowRect(windowRect);
	GetClientRect(rect);
	windowRect.bottom = windowRect.top + windowRect.Height() - rect.Height();

	Rect mainWindowRect;
	CMainFrame::GetMainFrame()->GetClientRect(mainWindowRect);

	const int realHeight = static_cast<int>(m_tempoSwing.size()) * m.rowHeight;
	const int displayHeight = std::min(realHeight, static_cast<int>(mainWindowRect.bottom - windowRect.Height() - m.paddingTop - m.footerHeight));

	Rect containerRect;
	m_container.GetClientRect(containerRect);
	containerRect.bottom = displayHeight;
	m_container.SetWindowPos(nullptr, 0, m.paddingTop, rect.right - m.scrollbarWidth, containerRect.bottom, ui::PosNoZOrder);

	// Need scrollbar?
	if(realHeight > displayHeight)
	{
		SCROLLINFO info;
		info.cbSize = sizeof(info);
		info.fMask = SIF_ALL;
		info.nMin = 0;
		info.nMax = realHeight;
		info.nPage = displayHeight;
		info.nTrackPos = info.nPos = 0;
		m_scrollBar.SetScrollInfo(&info, false);

		Rect scrollRect;
		m_scrollBar.GetClientRect(scrollRect);
		m_scrollBar.SetWindowPos(nullptr, containerRect.right, m.paddingTop, scrollRect.Width(), displayHeight, ui::PosNoZOrder);
	} else
	{
		m_scrollBar.ShowWindow(false);
	}

	rect.DeflateRect(m.paddingX, 0, m.paddingX + m.scrollbarWidth, 0);

	GetDlgItem(IDC_BUTTON2)->ShowWindow((m_pattern != PATTERNINDEX_INVALID) ? true : false);

	m_controls.resize(m_tempoSwing.size());
	const ui::Font font = GetFont();
	for(size_t i = 0; i < m_controls.size(); i++)
	{
		m_controls[i] = std::make_unique<RowCtls>();
		auto &r = m_controls[i];
		// Row label
		r->rowLabel.CreateChild(m_container, m.RowLabelRect(rect), 0);
		r->rowLabel.SetWindowText(MPT_UFORMAT("Row {}:")(i + 1));
		r->rowLabel.SetFont(font);

		// Value label
		r->valueLabel.CreateChild(m_container, m.ValueLabelRect(rect), 0);
		r->valueLabel.SetWindowText(UL_("100%"));
		r->valueLabel.SetFont(font);

		// Value slider
		r->valueSlider.CreateChild(m_container, m.ValueSliderRect(rect), 0xFFFF);
		r->valueSlider.SetFont(font);
		r->valueSlider.SetRange(-SliderResolution / 2, SliderResolution / 2);
		r->valueSlider.SetTicFreq(SliderResolution / 8);
		r->valueSlider.SetPageSize(SliderResolution / 8);
		r->valueSlider.SetUserData(static_cast<intptr_t>(i));
		r->SetValue(m_tempoSwing[i]);
		rect.MoveToY(rect.top + m.rowHeight);
	}

	static_cast<Spinner *>(GetDlgItem(IDC_EDIT1))->SetRange32(1, static_cast<int>(m_tempoSwing.size()));
	SetDlgItemInt(IDC_EDIT1, m_groupSize);
	OnToggleGroup();

	m_container.OnHScroll(0, 0, &m_controls[0]->valueSlider);
	rect.MoveToY(m.paddingTop + containerRect.bottom + m.paddingY);
	{
		// Buttons at dialog bottom
		const int cxEdge = ui::ScrollBarSize;
		Rect buttonRect;
		for(const uint32 i : { static_cast<uint32>(IDOK), static_cast<uint32>(IDCANCEL), static_cast<uint32>(IDC_BUTTON2) })
		{
			auto wnd = GetDlgItem(i);
			wnd->GetWindowRect(buttonRect);
			wnd->SetWindowPos(nullptr, buttonRect.left - windowRect.left - cxEdge, rect.top, 0, 0, ui::PosNoSize | ui::PosNoActivate | ui::PosNoZOrder);
		}
	}

	windowRect.bottom += displayHeight + m.paddingTop + m.footerHeight;
	SetWindowPos(nullptr, 0, 0, windowRect.Width(), windowRect.Height(), ui::PosNoMove | ui::PosNoActivate | ui::PosNoZOrder);

	return true;
}


void CTempoSwingDlg::OnDPIChanged()
{
	DialogBase::OnDPIChanged();

	TempoSwingMeasurements m{this};
	const ui::Font font = GetFont();
	Rect rect;
	m_container.GetClientRect(rect);
	rect.DeflateRect(m.paddingX, 0, m.paddingX + m.scrollbarWidth, 0);
	for(auto &r: m_controls)
	{
		r->rowLabel.SetFont(font);
		r->valueLabel.SetFont(font);
		r->valueSlider.SetFont(font);

		m_container.MoveChild(r->rowLabel, m.RowLabelRect(rect));
		m_container.MoveChild(r->valueLabel, m.ValueLabelRect(rect));
		m_container.MoveChild(r->valueSlider, m.ValueSliderRect(rect));
		rect.MoveToY(rect.top + m.rowHeight);
	}
}


void CTempoSwingDlg::OnOK()
{
	DialogBase::OnOK();
	// If this is the default setup, just clear the vector.
	if(m_pattern == PATTERNINDEX_INVALID)
	{
		if(static_cast<size_t>(std::count(m_tempoSwing.begin(), m_tempoSwing.end(), static_cast<TempoSwing::value_type>(TempoSwing::Unity))) == m_tempoSwing.size())
		{
			m_tempoSwing.clear();
		}
	} else
	{
		if(m_tempoSwing == m_sndFile.m_tempoSwing)
		{
			m_tempoSwing.clear();
		}
	}
	OnClose();
}


void CTempoSwingDlg::OnCancel()
{
	DialogBase::OnCancel();
	OnClose();
}


void CTempoSwingDlg::OnClose()
{
	// Restore original swing properties after preview
	if(m_pattern == PATTERNINDEX_INVALID)
	{
		m_sndFile.m_tempoSwing = m_origTempoSwing;
	} else
	{
		m_sndFile.Patterns[m_pattern].SetTempoSwing(m_origTempoSwing);
	}
}


void CTempoSwingDlg::OnReset()
{
	for(auto &control : m_controls)
	{
		control->valueSlider.SetPos(0);
	}
	m_container.OnHScroll(0, 0, &m_controls[0]->valueSlider);
}


void CTempoSwingDlg::OnUseGlobal()
{
	if(m_sndFile.m_tempoSwing.empty())
	{
		OnReset();
		return;
	}
	for(size_t i = 0; i < m_controls.size(); i++)
	{
		m_controls[i]->SetValue(m_sndFile.m_tempoSwing[i % m_sndFile.m_tempoSwing.size()]);
	}
	m_container.OnHScroll(0, 0, reinterpret_cast<ScrollBar *>(&(m_controls[0]->valueSlider)));
}


void CTempoSwingDlg::OnToggleGroup()
{
	const bool checked = m_checkGroup.GetCheck() != ui::CheckOff;
	GetDlgItem(IDC_EDIT1)->EnableWindow(checked);
}


void CTempoSwingDlg::OnGroupChanged()
{
	int val = GetDlgItemInt(IDC_EDIT1);
	if(val > 0) m_groupSize = std::min(val, static_cast<int>(m_tempoSwing.size()));
}


void CTempoSwingDlg::OnVScroll(uint32 nSBCode, uint32 nPos, Wnd *pScrollBarWnd)
{
	if(pScrollBarWnd == &m_scrollBar)
	{
		ScrollBar *pScrollBar = &m_scrollBar;
		// Get the minimum and maximum scrollbar positions.
		int minpos;
		int maxpos;
		pScrollBar->GetScrollRange(&minpos, &maxpos);

		SCROLLINFO sbInfo;
		pScrollBar->GetScrollInfo(&sbInfo);

		// Get the current position of scroll box.
		int curpos = pScrollBar->GetScrollPos();

		// Determine the new position of scroll box.
		switch(nSBCode)
		{
		case ui::ScrollLeft:			// Scroll to far left.
			curpos = minpos;
			break;

		case ui::ScrollRight:			// Scroll to far right.
			curpos = maxpos;
			break;

		case ui::ScrollEndScroll:		// End scroll.
			m_container.Invalidate();
			break;

		case ui::ScrollLineLeft:		// Scroll left.
			if(curpos > minpos)
				curpos--;
			break;

		case ui::ScrollLineRight:		// Scroll right.
			if(curpos < maxpos)
				curpos++;
			break;

		case ui::ScrollPageLeft:		// Scroll one page left.
			if(curpos > minpos)
			{
				curpos = std::max(minpos, curpos - static_cast<int>(sbInfo.nPage));
			}
			break;

		case ui::ScrollPageRight:		// Scroll one page right.
			if(curpos < maxpos)
			{
				curpos = std::min(maxpos, curpos + static_cast<int>(sbInfo.nPage));
			}
			break;

		case ui::ScrollThumbPosition:	// Scroll to absolute position. nPos is the position
			curpos = nPos;		// of the scroll box at the end of the drag operation.
			break;

		case ui::ScrollThumbTrack:		// Drag scroll box to specified position. nPos is the
			curpos = nPos;		// position that the scroll box has been dragged to.
			break;
		}

		// Set the new position of the thumb (scroll box).
		pScrollBar->SetScrollPos(curpos);

		m_container.ScrollChildren(0, m_scrollPos - curpos);
		m_scrollPos = curpos;
	}

	DialogBase::OnVScroll(nSBCode, nPos, pScrollBarWnd);
}


// Scrollable container for the sliders
UI_MESSAGE_MAP_BEGIN(CTempoSwingDlg::SliderContainer, Panel)
UI_MESSAGE_MAP_END()


void CTempoSwingDlg::SliderContainer::OnHScroll(uint32 nSBCode, uint32 nPos, Wnd *pScrollBar)
{
	if(m_parent.m_checkGroup.GetCheck() != ui::CheckOff)
	{
		// Edit groups
		size_t editedGroup = 0;
		int editedValue = static_cast<HSlider *>(pScrollBar)->GetPos();
		for(size_t i = 0; i < m_parent.m_controls.size(); i++)
		{
			if(&m_parent.m_controls[i]->valueSlider == pScrollBar)
			{
				editedGroup = (i / m_parent.m_groupSize) % 2u;
				break;
			}
		}
		for(size_t i = 0; i < m_parent.m_controls.size(); i++)
		{
			if((i / m_parent.m_groupSize) % 2u == editedGroup)
			{
				m_parent.m_controls[i]->valueSlider.SetPos(editedValue);
			}
		}
	}

	for(size_t i = 0; i < m_parent.m_controls.size(); i++)
	{
		m_parent.m_tempoSwing[i] = m_parent.m_controls[i]->GetValue();
	}
	m_parent.m_tempoSwing.Normalize();
	// Apply preview
	if(m_parent.m_pattern == PATTERNINDEX_INVALID)
	{
		m_parent.m_sndFile.m_tempoSwing = m_parent.m_tempoSwing;
	} else
	{
		m_parent.m_sndFile.Patterns[m_parent.m_pattern].SetTempoSwing(m_parent.m_tempoSwing);
	}

	for(size_t i = 0; i < m_parent.m_tempoSwing.size(); i++)
	{
		const int32 percent = Util::muldivr(m_parent.m_tempoSwing[i], 100, TempoSwing::Unity);
		m_parent.m_controls[i]->valueLabel.SetWindowText(ui::Format(UL_("%i%%"), percent));
		const int32 delta = percent - 100;
		m_parent.m_controls[i]->valueSlider.SetToolTipText(ui::Format(UL_("%s%d"), delta > 0 ? UL_("+") : UL_(""), delta));
	}

	Panel::OnHScroll(nSBCode, nPos, pScrollBar);
}


OPENMPT_NAMESPACE_END
