// FLTK port of openmpt/mptrack/Mainbar.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"
#include "CDecimalSupport.h"
#include "MPTrackUtil.h"
#include "CImageListEx.h"
#include "UpdateHints.h"
#include "../soundlib/Snd_defs.h"

OPENMPT_NAMESPACE_BEGIN

enum class MainToolBarItem : uint32;
class CMainToolBar;

class CStereoVU: public Static
{
protected:
	uint8 numChannels = 2;
	uint32 vuMeter[4] = {{}};
	uint64 lastVuUpdateTime;
	bool horizontal = true;
	bool allowRightToLeft = false;

public:
	CStereoVU() { lastVuUpdateTime = Util::GetTickCount64(); }
	void SetVuMeter(uint8 validChannels, const uint32 channels[4], bool force = false);
	void SetOrientation(bool h) { horizontal = h; }

	void draw() override;

protected:
	void DrawVuMeters(ui::Painter &dc);
	void DrawVuMeter(ui::Painter &dc, const Rect &rect, int index);

protected:
	void OnLButtonDown(uint32, Point) override;
};

#define MIN_BASEOCTAVE		0
#define MAX_BASEOCTAVE		8

class CTrackerSoundFile;
class CModDoc;
class CModTree;
class CMainFrame;
struct Notification;

class CMainToolBar: public ToolBar
{
protected:
	CImageListEx m_ImageList, m_ImageListDisabled;
	Static m_StaticOctave, m_StaticTempo, m_StaticSpeed, m_StaticRowsPerBeat, m_StaticGlobalVolume;
	Spinner m_SpinOctave, m_SpinTempo, m_SpinSpeed, m_SpinRowsPerBeat, m_SpinGlobalVolume;
	int m_currentSpeed = 0, m_currentOctave = -1, m_currentRowsPerBeat = 0, m_currentGlobalVolume = 0;
	TEMPO m_currentTempo{1, 0};
	bool m_updating = false;
public:
	CStereoVU m_VuMeter;

public:
	CMainToolBar() = default;

protected:
	void SetRowsPerBeat(ROWINDEX nNewRPB);

public:
	bool Create(Wnd *parent);
	void Init(CMainFrame *);
	uint32 GetBaseOctave() const;
	void SetBaseOctave(uint32 nOctave);
	void SetCurrentSong(CTrackerSoundFile *pModDoc);

	bool ToggleVisibility(MainToolBarItem item);

	// Total size that the bar needs
	Size CalcLayoutSize() const;

protected:
	void RefreshToolbar();
	void UpdateSizes();
	void UpdateControls();

	mpt::ustring GetButtonToolTip(uint32 id) const override;
	void OnTbnDropDownToolBar(NotifyHeader *pNMHDR, LResult *pResult);
	void OnSelectMIDIDevice(uint32 id);
	void OnOctaveChanged();
	void OnTempoSpinDelta(NotifyHeader *notify, LResult *result);
	void OnSpeedChanged();
	void OnTempoChanged();
	void OnRPBChanged();
	void OnGlobalVolChanged();

	UI_DECLARE_MESSAGE_MAP()
};


class CModTreeBar: public Panel
{
public:
	enum Status
	{
		MTB_VERTICAL = 0x01,
		MTB_CAPTURE = 0x02,
		MTB_DRAGGING = 0x04,
		MTB_TRACKER = 0x08,
	};

protected:
	FlagSet<Status> m_status;
	Point ptDragging;
	uint32 m_cxOriginal = 0, m_cyOriginal = 0, m_nTrackPos = 0;
	uint32 m_nTreeSplitRatio = 0;
	bool m_isOnLeft = true;

	std::unique_ptr<Edit> m_filterEdit;
	CModTree *m_filterSource = nullptr;
	uintptr_t m_filterTimer = 0;

public:
	CModTree *m_pModTree = nullptr, *m_pModTreeData = nullptr;

	CModTreeBar();
	~CModTreeBar() override;

public:
	void Init();
	void RecalcLayout();
	void DoMouseMove(Point point);
	void DoLButtonDown(Point point);
	void DoLButtonUp();
	void CancelTracking();
	void RefreshDlsBanks();
	void RefreshMidiLibrary();
	void OnOptionsChanged();
	void OnDocumentCreated(CModDoc *pModDoc);
	void OnDocumentClosed(CModDoc *pModDoc);
	void OnUpdate(CModDoc *pModDoc, UpdateHint hint, HintObject *pHint = nullptr);
	void UpdatePlayPos(CModDoc *pModDoc, Notification *pNotify);
	bool SetTreeSoundfile(FileReader &file);

	void StartTreeFilter(CModTree &source);

	void SetBarOnLeft(const bool left) { m_isOnLeft = left; Invalidate(); }
	bool BarOnLeft() const { return m_isOnLeft; }

	// Width the bar wants to have including the splitter
	int GetDesiredWidth() const;

	void draw() override;

protected:
	bool PreTranslateMessage(int event) override;

	void CloseTreeFilter();
	void CancelTimer();

	int Padding() const;

protected:
	void OnSize(uint32 nType, int cx, int cy) override;
	void OnMouseMove(uint32 nFlags, Point point) override;
	void OnLButtonDown(uint32, Point) override;
	void OnLButtonUp(uint32, Point) override;
	void OnRButtonDown(uint32, Point) override { CancelTracking(); }
	void OnFilterChanged();
	void OnFilterLostFocus();
	void OnTimer(uintptr_t id) override;
	UI_DECLARE_MESSAGE_MAP()
};

DECLARE_FLAGSET(CModTreeBar::Status)


OPENMPT_NAMESPACE_END
