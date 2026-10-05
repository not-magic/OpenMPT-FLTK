/*
 * ScaleEnvPointsDlg.h
 * -------------------
 * Purpose: Dialog for scaling instrument envelope points on x and y axis.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/ScaleEnvPointsDlg.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "CDecimalSupport.h"
#include "DialogBase.h"

OPENMPT_NAMESPACE_BEGIN

struct InstrumentEnvelope;

class CScaleEnvPointsDlg : public DialogBase
{
protected:
	CNumberEdit m_EditX, m_EditY, m_EditOffset;
	InstrumentEnvelope &m_Env;
	static double m_factorX, m_factorY, m_offsetY;
	const int m_nCenter;

public:
	CScaleEnvPointsDlg(Wnd* pParent, InstrumentEnvelope &env, int nCenter);

	void Apply();

protected:
	void OnOK() override;
	bool OnInitDialog() override;
};

OPENMPT_NAMESPACE_END
