/*
 * EffectVis.cpp
 * -------------
 * Purpose: Implementation of parameter visualisation dialog.
 * Notes  : (currenlty none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "ui/Ui.h"
#include "EffectVis.h"
#include "Childfrm.h"
#include "Globals.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "resource.h"
#include "View_pat.h"


OPENMPT_NAMESPACE_BEGIN

CEffectVis::Action CEffectVis::m_nAction = CEffectVis::Action::OverwriteFX;

UI_MESSAGE_MAP_BEGIN(CEffectVis, DialogBase)
	UI_NOTIFY(ui::ComboSelChange, IDC_VISACTION,     &CEffectVis::OnActionChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_VISEFFECTLIST, &CEffectVis::OnEffectChanged)
UI_MESSAGE_MAP_END()


CEffectVis::CEffectVis(CViewPattern *pViewPattern, ROWINDEX startRow, ROWINDEX endRow, CHANNELINDEX nchn, CModDoc &modDoc, PATTERNINDEX pat)
	: m_ModDoc(modDoc)
	, m_SndFile(modDoc.GetSoundFile())
	, m_pViewPattern(pViewPattern)
	, m_effectInfo(modDoc.GetSoundFile())
{
	m_nFillEffect = m_effectInfo.GetIndexFromEffect(CMD_SMOOTHMIDI, 0);
	m_templatePCNote.Set(NOTE_PCS, 1, 0, 0);
	UpdateSelection(startRow, endRow, nchn, pat);
}


void CEffectVis::DoDataExchange(DataExchange* pDX)
{
	DialogBase::DoDataExchange(pDX);
	pDX->BindControl(IDC_VISSTATUS, m_edVisStatus);
	pDX->BindControl(IDC_VISACTION, m_cmbActionList);
}


void CEffectVis::OnActionChanged()
{
	const auto oldActionWasPC = m_nAction == Action::FillPC || m_nAction == Action::OverwritePC;
	m_nAction = static_cast<Action>(m_cmbActionList.GetItemData(m_cmbActionList.GetCurSel()));
	const auto newActionIsPC = m_nAction == Action::FillPC || m_nAction == Action::OverwritePC;

	m_cmbEffectList.EnableWindow((m_nAction == Action::Preserve) ? false : true);

	if(oldActionWasPC != newActionIsPC)
		UpdateEffectList();
}


void CEffectVis::OnEffectChanged()
{
	if(m_nAction == Action::FillPC || m_nAction == Action::OverwritePC)
		m_templatePCNote.SetValueVolCol(static_cast<uint16>(m_cmbEffectList.GetCurSel()));
	else
		m_nFillEffect = static_cast<uint32>(m_cmbEffectList.GetItemData(m_cmbEffectList.GetCurSel()));
}


void CEffectVis::OnPaint(ui::Painter &dc)
{
	DrawGrid(dc);
	DrawNodes(dc);
	DrawPlayCursor(dc);
}


uint16 CEffectVis::GetParam(ROWINDEX row) const
{
	uint16 paramValue = 0;

	if(m_SndFile.Patterns.IsValidPat(m_nPattern))
	{
		const ModCommand &m = *m_SndFile.Patterns[m_nPattern].GetpModCommand(row, m_nChan);
		if (m.IsPcNote())
		{
			paramValue = m.GetValueEffectCol();
		} else
		{
			paramValue = m.param;
		}
	}

	return paramValue;
}


// Sets a row's param value based on the vertical cursor position.
// Sets either plain pattern effect parameter or PC note parameter
// as appropriate, depending on contents of row.
void CEffectVis::SetParamFromY(ROWINDEX row, int y)
{
	if(!m_SndFile.Patterns.IsValidPat(m_nPattern))
		return;

	ModCommand &m = *m_SndFile.Patterns[m_nPattern].GetpModCommand(row, m_nChan);
	if(IsPcNote(row))
	{
		uint16 param = ScreenYToPCParam(y);
		m.SetValueEffectCol(param);
	} else
	{
		ModCommand::PARAM param = ScreenYToFXParam(y);
		// Cap the parameter value as appropriate, based on effect type (e.g. Zxx gets capped to [0x00,0x7F])
		m_effectInfo.GetEffectFromIndex(m_effectInfo.GetIndexFromEffect(m.command, param), param);
		m.param = param;
	}
}



EffectCommand CEffectVis::GetCommand(ROWINDEX row) const
{
	if(m_SndFile.Patterns.IsValidPat(m_nPattern))
		return static_cast<EffectCommand>(m_SndFile.Patterns[m_nPattern].GetpModCommand(row, m_nChan)->command);
	else
		return CMD_NONE;
}


void CEffectVis::SetCommand(ROWINDEX row, EffectCommand command)
{
	if(m_SndFile.Patterns.IsValidPat(m_nPattern))
	{
		ModCommand &m = *m_SndFile.Patterns[m_nPattern].GetpModCommand(row, m_nChan);
		if(m.IsPcNote())
		{
			// Clear PC note
			m.note = 0;
			m.instr = 0;
			m.volcmd = VOLCMD_NONE;
			m.vol = 0;
		}
		m.command = command;
	}
}


std::pair<ROWINDEX, ROWINDEX> CEffectVis::GetTimeSignature() const
{
	ROWINDEX beat = m_SndFile.m_nDefaultRowsPerBeat, measure = m_SndFile.m_nDefaultRowsPerMeasure;
	if(m_SndFile.Patterns.IsValidIndex(m_nPattern) && m_SndFile.Patterns[m_nPattern].GetOverrideSignature())
	{
		beat = m_SndFile.Patterns[m_nPattern].GetRowsPerBeat();
		measure = m_SndFile.Patterns[m_nPattern].GetRowsPerMeasure();
	}
	return std::make_pair(beat, measure);
}


int CEffectVis::RowToScreenX(ROWINDEX row) const
{
	if ((row >= m_startRow) || (row <= m_endRow))
		return mpt::saturate_round<int>(m_rcDraw.left + m_innerBorder + (row - m_startRow) * m_pixelsPerRow);
	return -1;
}


int CEffectVis::RowToScreenY(ROWINDEX row) const
{
	int screenY = -1;

	if(m_SndFile.Patterns.IsValidPat(m_nPattern))
	{
		const ModCommand &m = *m_SndFile.Patterns[m_nPattern].GetpModCommand(row, m_nChan);
		if(m.IsPcNote())
		{
			uint16 paramValue = m.GetValueEffectCol();
			screenY = PCParamToScreenY(paramValue);
		} else
		{
			uint16 paramValue = m.param;
			screenY = FXParamToScreenY(paramValue);
		}
	}

	return screenY;
}


int CEffectVis::FXParamToScreenY(uint16 param) const
{
	if(param >= 0x00 && param <= 0xFF)
		return mpt::saturate_round<int>(m_rcDraw.bottom - param * m_pixelsPerFXParam);
	return -1;
}


int CEffectVis::PCParamToScreenY(uint16 param) const
{
	if(param >= 0x00 && param <= ModCommand::maxColumnValue)
		return mpt::saturate_round<int>(m_rcDraw.bottom - param*m_pixelsPerPCParam);
	return -1;
}


ModCommand::PARAM CEffectVis::ScreenYToFXParam(int y) const
{
	if(y <= FXParamToScreenY(0xFF))
		return 0xFF;

	if(y >= FXParamToScreenY(0x00))
		return 0x00;

	return mpt::saturate_round<ModCommand::PARAM>((m_rcDraw.bottom - y) / m_pixelsPerFXParam);
}


uint16 CEffectVis::ScreenYToPCParam(int y) const
{
	if(y <= PCParamToScreenY(ModCommand::maxColumnValue))
		return ModCommand::maxColumnValue;

	if(y >= PCParamToScreenY(0x00))
		return 0x00;

	return mpt::saturate_round<uint16>((m_rcDraw.bottom - y) / m_pixelsPerPCParam);
}


ROWINDEX CEffectVis::ScreenXToRow(int x) const
{
	if(x <= RowToScreenX(m_startRow))
		return m_startRow;

	if(x >= RowToScreenX(m_endRow))
		return m_endRow;

	return mpt::saturate_round<ROWINDEX>(m_startRow + (x - m_innerBorder) / m_pixelsPerRow);
}


void CEffectVis::DrawGrid(ui::Painter &dc)
{
	const auto [beat, measure] = GetTimeSignature();

	dc.FillSolidRect(m_rcDraw, 0);
	for(ROWINDEX row = m_startRow; row <= m_endRow; row++)
	{
		if(row % measure == 0)
			dc.SetPenColor(RGB(0xFF, 0xFF, 0xFF));
		else if(row % beat == 0)
			dc.SetPenColor(RGB(0x99, 0x99, 0x99));
		else
			dc.SetPenColor(RGB(0x55, 0x55, 0x55));
		const int x1 = RowToScreenX(row);
		dc.DrawLine(x1, m_rcDraw.top, x1, m_rcDraw.bottom);
	}

	// Draw horizontal grid lines
	constexpr uint32 numHorizontalLines = 4;
	for(uint32 i = 0; i < numHorizontalLines; i++)
	{
		ColorRef c = 0;
		switch(i % 4)
		{
		case 0: c = RGB(0x00, 0x00, 0x00); break;
		case 1: c = RGB(0x40, 0x40, 0x40); break;
		case 2: c = RGB(0x80, 0x80, 0x80); break;
		case 3: c = RGB(0xCC, 0xCC, 0xCC); break;
		}
		dc.SetPenColor(c);
		const int y1 = m_rcDraw.bottom / numHorizontalLines * i;
		dc.DrawLine(m_rcDraw.left + m_innerBorder, y1, m_rcDraw.right - m_innerBorder, y1);
	}
}


void CEffectVis::SetPlayCursor(PATTERNINDEX nPat, ROWINDEX nRow)
{
	if(nPat == m_nPattern && nRow == m_nOldPlayPos)
		return;

	m_nOldPlayPos = ((nRow < m_startRow) || (nRow > m_endRow) || (nPat != m_nPattern)) ? ROWINDEX_INVALID : nRow;
	InvalidateRect(&m_rcDraw, false);
}


void CEffectVis::DrawPlayCursor(ui::Painter &dc)
{
	if(m_nOldPlayPos < m_startRow || m_nOldPlayPos > m_endRow)
		return;
	const int x1 = RowToScreenX(m_nOldPlayPos);
	dc.SetPenColor(TrackerSettings::Instance().rgbCustomColors[MODCOLOR_SAMPLE]);
	dc.DrawLine(x1, m_rcDraw.top, x1, m_rcDraw.bottom);
}


void CEffectVis::DrawNodes(ui::Painter &dc)
{
	if(m_rcDraw.IsRectEmpty())
		return;

	if(m_nDragItem < m_startRow || m_nDragItem > m_endRow)
		m_nDragItem = m_startRow;

	const int lineWidth = ui::ScalePixels(1, this);
	const int nodeSizeHalf = m_nodeSizeHalf;
	const int nodeSizeHalf2 = nodeSizeHalf - lineWidth + 1;
	const int nodeSize = 2 * nodeSizeHalf + 1;

	for(ROWINDEX row = m_startRow; row <= m_endRow; row++)
	{
		const ColorRef col = IsPcNote(row) ? RGB(0xFF, 0xFF, 0x00) : RGB(0xD0, 0xFF, 0xFF);
		const int x = RowToScreenX(row);
		const int y = RowToScreenY(row);
		dc.FillSolidRect(x - nodeSizeHalf, y - nodeSizeHalf, nodeSize, lineWidth, col);  // Top
		dc.FillSolidRect(x + nodeSizeHalf2, y - nodeSizeHalf, lineWidth, nodeSize, col); // Right
		dc.FillSolidRect(x - nodeSizeHalf, y + nodeSizeHalf2, nodeSize, lineWidth, col); // Bottom
		dc.FillSolidRect(x - nodeSizeHalf, y - nodeSizeHalf, lineWidth, nodeSize, col);  // Left
	}
}


void CEffectVis::InvalidateRow(int row)
{
	if (((uint32)row < m_startRow) ||  ((uint32)row > m_endRow)) return;

//It seems this optimisation doesn't work properly yet.	Disable in Update()

	int x = RowToScreenX(row);
	Rect invalidated;
	invalidated.bottom = m_rcDraw.bottom;
	invalidated.top = m_rcDraw.top;
	invalidated.left = x - m_nodeSizeHalf;
	invalidated.right = x + m_nodeSizeHalf + 1;
	InvalidateRect(&invalidated, false);
}


void CEffectVis::OpenEditor(Wnd *parent)
{
	Create(IDD_EFFECTVISUALIZER, parent);

	if(TrackerSettings::Instance().effectVisWidth > 0 && TrackerSettings::Instance().effectVisHeight > 0)
	{
		WINDOWPLACEMENT wnd;
		wnd.length = sizeof(wnd);
		GetWindowPlacement(&wnd);
		wnd.showCmd = true;
		Rect rect = wnd.rcNormalPosition;
		const auto dpi = ui::GetDpiForWindow(this);
		if(TrackerSettings::Instance().effectVisX > int32_min && TrackerSettings::Instance().effectVisY > int32_min)
		{
			Rect mainRect;
			CMainFrame::GetMainFrame()->GetWindowRect(mainRect);
			rect.left = mainRect.left + MulDiv(TrackerSettings::Instance().effectVisX, dpi, 96);
			rect.top = mainRect.top + MulDiv(TrackerSettings::Instance().effectVisY, dpi, 96);
		}
		rect.right = rect.left + MulDiv(TrackerSettings::Instance().effectVisWidth, dpi, 96);
		rect.bottom = rect.top + MulDiv(TrackerSettings::Instance().effectVisHeight, dpi, 96);
		wnd.rcNormalPosition = rect;
		SetWindowPlacement(&wnd);
	}

	ShowWindow(true);
}


void CEffectVis::OnClose()
{
	DoClose();
}


void CEffectVis::OnOK()
{
	OnClose();
}


void CEffectVis::OnCancel()
{
	OnClose();
}


void CEffectVis::DoClose()
{
	WINDOWPLACEMENT wnd;
	wnd.length = sizeof(wnd);
	GetWindowPlacement(&wnd);
	Rect mainRect;
	CMainFrame::GetMainFrame()->GetWindowRect(mainRect);

	Rect rect = wnd.rcNormalPosition;
	rect.MoveToXY(rect.left - mainRect.left, rect.top - mainRect.top);
	const auto dpi = ui::GetDpiForWindow(this);
	TrackerSettings::Instance().effectVisWidth = MulDiv(rect.Width(), 96, dpi);
	TrackerSettings::Instance().effectVisHeight = MulDiv(rect.Height(), 96, dpi);
	TrackerSettings::Instance().effectVisX = MulDiv(rect.left, 96, dpi);
	TrackerSettings::Instance().effectVisY = MulDiv(rect.top, 96, dpi);

	DestroyWindow();
}


void CEffectVis::PostNcDestroy()
{
	m_pViewPattern->m_pEffectVis = nullptr;
}


void CEffectVis::OnSize(uint32 nType, int cx, int cy)
{
	MPT_UNREFERENCED_PARAMETER(nType);
	MPT_UNREFERENCED_PARAMETER(cx);
	MPT_UNREFERENCED_PARAMETER(cy);

	m_nodeSizeHalf = ui::ScalePixels(3, this);
	m_marginBottom = ui::ScalePixels(20, this);
	m_innerBorder = ui::ScalePixels(4, this);

	GetClientRect(&m_rcFullWin);
	m_rcDraw.SetRect(m_rcFullWin.left, m_rcFullWin.top, m_rcFullWin.right, m_rcFullWin.bottom - m_marginBottom);

	const int actionListWidth = ui::ScalePixels(170, this);
	const int commandListWidth = ui::ScalePixels(160, this);

	const int controlsTop = m_rcDraw.bottom, controlsHeight = m_rcFullWin.bottom - m_rcDraw.bottom;
	if(m_edVisStatus.IsWindow())
		m_edVisStatus.SetWindowPos(nullptr, m_rcFullWin.left, controlsTop, m_rcFullWin.right - commandListWidth - actionListWidth, controlsHeight, ui::PosNoActivate | ui::PosShowWindow | ui::PosNoZOrder);
	if(m_cmbActionList.IsWindow())
		m_cmbActionList.SetWindowPos(nullptr, m_rcFullWin.right - commandListWidth - actionListWidth, controlsTop, actionListWidth, controlsHeight, ui::PosNoActivate | ui::PosShowWindow | ui::PosNoZOrder);
	if(m_cmbEffectList.IsWindow())
		m_cmbEffectList.SetWindowPos(nullptr, m_rcFullWin.right - commandListWidth, controlsTop, commandListWidth, controlsHeight, ui::PosNoActivate | ui::PosShowWindow | ui::PosNoZOrder);

	if(m_nRows)
		m_pixelsPerRow = (float)(m_rcDraw.Width() - m_innerBorder * 2) / (float)m_nRows;
	else
		m_pixelsPerRow = 1;
	m_pixelsPerFXParam = (float)(m_rcDraw.Height())/(float)0xFF;
	m_pixelsPerPCParam = (float)(m_rcDraw.Height())/(float)ModCommand::maxColumnValue;
	InvalidateRect(nullptr, false);	//redraw everything
}

void CEffectVis::Update()
{
	if(IsWindow())
	{
		if (m_nRowToErase<0)
			InvalidateRect(NULL, false);	// redraw everything
		else
		{
			InvalidateRow(m_nRowToErase);
			m_nParamToErase=-1;
			m_nRowToErase=-1;
		}

	}
}

void CEffectVis::UpdateSelection(ROWINDEX startRow, ROWINDEX endRow, CHANNELINDEX nchn, PATTERNINDEX pat)
{
	m_startRow = startRow;
	m_endRow = endRow;
	m_nRows = endRow - startRow;
	m_nChan = nchn;
	m_nPattern = pat;

	//Check pattern, start row and channel exist
	if(!m_SndFile.Patterns.IsValidPat(m_nPattern) || !m_SndFile.Patterns[m_nPattern].IsValidRow(m_startRow) || m_nChan >= m_SndFile.GetNumChannels())
	{
		DoClose();
		return;
	}

	//Check end exists
	if(!m_SndFile.Patterns[m_nPattern].IsValidRow(m_endRow))
	{
		m_endRow = m_SndFile.Patterns[m_nPattern].GetNumRows() - 1;
		m_nRows = m_endRow - startRow;
	}

	if(m_nDragItem < m_startRow || m_nDragItem > m_endRow)
		m_nDragItem = m_startRow;

	if(m_nRows)
		m_pixelsPerRow = (float)(m_rcDraw.Width() - m_innerBorder * 2) / (float)m_nRows;
	else
		m_pixelsPerRow = 1;
	m_pixelsPerFXParam = (float)(m_rcDraw.Height())/(float)0xFF;
	m_pixelsPerPCParam = (float)(m_rcDraw.Height())/(float)ModCommand::maxColumnValue;

	Update();

}

void CEffectVis::OnRButtonDown(uint32 nFlags, Point point)
{
	if (!(m_dwStatus & FXVSTATUS_LDRAGGING))
	{
		SetFocus();
		SetCapture();

		m_nDragItem = ScreenXToRow(point.x);
		m_dwStatus |= FXVSTATUS_RDRAGGING;
		m_ModDoc.GetPatternUndo().PrepareUndo(static_cast<PATTERNINDEX>(m_nPattern), m_nChan, m_nDragItem, 1, 1, "Parameter Editor entry");
		OnMouseMove(nFlags, point);
	}

	DialogBase::OnRButtonDown(nFlags, point);
}

void CEffectVis::OnRButtonUp(uint32 nFlags, Point point)
{
	ReleaseCapture();
	m_dwStatus = 0x00;
	DialogBase::OnRButtonUp(nFlags, point);
}

void CEffectVis::OnMouseMove(uint32 nFlags, Point point)
{
	DialogBase::OnMouseMove(nFlags, point);

	ROWINDEX row = ScreenXToRow(point.x);

	if(m_dwStatus & FXVSTATUS_RDRAGGING)
	{
		m_nRowToErase = static_cast<int>(m_nDragItem);
		m_nParamToErase = GetParam(m_nDragItem);

		MakeChange(m_nDragItem, point.y);
		Update();
	} else if(m_dwStatus & FXVSTATUS_LDRAGGING)
	{
		// Interpolate if we detect that rows have been skipped but the left mouse button was not released.
		// This ensures we produce a smooth curve even when we are not notified of mouse movements at a high frequency (e.g. if CPU usage is high)
		const int steps = std::abs((int)row - (int)m_nLastDrawnRow);
		if (m_nLastDrawnRow != ROWINDEX_INVALID && m_nLastDrawnRow > m_startRow && steps > 1)
		{
			int direction = ((int)(row - m_nLastDrawnRow) > 0) ? 1 : -1;
			float factor = (float)(point.y - m_nLastDrawnY)/(float)steps + 0.5f;

			int currentRow;
			for (int i=1; i<=steps; i++)
			{
				currentRow = m_nLastDrawnRow+(direction*i);
				int interpolatedY = mpt::saturate_round<int>(m_nLastDrawnY + ((float)i * factor));
				MakeChange(currentRow, interpolatedY);
			}

			//Don't use single value update
			m_nRowToErase = -1;
			m_nParamToErase = -1;
		} else
		{
			m_nRowToErase = -1;
			m_nParamToErase = -1;
			MakeChange(row, point.y);
		}

		// Remember last modified point in case we need to interpolate
		m_nLastDrawnRow = row;
		m_nLastDrawnY = point.y;
		Update();
	}

	//update status bar
	mpt::ustring status;
	mpt::ustring effectName;
	uint16 paramValue;

	if (IsPcNote(row))
	{
		paramValue = ScreenYToPCParam(point.y);
		effectName = ui::Format(UL_("%s"), UL_("Param Control")); // TODO - show smooth & plug+param
	} else
	{
		paramValue = ScreenYToFXParam(point.y);
		m_effectInfo.GetEffectInfo(m_effectInfo.GetIndexFromEffect(GetCommand(row), ModCommand::PARAM(GetParam(row))), &effectName, true);
	}

	status = ui::Format(UL_("Pat: %d\tChn: %d\tRow: %d\tVal: %02X (%03d) [%s]"),
				m_nPattern, m_nChan+1, static_cast<signed int>(row), paramValue, paramValue, effectName.c_str());
	m_edVisStatus.SetWindowText(status);
}

void CEffectVis::OnLButtonDown(uint32 nFlags, Point point)
{
	if (!(m_dwStatus & FXVSTATUS_RDRAGGING))
	{
		SetFocus();
		SetCapture();

		m_nDragItem = ScreenXToRow(point.x);
		m_dwStatus |= FXVSTATUS_LDRAGGING;
		m_ModDoc.GetPatternUndo().PrepareUndo(static_cast<PATTERNINDEX>(m_nPattern), m_nChan, m_startRow, 1, m_endRow - m_startRow + 1, "Parameter Editor entry");
		OnMouseMove(nFlags, point);
	}

	DialogBase::OnLButtonDown(nFlags, point);
}

void CEffectVis::OnLButtonUp(uint32 nFlags, Point point)
{
	ReleaseCapture();
	m_dwStatus = 0x00;
	DialogBase::OnLButtonUp(nFlags, point);
	m_nLastDrawnRow = ROWINDEX_INVALID;
}


bool CEffectVis::OnInitDialog()
{
	DialogBase::OnInitDialog();

	// If first selected row is a PC event (or some other row but there aren't any other effects), default to PC note overwrite mode
	// and use it as a template for new PC notes that will be created via the visualiser.
	bool isPCevent = IsPcNote(m_startRow);
	ROWINDEX templatePCRow = m_startRow;
	if(!isPCevent)
	{
		for(ROWINDEX row = m_startRow; row <= m_endRow; row++)
		{
			if(IsPcNote(row))
			{
				isPCevent = true;
				templatePCRow = row;
			} else if(GetCommand(row) != CMD_NONE)
			{
				isPCevent = false;
				break;
			}
		}
	}

	if(isPCevent)
	{
		m_nAction = Action::OverwritePC;
		if(m_SndFile.Patterns.IsValidPat(m_nPattern))
		{
			ModCommand &m = *m_SndFile.Patterns[m_nPattern].GetpModCommand(templatePCRow, m_nChan);
			m_templatePCNote.Set(m.note, m.instr, m.GetValueVolCol(), 0);
		}
	} else
	{
		// Otherwise, default to FX overwrite and
		// use effect of first selected row as default effect type
		m_nAction = Action::OverwriteFX;
		m_nFillEffect = m_effectInfo.GetIndexFromEffect(GetCommand(m_startRow), ModCommand::PARAM(GetParam(m_startRow)));
		if(m_nFillEffect < 0 || m_nFillEffect >= MAX_EFFECTS)
			m_nFillEffect = m_effectInfo.GetIndexFromEffect(CMD_SMOOTHMIDI, 0);
	}


	UpdateEffectList();

	m_cmbActionList.ResetContent();
	m_cmbActionList.SetItemData(m_cmbActionList.AddString(UL_("Overwrite with effect:")), static_cast<uintptr_t>(Action::OverwriteFX));
	m_cmbActionList.SetItemData(m_cmbActionList.AddString(UL_("Overwrite effect next to note:")), static_cast<uintptr_t>(Action::OverwriteFXWithNote));
	m_cmbActionList.SetItemData(m_cmbActionList.AddString(UL_("Fill blanks with effect:")), static_cast<uintptr_t>(Action::FillFX));
	if(m_ModDoc.GetModType() == MOD_TYPE_MPT || isPCevent)
	{
		m_cmbActionList.SetItemData(m_cmbActionList.AddString(UL_("Overwrite with PC note")), static_cast<uintptr_t>(Action::OverwritePC));
		m_cmbActionList.SetItemData(m_cmbActionList.AddString(UL_("Fill blanks with PC note")), static_cast<uintptr_t>(Action::FillPC));
	}
	m_cmbActionList.SetItemData(m_cmbActionList.AddString(UL_("Never change effect type")), static_cast<uintptr_t>(Action::Preserve));

	m_cmbActionList.SetCurSel(static_cast<int>(m_nAction));
	return true;
}


void CEffectVis::UpdateEffectList()
{
	const bool fillPlugParams = m_nAction == Action::FillPC || m_nAction == Action::OverwritePC;
	if(!m_cmbEffectList.IsWindow())
	{
		m_cmbEffectList.CreateChild(*this, Rect(0, 0, 16, ui::ScalePixels(16, this)), IDC_VISEFFECTLIST);
		m_cmbEffectList.SetFont(GetFont());
	}
	m_cmbEffectList.ResetContent();
	m_cmbEffectList.SetSorted(!fillPlugParams);
	m_cmbEffectList.SetRedraw(false);
	mpt::ustring s;
	if(fillPlugParams)
	{
		IMixPlugin *plugin = nullptr;
		if(m_templatePCNote.instr > 0 && m_templatePCNote.instr <= MAX_MIXPLUGINS)
			plugin = m_SndFile.m_MixPlugins[m_templatePCNote.instr - 1].pMixPlugin;

		if(plugin)
		{
			AddPluginParameternamesToCombobox(m_cmbEffectList, *plugin);
		} else
		{
			for(PlugParamIndex i = 0; i < ModCommand::maxColumnValue; i++)
			{
				s = ui::Format(UL_("Parameter %u"), i);
				m_cmbEffectList.SetItemData(m_cmbEffectList.AddString(s), i);
			}
		}
		m_cmbEffectList.SetCurSel(m_templatePCNote.GetValueVolCol());
	} else
	{
		const uint32 numfx = m_effectInfo.GetNumEffects();
		for(uint32 i = 0; i < numfx; i++)
		{
			if(m_effectInfo.GetEffectInfo(i, &s, true))
			{
				const int k = m_cmbEffectList.AddString(s);
				m_cmbEffectList.SetItemData(k, i);
			}
		}
	}
	if(!fillPlugParams)
	{
		for(int k = 0; k < m_cmbEffectList.GetCount(); k++)
		{
			if(static_cast<int>(m_cmbEffectList.GetItemData(k)) == m_nFillEffect)
				m_cmbEffectList.SetCurSel(k);
		}
	}
	m_cmbEffectList.SetRedraw(true);
	m_cmbEffectList.Invalidate(false);
}

void CEffectVis::MakeChange(ROWINDEX row, int y)
{
	if(!m_SndFile.Patterns.IsValidPat(m_nPattern))
		return;

	ModCommand &m = *m_SndFile.Patterns[m_nPattern].GetpModCommand(row, m_nChan);

	switch(m_nAction)
	{
		case Action::FillFX:
			// Only set command if there isn't a command already at this row and it's not a PC note
			if (GetCommand(row) == CMD_NONE && !IsPcNote(row))
			{
				SetCommand(row, m_effectInfo.GetEffectFromIndex(m_nFillEffect));
			}
			// Always set param
			SetParamFromY(row, y);
			break;

		case Action::OverwriteFXWithNote:
			if(!m_SndFile.Patterns.IsValidPat(m_nPattern))
				break;
			if(!m_SndFile.Patterns[m_nPattern].GetpModCommand(row, m_nChan)->IsNote())
				break;
			[[fallthrough]];
		case Action::OverwriteFX:
			// Always set command and param. Blows away any PC notes.
			SetCommand(row, m_effectInfo.GetEffectFromIndex(m_nFillEffect));
			SetParamFromY(row, y);
			break;

		case Action::FillPC:
			// Fill only empty slots with PC notes - leave other slots alone.
			if (m.IsEmpty())
			{
				SetPcNote(row);
			}
			// Always set param
			SetParamFromY(row, y);
			break;

		case Action::OverwritePC:
			// Always convert to PC Note and set param value
			SetPcNote(row);
			SetParamFromY(row, y);
			break;

		case Action::Preserve:
			if (GetCommand(row) != CMD_NONE || IsPcNote(row))
			{
				// Only set param if we have an effect type or if this is a PC note.
				// Never change the effect type.
				SetParamFromY(row, y);
			}
			break;

	}

	m_ModDoc.SetModified();
	m_ModDoc.UpdateAllViews(nullptr, RowHint(row), this);
	InvalidateRow(row);  // Only invalidate, actual redrawing is triggered in OnMouseMove as we may modify multiple rows with one mouse event
}

void CEffectVis::SetPcNote(ROWINDEX row)
{
	if(!m_SndFile.Patterns.IsValidPat(m_nPattern))
		return;

	ModCommand &m = *m_SndFile.Patterns[m_nPattern].GetpModCommand(row, m_nChan);
	m.Set(m_templatePCNote.note, m_templatePCNote.instr, m_templatePCNote.GetValueVolCol(), 0);
}

bool CEffectVis::IsPcNote(ROWINDEX row) const
{
	if(m_SndFile.Patterns.IsValidPat(m_nPattern))
		return m_SndFile.Patterns[m_nPattern].GetpModCommand(row, m_nChan)->IsPcNote();
	else
		return false;
}


OPENMPT_NAMESPACE_END
