/*
 * CDecimalSupport.cpp
 * -------------------
 * Purpose: Edit field which allows negative and fractional values to be entered
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "ui/Ui.h"
#include "CDecimalSupport.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

OPENMPT_NAMESPACE_BEGIN

namespace
{

constexpr int kMaximumSignificantDigits = 17;
constexpr uint32 kNumberStyle = 0x2000;

std::string FormatTrimmed(double value, int digitCount)
{
	char buffer[400];
	std::snprintf(buffer, sizeof(buffer), "%.*f", std::max(digitCount, 0), value);
	std::string text = buffer;
	if(text.find('.') != std::string::npos)
	{
		while(!text.empty() && text.back() == '0')
			text.pop_back();
		if(!text.empty() && text.back() == '.')
			text.pop_back();
	}
	if(text == "-0")
		text = "0";
	return text;
}

}  // namespace


void CNumberEdit::ConfigureFromTemplate(const DialogControl &control)
{
	Edit::ConfigureFromTemplate(control);
	if(control.style & kNumberStyle)
		AllowFractions(m_allowFractions);
}


void CNumberEdit::AllowFractions(bool allow)
{
	m_allowFractions = allow;
	type(allow ? FL_FLOAT_INPUT : FL_INT_INPUT);
}


bool ParseDecimalValue(const mpt::ustring &textU, double &value)
{
	const std::string text = mpt::transcode<std::string>(mpt::common_encoding::utf8, textU);
	char *end = nullptr;
	value = std::strtod(text.c_str(), &end);
	return end != nullptr && end != text.c_str() && *end == '\0';
}


bool CNumberEdit::GetDecimalValue(double &value) const
{
	return ParseDecimalValue(GetWindowText(), value);
}


void CNumberEdit::SetFixedValue(double value, int digitCount)
{
	SetWindowText(mpt::transcode<mpt::ustring>(mpt::common_encoding::utf8, FormatTrimmed(value, digitCount)));
}


void CNumberEdit::SetDecimalValue(double value, int digitCount)
{
	const int integerDigits = (value == 0.0) ? 1 : std::max(1, static_cast<int>(std::floor(std::log10(std::fabs(value)))) + 1);
	SetFixedValue(value, digitCount - integerDigits);
}


void CNumberEdit::SetDecimalValue(double value)
{
	SetDecimalValue(value, kMaximumSignificantDigits);
}


void CNumberEdit::SetTempoValue(const TEMPO &t)
{
	SetFixedValue(t.ToDouble(), 4);
}


TEMPO CNumberEdit::GetTempoValue()
{
	double d = 0.0;
	GetDecimalValue(d);
	return TEMPO(d);
}


int CNumberEdit::handle(int event)
{
	if(event == FL_KEYBOARD && Fl::event_length() == 1)
	{
		const char key = Fl::event_text()[0];
		if(key == '-' && !m_allowNegative)
			return 1;
		if((key == '.' || key == ',') && !m_allowFractions)
			return 1;
		// Accept the decimal comma of locales that use one
		if(key == ',' && m_allowFractions)
		{
			insert(".");
			do_callback();
			return 1;
		}
	}
	return Edit::handle(event);
}

OPENMPT_NAMESPACE_END
