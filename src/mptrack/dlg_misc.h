/*
 * dlg_misc.h
 * ----------
 * Purpose: Implementation for various OpenMPT dialogs.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "CDecimalSupport.h"
#include "DialogBase.h"
#include "ResizableDialog.h"
#include "../soundlib/Sndfile.h"

OPENMPT_NAMESPACE_BEGIN

class Version;
class CModDoc;
class CDLSBank;

class CModTypeDlg : public DialogBase
{
protected:
	ComboBox m_TypeBox, m_ChannelsBox, m_TempoModeBox, m_PlugMixBox;
	Button m_CheckBox1, m_CheckBox2, m_CheckBox3, m_CheckBox4, m_CheckBox5, m_CheckBoxPT1x, m_CheckBoxFt2VolRamp, m_CheckBoxAmigaLimits;

	CTrackerSoundFile &sndFile;
public:
	TempoSwing m_tempoSwing;
	PlayBehaviourSet m_playBehaviour;
	CHANNELINDEX m_nChannels = 0;
	MODTYPE m_nType = MOD_TYPE_NONE;
	ui::Bitmap m_warnIcon;
	bool m_showWarning = false;
	bool m_initialized = false;

public:
	CModTypeDlg(CTrackerSoundFile &sf, Wnd *parent);
	bool VerifyData();
	void UpdateDialog();
	void OnPTModeChanged();
	void OnTempoModeChanged();
	void OnTempoSwing();
	void OnLegacyPlaybackSettings();
	void OnDefaultBehaviour();

protected:
	void UpdateChannelCBox();
	mpt::ustring FormatVersionNumber(Version version);

protected:
	void DoDataExchange(DataExchange* pDX) override;
	bool OnInitDialog() override;
	void OnDPIChanged() override;
	void OnOK() override;
	mpt::ustring GetToolTipText(uint32 id, WindowHandle WindowHandle) const override;

	UI_DECLARE_MESSAGE_MAP()
};


class CLegacyPlaybackSettingsDlg : public ResizableDialog
{
protected:
	ListBox m_CheckList;
	PlayBehaviourSet m_playBehaviour;
	MODTYPE m_modType;

public:
	CLegacyPlaybackSettingsDlg(Wnd *parent, PlayBehaviourSet &playBehaviour, MODTYPE modType);

	PlayBehaviourSet GetPlayBehaviour() const { return m_playBehaviour; }

protected:
	void DoDataExchange(DataExchange* pDX) override;
	bool OnInitDialog() override;

	void OnSelectDefaults();
	void UpdateSelectDefaults();
	void OnFilterStringChanged();

	UI_DECLARE_MESSAGE_MAP()
};


class CRemoveChannelsDlg : public DialogBase
{
public:
	CTrackerSoundFile &sndFile;
	std::vector<bool> m_bKeepMask;
	CHANNELINDEX m_nRemove;
	ListBox m_RemChansList;
	bool m_ShowCancel;

public:
	CRemoveChannelsDlg(CTrackerSoundFile &sf, CHANNELINDEX toRemove, bool showCancel = true, Wnd *parent = nullptr);

protected:
	void DoDataExchange(DataExchange *pDX) override;
	bool OnInitDialog() override;
	void OnOK() override;
	void OnChannelChanged();
	UI_DECLARE_MESSAGE_MAP()
};


class InfoDialog : protected ResizableDialog
{
private:
	mpt::ustring m_caption, m_content;

public:
	InfoDialog(Wnd *parent = nullptr);
	void SetCaption(mpt::ustring caption);
	void SetContent(mpt::ustring content);
	using ResizableDialog::DoModal;

protected:
	bool OnInitDialog() override;
};

////////////////////////////////////////////////////////////////////////
// Sound Banks

class CSoundBankProperties : public InfoDialog
{
public:
	CSoundBankProperties(const CDLSBank &bank, Wnd *parent = nullptr);
};


/////////////////////////////////////////////////////////////////////////
// Keyboard control

enum
{
	KBDNOTIFY_MOUSEMOVE=0,
	KBDNOTIFY_LBUTTONDOWN,
	KBDNOTIFY_LBUTTONUP,
};

class CKeyboardControl: public Panel
{
public:
	enum class KeyFlag : uint8
	{
		Normal    = 0x00,
		RedDot    = 0x01,
		BrightDot = 0x02,
	};

protected:
	Wnd *m_parent = nullptr;
	ui::Font m_font;
	int m_nOctaves = 1;
	int m_nSelection = -1;
	bool m_mouseCapture = false, m_cursorNotify = false;
	bool m_mouseDown = false;

	std::array<FlagSet<KeyFlag>, NOTE_MAX - NOTE_MIN + 1> KeyFlags;  // 10 octaves max
	std::array<SAMPLEINDEX, NOTE_MAX - NOTE_MIN + 1> m_sampleNum;

public:
	CKeyboardControl() = default;

public:
	void Init(Wnd *parent, int octaves = 1, bool cursorNotify = false);
	void OnDPIChanged();
	void SetFlags(uint32 key, FlagSet<KeyFlag> flags) { if (key < KeyFlags.size()) KeyFlags[key] = flags; }
	FlagSet<KeyFlag> GetFlags(uint32 key) const { return (key < KeyFlags.size()) ? KeyFlags[key] : KeyFlag::Normal; }
	void SetSample(uint32 key, SAMPLEINDEX sample) { if (key < m_sampleNum.size()) m_sampleNum[key] = sample; }
	SAMPLEINDEX GetSample(uint32 key) const { return (key < m_sampleNum.size()) ? m_sampleNum[key] : 0; }

protected:
	void DrawKey(ui::Painter &dc, const Rect rect, int key, bool black) const;

	void OnPaint(ui::Painter &dc) override;
	void OnMouseMove(uint32 nFlags, Point point);
	void OnLButtonDown(uint32 nFlags, Point point);
	void OnLButtonUp(uint32 nFlags, Point point);
	UI_DECLARE_MESSAGE_MAP()
};

DECLARE_FLAGSET(CKeyboardControl::KeyFlag)


/////////////////////////////////////////////////////////////////////////
// Sample Map

class CSampleMapDlg : public ResizableDialog
{
protected:
	enum class MouseAction : uint8
	{
		Unknown,  // Didn't mouse-down yet
		Set,      // Set selected sample
		Unset,    // Unset (revert to original keymap)
		Zero,     // Set to zero
	};

	struct SampleRegion
	{
		ModCommand::NOTE startNote = NOTE_NONE;
		ModCommand::NOTE endNote = NOTE_NONE;
	};

	CKeyboardControl m_Keyboard;
	ComboBox m_CbnSample, m_CbnRegion, m_CbnRegionStart, m_CbnRegionEnd;
	HSlider m_SbOctave;

	std::vector<SampleRegion> m_regions;

	CTrackerSoundFile &m_sndFile;
	const INSTRUMENTINDEX m_nInstrument;
	std::array<SAMPLEINDEX, NOTE_MAX - NOTE_MIN + 1> KeyboardMap;
	MouseAction m_mouseAction = MouseAction::Unknown;

	const ModCommand::NOTE m_minNote, m_maxNote;

public:
	CSampleMapDlg(CTrackerSoundFile &sf, INSTRUMENTINDEX instr, Wnd *parent = nullptr);

protected:
	void DoDataExchange(DataExchange *pDX) override;
	bool OnInitDialog() override;
	void OnOK() override;
	void OnDPIChanged() override;

	void OnUpdateSamples();
	void OnSampleChanged();
	void OnUpdateKeyboard();
	void OnUpdateOctave();
	void UpdateRegionStartEndSelection();
	void OnRegionBoundaryChanged();
	void OnHScroll(uint32, uint32, Wnd *) override;
	LResult OnKeyboardNotify(WParam, LParam);

	void RecalcSampleRegions();
	void UpdateRegionControls(ModCommand::NOTE modifiedNote = NOTE_NONE);

	UI_DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////
// Edit history dialog

class CEditHistoryDlg: public ResizableDialog
{
protected:
	CModDoc &m_modDoc;

public:
	CEditHistoryDlg(Wnd *parent, CModDoc &modDoc);

protected:
	bool OnInitDialog() override;
	void OnClearHistory();
	UI_DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////
// Generic input dialog

class CInputDlg : public DialogBase
{
protected:
	Edit m_edit;
	Spinner m_spinner;
	const mpt::ustring m_description;
	const double m_minValueDbl = 0.0, m_maxValueDbl = 0.0;
	const int32 m_minValueInt = 0, m_maxValueInt = 0;
	const int32 m_maxLength = 0;

public:
	int32 resultAsInt = 0;
	double resultAsDouble = 0.0;
	mpt::ustring resultAsString;

protected:
	CInputDlg(Wnd *parent, const mpt::ustring &desc, const mpt::ustring &defaultString, int32 maxLength, double minValDbl, double maxValDbl = 0.0, double defaultDbl = 0.0, int32 minValInt = 0, int32 maxValInt = 0, int32 defaultInt = 0);

public:
	// Initialize text input box
	CInputDlg(Wnd *parent, const mpt::ustring &desc, const mpt::ustring &defaultString, int32 maxLength = -1) : CInputDlg{parent, desc, defaultString, maxLength, 0.0} { }
	// Initialize numeric input box (float)
	CInputDlg(Wnd *parent, const mpt::ustring &desc, double minVal, double maxVal, double defaultNumber) : CInputDlg{parent, desc, {}, -1, minVal, maxVal, defaultNumber} { }
	CInputDlg(Wnd *parent, const mpt::ustring &desc, float minVal, float maxVal, float defaultNumber) : CInputDlg{parent, desc, {}, -1, minVal, maxVal, defaultNumber } { }
	// Initialize numeric input box (int)
	CInputDlg(Wnd *parent, const mpt::ustring &desc, int32 minVal, int32 maxVal, int32 defaultNumber) : CInputDlg{parent, desc, {}, -1, 0.0, 0.0, 0.0, minVal, maxVal, defaultNumber } { }

protected:
	void DoDataExchange(DataExchange *pDX) override;
	bool OnInitDialog() override;
	void OnOK() override;
};


/////////////////////////////////////////////////////////////////////////
// Messagebox with 'don't show again'-option.

// Enums for message entries. See dlg_misc.cpp for the array of entries.
enum enMsgBoxHidableMessage
{
	ModSaveHint                = 0,
	ItCompatibilityExportTip   = 1,
	XMCompatibilityExportTip   = 2,
	CompatExportDefaultWarning = 3,
	enMsgBoxHidableMessage_count
};

void MsgBoxHidable(enMsgBoxHidableMessage enMsg);

OPENMPT_NAMESPACE_END
