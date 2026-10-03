// FLTK port of openmpt/mptrack/Childfrm.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "Globals.h"
#include "PatternCursor.h"

#include "../common/FileReaderFwd.h"
#include "../tracklib/Types.h"

OPENMPT_NAMESPACE_BEGIN

class CModControlView;
class CModControlDlg;

struct GeneralViewState
{
	PlugParamIndex nParam = 0;
	CHANNELINDEX nTab = 0;
	PLUGINDEX nPlugin = 0;
	bool initialized = false;

	std::string Serialize() const { return {}; }
	void Deserialize(FileReader &) { }
};


struct PatternViewState
{
	PATTERNINDEX nPattern = 0;
	PatternCursor cursor = 0;
	PatternRect selection;
	ORDERINDEX nOrder = 0;
	ORDERINDEX initialOrder = ORDERINDEX_INVALID;
	std::bitset<PatternCursor::numColumns> visibleColumns;
	bool initialized = false;

	std::string Serialize() const;
	void Deserialize(FileReader &f);
};

struct SampleViewState
{
	SmpLength dwScrollPos = 0;
	SmpLength dwBeginSel = 0;
	SmpLength dwEndSel = 0;
	SampleChannelSelection channelSelection = SampleChannelSelection::None;
	SAMPLEINDEX nSample = 0;
	SAMPLEINDEX initialSample = 0;

	std::string Serialize() const;
	void Deserialize(FileReader &f);
};


struct InstrumentViewState
{
	float zoom = 4;
	EnvelopeType nEnv = ENV_VOLUME;
	INSTRUMENTINDEX initialInstrument = 0;
	INSTRUMENTINDEX instrument = 0;
	bool bGrid = false;
	bool initialized = false;

	std::string Serialize() const;
	void Deserialize(FileReader &f);
};

struct CommentsViewState
{
	uint32 nId = 0;
	bool initialized = false;

	std::string Serialize() const { return {}; }
	void Deserialize(FileReader &) {}
};



class CChildFrame: public ChildFrameBase
{
	friend class CModControlDlg;
public:
	CChildFrame();
	~CChildFrame() override;

protected:
	static CChildFrame *m_lastActiveFrame;
	static int glMdiOpenCount;

// Attributes
protected:
	CModControlView *m_controlView = nullptr;
	View *m_bottomView = nullptr;
	WindowHandle m_hWndCtrl = nullptr, m_hWndView = nullptr;
	GeneralViewState m_ViewGeneral;
	PatternViewState m_ViewPatterns;
	SampleViewState m_ViewSamples;
	InstrumentViewState m_ViewInstruments;
	CommentsViewState m_ViewComments;
	ViewType m_currentViewType = ViewType::None;
	int m_splitHeight = 0;
	bool m_isDraggingSplitter = false;
	int m_dpi = 0;
	bool m_maxWhenClosed = false;
	bool m_initialActivation = true;

// Operations
public:
	CModControlView *GetModControlView() const { return m_controlView; }
	bool CreateViews(int initialHeight);
	void OnCreateViews(Document &) override { CreateViews(h()); }
	bool ChangeViewType(ViewType newViewType);
	void ForceRefresh();
	void SavePosition(bool exit = false);
	LResult SendCtrlMessage(uint32 uMsg, LParam lParam = 0) const;
	LResult SendViewMessage(uint32 uMsg, LParam lParam = 0) const;
	LResult ActivateView(uint32 nId, LParam lParam);
	WindowHandle GetHwndCtrl() const { return m_hWndCtrl; }
	WindowHandle GetHwndView() const { return m_hWndView; }
	GeneralViewState &GetGeneralViewState() { return m_ViewGeneral; }
	PatternViewState &GetPatternViewState() { return m_ViewPatterns; }
	SampleViewState &GetSampleViewState() { return m_ViewSamples; }
	InstrumentViewState &GetInstrumentViewState() { return m_ViewInstruments; }
	CommentsViewState &GetCommentViewState() { return m_ViewComments; }

	bool IsPatternView() const;

	void SetSplitterHeight(int x);
	int GetSplitterHeight();

	void SaveAllViewStates();
	std::string SerializeView();
	void DeserializeView(FileReader &file);

	void ToggleViews();

	static CChildFrame *LastActiveFrame() { return m_lastActiveFrame; }

// Overrides
public:
	void resize(int x, int y, int width, int height) override;
	int handle(int event) override;
	void ActivateFrame(int nCmdShow = -1) override;
	void OnUpdateFrameTitle(bool bAddToTitle) override;

// Generated message map functions
protected:
	void OnDestroy() override;
	void OnMDIActivate(bool bActivate, Wnd *pActivateWnd, Wnd *pDeactivateWnd);
	void LayoutViews();
	LResult OnChangeViewClass(WParam, LParam lParam);
	LResult OnInstrumentSelected(WParam, LParam lParam);
	UI_DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////



OPENMPT_NAMESPACE_END
