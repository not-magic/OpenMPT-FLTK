// FLTK port of openmpt/mptrack/Ctrl_pat.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "Globals.h"

OPENMPT_NAMESPACE_BEGIN

class COrderList;
class CCtrlPatterns;
class ModSequence;

struct OrdSelection
{
	ORDERINDEX firstOrd = 0, lastOrd = 0;
	ORDERINDEX GetSelCount() const { return lastOrd - firstOrd + 1; }
};

class COrderList: public ScrollView
{
	friend class CCtrlPatterns;
protected:
	ui::Font m_hFont;
	int m_cxFont = 0, m_cyFont = 0;

	CModDoc &m_modDoc;
	CCtrlPatterns &m_pParent;

	ORDERINDEX m_nXScroll = 0;                        // Index of the leftmost displayed order list item
	ORDERINDEX m_nScrollPos = 0;                      // First selection point / selected order
	ORDERINDEX m_nScrollPos2nd = ORDERINDEX_INVALID;  // 2nd selection point if multiple orders are selected (not neccessarily the higher order - GetCurSel() is taking care of that.)
	ORDERINDEX m_nDropPos = ORDERINDEX_INVALID, m_nMouseDownPos = ORDERINDEX_INVALID, m_playPos = ORDERINDEX_INVALID;
	ORDERINDEX m_nDragOrder = ORDERINDEX_INVALID, m_menuOrder = ORDERINDEX_INVALID;
	//To tell how many orders('orderboxes') to show at least
	//on both sides of current order(when updating orderslist position).
	int m_nOrderlistMargins = 0;
	ORDERINDEX m_scrollMax = 0;  // Largest scroll position in orders
	bool m_bScrolling = false, m_bDragging = false;

public:
	COrderList(CCtrlPatterns &parent, CModDoc &document);

public:
	bool Init(const Rect &);
	void UpdateView(UpdateHint hint, HintObject *pObj = nullptr);
	void InvalidateSelection();
	PATTERNINDEX GetCurrentPattern() const;
	// make the current selection the secondary selection (used for keyboard orderlist navigation)
	inline void SetCurSelTo2ndSel(bool isSelectionKeyPressed)
	{
		if(isSelectionKeyPressed && m_nScrollPos2nd == ORDERINDEX_INVALID)
			m_nScrollPos2nd = m_nScrollPos;
		else if(!isSelectionKeyPressed && m_nScrollPos2nd != ORDERINDEX_INVALID)
			m_nScrollPos2nd = ORDERINDEX_INVALID;
	};
	void SetSelection(ORDERINDEX firstOrd, ORDERINDEX lastOrd = ORDERINDEX_INVALID);
	// Why VC wants to inline this huge function is beyond my understanding...
	MPT_ATTR_NOINLINE MPT_DECL_NOINLINE bool SetCurSel(ORDERINDEX sel, bool setPlayPos = true, bool shiftClick = false, bool ignoreCurSel = false);
	void UpdateScrollInfo();
	void UpdateInfoText();
	int GetFontWidth();

	void QueuePattern(ORDERINDEX order, OrderTransitionMode transitionMode);

	// Check if this module is currently playing
	bool IsPlaying() const;

	ORDERINDEX GetOrderFromPoint(const Point &pt) const;
	Rect GetRectFromOrder(ORDERINDEX ord) const;

	// Get the currently selected pattern(s).
	// Set ignoreSelection to true if only the first selected point is important.
	OrdSelection GetCurSel(bool ignoreSelection = false) const;

	// Sets target margin value and returns the effective margin value.
	ORDERINDEX SetMargins(int);

	// Returns the effective margin value.
	ORDERINDEX GetMargins() { return GetMargins(GetMarginsMax()); }

	// Returns the effective margin value.
	ORDERINDEX GetMargins(const ORDERINDEX maxMargins) const { return std::min(maxMargins, static_cast<ORDERINDEX>(m_nOrderlistMargins)); }

	// Returns maximum margin value given current window width.
	ORDERINDEX GetMarginsMax() { return GetMarginsMax(GetLength()); }

	// Returns maximum margin value when shown sequence has nLength orders.
	// For example: If length is 4 orders -> maxMargins = 4/2 - 1 = 1;
	// if maximum is 5 -> maxMargins = (int)5/2 = 2
	ORDERINDEX GetMarginsMax(const ORDERINDEX length) const { return (length > 0 && length % 2 == 0) ? length / 2 - 1 : length / 2; }

	// Returns the number of sequence items visible in the list.
	ORDERINDEX GetLength();

	// Return true if given order is in margins given that first shown order
	// is 'startOrder'. Begin part of the whole sequence
	// is not interpreted to be in margins regardless of the margin value.
	bool IsOrderInMargins(int order, int startOrder);

	// Ensure that a given order index is visible in the orderlist view.
	void EnsureVisible(ORDERINDEX order);

	// Set given sqeuence and update orderlist display.
	void SelectSequence(const SEQUENCEINDEX nSeq);

	// Helper function for entering pattern number
	void EnterPatternNum(int enterNum);

	void OnCopy(bool onlyOrders);

	// Update play state and order lock ranges after inserting order items.
	void InsertUpdatePlaystate(ORDERINDEX first, ORDERINDEX last);
	// Update play state and order lock ranges after deleting order items.
	void DeleteUpdatePlaystate(ORDERINDEX first, ORDERINDEX last);

	bool PreTranslateMessage(int event) override;
	bool FindToolTip(Point point, Rect &area, mpt::ustring &text) const override;

protected:
	ModSequence& Order();
	const ModSequence& Order() const;

	void SetScrollPos(int pos);
	int GetScrollPos(bool getTrackPos = false);

	// Resizes the order list if the specified order is past the order list length
	bool EnsureEditable(ORDERINDEX ord);

	void OnPaint(ui::Painter &dc) override;
	bool OnEraseBkgnd(ui::Painter *) { return true; }
	void OnSetFocus(Wnd *);
	void OnKillFocus(Wnd *);
	void OnLButtonDown(uint32, Point);
	void OnLButtonDblClk(uint32, Point);
	void OnRButtonUp(uint32, Point);
	void OnLButtonUp(uint32, Point);
	void OnMButtonDown(uint32, Point);
	void OnMouseMove(uint32, Point);
	bool OnScroll(uint32 code, uint32 position, bool isDoScroll = true) override;
	void OnSize(uint32 nType, int cx, int cy);
	void OnSwitchToView();
	void OnInsertOrder();
	void OnInsertSeparatorPattern();
	void OnDeleteOrder();
	void OnRenderOrder();
	void OnPatternProperties();
	void OnPlayerPlay();
	void OnPlayerPause();
	void OnPlayerPlayFromStart();
	void OnPatternPlayFromStart();
	void OnCreateNewPattern();
	void OnDuplicatePattern();
	void OnMergePatterns();
	void OnPatternCopy();
	void OnPatternPaste();
	void OnSetRestartPos();
	void OnEditCopy() { OnCopy(false); }
	void OnEditCopyOrders() { OnCopy(true); }
	void OnEditCut();
	LResult OnDragonDropping(WParam bDoDrop, LParam lParam);
	void OnSelectSequence(uint32 nid);
	LResult OnCustomKeyMsg(WParam, LParam);
	void OnLockPlayback();
	void OnUnlockPlayback();
	void OnQueueAtPatternEnd() { QueuePattern(m_menuOrder, OrderTransitionMode::AtPatternEnd); }
	void OnQueueAtMeasureEnd() { QueuePattern(m_menuOrder, OrderTransitionMode::AtMeasureEnd); }
	void OnQueueAtBeatEnd() { QueuePattern(m_menuOrder, OrderTransitionMode::AtBeatEnd); }
	void OnQueueAtRowEnd() { QueuePattern(m_menuOrder, OrderTransitionMode::AtRowEnd); }
	UI_DECLARE_MESSAGE_MAP()
};


class CCtrlPatterns: public CModControlDlg
{
	friend class COrderList;
protected:
	COrderList m_OrderList;
	Button m_BtnPrev, m_BtnNext;
	ComboBox m_CbnInstrument;
	Edit m_EditPatName;
	Spinner m_SpinSpacing, m_SpinSequence;
	SpinButton m_SpinInstrument;
	CModControlBar m_ToolBar;
	INSTRUMENTINDEX m_nInstrument = 0;
	bool m_bRecord = false, m_bVUMeters = false, m_bPluginNames = false;
	bool m_instrDropdownOpen = false;

public:
	CCtrlPatterns(CModControlView &parent, CModDoc &document);

public:
	const ModSequence &Order() const;
	ModSequence &Order();

	void SetCurrentPattern(PATTERNINDEX nPat);
	bool SetCurrentInstrument(uint32 nIns);
	bool GetFollowSong() const;
	bool GetLoopPattern() const;
	COrderList &GetOrderList() { return m_OrderList; }
	Setting<int32> &GetSplitPosRef() override;
	bool OnInitDialog() override;
	void DoDataExchange(DataExchange* pDX) override;	// DDX/DDV support
	void RecalcLayout() override;
	void UpdateView(UpdateHint hint = UpdateHint(), HintObject *pObj = nullptr) override;
	ViewType GetAssociatedViewType() override;
	LResult OnModCtrlMsg(WParam wParam, LParam lParam) override;
	void OnActivatePage(LParam) override;
	void OnDeactivatePage() override;
	mpt::ustring GetToolTipText(uint32 id, WindowHandle hwnd) const override;
	void OnDPIChanged() override;
protected:
	void OnVScroll(uint32 nSBCode, uint32 nPos, Wnd *pScrollBar) override;
	void OnTbnDropDownToolBar(NotifyHeader *pNMHDR, LResult *pResult);
	void OnSequenceNext();
	void OnSequencePrev();
	void OnChannelManager();
	bool OnKeyDown(uint32 nChar, uint32 nRepCnt, uint32 nFlags) override;
	void OnPlayerPause();
	void OnPatternNew();
	void OnPatternDuplicate();
	void OnPatternMerge();
	void OnPatternStop();
	void OnPatternPlay();
	void OnPatternPlayRow();
	void OnPatternPlayFromStart();
	void OnPatternRecord();
	void OnPatternVUMeters();
	void OnPatternViewPlugNames();
	void OnPatternProperties();
	void OnPatternExpand();
	void OnPatternShrink();
	void OnPatternAmplify();
	void OnPatternPaste();
	void OnToggleMetronome();
	void OnMetronomeSettings();
	void OnFollowSong();
	void OnChangeLoopStatus();
	// cppcheck-suppress duplInheritedMember
	void OnSwitchToView();
	void OnInstrumentChanged();
	void OnPrevInstrument();
	void OnNextInstrument();
	void OnSpacingChanged();
	void OnPatternNameChanged();
	void OnSequenceNameChanged();
	void OnChordEditor();
	void OnDetailSwitch();
	void OnDetailInstr();
	void OnDetailVolume();
	void OnDetailEffect();
	void OnEditUndo();
	void OnUpdateRecord(CmdUI *pCmdUI);
	void TogglePluginEditor();
	void OnToggleOverflowPaste();
	void OnSequenceNumChanged();
	void OnOpenInstrumentDropdown() { m_instrDropdownOpen = true; }
	void OnCancelInstrumentDropdown() { m_instrDropdownOpen = false; }

	UI_DECLARE_MESSAGE_MAP()
private:
	bool HasValidPlug(INSTRUMENTINDEX instr) const;
public:
	bool OnMouseWheel(uint32 nFlags, short zDelta, Point pt);
	void OnXButtonUp(uint32 nFlags, uint32 nButton, Point point);
};

OPENMPT_NAMESPACE_END
