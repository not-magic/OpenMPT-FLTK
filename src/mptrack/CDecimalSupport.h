/*
 * CDecimalSupport.h
 * -----------------
 * Purpose: Edit field which allows negative and fractional values to be entered
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"
#include "../soundlib/Snd_defs.h"

OPENMPT_NAMESPACE_BEGIN

// Converts text to a number. Returns false if the text is not a number.
bool ParseDecimalValue(const mpt::ustring &text, double &value);


class CNumberEdit : public Edit
{
public:
	void ConfigureFromTemplate(const DialogControl &control) override;

	void AllowNegative(bool allow) { m_allowNegative = allow; }
	void AllowFractions(bool allow);

	// Converts the text of the control to a number. Returns false if the text is not a number.
	bool GetDecimalValue(double &value) const;
	// Shows a number with a given count of digits after the decimal point; trailing zeros are removed.
	void SetFixedValue(double value, int digitCount);
	// Shows a number with a given count of significant digits; trailing zeros are removed.
	void SetDecimalValue(double value, int digitCount);
	void SetDecimalValue(double value);

	void SetTempoValue(const TEMPO &t);
	TEMPO GetTempoValue();

	int handle(int event) override;

private:
	bool m_allowNegative = true;
	bool m_allowFractions = true;
};

OPENMPT_NAMESPACE_END
