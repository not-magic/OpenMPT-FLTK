/*
 * ColourEdit.cpp
 * --------------
 * Purpose: Edit control with configurable text and background colours.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


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
