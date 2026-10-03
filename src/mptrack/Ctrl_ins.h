/*
 * Ctrl_ins.h
 * ----------
 * Purpose: Instrument tab, upper panel.
 * Notes  : (currently none)
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "CDecimalSupport.h"
#include "Globals.h"
#include "PluginComboBox.h"
#include "../soundlib/modcommand.h"

OPENMPT_NAMESPACE_BEGIN

class CModDoc;
class CNoteMapWnd;
class CCtrlInstruments;

class CNoteMapWnd: public Static
{
protected:
	CModDoc &m_modDoc;
	CCtrlInstruments &m_pParent;
	uint32 m_nNote = (NOTE_MIDDLEC - NOTE_MIN), m_nOldNote = 0, m_nOldIns = 0;
	INSTRUMENTINDEX m_nInstrument = 0;
	int m_cxFont = 0, m_cyFont = 0;
	CHANNELINDEX m_noteChannel = 0;
	ModCommand::NOTE m_nPlayingNote = NOTE_NONE;

	bool m_bIns = false;
	bool m_undo = true;

private:
	void MapTranspose(int nAmount);
	void PrepareUndo(const char *description);

public:
	CNoteMapWnd(CCtrlInstruments &parent, CModDoc &document)
		: m_modDoc(document)
		, m_pParent(parent)
	{
	}
	void SetCurrentInstrument(INSTRUMENTINDEX nIns);
	void SetCurrentNote(uint32 nNote);
	void EnterNote(uint32 note);
	bool HandleChar(WParam c);
	bool HandleNav(WParam k);
	void PlayNote(uint32 note);
	void StopNote();

	void UpdateAccessibleTitle();

public:
	bool PreTranslateMessage(int event) override;

protected:
	void OnLButtonDown(uint32, Point);
	void OnMButtonDown(uint32 flags, Point pt) { OnLButtonDown(flags, pt); }
	void OnRButtonUp(uint32, Point);
	void OnLButtonDblClk(uint32, Point);
	bool OnMouseWheel(uint32 nFlags, short zDelta, Point pt);
	void OnSetFocus(Wnd *pOldWnd);
	void OnKillFocus(Wnd *pNewWnd);
	bool OnEraseBkGnd(ui::Painter *) { return true; }
	void OnPaint(ui::Painter &dc) override;
	void OnMapCopySample();
	void OnMapCopyNote();
	void OnMapTransposeUp();
	void OnMapTransposeDown();
	void OnMapReset();
	void OnTransposeSamples();
	void OnMapRemove();
	void OnEditSample(uint32 nID);
	void OnEditSampleMap();
	void OnInstrumentDuplicate();
	LResult OnCustomKeyMsg(WParam, LParam);
	UI_DECLARE_MESSAGE_MAP()
};


class CCtrlInstruments: public CModControlDlg
{
protected:
	CModControlBar m_ToolBar;
	Spinner m_SpinInstrument, m_SpinFadeOut, m_SpinGlobalVol, m_SpinPanning;
	Spinner m_SpinMidiPR, m_SpinPPS, m_SpinMidiBK, m_SpinPWD;
	ComboBox m_ComboNNA, m_ComboDCT, m_ComboDCA, m_ComboPPC, m_CbnMidiCh, m_CbnResampling, m_CbnFilterMode, m_CbnPluginVolumeHandling;
	PluginComboBox m_CbnMixPlug;
	Edit m_EditName, m_EditFileName;
	Button m_CheckPanning, m_CheckCutOff, m_CheckResonance, velocityStyle;
	HSlider m_SliderVolSwing, m_SliderPanSwing, m_SliderCutSwing, m_SliderResSwing, m_SliderCutOff, m_SliderResonance;
	CNoteMapWnd m_NoteMap;
	HSlider m_SliderAttack;
	Spinner m_SpinAttack;
	//Tuning
	ComboBox m_ComboTuning;
	// Pitch/Tempo lock
	CNumberEdit m_EditPitchTempoLock;
	Button m_CheckPitchTempoLock;

	INSTRUMENTINDEX m_nInstrument = 1;
	bool m_openendPluginListWithMouse = false;
	bool m_startedHScroll = false;
	bool m_startedEdit = false;

	void UpdateTuningComboBox();
	void BuildTuningComboBox();

public:
	CCtrlInstruments(CModControlView &parent, CModDoc &document);

public:
	void SetModified(InstrumentHint hint, bool updateAll);
	bool SetCurrentInstrument(uint32 nIns, bool bUpdNum=true);
	bool InsertInstrument(bool duplicate);
	bool OpenInstrument(const mpt::PathString &fileName);
	bool OpenInstrument(const CSoundFile &sndFile, INSTRUMENTINDEX nInstr);
	void SaveInstrument(bool doBatchSave);
	bool EditSample(uint32 nSample);
	void UpdateFilterText();

public:
	Setting<int32> &GetSplitPosRef() override;
	bool OnInitDialog() override;
	void OnDPIChanged() override;
	void DoDataExchange(DataExchange* pDX) override;	// DDX/DDV support
	ViewType GetAssociatedViewType() override;
	void RecalcLayout() override;
	void OnActivatePage(LParam) override;
	void OnDeactivatePage() override;
	void UpdateView(UpdateHint hint, HintObject *pObj = nullptr) override;
	LResult OnModCtrlMsg(WParam wParam, LParam lParam) override;
	mpt::ustring GetToolTipText(uint32 uId, WindowHandle hwnd) const override;
	bool PreTranslateMessage(int event) override;
	bool OnDragonDrop(bool doDrop, const DRAGONDROP &dropInfo) override;
protected:
	void PrepareUndo(const char *description);

	void OnEditFocus();
	void OnVScroll(uint32 nCode, uint32 nPos, Wnd *pSB) override;
	void OnHScroll(uint32 nCode, uint32 nPos, Wnd *pSB) override;
	void OnTbnDropDownToolBar(NotifyHeader* pNMHDR, LResult* pResult);
	void OnInstrumentChanged();
	void OnPrevInstrument();
	void OnNextInstrument();
	void OnInstrumentNew();
	void OnInstrumentDuplicate() { InsertInstrument(true); }
	void OnInstrumentOpen();
	void OnInstrumentSave();
	void OnInstrumentSaveOne() { SaveInstrument(false); }
	void OnInstrumentSaveAll() { SaveInstrument(true); }
	void OnInstrumentPlay();
	void OnNameChanged();
	void OnFileNameChanged();
	void OnFadeOutVolChanged();
	void OnGlobalVolChanged();
	void OnSetPanningChanged();
	void OnPanningChanged();
	void OnNNAChanged();
	void OnDCTChanged();
	void OnDCAChanged();
	void OnMPRChanged();
	void OnMPRKillFocus();
	void OnMBKChanged();
	void OnMCHChanged();
	void OnResamplingChanged();
	void OnMixPlugChanged();
	void OnPPSChanged();
	void OnPPCChanged();
	void OnFilterModeChanged();
	void OnPluginVelocityHandlingChanged();
	void OnPluginVolumeHandlingChanged();
	void OnPitchWheelDepthChanged();
	void OnOpenPluginList() { m_openendPluginListWithMouse = true; }
	void OnAttackChanged();
	void OnEnableCutOff();
	void OnEnableResonance();
	void OnEditSampleMap();
	void TogglePluginEditor();
	LResult OnCustomKeyMsg(WParam, LParam);
	void OnCbnSelchangeCombotuning();
	void OnEnChangeEditPitchTempoLock();
	void OnBnClickedCheckPitchtempolock();
	void OnEnKillFocusEditPitchTempoLock();
	void OnEnKillFocusEditFadeOut();
	void OnXButtonUp(uint32 nFlags, uint32 nButton, Point point);
	UI_DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
