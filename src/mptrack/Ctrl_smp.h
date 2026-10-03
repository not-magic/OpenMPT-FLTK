// FLTK port of openmpt/mptrack/Ctrl_smp.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "CDecimalSupport.h"
#include "Globals.h"
#include "Undo.h"
#include "UpdateHints.h"
#include "../soundlib/SampleIO.h"
#include "../tracklib/Types.h"

OPENMPT_NAMESPACE_BEGIN

enum OpenSampleTypes
{
	OpenSampleKnown = (1<<0),
	OpenSampleRaw   = (1<<1),
};
MPT_DECLARE_ENUM(OpenSampleTypes)

class CCtrlSamples: public CModControlDlg
{
protected:
	friend class DoPitchShiftTimeStretch;

	struct SampleSelectionPoints
	{
		SmpLength start = 0;
		SmpLength end = 0;
		SampleChannelSelection channels = SampleChannelSelection::None;
		constexpr bool SelectionActive() const noexcept { return channels != SampleChannelSelection::None; }
	};

	CModControlBar m_ToolBar1, m_ToolBar2;
	Edit m_EditName, m_EditFileName;
	Spinner m_SpinSample;
	Spinner m_SpinLoopStart, m_SpinLoopEnd, m_SpinSustainStart, m_SpinSustainEnd;
	Spinner m_SpinVibSweep, m_SpinVibDepth, m_SpinVibRate;
	Spinner m_SpinVolume, m_SpinGlobalVol, m_SpinPanning;
	Spinner m_SpinFineTune, m_SpinTimeStretchRatio;
	ComboBox m_ComboAutoVib, m_ComboLoopType, m_ComboSustainType, m_ComboZoom, m_CbnBaseNote, m_ComboGrainSize;
	Button m_CheckPanning;
	SAMPLEINDEX m_nSample = 1;
	INSTRUMENTINDEX m_editInstrumentName = INSTRUMENTINDEX_INVALID;
	bool m_rememberRawFormat = false;
	bool m_startedEdit = false;

	ComboBox m_ComboPitch;

public:
	CCtrlSamples(CModControlView &parent, CModDoc &document);
	~CCtrlSamples();

protected:
	bool IsOPLInstrument() const;
	
	bool SetCurrentSample(SAMPLEINDEX nSmp, int32 lZoom = -1, bool bUpdNum = true);
	bool InsertSample(bool duplicate, int8 *confirm = nullptr);
	bool OpenSample(const mpt::PathString &fileName, FlagSet<OpenSampleTypes> types = OpenSampleKnown | OpenSampleRaw);
	bool OpenSample(const CTrackerSoundFile &sndFile, SAMPLEINDEX nSample);
	void OpenSamples(const std::vector<mpt::PathString> &files, FlagSet<OpenSampleTypes> types);
	void SaveSample(bool doBatchSave);

	void Normalize(bool allSamples);
	void RemoveDCOffset(bool allSamples);
	void Resample(bool allSamples);

	void ApplyAmplify(const double amp, const double fadeInStart, const double fadeOutEnd, const bool fadeIn, const bool fadeOut, const Fade::Law fadeLaw);
	void ApplyResample(SAMPLEINDEX smp, uint32 newRate, ResamplingMode mode, bool ignoreSelection = false, bool updatePatternCommands = false, bool updatePatternNotes = false);

	SampleSelectionPoints GetSelectionPoints();
	void SetSelectionPoints(SmpLength nStart, SmpLength nEnd, std::optional<SampleChannelSelection> channelSelection = {});

	void PropagateAutoVibratoChanges();
	void SetFinetune(int step);

public:
	Setting<int32> &GetSplitPosRef() override;
	bool OnInitDialog() override;
	void DoDataExchange(DataExchange* pDX) override;	// DDX/DDV support
	ViewType GetAssociatedViewType() override;
	void RecalcLayout() override;
	void OnActivatePage(LParam) override;
	void OnDeactivatePage() override;
	void UpdateView(UpdateHint hint, HintObject *pObj = nullptr) override;
	LResult OnModCtrlMsg(WParam wParam, LParam lParam) override;
	mpt::ustring GetToolTipText(uint32 uId, WindowHandle hwnd) const override;
	bool PreTranslateMessage(int event) override;
	void OnDPIChanged() override;
	bool OnDragonDrop(bool doDrop, const DRAGONDROP &dropInfo) override;
protected:
	void OnEditFocus();
	void OnSampleChanged();
	void OnZoomChanged();
	void OnPrevInstrument();
	void OnNextInstrument();
	void OnTbnDropDownToolBar(NotifyHeader* pNMHDR, LResult* pResult);
	void OnSampleNew();
	void OnSampleDuplicate() { InsertSample(true); }
	void OnSampleOpen();
	void OnSampleOpenKnown();
	void OnSampleOpenRaw();
	void OnSampleSave();
	void OnSampleSaveOne() { SaveSample(false); }
	void OnSampleSaveAll() { SaveSample(true); }
	void OnSamplePlay();
	void OnNormalize();
	void OnAmplify();
	void OnQuickFade();
	void OnRemoveDCOffset();
	void OnResample();
	void OnReverse();
	void OnSilence();
	void OnInvert();
	void OnSignUnSign();
	void OnAutotune();
	void OnNameChanged();
	void OnFileNameChanged();
	void OnVolumeChanged();
	void OnGlobalVolChanged();
	void OnSetPanningChanged();
	void OnPanningChanged();
	void OnFineTuneChanged();
	void OnFineTuneChangedDone();
	void OnBaseNoteChanged();
	void OnLoopTypeChanged();
	void OnLoopPointsChanged();
	void OnSustainTypeChanged();
	void OnSustainPointsChanged();
	void OnVibTypeChanged();
	void OnVibDepthChanged();
	void OnVibSweepChanged();
	void OnVibRateChanged();
	void OnXFade();
	void OnStereoSeparation();
	void OnKeepSampleOnDisk();
	void OnSpinDelta(NotifyHeader *notify, LResult *result);
	LResult OnCustomKeyMsg(WParam, LParam);
	void OnXButtonUp(uint32 nFlags, uint32 nButton, Point point);

	void OnPitchShiftTimeStretch();
	void OnToggleTimestretchQuality();
	void OnEstimateSampleSize();

	void OnInitOPLInstrument();

	MPT_ATTR_NOINLINE MPT_DECL_NOINLINE void SetModified(SAMPLEINDEX smp, SampleHint hint, bool updateAll, bool waveformModified);
	void SetModified(SampleHint hint, bool updateAll, bool waveformModified) { SetModified(m_nSample, hint, updateAll, waveformModified); }
	void PrepareUndo(const char *description, sampleUndoTypes type = sundo_none, SmpLength start = 0, SmpLength end = 0, SampleChannelSelection channelSelection = SampleChannelSelection::Both);

	UI_DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
