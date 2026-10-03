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
