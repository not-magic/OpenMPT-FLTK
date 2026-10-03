// FLTK port of openmpt/mptrack/ColourEdit.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "ColourEdit.h"


OPENMPT_NAMESPACE_BEGIN


CColourEdit::CColourEdit()
{
	m_crText = RGB(0, 0, 0);  //default text color
}


void CColourEdit::SetBackColor(ColorRef rgb)
{
	m_crBackGnd = rgb;
	color(fl_rgb_color(GetRValue(rgb), GetGValue(rgb), GetBValue(rgb)));
	Invalidate(true);
}


void CColourEdit::SetTextColor(ColorRef rgb)
{
	m_crText = rgb;
	textcolor(fl_rgb_color(GetRValue(rgb), GetGValue(rgb), GetBValue(rgb)));
	Invalidate(true);
}


OPENMPT_NAMESPACE_END
