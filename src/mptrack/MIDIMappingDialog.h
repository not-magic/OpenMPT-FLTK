/*
 * MIDIMappingDialog.h
 * -------------------
 * Purpose: Implementation of OpenMPT's MIDI mapping dialog, for mapping incoming MIDI messages to plugin parameters.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "CListCtrl.h"
#include "MIDIMapping.h"
#include "ResizableDialog.h"
#include "PluginComboBox.h"


OPENMPT_NAMESPACE_BEGIN

class CSoundFile;
class CMIDIMapper;

class CMIDIMappingDialog : public ResizableDialog
{
public:
	CMIDIMappingDirective m_Setting;

protected:
	CSoundFile &m_sndFile;
	CMIDIMapper &m_rMIDIMapper;
	WindowHandle oldMIDIRecondWnd;

	// Dialog Data
	ComboBox m_ControllerCBox;
	PluginComboBox m_PluginCBox;
	ComboBox m_PlugParamCBox;
	ComboBox m_ChannelCBox;
	ComboBox m_EventCBox;
	CListCtrlEx m_List;
	SpinButton m_SpinMoveMapping;

	uint8 m_lastCC = uint8_max;

public:
	CMIDIMappingDialog(Wnd *pParent, CSoundFile &rSndfile);
	~CMIDIMappingDialog();

protected:
	void UpdateDialog(int selItem = -1);
	void UpdateEvent();
	void UpdateParameters();
	int InsertItem(const CMIDIMappingDirective &m, int insertAt);
	void SelectItem(int i);

	void SetModified();

	bool OnInitDialog() override;
	void DoDataExchange(DataExchange* pDX) override;	// DDX/DDV support
	mpt::ustring GetToolTipText(uint32 id, WindowHandle hwnd) const override;

	UI_DECLARE_MESSAGE_MAP()

	void OnSelectionChanged(NotifyHeader *pNMHDR = nullptr, LResult *pResult = nullptr);
	void OnCheckChanged(NotifyHeader *pNMHDR, LResult *pResult);
	
	void OnBnClickedCheckactive();
	void OnBnClickedCheckCapture();
	void OnCbnSelchangeComboController();
	void OnCbnSelchangeComboChannel();
	void OnCbnSelchangeComboPlugin();
	void OnCbnSelchangeComboParam();
	void OnCbnSelchangeComboEvent();
	void OnBnClickedButtonAdd();
	void OnBnClickedButtonReplace();
	void OnBnClickedButtonRemove();
	LResult OnMidiMsg(WParam, LParam);
	void OnDeltaposSpinmovemapping(NotifyHeader *pNMHDR, LResult *pResult);
	void OnBnClickedCheckPatRecord();
};

OPENMPT_NAMESPACE_END
