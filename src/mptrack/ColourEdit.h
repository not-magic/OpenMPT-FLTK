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
