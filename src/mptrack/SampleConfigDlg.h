/*
 * SampleConfigDlg.h
 * -------------------
 * Purpose: Implementation of the sample/instrument editor settings dialog.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

OPENMPT_NAMESPACE_BEGIN

class COptionsSampleEditor : public PropertyPage
{
public:
	COptionsSampleEditor();

protected:
	bool OnInitDialog() override;
	void OnOK() override;
	void DoDataExchange(DataExchange* pDX) override;
	bool OnSetActive() override;

	void RecalcUndoSize();

	void OnHScroll(uint32 /*nSBCode*/, uint32 /*nPos*/, Wnd * /*pScrollBar*/) override { OnSettingsChanged(); }
	void OnSettingsChanged() { SetModified(true); }
	void OnUndoSizeChanged();
	UI_DECLARE_MESSAGE_MAP()

	ComboBox m_cbnDefaultSampleFormat, m_cbnDefaultVolumeHandling;
	Spinner m_undoBufferEdit, m_finetuneEdit;
};

OPENMPT_NAMESPACE_END
