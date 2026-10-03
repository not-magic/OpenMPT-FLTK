/*
 * AboutDialog.h
 * -------------
 * Purpose: About dialog with credits, system information and a fancy demo effect.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"
#include "DialogBase.h"

OPENMPT_NAMESPACE_BEGIN

class RawImage;

class CRippleBitmap: public Panel
{

public:

	static constexpr uint32 UPDATE_INTERVAL = 15; // milliseconds

protected:

	std::unique_ptr<RawImage> m_bitmapSrc, m_bitmapTarget;
	std::vector<int32> m_offset1, m_offset2;
	int32 *m_frontBuf, *m_backBuf;
	uint32 m_lastFrame = 0;	// Time of last frame
	uint32 m_lastRipple = 0;	// Time of last added ripple
	bool m_frame = false;		// Backbuffer toggle
	bool m_damp = true;		// Ripple damping status
	bool m_activity = true;	// There are actually some ripples

public:

	CRippleBitmap();
	~CRippleBitmap();
	bool Animate();

protected:
	void OnPaint(ui::Painter &dc) override;

	void OnMouseMove(uint32 nFlags, Point point) override;

	UI_DECLARE_MESSAGE_MAP()
};


class CAboutDlg : public DialogBase
{
protected:
	CRippleBitmap m_bmp;
	TabCtrl m_Tab;
	Edit m_TabEdit;
	uintptr_t m_TimerID = 0;
	static constexpr uintptr_t TIMERID_ABOUT_DEFAULT = 3;

public:
	static CAboutDlg *instance;

	~CAboutDlg();

	// Implementation
protected:
	bool OnInitDialog() override;
	void OnOK() override;
	void OnCancel() override;
	UI_DECLARE_MESSAGE_MAP()
	void DoDataExchange(DataExchange* pDX) override;
	void OnTabChange(NotifyHeader *pNMHDR, LResult *pResult);
	void OnTimer(uintptr_t nIDEvent);
public:
	static mpt::ustring GetTabText(int tab);
};

OPENMPT_NAMESPACE_END
