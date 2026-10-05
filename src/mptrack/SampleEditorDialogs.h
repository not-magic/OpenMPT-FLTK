/*
 * SampleEditorDialogs.h
 * ---------------------
 * Purpose: Code for various dialogs that are used in the sample editor.
 * Notes  : (currently none)
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/SampleEditorDialogs.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "CDecimalSupport.h"
#include "DialogBase.h"
#include "../common/FileReaderFwd.h"
#include "../soundlib/SampleIO.h"
#include "../tracklib/Types.h"

OPENMPT_NAMESPACE_BEGIN

//////////////////////////////////////////////////////////////////////////
// Sample amplification dialog

class CAmpDlg : public DialogBase
{
public:
	struct AmpSettings
	{
		Fade::Law fadeLaw;
		double fadeInStart, fadeOutEnd;
		double factor;
		AmplificationUnit unit;
		bool fadeIn, fadeOut;
	};

	AmpSettings &m_settings;

protected:
	const double m_factorMinLinear, m_factorMaxLinear;
	const double m_factorMinDecibels, m_factorMaxDecibels;

	ComboBox m_fadeBox;
	ComboBox m_unitBox;
	ImageList m_list;
	std::array<Spinner, 3> m_edit;
	std::array<Static, 3> m_unitLabel;

	AmplificationUnit m_unit = AmplificationUnit::Percent;
	bool m_locked = true;

public:
	CAmpDlg(Wnd *parent, AmpSettings &settings, double factorMin = int32_min, double factorMax = int32_max);

protected:
	void DoDataExchange(DataExchange *pDX) override;
	bool OnInitDialog() override;
	void OnDPIChanged() override;
	void OnOK() override;
	void OnDestroy();

	void EnableFadeIn();
	void EnableFadeOut();
	void OnUnitChanged();
	void UpdateUnitLabels();

	UI_DECLARE_MESSAGE_MAP()
};


//////////////////////////////////////////////////////////////////////////
// Sample import dialog

class CRawSampleDlg : public DialogBase
{
	friend class AutodetectFormatDlg;

protected:
	static SampleIO m_format;
	static SmpLength m_offset;

	Spinner m_SpinOffset;
	FileReader &m_file;
	bool m_rememberFormat = false;

public:
	SampleIO GetSampleFormat() const { return m_format; }
	void SetSampleFormat(SampleIO nFormat) { m_format = nFormat; }

	bool GetRemeberFormat() const { return m_rememberFormat; };
	void SetRememberFormat(bool remember) { m_rememberFormat = remember; };

	SmpLength GetOffset() const { return m_offset; }
	void SetOffset(SmpLength offset) { m_offset = offset; }

public:
	CRawSampleDlg(FileReader &file, Wnd *parent = nullptr);

protected:
	void DoDataExchange(DataExchange *pDX) override;
	bool OnInitDialog() override;
	void OnOK() override;
	void UpdateDialog();

	void OnBitDepthChanged(uint32 id);
	void OnEncodingChanged(uint32 id);
	void OnAutodetectFormat();

	UI_DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////
// Add silence dialog - add silence to a sample

class AddSilenceDlg : public DialogBase
{
public:
	enum AddSilenceOptions
	{
		kSilenceAtBeginning, // Add at beginning of sample
		kSilenceAtEnd,       // Add at end of sample
		kResize,             // Resize sample
		kOPLInstrument,      // Initialize as OPL instrument
	};

	SmpLength m_numSamples; // Add x samples (also containes the return value in all cases)
	SmpLength m_length;  // Set size to x samples (init value: current sample size)
	AddSilenceOptions m_editOption; // See above

protected:
	ComboBox m_ComboUnit;
	Spinner m_EditAmount;
	static SmpLength m_addSamples;
	static SmpLength m_createSamples;
	const uint32 m_sampleRate;
	SampleLengthUnit m_unit = SampleLengthUnit::Samples;
	bool m_allowOPL;

public:
	AddSilenceDlg(Wnd *parent, SmpLength origLength, uint32 sampleRate, bool allowOPL);

	void DoDataExchange(DataExchange *pDX) override;
	bool OnInitDialog() override;
	void OnOK() override;
	
protected:
	SmpLength GetEditLength() const;
	AddSilenceOptions GetEditMode() const;
	void OnEditModeChanged();
	void OnUnitChanged();
	UI_DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////
// Sample grid dialog


class CSampleGridDlg : public DialogBase
{
public:
	SampleGridMode m_mode = SampleGridMode::NoGrid;
	const SmpLength m_maxSegments;
	double m_segments = 0.0;
	double m_spacing = 0.0;
	SampleLengthUnit m_unit = SampleLengthUnit::Samples;
	const uint32 m_sampleRate;
	bool m_locked = true;

protected:
	Spinner m_EditSegments, m_EditSpacing;
	ComboBox m_ComboUnit;

public:
	CSampleGridDlg(Wnd *parent, SampleGridMode mode, double segments, double spacing, SampleLengthUnit unit, SmpLength maxSegments, uint32 sampleRate);

protected:
	void DoDataExchange(DataExchange *pDX) override;
	bool OnInitDialog() override;
	void OnOK() override;

	void OnEditChanged(int radio, bool onlyMouse);

	void OnUnitChanged();
	void OnSegmentsFocus() { OnEditChanged(1, true); }
	void OnSpacingFocus() { OnEditChanged(2, true); }
	void OnSegmentsChanged() { OnEditChanged(1, false); }
	void OnSpacingChanged() { OnEditChanged(2, false); }

	UI_DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////
// Sample cross-fade dialog

class CSampleXFadeDlg : public DialogBase
{
public:
	static uint32 m_fadeLength;
	static uint32 m_fadeLaw;
	static bool m_afterloopFade;
	static bool m_useSustainLoop;
	SmpLength m_loopLength = 0, m_maxLength = 0;

protected:
	HSlider m_SliderLength, m_SliderFadeLaw;
	Spinner m_SpinSamples;
	Button m_RadioNormalLoop, m_RadioSustainLoop;
	ModSample &m_sample;
	bool m_editLocked = true;

public:
	CSampleXFadeDlg(Wnd *parent, ModSample &sample);

	SmpLength PercentToSamples(uint32 percent) const { return Util::muldivr_unsigned(percent, m_loopLength, 100000); }
	uint32 SamplesToPercent(SmpLength samples) const { return Util::muldivr_unsigned(samples, 100000, m_loopLength); }

protected:
	void DoDataExchange(DataExchange* pDX) override;
	bool OnInitDialog() override;
	void OnOK() override;
	mpt::ustring GetToolTipText(uint32 id, WindowHandle hwnd) const override;

	void OnLoopTypeChanged();
	void OnFadeLengthChanged();
	void OnHScroll(uint32, uint32, Wnd *) override;

	UI_DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////
// Resampling dialog

class CResamplingDlg : public DialogBase
{
public:
	enum ResamplingOption : uint8
	{
		Upsample,
		Downsample,
		Custom
	};

	enum class Action : uint8
	{
		OneSample,
		OneChannel,
		AllSamples,
	};

protected:
	ResamplingMode m_srcMode;
	uint32 m_frequency;
	const Action m_action;
	const bool m_allowAdjustNotes;
	static uint32 m_lastFrequency;
	static ResamplingOption m_lastChoice;
	static bool m_updatePatternCommands;
	static bool m_updatePatternNotes;

public:
	CResamplingDlg(Wnd *parent, uint32 frequency, ResamplingMode srcMode, Action action, bool allowAdjustNotes);
	uint32 GetFrequency() const { return m_frequency; }
	ResamplingMode GetFilter() const { return m_srcMode; }
	static ResamplingOption GetResamplingOption() { return m_lastChoice; }
	static bool UpdatePatternCommands() { return m_updatePatternCommands; }
	static bool UpdatePatternNotes() { return m_updatePatternNotes; }

protected:
	bool OnInitDialog() override;
	void OnOK() override;

	void OnFocusEdit();
	void OnEditChanged();

	UI_DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////
// Sample mix dialog

class CMixSampleDlg : public DialogBase
{
protected:
	using SmpLengthSigned = std::make_signed<SmpLength>::type;

	// Dialog controls
	Spinner m_EditOffset;
	ComboBox m_ampUnitBox, m_lengthUnitBox;
	std::array<Spinner, 2> m_edit;
	std::array<Static, 2> m_unitLabel;

	const double m_factorMinLinear, m_factorMaxLinear;
	const double m_factorMinDecibels, m_factorMaxDecibels;
	const uint32 m_sampleRate;

public:
	static SmpLengthSigned sampleOffset;
	static double amplifyOriginal;
	static double amplifyMix;
	static AmplificationUnit m_ampUnit;
	static SampleLengthUnit m_lengthUnit;

public:
	CMixSampleDlg(Wnd *parent, uint32 sampleRate);

protected:
	void DoDataExchange(DataExchange* pDX) override;
	bool OnInitDialog() override;
	void OnOK() override;

	void OnLengthUnitChanged();
	void OnAmpUnitChanged();
	void UpdateUnitLabels();

	UI_DECLARE_MESSAGE_MAP()
};


OPENMPT_NAMESPACE_END
