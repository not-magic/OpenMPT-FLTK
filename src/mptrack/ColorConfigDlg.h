/*
 * ColorConfigDlg.cpp
 * ------------------
 * Purpose: Implementation of the color setup dialog.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"
#include "ColorPickerButton.h"
#include "TrackerSettings.h"

OPENMPT_NAMESPACE_BEGIN

struct MODPLUGDIB;

class COptionsColors : public PropertyPage
{
protected:
	std::array<ColorRef, MAX_MODCOLORS> CustomColors;
	ComboBox m_ComboDPIAwareness, m_ComboItem, m_ComboFont, m_ComboPreset;
	ColorPickerButton m_BtnColor[3];
	ui::OwnerDrawPanel m_BtnPreview;
	SpinButton m_ColorSpin;
	Spinner m_spinRowsPerBeat, m_spinRowsPerMeasure;
	Static m_TxtColor[3];
	std::unique_ptr<MODPLUGDIB> m_pPreviewDib;
	FontSetting patternFont, commentFont;
	uint32 m_nColorItem = 0;

public:
	COptionsColors();
	~COptionsColors();

protected:
	bool OnInitDialog() override;
	bool OnKillActive() override;
	void OnOK() override;
	void DoDataExchange(DataExchange* pDX) override;
	bool OnSetActive() override;
	void OnVScroll(uint32 nSBCode, uint32 nPos, Wnd *pScrollBar) override;
	void DrawPreview(ui::Painter &dc, const Rect &rect);

	void SelectColor(int colorIndex);

	void OnChoosePatternFont();
	void OnChooseCommentFont();
	void OnUpdateDialog();
	void OnColorSelChanged();
	void OnHighlightsChanged();
	void OnSettingsChanged();
	void OnSelectColor1() { SelectColor(0); }
	void OnSelectColor2() { SelectColor(1); }
	void OnSelectColor3() { SelectColor(2); }
	void OnPresetChange();
	void OnLoadColorScheme();
	void OnSaveColorScheme();
	void OnClearWindowCache();
	void OnPreviewChanged();
	UI_DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
