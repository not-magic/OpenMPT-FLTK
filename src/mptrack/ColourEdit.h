/*
 * ColourEdit.h
 * ------------
 * Purpose: Implementation of a coloured edit UI item.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/ColourEdit.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

OPENMPT_NAMESPACE_BEGIN

class CColourEdit : public Edit
{
public:
	CColourEdit();

public:
	void SetTextColor(ColorRef rgb);
	void SetBackColor(ColorRef rgb);

private:
	ColorRef m_crText;
	ColorRef m_crBackGnd = RGB(255, 255, 255);
};

OPENMPT_NAMESPACE_END
