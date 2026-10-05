/*
 * PatternEditorDialogs.h
 * ----------------------
 * Purpose: Code for various dialogs that are used in the pattern editor.
 * Notes  : (currently none)
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/PatternEditorDialogs.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "DialogBase.h"
#include "dlg_misc.h"  // for keyboard control
#include "ColorPickerButton.h"
#include "EffectInfo.h"
#include "PatternCursor.h"
#include "PluginComboBox.h"
#include "ResizableDialog.h"
#include "TrackerSettings.h"

OPENMPT_NAMESPACE_BEGIN

class CModDoc;
struct SplitKeyboardSettings;

class CPatternPropertiesDlg : public DialogBase
{
public:
	CPatternPropertiesDlg(CModDoc &modParent, PATTERNINDEX nPat, Wnd *parent = nullptr);

protected:
	void DoDataExchange(DataExchange *pDX) override;
	bool OnInitDialog() override;
	void OnOK() override;

	void OnChangeColor();
	void OnResetColor();
	void OnHalfRowNumber();
	void OnDoubleRowNumber();
	void OnOverrideSignature();
	void OnTempoSwing();
	void OnPatternChanged();
	void OnPatternSpinDelta(NotifyHeader *notify, LResult *result);

	UI_DECLARE_MESSAGE_MAP()

	struct PatternProperties
	{
		std::string name;
		TempoSwing tempoSwing;
		ROWINDEX numRows = 0, rowsPerBeat = 0, rowsPerMeasure = 0;
		ROWINDEX resizeWarningShown = 0;
		uint32 color = CPattern::INVALID_COLOR;
		bool resizeWarningAtEnd = false;
		bool resizeAtEnd = true, repeatContents = false;
	};

	PatternProperties& GetPatternProperties(PATTERNINDEX pat);
	PatternProperties& GetPatternProperties() { return GetPatternProperties(m_nPattern); }
	void StorePatternProperties();
	void SetCurrentPattern(PATTERNINDEX pat);
	bool ValidatePatternProperties();

	CModDoc &m_modDoc;
	std::map<PATTERNINDEX, PatternProperties> m_properties;
	PATTERNINDEX m_nPattern;
	Spinner m_spinPattern, m_spinRPB, m_spinRPM;
	ComboBox m_numRows;
	ColorPickerButton m_colorBtn;
	bool m_locked = true;
};


//////////////////////////////////////////////////////////////////////////
// Command Editing


class CEditCommand : public DialogBase
{
protected:
	ComboBox cbnNote, cbnVolCmd, cbnCommand, cbnPlugParam;
	PluginComboBox cbnInstr;
	HSlider sldVolParam, sldParam;
	CTrackerSoundFile &sndFile;
	const CModSpecifications *oldSpecs = nullptr;
	ModCommand *m = nullptr;
	EffectInfo effectInfo;
	PATTERNINDEX editPattern = PATTERNINDEX_INVALID;
	CHANNELINDEX editChannel = CHANNELINDEX_INVALID;
	ROWINDEX editRow = ROWINDEX_INVALID;
	uint32 xParam, xMultiplier;
	bool modified = false;

public:
	CEditCommand(CTrackerSoundFile &sndFile);

public:
	bool ShowEditWindow(PATTERNINDEX pat, const PatternCursor &cursor, Wnd *parent);

protected:
	void InitAll() { InitNote(); InitVolume(); InitEffect(); InitPlugParam(); }
	void InitNote();
	void InitVolume();
	void InitEffect();
	void InitPlugParam();

	void UpdateVolCmdRange();
	void UpdateVolCmdValue();
	void UpdateEffectRange(bool set);
	void UpdateEffectValue(bool set);

	void PrepareUndo(const char *description);

	void DoDataExchange(DataExchange *pDX) override;
	void OnOK() override { ShowWindow(false); }
	void OnCancel() override { ShowWindow(false); }
	bool PreTranslateMessage(int event) override;
	void OnActivate(bool isActive) override;
	void OnClose()	{ ShowWindow(false); }

	void OnNoteChanged();
	void OnVolCmdChanged();
	void OnCommandChanged();
	void OnPlugParamChanged();
	void OnHScroll(uint32, uint32, Wnd *) override;
	UI_DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////
// Chord Editor

class CChordEditor : public ResizableDialog
{
protected:
	CKeyboardControl m_Keyboard;
	ComboBox m_CbnShortcut, m_CbnBaseNote, m_CbnNote[MPTChord::notesPerChord - 1];
	MPTChords m_chords;
	MPTChord::NoteType m_mouseDownKey = MPTChord::noNote, m_dragKey = MPTChord::noNote;

	static constexpr MPTChord::NoteType CHORD_MIN = -24;
	static constexpr MPTChord::NoteType CHORD_MAX = 24;

public:
	CChordEditor(Wnd *parent = nullptr);

protected:
	MPTChord &GetChord();

	void DoDataExchange(DataExchange* pDX) override;
	bool OnInitDialog() override;
	void OnOK() override;
	void OnDPIChanged() override;

	void UpdateKeyboard();
	LResult OnKeyboardNotify(WParam, LParam);
	void OnChordChanged();
	void OnBaseNoteChanged();
	void OnNote1Changed() { OnNoteChanged(0); }
	void OnNote2Changed() { OnNoteChanged(1); }
	void OnNote3Changed() { OnNoteChanged(2); }
	void OnNoteChanged(int noteIndex);
	UI_DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////
// Keyboard Split Settings (pattern editor)

class CSplitKeyboardSettings : public DialogBase
{
protected:
	ComboBox m_CbnSplitInstrument, m_CbnSplitNote, m_CbnOctaveModifier, m_CbnSplitVolume;
	CTrackerSoundFile &sndFile;

public:
	SplitKeyboardSettings &m_Settings;

	CSplitKeyboardSettings(Wnd *parent, CTrackerSoundFile &sf, SplitKeyboardSettings &settings);

protected:
	void DoDataExchange(DataExchange* pDX) override;
	bool OnInitDialog() override;
	void OnOK() override;
	void OnCancel() override;

	void OnOctaveModifierChanged();

	UI_DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////
// Show channel properties from pattern editor

class QuickChannelProperties : public DialogBase
{
protected:
	CModDoc *m_document = nullptr;
	CHANNELINDEX m_channel = 0;
	bool m_visible = false;
	bool m_settingsChanged = false;
	bool m_settingColor = false;

	ColorPickerButton m_colorBtn, m_colorBtnPrev, m_colorBtnNext;
	HSlider m_volSlider, m_panSlider;
	Spinner m_volSpin, m_panSpin;
	Edit m_nameEdit;

public:
	QuickChannelProperties() = default;
	~QuickChannelProperties();

	void Show(CModDoc *modDoc, CHANNELINDEX chn, Point position);
	void UpdateDisplay();
	CHANNELINDEX GetChannel() const { return m_channel; }

protected:
	void DoDataExchange(DataExchange *pDX) override;

	void PrepareUndo();
	void PickColorFromChannel(CHANNELINDEX channel);

	void OnActivate(bool isActive) override;
	void OnVolChanged();
	void OnPanChanged();
	void OnHScroll(uint32, uint32, Wnd *) override;
	void OnMuteChanged();
	void OnSurroundChanged();
	void OnNameChanged();
	void OnPrevChannel();
	void OnNextChannel();
	void OnChangeColor();
	void OnPickPrevColor();
	void OnPickNextColor();
	LResult OnCustomKeyMsg(WParam, LParam);

	bool PreTranslateMessage(int event) override;
	mpt::ustring GetToolTipText(uint32 id, WindowHandle hwnd) const override;

	UI_DECLARE_MESSAGE_MAP()
};


class MetronomeSettingsDlg : public DialogBase
{
public:
	MetronomeSettingsDlg(Wnd *parent = nullptr);

protected:
	void DoDataExchange(DataExchange *pDX) override;
	bool OnInitDialog() override;
	mpt::ustring GetToolTipText(uint32 id, WindowHandle hwnd) const override;

	mpt::ustring GetVolumeString() const;
	void SetSampleInfo(const mpt::PathString &path, ComboBox &combo, Edit &edit, Button &browseButton);
	bool GetSampleInfo(Setting<mpt::PathString> &path, ComboBox &combo, Edit &edit, Button &browseButton);
	mpt::PathString BrowseForSample(const mpt::PathString &path);

	void OnHScroll(uint32, uint32, Wnd *) override;
	void OnToggleMetronome();
	void OnSampleChanged();
	void OnBrowseMeasure();
	void OnBrowseBeat();

	UI_DECLARE_MESSAGE_MAP()

protected:
	HSlider m_volumeSlider;
	ComboBox m_measureCombo, m_beatCombo;
	Edit m_measureEdit, m_beatEdit;
	Button m_measureButton, m_beatButton;
};


OPENMPT_NAMESPACE_END
