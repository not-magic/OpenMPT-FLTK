/*
 * view_gen.h
 * ----------
 * Purpose: General tab, lower panel.
 * Notes  : (currently none)
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/View_gen.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"
#include "ColorPickerButton.h"
#include "PluginComboBox.h"
#include "UpdateHints.h"

OPENMPT_NAMESPACE_BEGIN

//Note: Changing this won't increase the number of tabs in general view. Most
//of the code use plain number 4.
#define CHANNELS_IN_TAB	4

class CModDoc;
class IMixPlugin;

class CViewGlobals: public FormView
{
protected:
	Rect m_rcClient;
	TabCtrl m_TabCtrl;
	PluginComboBox m_CbnEffects[CHANNELS_IN_TAB];
	PluginComboBox m_CbnPlugin;
	ComboBox m_CbnParam, m_CbnOutput;

	HSlider m_sbVolume[CHANNELS_IN_TAB], m_sbPan[CHANNELS_IN_TAB], m_sbValue, m_sbDryRatio;
	ColorPickerButton m_channelColor[CHANNELS_IN_TAB];

	ComboBox m_CbnPreset;
	HSlider m_sbWetDry;
	Spinner m_spinVolume[CHANNELS_IN_TAB], m_spinPan[CHANNELS_IN_TAB];
	Button m_BtnSelect, m_BtnEdit;
	int m_nLockCount = 1;
	PlugParamIndex m_nCurrentParam = 0;
	CHANNELINDEX m_nActiveTab = 0;
	CHANNELINDEX m_lastEdit = CHANNELINDEX_INVALID;
	PLUGINDEX m_nCurrentPlugin = 0;

	ComboBox m_CbnSpecialMixProcessing;
	Spinner m_SpinMixGain;
	Button m_prevPluginButton, m_nextPluginButton;

	enum {AdjustPattern = true, NoPatternAdjust = false};

public:
	CViewGlobals();

public:
	CModDoc* GetDocument() const;
	void RecalcLayout();
	void LockControls() { m_nLockCount++; }
	void UnlockControls();
	bool IsLocked() const noexcept { return (m_nLockCount > 0); }
	int GetDlgItemIntEx(uint32 nID);
	void PopulateChannelPlugins(UpdateHint hint, const HintObject *pObj = nullptr);
	void BuildEmptySlotList(std::vector<PLUGINDEX> &emptySlots);
	bool MovePlug(PLUGINDEX src, PLUGINDEX dest, bool bAdjustPat = AdjustPattern);

public:
	void OnInitialUpdate() override;
	void DoDataExchange(DataExchange *pDX) override;
	void OnUpdate(View *pSender, LParam lHint, HintObject *pHint) override;

	void UpdateView(UpdateHint hint, HintObject *pObj = nullptr);
	LResult OnModViewMsg(WParam, LParam);
	LResult OnMidiMsg(WParam midiData, LParam);

private:
	void PrepareUndo(CHANNELINDEX chnMod4);
	void UndoRedo(bool undo);

	void OnEditColor(const CHANNELINDEX chnMod4);
	void OnMute(const CHANNELINDEX chnMod4, const uint32 itemID);
	void OnSurround(const CHANNELINDEX chnMod4, const uint32 itemID);
	void OnEditVol(const CHANNELINDEX chnMod4, const uint32 itemID);
	void OnEditPan(const CHANNELINDEX chnMod4, const uint32 itemID);
	void OnEditName(const CHANNELINDEX chnMod4, const uint32 itemID);
	void OnFxChanged(const CHANNELINDEX chnMod4);

	IMixPlugin *GetCurrentPlugin() const;

	void FillPluginProgramBox(int32 firstProg, int32 lastProg);
	void SetPluginModified();

	void UpdateDryWetDisplay();
	mpt::ustring GetControlToolTip(uint32 id) const;
	void UpdateToolTips();

protected:
	void OnEditUndo();
	void OnEditRedo();
	void OnUpdateUndo(CmdUI *pCmdUI);
	void OnUpdateRedo(CmdUI *pCmdUI);

	void OnEditColor1();
	void OnEditColor2();
	void OnEditColor3();
	void OnEditColor4();
	void OnMute1();
	void OnMute2();
	void OnMute3();
	void OnMute4();
	void OnSurround1();
	void OnSurround2();
	void OnSurround3();
	void OnSurround4();
	void OnEditVol1();
	void OnEditVol2();
	void OnEditVol3();
	void OnEditVol4();
	void OnEditPan1();
	void OnEditPan2();
	void OnEditPan3();
	void OnEditPan4();
	void OnEditName1();
	void OnEditName2();
	void OnEditName3();
	void OnEditName4();
	void OnFx1Changed();
	void OnFx2Changed();
	void OnFx3Changed();
	void OnFx4Changed();
	void OnPluginChanged();
	void OnPluginNameChanged();
	void OnFillParamCombo();
	void OnParamChanged();
	void OnFocusParam();
	void OnFillProgramCombo();
	void OnProgramChanged();
	void OnLoadParam();
	void OnSaveParam();
	void OnSelectPlugin();
	void OnRemovePlugin();
	void OnSetParameter();
	void OnEditPlugin();
	void OnMixModeChanged();
	void OnBypassChanged();
	void OnDryMixChanged();
	void OnMovePlugToSlot();
	void OnInsertSlot();
	void OnClonePlug();
	LResult OnParamAutomated(WParam plugin, LParam param);
	LResult OnDryWetRatioChangedFromPlayer(WParam plugin, LParam);

	void OnWetDryExpandChanged();
	void OnAutoSuspendChanged();
	void OnSpecialMixProcessingChanged();

	void OnOutputRoutingChanged();
	void OnPrevPlugin();
	void OnNextPlugin();
	void OnDestroy();
	void OnHScroll(uint32 nSBCode, uint32 nPos, Wnd * pScrollBar) override;
	void OnEditMixGain();
	void OnSize(uint32 nType, int cx, int cy);
	void OnTabSelchange(NotifyHeader* pNMHDR, LResult* pResult);
	LResult OnMDIDeactivate(WParam, LParam);
	LResult OnUnlockControls(WParam, LParam) { if (m_nLockCount > 0) m_nLockCount--; return 0; }
	UI_DECLARE_MESSAGE_MAP()
};


OPENMPT_NAMESPACE_END
