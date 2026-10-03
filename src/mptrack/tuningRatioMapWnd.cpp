/*
 * tuningRatioMapWnd.cpp
 * ---------------------
 * Purpose: Alternative sample tuning configuration dialog - ratio map edit control.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "ui/Ui.h"
#include "tuningRatioMapWnd.h"
#include "Mainfrm.h"
#include "Mptrack.h"
#include "TuningDialog.h"
#include "../soundlib/tuning.h"


OPENMPT_NAMESPACE_BEGIN


UI_MESSAGE_MAP_BEGIN(CTuningRatioMapWnd, Static)
UI_MESSAGE_MAP_END()


void CTuningRatioMapWnd::Init(CTuningDialog* const pParent, CTuning* const tuning)
{
	m_pParent = pParent;
	m_pTuning = tuning;
}

void CTuningRatioMapWnd::OnPaint(ui::Painter &dc)
{
	if(!m_pTuning)
		return;

	const Rect rcClient = GetClientRect();

	const auto colorText = ui::GetSystemColor(ui::SysColor::WindowText);
	const auto colorTextSel = ui::GetSystemColor(ui::SysColor::HighlightText);
	const auto colorHighlight = ui::GetSystemColor(ui::SysColor::Highlight);
	const auto colorWindow = ui::GetSystemColor(ui::SysColor::Window);
	const int lineWidth = ui::ScalePixels(1, *this);

	m_font = ui::GetGuiFont();

	dc.SetFont(m_font);
	dc.SetBkTransparent(true);
	if ((m_cxFont <= 0) || (m_cyFont <= 0))
	{
		const Size sz = dc.GetTextExtent(UL_("123456789"));
		m_cyFont = sz.cy + 2;
		m_cxFont = rcClient.right / 4;
	}
	dc.PushClip(rcClient);
	if ((m_cxFont > 0) && (m_cyFont > 0))
	{
		const bool focus = HasFocus();
		Rect rect;

		NOTEINDEXTYPE nNotes = static_cast<NOTEINDEXTYPE>((rcClient.bottom + m_cyFont - 1) / m_cyFont);
		NOTEINDEXTYPE nPos = m_nNote - (nNotes/2);
		int ypaint = 0;

		for (int ynote=0; ynote<nNotes; ynote++, ypaint+=m_cyFont, nPos++)
		{
			// Note
			NOTEINDEXTYPE noteToDraw = nPos - m_nNoteCentre;
			const bool isValidNote = m_pTuning->IsValidNote(noteToDraw);

			rect.SetRect(0, ypaint, m_cxFont, ypaint + m_cyFont);
			const auto noteStr = isValidNote ? mpt::ufmt::val(noteToDraw) : mpt::ustring(UL_("..."));
			DrawButtonRect(dc, lineWidth, m_font, rect, noteStr, false, false);

			// Mapped Note
			const bool highLight = focus && (nPos == (int)m_nNote);
			rect.left = rect.right;
			rect.right = m_cxFont*4-1;
			dc.FillSolidRect(rect, highLight ? colorHighlight : colorWindow);
			if(nPos == (int)m_nNote)
			{
				rect.InflateRect(-1, -1);
				dc.DrawFocusRect(rect);
				rect.InflateRect(1, 1);
			}
			dc.SetTextColor(highLight ? colorTextSel : colorText);

			rect.SetRect(m_cxFont * 1, ypaint, m_cxFont * 2 - 1, ypaint + m_cyFont);
			dc.DrawText(mpt::ToUnicode(m_pTuning->GetNoteName(noteToDraw)), rect, ui::TextSingleLine | ui::TextCenter | ui::TextVCenter | ui::TextNoPrefix);

			rect.SetRect(m_cxFont * 2, ypaint, m_cxFont * 3 - 1, ypaint + m_cyFont);
			dc.DrawText(mpt::ufmt::flt(m_pTuning->GetRatio(noteToDraw), 6), rect, ui::TextSingleLine | ui::TextCenter | ui::TextVCenter | ui::TextNoPrefix);

			rect.SetRect(m_cxFont * 3, ypaint, m_cxFont * 4 - 1, ypaint + m_cyFont);
			dc.DrawText(mpt::ufmt::fix(std::log2(static_cast<double>(m_pTuning->GetRatio(noteToDraw))) * 1200.0, 1), rect, ui::TextSingleLine | ui::TextCenter | ui::TextVCenter | ui::TextNoPrefix);

		}
		rect.SetRect(rcClient.left + m_cxFont * 4 - 1, rcClient.top, rcClient.left + m_cxFont * 4 + 3, ypaint);
		DrawButtonRect(dc, lineWidth, m_font, rect, UL_(""));
		if (ypaint < rcClient.bottom)
		{
			rect.SetRect(rcClient.left, ypaint, rcClient.right, rcClient.bottom);
			dc.FillSolidRect(rect, ui::GetSystemColor(ui::SysColor::ButtonFace));
		}
	}
}

void CTuningRatioMapWnd::OnSetFocus(Wnd *pOldWnd)
{
	Wnd::OnSetFocus(pOldWnd);
	InvalidateRect(NULL, false);
}


void CTuningRatioMapWnd::OnKillFocus(Wnd *pNewWnd)
{
	Wnd::OnKillFocus(pNewWnd);
	InvalidateRect(NULL, false);
}


bool CTuningRatioMapWnd::OnMouseWheel(uint32 nFlags, int16 zDelta, Point pt)
{
	NOTEINDEXTYPE note = static_cast<NOTEINDEXTYPE>(m_nNote - mpt::signum(zDelta));
	if(m_pTuning->IsValidNote(note - m_nNoteCentre))
	{
		m_nNote = note;
		InvalidateRect(NULL, false);
		if(m_pParent)
			m_pParent->UpdateRatioMapEdits(GetShownCentre());
	}

	return Wnd::OnMouseWheel(nFlags, zDelta, pt);
}


void CTuningRatioMapWnd::OnLButtonDown(uint32, Point pt)
{
	if ((pt.x >= m_cxFont) && (pt.x < m_cxFont*2))
	{
		InvalidateRect(NULL, false);
	}
	if ((pt.x > m_cxFont*2) && (pt.x <= m_cxFont*3))
	{
		InvalidateRect(NULL, false);
	}
	if ((pt.x >= 0) && (m_cyFont))
	{
		const Rect rcClient = GetClientRect();
		int nNotes = (rcClient.bottom + m_cyFont - 1) / m_cyFont;
		const int n = (pt.y / m_cyFont) + m_nNote - (nNotes/2);
		const NOTEINDEXTYPE note = static_cast<NOTEINDEXTYPE>(n - m_nNoteCentre);
		if(m_pTuning->IsValidNote(note))
		{
			m_nNote = static_cast<NOTEINDEXTYPE>(n);
			InvalidateRect(NULL, false);
			if(m_pParent)
				m_pParent->UpdateRatioMapEdits(GetShownCentre());
		}

	}
	SetFocus();
}



NOTEINDEXTYPE CTuningRatioMapWnd::GetShownCentre() const
{
	return m_nNote - m_nNoteCentre;
}


OPENMPT_NAMESPACE_END
