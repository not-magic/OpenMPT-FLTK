// FLTK port of openmpt/mptrack/ChannelManagerDlg.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"
#include "DialogBase.h"

OPENMPT_NAMESPACE_BEGIN

class CModDoc;
class QuickChannelProperties;
struct UpdateHint;

class CChannelManagerDlg : public DialogBase
{
	enum Tab
	{
		kSoloMute      = 0,
		kRecordSelect  = 1,
		kPluginState   = 2,
		kReorderRemove = 3,
		kNumTabs
	};

public:

	static CChannelManagerDlg *sharedInstance() { return sharedInstance_; }
	static CChannelManagerDlg *sharedInstanceCreate();
	static void DestroySharedInstance() { delete sharedInstance_; sharedInstance_ = nullptr; }
	void SetDocument(CModDoc *modDoc);
	CModDoc *GetDocument() const { return m_ModDoc; }
	bool IsDisplayed() const;
	void Update(UpdateHint hint, HintObject* pHint);
	void Show();
	void Hide();

private:
	static CChannelManagerDlg *sharedInstance_;
	std::unique_ptr<QuickChannelProperties> m_quickChannelProperties;

protected:

	enum ButtonAction : uint8
	{
		kUndetermined,
		kAction1,
		kAction2,
	};

	enum MouseButton : uint8
	{
		CM_BT_NONE,
		CM_BT_LEFT,
		CM_BT_RIGHT,
	};

	CChannelManagerDlg();
	~CChannelManagerDlg();

	struct State
	{
		Rect move;
		CHANNELINDEX sourceChn = 0;
		bool removed = false;
		bool select = false;
		bool memoryMute = false;
		uint8 memoryRecordGroup = 0;
		bool memoryNoFx = false;
	};

	std::vector<State> m_states;
	CModDoc *m_ModDoc = nullptr;
	ui::Bitmap m_bkgnd;
	ui::TabCtrl m_tabCtrl;
	Rect m_drawableArea;
	Tab m_currentTab = kSoloMute;
	int m_downX = 0, m_downY = 0;
	int m_moveX = 0, m_moveY = 0;
	int m_buttonHeight = 0;
	ButtonAction m_buttonAction = kUndetermined;
	bool m_leftButton = false;
	bool m_rightButton = false;
	bool m_moveRect = false;
	bool m_show = false;

	CHANNELINDEX ButtonHit(Point point, Rect *invalidate = nullptr) const;
	void MouseEvent(uint32 nFlags, Point point, MouseButton button);
	void ResetState(bool bSelection = true, bool bMove = true, bool bInternal = true, bool bOrder = false);
	void ResizeWindow();

	bool OnInitDialog() override;
	void OnDPIChanged() override;
	bool FindToolTip(Point point, Rect &area, mpt::ustring &text) const override;
	mpt::ustring GetChannelToolTip(uint32 id) const;
	void DoDataExchange(DataExchange *pDX) override;
	void OnApply();
	void OnClose();
	void OnSelectAll();
	void OnInvert();
	void OnAction1() { OnAction(1); }
	void OnAction2() { OnAction(2); }
	void OnAction(uint8 action);
	void OnStore();
	void OnRestore();
	void OnTabSelchange(NotifyHeader*, LResult* pResult);
	void OnPaint(ui::Painter &dc) override;
	void DrawChannels(ui::Painter &dc, const Rect &rcPaint);
	void OnMouseMove(uint32 nFlags,Point point);
	void OnLButtonUp(uint32 nFlags,Point point);
	void OnLButtonDown(uint32 nFlags,Point point);
	void OnRButtonUp(uint32 nFlags,Point point);
	void OnRButtonDown(uint32 nFlags,Point point);
	void OnMButtonDown(uint32 nFlags,Point point);
	void OnLButtonDblClk(uint32 nFlags, Point point) override;
	UI_DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
