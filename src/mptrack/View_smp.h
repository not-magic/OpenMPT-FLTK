/*
 * View_smp.h
 * ----------
 * Purpose: Sample tab, lower panel.
 * Notes  : (currently none)
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"
#include "Globals.h"
#include "Moddoc.h"
#include "TrackerSettings.h"
#include "../soundlib/modsmp_ctrl.h"
#include "../tracklib/Types.h"


OPENMPT_NAMESPACE_BEGIN

#define SMP_LEFTBAR_BUTTONS		8
class OPLInstrDlg;

class CViewSample: public CModScrollView
{
public:
	enum Flags : uint8
	{
		SMPSTATUS_MOUSEDRAG  = 0x01,
		SMPSTATUS_KEYDOWN    = 0x02,
		SMPSTATUS_NCLBTNDOWN = 0x04,
		SMPSTATUS_DRAWING    = 0x08,
		SMPSTATUS_MOUSEMOVED = 0x10,
	};

protected:
	enum class PasteMode : uint8
	{
		Replace,
		MixPaste,
		Insert
	};

	enum class HitTestItem : uint8
	{
		Nothing,
		SampleData,
		SelectionStart,
		SelectionEnd,
		LoopStart,
		LoopEnd,
		SustainStart,
		SustainEnd,
		CuePointFirst,
		CuePointLast = CuePointFirst + mpt::array_size<decltype(ModSample::cues)>::size - 1,
	};

	enum class ScrollTarget : uint8
	{
		Left,
		Right,
		Center,
	};

	std::unique_ptr<OPLInstrDlg> m_oplEditor;
	Rect m_rcClient;
	ui::OffscreenBuffer m_waveformBuffer;
	ui::Bitmap m_waveformBitmap;
	ui::Font m_timelineFont;
	Size m_sizeTotal;
	uint32 m_nBtnMouseOver = 0xFFFF;
	int m_nZoom = 0;	// < 0: Zoom into sample (2^x:1 ratio), 0: Auto zoom, > 0: Zoom out (1:2^x ratio)
	int m_timelineHeight = 0;
	int m_timelineUnit = 0;
	int m_timelineInterval = 0;
	decltype(ModSample::nC5Speed) m_cachedSampleRate = 8363;
	FlagSet<Flags> m_dwStatus;
	SAMPLEINDEX m_nSample = 1;
	SmpLength m_dwMenuParam = 0;
	SmpLength m_dwBeginSel = 0, m_dwEndSel = 0, m_dwBeginDrag = 0, m_dwEndDrag = 0;
	SampleChannelSelection m_channelSelection = SampleChannelSelection::Both;
	SampleChannelSelection m_menuChannelSelection = SampleChannelSelection::Both;

	// Drag & Drop
	HitTestItem m_dragItem = HitTestItem::Nothing;
	SampleChannelSelection m_startDragChannelSelection = SampleChannelSelection::Both;
	Point m_startDragPoint;
	SmpLength m_startDragValue = MAX_SAMPLE_LENGTH;
	bool m_dragPreparedUndo = false, m_fineDrag = false, m_forceRedrawWaveform = true, m_scrolledSinceLastMouseMove = false;

	// Sample grid
	SampleGridMode m_gridMode = SampleGridMode::NoGrid;
	double m_gridSegments = 2.0;
	double m_gridSpacing = 1000.0;
	SampleLengthUnit m_gridUnit = SampleLengthUnit::Milliseconds;

	// Sample drawing
	Point m_lastDrawPoint{-1, -1};  // For drawing horizontal lines
	int m_drawChannel = 0;           // Which sample channel are we drawing on?

	// Note-off event buffer for MIDI sustain pedal
	std::array<std::vector<uint32>, 16> m_midiSustainBuffer;
	std::bitset<16> m_midiSustainActive;

	uint32 m_NcButtonState[SMP_LEFTBAR_BUTTONS];
	std::array<SmpLength, MAX_CHANNELS> m_dwNotifyPos;
	CModDoc::NoteToChannelMap m_noteChannel;  // Note -> Preview channel assignment

public:
	CViewSample();

protected:
	MPT_ATTR_NOINLINE MPT_DECL_NOINLINE void SetModified(SAMPLEINDEX smp, SampleHint hint, bool updateAll, bool waveformModified);
	void SetModified(SampleHint hint, bool updateAll, bool waveformModified) { SetModified(m_nSample, hint, updateAll, waveformModified); }
	void UpdateScrollSize() { UpdateScrollSize(m_nZoom, true); }
	void UpdateScrollSize(int newZoom, bool forceRefresh, SmpLength centeredSample = SmpLength(-1));
	void UpdateOPLEditor();
	void SetCurrentSample(SAMPLEINDEX nSmp);
	bool IsOPLInstrument() const;
	void SetZoom(int nZoom, SmpLength centeredSample = SmpLength(-1));
	int32 SampleToScreen(SmpLength pos, bool ignoreScrollPos = false) const;
	SmpLength ScreenToSample(int32 x, bool ignoreSampleLength = false) const;
	int32 SecondsToScreen(double x) const;
	double ScreenToSeconds(int32 x, bool ignoreSampleLength = false) const;
	std::pair<HitTestItem, SmpLength> PointToItem(Point point, Rect *rect = nullptr) const;
	void PlayNote(ModCommand::NOTE note, const SmpLength nStartPos = 0, int volume = -1);
	void NoteOff(ModCommand::NOTE note);
	void InvalidateSample(bool invalidateWaveform = true);
	void InvalidateTimeline();
	void SetCurSel(SmpLength begin, SmpLength end) { SetCurSel(begin, end, m_channelSelection); }
	void SetCurSel(SmpLength begin, SmpLength end, SampleChannelSelection channelSel);
	bool HasSelection() const noexcept { return m_dwEndSel > m_dwBeginSel; };
	SampleChannelSelection SelectedChannel() const noexcept { return HasSelection() ? m_channelSelection : SampleChannelSelection::Both; }
	void ScrollToPosition(int x);
	void DrawPositionMarks(ui::Painter &dc);
	void InvalidatePositionMarks();
	template <typename Tsample>
	void DrawSampleData1(ui::Painter * hdc, int ymed, int cx, int cy, SmpLength len, SampleFlags uFlags, const Tsample *psample);
	template <typename Tsample>
	void DrawSampleData2(ui::Painter * hdc, int ymed, int cx, int cy, SmpLength len, SampleFlags uFlags, const Tsample *psample);
	void DrawNcButton(ui::Painter *pDC, uint32 nBtn);
	bool GetNcButtonRect(uint32 button, Rect &rect) const;
	uint32 GetNcButtonAtPoint(Point point, Rect *outRect = nullptr) const;
	void UpdateNcButtonState();
	void DoPaste(PasteMode pasteMode);

	// Sets sample data on sample draw.
	template<class T>
	void SetSampleData(ModSample &smp, const Point &point, const SmpLength old);

	// Sets initial draw point on sample draw.
	template<class T>
	void SetInitialDrawPoint(ModSample &smp, const Point &point);

	// Returns sample value corresponding given point in the sample view.
	template<class T>
	T GetSampleValueFromPoint(const ModSample &smp, const Point &point) const;

	// Returns Left, Right or Both
	SampleChannelSelection GetChannelSelectionFromPoint(const Point &point, const ModSample &sample) const;
	// Returns Left or Right
	SampleChannelSelection GetChannelFromPoint(const Point &point, const ModSample &sample) const;
	int GetZoomLevel(SmpLength length) const;
	void DoZoom(int direction, const Point &zoomPoint = Point(-1, -1));
	bool CanZoomSelection() const;
	void ScrollToSample(SmpLength sample, bool refresh = true, ScrollTarget target = ScrollTarget::Center);

	SmpLength ScrollPosToSamplePos() const {return ScrollPosToSamplePos(m_nZoom);}
	SmpLength ScrollPosToSamplePos(int nZoom) const;

	void OnMonoConvert(ctrlSmp::StereoToMonoMode convert);
	void TrimSample(bool trimToLoopEnd);
	void Convert8Bit(bool allSamples);

	int CalcScroll(int &currentPos, int amount, int bar);

	int WaveformHeight() const noexcept { return m_rcClient.Height() - m_timelineHeight; }

	SmpLength SnapToGrid(const SmpLength pos) const;
	// Returns effective grid segment size, in samples
	double GetGridSegmentSize(const ModSample &sample, const CTrackerSoundFile &sndFile) const;

	// Returns index of preview channel if exactly one one is being previewed, CHANNELINDEX_INVALID otherwise.
	CHANNELINDEX GetPreviewChannel() const;
	
	void PlayOrSetCuePoint(size_t cue);

public:
	void OnDraw(ui::Painter *) override;
	void OnInitialUpdate() override;
	void UpdateView(UpdateHint hint, HintObject *pObj = nullptr) override;
	LResult OnModViewMsg(WParam, LParam) override;
	bool OnDragonDrop(bool doDrop, const DRAGONDROP &dropInfo) override { return GetControlDlg()->OnDragonDrop(doDrop, dropInfo); }
	LResult OnPlayerNotify(Notification *) override;
	bool PreTranslateMessage(int event) override;
	bool OnScrollBy(Size sizeScroll, bool bDoScroll = true) override;
	bool CanDropFiles(Point) const override { return true; }
	void OnDropFiles(const std::vector<mpt::PathString> &files) override;
	bool FindToolTip(Point point, Rect &area, mpt::ustring &text) const override;

protected:
	bool OnEraseBkgnd(ui::Painter *) { return true; }
	// cppcheck-suppress duplInheritedMember
	void OnSetFocus(Wnd *pOldWnd);
	void OnSize(uint32 nType, int cx, int cy);
	void OnNcPaint(ui::Painter &dc) override;
	bool FindNcButtonToolTip(uint32 button, Rect &area, mpt::ustring &text) const;
	void OnChar(uint32 nChar, uint32 nRepCnt, uint32 nFlags);
	void OnNcMouseMove(Point point) override;
	void OnNcLButtonDown(uint32, Point) override;
	void OnNcLButtonUp(uint32, Point) override;
	void OnLButtonDblClk(uint32 nFlags, Point point);
	void OnLButtonDown(uint32 nFlags, Point point);
	void OnLButtonUp(uint32 nFlags, Point point);
	void OnRButtonUp(uint32, Point);
	void OnMouseMove(uint32, Point);
	void OnEditSelectAll();
	void OnEditDelete();
	void OnEditCut();
	void OnEditCopy();
	void OnEditPaste();
	void OnEditMixPaste();
	void OnEditInsertPaste();
	void OnEditUndo();
	void OnEditRedo();
	void OnSetLoop();
	void OnSetSustainLoop();
	void On8BitConvert();
	void On16BitConvert();
	void OnMonoConvertMix() { OnMonoConvert(ctrlSmp::mixChannels); }
	void OnMonoConvertLeft() { OnMonoConvert(ctrlSmp::onlyLeft); }
	void OnMonoConvertRight() { OnMonoConvert(ctrlSmp::onlyRight); }
	void OnMonoConvertSplit() { OnMonoConvert(ctrlSmp::splitSample); }
	void OnStereoConvert();
	void OnSampleTrim() { TrimSample(false); }
	void OnPrevInstrument();
	void OnNextInstrument();
	void OnZoomOnSel();
	void OnSetLoopStart();
	void OnSetLoopEnd();
	void OnConvertPingPongLoop();
	void OnConvertNormalLoopToSustain();
	void OnSetSustainStart();
	void OnSetSustainEnd();
	void OnConvertPingPongSustain();
	void OnConvertSustainLoopToNormal();
	void OnSetCuePoint(uint32 nID);
	void OnZoomUp();
	void OnZoomDown();
	void OnDrawingToggle();
	void OnAddSilence();
	void OnChangeGridSize();
	void OnQuickFade();
	LResult OnMidiMsg(WParam, LParam);
	LResult OnCustomKeyMsg(WParam, LParam); //rewbs.customKeys
	// cppcheck-suppress duplInheritedMember
	bool OnMouseWheel(uint32 nFlags, short zDelta, Point pt);
	void OnXButtonUp(uint32 nFlags, uint32 nButton, Point point);
	void OnUpdateUndo(CmdUI *pCmdUI);
	void OnUpdateRedo(CmdUI *pCmdUI);
	void OnSampleSliceCuePoints();
	void OnSampleSliceGrid();
	void OnSampleSlice(mpt::span<SmpLength> slicePoints);
	void OnSampleInsertCuePoint();
	void OnSampleDeleteCuePoint();
	void OnSendSelectionToNewSlot();
	void OnTimelineFormatSeconds() { SetTimelineFormat(TimelineFormat::Seconds); }
	void OnTimelineFormatSamples() { SetTimelineFormat(TimelineFormat::Samples); }
	void OnTimelineFormatSamplesPow2() { SetTimelineFormat(TimelineFormat::SamplesPow2); }
	void SetTimelineFormat(TimelineFormat fmt);
	UI_DECLARE_MESSAGE_MAP()


	static bool IsCuePoint(HitTestItem item) { return item >= HitTestItem::CuePointFirst && item <= HitTestItem::CuePointLast; }
	static int CuePointFromItem(HitTestItem item) { return static_cast<int>(item) - static_cast<int>(HitTestItem::CuePointFirst); }
};

DECLARE_FLAGSET(CViewSample::Flags)


OPENMPT_NAMESPACE_END
