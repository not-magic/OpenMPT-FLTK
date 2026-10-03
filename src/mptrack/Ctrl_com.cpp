// FLTK port of openmpt/mptrack/Ctrl_com.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "Ctrl_com.h"
#include "Globals.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "view_com.h"
#include "../soundlib/mod_specifications.h"
#include "mpt/format/join.hpp"
#include "mpt/string/utility.hpp"


//#define MPT_COMMENTS_LONG_LINES_WRAP
//#define MPT_COMMENTS_LONG_LINES_TRUNCATE


#define MPT_COMMENTS_MARGIN 4



OPENMPT_NAMESPACE_BEGIN


UI_MESSAGE_MAP_BEGIN(CCtrlComments, CModControlDlg)
	UI_NOTIFY(ui::EditUpdate, IDC_EDIT_COMMENTS, &CCtrlComments::OnCommentsUpdated)
	UI_NOTIFY(ui::EditChange, IDC_EDIT_COMMENTS, &CCtrlComments::OnCommentsChanged)
UI_MESSAGE_MAP_END()

void CCtrlComments::DoDataExchange(DataExchange* pDX)
{
	CModControlDlg::DoDataExchange(pDX);
	pDX->BindControl(IDC_EDIT_COMMENTS, m_EditComments);
}


CCtrlComments::CCtrlComments(CModControlView &parent, CModDoc &document) : CModControlDlg(parent, document)
{
}


ViewType CCtrlComments::GetAssociatedViewType()
{
	return ViewType::Comments;
}


void CCtrlComments::OnActivatePage(LParam)
{
	// Don't stop generating VU meter messages
	m_modDoc.SetNotifications(Notification::Default);
	m_modDoc.SetFollowWnd(this);
	m_EditComments.SetFocus();
}


void CCtrlComments::OnDeactivatePage()
{
	CModControlDlg::OnDeactivatePage();
}


bool CCtrlComments::OnInitDialog()
{
	CModControlDlg::OnInitDialog();
	// Initialize comments
	uint32 margin = ui::ScalePixels(MPT_COMMENTS_MARGIN, (&m_EditComments));
	m_EditComments.SetMargins(margin, margin);
	UpdateView(CommentHint().ModType());
	m_EditComments.SetFocus();
	m_initialized = true;
	return false;
}


Setting<int32> &CCtrlComments::GetSplitPosRef() { return TrackerSettings::Instance().glCommentsWindowHeight; }


void CCtrlComments::RecalcLayout()
{
	Rect rcClient, rect;
	int cx0, cy0;
	
	if(m_EditComments.GetParent() == nullptr) return;
	GetClientRect(&rcClient);
	m_EditComments.GetWindowRect(&rect);
	ScreenToClient(&rect);
	cx0 = rect.Width();
	cy0 = rect.Height();
	rect.bottom = rcClient.bottom - 3;
	rect.right = rcClient.right - rect.left;
	if ((rect.right > rect.left) && (rect.bottom > rect.top))
	{
		int cx = rect.Width(), cy = rect.Height();
		if(m_sndFile.GetModSpecifications().commentLineLengthMax != 0)
		{
			const int cxBorder = 1;
			const int cxVScroll = ui::ScrollBarSize;
			int cxmax = cxBorder + m_sndFile.GetModSpecifications().commentLineLengthMax * charWidth + cxVScroll + cxBorder + ui::ScalePixels(MPT_COMMENTS_MARGIN + MPT_COMMENTS_MARGIN - 1, (&m_EditComments));
			if(cx != cxmax && cxmax != 0)
				cx = cxmax;
			//SetWindowLong((&m_EditComments), GWL_STYLE, GetWindowLong((&m_EditComments), GWL_STYLE) & ~WS_HSCROLL);
		} else
		{
			//SetWindowLong((&m_EditComments), GWL_STYLE, GetWindowLong((&m_EditComments), GWL_STYLE) | WS_HSCROLL);
		}
		if(cx != cx0 || cy != cy0)
			m_EditComments.SetWindowPos(NULL, 0,0, cx, cy, ui::PosNoMove|ui::PosNoZOrder|ui::PosDrawFrame);
	}
}


void CCtrlComments::OnDPIChanged()
{
	UpdateView(UpdateHint().MPTOptions(), nullptr);
	CModControlDlg::OnDPIChanged();
}


void CCtrlComments::UpdateView(UpdateHint hint, HintObject *pHint)
{
	CommentHint commentHint = hint.ToType<CommentHint>();
	if(pHint == this || !commentHint.GetType()[HINT_MODCOMMENTS | HINT_MPTOPTIONS | HINT_MODTYPE])
		return;
	if(IsLocked())
		return;
	LockControls();

	ui::Font &hFont = CMainFrame::GetMainFrame()->GetCommentsFont();
	static FontSetting previousFont;
	static int previousFontSize = 0;
	FontSetting font = TrackerSettings::Instance().commentsFont;
	// Point size to pixels
	int32 fontSize = MulDiv(font.size, ui::kLogicalDpi, 720);
	if(previousFont != font || previousFontSize != fontSize)
	{
		previousFont = font;
		previousFontSize = fontSize;
		hFont = ui::CreateFont(font.name, fontSize, font.flags[FontSetting::Bold], font.flags[FontSetting::Italic], true);
	}
	m_EditComments.SetFont(hFont);
	{
		ui::Painter dc;
		dc.SetFont(hFont);
		charWidth = dc.GetTextExtent(UL_("0")).cx;
	}

	RecalcLayout();

	m_EditComments.SetRedraw(false);

	std::string text = m_sndFile.m_songMessage.GetFormatted(SongMessage::leLF);
	for(std::size_t i = 0; i < text.length(); ++i)
	{
		// replace control characters
		char c = text[i];
		if(c > '\0' && c < ' ' && c != '\n')
		{
			c = ' ';
		}
		text[i] = c;
	}
	mpt::ustring new_text = mpt::ToUnicode(m_sndFile.GetCharsetInternal(), text);
	mpt::ustring old_text;
	m_EditComments.GetWindowText(old_text);
	if(new_text != old_text)
	{
		m_EditComments.SetWindowText(new_text);
	}

	if(commentHint.GetType() & HINT_MODTYPE)
	{
		m_EditComments.SetReadOnly(!m_sndFile.GetModSpecifications().hasComments);
	}

	m_EditComments.SetRedraw(true);
	UnlockControls();
}


void CCtrlComments::OnCommentsUpdated()
{

#if defined(MPT_COMMENTS_LONG_LINES_TRUNCATE) || defined(MPT_COMMENTS_LONG_LINES_WRAP)

	if(m_Reformatting)
	{
		return;
	}

	if(!m_sndFile.GetModSpecifications().hasComments)
	{
		return;
	}
	if(m_sndFile.GetModSpecifications().commentLineLengthMax == 0)
	{
		return;
	}

	m_Reformatting = true;
	const std::size_t maxline = m_sndFile.GetModSpecifications().commentLineLengthMax;
	int beg = 0;
	int end = 0;
	m_EditComments.GetSel(beg, end);
	mpt::ustring text;
	m_EditComments.GetWindowText(text);
	std::string lines_new;
	lines_new.reserve(text.length());
	bool modified = false;
	std::size_t pos = 0;

#if defined(MPT_COMMENTS_LONG_LINES_WRAP)

	std::string lines = text.c_str();
	std::size_t line_length = 0;
	for(std::size_t i = 0; i < lines.length(); ++i)
	{
		if(lines[i] == '\r')
		{
			// nothing
		} else if (lines[i] == '\n')
		{
			line_length = 0;
		} else
		{
			line_length += 1;
		}
		if(line_length > maxline)
		{
			modified = true;
			lines_new.push_back('\r');
			lines_new.push_back('\n');
			if(beg >= 0)
			{
				if(beg >= pos)
				{
					beg += 2;
				}
			}
			if(end >= 0)
			{
				if(end >= pos)
				{
					end += 2;
				}
			}
			pos += 2;
			line_length = 1;
		}
		lines_new.push_back(lines[i]);
		pos++;
	}

#elif defined(MPT_COMMENTS_LONG_LINES_TRUNCATE)

	std::vector<std::string> lines = mpt::split(std::string(text.c_str()), std::string("\r\n"));
	for(std::size_t i = 0; i < lines.size(); ++i)
	{
		if(i > 0)
		{
			pos += 2;
		}
		if(lines[i].length() > maxline)
		{
			modified = true;
			pos += maxline;
			for(std::size_t n = 0; n < lines[i].length() - maxline; ++n)
			{
				if(beg >= 0)
				{
					if(beg > pos)
					{
						beg--;
					}
				}
				if(end >= 0)
				{
					if(end > pos)
					{
						end--;
					}
				}
			}
			lines[i] = lines[i].substr(0, maxline);
		} else
		{
			pos += lines[i].length();
		}
	}
	lines_new = mpt::join_format(lines, std::string("\r\n"));

#endif

	if(modified)
	{
		text = lines_new.c_str();
		m_EditComments.SetWindowText(text);
		m_EditComments.SetSel(beg, end);
	}
	m_Reformatting = false;

#endif

}


void CCtrlComments::OnCommentsChanged()
{
	if(m_nLockCount)
		return;
	if(!m_initialized || !(&m_EditComments) || !m_EditComments.GetModify())
		return;
	mpt::ustring text;
	m_EditComments.GetWindowText(text);
	m_EditComments.SetModify(false);
	if(m_sndFile.m_songMessage.SetFormatted(mpt::ToCharset(m_sndFile.GetCharsetInternal(), text), SongMessage::leLF))
	{
		m_modDoc.SetModified();
		m_modDoc.UpdateAllViews(nullptr, CommentHint(), this);
	}
}


bool CCtrlComments::PreTranslateMessage(int event)
{
	if(event == FL_KEYBOARD && ui::KeyFromEvent() == 'A' && ui::IsKeyDown(ui::Key_CONTROL))
	{
		// Ctrl-A is not handled by multiline edit boxes
		if(Wnd::GetFocus() == (&m_EditComments))
			m_EditComments.SetSel(0, -1);
	} else if(event == FL_KEYBOARD && ui::KeyFromEvent() == ui::Key_TAB && CInputHandler::GetModifierMask() == ModNone)
	{
		int selStart, selEnd;
		m_EditComments.GetSel(selStart, selEnd);
		int posInLine = (selStart - m_EditComments.LineIndex(m_EditComments.LineFromChar(selStart)));
		mpt::ustring tabs(4 - (posInLine % 4), UL_(' '));
		m_EditComments.ReplaceSel(tabs);
		return true;
	}
	return CModControlDlg::PreTranslateMessage(event);
}


OPENMPT_NAMESPACE_END
