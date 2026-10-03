// FLTK port of openmpt/mptrack/View_smp.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "View_smp.h"
#include "Childfrm.h"
#include "Clipboard.h"
#include "Ctrl_smp.h"
#include "dlg_misc.h"  // CInputDlg
#include "DlsBankExt.h"
#include "Globals.h"
#include "ImageLists.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "MPTrackUtil.h"
#include "OPLInstrDlg.h"
#include "Reporting.h"
#include "resource.h"
#include "SampleEditorDialogs.h"
#include "WindowMessages.h"
#include "../common/FileReader.h"
#include "../soundlib/MIDIEvents.h"
#include "../soundlib/mod_specifications.h"
#include "../soundlib/S3MTools.h"
#include "../soundlib/SampleCopy.h"
#include "../soundlib/WAVTools.h"
#include "../tracklib/SampleEdit.h"
#include "mpt/io/base.hpp"
#include "mpt/io/io.hpp"
#include "mpt/io/io_span.hpp"
#include "mpt/io/io_stdstream.hpp"
#include "mpt/io/io_virtual_wrapper.hpp"
#include "openmpt_ext/sndlib/TrackerCriticalSection.h"

OPENMPT_NAMESPACE_BEGIN


// Non-client toolbar
#define SMP_LEFTBAR_CY    ui::ScalePixels(29, this)
#define SMP_LEFTBAR_CXSEP ui::ScalePixels(14, this)
#define SMP_LEFTBAR_CXSPC ui::ScalePixels(3, this)
#define SMP_LEFTBAR_CXBTN ui::ScalePixels(24, this)
#define SMP_LEFTBAR_CYBTN ui::ScalePixels(22, this)
static constexpr int TIMELINE_HEIGHT = 26;

static int TimelineHeight(WindowHandle hwnd)
{
	auto height = ui::ScalePixels(TIMELINE_HEIGHT, hwnd);
	if(height % 2)
		height++;  // Avoid weird-looking triangles if timeline is scaled to odd height
	return height;
}


static constexpr int MIN_ZOOM = -6;
static constexpr int MAX_ZOOM = 10;

// Defines the minimum length for selection for which
// trimming will be done. This is the minimum value for
// selection difference, so the minimum length of result
// of trimming is nTrimLengthMin + 1.
static constexpr SmpLength MIN_TRIM_LENGTH = 4;

static constexpr uint32 cLeftBarButtons[SMP_LEFTBAR_BUTTONS] =
{
	ID_SAMPLE_ZOOMUP,
	ID_SAMPLE_ZOOMDOWN,
		ID_SEPARATOR,
	ID_SAMPLE_DRAW,
	ID_SAMPLE_ADDSILENCE,
		ID_SEPARATOR,
	ID_SAMPLE_GRID,
		ID_SEPARATOR,
};


UI_MESSAGE_MAP_BEGIN(CViewSample, CModScrollView)

	UI_COMMAND(ID_EDIT_UNDO,				&CViewSample::OnEditUndo)
	UI_COMMAND(ID_EDIT_REDO,				&CViewSample::OnEditRedo)
	UI_COMMAND(ID_EDIT_SELECT_ALL,			&CViewSample::OnEditSelectAll)
	UI_COMMAND(ID_EDIT_CUT,					&CViewSample::OnEditCut)
	UI_COMMAND(ID_EDIT_COPY,				&CViewSample::OnEditCopy)
	UI_COMMAND(ID_EDIT_PASTE,				&CViewSample::OnEditPaste)
	UI_COMMAND(ID_EDIT_MIXPASTE,			&CViewSample::OnEditMixPaste)
	UI_COMMAND(ID_EDIT_PUSHFORWARDPASTE,	&CViewSample::OnEditInsertPaste)
	UI_COMMAND(ID_SAMPLE_SETLOOP,			&CViewSample::OnSetLoop)
	UI_COMMAND(ID_SAMPLE_SETSUSTAINLOOP,	&CViewSample::OnSetSustainLoop)
	UI_COMMAND(ID_SAMPLE_8BITCONVERT,		&CViewSample::On8BitConvert)
	UI_COMMAND(ID_SAMPLE_16BITCONVERT,		&CViewSample::On16BitConvert)
	UI_COMMAND(ID_SAMPLE_MONOCONVERT,		&CViewSample::OnMonoConvertMix)
	UI_COMMAND(ID_SAMPLE_MONOCONVERT_LEFT,	&CViewSample::OnMonoConvertLeft)
	UI_COMMAND(ID_SAMPLE_MONOCONVERT_RIGHT,	&CViewSample::OnMonoConvertRight)
	UI_COMMAND(ID_SAMPLE_MONOCONVERT_SPLIT,	&CViewSample::OnMonoConvertSplit)
	UI_COMMAND(ID_SAMPLE_STEREOCONVERT,		&CViewSample::OnStereoConvert)
	UI_COMMAND(ID_SAMPLE_TRIM,				&CViewSample::OnSampleTrim)
	UI_COMMAND(ID_PREVINSTRUMENT,			&CViewSample::OnPrevInstrument)
	UI_COMMAND(ID_NEXTINSTRUMENT,			&CViewSample::OnNextInstrument)
	UI_COMMAND(ID_SAMPLE_ZOOMONSEL,			&CViewSample::OnZoomOnSel)
	UI_COMMAND(ID_SAMPLE_SETLOOPSTART,		&CViewSample::OnSetLoopStart)
	UI_COMMAND(ID_SAMPLE_SETLOOPEND,		&CViewSample::OnSetLoopEnd)
	UI_COMMAND(ID_CONVERT_PINGPONG_LOOP,	&CViewSample::OnConvertPingPongLoop)
	UI_COMMAND(ID_CONVERT_NORMAL_TO_SUSTAIN, &CViewSample::OnConvertNormalLoopToSustain)
	UI_COMMAND(ID_SAMPLE_SETSUSTAINSTART,	&CViewSample::OnSetSustainStart)
	UI_COMMAND(ID_SAMPLE_SETSUSTAINEND,		&CViewSample::OnSetSustainEnd)
	UI_COMMAND(ID_CONVERT_PINGPONG_SUSTAIN,	&CViewSample::OnConvertPingPongSustain)
	UI_COMMAND(ID_CONVERT_SUSTAIN_TO_NORMAL, &CViewSample::OnConvertSustainLoopToNormal)
	UI_COMMAND(ID_SAMPLE_ZOOMUP,			&CViewSample::OnZoomUp)
	UI_COMMAND(ID_SAMPLE_ZOOMDOWN,			&CViewSample::OnZoomDown)
	UI_COMMAND(ID_SAMPLE_DRAW,				&CViewSample::OnDrawingToggle)
	UI_COMMAND(ID_SAMPLE_ADDSILENCE,		&CViewSample::OnAddSilence)
	UI_COMMAND(ID_SAMPLE_GRID,				&CViewSample::OnChangeGridSize)
	UI_COMMAND(ID_SAMPLE_QUICKFADE,			&CViewSample::OnQuickFade)
	UI_COMMAND(ID_SAMPLE_SLICE,				&CViewSample::OnSampleSliceCuePoints)
	UI_COMMAND(ID_SAMPLE_SLICE_GRID,		&CViewSample::OnSampleSliceGrid)
	UI_COMMAND(ID_SAMPLE_INSERT_CUEPOINT,	&CViewSample::OnSampleInsertCuePoint)
	UI_COMMAND(ID_SAMPLE_DELETE_CUEPOINT,	&CViewSample::OnSampleDeleteCuePoint)
	UI_COMMAND(ID_SAMPLE_SEND_TO_NEW_SLOT,	&CViewSample::OnSendSelectionToNewSlot)
	UI_COMMAND(ID_SAMPLE_TIMELINE_SECONDS,	&CViewSample::OnTimelineFormatSeconds)
	UI_COMMAND(ID_SAMPLE_TIMELINE_SAMPLES,	&CViewSample::OnTimelineFormatSamples)
	UI_COMMAND(ID_SAMPLE_TIMELINE_SAMPLES_POW2, &CViewSample::OnTimelineFormatSamplesPow2)
	UI_COMMAND_RANGE(ID_SAMPLE_CUE_1, ID_SAMPLE_CUE_9, &CViewSample::OnSetCuePoint)
	UI_UPDATE_COMMAND(ID_EDIT_UNDO,		&CViewSample::OnUpdateUndo)
	UI_UPDATE_COMMAND(ID_EDIT_REDO,		&CViewSample::OnUpdateRedo)
	UI_MESSAGE(MSG_MOD_MIDIMSG,				&CViewSample::OnMidiMsg)
	UI_MESSAGE(MSG_MOD_KEYCOMMAND,			&CViewSample::OnCustomKeyMsg)
UI_MESSAGE_MAP_END()


///////////////////////////////////////////////////////////////
// CViewSample operations

CViewSample::CViewSample()
	: m_timelineHeight{TIMELINE_HEIGHT}
{
	MemsetZero(m_NcButtonState);
	m_dwNotifyPos.fill(Notification::PosInvalid);
}


void CViewSample::OnInitialUpdate()
{
	CModScrollView::OnInitialUpdate();
	m_dwBeginSel = m_dwEndSel = 0;
	m_dwStatus.reset(SMPSTATUS_DRAWING);
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if (pMainFrm)
	{
		pMainFrm->SetInfoText(UL_(""));
		pMainFrm->SetXInfoText(UL_(""));
	}
	UpdateOPLEditor();
	UpdateScrollSize();
	UpdateNcButtonState();
}


// updateAll: Update all views including this one. Otherwise, only update update other views.
void CViewSample::SetModified(SAMPLEINDEX smp, SampleHint hint, bool updateAll, bool waveformModified)
{
	CModDoc *pModDoc = GetDocument();
	pModDoc->SetModified();

	if(waveformModified)
	{
		// Update on-disk sample status in tree
		ModSample &sample = pModDoc->GetSoundFile().GetSample(smp);
		if(sample.uFlags[SMP_KEEPONDISK] && !sample.uFlags[SMP_MODIFIED])
			hint.Names();
		sample.uFlags.set(SMP_MODIFIED);
	}
	pModDoc->UpdateAllViews(nullptr, hint.SetData(smp), updateAll ? nullptr : this);
}


void CViewSample::UpdateScrollSize(int newZoom, bool forceRefresh, SmpLength centeredSample)
{
	CModDoc *pModDoc = GetDocument();
	if(pModDoc == nullptr || (newZoom == m_nZoom && !forceRefresh))
	{
		return;
	}

	const int oldZoom = m_nZoom;
	m_nZoom = newZoom;

	GetClientRect(&m_rcClient);

	if(m_oplEditor && IsOPLInstrument())
	{
		const auto size = m_oplEditor->GetMinimumSize();
		m_oplEditor->SetWindowPos(nullptr, -m_nScrollPosX, -m_nScrollPosY, std::max(size.cx, m_rcClient.right), std::max(size.cy, m_rcClient.bottom), ui::PosNoZOrder | ui::PosNoActivate);
		SetScrollSizes(size);
		return;
	}

	const CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
	Size sizePage, sizeLine;
	SmpLength dwLen = 0;
	uint32 sampleRate = 8363;

	if((m_nSample > 0) && (m_nSample <= sndFile.GetNumSamples()))
	{
		const ModSample &sample = sndFile.GetSample(m_nSample);
		if(sample.HasSampleData())
			dwLen = sample.nLength;
		sampleRate = sample.GetSampleRate(sndFile.GetType());
	}
	// Compute scroll size in pixels
	if (newZoom == 0)		// Fit to display
		m_sizeTotal.cx = m_rcClient.Width();
	else if(newZoom == 1)	// 1:1
		m_sizeTotal.cx = dwLen;
	else if(newZoom > 1)	// Zoom out
		m_sizeTotal.cx = (dwLen + (1 << (newZoom - 1)) - 1) >> (newZoom - 1);
	else					// Zoom in - here, we don't compute the real number of visible pixels so that the scroll bar doesn't grow unnecessarily long. The scrolling code in OnScrollBy() compensates for this.
		m_sizeTotal.cx = dwLen + m_rcClient.Width() - (m_rcClient.Width() >> (-newZoom - 1));

	m_sizeTotal.cy = 1;
	sizeLine.cx = (m_rcClient.right / 16) + 1;
	if(newZoom < 0)
		sizeLine.cx >>= (-newZoom - 1);
	sizeLine.cy = 1;
	sizePage.cx = sizeLine.cx * 4;
	sizePage.cy = 1;

	SetScrollSizes(m_sizeTotal, sizePage, sizeLine);

	if(oldZoom != newZoom) // After zoom change, keep the view position.
	{
		if(centeredSample != SmpLength(-1))
		{
			ScrollToSample(centeredSample, false);
		} else
		{
			const SmpLength nOldPos = ScrollPosToSamplePos(oldZoom);
			const float fPosFraction = (dwLen > 0) ? static_cast<float>(nOldPos) / dwLen : 0;
			SetScrollPos(SB_HORZ, static_cast<int>(fPosFraction * GetScrollLimit(SB_HORZ)));
		}
	}

	// Choose optimal timeline interval for this zoom level
	if(m_sizeTotal.cx == 0 || dwLen == 0)
		return;

	const TimelineFormat format = TrackerSettings::Instance().sampleEditorTimelineFormat;
	double timelineInterval = MulDiv(150, m_dpi, 96);  // Timeline interval should be around 150 pixels
	if(m_nZoom > 0)
		timelineInterval *= 1 << (m_nZoom - 1);
	else if(m_nZoom < 0)
		timelineInterval /= 1 << (-m_nZoom - 1);
	else if(m_sizeTotal.cx != 0)
		timelineInterval = timelineInterval * dwLen / m_sizeTotal.cx;
	if(format == TimelineFormat::Seconds)
		timelineInterval *= 1000.0 / sampleRate;
	if(timelineInterval < 1)
		timelineInterval = 1;

	const double power = (format == TimelineFormat::SamplesPow2) ? 2.0 : 10.0;
	m_timelineUnit = mpt::saturate_round<int>(std::log(static_cast<double>(timelineInterval)) / std::log(power));
	if(m_timelineUnit < 1)
		m_timelineUnit = 0;
	m_timelineUnit = mpt::saturate_trunc<int>(std::pow(power, m_timelineUnit));
	timelineInterval = std::max(1.0, std::round(timelineInterval / m_timelineUnit)) * m_timelineUnit;
	if(format == TimelineFormat::Seconds)
		timelineInterval *= sampleRate / 1000.0;

	if(m_nZoom > 0)
		timelineInterval /= 1 << (m_nZoom - 1);
	else if(m_nZoom < 0)
		timelineInterval *= 1 << (-m_nZoom - 1);
	else
		timelineInterval = timelineInterval * m_sizeTotal.cx / dwLen;
	m_timelineInterval = mpt::saturate_round<int>(timelineInterval);

	m_cachedSampleRate = sampleRate;
}


void CViewSample::ScrollToSample(SmpLength sample, bool refresh, ScrollTarget target)
{
	int scrollToSample = sample >> (std::max(1, m_nZoom) - 1);
	if(target != ScrollTarget::Left)
		scrollToSample -= (m_rcClient.Width() / ((target == ScrollTarget::Right) ? 1 : 2)) >> (-std::min(-1, m_nZoom) - 1);

	Limit(scrollToSample, 0, GetScrollLimit(SB_HORZ));
	if(GetScrollPos(SB_HORZ) != scrollToSample)
	{
		SetScrollPos(SB_HORZ, scrollToSample);
		if(refresh)
			InvalidateSample();
	}
}


int CViewSample::CalcScroll(int &currentPos, int amount, int bar)
{
	// Don't scroll if there is no valid scroll range (ie. no scroll bar)
	if(!IsBarVisible(bar))
		amount = 0;

	const int orig = GetScrollPos(bar);
	currentPos = Clamp(orig + amount, 0, GetScrollLimit(bar));

	return -(currentPos - orig);
}


bool CViewSample::OnScrollBy(Size sizeScroll, bool bDoScroll)
{
	int x = 0, y = 0;
	int scrollByX = CalcScroll(x, sizeScroll.cx, SB_HORZ);
	int scrollByY = CalcScroll(y, sizeScroll.cy, SB_VERT);

	if(!scrollByX && !scrollByY)
	{
		// Nothing changed!
		return false;
	}

	if (bDoScroll)
	{
		// Don't allow to scroll into the middle of a sampling point
		if(m_nZoom < 0 && !IsOPLInstrument())
		{
			scrollByX *= (1 << (-m_nZoom - 1));
		}

		Invalidate();
		if(scrollByX) SetScrollPos(SB_HORZ, x);
		if(scrollByY) SetScrollPos(SB_VERT, y);
		m_forceRedrawWaveform  = true;
		m_scrolledSinceLastMouseMove = true;
	}
	return true;
}


void CViewSample::SetCurrentSample(SAMPLEINDEX nSmp)
{
	CModDoc *pModDoc = GetDocument();
	if(!pModDoc)
		return;
	if(nSmp < 1 || nSmp > pModDoc->GetNumSamples())
		return;
	pModDoc->SetNotifications(Notification::Sample, nSmp);
	pModDoc->SetFollowWnd(this);
	if(nSmp == m_nSample)
		return;
	m_dwBeginSel = m_dwEndSel = 0;
	m_dwStatus.reset(SMPSTATUS_DRAWING);
	if(CMainFrame *pMainFrm = CMainFrame::GetMainFrame(); pMainFrm)
		pMainFrm->SetInfoText(UL_(""));
	const bool wasOPL = IsOPLInstrument();
	m_nSample = nSmp;
	m_dwNotifyPos.fill(Notification::PosInvalid);
	if(!wasOPL && IsOPLInstrument())
		SetScrollPos(SB_HORZ, 0);
	UpdateOPLEditor();
	UpdateScrollSize();
	UpdateNcButtonState();
	InvalidateSample();
}


bool CViewSample::IsOPLInstrument() const
{
	return m_nSample >= 1 && m_nSample <= GetDocument()->GetNumSamples() && GetDocument()->GetSoundFile().GetSample(m_nSample).uFlags[CHN_ADLIB];
}


void CViewSample::UpdateOPLEditor()
{
	if(!IsOPLInstrument())
	{
		if(m_oplEditor && m_oplEditor->IsWindowVisible())
		{
			m_oplEditor->SetEnabled(false);
			SetFocus();
		}
		return;
	}
	CTrackerSoundFile &sndFile = GetDocument()->GetSoundFile();
	if(!m_oplEditor)
	{
		try
		{
			m_oplEditor = std::make_unique<OPLInstrDlg>(*this, sndFile);
		} catch(mpt::out_of_memory e)
		{
			mpt::delete_out_of_memory(e);
			return;
		}
	}
	m_oplEditor->SetPatch(sndFile.GetSample(m_nSample).adlib);
	auto size = m_oplEditor->GetMinimumSize();
	m_oplEditor->SetWindowPos(nullptr, -m_nScrollPosX, -m_nScrollPosY, std::max(size.cx, m_rcClient.right), std::max(size.cy, m_rcClient.bottom), ui::PosNoZOrder | ui::PosNoActivate);
	m_oplEditor->SetEnabled(true);
}


// cppcheck-suppress duplInheritedMember
void CViewSample::OnSetFocus(Wnd *pOldWnd)
{
	CModScrollView::OnSetFocus(pOldWnd);
	SetCurrentSample(m_nSample);
}


void CViewSample::SetZoom(int nZoom, SmpLength centeredSample)
{

	if(nZoom == m_nZoom && centeredSample == SmpLength(-1))
		return;
	if(nZoom > MAX_ZOOM)
		return;

	UpdateScrollSize(nZoom, true, centeredSample);
	InvalidateSample();
}


double CViewSample::GetGridSegmentSize(const ModSample &sample, const CTrackerSoundFile &sndFile) const
{
	switch(m_gridMode)
	{
	case SampleGridMode::NoGrid:
		return 0.0;
	case SampleGridMode::DivideIntoSegments:
		if(m_gridSegments > 1.0 && m_gridSegments < sample.nLength)
			return static_cast<double>(sample.nLength) / m_gridSegments;
		break;
	case SampleGridMode::DivideEveryN:
		if(m_gridSpacing > 0.0)
		{
			if(m_gridUnit == SampleLengthUnit::Milliseconds)
			{
				uint32 sampleRate = sample.GetSampleRate(sndFile.GetType());
				if(!sampleRate)
					sampleRate = 8363;
				return m_gridSpacing * sampleRate * (1.0 / 1000.0);
			}
			return m_gridSpacing;
		}
		break;
	}
	return 0.0;
}


SmpLength CViewSample::SnapToGrid(const SmpLength pos) const
{
	if(m_gridMode == SampleGridMode::NoGrid || GetDocument() == nullptr)
		return pos;
	const CTrackerSoundFile &sndFile = GetDocument()->GetSoundFile();
	const ModSample &sample = sndFile.GetSample(m_nSample);
	const double samplesPerSegment = GetGridSegmentSize(sample, sndFile);
	if(samplesPerSegment <= 1.0)
		return pos;
	SmpLength snapped = mpt::saturate_round<SmpLength>(mpt::round(pos / samplesPerSegment) * samplesPerSegment);
	// Also consider the sample end as a snap point, so that a short final segment in "divide every N" mode remains selectable
	SmpLength distToGrid = (snapped > pos) ? (snapped - pos) : (pos - snapped);
	SmpLength distToEnd = (sample.nLength > pos) ? (sample.nLength - pos) : (pos - sample.nLength);
	if(distToEnd < distToGrid)
		snapped = sample.nLength;
	return snapped;
}


void CViewSample::SetCurSel(SmpLength begin, SmpLength end, SampleChannelSelection channelSel)
{
	if(GetDocument() == nullptr)
		return;

	CTrackerSoundFile &sndFile = GetDocument()->GetSoundFile();
	const ModSample &sample = sndFile.GetSample(m_nSample);

	begin = SnapToGrid(begin);
	end = SnapToGrid(end);

	if(begin > end)
		std::swap(begin, end);

	if(channelSel == SampleChannelSelection::None)
		end = begin;

	if(begin == m_dwBeginSel && end == m_dwEndSel && channelSel == m_channelSelection)
		return;

	SmpLength updateBegin = m_dwBeginSel, updateEnd = m_dwEndSel;
	if(m_dwBeginSel >= m_dwEndSel)
	{
		// There was no active selection
		updateBegin = begin;
		updateEnd = end;
	}
	if(begin == updateBegin && updateEnd != end)
	{
		// Extend existing selection to the right
		updateBegin = updateEnd;
		updateBegin = std::min(updateBegin, end);
		updateEnd = std::max(updateEnd, end);
	} else if(end == updateEnd && updateBegin != begin)
	{
		// Extend existing selection to the left
		updateEnd = updateBegin;
		updateBegin = std::min(updateBegin, begin);
		updateEnd = std::max(updateEnd, begin);
	} else
	{
		// Extend in both directions
		updateBegin = std::min(updateBegin, begin);
		updateEnd = std::max(updateEnd, end);
	}

	SampleChannelSelection updateChannels = channelSel;
	if(channelSel != m_channelSelection)
	{
		updateChannels = SampleChannelSelection::Both;
		updateBegin = std::min(updateBegin, begin);
		updateEnd = std::max(updateEnd, end);
	}

	m_dwBeginSel = begin;
	m_dwEndSel = end;
	m_channelSelection = channelSel;

	Rect rect{std::max(0, SampleToScreen(updateBegin)), m_timelineHeight, std::min(static_cast<int>(m_rcClient.right), SampleToScreen(updateEnd) + 1), m_rcClient.bottom};
	const int halfHeight = WaveformHeight() / 2;
	if(updateChannels == SampleChannelSelection::Left)
		rect.bottom = m_timelineHeight + halfHeight;
	else if(updateChannels == SampleChannelSelection::Right)
		rect.top = m_timelineHeight + halfHeight;
	if(rect.right > rect.left)
		InvalidateRect(&rect, false);

	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if(!pMainFrm)
		return;

	mpt::ustring s;
	if(HasSelection())
	{
		const SmpLength selLength = m_dwEndSel - m_dwBeginSel;

		mpt::ustring channelStr;
		if(m_channelSelection == SampleChannelSelection::Left)
			channelStr = UL_(" L");
		else if(m_channelSelection == SampleChannelSelection::Right)
			channelStr = UL_(" R");

		mpt::ustring (*fmt)(unsigned int, mpt::ustring, const SmpLength &) = &mpt::ufmt::dec<SmpLength>;
		if(TrackerSettings::Instance().cursorPositionInHex)
			fmt = &mpt::ufmt::HEX<SmpLength>;
		s = MPT_UFORMAT("[{}-{}{}] ({} sample{}, ")(fmt(3, UL_(","), m_dwBeginSel), fmt(3, UL_(","), m_dwEndSel), channelStr, fmt(3, UL_(","), selLength), (selLength == 1) ? UL_("") : UL_("s"));

		// Length in seconds
		auto sampleRate = sample.GetSampleRate(sndFile.GetType());
		if(sampleRate <= 0) sampleRate = 8363;
		double sec = selLength / static_cast<double>(sampleRate);
		if(sec < 1)
			s += MPT_UFORMAT("{}ms")(mpt::ufmt::flt(sec * 1000.0, 3));
		else
			s += MPT_UFORMAT("{}s")(mpt::ufmt::flt(sec, 3));

		// Length in beats
		double beats = selLength;
		if(sndFile.m_nTempoMode == TempoMode::Modern)
		{
			beats *= sndFile.m_PlayState.m_nMusicTempo.ToDouble() * (1.0 / 60.0) / sampleRate;
		} else
		{
			sndFile.RecalculateSamplesPerTick();
			beats *= sndFile.GetSampleRate() / static_cast<double>(Util::mul32to64_unsigned(sndFile.m_PlayState.m_nCurrentRowsPerBeat, sndFile.m_PlayState.m_nMusicSpeed) * Util::mul32to64_unsigned(sndFile.m_PlayState.m_nSamplesPerTick, sampleRate));
		}
		s += MPT_UFORMAT(", {} beats)")(mpt::ufmt::flt(beats, 5));
	}
	pMainFrm->SetInfoText(mpt::ToUnicode(s));
}


SampleChannelSelection CViewSample::GetChannelSelectionFromPoint(const Point &point, const ModSample &sample) const
{
	if(!sample.uFlags[CHN_STEREO])
		return SampleChannelSelection::Both;
	const int waveformHeight = WaveformHeight();
	if(waveformHeight <= 0)
		return SampleChannelSelection::Both;
	const int y = point.y - m_timelineHeight;
	if(y < waveformHeight / 4)
		return SampleChannelSelection::Left;
	if(y >= waveformHeight * 3 / 4)
		return SampleChannelSelection::Right;
	return SampleChannelSelection::Both;
}


SampleChannelSelection CViewSample::GetChannelFromPoint(const Point &point, const ModSample &sample) const
{
	if(!sample.uFlags[CHN_STEREO])
		return SampleChannelSelection::Left;
	const int waveformHeight = WaveformHeight();
	if(waveformHeight <= 0)
		return SampleChannelSelection::Left;
	const int y = point.y - m_timelineHeight;
	if(y < waveformHeight / 2)
		return SampleChannelSelection::Left;
	return SampleChannelSelection::Right;
}


int32 CViewSample::SampleToScreen(SmpLength pos, bool ignoreScrollPos) const
{
	CModDoc *pModDoc = GetDocument();
	if((pModDoc) && (m_nSample <= pModDoc->GetNumSamples()))
	{
		SmpLength nLen = pModDoc->GetSoundFile().GetSample(m_nSample).nLength;
		if(!nLen)
			return 0;

		const SmpLength scrollPos = ignoreScrollPos ? 0 : m_nScrollPosX;
		if(m_nZoom > 0)
			return (pos >> (m_nZoom - 1)) - scrollPos;
		else if(m_nZoom < 0)
			return (pos - scrollPos) << (-m_nZoom - 1);
		else
			return Util::muldiv(pos, m_sizeTotal.cx, nLen);
	}
	return 0;
}


SmpLength CViewSample::ScreenToSample(int32 x, bool ignoreSampleLength) const
{
	const CModDoc *pModDoc = GetDocument();
	SmpLength n = 0;

	if((pModDoc) && (m_nSample <= pModDoc->GetNumSamples()))
	{
		SmpLength smpLen = pModDoc->GetSoundFile().GetSample(m_nSample).nLength;
		if(!smpLen)
			return 0;

		if(m_nZoom > 0)
			n = std::max(0, m_nScrollPosX + x) << (m_nZoom - 1);
		else if(m_nZoom < 0)
			n = std::max(0, m_nScrollPosX + mpt::rshift_signed(x, (-m_nZoom - 1)));
		else
		{
			if(x < 0)
				x = 0;
			if(m_sizeTotal.cx)
				n = Util::muldiv(x, smpLen, m_sizeTotal.cx);
		}
		if(!ignoreSampleLength)
			LimitMax(n, smpLen);
	}
	return n;
}


int32 CViewSample::SecondsToScreen(double x) const
{
	const ModSample &sample = GetDocument()->GetSoundFile().GetSample(m_nSample);
	const auto sampleRate = sample.GetSampleRate(GetDocument()->GetModType());
	if(sampleRate == 0 || sample.nLength == 0)
		return 0;

	x *= sampleRate;
	// This is essentially duplicated from SampleToScreen but carried out in double precision to avoid rounding errors at very high zoom levels
	if(m_nZoom > 0)
		x = x / (1 << (m_nZoom - 1)) - m_nScrollPosX;
	else if(m_nZoom < 0)
		x = (x - m_nScrollPosX) * (1 << (-m_nZoom - 1));
	else
		x = x * m_sizeTotal.cx / sample.nLength;

	return mpt::saturate_round<int32>(x);
}


double CViewSample::ScreenToSeconds(int32 x, bool ignoreSampleLength) const
{
	const ModSample &sample = GetDocument()->GetSoundFile().GetSample(m_nSample);
	const auto sampleRate = sample.GetSampleRate(GetDocument()->GetModType());
	if(sampleRate == 0)
		return 0;
	return ScreenToSample(x, ignoreSampleLength) / static_cast<double>(sampleRate);
}


static bool HitTest(int pointX, int objX, int marginL, int marginR, int top, int bottom, Rect *rect)
{
	if(!mpt::is_in_range(pointX, objX - marginL, objX + marginR))
		return false;
	if(rect)
		*rect = Rect{objX - marginL, top, objX + marginR + 1, bottom};
	return true;
}

std::pair<CViewSample::HitTestItem, SmpLength> CViewSample::PointToItem(Point point, Rect *rect) const
{
	if(IsOPLInstrument())
		return {HitTestItem::Nothing, MAX_SAMPLE_LENGTH};

	const bool inTimeline = point.y < m_timelineHeight;
	const CTrackerSoundFile &sndFile = GetDocument()->GetSoundFile();
	if(m_nSample > sndFile.GetNumSamples())
	{
		if(inTimeline)
			return {HitTestItem::Nothing, MAX_SAMPLE_LENGTH};
		else
			return {HitTestItem::SampleData, ScreenToSample(point.x)};
	}
	const ModSample &sample = sndFile.GetSample(m_nSample);

	if(inTimeline)
	{
		if(sample.nSustainStart < sample.nSustainEnd && sample.nSustainStart < sample.nLength)
		{
			if(HitTest(point.x, SampleToScreen(sample.nSustainEnd), m_timelineHeight / 2, 0, 0, m_timelineHeight, rect))
				return {HitTestItem::SustainEnd, sample.nSustainEnd};
			if(HitTest(point.x, SampleToScreen(sample.nSustainStart), 0, m_timelineHeight / 2, 0, m_timelineHeight, rect))
				return {HitTestItem::SustainStart, sample.nSustainStart};
		}
		if (sample.nLoopStart < sample.nLoopEnd && sample.nLoopStart < sample.nLength)
		{
			if(HitTest(point.x, SampleToScreen(sample.nLoopEnd), m_timelineHeight / 2, 0, 0, m_timelineHeight, rect))
				return {HitTestItem::LoopEnd, sample.nLoopEnd};
			if(HitTest(point.x, SampleToScreen(sample.nLoopStart), 0, m_timelineHeight / 2, 0, m_timelineHeight, rect))
				return {HitTestItem::LoopStart, sample.nLoopStart };
		}
		for(size_t i = 0; i < std::size(sample.cues); i++)
		{
			size_t cue = std::size(sample.cues) - 1 - i;  // If two cues overlap visually, the cue with the higher ID is drawn on top, so pick it first
			if(sample.cues[cue] < sample.nLength && HitTest(point.x, SampleToScreen(sample.cues[cue]), m_timelineHeight / 2, m_timelineHeight / 2, 0, m_timelineHeight, rect))
				return {static_cast<HitTestItem>(static_cast<size_t>(HitTestItem::CuePointFirst) + cue), sample.cues[cue]};
		}
		return {HitTestItem::Nothing, MAX_SAMPLE_LENGTH};
	} else
	{
		if(HasSelection() && !m_dwStatus[SMPSTATUS_DRAWING])
		{
			// Only one channel selected? Limit vertical range of selection marker
			int yMin = m_timelineHeight, yMax = m_rcClient.bottom, yCenter = m_timelineHeight + WaveformHeight() / 2;
			if(sample.GetNumChannels() == 2)
			{
				if(m_channelSelection == SampleChannelSelection::Left)
					yMax = yCenter;
				else if(m_channelSelection == SampleChannelSelection::Right)
					yMin = yCenter;
			}

			if(mpt::is_in_range(point.y, yMin, yMax))
			{
				const int margin = ui::ScalePixels(5, this);
				if(HitTest(point.x, SampleToScreen(m_dwBeginSel), margin, margin, yMin, yMax, rect))
					return {HitTestItem::SelectionStart, m_dwBeginSel};
				if(HitTest(point.x, SampleToScreen(m_dwEndSel), margin, margin, yMin, yMax, rect))
					return {HitTestItem::SelectionEnd, m_dwEndSel};
			}
		}
		return {HitTestItem::SampleData, ScreenToSample(point.x)};
	}
}


void CViewSample::InvalidateSample(bool invalidateWaveform)
{
	if(invalidateWaveform)
		m_forceRedrawWaveform = true;
	InvalidateRect(nullptr, false);
}


void CViewSample::InvalidateTimeline()
{
	auto rect = m_rcClient;
	rect.bottom = m_timelineHeight;
	InvalidateRect(&rect, false);
}


LResult CViewSample::OnModViewMsg(WParam wParam, LParam lParam)
{
	switch(wParam)
	{
	case VIEWMSG_SETCURRENTSAMPLE:
		SetZoom(static_cast<int>(lParam) >> 16);
		SetCurrentSample(lParam & 0xFFFF);
		break;

	case VIEWMSG_LOADSTATE:
		if (lParam)
		{
			SampleViewState *pState = (SampleViewState *)lParam;
			if (pState->nSample == m_nSample)
			{
				SetCurSel(pState->dwBeginSel, pState->dwEndSel, pState->channelSelection);
				SetScrollPos(SB_HORZ, pState->dwScrollPos);
				UpdateScrollSize();
				InvalidateSample();
			}
		}
		break;

	case VIEWMSG_SAVESTATE:
		if (lParam)
		{
			SampleViewState *pState = (SampleViewState *)lParam;
			pState->dwScrollPos = m_nScrollPosX;
			pState->dwBeginSel = m_dwBeginSel;
			pState->dwEndSel = m_dwEndSel;
			pState->channelSelection = m_channelSelection;
			pState->nSample = m_nSample;
		}
		break;

	case VIEWMSG_SETMODIFIED:
		// Update from OPL editor
		SetModified(UpdateHint::FromLPARAM(lParam).ToType<SampleHint>(), false, true);
		GetDocument()->UpdateOPLInstrument(m_nSample);
		break;

	case VIEWMSG_PREPAREUNDO:
		GetDocument()->GetSampleUndo().PrepareUndo(m_nSample, sundo_none, "Edit OPL Patch");
		break;

	default:
		return CModScrollView::OnModViewMsg(wParam, lParam);
	}
	return 0;
}


///////////////////////////////////////////////////////////////
// CViewSample drawing

void CViewSample::UpdateView(UpdateHint hint, HintObject *pObj)
{
	if(pObj == this)
	{
		return;
	}
	auto modDoc = GetDocument();
	auto &sndFile = modDoc->GetSoundFile();

	const SampleHint sampleHint = hint.ToType<SampleHint>();
	FlagSet<HintType> hintType = sampleHint.GetType();
	const SAMPLEINDEX updateSmp = sampleHint.GetSample();
	if(hintType[HINT_MPTOPTIONS | HINT_MODTYPE]
		|| (hintType[HINT_SAMPLEDATA] && (m_nSample == updateSmp || updateSmp == 0)))
	{
		if(hintType[HINT_SAMPLEDATA] && m_oplEditor && m_nSample <= sndFile.GetNumSamples())
		{
			ModSample &sample = sndFile.GetSample(m_nSample);
			if(sample.uFlags[CHN_ADLIB])
				m_oplEditor->SetPatch(sample.adlib);
		}
		UpdateOPLEditor();
		UpdateScrollSize();
		UpdateNcButtonState();
		InvalidateSample();
	}
	if(hintType[HINT_SAMPLEINFO])
	{
		// Sample rate change may imply redrawing of timeline
		if(m_nSample <= sndFile.GetNumSamples() && m_cachedSampleRate != sndFile.GetSample(m_nSample).GetSampleRate(sndFile.GetType()))
		{
			UpdateScrollSize();
			InvalidateTimeline();
		}
		if(m_nSample > sndFile.GetNumSamples() || !sndFile.GetSample(m_nSample).HasSampleData())
		{
			// Disable sample drawing if we cannot actually draw anymore.
			m_dwStatus.reset(SMPSTATUS_DRAWING);
			UpdateNcButtonState();
		}
		if(m_nSample == updateSmp || updateSmp == 0)
			InvalidateSample(false);
	}
}

#define YCVT(n, bits)		(ymed - (((n) * yrange) >> (bits)))


// Draw one channel of sample data, 1:1 ratio or higher (zoomed in)
template <typename Tsample>
void CViewSample::DrawSampleData1(ui::Painter * hdc, int ymed, int cx, int cy, SmpLength len, SampleFlags uFlags, const Tsample *psample)
{
	int yrange = cy/2;
	int y0 = 0;

	const int numChannels = (uFlags & CHN_STEREO) ? 2 : 1;
	MPT_ASSERT(sizeof(Tsample) == ((uFlags & CHN_16BIT) ? 2 : 1));

	if (uFlags & CHN_16BIT)
	{
		y0 = YCVT(*(psample-numChannels),15);
	} else
	{
		y0 = YCVT(*(psample-numChannels),7);
	}

	SmpLength numDrawSamples, loopDiv = 0;
	int loopShift = 0;
	if (m_nZoom == 1)
	{
		// Linear 1:1 scale
		numDrawSamples = cx;
	} else if(m_nZoom < 0)
	{
		// 2:1, 4:1, etc... zoom
		loopShift = (-m_nZoom - 1);
		// Round up
		numDrawSamples = (cx + (1 << loopShift) - 1) >> loopShift;
	} else
	{
		// Stretch to screen
		MPT_ASSERT(!m_nZoom);
		numDrawSamples = len;
		loopDiv = numDrawSamples;
	}
	LimitMax(numDrawSamples, len);

	const int x0 = loopDiv ? -static_cast<int>(cx / loopDiv) : (-1 << loopShift);
	hdc->MoveTo(x0, y0);

	if (uFlags & CHN_16BIT)
	{
		// 16-Bit
		for (SmpLength n = 0; n <= numDrawSamples; n++)
		{
			int x = loopDiv ? ((n * cx) / loopDiv) : (n << loopShift);
			int y = *psample;
			hdc->LineTo(x, YCVT(y, 15));
			psample += numChannels;
		}
	} else
	{
		// 8-bit
		for (SmpLength n = 0; n <= numDrawSamples; n++)
		{
			int x = loopDiv ? ((n * cx) / loopDiv) : (n << loopShift);
			int y = *psample;
			hdc->LineTo(x, YCVT(y, 7));
			psample += numChannels;
		}
	}
}


// Draw one channel of zoomed-out sample data
template <typename Tsample>
void CViewSample::DrawSampleData2(ui::Painter * hdc, int ymed, int cx, int cy, SmpLength len, SampleFlags uFlags, const Tsample *psample)
{
	int oldsmin, oldsmax;
	int yrange = cy/2;
	int32 y0 = 0, xmax;
	SmpLength poshi;
	uint64 posincr, posfrac;	// Increments have 16-bit fractional part

	if (len <= 0) return;
	const int numChannels = (uFlags & CHN_STEREO) ? 2 : 1;
	MPT_ASSERT(sizeof(Tsample) == ((uFlags & CHN_16BIT) ? 2 : 1));

	if (uFlags & CHN_16BIT)
	{
		y0 = YCVT(*(psample-numChannels), 15);
	} else
	{
		y0 = YCVT(*(psample-numChannels), 7);
	}
	oldsmin = oldsmax = y0;
	if (m_nZoom > 0)
	{
		xmax = len>>(m_nZoom-1);
		if (xmax > cx) xmax = cx;
		posincr = (uint64(1) << (m_nZoom-1+16));
	} else
	{
		xmax = cx;
		//posincr = Util::muldiv(len, 0x10000, cx);
		posincr = uint64(len) * uint64(0x10000) / uint64(cx);
	}
	hdc->MoveTo(0, ymed);
	posfrac = 0;
	poshi = 0;
	for (int x=0; x<xmax; x++)
	{
		//int smin, smax, scanlen;
		int smin, smax;
		SmpLength scanlen;

		posfrac += posincr;
		scanlen = static_cast<int32>((posfrac+0xffff) >> 16);
		if (poshi >= len) poshi = len-1;
		if (poshi + scanlen > len) scanlen = len-poshi;
		if (scanlen < 1) scanlen = 1;
		// 16-bit
		if (uFlags & CHN_16BIT)
		{
			const Tsample *p = psample + poshi * numChannels;
			auto minMax = SampleEdit::FindMinMax(p, scanlen, numChannels);
			smin = YCVT(minMax.first, 15);
			smax = YCVT(minMax.second, 15);
		} else
		// 8-bit
		{
			const Tsample *p = psample + poshi * numChannels;
			auto minMax = SampleEdit::FindMinMax(p, scanlen, numChannels);
			smin = YCVT(minMax.first, 7);
			smax = YCVT(minMax.second, 7);
		}
		if (smin > oldsmax)
		{
			hdc->MoveTo(x-1, oldsmax - 1);
			hdc->LineTo(x, smin);
		}
		if (smax < oldsmin)
		{
			hdc->MoveTo(x-1, oldsmin);
			hdc->LineTo(x, smax);
		}
		hdc->MoveTo(x, smax-1);
		hdc->LineTo(x, smin);
		oldsmin = smin;
		oldsmax = smax;
		poshi += static_cast<int32>(posfrac>>16);
		posfrac &= 0xffff;
	}
}


static void DrawTriangleHorz(ui::Painter &dc, int x, int width, int height, ColorRef color)
{
	const POINT points[] =
	{
		{x, 0},
		{x, height},
		{x + width, height / 2},
	};
	dc.SetDCPenColor(RGB(GetRValue(color) / 2, GetGValue(color) / 2, GetBValue(color) / 2));
	dc.SetDCBrushColor(color);
	dc.Polygon(points, static_cast<int>(std::size(points)));
}

static void DrawTriangleVert(ui::Painter &dc, int x, int width, int height, ColorRef color)
{
	const POINT points[] =
	{
		{x - width, height / 2},
		{x + width, height / 2},
		{x, height},
	};
	dc.SetDCPenColor(RGB(GetRValue(color) / 2, GetGValue(color) / 2, GetBValue(color) / 2));
	dc.SetDCBrushColor(color);
	dc.Polygon(points, static_cast<int>(std::size(points)));
}


void CViewSample::OnDraw(ui::Painter *pDC)
{
	const CModDoc *pModDoc = GetDocument();
	if ((!pModDoc) || (!pDC)) return;

	const Rect rcClient = m_rcClient;
	Rect rect, rc;
	const auto &colors = TrackerSettings::Instance().rgbCustomColors;
	const CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
	const ModSample &sample = sndFile.GetSample((m_nSample <= sndFile.GetNumSamples()) ? m_nSample : 0);
	const SmpLength smpScrollPos = std::min(ScrollPosToSamplePos(), sample.nLength);
	if(sample.uFlags[CHN_ADLIB])
	{
		CModScrollView::OnDraw(pDC);
		return;
	}
	LimitMax(m_dwBeginSel, sample.nLength);
	LimitMax(m_dwEndSel, sample.nLength);

	const ui::Font guiFont = ui::GetGuiFont();
	m_timelineFont = ui::Font(guiFont.face, std::max(1, mpt::saturate_round<int>(guiFont.size * 0.8)));
	if(m_waveformBitmap.GetWidth() != m_rcClient.Width() || m_waveformBitmap.GetHeight() != m_rcClient.Height())
		m_forceRedrawWaveform = true;

	pDC->SetPenColor(CMainFrame::penDarkGray);
	pDC->SetPenDotted(false);
	pDC->SetFont(m_timelineFont);

	// Draw timeline
	const int timelineHeight = TimelineHeight(this);
	if(timelineHeight != m_timelineHeight)
	{
		m_timelineHeight = timelineHeight;
		m_forceRedrawWaveform = true;
	}

	{
		const TimelineFormat format = TrackerSettings::Instance().sampleEditorTimelineFormat;
		Rect timeline = rcClient;
		timeline.bottom = timeline.top + timelineHeight + 1;
		pDC->FillSolidRect(timeline, ui::GetSystemColor(ui::SysColor::ButtonFace));
		pDC->DrawEtchedEdge(timeline, ui::EdgeBottom);
		pDC->SetTextColor(ui::GetSystemColor(ui::SysColor::ButtonText));
		pDC->SetBkMode(ui::TRANSPARENT);
		if(!m_timelineUnit)
			m_timelineUnit = 1;

		if(m_timelineInterval && sample.nLength)
		{
			rc = timeline;
			const auto sampleRate = sample.GetSampleRate(sndFile.GetType());
			const int textOffset = ui::ScalePixels(4, this);
			mpt::ustring text;
			for(int x = -(SampleToScreen(ScrollPosToSamplePos(), true) % m_timelineInterval); x < m_rcClient.right + m_timelineInterval; x += m_timelineInterval)
			{
				text.clear();
				if(format == TimelineFormat::Seconds && sampleRate)
				{
					const int64 time = mpt::saturate_round<int64>(std::round(ScreenToSeconds(x, true) * 1000.0 / m_timelineUnit) * m_timelineUnit);
					
					rc.left = SecondsToScreen(time / 1000.0);
					if(rc.left >= m_rcClient.right)
						break;

					const bool showSeconds = time >= 1000 || (time == 0 && m_timelineUnit >= 1000);
					const auto secMs = std::div(time, int64(1000));
					if(showSeconds)
					{
						const auto minSec = std::div(secMs.quot, int64(60));
						const bool showMinutes = minSec.quot != 0 || (time == 0 && m_timelineUnit >= 60000);
						if(showMinutes)
							text += mpt::ufmt::dec(3, UL_(","), minSec.quot) + UL_("mn");
						if(minSec.rem || !showMinutes)
							text += mpt::ufmt::dec(3, UL_(","), minSec.rem) + UL_("s");
					}
					if(secMs.rem || !showSeconds)
					{
						text += mpt::ufmt::val(secMs.rem) + UL_("ms");
					}
				} else
				{
					const SmpLength smp = mpt::saturate_round<SmpLength>(std::round(ScreenToSample(x, true) / static_cast<double>(m_timelineUnit)) * m_timelineUnit);

					rc.left = SampleToScreen(smp);
					if(rc.left >= m_rcClient.right)
						break;

					text += mpt::ufmt::dec(3, UL_(","), smp) + UL_(" smp");
				}

				rc.bottom = timelineHeight;
				for(int i = 0; i < 10; i++)
				{
					rect = rc;
					rect.left += i * m_timelineInterval / 10;
					if(i == 0)
						rect.top = 0;
					else if(i == 5)
						rect.top = timelineHeight / 2;
					else
						rect.top = timelineHeight - timelineHeight / 4;
					pDC->DrawEtchedEdge(rect, ui::EdgeLeft);
				}
				rc.bottom = timelineHeight / 2;
				rc.left += textOffset;
				pDC->DrawText(text.c_str(), rc, ui::TextVCenter | ui::TextSingleLine | ui::TextNoPrefix);
			}

			// Cues
						pDC->SetTextColor(RGB(0, 0, 0));
			const int arrowWidth = timelineHeight / 2;
			for(size_t i = 0; i < std::size(sample.cues); i++)
			{
				if(sample.cues[i] >= sample.nLength)
					continue;
				int xl = SampleToScreen(sample.cues[i]);
				if((xl >= -arrowWidth) && (xl < rcClient.right))
				{
					DrawTriangleVert(*pDC, xl, arrowWidth, timelineHeight, colors[MODCOLOR_SAMPLE_CUEPOINT]);
					rc.SetRect(xl - arrowWidth, timelineHeight / 2 - 1, xl + arrowWidth, timelineHeight);
					pDC->DrawText(mpt::ufmt::val(i + 1).c_str(), rc, ui::TextCenter | ui::TextSingleLine | ui::TextNoPrefix);
				}
			}

			// Loop Start/End
			if(sample.nLoopEnd > sample.nLoopStart)
			{
				int xl = SampleToScreen(sample.nLoopStart);
				if((xl >= -arrowWidth) && (xl <= rcClient.right + arrowWidth))
					DrawTriangleHorz(*pDC, xl, arrowWidth, timelineHeight, colors[MODCOLOR_SAMPLE_LOOPMARKER]);

				xl = SampleToScreen(sample.nLoopEnd);
				if((xl >= -arrowWidth) && (xl <= rcClient.right + arrowWidth))
					DrawTriangleHorz(*pDC, xl, -timelineHeight / 2, timelineHeight, colors[MODCOLOR_SAMPLE_LOOPMARKER]);
			}

			// Sustain Loop Start/End
			if(sample.nSustainEnd > sample.nSustainStart)
			{
				int xl = SampleToScreen(sample.nSustainStart);
				if((xl >= -arrowWidth) && (xl <= rcClient.right + arrowWidth))
					DrawTriangleHorz(*pDC, xl, timelineHeight / 2, timelineHeight, colors[MODCOLOR_SAMPLE_SUSTAINMARKER]);

				xl = SampleToScreen(sample.nSustainEnd);
				if((xl >= -arrowWidth) && (xl <= rcClient.right + arrowWidth))
					DrawTriangleHorz(*pDC, xl, -timelineHeight / 2, timelineHeight, colors[MODCOLOR_SAMPLE_SUSTAINMARKER]);
			}
		}
	}

	rect = rcClient;
	rect.top = timelineHeight;
	if((rcClient.bottom > rcClient.top) && (rcClient.right > rcClient.left))
	{
		const int ymed = mpt::midpoint(rect.top, rect.bottom);
		const int yrange = (rect.Height()) / 2;

		// Erase background
		if ((m_dwBeginSel < m_dwEndSel) && (m_dwEndSel > smpScrollPos))
		{
			rc = rect;
			if (m_dwBeginSel > smpScrollPos)
			{
				rc.right = SampleToScreen(m_dwBeginSel);
				if (rc.right > rcClient.right) rc.right = rcClient.right;
				if (rc.right > rc.left) pDC->FillSolidRect(rc, colors[MODCOLOR_BACKSAMPLE]);
				rc.left = rc.right;
			}
			if (rc.left < 0) rc.left = 0;
			rc.right = SampleToScreen(m_dwEndSel) + 1;
			if (rc.right > rcClient.right) rc.right = rcClient.right;
			if(rc.right > rc.left)
			{
				if(SampleEdit::IsSingleChannel(sample, m_channelSelection))
				{
					Rect rcLeft = rc, rcRight = rc;
					rcLeft.bottom = ymed;
					rcRight.top = ymed;
					pDC->FillSolidRect(rcLeft, m_channelSelection == SampleChannelSelection::Left ? colors[MODCOLOR_SAMPLESELECTED] : colors[MODCOLOR_BACKSAMPLE]);
					pDC->FillSolidRect(rcRight, m_channelSelection == SampleChannelSelection::Right ? colors[MODCOLOR_SAMPLESELECTED] : colors[MODCOLOR_BACKSAMPLE]);
				} else
				{
					pDC->FillSolidRect(rc, colors[MODCOLOR_SAMPLESELECTED]);
				}
			}
			rc.left = rc.right;
			if (rc.left < 0) rc.left = 0;
			rc.right = rcClient.right;
			if(rc.right > rc.left)
				pDC->FillSolidRect(rc, colors[MODCOLOR_BACKSAMPLE]);
		} else
		{
			pDC->FillSolidRect(rect, colors[MODCOLOR_BACKSAMPLE]);
		}
		pDC->SetPenColor(CMainFrame::penDarkGray);
			pDC->SetPenDotted(false);
		if (sample.uFlags[CHN_STEREO])
		{
			pDC->MoveTo(0, ymed - yrange / 2);
			pDC->LineTo(rcClient.right, ymed - yrange / 2);
			pDC->MoveTo(0, ymed + yrange / 2);
			pDC->LineTo(rcClient.right, ymed + yrange / 2);
		} else
		{
			pDC->MoveTo(0, ymed);
			pDC->LineTo(rcClient.right, ymed);
		}
		// Drawing sample
		if(sample.HasSampleData() && yrange && (sample.nLength > 1) && (rect.right > 1))
		{
			// Loop Start/End
			if ((sample.nLoopEnd > smpScrollPos) && (sample.nLoopEnd > sample.nLoopStart))
			{
				int xl = SampleToScreen(sample.nLoopStart);
				if ((xl >= 0) && (xl < rcClient.right))
				{
					pDC->MoveTo(xl, rect.top);
					pDC->LineTo(xl, rect.bottom);
				}

				xl = SampleToScreen(sample.nLoopEnd);
				if((xl >= 0) && (xl < rcClient.right))
				{
					pDC->MoveTo(xl, rect.top);
					pDC->LineTo(xl, rect.bottom);
				}
			}
			// Sustain Loop Start/End
			if ((sample.nSustainEnd > smpScrollPos) && (sample.nSustainEnd > sample.nSustainStart))
			{
				pDC->SetBkMode(ui::OPAQUE);
				pDC->SetBkColor(RGB(0xFF, 0xFF, 0xFF));
				pDC->SetPenColor(CMainFrame::penHalfDarkGray);
			pDC->SetPenDotted(true);
				int xl = SampleToScreen(sample.nSustainStart);
				if ((xl >= 0) && (xl < rcClient.right))
				{
					pDC->MoveTo(xl, rect.top);
					pDC->LineTo(xl, rect.bottom);
				}

				xl = SampleToScreen(sample.nSustainEnd);
				if ((xl >= 0) && (xl < rcClient.right))
				{
					pDC->MoveTo(xl, rect.top);
					pDC->LineTo(xl, rect.bottom);
				}
			}
			// Active cue point
			if(IsCuePoint(m_dragItem))
			{
				pDC->SetBkMode(ui::TRANSPARENT);
				pDC->SetPenColor(CMainFrame::penHalfDarkGray);
			pDC->SetPenDotted(true);
				int xl = SampleToScreen(sample.cues[CuePointFromItem(m_dragItem)]);
				if((xl >= 0) && (xl < rcClient.right))
				{
					pDC->MoveTo(xl, rect.top);
					pDC->LineTo(xl, rect.bottom);
				}
			}

			// Drawing Sample Data
			const auto backgroundCol = ~colors[MODCOLOR_SAMPLE];
			if(m_forceRedrawWaveform)
			{
				m_forceRedrawWaveform = false;
				m_waveformBuffer.Create(m_rcClient.Width(), m_rcClient.Height());
				m_waveformBuffer.Begin();
				ui::Painter waveformDC;
				waveformDC.FillSolidRect(rect, backgroundCol);
				waveformDC.SetPenColor(colors[MODCOLOR_SAMPLE]);
				if(m_nZoom == 1 || m_nZoom < 0 || ((!m_nZoom) && (sample.nLength <= (SmpLength)rect.Width())))
				{
					// Draw sample data in 1:1 ratio or higher (zoom in)
					SmpLength len = sample.nLength - smpScrollPos;
					if(sample.uFlags[CHN_16BIT])
					{
						const int16 *psample = sample.sample16() + smpScrollPos * sample.GetNumChannels();
						if(sample.uFlags[CHN_STEREO])
						{
							DrawSampleData1(&waveformDC, ymed - yrange / 2, rect.right, yrange, len, sample.uFlags, psample);
							DrawSampleData1(&waveformDC, ymed + yrange / 2, rect.right, yrange, len, sample.uFlags, psample + 1);
						} else
						{
							DrawSampleData1(&waveformDC, ymed, rect.right, yrange * 2, len, sample.uFlags, psample);
						}
					} else
					{
						const int8 *psample = sample.sample8() + smpScrollPos * sample.GetNumChannels();
						if(sample.uFlags[CHN_STEREO])
						{
							DrawSampleData1(&waveformDC, ymed - yrange / 2, rect.right, yrange, len, sample.uFlags, psample);
							DrawSampleData1(&waveformDC, ymed + yrange / 2, rect.right, yrange, len, sample.uFlags, psample + 1);
						} else
						{
							DrawSampleData1(&waveformDC, ymed, rect.right, yrange * 2, len, sample.uFlags, psample);
						}
					}
				} else
				{
					// Draw zoomed-out saple data
					SmpLength len = sample.nLength;
					int xscroll = 0;
					if(m_nZoom > 0)
					{
						xscroll = smpScrollPos;
						len -= smpScrollPos;
					}
					if(sample.uFlags[CHN_16BIT])
					{
						const int16 *psample = sample.sample16() + xscroll * sample.GetNumChannels();
						if(sample.uFlags[CHN_STEREO])
						{
							DrawSampleData2(&waveformDC, ymed - yrange / 2, rect.right, yrange, len, sample.uFlags, psample);
							DrawSampleData2(&waveformDC, ymed + yrange / 2, rect.right, yrange, len, sample.uFlags, psample + 1);
						} else
						{
							DrawSampleData2(&waveformDC, ymed, rect.right, yrange * 2, len, sample.uFlags, psample);
						}
					} else
					{
						const int8 *psample = sample.sample8() + xscroll * sample.GetNumChannels();
						if(sample.uFlags[CHN_STEREO])
						{
							DrawSampleData2(&waveformDC, ymed - yrange / 2, rect.right, yrange, len, sample.uFlags, psample);
							DrawSampleData2(&waveformDC, ymed + yrange / 2, rect.right, yrange, len, sample.uFlags, psample + 1);
						} else
						{
							DrawSampleData2(&waveformDC, ymed, rect.right, yrange * 2, len, sample.uFlags, psample);
						}
					}
				}
				m_waveformBitmap = m_waveformBuffer.ReadPixels();
				m_waveformBuffer.End();
				m_waveformBitmap.SetHasAlpha(true);
				uint32 *pixels = m_waveformBitmap.GetPixels();
				// Bitmap pixels are 0xRRGGBB, colour references 0xBBGGRR
				const uint32 transparentRgb = (uint32(GetRValue(backgroundCol)) << 16) | (uint32(GetGValue(backgroundCol)) << 8) | GetBValue(backgroundCol);
				const std::size_t pixelCount = static_cast<std::size_t>(m_waveformBitmap.GetWidth()) * static_cast<std::size_t>(m_waveformBitmap.GetHeight());
				for(std::size_t i = 0; i < pixelCount; ++i)
					pixels[i] = ((pixels[i] & 0xFFFFFF) == transparentRgb) ? 0 : (pixels[i] | 0xFF000000u);
			}
			pDC->DrawBitmap(m_waveformBitmap, rect.left, rect.top, rect.left, rect.top, rect.Width(), rect.Height());
		}
	}

	if(m_gridMode != SampleGridMode::NoGrid && sample.nLength != 0)
	{
		// Draw sample grid
		const double samplesPerSegment = GetGridSegmentSize(sample, sndFile);
		if(samplesPerSegment >= 1.0)
		{
			pDC->SetBkColor(TrackerSettings::Instance().rgbCustomColors[MODCOLOR_BACKSAMPLE]);
			pDC->SetPenColor(CMainFrame::penHalfDarkGray);
			pDC->SetPenDotted(true);
			const uint32 leftSegment = std::max(uint32(1), mpt::saturate_round<uint32>(ScreenToSample(rect.left) / samplesPerSegment));
			const uint32 rightSegment = mpt::saturate_round<uint32>(ScreenToSample(rect.right) / samplesPerSegment);
			int lastScreenPos = -1;
			for(uint32 i = leftSegment; i <= rightSegment; i++)
			{
				SmpLength samplePos = mpt::saturate_round<SmpLength>(samplesPerSegment * i);
				if(samplePos >= sample.nLength)
					break;
				int screenPos = SampleToScreen(samplePos);
				if(screenPos <= lastScreenPos)
					continue;
				pDC->MoveTo(screenPos, rect.top);
				pDC->LineTo(screenPos, rect.bottom);
				lastScreenPos = screenPos + 1;  // Leave at least one empty pixel row between lines, otherwise this makes no sense visually
			}
		}
	}

	DrawPositionMarks(*pDC);

	pDC->SetPenDotted(false);
}


void CViewSample::InvalidatePositionMarks()
{
	for(auto pos : m_dwNotifyPos) if(pos != Notification::PosInvalid)
	{
		const int x = SampleToScreen(pos);
		if(x >= 0 && x < m_rcClient.right)
			{
			const Rect markRect(x, m_timelineHeight, x + 1, m_rcClient.bottom + 1);
			InvalidateRect(&markRect);
		}
	}
}


void CViewSample::DrawPositionMarks(ui::Painter &dc)
{
	const ModSample &sample = GetDocument()->GetSoundFile().GetSample(m_nSample);
	if(!sample.HasSampleData() || sample.uFlags[CHN_ADLIB])
	{
		return;
	}
	Rect rect;
	for(auto pos : m_dwNotifyPos) if (pos != Notification::PosInvalid)
	{
		rect.top = m_timelineHeight;
		rect.left = SampleToScreen(pos);
		rect.right = rect.left + 1;
		rect.bottom = m_rcClient.bottom + 1;
		if ((rect.right >= 0) && (rect.right < m_rcClient.right)) dc.InvertRect(rect);
	}
}


LResult CViewSample::OnPlayerNotify(Notification *pnotify)
{
	CModDoc *pModDoc = GetDocument();
	if ((!pnotify) || (!pModDoc)) return 0;
	if (pnotify->type[Notification::Stop])
	{
		bool invalidate = false;
		for(auto &pos : m_dwNotifyPos)
		{
			if(pos != Notification::PosInvalid)
			{
				pos = Notification::PosInvalid;
				invalidate = true;
			}
		}
		if(invalidate)
			InvalidateSample(false);
	} else if (pnotify->type[Notification::Sample] && pnotify->item == m_nSample && !IsOPLInstrument())
	{
		if(m_dwNotifyPos != pnotify->pos)
		{
			InvalidatePositionMarks();
			m_dwNotifyPos = pnotify->pos;

			if(m_nZoom != 0 && TrackerSettings::Instance().m_followSamplePlayCursor != FollowSamplePlayCursor::DoNotFollow)
			{
				// Scroll sample into view if it's not in the visible range
				const CHANNELINDEX previewChannel = GetPreviewChannel();
				if(previewChannel != CHANNELINDEX_INVALID)
				{
					const SmpLength scrollToPos = m_dwNotifyPos[previewChannel];
					const auto screenPos = SampleToScreen(scrollToPos);
					const bool alwaysCenter = (TrackerSettings::Instance().m_followSamplePlayCursor == FollowSamplePlayCursor::FollowCentered);
					if(alwaysCenter || screenPos < m_rcClient.left || screenPos >= m_rcClient.right)
					{
						ScrollTarget target = ScrollTarget::Left;
						if(alwaysCenter)
							target = ScrollTarget::Center;
						else if(GetDocument()->GetSoundFile().m_PlayState.Chn[previewChannel].dwFlags[CHN_PINGPONGFLAG])  // TODO: this should be taken via notification, not directly from player state
							target = ScrollTarget::Right;
						ScrollToSample(scrollToPos, true, target);
					}
				}
			}

			InvalidatePositionMarks();
		}
	}
	return 0;
}


CHANNELINDEX CViewSample::GetPreviewChannel() const
{
	size_t count = 0;
	CHANNELINDEX channel = CHANNELINDEX_INVALID;
	const auto &playChns = GetDocument()->GetSoundFile().m_PlayState.Chn;
	for(CHANNELINDEX chn = 0; chn < MAX_CHANNELS; chn++)
	{
		if(m_dwNotifyPos[chn] == Notification::PosInvalid)
			continue;

		// Only update based on notes triggered by this view
		if(!playChns[chn].isPreviewNote)
			continue;
		if(!ModCommand::IsNote(playChns[chn].nNewNote) || m_noteChannel[playChns[chn].nNewNote - NOTE_MIN] != chn)
			continue;

		count++;
		if(count > 1)
			return CHANNELINDEX_INVALID;
		channel = chn;
	}
	return channel;
}


bool CViewSample::GetNcButtonRect(uint32 button, Rect &rect) const
{
	rect.left = 4;
	rect.top = 3;
	rect.bottom = rect.top + SMP_LEFTBAR_CYBTN;
	if(button >= SMP_LEFTBAR_BUTTONS) return false;
	for(uint32 i = 0; i < button; i++)
	{
		if(cLeftBarButtons[i] == ID_SEPARATOR)
			rect.left += SMP_LEFTBAR_CXSEP;
		else
			rect.left += SMP_LEFTBAR_CXBTN + SMP_LEFTBAR_CXSPC;
	}
	if(cLeftBarButtons[button] == ID_SEPARATOR)
	{
		rect.left += SMP_LEFTBAR_CXSEP/2 - 2;
		rect.right = rect.left + 2;
		return false;
	} else
	{
		rect.right = rect.left + SMP_LEFTBAR_CXBTN;
	}
	return true;
}


uint32 CViewSample::GetNcButtonAtPoint(Point point, Rect *outRect) const
{
	Rect rect;
	uint32 button = uint32_max;
	for(uint32 i = 0; i < SMP_LEFTBAR_BUTTONS; i++)
	{
		if(!(m_NcButtonState[i] & NCBTNS_DISABLED) && GetNcButtonRect(i, rect))
		{
			if(rect.PtInRect(point))
			{
				button = i;
				break;
			}
		}
	}
	if(outRect)
		*outRect = rect;
	return button;
}


void CViewSample::DrawNcButton(ui::Painter *pDC, uint32 nBtn)
{
	Rect rect;
	ColorRef crHi = ui::GetSystemColor(ui::SysColor::Highlight3d);
	ColorRef crDk = ui::GetSystemColor(ui::SysColor::Shadow3d);
	ColorRef crFc = ui::GetSystemColor(ui::SysColor::Face3d);
	ColorRef c1, c2;

	const bool flat = (TrackerSettings::Instance().patternSetup & PatternSetup::FlatToolbarButtons);
	if(GetNcButtonRect(nBtn, rect))
	{
		uint32 dwStyle = m_NcButtonState[nBtn];
		ColorRef c3, c4;
		int xofs = 0, yofs = 0, nImage = 0;

		c1 = c2 = c3 = c4 = crFc;
		if(!flat)
		{
			c1 = c3 = crHi;
			c2 = crDk;
			c4 = RGB(0,0,0);
		}
		if(dwStyle & (NCBTNS_PUSHED | NCBTNS_CHECKED))
		{
			c1 = crDk;
			c2 = crHi;
			if(!flat)
			{
				c4 = crHi;
				c3 = (dwStyle & NCBTNS_PUSHED) ? RGB(0,0,0) : crDk;
			}
			xofs = yofs = 1;
		} else
		if((dwStyle & NCBTNS_MOUSEOVER) && flat)
		{
			c1 = crHi;
			c2 = crDk;
		}
		switch(cLeftBarButtons[nBtn])
		{
		case ID_SAMPLE_ZOOMUP:		nImage = SIMAGE_ZOOMUP; break;
		case ID_SAMPLE_ZOOMDOWN:	nImage = SIMAGE_ZOOMDOWN; break;
		case ID_SAMPLE_DRAW:		nImage = SIMAGE_DRAW; break;
		case ID_SAMPLE_ADDSILENCE:	nImage = SIMAGE_RESIZE; break;
		case ID_SAMPLE_GRID:		nImage = SIMAGE_GRID; break;
		}
		pDC->Draw3dRect(Rect(rect.left-1, rect.top-1, (rect.left-1) + (SMP_LEFTBAR_CXBTN+2), (rect.top-1) + (SMP_LEFTBAR_CYBTN+2)), c3, c4);
		pDC->Draw3dRect(Rect(rect.left, rect.top, (rect.left) + (SMP_LEFTBAR_CXBTN), (rect.top) + (SMP_LEFTBAR_CYBTN)), c1, c2);
		rect.DeflateRect(1, 1);
		pDC->FillSolidRect(rect, crFc);
		rect.left += xofs;
		rect.top += yofs;
		if(dwStyle & NCBTNS_CHECKED)
			CMainFrame::GetMainFrame()->m_SampleIcons.Draw(*pDC, SIMAGE_CHECKED, rect.TopLeft());
		CMainFrame::GetMainFrame()->m_SampleIcons.Draw(*pDC, nImage, rect.TopLeft());
	} else
	{
		c1 = c2 = crFc;
		if(flat)
		{
			c1 = crDk;
			c2 = crHi;
		}
		pDC->Draw3dRect(Rect(rect.left, rect.top, (rect.left) + (2), (rect.top) + (SMP_LEFTBAR_CYBTN)), c1, c2);
	}
}


void CViewSample::OnNcPaint(ui::Painter &dc)
{
	Rect rect(0, 0, w(), SMP_LEFTBAR_CY);
	if(rect.left >= rect.right)
		return;
	dc.FillSolidRect(Rect(rect.left, rect.bottom - 1, rect.right, rect.bottom), ui::GetSystemColor(ui::SysColor::ButtonShadow));
	rect.bottom--;
	dc.FillSolidRect(rect, ui::GetSystemColor(ui::SysColor::ButtonFace));
	if(rect.top + 2 < rect.bottom)
	{
		for(uint32 i = 0; i < SMP_LEFTBAR_BUTTONS; i++)
		{
			DrawNcButton(&dc, i);
		}
	}
}


void CViewSample::UpdateNcButtonState()
{
	CModDoc *pModDoc = GetDocument();
	if (!pModDoc) return;
	for (uint32 i=0; i<SMP_LEFTBAR_BUTTONS; i++) if (cLeftBarButtons[i] != ID_SEPARATOR)
	{
		uint32 dwStyle = 0;

		if (m_nBtnMouseOver == i)
		{
			dwStyle |= NCBTNS_MOUSEOVER;
			if(m_dwStatus[SMPSTATUS_NCLBTNDOWN]) dwStyle |= NCBTNS_PUSHED;
		}

		switch(cLeftBarButtons[i])
		{
			case ID_SAMPLE_DRAW:
				if(m_dwStatus[SMPSTATUS_DRAWING]) dwStyle |= NCBTNS_CHECKED;
				if(m_nSample > pModDoc->GetNumSamples() || IsOPLInstrument())
				{
					dwStyle |= NCBTNS_DISABLED;
				}
				break;
			case ID_SAMPLE_ZOOMUP:
			case ID_SAMPLE_ZOOMDOWN:
			case ID_SAMPLE_GRID:
				if(IsOPLInstrument()) dwStyle |= NCBTNS_DISABLED;
				break;
		}

		if (dwStyle != m_NcButtonState[i])
		{
			m_NcButtonState[i] = dwStyle;
			Invalidate();
		}
	}
}


///////////////////////////////////////////////////////////////
// CViewSample messages

void CViewSample::OnSize(uint32 nType, int cx, int cy)
{
	CModScrollView::OnSize(nType, cx, cy);

	m_waveformBuffer.Destroy();
	m_waveformBitmap = ui::Bitmap();

	if (((nType == SIZE_RESTORED) || (nType == SIZE_MAXIMIZED)) && (cx > 0) && (cy > 0))
	{
		UpdateScrollSize();
	}
}


void CViewSample::ScrollToPosition(int x)    // logical coordinates
{
	Point pt;
	// now in device coordinates - limit if out of range
	int xMax = GetScrollLimit(SB_HORZ);
	pt.x = x;
	pt.y = 0;
	if (pt.x < 0)
		pt.x = 0;
	else if (pt.x > xMax)
		pt.x = xMax;
	CModScrollView::ScrollToPosition(pt);
}


template<class T>
T CViewSample::GetSampleValueFromPoint(const ModSample &smp, const Point &point) const
{
	static_assert(sizeof(T) < sizeof(int));
	const int channelHeight = WaveformHeight() / smp.GetNumChannels();
	const int yPos = point.y - m_drawChannel * channelHeight - m_timelineHeight;

	const int value = std::numeric_limits<T>::max() - (std::numeric_limits<T>::max() - std::numeric_limits<T>::min()) * yPos / channelHeight;
	return mpt::saturate_cast<T>(value);
}


template<class T>
void CViewSample::SetInitialDrawPoint(ModSample &smp, const Point &point)
{
	if(const auto waveformHeight = WaveformHeight(); waveformHeight > 0)
		m_drawChannel = (point.y - m_timelineHeight) * smp.GetNumChannels() / waveformHeight;
	else
		m_drawChannel = 0;
	Limit(m_drawChannel, 0, static_cast<int>(smp.GetNumChannels() - 1));

	T *data = static_cast<T *>(smp.samplev()) + m_drawChannel;
	data[m_dwEndDrag * smp.GetNumChannels()] = GetSampleValueFromPoint<T>(smp, point);
}


template<class T>
void CViewSample::SetSampleData(ModSample &smp, const Point &point, const SmpLength old)
{
	SmpLength x1 = old, x2 = m_dwEndDrag;
	T *data = static_cast<T *>(smp.samplev()) + m_drawChannel + x1 * smp.GetNumChannels();
	T v1 = *data, v2 = GetSampleValueFromPoint<T>(smp, point);
	if(x1 > x2)
	{
		data -= (x1 - x2) * smp.GetNumChannels();
		std::swap(x1, x2);
		std::swap(v1, v2);
	}

	const uint8 ptrInc = smp.GetNumChannels();
	for(SmpLength length = x2 - x1, remain = length; remain != 0; remain--, data += ptrInc)
	{
		*data = static_cast<T>(v2 + Util::muldivr(v1 - v2, remain, length));
	}
	*data = v2;
}


void CViewSample::OnMouseMove(uint32 flags, Point point)
{
	CModDoc *pModDoc = GetDocument();

	if(m_nBtnMouseOver < SMP_LEFTBAR_BUTTONS || m_dwStatus[SMPSTATUS_NCLBTNDOWN])
	{
		m_dwStatus.reset(SMPSTATUS_NCLBTNDOWN);
		m_nBtnMouseOver = 0xFFFF;
		UpdateNcButtonState();
		CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
		if (pMainFrm) pMainFrm->SetHelpText(UL_(""));
	}
	if(!pModDoc)
		return;
	CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
	if(m_nSample > sndFile.GetNumSamples())
		return;
	auto &sample = sndFile.GetSample(m_nSample);

	if (m_rcClient.PtInRect(point))
	{
		const SmpLength x = ScreenToSample(point.x);
		mpt::ustring(*fmt)(unsigned int, mpt::ustring, const SmpLength &) = &mpt::ufmt::dec<SmpLength>;
		if(TrackerSettings::Instance().cursorPositionInHex)
			fmt = &mpt::ufmt::HEX<SmpLength>;
		UpdateIndicator(MPT_UFORMAT("Cursor: {}")(fmt(3, UL_(","), x)));

		CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
		if(pMainFrm && !HasSelection())
		{
			// Show cursor position as offset effect if no selection is made.
			if(m_nSample > 0 && sample.HasSampleData() && x < sample.nLength)
			{
				const SmpLength xLow = (x / 0x100) % 0x100;
				const SmpLength xHigh = x / 0x10000;

				const char offsetChar = sndFile.GetModSpecifications().GetEffectLetter(CMD_OFFSET);
				const bool hasHighOffset = (sndFile.GetType() & (MOD_TYPE_S3M | MOD_TYPE_IT | MOD_TYPE_MPT | MOD_TYPE_XM));
				const char highOffsetChar = sndFile.GetModSpecifications().GetEffectLetter(static_cast<ModCommand::COMMAND>(sndFile.GetModSpecifications().HasCommand(CMD_S3MCMDEX) ? CMD_S3MCMDEX : CMD_XFINEPORTAUPDOWN));

				mpt::ustring s;
				if(xHigh == 0)
					s = ui::Format(UL_("Offset: %c%02X"), offsetChar, xLow);
				else if(hasHighOffset && xHigh < 0x10)
					s = ui::Format(UL_("Offset: %c%02X, %cA%X"), offsetChar, xLow, highOffsetChar, xHigh);
				else
					s = UL_("Beyond offset range");
				pMainFrm->SetInfoText(s);

				double linear;
				SmpLength offset = x * sample.GetNumChannels() + (point.y - m_timelineHeight) * sample.GetNumChannels() / std::max(1, WaveformHeight());
				if(sample.uFlags[CHN_16BIT])
					linear = sample.sample16()[offset] / 32768.0;
				else
					linear = sample.sample8()[offset] / 128.0;
				pMainFrm->SetXInfoText(MPT_UFORMAT("Value At Cursor: {}% / {}")(mpt::ufmt::fix(linear * 100.0, 3), CModDoc::LinearToDecibelsString(std::abs(linear), 1.0)).c_str());
			} else
			{
				pMainFrm->SetInfoText(UL_(""));
				pMainFrm->SetXInfoText(UL_(""));
			}
		}
	} else
	{
		UpdateIndicator(nullptr);
	}

	if(m_dwStatus[SMPSTATUS_MOUSEDRAG])
	{
		if(!m_dwStatus[SMPSTATUS_MOUSEMOVED] && m_startDragPoint == point)
			return;

		m_dwStatus.set(SMPSTATUS_MOUSEMOVED);
		const SmpLength len = sndFile.GetSample(m_nSample).nLength;
		if(!len)
			return;
		SmpLength old = m_dwEndDrag;
		if(m_nZoom)
		{
			if(point.x < 0)
			{
				Size pt(point.x, 0);
				if(OnScrollBy(pt))
				{
					UpdateWindow();
				}
				point.x = 0;
			}
			if (point.x > m_rcClient.right)
			{
				Size pt(point.x - m_rcClient.right, 0);
				if (OnScrollBy(pt))
				{
					UpdateWindow();
				}
				point.x = m_rcClient.right;
			}
		}

		// Note: point.x might have changed in if block above in case we're scrolling.
		SmpLength x;
		const bool dragItemIsInWaveform = (m_dragItem == HitTestItem::SampleData || m_dragItem == HitTestItem::SelectionStart || m_dragItem == HitTestItem::SelectionEnd);
		const bool zoomIn = (m_nZoom < 0 || (m_nZoom == 0 && sample.nLength < static_cast<SmpLength>(m_rcClient.Width())));
		if(m_dwStatus[SMPSTATUS_DRAWING])
		{
			// Do not snap to grid and adjust for mouse-down position when drawing
			x = ScreenToSample(point.x);
		} else if(m_fineDrag && !zoomIn)
		{
			x = m_startDragValue + (point.x - m_startDragPoint.x) / ui::ScalePixels(2, this);
		} else if(dragItemIsInWaveform && zoomIn)
		{
			// Don't adjust selection to mouse down point when zooming into the sample
			x = SnapToGrid(ScreenToSample(point.x));
		} else
		{
			x = SnapToGrid(ScreenToSample(SampleToScreen(m_startDragValue) + point.x - m_startDragPoint.x));
		}
		
		if((flags & ui::MouseShift) && !m_fineDrag)
		{
			m_fineDrag = true;
			m_startDragPoint = point;
			m_startDragValue = x;
		} else if(!(flags & ui::MouseShift) && (m_fineDrag || m_scrolledSinceLastMouseMove))
		{
			m_fineDrag = false;
			m_startDragPoint = point;
			m_startDragValue = ScreenToSample(point.x);
		}

		const bool moveLoop = (flags & ui::MouseControl);
		bool update = false;
		SmpLength *updateLoopPoint = nullptr;
		const char *updateLoopDesc = nullptr;
		SmpLength loopLength = 0;
		switch(m_dragItem)
		{
		case HitTestItem::SelectionStart:
		case HitTestItem::SelectionEnd:
			{
				// If the selection originated from only one stereo channel, switch to selecting both channels
				// when the cursor crosses into the other channel's half, and switch back if the cursor crosses
				// back into the starting channel's region.
				SampleChannelSelection newChannelSelection = m_startDragChannelSelection;
				if(SampleEdit::IsSingleChannel(sample, m_startDragChannelSelection))
				{
					if(m_startDragChannelSelection != GetChannelFromPoint(point, sample))
						newChannelSelection = SampleChannelSelection::Both;
				}
				if(m_dwEndDrag != x || newChannelSelection != m_channelSelection)
				{
					m_dwEndDrag = x;
					SetCurSel(m_dwBeginDrag, m_dwEndDrag, newChannelSelection);
					update = true;
				}
			}
			break;
		case HitTestItem::LoopStart:
			if(moveLoop)
			{
				updateLoopPoint = &sample.nLoopStart;
				updateLoopDesc = "Move Loop";
				loopLength = sample.nLoopEnd - sample.nLoopStart;
			} else if(x < sample.nLoopEnd)
			{
				updateLoopPoint = &sample.nLoopStart;
				updateLoopDesc = "Set Loop Start";
			}
			break;
		case HitTestItem::LoopEnd:
			if(moveLoop)
			{
				updateLoopPoint = &sample.nLoopStart;
				updateLoopDesc = "Move Loop";
				loopLength = sample.nLoopEnd - sample.nLoopStart;
				x = (x > loopLength) ? x - loopLength : 0;
			} else if(x > sample.nLoopStart)
			{
				updateLoopPoint = &sample.nLoopEnd;
				updateLoopDesc = "Set Loop End";
			}
			break;
		case HitTestItem::SustainStart:
			if(moveLoop)
			{
				updateLoopPoint = &sample.nSustainStart;
				updateLoopDesc = "Move Sustain Loop";
				loopLength = sample.nSustainEnd - sample.nSustainStart;
			} else if(x < sample.nSustainEnd)
			{
				updateLoopPoint = &sample.nSustainStart;
				updateLoopDesc = "Set Sustain Start";
			}
			break;
		case HitTestItem::SustainEnd:
			if(moveLoop)
			{
				updateLoopPoint = &sample.nSustainStart;
				updateLoopDesc = "Move Loop";
				loopLength = sample.nSustainEnd - sample.nSustainStart;
				x = (x > loopLength) ? x - loopLength : 0;
			} else if(x > sample.nSustainStart)
			{
				updateLoopPoint = &sample.nSustainEnd;
				updateLoopDesc = "Set Sustain End";
			}
			break;
		default:
			if(IsCuePoint(m_dragItem))
			{
				int cue = CuePointFromItem(m_dragItem);
				updateLoopPoint = &sample.cues[cue];
				updateLoopDesc ="Set Cue Point";
			}
			break;
		}

		if(loopLength)
			LimitMax(x, sample.nLength - loopLength);

		if(updateLoopPoint && updateLoopDesc && *updateLoopPoint != x)
		{
			if(!m_dragPreparedUndo)
				pModDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_none, updateLoopDesc);
			m_dragPreparedUndo = true;
			update = true;
			*updateLoopPoint = x;
			if(loopLength && updateLoopPoint == &sample.nLoopStart)
				sample.nLoopEnd = sample.nLoopStart + loopLength;
			else if(loopLength && updateLoopPoint == &sample.nSustainStart)
				sample.nSustainEnd = sample.nSustainStart + loopLength;
			sample.PrecomputeLoops(sndFile, true);
			SetModified(SampleHint().Info(), true, false);
		}

		if(m_dwStatus[SMPSTATUS_DRAWING] && m_dragItem == HitTestItem::SampleData)
		{
			m_dwEndDrag = x;
			if(m_dwEndDrag < len)
			{
				// Shift = draw horizontal lines
				if(flags & ui::MouseShift)
				{
					if(m_lastDrawPoint.y != -1)
						point.y = m_lastDrawPoint.y;
					m_lastDrawPoint = point;
				} else
				{
					m_lastDrawPoint.SetPoint(-1, -1);
				}

				LimitMax(old, sample.nLength);
				if(sample.GetElementarySampleSize() == 2)
					SetSampleData<int16>(sample, point, old);
				else if(sample.GetElementarySampleSize() == 1)
					SetSampleData<int8>(sample, point, old);

				sample.PrecomputeLoops(sndFile, false);

				InvalidateSample();
				SetModified(SampleHint().Data(), false, true);
			}
		} else if(update)
		{
			UpdateWindow();
		}
	}
	m_scrolledSinceLastMouseMove = false;
}


void CViewSample::OnLButtonDown(uint32 flags, Point point)
{
	CModDoc *pModDoc = GetDocument();

	if(m_dwStatus[SMPSTATUS_MOUSEDRAG] || (!pModDoc)) return;
	CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
	ModSample &sample = sndFile.GetSample(m_nSample);

	if (!sample.nLength)
		return;

	m_dwStatus.set(SMPSTATUS_MOUSEDRAG);
	m_dwStatus.reset(SMPSTATUS_MOUSEMOVED);
	SetFocus();
	SetCapture();
	SampleChannelSelection newSelection = m_channelSelection;
	bool oldsel = (m_dwBeginSel != m_dwEndSel);

	// shift + click = update selection
	const auto [item, itemPos] = PointToItem(point);
	if(!m_dwStatus[SMPSTATUS_DRAWING] && (flags & ui::MouseShift) && item == HitTestItem::SampleData)
	{
		oldsel = true;
		m_dwEndDrag = itemPos;

		// Extend single-channel selection into both channels if the shift-clicked point is inside the other channel region
		SampleChannelSelection newChannelSelection = m_channelSelection;
		if(SampleEdit::IsSingleChannel(sample, m_channelSelection))
		{
			if(m_channelSelection != GetChannelFromPoint(point, sample))
				newChannelSelection = SampleChannelSelection::Both;
		}

		SetCurSel(m_dwBeginDrag, m_dwEndDrag, newChannelSelection);
	} else
	{
		m_dragItem = item;
		m_startDragPoint = point;
		m_startDragValue = itemPos;
		m_fineDrag = (flags & ui::MouseShift);
		m_dragPreparedUndo = false;
		m_scrolledSinceLastMouseMove = false;

		switch(m_dragItem)
		{
		case HitTestItem::SampleData:
			m_dwBeginDrag = m_dwEndDrag = ScreenToSample(point.x);
			if(!m_dwStatus[SMPSTATUS_DRAWING])
				m_dragItem = HitTestItem::SelectionEnd;
			m_startDragChannelSelection = newSelection = GetChannelSelectionFromPoint(point, sample);
			oldsel = true;
			break;
		case HitTestItem::SelectionStart:
			m_dwBeginDrag = m_dwEndSel;
			m_dwEndDrag = itemPos;
			if(m_channelSelection == SampleChannelSelection::Both)
				m_startDragChannelSelection = GetChannelSelectionFromPoint(point, sample);
			else
				m_startDragChannelSelection = m_channelSelection;  // Continue dragging just this channel even if mouse is now in the "both channels" area
			break;
		case HitTestItem::SelectionEnd:
			m_dwBeginDrag = m_dwBeginSel;
			m_dwEndDrag = itemPos;
 			if(m_channelSelection == SampleChannelSelection::Both)
 				m_startDragChannelSelection = GetChannelSelectionFromPoint(point, sample);
 			else
				m_startDragChannelSelection = m_channelSelection;  // Continue dragging just this channel even if mouse is now in the "both channels" area
			break;
		default:
			if(IsCuePoint(m_dragItem))
				InvalidateSample(false);
			break;
		}
	}
	if(oldsel || newSelection != m_channelSelection)
		SetCurSel(m_dwBeginDrag, m_dwEndDrag, newSelection);

	// set initial point for sample drawing
	if (m_dwStatus[SMPSTATUS_DRAWING] && m_dragItem == HitTestItem::SampleData)
	{
		m_lastDrawPoint = point;
		pModDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_replace, "Draw Sample");
		if(sample.GetElementarySampleSize() == 2)
			SetInitialDrawPoint<int16>(sample, point);
		else if(sample.GetElementarySampleSize() == 1)
			SetInitialDrawPoint<int8>(sample, point);

		sndFile.GetSample(m_nSample).PrecomputeLoops(sndFile, false);

		InvalidateSample();
		SetModified(SampleHint().Data(), false, true);
	} else
	{
		// ctrl + click = play from cursor pos
		if((flags & ui::MouseControl) && point.y >= m_timelineHeight)
			PlayNote(NOTE_MIDDLEC, ScreenToSample(point.x));
	}
}


void CViewSample::OnLButtonUp(uint32, Point)
{
	if(m_dwStatus[SMPSTATUS_MOUSEDRAG])
	{
		m_dwStatus.reset(SMPSTATUS_MOUSEDRAG);
		ReleaseCapture();
	}
	if(IsCuePoint(m_dragItem))
		InvalidateSample(false);
	m_dragItem = HitTestItem::Nothing;
	m_startDragValue = MAX_SAMPLE_LENGTH;
	m_lastDrawPoint.SetPoint(-1, -1);
}


void CViewSample::OnLButtonDblClk(uint32, Point pt)
{
	CModDoc *modDoc = GetDocument();
	if(!modDoc)
		return;

	auto &sample = modDoc->GetSoundFile().GetSample(m_nSample);
	if(pt.y < m_timelineHeight)
	{
		const auto item = PointToItem(pt).first;
		const char *undoName = "";
		mpt::ustring name;
		SmpLength minVal = 0, maxVal = sample.nLength;
		SmpLength *target = nullptr;
		switch(item)
		{
		case HitTestItem::LoopStart:
			undoName = "Set Loop Start";
			name = UL_("Loop Start");
			target = &sample.nLoopStart;
			if(sample.uFlags[CHN_LOOP])
				maxVal = sample.nLoopEnd;
			break;
		case HitTestItem::LoopEnd:
			undoName = "Set Loop End";
			name = UL_("Loop End");
			if(sample.uFlags[CHN_LOOP])
				minVal = sample.nLoopStart;
			target = &sample.nLoopEnd;
			break;
		case HitTestItem::SustainStart:
			undoName = "Set Sustain Loop Start";
			name = UL_("Sustain Loop Start");
			target = &sample.nSustainStart;
			if(sample.uFlags[CHN_SUSTAINLOOP])
				maxVal = sample.nSustainEnd;
			break;
		case HitTestItem::SustainEnd:
			undoName = "Set Sustain Loop End";
			name = UL_("Sustain Loop End");
			target = &sample.nSustainEnd;
			if(sample.uFlags[CHN_SUSTAINLOOP])
				minVal = sample.nSustainStart;
			break;
		default:
			if(IsCuePoint(item))
			{
				undoName = "Set Cue Point";
				name = ui::Format(UL_("Cue Point %d"), CuePointFromItem(item) + 1);
				target = &sample.cues[CuePointFromItem(item)];
				maxVal = sample.nLength - 1u;
			}
			break;
		}
		
		if(!target)
			return;

		CInputDlg dlg{this, UL_("Enter new position of ") + name, static_cast<int32>(minVal), static_cast<int32>(maxVal), static_cast<int32>(*target)};
		if(dlg.DoModal() == IDOK)
		{
			modDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_none, undoName);
			*target = dlg.resultAsInt;
			switch(item)
			{
			case HitTestItem::LoopStart:
				if(!sample.uFlags[CHN_LOOP] && sample.nLoopEnd <= sample.nLoopStart)
					sample.nLoopEnd = sample.nLength;
				break;
			case HitTestItem::LoopEnd:
				if(!sample.uFlags[CHN_LOOP] && sample.nLoopEnd <= sample.nLoopStart)
					sample.nLoopStart = 0;
				break;
			case HitTestItem::SustainStart:
				if(!sample.uFlags[CHN_SUSTAINLOOP] && sample.nSustainEnd <= sample.nSustainStart)
					sample.nSustainEnd = sample.nLength;
				break;
			case HitTestItem::SustainEnd:
				if(!sample.uFlags[CHN_SUSTAINLOOP] && sample.nSustainEnd <= sample.nSustainStart)
					sample.nSustainStart = 0;
				break;
			default:
				break;
			}
			sample.PrecomputeLoops(modDoc->GetSoundFile());
			SetModified(SampleHint().Info(), true, false);
		}
	} else
	{
		SmpLength len = sample.nLength;
		if(len && !m_dwStatus[SMPSTATUS_DRAWING])
			SetCurSel(0, len, GetChannelSelectionFromPoint(pt, sample));
	}
}


void CViewSample::OnRButtonUp(uint32, Point pt)
{
	CModDoc *pModDoc = GetDocument();
	if(pModDoc)
	{
		const CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
		const ModSample &sample = sndFile.GetSample(m_nSample);
		HMENU hMenu = ui::CreatePopupMenu();
		CInputHandler* ih = CMainFrame::GetInputHandler();
		if(!hMenu)
			return;

		mpt::uchar s[256];

		if(pt.y < m_timelineHeight)
		{
			const auto item = PointToItem(pt).first;
			if(IsCuePoint(item))
			{
				m_dwMenuParam = CuePointFromItem(item);
				wsprintf(s, UL_("&Delete Cue Point %d"), 1 + static_cast<int>(m_dwMenuParam));
				ui::AppendMenu(hMenu, ui::MenuItemString, ID_SAMPLE_DELETE_CUEPOINT, s);
				ui::AppendMenu(hMenu, ui::MenuItemSeparator, 0, UL_(""));
			} else
			{
				if(*std::max_element(sample.cues.begin(), sample.cues.end()) >= sample.nLength)
				{
					m_dwMenuParam = ScreenToSample(pt.x);
					wsprintf(s, UL_("&Insert Cue Point at %s"), mpt::ufmt::dec(3, UL_(","), m_dwMenuParam).c_str());
					ui::AppendMenu(hMenu, ui::MenuItemString, ID_SAMPLE_INSERT_CUEPOINT, s);
					ui::AppendMenu(hMenu, ui::MenuItemSeparator, 0, UL_(""));
				}
			}

			auto fmt = TrackerSettings::Instance().sampleEditorTimelineFormat.Get();
			ui::AppendMenu(hMenu, ui::MenuItemString | (fmt == TimelineFormat::Seconds ? ui::MenuItemChecked : 0), ID_SAMPLE_TIMELINE_SECONDS, UL_("&Seconds"));
			ui::AppendMenu(hMenu, ui::MenuItemString | (fmt == TimelineFormat::Samples ? ui::MenuItemChecked : 0), ID_SAMPLE_TIMELINE_SAMPLES, UL_("S&amples"));
			ui::AppendMenu(hMenu, ui::MenuItemString | (fmt == TimelineFormat::SamplesPow2 ? ui::MenuItemChecked : 0), ID_SAMPLE_TIMELINE_SAMPLES_POW2, UL_("Samples (&Power of 2)"));
			ClientToScreen(&pt);
			ui::TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, this, NULL);
			ui::DestroyMenu(hMenu);
			return;
		}

		if (sample.HasSampleData() && !sample.uFlags[CHN_ADLIB])
		{
			m_menuChannelSelection = GetChannelSelectionFromPoint(pt, sample);
			if(m_dwEndSel >= m_dwBeginSel + 4)
			{
				ui::AppendMenu(hMenu, ui::MenuItemString | (CanZoomSelection() ? 0 : ui::MenuItemGrayed), ID_SAMPLE_ZOOMONSEL, ih->GetKeyTextFromCommand(kcSampleZoomSelection, UL_("&Zoom")));
				ui::AppendMenu(hMenu, ui::MenuItemString, ID_SAMPLE_SETLOOP, UL_("Set As &Loop"));
				if (sndFile.GetType() & (MOD_TYPE_IT|MOD_TYPE_MPT))
					ui::AppendMenu(hMenu, ui::MenuItemString, ID_SAMPLE_SETSUSTAINLOOP, UL_("Set As &Sustain Loop"));
				ui::AppendMenu(hMenu, ui::MenuItemString, ID_SAMPLE_SEND_TO_NEW_SLOT, ih->GetKeyTextFromCommand(kcSampleSendSelectionToNew, UL_("Send to &New Sample Slot")));
				ui::AppendMenu(hMenu, ui::MenuItemSeparator, 0, UL_(""));
			} else
			{
				SmpLength dwPos = ScreenToSample(pt.x);
				mpt::ustring pos = mpt::ufmt::dec(3, UL_(","), dwPos);
				if (dwPos <= sample.nLength)
				{
					//Set loop points
					SmpLength loopEnd = (sample.nLoopEnd > 0) ? sample.nLoopEnd : sample.nLength;
					wsprintf(s, UL_("Set &Loop Start to:\t%s"), pos.c_str());
					ui::AppendMenu(hMenu, ui::MenuItemString | (dwPos + 4 <= loopEnd ? 0 : ui::MenuItemGrayed),
						ID_SAMPLE_SETLOOPSTART, s);
					wsprintf(s, UL_("Set &Loop End to:\t%s"), pos.c_str());
					ui::AppendMenu(hMenu, ui::MenuItemString | (dwPos >= sample.nLoopStart + 4 ? 0 : ui::MenuItemGrayed),
						ID_SAMPLE_SETLOOPEND, s);
					if(sample.HasPingPongLoop())
						ui::AppendMenu(hMenu, ui::MenuItemString, ID_CONVERT_PINGPONG_LOOP, ih->GetKeyTextFromCommand(kcSampleConvertPingPongLoop, UL_("Convert to Unidirectional Loop")));
					if(sample.HasLoop() && (sndFile.GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT)))
						ui::AppendMenu(hMenu, ui::MenuItemString, ID_CONVERT_NORMAL_TO_SUSTAIN, ih->GetKeyTextFromCommand(kcSampleConvertNormalLoopToSustain, UL_("Convert to Sustain Loop")));

					if (sndFile.GetType() & (MOD_TYPE_IT|MOD_TYPE_MPT))
					{
						//Set sustain loop points
						SmpLength sustainEnd = (sample.nSustainEnd > 0) ? sample.nSustainEnd : sample.nLength;
						ui::AppendMenu(hMenu, ui::MenuItemSeparator, 0, UL_(""));
						wsprintf(s, UL_("Set &Sustain Start to:\t%s"), pos.c_str());
						ui::AppendMenu(hMenu, ui::MenuItemString | (dwPos + 4 <= sustainEnd ? 0 : ui::MenuItemGrayed),
							ID_SAMPLE_SETSUSTAINSTART, s);
						wsprintf(s, UL_("Set &Sustain End to:\t%s"), pos.c_str());
						ui::AppendMenu(hMenu, ui::MenuItemString | (dwPos >= sample.nSustainStart + 4 ? 0 : ui::MenuItemGrayed),
							ID_SAMPLE_SETSUSTAINEND, s);
						if(sample.HasPingPongSustainLoop())
							ui::AppendMenu(hMenu, ui::MenuItemString, ID_CONVERT_PINGPONG_SUSTAIN, ih->GetKeyTextFromCommand(kcSampleConvertPingPongSustain, UL_("Convert to Unidirectional Sustain Loop")));
						if(sample.HasSustainLoop())
							ui::AppendMenu(hMenu, ui::MenuItemString, ID_CONVERT_SUSTAIN_TO_NORMAL, ih->GetKeyTextFromCommand(kcSampleConvertSustainLoopToNormal, UL_("Convert to Normal Loop")));
					}

					//if(sndFile.GetModSpecifications().HasVolCommand(VOLCMD_OFFSET))
					{
						// Sample cues
						ui::AppendMenu(hMenu, ui::MenuItemSeparator, 0, UL_(""));
						HMENU hCueMenu = ui::CreatePopupMenu();
						bool hasValidCues = false;
						for(std::size_t i = 0; i < std::size(sample.cues); i++)
						{
							const SmpLength cue = sample.cues[i];
							wsprintf(s, UL_("Cue &%d: %s"), 1 + static_cast<int>(i),
								cue < sample.nLength ? mpt::ufmt::dec(3, UL_(","), cue).c_str() : UL_("unused"));
							ui::AppendMenu(hCueMenu, ui::MenuItemString, ID_SAMPLE_CUE_1 + i, s);
							if(cue > 0 && cue < sample.nLength) hasValidCues = true;
						}
						wsprintf(s, UL_("Set Sample Cu&e to:\t%s"), pos.c_str());
						ui::AppendMenu(hMenu, ui::MenuItemPopup, reinterpret_cast<uintptr_t>(hCueMenu), s);
						ui::AppendMenu(hMenu, ui::MenuItemString | (hasValidCues ? 0 : ui::MenuItemGrayed), ID_SAMPLE_SLICE, ih->GetKeyTextFromCommand(kcSampleSliceCuePoints, UL_("Slice at cue points")));
						if(m_gridMode != SampleGridMode::NoGrid)
							ui::AppendMenu(hMenu, ui::MenuItemString, ID_SAMPLE_SLICE_GRID, ih->GetKeyTextFromCommand(kcSampleSliceGrid, UL_("Slice at grid")));
					}

					ui::AppendMenu(hMenu, ui::MenuItemSeparator, 0, UL_(""));
					m_dwMenuParam = dwPos;
				}
			}

			if(sample.GetElementarySampleSize() > 1) ui::AppendMenu(hMenu, ui::MenuItemString, ID_SAMPLE_8BITCONVERT, ih->GetKeyTextFromCommand(kcSample8Bit, UL_("Convert to &8-bit")));
			else ui::AppendMenu(hMenu, ui::MenuItemString, ID_SAMPLE_16BITCONVERT, ih->GetKeyTextFromCommand(kcSample8Bit, UL_("Convert to &16-bit")));
			if(sample.GetNumChannels() > 1)
			{
				HMENU hMonoMenu = ui::CreatePopupMenu();
				ui::AppendMenu(hMonoMenu, ui::MenuItemString, ID_SAMPLE_MONOCONVERT, ih->GetKeyTextFromCommand(kcSampleMonoMix, UL_("&Mix Channels")));
				ui::AppendMenu(hMonoMenu, ui::MenuItemString, ID_SAMPLE_MONOCONVERT_LEFT, ih->GetKeyTextFromCommand(kcSampleMonoLeft, UL_("&Left Channel")));
				ui::AppendMenu(hMonoMenu, ui::MenuItemString, ID_SAMPLE_MONOCONVERT_RIGHT, ih->GetKeyTextFromCommand(kcSampleMonoRight, UL_("&Right Channel")));
				ui::AppendMenu(hMonoMenu, ui::MenuItemString, ID_SAMPLE_MONOCONVERT_SPLIT, ih->GetKeyTextFromCommand(kcSampleMonoSplit, UL_("&Split Sample")));
				ui::AppendMenu(hMenu, ui::MenuItemPopup, reinterpret_cast<uintptr_t>(hMonoMenu), UL_("Convert to &Mono"));
			} else
			{
				ui::AppendMenu(hMenu, ui::MenuItemString, ID_SAMPLE_STEREOCONVERT, ih->GetKeyTextFromCommand(kcSampleStereo, UL_("Convert to Stere&o")));
			}

			// "Trim" menu item is responding differently if there's no selection,
			// but a loop present: "trim around loop point"! (jojo in topic 2258)
			mpt::ustring trimMenuText = UL_("Tr&im");
			bool isGrayed = (!HasSelection() || (m_dwEndSel - m_dwBeginSel < MIN_TRIM_LENGTH)
								|| (m_dwEndSel - m_dwBeginSel == sample.nLength));

			if ((m_dwBeginSel == m_dwEndSel) && (sample.nLoopStart < sample.nLoopEnd))
			{
				// no selection => use loop points
				trimMenuText += UL_(" around loop points");
				// Check whether trim menu item can be enabled (loop not too short or long for trimming).
				if( (sample.nLoopEnd <= sample.nLength) &&
					(sample.nLoopEnd - sample.nLoopStart >= MIN_TRIM_LENGTH) &&
					(sample.nLoopEnd - sample.nLoopStart < sample.nLength) )
					isGrayed = false;
			}

			ui::AppendMenu(hMenu, ui::MenuItemString | (isGrayed ? ui::MenuItemGrayed : 0), ID_SAMPLE_TRIM, ih->GetKeyTextFromCommand(kcSampleTrim, trimMenuText));
			if((m_dwBeginSel == 0 && m_dwEndSel != 0) || (m_dwBeginSel < sample.nLength && m_dwEndSel == sample.nLength))
			{
				ui::AppendMenu(hMenu, ui::MenuItemString, ID_SAMPLE_QUICKFADE, ih->GetKeyTextFromCommand(kcSampleQuickFade, UL_("Quick &Fade")));
			}
			ui::AppendMenu(hMenu, ui::MenuItemString, ID_EDIT_CUT, ih->GetKeyTextFromCommand(kcEditCut, UL_("Cu&t")));
			ui::AppendMenu(hMenu, ui::MenuItemString, ID_EDIT_COPY, ih->GetKeyTextFromCommand(kcEditCopy, UL_("&Copy")));
		}

		const uint32 clipboardFlag = (ui::IsClipboardFormatAvailable(ui::ClipboardWave) ? 0 : ui::MenuItemGrayed);
		mpt::ustring paste = UL_("Paste");
		if(m_menuChannelSelection == SampleChannelSelection::Left)
			paste += UL_(" into Left channel");
		else if(m_menuChannelSelection == SampleChannelSelection::Right)
			paste += UL_(" into Right channel");
		ui::AppendMenu(hMenu, ui::MenuItemString | clipboardFlag, ID_EDIT_PASTE, ih->GetKeyTextFromCommand(kcEditPaste, UL_("&Paste (Replace)")));
		ui::AppendMenu(hMenu, ui::MenuItemString | clipboardFlag, ID_EDIT_PUSHFORWARDPASTE, ih->GetKeyTextFromCommand(kcEditPushForwardPaste, paste + UL_(" (&Insert)")));
		ui::AppendMenu(hMenu, ui::MenuItemString | clipboardFlag, ID_EDIT_MIXPASTE, ih->GetKeyTextFromCommand(kcEditMixPaste, UL_("Mi&x " + paste)));

		ui::AppendMenu(hMenu, ui::MenuItemString | (pModDoc->GetSampleUndo().CanUndo(m_nSample) ? 0 : ui::MenuItemGrayed), ID_EDIT_UNDO, ih->GetKeyTextFromCommand(kcEditUndo, UL_("&Undo ") + mpt::ToUnicode(pModDoc->GetSoundFile().GetCharsetInternal(), pModDoc->GetSampleUndo().GetUndoName(m_nSample))));
		ui::AppendMenu(hMenu, ui::MenuItemString | (pModDoc->GetSampleUndo().CanRedo(m_nSample) ? 0 : ui::MenuItemGrayed), ID_EDIT_REDO, ih->GetKeyTextFromCommand(kcEditRedo, UL_("&Redo ") + mpt::ToUnicode(pModDoc->GetSoundFile().GetCharsetInternal(), pModDoc->GetSampleUndo().GetRedoName(m_nSample))));

		ClientToScreen(&pt);
		ui::TrackPopupMenu(hMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, pt.x, pt.y, 0, this, NULL);
		ui::DestroyMenu(hMenu);
	}
}


void CViewSample::OnNcMouseMove(Point point)
{
	const auto button = GetNcButtonAtPoint(point);
	if(button != m_nBtnMouseOver)
	{
		CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
		if(pMainFrm)
		{
			mpt::ustring strText;
			if(button < SMP_LEFTBAR_BUTTONS && cLeftBarButtons[button] != ID_SEPARATOR)
			{
				strText = LoadResourceString(cLeftBarButtons[button]);
			}
			pMainFrm->SetHelpText(strText);
		}
		m_nBtnMouseOver = button;
		UpdateNcButtonState();
		Rect buttonRect;
		mpt::ustring buttonText;
		if(FindNcButtonToolTip(button, buttonRect, buttonText))
			ShowToolTip(buttonRect, buttonText);
	}
	CModScrollView::OnNcMouseMove(point);
}


void CViewSample::OnNcLButtonDown(uint32 uFlags, Point point)
{
	if (m_nBtnMouseOver < SMP_LEFTBAR_BUTTONS)
	{
		m_dwStatus.set(SMPSTATUS_NCLBTNDOWN);
		if (cLeftBarButtons[m_nBtnMouseOver] != ID_SEPARATOR)
		{
			PostCommand(cLeftBarButtons[m_nBtnMouseOver]);
			UpdateNcButtonState();
		}
	}
	CModScrollView::OnNcLButtonDown(uFlags, point);
}


void CViewSample::OnNcLButtonUp(uint32 uFlags, Point point)
{
	if(m_dwStatus[SMPSTATUS_NCLBTNDOWN])
	{
		m_dwStatus.reset(SMPSTATUS_NCLBTNDOWN);
		UpdateNcButtonState();
	}
	CModScrollView::OnNcLButtonUp(uFlags, point);
}


void CViewSample::OnPrevInstrument()
{
	SendCtrlMessage(CTRLMSG_SMP_PREVINSTRUMENT);
}


void CViewSample::OnNextInstrument()
{
	SendCtrlMessage(CTRLMSG_SMP_NEXTINSTRUMENT);
}


void CViewSample::OnSetLoop()
{
	CModDoc *pModDoc = GetDocument();
	if (pModDoc)
	{
		CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
		ModSample &sample = sndFile.GetSample(m_nSample);
		if ((m_dwEndSel > m_dwBeginSel + 15) && (m_dwEndSel <= sample.nLength))
		{
			if ((sample.nLoopStart != m_dwBeginSel) || (sample.nLoopEnd != m_dwEndSel))
			{
				pModDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_none, "Set Loop");
				sample.SetLoop(m_dwBeginSel, m_dwEndSel, true, sample.uFlags[CHN_PINGPONGLOOP], sndFile);
				SetModified(SampleHint().Info(), true, false);
			}
		}
	}
}


void CViewSample::OnSetSustainLoop()
{
	CModDoc *pModDoc = GetDocument();
	if (pModDoc)
	{
		CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
		ModSample &sample = sndFile.GetSample(m_nSample);
		if ((m_dwEndSel > m_dwBeginSel + 15) && (m_dwEndSel <= sample.nLength))
		{
			if ((sample.nSustainStart != m_dwBeginSel) || (sample.nSustainEnd != m_dwEndSel))
			{
				pModDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_none, "Set Sustain Loop");
				sample.SetSustainLoop(m_dwBeginSel, m_dwEndSel, true, sample.uFlags[CHN_PINGPONGSUSTAIN], sndFile);
				SetModified(SampleHint().Info(), true, false);
			}
		}
	}
}


void CViewSample::OnEditSelectAll()
{
	CModDoc *pModDoc = GetDocument();
	if (pModDoc)
	{
		SmpLength len = pModDoc->GetSoundFile().GetSample(m_nSample).nLength;
		if(len)
			SetCurSel(0, len, SampleChannelSelection::Both);
	}
}


void CViewSample::OnEditDelete()
{
	CModDoc *pModDoc = GetDocument();
	if(!pModDoc)
		return;

	SampleHint updateHint;
	updateHint.Info().Data();

	CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
	ModSample &sample = sndFile.GetSample(m_nSample);
	if(!sample.HasSampleData())
		return;

	LimitMax(m_dwEndSel, sample.nLength);
	if(m_dwBeginSel >= m_dwEndSel ||
		(!SampleEdit::IsSingleChannel(sample, m_channelSelection) && (m_dwEndSel - m_dwBeginSel + 4 >= sample.nLength)))
	{
		if(Reporting::Confirm("Remove this sample?", "Remove Sample", true) != cnfYes)
			return;
		pModDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_replace, "Delete Sample");
		sndFile.DestroySampleThreadsafe(m_nSample);
		updateHint.Names();
	} else
	{
		pModDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_delete, "Delete Selection", m_dwBeginSel, m_dwEndSel, m_channelSelection);

		TrackerCriticalSection cs;
		SampleEdit::RemoveRange(sample, m_dwBeginSel, m_dwEndSel, m_channelSelection, sndFile);
	}
	SetCurSel(0, 0);
	SetModified(updateHint, true, true);
}


void CViewSample::OnEditCut()
{
	OnEditCopy();
	OnEditDelete();
}


void CViewSample::OnEditCopy()
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if(pMainFrm == nullptr || GetDocument() == nullptr)
	{
		return;
	}

	const CTrackerSoundFile &sndFile = GetDocument()->GetSoundFile();
	const ModSample &sample = sndFile.GetSample(m_nSample);

	if(sample.uFlags[CHN_ADLIB])
	{
		// We cannot store an OPL patch in a Wave file...
		Clipboard clipboard(ui::ClipboardWave, sizeof(S3MSampleHeader));
		if(clipboard.IsValid())
		{
			S3MSampleHeader sampleHeader;
			MemsetZero(sampleHeader);
			sampleHeader.ConvertToS3M(sample);
			mpt::String::WriteBuf(mpt::String::nullTerminated, sampleHeader.name) = sndFile.m_szNames[m_nSample];
			mpt::String::WriteBuf(mpt::String::maybeNullTerminated, sampleHeader.reserved2) = mpt::ToCharset(mpt::Charset::UTF8, Version::Current().GetOpenMPTVersionString());
			clipboard = sampleHeader;
		}
		return;
	}

	SmpLength rangeStart = 0, rangeEnd = sample.nLength;
	SampleChannelSelection channelSel = SampleChannelSelection::Both;

	// First things first: Calculate sample size, taking partial selections into account.
	LimitMax(m_dwEndSel, sample.nLength);
	if(HasSelection())
	{
		rangeStart = m_dwBeginSel;
		rangeEnd = m_dwEndSel;
		channelSel = m_channelSelection;
	}

	const bool singleChannel = SampleEdit::IsSingleChannel(sample, channelSel);
	const uint8 writeNumChannels = singleChannel ? uint8(1) : sample.GetNumChannels();
	const SmpLength rangeLength = rangeEnd - rangeStart;
	const size_t sampleDataSize = rangeLength * sample.GetElementarySampleSize() * writeNumChannels;

	// Ok, now calculate size of the resulting WAV file.
	size_t memSize = sizeof(RIFFHeader)                    // RIFF Header
		+ sizeof(RIFFChunk) + sizeof(WAVFormatChunk)       // Sample format
		+ sizeof(RIFFChunk) + ((sampleDataSize + 1) & ~1)  // Sample data
		+ sizeof(RIFFChunk) + sizeof(WAVExtraChunk)        // Sample metadata
		+ MAX_SAMPLENAME + MAX_SAMPLEFILENAME;             // Sample name
	static_assert((sizeof(WAVExtraChunk) % 2u) == 0);
	static_assert((MAX_SAMPLENAME % 2u) == 0);
	static_assert((MAX_SAMPLEFILENAME % 2u) == 0);

	// We want to store some loop metadata as well.
	memSize += sizeof(RIFFChunk) + sizeof(WAVSampleInfoChunk) + 2 * sizeof(WAVSampleLoop);
	// ...and cue points, too.
	memSize += sizeof(RIFFChunk) + sizeof(uint32) + std::size(sample.cues) * sizeof(WAVCuePoint);

	MPT_ASSERT((memSize % 2u) == 0);

	BeginWaitCursor();
	Clipboard clipboard(ui::ClipboardWave, memSize);
	if(auto data = clipboard.Get(); data.data())
	{
		std::pair<mpt::byte_span, mpt::IO::Offset> mf(data, 0);
		mpt::IO::OFile<std::pair<mpt::byte_span, mpt::IO::Offset>> ff(mf);
		WAVSampleWriter file(ff);

		// Write sample format
		file.WriteFormat(sample.GetSampleRate(sndFile.GetType()), sample.GetElementarySampleSize() * 8, writeNumChannels, WAVFormatChunk::fmtPCM);

		// Write sample data
		file.StartChunk(RIFFChunk::iddata);
		
		std::byte *sampleData = data.data() + ff.TellWrite();
		const uint8 selectedChn = SampleEdit::SelectedChannel(channelSel);
		const uint8 srcChannels = sample.GetNumChannels();
		switch(sample.GetElementarySampleSize())
		{
		case 1:
			EncodeSample<SC::EncodeChain<SC::EncodeEndian<mpt::endian::little, uint8>, SC::Convert<uint8, int8>>>(sampleData, rangeLength * (singleChannel ? 1 : srcChannels), 1, sample.sample8() + rangeStart * srcChannels + selectedChn, rangeLength * sample.GetBytesPerSample(), singleChannel ? srcChannels : 1);
			break;
		case 2:
			EncodeSample<SC::EncodeEndian<mpt::endian::little, int16>>(sampleData, rangeLength * (singleChannel ? 1 : srcChannels), 1, sample.sample16() + rangeStart * srcChannels + selectedChn, rangeLength * sample.GetBytesPerSample(), singleChannel ? srcChannels : 1);
			break;
		}
		ff.SeekRelative(sampleDataSize);

		file.WriteLoopInformation(sample, rangeStart, rangeEnd);
		file.WriteCueInformation(sample, rangeStart, rangeEnd);
		file.WriteExtraInformation(sample, sndFile.GetType(), sndFile.GetSampleName(m_nSample));

		mpt::IO::Offset totalSize = file.Finalize();
		MPT_ASSERT(totalSize <= static_cast<mpt::IO::Offset>(memSize));
		MPT_UNUSED_VARIABLE(totalSize);

		clipboard.Close();
	}
	EndWaitCursor();
}


void CViewSample::OnEditPaste()
{
	DoPaste(PasteMode::Replace);
}


void CViewSample::OnEditMixPaste()
{
	CMixSampleDlg::sampleOffset = m_dwMenuParam;
	DoPaste(PasteMode::MixPaste);
}


void CViewSample::OnEditInsertPaste()
{
	m_dwBeginSel = m_dwEndSel = m_dwBeginDrag = m_dwEndDrag = m_dwMenuParam;
	DoPaste(PasteMode::Insert);
}


template<typename Tsrc>
static void MixSampleLoop(SmpLength numSamples, const Tsrc *src, uint8 srcInc, double amplify, int32 *dst, uint8 dstInc)
{
	SC::Convert<int16, Tsrc> conv;  // Mixing 16-bit data into 32-bit accumulation buffer
	while(numSamples--)
	{
		*dst += mpt::saturate_round<int32>(conv(*src) * amplify);
		src += srcInc;
		dst += dstInc;
	}
}


static void MixSampleOnto(const ModSample &sample, SmpLength srcStart, SmpLength srcLen, double amplify, uint8 srcChn, uint8 dstChn, uint8 dstStride, int32 *dst)
{
	uint8 numChannels = sample.GetNumChannels();
	srcLen = std::min(srcLen, sample.nLength - srcStart);
	switch(sample.GetElementarySampleSize())
	{
	case 1:
		MixSampleLoop(srcLen, sample.sample8() + srcStart * numChannels + (srcChn % numChannels), numChannels, amplify / 100.0, dst + dstChn, dstStride);
		break;
	case 2:
		MixSampleLoop(srcLen, sample.sample16() + srcStart * numChannels + (srcChn % numChannels), numChannels, amplify / 100.0, dst + dstChn, dstStride);
		break;
	default:
		MPT_ASSERT_NOTREACHED();
	}
}


void CViewSample::DoPaste(PasteMode pasteMode)
{
	CModDoc *pModDoc = GetDocument();
	BeginWaitCursor();
	Clipboard clipboard(ui::ClipboardWave);
	if(auto data = clipboard.Get(); data.data())
	{
		SmpLength selBegin = 0, selEnd = 0;
		pModDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_replace, "Paste");

		CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
		ModSample &sample = sndFile.GetSample(m_nSample);
		const auto parentIns = pModDoc->GetParentInstrumentWithSameName(m_nSample);

		if(!sample.HasSampleData() || sample.uFlags[CHN_ADLIB])
			pasteMode = PasteMode::Replace;
		// Show mix paste dialog
		if(pasteMode == PasteMode::MixPaste)
		{
			CMixSampleDlg dlg(this, sample.GetSampleRate(sndFile.GetType()));
			if(dlg.DoModal() != IDOK)
			{
				EndWaitCursor();
				return;
			}
		}

		// Save old data for mixpaste
		ModSample oldSample = sample;
		std::string oldSampleName = sndFile.m_szNames[m_nSample];
		mpt::PathString oldSamplePath = sndFile.GetSamplePath(m_nSample);

		if(pasteMode != PasteMode::Replace)
		{
			sample.pData.pSample = nullptr;  // prevent old sample from being deleted.
		}

		FileReader file(data);
		TrackerCriticalSection cs;
		bool ok = sndFile.ReadSampleFromFile(m_nSample, file, TrackerSettings::Instance().m_MayNormalizeSamplesOnLoad);
		clipboard.Close();
		if(sample.uFlags[CHN_ADLIB] != oldSample.uFlags[CHN_ADLIB] && pasteMode != PasteMode::Replace)
		{
			// Cannot mix PCM with FM
			pasteMode = PasteMode::Replace;
			oldSample.FreeSample();
		}
		if (!sndFile.m_szNames[m_nSample][0] || pasteMode != PasteMode::Replace)
		{
			sndFile.m_szNames[m_nSample] = oldSampleName;
		}
		if (!sample.filename[0])
		{
			sample.filename = oldSample.filename;
		}

		const bool singleChannel = pasteMode != PasteMode::Replace && SampleEdit::IsSingleChannel(oldSample, m_menuChannelSelection);
		const uint8 targetChn = SampleEdit::SelectedChannel(m_menuChannelSelection);
		if(pasteMode == PasteMode::MixPaste && ok)
		{
			// Mix new sample (stored in the actual sample slot) and old sample (stored in oldSample)
			SmpLength newLength = std::max(oldSample.nLength, mpt::saturate_cast<SmpLength>(CMixSampleDlg::sampleOffset + sample.nLength));
			// When pasting into a single channel, the result keeps the old channel count;
			// otherwise pick the wider of the two samples.
			const uint8 newNumChannels = singleChannel ? oldSample.GetNumChannels() : std::max(oldSample.GetNumChannels(), sample.GetNumChannels());

			// Result is at least 9 bits (when mixing two 8-bit samples), so always enforce 16-bit resolution
			int16 *pNewSample = static_cast<int16 *>(ModSample::AllocateSample(newLength, 2u * newNumChannels));
			if(pNewSample == nullptr)
			{
				cs.Leave();
				ErrorBox(IDS_ERR_OUTOFMEMORY, this);
				ok = false;
			} else
			{
				selBegin = mpt::saturate_cast<SmpLength>(CMixSampleDlg::sampleOffset);
				selEnd = selBegin + sample.nLength;

				static constexpr SmpLength chunkSize = 4096;
				std::vector<int32> mixBuffer;
				const int64 mixOffset = CMixSampleDlg::sampleOffset;
				const uint8 srcChannels = sample.GetNumChannels();

				for(SmpLength pos = 0; pos < newLength; pos += chunkSize)
				{
					const SmpLength chunkLen = std::min(chunkSize, newLength - pos);
					mixBuffer.assign(chunkLen * newNumChannels, 0);

					// Mix old sample
					if(pos < oldSample.nLength)
					{
						SmpLength oldChunk = std::min(chunkLen, oldSample.nLength - pos);
						for(uint8 chn = 0; chn < newNumChannels; chn++)
							MixSampleOnto(oldSample, pos, oldChunk, CMixSampleDlg::amplifyOriginal, chn, chn, newNumChannels, mixBuffer.data());
					}

					// Mix pasted sample
					if(pos + chunkLen > mixOffset && pos < mixOffset + sample.nLength)
					{
						SmpLength srcStart = static_cast<SmpLength>(std::max(static_cast<int64>(pos), mixOffset) - mixOffset);
						SmpLength dstStart = static_cast<SmpLength>(std::max(mixOffset, static_cast<int64>(pos)) - pos);
						SmpLength srcChunk = std::min(chunkLen - dstStart, sample.nLength - srcStart);
						int32 *chunkDst = mixBuffer.data() + dstStart * newNumChannels;

						if(singleChannel)
						{
							for(uint8 srcChn = 0; srcChn < srcChannels; srcChn++)
								MixSampleOnto(sample, srcStart, srcChunk, CMixSampleDlg::amplifyMix / static_cast<int>(srcChannels), srcChn, targetChn, newNumChannels, chunkDst);
						} else
						{
							for(uint8 chn = 0; chn < newNumChannels; chn++)
								MixSampleOnto(sample, srcStart, srcChunk, CMixSampleDlg::amplifyMix, chn, chn, newNumChannels, chunkDst);
						}
					}

					int16 *out = pNewSample + pos * newNumChannels;
					for(SmpLength i = 0; i < chunkLen * newNumChannels; i++)
						out[i] = mpt::saturate_cast<int16>(mixBuffer[i]);
				}

				sndFile.DestroySample(m_nSample);
				sample = oldSample;
				sample.uFlags.set(CHN_16BIT);
				sample.uFlags.set(CHN_STEREO, newNumChannels == 2);
				CallLocked([&] { return sample.ReplaceWaveform(pNewSample, newLength, sndFile); });
			}
		} else if(pasteMode == PasteMode::Insert && ok)
		{
			// Insert / replace selection
			SmpLength oldLength = oldSample.nLength;
			SmpLength selLength = m_dwEndSel - m_dwBeginSel;
			if(HasSelection())
			{
				// Replace selection with pasted data
				if(selLength >= sample.nLength)
					ok = true;
				else
					ok = CallLocked([&] { return SampleEdit::InsertSilence(oldSample, sample.nLength - selLength, m_dwBeginSel, singleChannel ? m_menuChannelSelection : SampleChannelSelection::Both, sndFile); }) > oldLength;
			} else
			{
				m_dwBeginSel = m_dwBeginDrag;
				ok = CallLocked([&] { return SampleEdit::InsertSilence(oldSample, sample.nLength, m_dwBeginSel, singleChannel ? m_menuChannelSelection : SampleChannelSelection::Both, sndFile); }) > oldLength;
			}
			if(ok && !singleChannel && sample.GetNumChannels() > oldSample.GetNumChannels())
			{
				// Keep channel configuration with higher channel count
				ok = CallLocked([&] { return ctrlSmp::ConvertToStereo(oldSample, sndFile); });
			}
			if(ok && sample.GetElementarySampleSize() > oldSample.GetElementarySampleSize())
			{
				// Keep higher bit depth of the two samples
				ok = CallLocked([&] { return SampleEdit::ConvertTo16Bit(oldSample, sndFile); });
			}
			if(ok)
			{
				selBegin = m_dwBeginSel;
				selEnd = selBegin + sample.nLength;
				uint8 numChannels = oldSample.GetNumChannels();
				SmpLength offset = m_dwBeginSel * numChannels;
				if(singleChannel)
				{
					// Paste into a single channel, mixing stereo source to mono
					uint8 srcChannels = sample.GetNumChannels();
					if(oldSample.GetElementarySampleSize() == 2)
					{
						SC::Convert<int16, int16> conv16;
						SC::Convert<int16, int8> conv8;
						int16 *dst = oldSample.sample16() + offset + targetChn;
						for(SmpLength i = 0; i < sample.nLength; i++, dst += numChannels)
						{
							int32 mix = 0;
							for(uint8 srcChn = 0; srcChn < srcChannels; srcChn++)
							{
								if(sample.GetElementarySampleSize() == 2)
									mix += conv16(sample.sample16()[i * srcChannels + srcChn]);
								else
									mix += conv8(sample.sample8()[i * srcChannels + srcChn]);
							}
							*dst = mpt::saturate_cast<int16>(mix / static_cast<int>(srcChannels));
						}
					} else
					{
						SC::Convert<int8, int8> conv8;
						int8 *dst = oldSample.sample8() + offset + targetChn;
						for(SmpLength i = 0; i < sample.nLength; i++, dst += numChannels)
						{
							int32 mix = 0;
							for(uint8 srcChn = 0; srcChn < srcChannels; srcChn++)
								mix += conv8(sample.sample8()[i * srcChannels + srcChn]);
							*dst = mpt::saturate_cast<int8>(mix / static_cast<int>(srcChannels));
						}
					}
				} else
				{
					for(uint8 chn = 0; chn < numChannels; chn++)
					{
						uint8 newChn = chn % sample.GetNumChannels();
						if(oldSample.GetElementarySampleSize() == 1 && sample.GetElementarySampleSize() == 1)
						{
							CopySample(oldSample.sample8() + offset + chn, sample.nLength, numChannels, sample.sample8() + newChn, sample.GetSampleSizeInBytes(), sample.GetNumChannels(), SC::Convert<int8, int8>());
						} else if(oldSample.GetElementarySampleSize() == 2 && sample.GetElementarySampleSize() == 1)
						{
							CopySample(oldSample.sample16() + offset + chn, sample.nLength, numChannels, sample.sample8() + newChn, sample.GetSampleSizeInBytes(), sample.GetNumChannels(), SC::Convert<int16, int8>());
						} else if(oldSample.GetElementarySampleSize() == 2 && sample.GetElementarySampleSize() == 2)
						{
							CopySample(oldSample.sample16() + offset + chn, sample.nLength, numChannels, sample.sample16() + newChn, sample.GetSampleSizeInBytes(), sample.GetNumChannels(), SC::Convert<int16, int16>());
						} else
						{
							MPT_ASSERT_NOTREACHED();
						}
					}
				}
			} else
			{
				ErrorBox(IDS_ERR_OUTOFMEMORY, this);
			}
			sndFile.DestroySample(m_nSample);
			sample = oldSample;
		}

		if(ok)
		{
			SetCurSel(selBegin, selEnd, singleChannel ? m_menuChannelSelection : SampleChannelSelection::Both);
			sample.PrecomputeLoops(sndFile, true);
			cs.Leave();
			SetModified(SampleHint().Info().Data().Names(), true, false);
			if(pasteMode == PasteMode::Replace)
			{
				sndFile.ResetSamplePath(m_nSample);

				if(parentIns <= sndFile.GetNumInstruments())
				{
					if(auto instr = sndFile.Instruments[parentIns]; instr != nullptr)
					{
						pModDoc->GetInstrumentUndo().PrepareUndo(parentIns, "Set Name");
						instr->name = sndFile.m_szNames[m_nSample];
						pModDoc->UpdateAllViews(this, InstrumentHint(parentIns).Names(), this);
					}
				}
			} else
			{
				sndFile.SetSamplePath(m_nSample, std::move(oldSamplePath));
			}
		} else
		{
			if(pasteMode == PasteMode::MixPaste)
				ModSample::FreeSample(oldSample.samplev());
			pModDoc->GetSampleUndo().Undo(m_nSample);
			sndFile.m_szNames[m_nSample] = oldSampleName;
		}
	}
	EndWaitCursor();
}


void CViewSample::OnEditUndo()
{
	CModDoc *pModDoc = GetDocument();
	if(pModDoc == nullptr) return;
	if(pModDoc->GetSampleUndo().Undo(m_nSample))
	{
		SetModified(SampleHint().Info().Data().Names(), true, false);
	}
}


void CViewSample::OnEditRedo()
{
	CModDoc *pModDoc = GetDocument();
	if(pModDoc == nullptr) return;
	if(pModDoc->GetSampleUndo().Redo(m_nSample))
	{
		SetModified(SampleHint().Info().Data().Names(), true, false);
	}
}


void CViewSample::On8BitConvert()
{
	Convert8Bit(CInputHandler::ShiftPressed());
}


void CViewSample::Convert8Bit(bool allSamples)
{
	CModDoc *pModDoc = GetDocument();
	if(!pModDoc)
		return;

	if(allSamples && Reporting::Confirm(UL_("This will convert all samples to 8-bit. Continue?"), UL_("8-Bit Conversion")) == cnfNo)
		return;

	BeginWaitCursor();
	CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
	SAMPLEINDEX minSample = m_nSample, maxSample = m_nSample;
	if(allSamples)
	{
		minSample = 1;
		maxSample = sndFile.GetNumSamples();
	}
	for(SAMPLEINDEX smp = minSample; smp <= maxSample; smp++)
	{
		ModSample &sample = sndFile.GetSample(smp);
		if(sample.uFlags[CHN_16BIT] && !sample.uFlags[CHN_ADLIB] && sample.HasSampleData())
		{
			MPT_ASSERT(sample.GetElementarySampleSize() == 2);
			pModDoc->GetSampleUndo().PrepareUndo(smp, sundo_replace, "8-Bit Conversion");

			TrackerCriticalSection cs;
			SampleEdit::ConvertTo8Bit(sample, sndFile);
			cs.Leave();

			SetModified(smp, SampleHint().Info().Data(), smp == m_nSample, true);
		}
	}
	EndWaitCursor();
}


void CViewSample::On16BitConvert()
{
	CModDoc *pModDoc = GetDocument();
	BeginWaitCursor();
	if ((pModDoc) && (m_nSample <= pModDoc->GetNumSamples()))
	{
		CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
		ModSample &sample = sndFile.GetSample(m_nSample);
		if(!sample.uFlags[CHN_16BIT] && !sample.uFlags[CHN_ADLIB] && sample.HasSampleData())
		{
			MPT_ASSERT(sample.GetElementarySampleSize() == 1);
			pModDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_replace, "16-Bit Conversion");
			if(!CallLocked([&] { return SampleEdit::ConvertTo16Bit(sample, sndFile); }))
			{
				pModDoc->GetSampleUndo().RemoveLastUndoStep(m_nSample);
			} else
			{
				SetModified(SampleHint().Info().Data(), true, true);
			}
		}
	}
	EndWaitCursor();
}


void CViewSample::OnMonoConvert(ctrlSmp::StereoToMonoMode convert)
{
	CModDoc *pModDoc = GetDocument();
	if(pModDoc == nullptr || m_nSample > pModDoc->GetNumSamples())
		return;

	CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
	ModSample &sample = sndFile.GetSample(m_nSample);
	if(sample.uFlags[CHN_ADLIB] || !sample.HasSampleData() || sample.GetNumChannels() != 2)
		return;

	BeginWaitCursor();
	bool success = false;
	if(sample.GetNumChannels() > 1)
	{
		SAMPLEINDEX rightSmp = SAMPLEINDEX_INVALID;
		if(convert == ctrlSmp::splitSample)
		{
			// Split sample into two slots
			rightSmp = pModDoc->InsertSample();
			if(rightSmp == SAMPLEINDEX_INVALID)
			{
				EndWaitCursor();
				return;
			}
		}

		pModDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_replace, "Mono Conversion");

		if(convert == ctrlSmp::splitSample)
		{
			ModSample &right = sndFile.GetSample(rightSmp);
			success = CallLocked([&] { return ctrlSmp::SplitStereo(sample, sample, right, sndFile); });

			// Try to create a new instrument as well which maps to the right sample.
			if(success)
			{
				INSTRUMENTINDEX ins = pModDoc->FindSampleParent(m_nSample);
				if(ins != INSTRUMENTINDEX_INVALID)
				{
					INSTRUMENTINDEX rightIns = pModDoc->InsertInstrument(0, ins);
					if(rightIns != INSTRUMENTINDEX_INVALID)
					{
						for(auto &smp : sndFile.Instruments[rightIns]->Keyboard)
						{
							if(smp == m_nSample)
								smp = rightSmp;
						}
					}
					pModDoc->UpdateAllViews(this, InstrumentHint(rightIns).Info().Envelope().Names(), this);
				}

				// Finally, adjust sample panning
				if(sndFile.GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT | MOD_TYPE_XM))
				{
					sample.uFlags.set(CHN_PANNING);
					sample.nPan = 0;
					right.uFlags.set(CHN_PANNING);
					right.nPan = 256;
				}
				pModDoc->UpdateAllViews(this, SampleHint(rightSmp).Info().Data().Names(), this);
			}
		} else
		{
			success = CallLocked([&] { return ctrlSmp::ConvertToMono(sample, sndFile, convert); });
		}
	}

	if(success)
		SetModified(SampleHint().Info().Data().Names(), true, true);
	else
		pModDoc->GetSampleUndo().RemoveLastUndoStep(m_nSample);

	EndWaitCursor();
}


void CViewSample::OnStereoConvert()
{
	CModDoc *pModDoc = GetDocument();
	if(pModDoc == nullptr || m_nSample > pModDoc->GetNumSamples())
		return;
	CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
	ModSample &sample = sndFile.GetSample(m_nSample);
	if(sample.uFlags[CHN_ADLIB] || !sample.HasSampleData() || sample.GetNumChannels() != 1)
		return;

	BeginWaitCursor();
	pModDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_replace, "Stereo Conversion");
	if(CallLocked([&] { return ctrlSmp::ConvertToStereo(sample, sndFile); }))
		SetModified(SampleHint().Info().Data().Names(), true, true);
	else
		pModDoc->GetSampleUndo().RemoveLastUndoStep(m_nSample);
	EndWaitCursor();
}


void CViewSample::OnSendSelectionToNewSlot()
{
	CModDoc *modDoc = GetDocument();
	if(modDoc == nullptr || m_nSample > modDoc->GetNumSamples())
		return;
	CTrackerSoundFile &sndFile = modDoc->GetSoundFile();
	const ModSample &sourceSmp = sndFile.GetSample(m_nSample);
	LimitMax(m_dwBeginSel, sourceSmp.nLength);
	LimitMax(m_dwEndSel, sourceSmp.nLength);
	if(!sourceSmp.HasSampleData() || sourceSmp.uFlags[CHN_ADLIB] || !HasSelection())
		return;

	const SAMPLEINDEX newSample = modDoc->InsertSample();
	if(newSample == SAMPLEINDEX_INVALID)
		return;

	TrackerCriticalSection cs;
	modDoc->GetSampleUndo().PrepareUndo(newSample, sundo_replace, "Send Selection to New Sample Slot");
	ModSample &targetSmp = sndFile.GetSample(newSample);
	targetSmp = sourceSmp;
	targetSmp.nLoopStart = targetSmp.nLoopEnd = 0;
	targetSmp.nSustainStart = targetSmp.nSustainEnd = 0;
	targetSmp.uFlags.reset(CHN_LOOP | CHN_SUSTAINLOOP | CHN_PINGPONGLOOP | CHN_PINGPONGSUSTAIN | SMP_KEEPONDISK);
	targetSmp.RemoveAllCuePoints();
	if(!targetSmp.CopyWaveform(sourceSmp, m_dwBeginSel, m_dwEndSel))
	{
		targetSmp.pData.pSample = nullptr;
		targetSmp.nLength = 0;
	}
	targetSmp.PrecomputeLoops(sndFile, false);
	sndFile.m_szNames[newSample] = sndFile.m_szNames[m_nSample];
	cs.Leave();
	modDoc->SetModified();
	modDoc->UpdateAllViews(nullptr, SampleHint(newSample).Info().Data().Names());
}


void CViewSample::TrimSample(bool trimToLoopEnd)
{
	CModDoc *pModDoc = GetDocument();
	//nothing loaded or invalid sample slot.
	if(!pModDoc || m_nSample > pModDoc->GetNumSamples() || IsOPLInstrument()) return;

	CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
	ModSample &sample = sndFile.GetSample(m_nSample);

	if(trimToLoopEnd)
	{
		m_dwBeginSel = 0;
		m_dwEndSel = sample.nLoopEnd;
	} else if(m_dwBeginSel == m_dwEndSel)
	{
		// Trim around loop points if there's no selection
		m_dwBeginSel = sample.nLoopStart;
		m_dwEndSel = sample.nLoopEnd;
	}

	if (m_dwBeginSel >= m_dwEndSel) return; // invalid selection

	BeginWaitCursor();
	SmpLength nStart = m_dwBeginSel;
	SmpLength nEnd = m_dwEndSel - m_dwBeginSel;

	if(sample.HasSampleData() && (nStart + nEnd <= sample.nLength) && (nEnd >= MIN_TRIM_LENGTH))
	{
		pModDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_replace, "Trim");

		TrackerCriticalSection cs;

		// Note: Sample is overwritten in-place! Unused data is not deallocated!
		memmove(sample.sampleb(), sample.sampleb() + nStart * sample.GetBytesPerSample(), nEnd * sample.GetBytesPerSample());

		for(SmpLength &point : SampleEdit::GetCuesAndLoops(sample))
		{
			if(point >= nStart)
				point -= nStart;
			else
				point = sample.nLength;
		}
		sample.nLength = nEnd;
		sample.PrecomputeLoops(sndFile);
		cs.Leave();

		SetCurSel(0, 0);
		SetModified(SampleHint().Info().Data(), true, true);
	}
	EndWaitCursor();
}


void CViewSample::OnChar(uint32 /*nChar*/, uint32, uint32 /*nFlags*/)
{
}


void CViewSample::PlayNote(ModCommand::NOTE note, const SmpLength nStartPos, int volume)
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	CModDoc *pModDoc = GetDocument();
	if ((pModDoc) && (pMainFrm))
	{
		if (note >= NOTE_MIN_SPECIAL)
		{
			pModDoc->NoteOff(0, (note == NOTE_NOTECUT));
		} else
		{
			if(m_dwStatus[SMPSTATUS_KEYDOWN])
				pModDoc->NoteOff(note, true, INSTRUMENTINDEX_INVALID, m_noteChannel[note - NOTE_MIN]);
			else
				pModDoc->NoteOff(0, true);

			const CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
			const ModSample &sample = sndFile.GetSample(m_nSample);

			SmpLength loopstart = m_dwBeginSel, loopend = m_dwEndSel;
			// If selection is too small -> no loop
			if((m_nZoom >= 0 && loopend - loopstart < (SmpLength)(4 << m_nZoom))
				|| (m_nZoom < 0 && loopend - loopstart < 4)
				|| (loopstart >= sample.nLength))
			{
				loopend = loopstart = 0;
			}

			PlayNoteParam params = PlayNoteParam(note).Sample(m_nSample).Volume(volume).LoopStart(loopstart).LoopEnd(loopend).Offset(nStartPos);
			if(loopend > loopstart && SampleEdit::IsSingleChannel(sample, m_channelSelection))
				params.Panning(SampleEdit::SelectedChannel(m_channelSelection) * 256);
			pModDoc->PlayNote(params, &m_noteChannel);

			m_dwStatus.set(SMPSTATUS_KEYDOWN);

			uint32 freq = sndFile.GetFreqFromPeriod(sndFile.GetPeriodFromNote(note + (sndFile.GetType() == MOD_TYPE_XM ? sample.RelativeTone : 0), sample.nFineTune, sample.nC5Speed), sample.nC5Speed, 0);

			pMainFrm->SetInfoText(MPT_UFORMAT("{} ({}.{} Hz)")(
				mpt::ToUnicode(sndFile.GetNoteName((ModCommand::NOTE)note)),
				freq >> FREQ_FRACBITS,
				mpt::ufmt::dec0<2>(Util::muldiv(freq & ((1 << FREQ_FRACBITS) - 1), 100, 1 << FREQ_FRACBITS))));
		}
	}
}


void CViewSample::NoteOff(ModCommand::NOTE note)
{
	CTrackerSoundFile &sndFile = GetDocument()->GetSoundFile();
	ModChannel &chn = sndFile.m_PlayState.Chn[m_noteChannel[note - NOTE_MIN]];
	sndFile.KeyOff(chn);
	chn.dwFlags.set(CHN_NOTEFADE);
	m_noteChannel[note - NOTE_MIN] = CHANNELINDEX_INVALID;
}


void CViewSample::OnDropFiles(const std::vector<mpt::PathString> &files)
{
	CMainFrame::GetMainFrame()->SetForegroundWindow();
	for(size_t f = 0; f < files.size(); f++)
	{
		if(SendCtrlMessage(CTRLMSG_SMP_OPENFILE, (LParam)&files[f]) && f + 1 < files.size())
		{
			// Insert more sample slots
			if(!SendCtrlMessage(CTRLMSG_SMP_NEWSAMPLE))
				break;
		}
	}
}


void CViewSample::OnZoomOnSel()
{
	int zoom = 0;
	SmpLength selLength = (m_dwEndSel - m_dwBeginSel);
	if (selLength > 0 && m_rcClient.right > 0)
	{
		zoom = GetZoomLevel(selLength);
		if(zoom < 0)
		{
			zoom++;
			if(zoom >= -1)
				zoom = 1;
			else if(zoom < MIN_ZOOM)
				zoom = MIN_ZOOM;
		} else if(zoom > MAX_ZOOM)
		{
			zoom = 0;
		}
	}

	if(zoom)
	{
		SetZoom(zoom, m_dwBeginSel + selLength / 2);
	}
	SendCtrlMessage(CTRLMSG_SMP_SETZOOM, zoom);
}


SmpLength CViewSample::ScrollPosToSamplePos(int nZoom) const
{
	if(nZoom < 0)
		return m_nScrollPosX;
	else if(nZoom > 0)
		return m_nScrollPosX << (nZoom - 1);
	else
		return 0;
}


void CViewSample::OnSetLoopStart()
{
	CModDoc *pModDoc = GetDocument();
	if (pModDoc)
	{
		CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
		ModSample &sample = sndFile.GetSample(m_nSample);
		SmpLength loopEnd = (sample.nLoopEnd > 0) ? sample.nLoopEnd : sample.nLength;
		if ((m_dwMenuParam + 4 <= loopEnd) && (sample.nLoopStart != m_dwMenuParam))
		{
			pModDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_none, "Set Loop Start");
			sample.SetLoop(m_dwMenuParam, loopEnd, true, sample.uFlags[CHN_PINGPONGLOOP], sndFile);
			SetModified(SampleHint().Info(), true, false);
		}
	}
}


void CViewSample::OnSetLoopEnd()
{
	CModDoc *pModDoc = GetDocument();
	if (pModDoc)
	{
		CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
		ModSample &sample = sndFile.GetSample(m_nSample);
		if ((m_dwMenuParam >= sample.nLoopStart + 4) && (sample.nLoopEnd != m_dwMenuParam))
		{
			pModDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_none, "Set Loop End");
			sample.SetLoop(sample.nLoopStart, m_dwMenuParam, true, sample.uFlags[CHN_PINGPONGLOOP], sndFile);
			SetModified(SampleHint().Info(), true, false);
		}
	}
}


void CViewSample::OnConvertPingPongLoop()
{
	CModDoc *pModDoc = GetDocument();
	if(pModDoc)
	{
		CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
		ModSample &sample = sndFile.GetSample(m_nSample);
		if(!sample.HasPingPongLoop())
			return;

		pModDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_replace, "Convert Bidi Loop");
		if(SampleEdit::ConvertPingPongLoop(sample, sndFile, false))
			SetModified(SampleHint().Info().Data(), true, true);
		else
			pModDoc->GetSampleUndo().RemoveLastUndoStep(m_nSample);
	}
}


void CViewSample::OnConvertNormalLoopToSustain()
{
	CModDoc *pModDoc = GetDocument();
	if(!pModDoc)
		return;
	
	CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
	ModSample &sample = sndFile.GetSample(m_nSample);
	if(!sample.HasLoop())
		return;
	pModDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_replace, "Convert Normal Loop To Sustain");
	const auto origStart = sample.nLoopStart, origEnd = sample.nLoopEnd;
	const bool origPingPong = sample.uFlags[CHN_PINGPONGLOOP];
	// Swap them
	if(sample.HasSustainLoop())
		sample.SetLoop(sample.nSustainStart, sample.nSustainEnd, true, sample.uFlags[CHN_PINGPONGSUSTAIN], sndFile);
	else
		sample.uFlags.reset(CHN_LOOP | CHN_PINGPONGLOOP);
	sample.SetSustainLoop(origStart, origEnd, true, origPingPong, sndFile);
	SetModified(SampleHint().Info().Data(), true, true);
}


void CViewSample::OnSetSustainStart()
{
	CModDoc *pModDoc = GetDocument();
	if (pModDoc)
	{
		CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
		ModSample &sample = sndFile.GetSample(m_nSample);
		SmpLength sustainEnd = (sample.nSustainEnd > 0) ? sample.nSustainEnd : sample.nLength;
		if ((m_dwMenuParam + 4 <= sustainEnd) && (sample.nSustainStart != m_dwMenuParam))
		{
			pModDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_none, "Set Sustain Start");
			sample.SetSustainLoop(m_dwMenuParam, sustainEnd, true, sample.uFlags[CHN_PINGPONGSUSTAIN], sndFile);
			SetModified(SampleHint().Info(), true, false);
		}
	}
}


void CViewSample::OnSetSustainEnd()
{
	CModDoc *pModDoc = GetDocument();
	if (pModDoc)
	{
		CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
		ModSample &sample = sndFile.GetSample(m_nSample);
		if ((m_dwMenuParam >= sample.nSustainStart + 4) && (sample.nSustainEnd != m_dwMenuParam))
		{
			pModDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_none, "Set Sustain End");
			sample.SetSustainLoop(sample.nSustainStart, m_dwMenuParam, true, sample.uFlags[CHN_PINGPONGSUSTAIN], sndFile);
			SetModified(SampleHint().Info(), true, false);
		}
	}
}


void CViewSample::OnConvertPingPongSustain()
{
	CModDoc *pModDoc = GetDocument();
	if(pModDoc)
	{
		CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
		ModSample &sample = sndFile.GetSample(m_nSample);
		if(!sample.HasPingPongSustainLoop())
			return;

		pModDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_replace, "Convert Bidi Sustain Loop");
		if(SampleEdit::ConvertPingPongLoop(sample, sndFile, true))
			SetModified(SampleHint().Info().Data(), true, true);
		else
			pModDoc->GetSampleUndo().RemoveLastUndoStep(m_nSample);
	}
}


void CViewSample::OnConvertSustainLoopToNormal()
{
	CModDoc *pModDoc = GetDocument();
	if(!pModDoc)
		return;

	CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
	ModSample &sample = sndFile.GetSample(m_nSample);
	if(!sample.HasSustainLoop())
		return;
	pModDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_replace, "Convert Sustain Loop To Normal");
	const auto origStart = sample.nSustainStart, origEnd = sample.nSustainEnd;
	const bool origPingPong = sample.uFlags[CHN_PINGPONGSUSTAIN];
	// Swap them
	if(sample.HasLoop())
		sample.SetSustainLoop(sample.nLoopStart, sample.nLoopEnd, true, sample.uFlags[CHN_PINGPONGLOOP], sndFile);
	else
		sample.uFlags.reset(CHN_SUSTAINLOOP | CHN_PINGPONGSUSTAIN);
	sample.SetLoop(origStart, origEnd, true, origPingPong, sndFile);
	SetModified(SampleHint().Info().Data(), true, true);
}


void CViewSample::OnSetCuePoint(uint32 nID)
{
	nID -= ID_SAMPLE_CUE_1;
	CModDoc *pModDoc = GetDocument();
	if (pModDoc)
	{
		CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
		ModSample &sample = sndFile.GetSample(m_nSample);

		pModDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_none, "Set Cue Point");
		sample.cues[nID] = m_dwMenuParam;
		SetModified(SampleHint().Info(), true, false);
	}
}


void CViewSample::OnZoomUp()
{
	DoZoom(1);
}


void CViewSample::OnZoomDown()
{
	DoZoom(-1);
}


void CViewSample::OnDrawingToggle()
{
	const CModDoc *pModDoc = GetDocument();
	if(!pModDoc) return;
	const CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();

	const ModSample &sample = sndFile.GetSample(m_nSample);
	if(!sample.HasSampleData())
	{
		OnAddSilence();
		if(!sample.HasSampleData())
		{
			return;
		}
	}

	m_dwStatus.flip(SMPSTATUS_DRAWING);
	UpdateNcButtonState();
}


void CViewSample::OnAddSilence()
{
	CModDoc *pModDoc = GetDocument();
	if (!pModDoc) return;
	CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();

	ModSample &sample = sndFile.GetSample(m_nSample);

	const SmpLength oldLength = IsOPLInstrument() ? 0 : sample.nLength;

	AddSilenceDlg dlg(this, oldLength, sample.GetSampleRate(sndFile.GetType()), sndFile.SupportsOPL());
	if (dlg.DoModal() != IDOK) return;

	if(dlg.m_editOption == AddSilenceDlg::kOPLInstrument)
	{
		SendCtrlMessage(CTRLMSG_SMP_INITOPL);
		return;
	}

	if(MAX_SAMPLE_LENGTH - oldLength < dlg.m_numSamples && dlg.m_editOption != AddSilenceDlg::kResize)
	{
		mpt::ustring str; str = ui::Format(UL_("Cannot add silence because the new sample length would exceed maximum sample length %u."), MAX_SAMPLE_LENGTH);
		Reporting::Information(str);
		return;
	}

	BeginWaitCursor();

	if(sample.nLength == 0 && sample.nVolume == 0)
	{
		sample.nVolume = 256;
	}
	if(dlg.m_editOption == AddSilenceDlg::kResize)
	{
		// resize - dlg.m_nSamples = new size
		if(dlg.m_numSamples == 0)
		{
			pModDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_replace, "Delete Sample");
			sndFile.DestroySampleThreadsafe(m_nSample);
		} else if(dlg.m_numSamples != sample.nLength)
		{
			TrackerCriticalSection cs;

			if(dlg.m_numSamples < sample.nLength)	// make it shorter!
				pModDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_delete, "Resize", dlg.m_numSamples, sample.nLength);
			else	// make it longer!
				pModDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_insert, "Add Silence", sample.nLength, dlg.m_numSamples);
			sample.SetAdlib(false);
			CallLocked([&] { return SampleEdit::ResizeSample(sample, dlg.m_numSamples, sndFile); });
		}
	} else
	{
		// add silence - dlg.m_nSamples = amount of bytes to be added
		if(dlg.m_numSamples > 0)
		{
			TrackerCriticalSection cs;

			SmpLength nStart = (dlg.m_editOption == AddSilenceDlg::kSilenceAtEnd) ? sample.nLength : 0;
			pModDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_insert, "Add Silence", nStart, nStart + dlg.m_numSamples);
			sample.SetAdlib(false);
			CallLocked([&] { return SampleEdit::InsertSilence(sample, dlg.m_numSamples, nStart, SampleChannelSelection::Both, sndFile); });
		}
	}

	EndWaitCursor();

	if(oldLength != sample.nLength)
	{
		SetCurSel(0, 0);
		SetModified(SampleHint().Info().Data().Names(), true, true);
	}
}

LResult CViewSample::OnMidiMsg(WParam midiDataParam, LParam)
{
	const uint32 midiData = static_cast<uint32>(midiDataParam);
	static uint8 midiVolume = 127;

	CModDoc *pModDoc = GetDocument();
	const uint8 midiByte1 = MIDIEvents::GetDataByte1FromEvent(midiData);
	const uint8 midiByte2 = MIDIEvents::GetDataByte2FromEvent(midiData);
	const uint8 channel = MIDIEvents::GetChannelFromEvent(midiData);

	CTrackerSoundFile *pSndFile = (pModDoc) ? &pModDoc->GetSoundFile() : nullptr;
	if (!pSndFile) return 0;

	uint8 nNote = midiByte1 + NOTE_MIN;
	int nVol = midiByte2;
	MIDIEvents::EventType event  = MIDIEvents::GetTypeFromEvent(midiData);
	if(event == MIDIEvents::evNoteOn && !nVol)
		event = MIDIEvents::evNoteOff;	//Convert event to note-off if req'd

	// Handle MIDI messages assigned to shortcuts
	CInputHandler *ih = CMainFrame::GetInputHandler();
	if(ih->HandleMIDIMessage(kCtxViewSamples, midiData) != kcNull
		|| ih->HandleMIDIMessage(kCtxAllContexts, midiData) != kcNull)
	{
		// Mapped to a command, no need to pass message on.
		return 1;
	}

	switch(event)
	{
	case MIDIEvents::evNoteOff: // Note Off
		if(m_midiSustainActive[channel])
		{
			m_midiSustainBuffer[channel].push_back(midiData);
			return 1;
		}
		[[fallthrough]];
	case MIDIEvents::evNoteOn: // Note On
		LimitMax(nNote, NOTE_MAX);
		pModDoc->NoteOff(nNote, true);
		if(event != MIDIEvents::evNoteOff)
		{
			nVol = CMainFrame::ApplyVolumeRelatedSettings(midiData, midiVolume);
			PlayNote(nNote, 0, nVol);
		}
		break;

	case MIDIEvents::evControllerChange: // Controller change
		switch(midiByte1)
		{
		case MIDIEvents::MIDICC_Volume_Coarse: // Volume
			midiVolume = midiByte2;
			break;

		case MIDIEvents::MIDICC_HoldPedal_OnOff:
			m_midiSustainActive[channel] = (midiByte2 >= 0x40);
			if(!m_midiSustainActive[channel])
			{
				// Release all notes
				for(const auto offEvent : m_midiSustainBuffer[channel])
				{
					OnMidiMsg(offEvent, 0);
				}
				m_midiSustainBuffer[channel].clear();
			}
			break;
		}
		break;

	case MIDIEvents::evPitchBend:
		for(CHANNELINDEX chn : m_noteChannel)
		{
			if(chn != CHANNELINDEX_INVALID)
				pSndFile->m_PlayState.Chn[chn].SetMIDIPitchBend(midiByte2, midiByte1);
		}
		break;

	default:
		break;
	}

	return 1;
}

bool CViewSample::PreTranslateMessage(int event)
{
	if(event == FL_KEYBOARD || event == FL_KEYUP)
	{
		CInputHandler *ih = CMainFrame::GetInputHandler();
		const auto keyEvent = ih->Translate(ui::KeyFromEvent(), 1, (event == FL_KEYUP) ? ui::KeyFlagRelease : (ui::IsKeyRepeat() ? ui::KeyFlagRepeat : 0));
		if(ih->KeyEvent(kCtxViewSamples, keyEvent) != kcNull)
			return true;
	}

	return CModScrollView::PreTranslateMessage(event);
}


LResult CViewSample::OnCustomKeyMsg(WParam wParam, LParam lParam)
{
	CModDoc *pModDoc = GetDocument();
	if(!pModDoc)
		return kcNull;
	CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();

	switch(wParam)
	{
		case kcContextMenu: OnRButtonUp(0, Point(0, TimelineHeight(this))); return wParam;

		case kcSampleTrim:			TrimSample(false); return wParam;
		case kcSampleTrimToLoopEnd:	TrimSample(true); return wParam;
		case kcSampleZoomUp:		OnZoomUp(); return wParam;
		case kcSampleZoomDown:		OnZoomDown(); return wParam;
		case kcSampleZoomSelection:	OnZoomOnSel(); return wParam;
		case kcSampleCenterSampleStart:
		case kcSampleCenterSampleEnd:
		case kcSampleCenterLoopStart:
		case kcSampleCenterLoopEnd:
		case kcSampleCenterSustainStart:
		case kcSampleCenterSustainEnd:
			{
				SmpLength point = 0;
				ModSample &sample = sndFile.GetSample(m_nSample);
				switch(wParam)
				{
				case kcSampleCenterSampleStart:		point = 0; break;
				case kcSampleCenterSampleEnd:		point = sample.nLength; break;
				case kcSampleCenterLoopStart:		point = sample.nLoopStart; break;
				case kcSampleCenterLoopEnd:			point = sample.nLoopEnd; break;
				case kcSampleCenterSustainStart:	point = sample.nSustainStart; break;
				case kcSampleCenterSustainEnd:		point = sample.nSustainEnd; break;
				}
				if(!m_nZoom)
					SendCtrlMessage(CTRLMSG_SMP_SETZOOM, 1);
				ScrollToSample(point);
			}
			return wParam;
		case kcPrevInstrument:	OnPrevInstrument(); return wParam;
		case kcNextInstrument:	OnNextInstrument(); return wParam;
		case kcEditSelectAll:	OnEditSelectAll(); return wParam;
		case kcSampleDelete:	OnEditDelete(); return wParam;
		case kcEditCut:			OnEditCut(); return wParam;
		case kcEditCopy:		OnEditCopy(); return wParam;
		case kcEditPaste:		OnEditPaste(); return wParam;
		case kcEditMixPasteITStyle:
		case kcEditMixPaste:	m_menuChannelSelection = SelectedChannel(); DoPaste(PasteMode::MixPaste); return wParam;
		case kcEditPushForwardPaste: m_menuChannelSelection = SelectedChannel(); DoPaste(PasteMode::Insert); return wParam;
		case kcEditUndo:		OnEditUndo(); return wParam;
		case kcEditRedo:		OnEditRedo(); return wParam;
		case kcSampleConvertPingPongLoop: OnConvertPingPongLoop(); return wParam;
		case kcSampleConvertPingPongSustain: OnConvertPingPongSustain(); return wParam;
		case kcSample8Bit:		if(sndFile.GetSample(m_nSample).uFlags[CHN_16BIT])
									Convert8Bit(false);
								else
									On16BitConvert();
								return wParam;
		case kcSample8BitAll:	Convert8Bit(true); return wParam;
		case kcSampleMonoMix:	OnMonoConvertMix(); return wParam;
		case kcSampleMonoLeft:	OnMonoConvertLeft(); return wParam;
		case kcSampleMonoRight:	OnMonoConvertRight(); return wParam;
		case kcSampleMonoSplit:	OnMonoConvertSplit(); return wParam;
		case kcSampleStereo: OnStereoConvert(); return wParam;
		case kcSampleSendSelectionToNew: OnSendSelectionToNewSlot(); return wParam;

		case kcSampleSliceCuePoints:	OnSampleSliceCuePoints(); return wParam;
		case kcSampleSliceGrid:			OnSampleSliceGrid(); return wParam;
		case kcSampleToggleDrawing:		OnDrawingToggle(); return wParam;
		case kcSampleResize:			OnAddSilence(); return wParam;
		case kcSampleGrid:				OnChangeGridSize(); return wParam;

		// Those don't seem to work.
		case kcNoteOff:			PlayNote(NOTE_KEYOFF); return wParam;
		case kcNoteCut:			PlayNote(NOTE_NOTECUT); return wParam;

		case kcSampleToggleFollowPlayCursor:
			TrackerSettings::Instance().m_followSamplePlayCursor = static_cast<FollowSamplePlayCursor>(
				(static_cast<int>(TrackerSettings::Instance().m_followSamplePlayCursor.Get()) + 1) % int(FollowSamplePlayCursor::MaxOptions));
			switch(TrackerSettings::Instance().m_followSamplePlayCursor.Get())
			{
			case FollowSamplePlayCursor::DoNotFollow:
				CMainFrame::GetMainFrame()->SetHelpText(UL_("Follow Sample Play Cursor: Do not follow"));
				break;
			case FollowSamplePlayCursor::Follow:
				CMainFrame::GetMainFrame()->SetHelpText(UL_("Follow Sample Play Cursor: Follow"));
				break;
			case FollowSamplePlayCursor::FollowCentered:
				CMainFrame::GetMainFrame()->SetHelpText(UL_("Follow Sample Play Cursor: Follow centered"));
				break;
			case FollowSamplePlayCursor::MaxOptions:
				MPT_ASSERT_NOTREACHED();
				break;
			}
			return wParam;
	}

	if(wParam >= kcSampStartNotes && wParam <= kcSampEndNotes)
	{
		const ModCommand::NOTE note = pModDoc->GetNoteWithBaseOctave(static_cast<int>(wParam - kcSampStartNotes), 0);
		if(ModCommand::IsNote(note))
		{
			switch(TrackerSettings::Instance().sampleEditorKeyBehaviour)
			{
			case seNoteOffOnKeyRestrike:
				if(m_noteChannel[note - NOTE_MIN] != CHANNELINDEX_INVALID)
				{
					NoteOff(note);
					break;
				}
				[[fallthrough]];
			default:
				PlayNote(note);
			}
			return wParam;
		}
	} else if(wParam >= kcSampStartNoteStops && wParam <= kcSampEndNoteStops)
	{
		const ModCommand::NOTE note = pModDoc->GetNoteWithBaseOctave(static_cast<int>(wParam - kcSampStartNoteStops), 0);
		if(ModCommand::IsNote(note))
		{
			switch(TrackerSettings::Instance().sampleEditorKeyBehaviour)
			{
			case seNoteOffOnNewKey:
				m_dwStatus.reset(SMPSTATUS_KEYDOWN);
				if(m_noteChannel[note - NOTE_MIN] != CHANNELINDEX_INVALID)
				{
					// Release sustain loop on key up
					sndFile.KeyOff(sndFile.m_PlayState.Chn[m_noteChannel[note - NOTE_MIN]]);
				}
				break;
			case seNoteOffOnKeyUp:
				if(m_noteChannel[note - NOTE_MIN] != CHANNELINDEX_INVALID)
				{
					NoteOff(note);
				}
				break;
			case seNoteOffOnKeyRestrike:
				break;
			}
			return wParam;
		}
	} else if(wParam >= kcStartSampleCues && wParam <= kcEndSampleCues)
	{
		PlayOrSetCuePoint(wParam - kcStartSampleCues);
		return wParam;
	}

	// Pass on to CCtrlSamples
	return GetControlDlg()->SendMessage(MSG_MOD_KEYCOMMAND, wParam, lParam);
}


void CViewSample::PlayOrSetCuePoint(size_t cue)
{
	CModDoc *modDoc = GetDocument();
	if(modDoc == nullptr)
		return;
	CTrackerSoundFile &sndFile = modDoc->GetSoundFile();
	ModSample &sample = sndFile.GetSample(m_nSample);
	SmpLength offset = sample.cues[cue];
	if(offset < sample.nLength)
	{
		PlayNote(NOTE_MIDDLEC, offset);
		return;
	}

	const CHANNELINDEX previewChannel = GetPreviewChannel();
	if(previewChannel == CHANNELINDEX_INVALID)
		return;
	modDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_none, "Set Cue Point");
	sample.cues[cue] = m_dwNotifyPos[previewChannel];
	SetModified(SampleHint().Info(), true, false);
}


void CViewSample::OnSampleSliceCuePoints()
{
	const CModDoc *modDoc = GetDocument();
	if(modDoc == nullptr || m_nSample > modDoc->GetNumSamples())
		return;
	const ModSample &sample = modDoc->GetSoundFile().GetSample(m_nSample);
	if(!sample.HasSampleData() || sample.uFlags[CHN_ADLIB])
		return;

	// Sort cue points and add two fake cue points to make things easier below...
	constexpr size_t NUM_CUES = mpt::array_size<decltype(sample.cues)>::size;
	std::array<SmpLength, NUM_CUES + 2> cues;
	bool hasValidCues = false;  // Any cues in ]0, length[
	for(std::size_t i = 0; i < std::size(sample.cues); i++)
	{
		cues[i] = sample.cues[i];
		if(cues[i] == 0 || cues[i] >= sample.nLength)
			cues[i] = sample.nLength;
		else
			hasValidCues = true;
	}
	// Nothing to slice?
	if(!hasValidCues)
		return;

	cues[NUM_CUES] = 0;
	cues[NUM_CUES + 1] = sample.nLength;
	std::sort(cues.begin(), cues.end());
	OnSampleSlice(mpt::as_span(cues));
}


void CViewSample::OnSampleSliceGrid()
{
	const CModDoc *modDoc = GetDocument();
	if(modDoc == nullptr || m_nSample > modDoc->GetNumSamples() || m_gridMode == SampleGridMode::NoGrid)
		return;
	const CTrackerSoundFile &sndFile = modDoc->GetSoundFile();
	const ModSample &sample = sndFile.GetSample(m_nSample);
	if(!sample.HasSampleData() || sample.uFlags[CHN_ADLIB])
		return;

	const double samplesPerSegment = GetGridSegmentSize(sample, sndFile);
	if(samplesPerSegment <= 1.0)
		return;

	std::vector<SmpLength> slicePoints;
	slicePoints.reserve(2 + static_cast<size_t>(sample.nLength / samplesPerSegment));
	slicePoints.push_back(0);
	for(SmpLength i = 1; ; i++)
	{
		SmpLength pos = mpt::saturate_round<SmpLength>(samplesPerSegment * i);
		if(pos >= sample.nLength)
			break;
		slicePoints.push_back(pos);
	}
	slicePoints.push_back(sample.nLength);
	OnSampleSlice(mpt::as_span(slicePoints));
}


void CViewSample::OnSampleSlice(mpt::span<SmpLength> cues)
{
	CModDoc *modDoc = GetDocument();
	if(modDoc == nullptr || m_nSample > modDoc->GetNumSamples())
		return;
	CTrackerSoundFile &sndFile = modDoc->GetSoundFile();
	ModSample &sample = sndFile.GetSample(m_nSample);
	if(!sample.HasSampleData() || sample.uFlags[CHN_ADLIB])
		return;

	// Now slice the sample at each cue point
	for(size_t i = 1; i < cues.size() - 1; i++)
	{
		const SmpLength cue  = cues[i];
		if(cue > cues[i - 1] && cue < cues[i + 1])
		{
			SAMPLEINDEX nextSmp = modDoc->InsertSample();
			if(nextSmp == SAMPLEINDEX_INVALID)
				break;

			ModSample &newSample = sndFile.GetSample(nextSmp);
			newSample = sample;
			newSample.RemoveAllCuePoints();
			newSample.uFlags.reset(SMP_KEEPONDISK);
			newSample.pData.pSample = nullptr;
			sndFile.m_szNames[nextSmp] = sndFile.m_szNames[m_nSample];
			if(newSample.CopyWaveform(sample, cues[i], cues[i + 1]))
			{
				Util::DeleteRange(SmpLength(0), cues[i] - SmpLength(1), newSample.nLoopStart, newSample.nLoopEnd);
				Util::DeleteRange(SmpLength(0), cues[i] - SmpLength(1), newSample.nSustainStart, newSample.nSustainEnd);
				newSample.PrecomputeLoops(sndFile, false);

				if(sndFile.GetNumInstruments() > 0)
				{
					if(auto instr = modDoc->InsertInstrument(nextSmp); instr != INSTRUMENTINDEX_INVALID)
						sndFile.Instruments[instr]->name = sndFile.m_szNames[nextSmp];
				}
			}
		}
	}
	
	modDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_delete, "Slice Sample", cues[1], sample.nLength);
	CallLocked([&] { return SampleEdit::ResizeSample(sample, cues[1], sndFile); });
	sample.PrecomputeLoops(sndFile, true);
	sample.uFlags.reset(SMP_KEEPONDISK);
	SetModified(SampleHint().Info().Data().Names(), true, true);
	modDoc->UpdateAllViews(this, SampleHint().Info().Data().Names(), this);
	modDoc->UpdateAllViews(this, InstrumentHint().Info().Envelope().Names(), this);
}


void CViewSample::OnSampleInsertCuePoint()
{
	CModDoc *modDoc = GetDocument();
	if(modDoc == nullptr || m_nSample > modDoc->GetNumSamples())
		return;
	CTrackerSoundFile &sndFile = modDoc->GetSoundFile();
	ModSample &sample = sndFile.GetSample(m_nSample);
	if(!sample.HasSampleData() || sample.uFlags[CHN_ADLIB] || m_dwMenuParam >= sample.nLength)
		return;

	for(auto &pt : sample.cues)
	{
		if(pt >= sample.nLength)
		{
			modDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_none, "Insert Cue Point");
			pt = m_dwMenuParam;
			SetModified(SampleHint().Info().Data(), true, false);
			break;
		}
	}
}


void CViewSample::OnSampleDeleteCuePoint()
{
	CModDoc *modDoc = GetDocument();
	if(modDoc == nullptr || m_nSample > modDoc->GetNumSamples())
		return;
	CTrackerSoundFile &sndFile = modDoc->GetSoundFile();
	ModSample &sample = sndFile.GetSample(m_nSample);
	if(!sample.HasSampleData() || sample.uFlags[CHN_ADLIB] || m_dwMenuParam >= std::size(sample.cues))
		return;

	modDoc->GetSampleUndo().PrepareUndo(m_nSample, sundo_none, "Delete Cue Point");
	sample.cues[m_dwMenuParam] = MAX_SAMPLE_LENGTH;
	SetModified(SampleHint().Info().Data(), true, false);
}


bool CViewSample::CanZoomSelection() const
{
	return GetZoomLevel(m_dwEndSel - m_dwBeginSel) <= MAX_ZOOM;
}


// Returns auto-zoom level compared to other zoom levels.
// Result is not limited to MIN_ZOOM...MAX_ZOOM range.
int CViewSample::GetZoomLevel(SmpLength length) const
{
	if(m_rcClient.Width() == 0 || length == 0)
		return MAX_ZOOM + 1;

	// When m_nZoom > 0, 2^(m_nZoom - 1) = samplesPerPixel  [1]
	// With auto-zoom setting the whole sample is fitted to screen:
	// ViewScreenWidthInPixels * samplesPerPixel = sampleLength (approximately)  [2].
	// Solve samplesPerPixel from [2], then "m_nZoom" from [1].
	double zoom = static_cast<double>(length) / m_rcClient.Width();
	zoom = 1 + (std::log10(zoom) / std::log10(2.0));
	if(zoom <= 0) zoom -= 2;

	return static_cast<int>(zoom + mpt::signum(zoom));
}


void CViewSample::DoZoom(int direction, const Point &zoomPoint)
{
	const CTrackerSoundFile &sndFile = GetDocument()->GetSoundFile();
	// zoomOrder: Biggest to smallest zoom order.
	std::array<int, (-MIN_ZOOM - 1) + (MAX_ZOOM + 1)> zoomOrder;
	for(int i = 2; i < -MIN_ZOOM + 1; ++i)
		zoomOrder[i - 2] = MIN_ZOOM + i - 2;	// -6, -5, -4, -3...

	for(int i = 1; i <= MAX_ZOOM; ++i)
		zoomOrder[i - 1 + (-MIN_ZOOM - 1)] = i; // 1, 2, 3...
	zoomOrder.back() = 0;
	const int autoZoomLevel = std::max(MIN_ZOOM, GetZoomLevel(sndFile.GetSample(m_nSample).nLength));

	// Move auto-zoom index (=zero) to the right position in the zoom order according to its real zoom strength.
	if (autoZoomLevel < MAX_ZOOM + 1)
	{
		auto p = std::find(zoomOrder.begin(), zoomOrder.end(), autoZoomLevel);
		if(p != zoomOrder.end())
		{
			std::move(p, zoomOrder.end() - 1, p + 1);
			*p = 0;
		}
		else
			MPT_ASSERT_NOTREACHED();
	}
	const std::ptrdiff_t nPos = std::distance(zoomOrder.begin(), std::find(zoomOrder.begin(), zoomOrder.end(), m_nZoom));

	int newZoom;
	if(direction > 0 && nPos > 0)  // Zoom in
		newZoom = zoomOrder[nPos - 1];
	else if(direction < 0 && nPos + 1 < static_cast<std::ptrdiff_t>(zoomOrder.size()))
		newZoom = zoomOrder[nPos + 1];
	else
		return;

	if(m_rcClient.PtInRect(zoomPoint))
	{
		SetZoom(newZoom, ScreenToSample(zoomPoint.x));
	} else
	{
		SetZoom(newZoom);
	}
	SendCtrlMessage(CTRLMSG_SMP_SETZOOM, newZoom);
}


// cppcheck-suppress duplInheritedMember
bool CViewSample::OnMouseWheel(uint32 nFlags, short zDelta, Point pt)
{
	// Ctrl + mouse wheel: zoom control.
	// One scroll direction zooms in and the other zooms out.
	// This behaviour is different from what would happen if simply scrolling
	// the zoom levels in the zoom combobox.
	if (nFlags == ui::MouseControl && GetDocument())
	{
		ScreenToClient(&pt);
		DoZoom(zDelta, pt);
	}

	return CModScrollView::OnMouseWheel(nFlags, zDelta, pt);
}


void CViewSample::OnXButtonUp(uint32 nFlags, uint32 nButton, Point point)
{
	if(nButton == XBUTTON1) OnPrevInstrument();
	else if(nButton == XBUTTON2) OnNextInstrument();
	CModScrollView::OnXButtonUp(nFlags, nButton, point);
}


void CViewSample::OnChangeGridSize()
{
	const CTrackerSoundFile &sndFile = GetDocument()->GetSoundFile();
	const ModSample &sample = sndFile.GetSample(m_nSample);
	CSampleGridDlg dlg{this, m_gridMode, m_gridSegments, m_gridSpacing, m_gridUnit, sample.nLength, sample.GetSampleRate(sndFile.GetType())};
	if(dlg.DoModal() == IDOK)
	{
		m_gridMode = dlg.m_mode;
		switch(m_gridMode)
		{
		case SampleGridMode::NoGrid:
			break;
		case SampleGridMode::DivideIntoSegments:
			m_gridSegments = dlg.m_segments;
			break;
		case SampleGridMode::DivideEveryN:
			m_gridSpacing = dlg.m_spacing;
			m_gridUnit = dlg.m_unit;
			break;
		}
		InvalidateSample(false);
	}
}


void CViewSample::OnQuickFade()
{
	PostCtrlMessage(IDC_SAMPLE_QUICKFADE);
}


void CViewSample::SetTimelineFormat(TimelineFormat fmt)
{
	TrackerSettings::Instance().sampleEditorTimelineFormat = fmt;
	UpdateScrollSize();
	InvalidateTimeline();
}


void CViewSample::OnUpdateUndo(CmdUI *pCmdUI)
{
	CModDoc *pModDoc = GetDocument();
	if ((pCmdUI) && (pModDoc))
	{
		pCmdUI->Enable(pModDoc->GetSampleUndo().CanUndo(m_nSample));
		pCmdUI->SetText(CMainFrame::GetInputHandler()->GetKeyTextFromCommand(kcEditUndo, UL_("Undo ") + mpt::ToUnicode(pModDoc->GetSoundFile().GetCharsetInternal(), pModDoc->GetSampleUndo().GetUndoName(m_nSample))));
	}
}


void CViewSample::OnUpdateRedo(CmdUI *pCmdUI)
{
	CModDoc *pModDoc = GetDocument();
	if ((pCmdUI) && (pModDoc))
	{
		pCmdUI->Enable(pModDoc->GetSampleUndo().CanRedo(m_nSample));
		pCmdUI->SetText(CMainFrame::GetInputHandler()->GetKeyTextFromCommand(kcEditRedo, UL_("Redo ") + mpt::ToUnicode(pModDoc->GetSoundFile().GetCharsetInternal(), pModDoc->GetSampleUndo().GetRedoName(m_nSample))));
	}
}


bool CViewSample::FindNcButtonToolTip(uint32 button, Rect &area, mpt::ustring &text) const
{
	if(button >= SMP_LEFTBAR_BUTTONS || cLeftBarButtons[button] == ID_SEPARATOR || !GetNcButtonRect(button, area))
		return false;
	area.OffsetRect(0, -GetNonClientTop());
	const int buttonID = cLeftBarButtons[button];

	text = LoadResourceString(buttonID);

	CommandID cmd = kcNull;
	switch(buttonID)
	{
	case ID_SAMPLE_ZOOMUP: cmd = kcSampleZoomUp; break;
	case ID_SAMPLE_ZOOMDOWN: cmd = kcSampleZoomDown; break;
	case ID_SAMPLE_DRAW: cmd = kcSampleToggleDrawing; break;
	case ID_SAMPLE_ADDSILENCE: cmd = kcSampleResize; break;
	case ID_SAMPLE_GRID: cmd = kcSampleGrid; break;
	}
	if(cmd != kcNull)
	{
		auto keyText = CMainFrame::GetInputHandler()->m_activeCommandSet->GetKeyTextFromCommand(cmd, 0);
		if(!keyText.empty())
			text += MPT_UFORMAT(" ({})")(keyText);
	}

	return true;
}


bool CViewSample::FindToolTip(Point point, Rect &area, mpt::ustring &text) const
{
	const ModSample &sample = GetDocument()->GetSoundFile().GetSample(m_nSample <= GetDocument()->GetNumSamples() ? m_nSample : 0);
	auto [item, pos] = PointToItem(point, &area);
	switch(item)
	{
	case HitTestItem::LoopStart:
		text = UL_("Loop Start");
		break;
	case HitTestItem::LoopEnd:
		text = UL_("Loop End");
		break;
	case HitTestItem::SustainStart:
		text = UL_("Sustain Start");
		break;
	case HitTestItem::SustainEnd:
		text = UL_("Sustain End");
		break;
	default:
		if(!IsCuePoint(item))
			return false;
		auto cue = CuePointFromItem(item);
		text = MPT_UFORMAT("Cue Point {}")(cue + 1);
	}
	if(pos <= sample.nLength)
		text += UL_(": ") + mpt::ufmt::dec(3, UL_(","), pos);
	return true;
}


OPENMPT_NAMESPACE_END
