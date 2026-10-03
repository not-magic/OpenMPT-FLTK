/*
 * TempoSwingDialog.h
 * ------------------
 * Purpose: Implementation of the tempo swing configuration dialog.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "DialogBase.h"
#include "../soundlib/Snd_defs.h"

OPENMPT_NAMESPACE_BEGIN

class CSoundFile;

class CTempoSwingDlg : public DialogBase
{
protected:
	// Scrollable container for the sliders
	class SliderContainer : public Panel
	{
	public:
		CTempoSwingDlg &m_parent;

		SliderContainer(CTempoSwingDlg &parent) : m_parent(parent) { }

		void OnHScroll(uint32 nSBCode, uint32 nPos, Wnd *pScrollBar) override;

		UI_DECLARE_MESSAGE_MAP()
	};

	enum { SliderResolution = 1000, SliderUnity = SliderResolution / 2 };
	struct RowCtls
	{
		Static rowLabel, valueLabel;
		HSlider valueSlider;

		void SetValue(TempoSwing::value_type v);
		TempoSwing::value_type GetValue() const;
	};
	std::vector<std::unique_ptr<RowCtls>> m_controls;

	Button m_checkGroup;
	ScrollBar m_scrollBar;
	SliderContainer m_container;

	int m_scrollPos;
	static int m_groupSize;

public:
	TempoSwing m_tempoSwing;
	const TempoSwing m_origTempoSwing;
	CSoundFile &m_sndFile;
	PATTERNINDEX m_pattern;

public:
	CTempoSwingDlg(Wnd *parent, const TempoSwing &currentTempoSwing, CSoundFile &sndFile, PATTERNINDEX pattern = PATTERNINDEX_INVALID);

protected:
	void DoDataExchange(DataExchange* pDX) override;
	bool OnInitDialog() override;
	void OnDPIChanged() override;
	void OnOK() override;
	void OnCancel() override;
	void OnClose();
	void OnReset();
	void OnUseGlobal();
	void OnToggleGroup();
	void OnGroupChanged();
	void OnVScroll(uint32 nSBCode, uint32 nPos, Wnd *pScrollBar) override;

	UI_DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
