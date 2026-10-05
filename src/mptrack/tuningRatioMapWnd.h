/*
 * tuningRatioMapWnd.h
 * -------------------
 * Purpose: Alternative sample tuning configuration dialog - ratio map edit control.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/tuningRatioMapWnd.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "../soundlib/tuning.h"
#include "../soundlib/modcommand.h"

OPENMPT_NAMESPACE_BEGIN

class CTuningDialog;

using NOTEINDEXTYPE = Tuning::NOTEINDEXTYPE;

//Copied from CNoteMapWnd.
class CTuningRatioMapWnd: public Static
{
	friend class CTuningDialog;
protected:
	const CTuning* m_pTuning = nullptr;
	CTuningDialog* m_pParent = nullptr;

	ui::Font m_font;
	int m_cxFont = 0, m_cyFont = 0;
	NOTEINDEXTYPE m_nNote = NOTE_MIDDLEC;

	NOTEINDEXTYPE m_nNoteCentre = NOTE_MIDDLEC;


public:
	void Init(CTuningDialog* const pParent, CTuning* const tuning);

	NOTEINDEXTYPE GetShownCentre() const;

protected:

	bool OnMouseWheel(uint32 nFlags, int16 zDelta, Point pt) override;
	void OnLButtonDown(uint32, Point) override;
	void OnSetFocus(Wnd *pOldWnd) override;
	void OnKillFocus(Wnd *pNewWnd) override;
	void OnPaint(ui::Painter &dc) override;

	UI_DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
