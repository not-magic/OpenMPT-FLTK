/*
 * PatternEditorDialogs.cpp
 * ------------------------
 * Purpose: Code for various dialogs that are used in the pattern editor.
 * Notes  : (currently none)
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "ui/Ui.h"
#include "PatternEditorDialogs.h"
#include "FileDialog.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "resource.h"
#include "TempoSwingDialog.h"
#include "View_pat.h"
#include "WindowMessages.h"
#include "../common/mptStringBuffer.h"
#include "../soundlib/mod_specifications.h"


OPENMPT_NAMESPACE_BEGIN


static constexpr EffectCommand ExtendedCommands[] = {CMD_OFFSET, CMD_PATTERNBREAK, CMD_POSITIONJUMP, CMD_TEMPO, CMD_FINETUNE, CMD_FINETUNE_SMOOTH};

// For a given pattern cell, check if it contains a command supported by the X-Param mechanism.
// If so, calculate the multipler for this cell and the value of all the other cells belonging to this param.
void getXParam(ModCommand::COMMAND command, PATTERNINDEX nPat, ROWINDEX nRow, CHANNELINDEX nChannel, const CTrackerSoundFile &sndFile, uint32 &xparam, uint32 &multiplier)
{
	uint32 xp = 0, mult = 1;
	int cmdRow = static_cast<int>(nRow);
	const auto &pattern = sndFile.Patterns[nPat];

	if(command == CMD_XPARAM)
	{
		// current command is a parameter extension command
		cmdRow--;

		// Try to find previous command parameter to be extended
		while(cmdRow >= 0)
		{
			const ModCommand &m = *pattern.GetpModCommand(cmdRow, nChannel);
			if(mpt::contains(ExtendedCommands, m.command))
				break;
			if(m.command != CMD_XPARAM)
			{
				cmdRow = -1;
				break;
			}
			cmdRow--;
		}
	} else if(!mpt::contains(ExtendedCommands, command))
	{
		// If current row do not own any satisfying command parameter to extend, set return state
		cmdRow = -1;
	}

	if(cmdRow >= 0)
	{
		// An 'extendable' command parameter has been found
		const ModCommand &m = *pattern.GetpModCommand(cmdRow, nChannel);

		// Find extension resolution (8 to 24 bits)
		uint32 n = 1;
		while(n < 4 && cmdRow + n < pattern.GetNumRows())
		{
			if(pattern.GetpModCommand(cmdRow + n, nChannel)->command != CMD_XPARAM)
				break;
			n++;
		}

		// Parameter extension found (above 8 bits non-standard parameters)
		if(n > 1)
		{
			// Limit offset command to 24 bits, other commands to 16 bits
			n = m.command == CMD_OFFSET ? n : (n > 2 ? 2 : n);

			// Compute extended value WITHOUT current row parameter value : this parameter
			// is being currently edited (this is why this function is being called) so we
			// only need to compute a multiplier so that we can add its contribution while
			// its value is changed by user
			for(uint32 j = 0; j < n; j++)
			{
				const ModCommand &mx = *pattern.GetpModCommand(cmdRow + j, nChannel);

				uint32 k = 8 * (n - j - 1);
				if(cmdRow + j == nRow)
					mult = 1 << k;
				else
					xp += (mx.param << k);
			}
		} else if(m.command == CMD_OFFSET || m.command == CMD_FINETUNE || m.command == CMD_FINETUNE_SMOOTH)
		{
			// Even when no parameter extension is found, extend the offset / finetune parameters to full resolution.
			mult <<= 8;
		}

		const auto modDoc = sndFile.GetpModDoc();
		if(m.command == CMD_OFFSET && m.volcmd == VOLCMD_OFFSET && modDoc != nullptr)
		{
			SAMPLEINDEX smp = modDoc->GetSampleIndex(m);
			if(m.vol == 0 && smp != 0)
			{
				xp = Util::muldivr_unsigned(sndFile.GetSample(smp).nLength, pattern.GetpModCommand(nRow, nChannel)->param  * mult + xp, 256u << (8u * (std::max(uint32(2), n) - 1u)));
				mult = 0;
			} else if(m.vol > 0 && smp != 0)
			{
				xp += sndFile.GetSample(smp).cues[m.vol - 1];
			}
		}
	}

	// Return x-parameter
	multiplier = mult;
	xparam = xp;
}


/////////////////////////////////////////////////////////////////////////////////////////////
// CPatternPropertiesDlg

UI_MESSAGE_MAP_BEGIN(CPatternPropertiesDlg, DialogBase)
	UI_COMMAND(IDC_BUTTON_HALF,   &CPatternPropertiesDlg::OnHalfRowNumber)
	UI_COMMAND(IDC_BUTTON_DOUBLE, &CPatternPropertiesDlg::OnDoubleRowNumber)
	UI_COMMAND(IDC_CHECK1,        &CPatternPropertiesDlg::OnOverrideSignature)
	UI_COMMAND(IDC_BUTTON1,       &CPatternPropertiesDlg::OnTempoSwing)
	UI_COMMAND(IDC_BUTTON2,       &CPatternPropertiesDlg::OnChangeColor)
	UI_COMMAND(IDC_BUTTON3,       &CPatternPropertiesDlg::OnResetColor)
	UI_NOTIFY(ui::EditChange, IDC_EDIT1,       &CPatternPropertiesDlg::OnPatternChanged)
	UI_NOTIFY(ui::SpinDeltaPos, IDC_EDIT1,     &CPatternPropertiesDlg::OnPatternSpinDelta)
UI_MESSAGE_MAP_END()


void CPatternPropertiesDlg::DoDataExchange(DataExchange *pDX)
{
	DialogBase::DoDataExchange(pDX);
	pDX->BindControl(IDC_COMBO1, m_numRows);
	pDX->BindControl(IDC_EDIT1, m_spinPattern);
	pDX->BindControl(IDC_ROWSPERBEAT, m_spinRPB);
	pDX->BindControl(IDC_ROWSPERMEASURE, m_spinRPM);
}

CPatternPropertiesDlg::CPatternPropertiesDlg(CModDoc &modParent, PATTERNINDEX nPat, Wnd *parent)
	: DialogBase{IDD_PATTERN_PROPERTIES, parent}
	, m_modDoc{modParent}
	, m_nPattern{nPat}
{
}


bool CPatternPropertiesDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();

	m_numRows.SetEditNumberOnly(true);

	const CTrackerSoundFile &sndFile = m_modDoc.GetSoundFile();
	const CModSpecifications &specs = sndFile.GetModSpecifications();
	m_numRows.SetRedraw(false);
	for(ROWINDEX irow = specs.patternRowsMin; irow <= specs.patternRowsMax; irow++)
	{
		m_numRows.AddString(mpt::ufmt::dec(irow));
	}
	m_numRows.SetRedraw(true);
	GotoDlgCtrl(&m_numRows);
	m_spinRPB.SetRange32(1, MAX_ROWS_PER_BEAT);
	m_spinRPM.SetRange32(1, MAX_ROWS_PER_MEASURE);
	m_spinPattern.SetRange32(0, std::max(sndFile.Patterns.Size(), PATTERNINDEX(1)) - 1);
	m_spinPattern.SetPos(m_nPattern);
	CheckRadioButton(IDC_RADIO1, IDC_RADIO2, IDC_RADIO2);
	static_cast<Edit *>(GetDlgItem(IDC_EDIT2))->SetLimitText(MAX_PATTERNNAME - 1);

	m_colorBtn.SubclassDlgItem(IDC_BUTTON2, this);

	m_locked = false;
	SetCurrentPattern(m_nPattern);

	return false;
}


void CPatternPropertiesDlg::SetCurrentPattern(PATTERNINDEX pat)
{
	if(m_locked)
		return;
	const CTrackerSoundFile &sndFile = m_modDoc.GetSoundFile();
	const bool validPat = sndFile.Patterns.IsValidPat(pat);

	for(auto id : { IDC_EDIT2, IDC_COMBO1, IDC_BUTTON_HALF, IDC_BUTTON_DOUBLE, IDC_RADIO1, IDC_RADIO2, IDC_CHECK2, IDC_BUTTON1, IDC_ROWSPERBEAT, IDC_ROWSPERMEASURE })
	{
		GetDlgItem(id)->EnableWindow(validPat ? true : false);
	}
	GetDlgItem(IDC_CHECK1)->EnableWindow((sndFile.GetModSpecifications().hasPatternSignatures && validPat) ? true : false);
	if(validPat)
	{
		m_nPattern = pat;
		PatternProperties &prop = GetPatternProperties();
		m_numRows.SetCurSel(prop.numRows - sndFile.GetModSpecifications().patternRowsMin);
		CheckRadioButton(IDC_RADIO1, IDC_RADIO2, prop.resizeAtEnd ? IDC_RADIO2 : IDC_RADIO1);

		// Window title
		const mpt::ustring patternName = mpt::ToUnicode(sndFile.GetCharsetInternal(), prop.name);
		SetDlgItemText(IDC_EDIT2, patternName);
		mpt::ustring s = MPT_UFORMAT("Pattern Properties for Pattern #{}")(m_nPattern);
		if(!patternName.empty())
		{
			s += UL_(" (");
			s += patternName;
			s += UL_(")");
		}
		SetWindowText(s);

		// Pattern time signature
		const bool overrideSignature = prop.rowsPerBeat != 0 || prop.rowsPerMeasure != 0;
		ROWINDEX rpb = prop.rowsPerBeat, rpm = prop.rowsPerMeasure;
		if(rpb == 0 || !overrideSignature)
			rpb = sndFile.m_nDefaultRowsPerBeat;
		if(rpm == 0 || !overrideSignature)
			rpm = sndFile.m_nDefaultRowsPerMeasure;

		CheckDlgButton(IDC_CHECK1, overrideSignature ? ui::CheckOn : ui::CheckOff);
		CheckDlgButton(IDC_CHECK2, prop.repeatContents ? ui::CheckOn : ui::CheckOff);
		SetDlgItemInt(IDC_ROWSPERBEAT, rpb, false);
		SetDlgItemInt(IDC_ROWSPERMEASURE, rpm, false);
		OnOverrideSignature();

		m_colorBtn.SetColor(prop.color);
	} else
	{
		ui::Beep();
	}
}


void CPatternPropertiesDlg::StorePatternProperties()
{
	if(m_locked)
		return;
	auto &prop = GetPatternProperties();
	mpt::ustring str;
	GetDlgItemText(IDC_EDIT2, str);
	prop.name = mpt::ToCharset(m_modDoc.GetSoundFile().GetCharsetInternal(), str);
	prop.numRows = GetDlgItemInt(IDC_COMBO1);
	prop.resizeAtEnd = IsDlgButtonChecked(IDC_RADIO2) != ui::CheckOff;
	prop.repeatContents = IsDlgButtonChecked(IDC_CHECK2) != ui::CheckOff;
	if(IsDlgButtonChecked(IDC_CHECK1) != ui::CheckOff)
	{
		prop.rowsPerBeat = GetDlgItemInt(IDC_ROWSPERBEAT);
		prop.rowsPerMeasure = GetDlgItemInt(IDC_ROWSPERMEASURE);
	} else
	{
		prop.rowsPerBeat = prop.rowsPerMeasure = 0;
	}
}


bool CPatternPropertiesDlg::ValidatePatternProperties()
{
	StorePatternProperties();

	CTrackerSoundFile &sndFile = m_modDoc.GetSoundFile();
	auto &prop = GetPatternProperties();
	// Check for valid time signatures
	if((prop.rowsPerBeat != 0 || prop.rowsPerMeasure != 0) && !CPattern::IsValidSignature(prop.rowsPerBeat, prop.rowsPerMeasure))
	{
		Reporting::Error("Invalid time signature!", "Pattern Properties");
		SetCurrentPattern(m_nPattern);
		SetDlgItemInt(IDC_EDIT1, m_nPattern);
		GotoDlgCtrl(GetDlgItem(IDC_ROWSPERBEAT));
		return false;
	}

	// Check if any pattern data would be removed.
	CPattern &pattern = sndFile.Patterns[m_nPattern];
	const ROWINDEX newSize = prop.numRows;
	const ROWINDEX oldSize = pattern.GetNumRows();
	if(pattern.IsValid() && newSize < oldSize
		&& (prop.resizeWarningShown != newSize || prop.resizeWarningAtEnd != prop.resizeAtEnd))
	{
		ROWINDEX firstRow = prop.resizeAtEnd ? newSize : 0;
		ROWINDEX lastRow = prop.resizeAtEnd ? oldSize : oldSize - newSize;
		for(ROWINDEX row = firstRow; row < lastRow; row++)
		{
			if(!pattern.IsEmptyRow(row))
			{
				bool resize = (Reporting::Confirm(MPT_AFORMAT("Data at the {} of pattern {} will be lost.\nDo you want to continue?")(prop.resizeAtEnd ? "end" : "start", m_nPattern), "Shrink Pattern") == cnfYes);
				if(!resize)
				{
					SetCurrentPattern(m_nPattern);
					SetDlgItemInt(IDC_EDIT1, m_nPattern);
					GotoDlgCtrl(&m_numRows);
					return false;
				}
				break;
			}
		}
		prop.resizeWarningShown = prop.numRows;
		prop.resizeWarningAtEnd = prop.resizeAtEnd;
	}
	return true;
}


void CPatternPropertiesDlg::OnPatternChanged()
{
	auto pat = GetDlgItemInt(IDC_EDIT1);
	if(m_locked || pat == m_nPattern)
		return;
	if(!ValidatePatternProperties())
		return;
	SetCurrentPattern(mpt::saturate_cast<PATTERNINDEX>(pat));
}


// Steps to the next existing pattern instead of the next index
void CPatternPropertiesDlg::OnPatternSpinDelta(NotifyHeader *notify, LResult *result)
{
	*result = 1;
	const int direction = static_cast<const ui::SpinDelta *>(notify->extra)->delta;
	const auto &patterns = m_modDoc.GetSoundFile().Patterns;
	if(!direction || !patterns.IsValidPat(m_nPattern))
		return;
	int pattern = std::clamp(m_spinPattern.GetPos(), 0, patterns.Size() - 1);
	const int startPattern = pattern;
	do
	{
		pattern += direction;
		if(pattern < 0)
			pattern = patterns.Size() - 1;
		else if(pattern >= patterns.Size())
			pattern = 0;
		else if(pattern == startPattern)
			break;  // Couldn't find any other pattern, abort
	} while(!patterns.IsValidPat(static_cast<PATTERNINDEX>(pattern)));
	m_spinPattern.SetPos(pattern);
	OnPatternChanged();
}


CPatternPropertiesDlg::PatternProperties& CPatternPropertiesDlg::GetPatternProperties(PATTERNINDEX pat)
{
	if(auto p = m_properties.find(pat); p != m_properties.end())
		return p->second;

	const CTrackerSoundFile &sndFile = m_modDoc.GetSoundFile();
	PatternProperties prop;
	if(sndFile.Patterns.IsValidPat(pat))
	{
		const CPattern &pattern = sndFile.Patterns[pat];
		prop.name = pattern.GetName();
		if(pattern.HasTempoSwing())
			prop.tempoSwing = pattern.GetTempoSwing();
		else
			prop.tempoSwing = sndFile.m_tempoSwing;
		prop.numRows = pattern.GetNumRows();
		prop.rowsPerBeat = pattern.GetRowsPerBeat();
		prop.rowsPerMeasure = pattern.GetRowsPerMeasure();
		prop.color = pattern.GetColor();
	}
	// Take whatever was selected for the previous pattern to be the default for this newly-edited pattern as well
	prop.resizeAtEnd = IsDlgButtonChecked(IDC_RADIO2) != ui::CheckOff;
	prop.repeatContents = IsDlgButtonChecked(IDC_CHECK2) != ui::CheckOff;
	return m_properties.insert(std::make_pair(pat, std::move(prop))).first->second;
}


void CPatternPropertiesDlg::OnChangeColor()
{
	const auto &patterns = m_modDoc.GetSoundFile().Patterns;
	const PATTERNINDEX numPatterns = patterns.GetNumPatterns();
	std::vector<ColorRef> colors(numPatterns, CPattern::INVALID_COLOR);
	for(PATTERNINDEX pat = 0; pat < numPatterns; pat++)
	{
		if(auto it = m_properties.find(pat); it != m_properties.end())
			colors[pat] = it->second.color;
		else if(patterns.IsValidPat(pat))
			colors[pat] = patterns[pat].GetColor();
	}

	if(auto color = m_colorBtn.PickPatternColor(mpt::as_span(colors), m_nPattern); color.has_value())
		GetPatternProperties().color = *color;
}


void CPatternPropertiesDlg::OnResetColor()
{
	GetPatternProperties().color = CPattern::INVALID_COLOR;
	m_colorBtn.SetColor(CPattern::INVALID_COLOR);
}


void CPatternPropertiesDlg::OnHalfRowNumber()
{
	const CTrackerSoundFile &sndFile = m_modDoc.GetSoundFile();

	uint32 nRows = GetDlgItemInt(IDC_COMBO1, nullptr, false);
	nRows /= 2;
	if(nRows < sndFile.GetModSpecifications().patternRowsMin)
		nRows = sndFile.GetModSpecifications().patternRowsMin;
	SetDlgItemInt(IDC_COMBO1, nRows, false);
}


void CPatternPropertiesDlg::OnDoubleRowNumber()
{
	const CTrackerSoundFile &sndFile = m_modDoc.GetSoundFile();

	uint32 nRows = GetDlgItemInt(IDC_COMBO1, nullptr, false);
	nRows *= 2;
	if(nRows > sndFile.GetModSpecifications().patternRowsMax)
		nRows = sndFile.GetModSpecifications().patternRowsMax;
	SetDlgItemInt(IDC_COMBO1, nRows, false);
}


void CPatternPropertiesDlg::OnOverrideSignature()
{
	const bool enableTimeSignature = IsDlgButtonChecked(IDC_CHECK1);
	GetDlgItem(IDC_ROWSPERBEAT)->EnableWindow(enableTimeSignature);
	GetDlgItem(IDC_ROWSPERMEASURE)->EnableWindow(enableTimeSignature);
	GetDlgItem(IDC_BUTTON1)->EnableWindow(enableTimeSignature && m_modDoc.GetSoundFile().m_nTempoMode == TempoMode::Modern);
}


void CPatternPropertiesDlg::OnTempoSwing()
{
	CPattern &pat = m_modDoc.GetSoundFile().Patterns[m_nPattern];
	const ROWINDEX oldRPB = pat.GetRowsPerBeat();
	const ROWINDEX oldRPM = pat.GetRowsPerMeasure();

	// Temporarily apply new tempo signature for preview
	const ROWINDEX newRPB = std::clamp(static_cast<ROWINDEX>(GetDlgItemInt(IDC_ROWSPERBEAT)), ROWINDEX(1), MAX_ROWS_PER_BEAT);
	const ROWINDEX newRPM = std::clamp(static_cast<ROWINDEX>(GetDlgItemInt(IDC_ROWSPERMEASURE)), newRPB, MAX_ROWS_PER_BEAT);
	pat.SetSignature(newRPB, newRPM);

	TempoSwing &tempoSwing = GetPatternProperties().tempoSwing;
	tempoSwing.resize(newRPB, TempoSwing::Unity);
	CTempoSwingDlg dlg(this, tempoSwing, m_modDoc.GetSoundFile(), m_nPattern);
	if(dlg.DoModal() == IDOK)
	{
		tempoSwing = dlg.m_tempoSwing;
	}
	pat.SetSignature(oldRPB, oldRPM);
}


void CPatternPropertiesDlg::OnOK()
{
	if(!ValidatePatternProperties())
		return;

	CTrackerSoundFile &sndFile = m_modDoc.GetSoundFile();
	bool modified = false;
	for(const auto &[pat, prop] : m_properties)
	{
		PatternHint updateHint = PatternHint{pat};

		CPattern &pattern = sndFile.Patterns[pat];
		// Update pattern signature if necessary
		if(sndFile.GetModSpecifications().hasPatternSignatures)
		{
			if(prop.rowsPerBeat != 0 || prop.rowsPerMeasure != 0)
			{
				// Enable signature
				const ROWINDEX newRPB = prop.rowsPerBeat;
				const ROWINDEX newRPM = prop.rowsPerMeasure;
				TempoSwing tempoSwing = prop.tempoSwing;
				if(newRPB != pattern.GetRowsPerBeat() || newRPM != pattern.GetRowsPerMeasure() || tempoSwing != pattern.GetTempoSwing())
				{
					pattern.SetSignature(newRPB, newRPM);
					tempoSwing.resize(newRPB, TempoSwing::Unity);
					pattern.SetTempoSwing(tempoSwing);
					updateHint.Data();
				}
			} else
			{
				// Disable signature
				if(pattern.GetOverrideSignature() || pattern.HasTempoSwing())
				{
					pattern.RemoveSignature();
					pattern.RemoveTempoSwing();
					updateHint.Data();
				}
			}
		}

		const ROWINDEX newSize = prop.numRows;

		// Check if any pattern data would be removed.
		const ROWINDEX oldSize = pattern.GetNumRows();
		bool resize = newSize != oldSize;
		const bool resizeAtEnd = prop.resizeAtEnd;
		if(resize)
		{
			const bool copyContents = (newSize > oldSize) && prop.repeatContents;
			m_modDoc.BeginWaitCursor();
			m_modDoc.GetPatternUndo().PrepareUndo(m_nPattern, 0, 0, sndFile.Patterns[m_nPattern].GetNumChannels(), oldSize, "Resize");
			if(sndFile.Patterns[m_nPattern].Resize(newSize, true, resizeAtEnd))
			{
				if(copyContents)
				{
					const ROWINDEX copyRows = newSize - oldSize;
					const ROWINDEX sourceBaseRow = resizeAtEnd ? 0 : copyRows;
					const ROWINDEX baseOffset = resizeAtEnd ? 0 : (oldSize - copyRows % oldSize);
					ROWINDEX destRow = resizeAtEnd ? oldSize : 0;
					for(ROWINDEX row = 0; row < copyRows; row++, destRow++)
					{
						ROWINDEX sourceRow = sourceBaseRow + (baseOffset + row) % oldSize;
						const auto sourceRowData = pattern.GetRow(sourceRow);
						auto destRowData = pattern.GetRow(destRow);
						std::copy(sourceRowData.begin(), sourceRowData.end(), destRowData.begin());
					}
				}
				updateHint.Data();
			}
			m_modDoc.EndWaitCursor();
		}
		if(pattern.GetName() != prop.name)
		{
			pattern.SetName(prop.name);
			updateHint.Names();
		}
		if(pattern.GetColor() != prop.color)
		{
			pattern.SetColor(prop.color);
			if(m_modDoc.GetModType() == MOD_TYPE_MPT)
				modified = true;
			m_modDoc.UpdateAllViews(nullptr, SequenceHint{SEQUENCEINDEX_INVALID}.Data());
		}

		if(updateHint.GetType() != HINT_NONE)
		{
			m_modDoc.UpdateAllViews(nullptr, updateHint, this);
			modified = true;
		}
	}

	if(modified)
		m_modDoc.SetModified();

	DialogBase::OnOK();
}


////////////////////////////////////////////////////////////////////////////////////////////
// CEditCommand

UI_MESSAGE_MAP_BEGIN(CEditCommand, DialogBase)

	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO1,	&CEditCommand::OnNoteChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO2,	&CEditCommand::OnNoteChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO3,	&CEditCommand::OnVolCmdChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO4,	&CEditCommand::OnCommandChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO5,	&CEditCommand::OnPlugParamChanged)
UI_MESSAGE_MAP_END()


void CEditCommand::DoDataExchange(DataExchange* pDX)
{
	DialogBase::DoDataExchange(pDX);
	pDX->BindControl(IDC_COMBO1, cbnNote);
	pDX->BindControl(IDC_COMBO2, cbnInstr);
	pDX->BindControl(IDC_COMBO3, cbnVolCmd);
	pDX->BindControl(IDC_COMBO4, cbnCommand);
	pDX->BindControl(IDC_COMBO5, cbnPlugParam);
	pDX->BindControl(IDC_SLIDER1, sldVolParam);
	pDX->BindControl(IDC_SLIDER2, sldParam);
}


CEditCommand::CEditCommand(CTrackerSoundFile &sndFile)
    : sndFile(sndFile), effectInfo(sndFile)
{
	DialogBase::Create(IDD_PATTERN_EDITCOMMAND);
}


bool CEditCommand::PreTranslateMessage(int event)
{
	if(event == FL_KEYBOARD)
	{
		const uint32 key = ui::KeyFromEvent();
		if((key == ui::Key_ESCAPE) || (key == ui::Key_RETURN) || (key == ui::Key_APPS))
		{
			OnClose();
			return true;
		}
	}
	return DialogBase::PreTranslateMessage(event);
}


bool CEditCommand::ShowEditWindow(PATTERNINDEX pat, const PatternCursor &cursor, Wnd *parent)
{
	editPattern = pat;
	const ROWINDEX row = editRow = cursor.GetRow();
	const CHANNELINDEX chn = editChannel = cursor.GetChannel();

	if(!sndFile.Patterns.IsValidPat(pat)
	   || !sndFile.Patterns[pat].IsValidRow(row)
	   || chn >= sndFile.GetNumChannels())
	{
		ShowWindow(false);
		return false;
	}

	m = sndFile.Patterns[pat].GetpModCommand(row, chn);
	modified = false;

	InitAll();

	switch(cursor.GetColumnType())
	{
	case PatternCursor::noteColumn:
		cbnNote.SetFocus();
		break;
	case PatternCursor::instrColumn:
		cbnInstr.SetFocus();
		break;
	case PatternCursor::volumeColumn:
		if(m->IsPcNote())
			cbnPlugParam.SetFocus();
		else
			cbnVolCmd.SetFocus();
		break;
	case PatternCursor::effectColumn:
		if(m->IsPcNote())
			sldParam.SetFocus();
		else
			cbnCommand.SetFocus();
		break;
	case PatternCursor::paramColumn:
		sldParam.SetFocus();
		break;
	case PatternCursor::numColumns:
		MPT_ASSERT_NOTREACHED();
		break;
	}

	// Update Window Title
	SetWindowText(MPT_UFORMAT("Note Properties - Row {}, Channel {}")(row, chn + 1));

	Rect rectParent, rectWnd;
	parent->GetWindowRect(&rectParent);
	GetClientRect(&rectWnd);
	SetWindowPos(CMainFrame::GetMainFrame(),
		rectParent.left + (rectParent.Width() - rectWnd.right) / 2,
		rectParent.top + (rectParent.Height() - rectWnd.bottom) / 2,
		-1, -1, ui::PosNoSize | ui::PosNoActivate);
	ShowWindow(true);
	return true;
}


void CEditCommand::InitNote()
{
	// Note
	cbnNote.SetRedraw(false);
	if(oldSpecs != &sndFile.GetModSpecifications())
	{
		cbnNote.ResetContent();
		cbnNote.SetItemData(cbnNote.AddString(UL_("No Note")), 0);
		AppendNotesToControlEx(cbnNote, sndFile, m->instr);
		oldSpecs = &sndFile.GetModSpecifications();
	}

	if(m->IsNote())
	{
		// Normal note / no note
		const ModCommand::NOTE noteStart = sndFile.GetModSpecifications().noteMin;
		cbnNote.SetCurSel(m->note - (noteStart - 1));
	} else if(m->note == NOTE_NONE)
	{
		cbnNote.SetCurSel(0);
	} else
	{
		// Special notes
		for(int i = cbnNote.GetCount() - 1; i >= 0; --i)
		{
			if(cbnNote.GetItemData(i) == m->note)
			{
				cbnNote.SetCurSel(i);
				break;
			}
		}
	}
	cbnNote.SetRedraw(true);

	// Instrument
	cbnInstr.SetRedraw(false);
	cbnInstr.ResetContent();

	if(m->IsPcNote())
	{
		// control plugin param note
		cbnInstr.Update(PluginComboBox::Config{PluginComboBox::ShowNoPlugin | PluginComboBox::ShowEmptySlots}.CurrentSelection(m->instr ? (m->instr - 1) : PLUGINDEX_INVALID), sndFile);
	} else
	{
		// instrument / sample
		cbnInstr.SetItemData(cbnInstr.AddString(UL_("No Instrument")), 0);
		const uint32 nmax = sndFile.GetNumInstruments() ? sndFile.GetNumInstruments() : sndFile.GetNumSamples();
		for(uint32 i = 1; i <= nmax; i++)
		{
			mpt::ustring s = mpt::ufmt::val(i) + UL_(": ");
			// instrument / sample
			if(sndFile.GetNumInstruments())
			{
				if(sndFile.Instruments[i])
					s += mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.Instruments[i]->name);
			} else
				s += mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.m_szNames[i]);
			cbnInstr.SetItemData(cbnInstr.AddString(s), i);
		}
		cbnInstr.SetRawSelection(m->instr);
	}
	cbnInstr.SetRedraw(true);
}


void CEditCommand::InitVolume()
{
	cbnVolCmd.SetRedraw(false);
	cbnVolCmd.ResetContent();
	if(sndFile.GetType() == MOD_TYPE_MOD || m->IsPcNote())
	{
		cbnVolCmd.EnableWindow(false);
		sldVolParam.EnableWindow(false);
	} else
	{
		// Normal volume column effect
		cbnVolCmd.EnableWindow(true);
		sldVolParam.EnableWindow(true);
		uint32 count = effectInfo.GetNumVolCmds();
		cbnVolCmd.SetItemData(cbnVolCmd.AddString(UL_(" None")), (uintptr_t)-1);
		cbnVolCmd.SetCurSel(0);
		uint32 fxndx = effectInfo.GetIndexFromVolCmd(m->volcmd);
		for(uint32 i = 0; i < count; i++)
		{
			mpt::ustring s;
			if(effectInfo.GetVolCmdInfo(i, &s))
			{
				int k = cbnVolCmd.AddString(s);
				cbnVolCmd.SetItemData(k, i);
				if(i == fxndx)
					cbnVolCmd.SetCurSel(k);
			}
		}
		UpdateVolCmdRange();
	}
	cbnVolCmd.SetRedraw(true);
}


void CEditCommand::InitEffect()
{
	if(m->IsPcNote())
	{
		cbnCommand.ShowWindow(false);
		return;
	}
	cbnCommand.ShowWindow(true);
	xParam = 0;
	xMultiplier = 1;
	getXParam(m->command, editPattern, editRow, editChannel, sndFile, xParam, xMultiplier);

	cbnCommand.SetRedraw(false);
	cbnCommand.ResetContent();
	uint32 numfx = effectInfo.GetNumEffects();
	uint32 fxndx = effectInfo.GetIndexFromEffect(m->command, m->param);
	cbnCommand.SetItemData(cbnCommand.AddString(UL_(" None")), (uintptr_t)-1);
	if(m->command == CMD_NONE)
		cbnCommand.SetCurSel(0);

	mpt::ustring s;
	for(uint32 i = 0; i < numfx; i++)
	{
		if(effectInfo.GetEffectInfo(i, &s, true))
		{
			int k = cbnCommand.AddString(s);
			cbnCommand.SetItemData(k, i);
			if(i == fxndx)
				cbnCommand.SetCurSel(k);
		}
	}
	UpdateEffectRange(false);
	cbnCommand.SetRedraw(true);
	cbnCommand.Invalidate();
}


void CEditCommand::InitPlugParam()
{
	if(!m->IsPcNote())
	{
		cbnPlugParam.ShowWindow(false);
		return;
	}
	cbnPlugParam.ShowWindow(true);

	cbnPlugParam.SetRedraw(false);
	cbnPlugParam.ResetContent();

	if(m->instr > 0 && m->instr <= MAX_MIXPLUGINS)
	{
		AddPluginParameternamesToCombobox(cbnPlugParam, sndFile.m_MixPlugins[m->instr - 1]);
		cbnPlugParam.SetCurSel(m->GetValueVolCol());
	}
	UpdateEffectRange(false);

	cbnPlugParam.SetRedraw(true);
	cbnPlugParam.Invalidate();
}


void CEditCommand::UpdateVolCmdRange()
{
	ModCommand::VOL rangeMin = 0, rangeMax = 0;
	int32 fxndx = effectInfo.GetIndexFromVolCmd(m->volcmd);
	bool ok = effectInfo.GetVolCmdInfo(fxndx, NULL, &rangeMin, &rangeMax);
	if(ok && rangeMax > rangeMin)
	{
		sldVolParam.EnableWindow(true);
		sldVolParam.SetRange(rangeMin, rangeMax);
		Limit(m->vol, rangeMin, rangeMax);
		sldVolParam.SetPos(effectInfo.MapVolumeToPos(m->volcmd, m->vol));
	} else
	{
		// Why does this not update the display at all?
		sldVolParam.SetRange(0, 0);
		sldVolParam.SetPos(0);
		sldVolParam.EnableWindow(false);
	}
	UpdateVolCmdValue();
}


void CEditCommand::UpdateEffectRange(bool set)
{
	uint32 pos;
	bool enable = true;

	if(m->IsPcNote())
	{
		// plugin param control note
		sldParam.SetRange(0, ModCommand::maxColumnValue);
		pos = m->GetValueEffectCol();
	} else
	{
		// process as effect
		ModCommand::PARAM rangeMin = 0, rangeMax = 0;
		int32 fxndx = effectInfo.GetIndexFromEffect(m->command, m->param);
		enable = ((fxndx >= 0) && (effectInfo.GetEffectInfo(fxndx, NULL, false, &rangeMin, &rangeMax)));

		pos = effectInfo.MapValueToPos(fxndx, m->param);
		if(pos > rangeMax)
			pos = rangeMin | (pos & 0x0F);
		Limit(pos, rangeMin, rangeMax);

		sldParam.SetRange(rangeMin, rangeMax);
	}

	if(enable)
	{
		sldParam.EnableWindow(true);
		sldParam.SetPageSize(1);
		sldParam.SetPos(pos);
	} else
	{
		// Why does this not update the display at all?
		sldParam.SetRange(0, 0);
		sldParam.SetPos(0);
		sldParam.EnableWindow(false);
	}
	UpdateEffectValue(set);
}


void CEditCommand::OnNoteChanged()
{
	const bool wasParamControl = m->IsPcNote();
	ModCommand::NOTE newNote = m->note;
	ModCommand::INSTR newInstr = m->instr;

	int n = cbnNote.GetCurSel();
	if(n >= 0)
		newNote = static_cast<ModCommand::NOTE>(cbnNote.GetItemData(n));

	if(wasParamControl)
	{
		if(auto sel = cbnInstr.GetSelection(); sel)
			newInstr = *sel + 1;
		else
			newInstr = 0;
	} else
	{
		if(n = cbnInstr.GetRawSelection(); n >= 0)
			newInstr = static_cast<ModCommand::INSTR>(cbnInstr.GetItemData(n));
	}

	if(m->note != newNote || m->instr != newInstr)
	{
		PrepareUndo("Note Entry");
		CModDoc *modDoc = sndFile.GetpModDoc();
		m->note = newNote;
		m->instr = newInstr;

		modDoc->UpdateAllViews(nullptr, RowHint(editRow), nullptr);

		if(wasParamControl != m->IsPcNote())
		{
			InitAll();
		} else if(!m->IsPcNote()
		          && m->instr <= sndFile.GetNumInstruments()
		          && newInstr <= sndFile.GetNumInstruments()
		          && sndFile.Instruments[m->instr] != nullptr
		          && sndFile.Instruments[newInstr] != nullptr
		          && sndFile.Instruments[newInstr]->pTuning != sndFile.Instruments[m->instr]->pTuning)
		{
			//Checking whether note names should be recreated.
			InitNote();
		} else if(m->IsPcNote())
		{
			// Update parameter list
			InitPlugParam();
		}
	}
}


void CEditCommand::OnVolCmdChanged()
{
	ModCommand::VOLCMD newVolCmd = m->volcmd;
	ModCommand::VOL newVol = m->vol;

	int n = cbnVolCmd.GetCurSel();
	if(n >= 0)
	{
		newVolCmd = effectInfo.GetVolCmdFromIndex(static_cast<uint32>(cbnVolCmd.GetItemData(n)));
	}

	newVol = effectInfo.MapPosToVolume(newVolCmd, sldVolParam.GetPos());

	const bool volCmdChanged = m->volcmd != newVolCmd;
	if(volCmdChanged || m->vol != newVol)
	{
		PrepareUndo("Volume Entry");
		CModDoc *modDoc = sndFile.GetpModDoc();
		m->volcmd = newVolCmd;
		m->vol = newVol;

		modDoc->UpdateAllViews(nullptr, RowHint(editRow), nullptr);

		if(volCmdChanged)
			UpdateVolCmdRange();
		else
			UpdateVolCmdValue();
	}
}


void CEditCommand::OnCommandChanged()
{
	ModCommand::COMMAND newCommand = m->command;
	ModCommand::PARAM newParam = m->param;

	int n = cbnCommand.GetCurSel();
	if(n >= 0)
	{
		int ndx = static_cast<int>(cbnCommand.GetItemData(n));
		newCommand = static_cast<ModCommand::COMMAND>((ndx >= 0) ? effectInfo.GetEffectFromIndex(ndx, newParam) : CMD_NONE);
	}

	if(m->command != newCommand || m->param != newParam)
	{
		PrepareUndo("Effect Entry");

		m->command = newCommand;
		if(newCommand != CMD_NONE)
		{
			m->param = newParam;
		}

		xParam = 0;
		xMultiplier = 1;
		if(newCommand == CMD_XPARAM || mpt::contains(ExtendedCommands, newCommand))
		{
			getXParam(newCommand, editPattern, editRow, editChannel, sndFile, xParam, xMultiplier);
		}

		UpdateEffectRange(true);

		sndFile.GetpModDoc()->UpdateAllViews(nullptr, RowHint(editRow), nullptr);
	}
}


void CEditCommand::OnPlugParamChanged()
{
	uint16 newPlugParam = m->GetValueVolCol();

	int n = cbnPlugParam.GetCurSel();
	if(n >= 0)
	{
		newPlugParam = static_cast<uint16>(cbnPlugParam.GetItemData(n));
	}

	if(m->GetValueVolCol() != newPlugParam)
	{
		PrepareUndo("Effect Entry");
		m->SetValueVolCol(newPlugParam);
		sndFile.GetpModDoc()->UpdateAllViews(nullptr, RowHint(editRow), nullptr);
	}
}


void CEditCommand::UpdateVolCmdValue()
{
	mpt::ustring s;
	if(m->IsPcNote())
	{
		// plugin param control note
		uint16 plugParam = static_cast<uint16>(sldVolParam.GetPos());
		s = ui::Format(UL_("Value: %u"), plugParam);
	} else
	{
		// process as effect
		effectInfo.GetVolCmdParamInfo(*m, &s, TrackerSettings::Instance().patternVolColHex);
	}
	SetDlgItemText(IDC_TEXT2, s);
}


void CEditCommand::UpdateEffectValue(bool set)
{
	mpt::ustring s;

	uint16 newPlugParam = 0;
	ModCommand::PARAM newParam = 0;

	if(m->IsPcNote())
	{
		// plugin param control note
		newPlugParam = static_cast<uint16>(sldParam.GetPos());
		s = ui::Format(UL_("Value: %u"), newPlugParam);
	} else
	{
		// process as effect
		int32 fxndx = effectInfo.GetIndexFromEffect(m->command, m->param);
		if(fxndx >= 0)
		{
			newParam = static_cast<ModCommand::PARAM>(effectInfo.MapPosToValue(fxndx, sldParam.GetPos()));
			effectInfo.GetEffectNameEx(s, *m, newParam * xMultiplier + xParam, editChannel);
		}
	}
	SetDlgItemText(IDC_TEXT1, s);

	if(set)
	{
		if((!m->IsPcNote() && m->param != newParam)
		   || (m->IsPcNote() && m->GetValueVolCol() != newPlugParam))
		{
			PrepareUndo("Effect Entry");
			CModDoc *modDoc = sndFile.GetpModDoc();
			if(m->IsPcNote())
			{
				m->SetValueEffectCol(newPlugParam);
			} else
			{
				m->param = newParam;
			}

			modDoc->UpdateAllViews(nullptr, RowHint(editRow), nullptr);
		}
	}
}


void CEditCommand::PrepareUndo(const char *description)
{
	CModDoc *modDoc = sndFile.GetpModDoc();
	if(!modified)
	{
		// Let's create just one undo step.
		modDoc->GetPatternUndo().PrepareUndo(editPattern, editChannel, editRow, 1, 1, description);
		modified = true;
	}
	modDoc->SetModified();
}


void CEditCommand::OnHScroll(uint32, uint32, Wnd *bar)
{
	if(bar == static_cast<Wnd *>(&sldVolParam))
	{
		OnVolCmdChanged();
	} else if(bar == static_cast<Wnd *>(&sldParam))
	{
		UpdateEffectValue(true);
	}
}


void CEditCommand::OnActivate(bool isActive)
{
	DialogBase::OnActivate(isActive);
	if(!isActive)
		ShowWindow(false);
}


////////////////////////////////////////////////////////////////////////////////////////////
// Chord Editor

UI_MESSAGE_MAP_BEGIN(CChordEditor, ResizableDialog)
	UI_MESSAGE(MSG_MOD_KBDNOTIFY, &CChordEditor::OnKeyboardNotify)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO1, &CChordEditor::OnChordChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO2, &CChordEditor::OnBaseNoteChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO3, &CChordEditor::OnNote1Changed)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO4, &CChordEditor::OnNote2Changed)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO5, &CChordEditor::OnNote3Changed)
UI_MESSAGE_MAP_END()


void CChordEditor::DoDataExchange(DataExchange *pDX)
{
	ResizableDialog::DoDataExchange(pDX);
	pDX->BindControl(IDC_KEYBOARD1, m_Keyboard);
	pDX->BindControl(IDC_COMBO1, m_CbnShortcut);
	pDX->BindControl(IDC_COMBO2, m_CbnBaseNote);
	pDX->BindControl(IDC_COMBO3, m_CbnNote[0]);
	pDX->BindControl(IDC_COMBO4, m_CbnNote[1]);
	pDX->BindControl(IDC_COMBO5, m_CbnNote[2]);
	static_assert(mpt::array_size<decltype(m_CbnNote)>::size == 3);
}


CChordEditor::CChordEditor(Wnd *parent)
    : ResizableDialog(IDD_CHORDEDIT, parent)
{
	m_chords = TrackerSettings::GetChords();
}

bool CChordEditor::OnInitDialog()
{
	ResizableDialog::OnInitDialog();
	m_Keyboard.Init(this, (CHORD_MAX - CHORD_MIN) / 12, true);

	m_CbnShortcut.SetRedraw(false);
	m_CbnBaseNote.SetRedraw(false);
	for(auto &combo : m_CbnNote)
		combo.SetRedraw(false);

	// Shortcut key combo box
	AppendNotesToControl(m_CbnShortcut, NOTE_MIN, NOTE_MIN + static_cast<int>(kcVPEndChords) - static_cast<int>(kcVPStartChords));

	m_CbnShortcut.SetCurSel(0);
	// Base Note combo box
	m_CbnBaseNote.SetItemData(m_CbnBaseNote.AddString(UL_("Relative")), MPTChord::relativeMode);
	AppendNotesToControl(m_CbnBaseNote, NOTE_MIN, NOTE_MIN + 3 * 12 - 1);

	// Chord Note combo boxes
	mpt::ustring s;
	for(int note = CHORD_MIN - 1; note < CHORD_MAX; note++)
	{
		int noteVal = note;
		if(note == CHORD_MIN - 1)
		{
			s = UL_("--");
			noteVal = MPTChord::noNote;
		} else
		{
			s = mpt::ToUnicode(CTrackerSoundFile::GetDefaultNoteName(mpt::wrapping_modulo(note, 12)));
			const int octave = mpt::wrapping_divide(note, 12);
			if(octave > 0)
				s += ui::Format(UL_(" (+%d)"), octave);
			else if(octave < 0)
				s += ui::Format(UL_(" (%d)"), octave);
		}
		for(auto &combo : m_CbnNote)
			combo.SetItemData(combo.AddString(s), noteVal);
	}

	m_CbnShortcut.SetRedraw(true);
	m_CbnBaseNote.SetRedraw(true);
	for(auto &combo : m_CbnNote)
		combo.SetRedraw(true);

	// Update Dialog
	OnChordChanged();
	return true;
}


void CChordEditor::OnDPIChanged()
{
	ResizableDialog::OnDPIChanged();
}


void CChordEditor::OnOK()
{
	TrackerSettings::GetChords() = m_chords;
	ResizableDialog::OnOK();
}


MPTChord &CChordEditor::GetChord()
{
	int chord = m_CbnShortcut.GetCurSel();
	if(chord >= 0)
		chord = static_cast<int>(m_CbnShortcut.GetItemData(chord)) - NOTE_MIN;
	if(chord < 0 || chord >= static_cast<int>(m_chords.size()))
		chord = 0;
	return m_chords[chord];
}


LResult CChordEditor::OnKeyboardNotify(WParam cmd, LParam nKey)
{
	const bool outside = static_cast<int>(nKey) == -1;
	if(cmd == KBDNOTIFY_LBUTTONUP && outside)
	{
		// Stopped dragging ouside of keyboard area
		m_mouseDownKey = m_dragKey = MPTChord::noNote;
		return 0;
	} else if (cmd == KBDNOTIFY_MOUSEMOVE || outside)
	{
		return 0;
	}

	MPTChord &chord = GetChord();
	const MPTChord::NoteType key = static_cast<MPTChord::NoteType>(nKey) + CHORD_MIN;
	bool update = false;

	if(cmd == KBDNOTIFY_LBUTTONDOWN && m_mouseDownKey == MPTChord::noNote)
	{
		// Initial mouse down
		m_mouseDownKey = key;
		m_dragKey = MPTChord::noNote;
		return 0;
	}
	if(cmd == KBDNOTIFY_LBUTTONDOWN && m_dragKey == MPTChord::noNote && key != m_mouseDownKey)
	{
		// Start dragging
		m_dragKey = m_mouseDownKey;
	}

	// Remove dragged note or toggle
	bool noteIsSet = false;
	for(auto &note : chord.notes)
	{
		if((m_dragKey != MPTChord::noNote && note == m_dragKey)
		   || (m_dragKey == MPTChord::noNote && note == m_mouseDownKey))
		{
			note = MPTChord::noNote;
			noteIsSet = update = true;
			break;
		}
	}

	// Move or toggle note
	if(cmd != KBDNOTIFY_LBUTTONUP || m_dragKey != MPTChord::noNote || !noteIsSet)
	{
		for(auto &note : chord.notes)
		{
			if(note == MPTChord::noNote)
			{
				note = key;
				update = true;
				break;
			}
		}
	}

	if(cmd == KBDNOTIFY_LBUTTONUP)
		m_mouseDownKey = m_dragKey = MPTChord::noNote;
	else
		m_dragKey = key;

	if(update)
	{
		std::sort(chord.notes.begin(), chord.notes.end(), [](MPTChord::NoteType left, MPTChord::NoteType right)
		{
			return (left == MPTChord::noNote)  ? false : (left < right);
		});
		OnChordChanged();
	}
	return 0;
}


void CChordEditor::OnChordChanged()
{
	const MPTChord &chord = GetChord();
	if(chord.key != MPTChord::relativeMode)
		m_CbnBaseNote.SetCurSel(chord.key + 1);
	else
		m_CbnBaseNote.SetCurSel(0);
	for(int i = 0; i < MPTChord::notesPerChord - 1; i++)
	{
		int note = chord.notes[i];
		if(note == MPTChord::noNote)
			note = 0;
		else
			note += 1 - CHORD_MIN;
		m_CbnNote[i].SetCurSel(note);
	}
	UpdateKeyboard();
}


void CChordEditor::UpdateKeyboard()
{
	MPTChord &chord = GetChord();
	const int baseNote = (chord.key == MPTChord::relativeMode) ? 0 : (chord.key % 12);
	for(int i = CHORD_MIN; i < CHORD_MAX; i++)
	{
		FlagSet<CKeyboardControl::KeyFlag> b = CKeyboardControl::KeyFlag::Normal;
		for(const auto note : chord.notes)
		{
			if(i == note)
				b.set(CKeyboardControl::KeyFlag::RedDot);
		}
		if(i == baseNote)
			b.set(CKeyboardControl::KeyFlag::BrightDot);
		m_Keyboard.SetFlags(i - CHORD_MIN, b);
	}
	m_Keyboard.InvalidateRect(nullptr, false);
}


void CChordEditor::OnBaseNoteChanged()
{
	MPTChord &chord = GetChord();
	int basenote = static_cast<int>(m_CbnBaseNote.GetItemData(m_CbnBaseNote.GetCurSel()));
	if(basenote != MPTChord::relativeMode)
		basenote -= NOTE_MIN;
	chord.key = (uint8)basenote;
	UpdateKeyboard();
}


void CChordEditor::OnNoteChanged(int noteIndex)
{
	MPTChord &chord = GetChord();
	int note = m_CbnNote[noteIndex].GetCurSel();
	if(note < 0)
		return;
	chord.notes[noteIndex] = static_cast<int8>(m_CbnNote[noteIndex].GetItemData(note));
	UpdateKeyboard();
}


////////////////////////////////////////////////////////////////////////////////////////////
// Keyboard Split Settings (pattern editor)

UI_MESSAGE_MAP_BEGIN(CSplitKeyboardSettings, DialogBase)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO_OCTAVEMODIFIER, &CSplitKeyboardSettings::OnOctaveModifierChanged)
UI_MESSAGE_MAP_END()


void CSplitKeyboardSettings::DoDataExchange(DataExchange *pDX)
{
	DialogBase::DoDataExchange(pDX);
	pDX->BindControl(IDC_COMBO_SPLITINSTRUMENT, m_CbnSplitInstrument);
	pDX->BindControl(IDC_COMBO_SPLITNOTE, m_CbnSplitNote);
	pDX->BindControl(IDC_COMBO_OCTAVEMODIFIER, m_CbnOctaveModifier);
	pDX->BindControl(IDC_COMBO_SPLITVOLUME, m_CbnSplitVolume);
}


CSplitKeyboardSettings::CSplitKeyboardSettings(Wnd *parent, CTrackerSoundFile &sf, SplitKeyboardSettings &settings)
	: DialogBase{IDD_KEYBOARD_SPLIT, parent}
	, sndFile{sf}
	, m_Settings{settings}
{
}


bool CSplitKeyboardSettings::OnInitDialog()
{
	if(sndFile.GetpModDoc() == nullptr)
		return true;

	DialogBase::OnInitDialog();

	mpt::ustring s;

	// Split Notes
	AppendNotesToControl(m_CbnSplitNote, sndFile.GetModSpecifications().noteMin, sndFile.GetModSpecifications().noteMax);
	m_CbnSplitNote.SetCurSel(m_Settings.splitNote - (sndFile.GetModSpecifications().noteMin - NOTE_MIN));

	// Octave modifier
	m_CbnOctaveModifier.SetRedraw(false);
	int numItems = SplitKeyboardSettings::splitOctaveRange * 2 + 1;
	m_CbnSplitVolume.InitStorage(numItems, static_cast<uint32>(numItems * 9 * sizeof(mpt::uchar)));
	for(int i = -SplitKeyboardSettings::splitOctaveRange; i < SplitKeyboardSettings::splitOctaveRange + 1; i++)
	{
		s = ui::Format(i < 0 ? UL_("Octave -%d") : i > 0 ? UL_("Octave +%d") : UL_("No Change"), std::abs(i));
		int n = m_CbnOctaveModifier.AddString(s);
		m_CbnOctaveModifier.SetItemData(n, i);
	}
	m_CbnOctaveModifier.SetRedraw(true);
	m_CbnOctaveModifier.SetCurSel(m_Settings.octaveModifier + SplitKeyboardSettings::splitOctaveRange);
	CheckDlgButton(IDC_PATTERN_OCTAVELINK, (m_Settings.octaveLink && m_Settings.octaveModifier != 0) ? ui::CheckOn : ui::CheckOff);

	// Volume
	m_CbnSplitVolume.SetRedraw(false);
	m_CbnSplitVolume.InitStorage(65, 65 * 4 * sizeof(mpt::uchar));
	m_CbnSplitVolume.AddString(UL_("No Change"));
	m_CbnSplitVolume.SetItemData(0, 0);
	for(int i = 1; i <= 64; i++)
	{
		s = ui::Format(UL_("%d"), i);
		int n = m_CbnSplitVolume.AddString(s);
		m_CbnSplitVolume.SetItemData(n, i);
	}
	m_CbnSplitVolume.SetRedraw(true);
	m_CbnSplitVolume.SetCurSel(m_Settings.splitVolume);

	// Instruments
	m_CbnSplitInstrument.SetRedraw(false);
	numItems = 1 + (sndFile.GetNumInstruments() ? sndFile.GetNumInstruments() : sndFile.GetNumSamples());
	m_CbnSplitInstrument.InitStorage(numItems, static_cast<uint32>(numItems * 16 * sizeof(mpt::uchar)));
	m_CbnSplitInstrument.SetItemData(m_CbnSplitInstrument.AddString(UL_("No Change")), 0);

	if(sndFile.GetNumInstruments())
	{
		for(INSTRUMENTINDEX nIns = 1; nIns <= sndFile.GetNumInstruments(); nIns++)
		{
			if(sndFile.Instruments[nIns] == nullptr)
				continue;

			mpt::ustring displayName = sndFile.GetpModDoc()->GetPatternViewInstrumentName(nIns);
			int n = m_CbnSplitInstrument.AddString(displayName);
			m_CbnSplitInstrument.SetItemData(n, nIns);
		}
	} else
	{
		for(SAMPLEINDEX nSmp = 1; nSmp <= sndFile.GetNumSamples(); nSmp++)
		{
			if(sndFile.GetSample(nSmp).HasSampleData())
			{
				s = ui::Format(UL_("%02d: "), nSmp);
				s += mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.m_szNames[nSmp]);
				int n = m_CbnSplitInstrument.AddString(s);
				m_CbnSplitInstrument.SetItemData(n, nSmp);
			}
		}
	}
	m_CbnSplitInstrument.SetRedraw(true);
	m_CbnSplitInstrument.SetCurSel(m_Settings.splitInstrument);

	return true;
}


void CSplitKeyboardSettings::OnOK()
{
	DialogBase::OnOK();

	m_Settings.splitNote = static_cast<ModCommand::NOTE>(m_CbnSplitNote.GetItemData(m_CbnSplitNote.GetCurSel()) - 1);
	m_Settings.octaveModifier = m_CbnOctaveModifier.GetCurSel() - SplitKeyboardSettings::splitOctaveRange;
	m_Settings.octaveLink = (IsDlgButtonChecked(IDC_PATTERN_OCTAVELINK) != ui::CheckOff);
	m_Settings.splitVolume = static_cast<ModCommand::VOL>(m_CbnSplitVolume.GetCurSel());
	m_Settings.splitInstrument = static_cast<ModCommand::INSTR>(m_CbnSplitInstrument.GetItemData(m_CbnSplitInstrument.GetCurSel()));
}


void CSplitKeyboardSettings::OnCancel()
{
	DialogBase::OnCancel();
}


void CSplitKeyboardSettings::OnOctaveModifierChanged()
{
	CheckDlgButton(IDC_PATTERN_OCTAVELINK, (m_CbnOctaveModifier.GetCurSel() != 9) ? ui::CheckOn : ui::CheckOff);
}


/////////////////////////////////////////////////////////////////////////
// Show channel properties from pattern editor

UI_MESSAGE_MAP_BEGIN(QuickChannelProperties, DialogBase)
	UI_NOTIFY(ui::EditUpdate, IDC_EDIT1,	&QuickChannelProperties::OnVolChanged)
	UI_NOTIFY(ui::EditUpdate, IDC_EDIT2,	&QuickChannelProperties::OnPanChanged)
	UI_NOTIFY(ui::EditUpdate, IDC_EDIT3,	&QuickChannelProperties::OnNameChanged)
	UI_COMMAND(IDC_CHECK1,	&QuickChannelProperties::OnMuteChanged)
	UI_COMMAND(IDC_CHECK2,	&QuickChannelProperties::OnSurroundChanged)
	UI_COMMAND(IDC_BUTTON1,	&QuickChannelProperties::OnPrevChannel)
	UI_COMMAND(IDC_BUTTON2,	&QuickChannelProperties::OnNextChannel)
	UI_COMMAND(IDC_BUTTON3,	&QuickChannelProperties::OnChangeColor)
	UI_COMMAND(IDC_BUTTON4,	&QuickChannelProperties::OnChangeColor)
	UI_COMMAND(IDC_BUTTON5, &QuickChannelProperties::OnPickPrevColor)
	UI_COMMAND(IDC_BUTTON6, &QuickChannelProperties::OnPickNextColor)
	UI_MESSAGE(MSG_MOD_KEYCOMMAND,	&QuickChannelProperties::OnCustomKeyMsg)
UI_MESSAGE_MAP_END()


void QuickChannelProperties::DoDataExchange(DataExchange *pDX)
{
	pDX->BindControl(IDC_SLIDER1, m_volSlider);
	pDX->BindControl(IDC_SLIDER2, m_panSlider);
	pDX->BindControl(IDC_EDIT1, m_volSpin);
	pDX->BindControl(IDC_EDIT2, m_panSpin);
	pDX->BindControl(IDC_EDIT3, m_nameEdit);
}


QuickChannelProperties::~QuickChannelProperties()
{
	DestroyWindow();
}


void QuickChannelProperties::OnActivate(bool isActive)
{
	if(!isActive && !m_settingColor)
	{
		// Hide window when changing focus to another window.
		m_visible = false;
		ShowWindow(false);
	}
}


// Show channel properties for a given channel at a given screen position.
void QuickChannelProperties::Show(CModDoc *modDoc, CHANNELINDEX chn, Point position)
{
	if(!IsWindow())
	{
		Create(IDD_CHANNELSETTINGS, nullptr);
		m_colorBtn.SubclassDlgItem(IDC_BUTTON4, this);
		m_colorBtnPrev.SubclassDlgItem(IDC_BUTTON5, this);
		m_colorBtnNext.SubclassDlgItem(IDC_BUTTON6, this);

		m_volSlider.SetRange(0, 64);
		m_volSlider.SetTicFreq(8);
		m_volSpin.SetRange(0, 64);

		m_panSlider.SetRange(0, 64);
		m_panSlider.SetTicFreq(8);
		m_panSpin.SetRange(0, 256);
	}
	m_document = modDoc;
	m_channel = chn;

	// Center window around point where user clicked.
	Rect rect = GetWindowRect();
	const Size screenSize = ui::GetScreenSize();
	rect.MoveToXY(
	    Clamp(static_cast<int>(position.x) - rect.Width() / 2, 0, static_cast<int>(screenSize.cx) - rect.Width()),
	    Clamp(static_cast<int>(position.y) - rect.Height() / 2, 0, static_cast<int>(screenSize.cy) - rect.Height()));
	if(Fl_Window *frame = GetFrameWindow())
		frame->position(rect.left, rect.top);

	UpdateDisplay();

	const bool enablePan = (m_document->GetModType() & (MOD_TYPE_XM | MOD_TYPE_MOD)) ? false : true;
	const bool itOnly = (m_document->GetModType() & (MOD_TYPE_IT | MOD_TYPE_MPT)) ? true : false;

	// Volume controls
	m_volSlider.EnableWindow(itOnly);
	m_volSpin.EnableWindow(itOnly);
	GetDlgItem(IDC_EDIT1)->EnableWindow(itOnly);

	// Pan controls
	m_panSlider.EnableWindow(enablePan);
	m_panSpin.EnableWindow(enablePan);
	GetDlgItem(IDC_EDIT2)->EnableWindow(enablePan);
	GetDlgItem(IDC_CHECK2)->EnableWindow(itOnly);

	// Channel name
	m_nameEdit.EnableWindow((m_document->GetModType() & (MOD_TYPE_IT | MOD_TYPE_MPT | MOD_TYPE_XM)) ? true : false);

	SetFocusToFirstControl();
	ShowWindow(true);
	m_visible = true;
}


void QuickChannelProperties::UpdateDisplay()
{
	const Wnd *oldFocusWnd = GetFocus();
	SetWindowText(MPT_UFORMAT("Settings for Channel {}")(m_channel + 1).c_str());

	// Set up channel properties
	m_visible = false;
	const ModChannelSettings &settings = m_document->GetSoundFile().ChnSettings[m_channel];
	SetDlgItemInt(IDC_EDIT1, settings.nVolume, false);
	SetDlgItemInt(IDC_EDIT2, settings.nPan, false);
	m_volSlider.SetPos(settings.nVolume);
	m_panSlider.SetPos(settings.nPan / 4u);
	CheckDlgButton(IDC_CHECK1, (settings.dwFlags[CHN_MUTE]) ? true : false);
	CheckDlgButton(IDC_CHECK2, (settings.dwFlags[CHN_SURROUND]) ? true : false);

	mpt::uchar description[16];
	wsprintf(description, UL_("Channel %d:"), m_channel + 1);
	SetDlgItemText(IDC_STATIC_CHANNEL_NAME, description);
	m_nameEdit.LimitText(MAX_CHANNELNAME - 1);
	m_nameEdit.SetWindowText(mpt::ToUnicode(m_document->GetSoundFile().GetCharsetInternal(), settings.szName));

	const bool isFirst = (m_channel <= 0), isLast = (m_channel >= m_document->GetNumChannels() - 1);

	m_colorBtn.SetColor(m_document->GetSoundFile().GetChannelColor(m_channel));
	m_colorBtnPrev.EnableWindow(isFirst ? false : true);
	if(!isFirst)
		m_colorBtnPrev.SetColor(m_document->GetSoundFile().GetChannelColor(m_channel - 1));
	m_colorBtnNext.EnableWindow(isLast ? false : true);
	if(!isLast)
		m_colorBtnNext.SetColor(m_document->GetSoundFile().GetChannelColor(m_channel + 1));

	m_settingsChanged = false;
	m_visible = true;

	GetDlgItem(IDC_BUTTON1)->EnableWindow(isFirst ? false : true);
	GetDlgItem(IDC_BUTTON2)->EnableWindow(isLast ? false : true);

	// Avoid focus trap if we just navigated to the first or last channel
	const int PrevNextButtons[] = {IDC_BUTTON1, IDC_BUTTON2, IDC_BUTTON5, IDC_BUTTON6};
	for(int button : PrevNextButtons)
	{
		const Wnd *wnd = GetDlgItem(button);
		if (oldFocusWnd == wnd && !wnd->IsWindowEnabled())
		{
			SetFocusToFirstControl();
			break;
		}
	}
}

void QuickChannelProperties::PrepareUndo()
{
	if(!m_settingsChanged)
	{
		// Backup old channel settings through pattern undo.
		m_settingsChanged = true;
		m_document->GetPatternUndo().PrepareChannelUndo(m_channel, 1, "Channel Settings");
	}
}


void QuickChannelProperties::OnVolChanged()
{
	if(!m_visible)
	{
		return;
	}

	uint16 volume = static_cast<uint16>(GetDlgItemInt(IDC_EDIT1));
	if(volume >= 0 && volume <= 64)
	{
		PrepareUndo();
		m_document->SetChannelGlobalVolume(m_channel, volume);
		m_volSlider.SetPos(volume);
		m_document->UpdateAllViews(nullptr, GeneralHint(m_channel).Channels(), this);
	}
}


void QuickChannelProperties::OnPanChanged()
{
	if(!m_visible)
	{
		return;
	}

	uint16 panning = static_cast<uint16>(GetDlgItemInt(IDC_EDIT2));
	if(panning >= 0 && panning <= 256)
	{
		PrepareUndo();
		m_document->SetChannelDefaultPan(m_channel, panning);
		m_panSlider.SetPos(panning / 4u);
		m_document->UpdateAllViews(nullptr, GeneralHint(m_channel).Channels(), this);
		// Surround is forced off when changing pan, so uncheck the checkbox.
		CheckDlgButton(IDC_CHECK2, ui::CheckOff);
	}
}


void QuickChannelProperties::OnHScroll(uint32, uint32, Wnd *bar)
{
	if(!m_visible)
	{
		return;
	}

	bool update = false;

	// Volume slider
	if(bar == static_cast<Wnd *>(&m_volSlider))
	{
		uint16 pos = static_cast<uint16>(m_volSlider.GetPos());
		PrepareUndo();
		if(m_document->SetChannelGlobalVolume(m_channel, pos))
		{
			SetDlgItemInt(IDC_EDIT1, pos);
			update = true;
		}
	}
	// Pan slider
	if(bar == static_cast<Wnd *>(&m_panSlider))
	{
		uint16 pos = static_cast<uint16>(m_panSlider.GetPos());
		PrepareUndo();
		if(m_document->SetChannelDefaultPan(m_channel, pos * 4u))
		{
			SetDlgItemInt(IDC_EDIT2, pos * 4u);
			CheckDlgButton(IDC_CHECK2, ui::CheckOff);
			update = true;
		}
	}

	if(update)
	{
		m_document->UpdateAllViews(nullptr, GeneralHint(m_channel).Channels(), this);
	}
}


void QuickChannelProperties::OnMuteChanged()
{
	if(!m_visible)
	{
		return;
	}

	m_document->MuteChannel(m_channel, IsDlgButtonChecked(IDC_CHECK1) != ui::CheckOff);
	m_document->UpdateAllViews(nullptr, GeneralHint(m_channel).Channels(), this);
}


void QuickChannelProperties::OnSurroundChanged()
{
	if(!m_visible)
	{
		return;
	}

	PrepareUndo();
	m_document->SurroundChannel(m_channel, IsDlgButtonChecked(IDC_CHECK2) != ui::CheckOff);
	m_document->UpdateAllViews(nullptr, GeneralHint(m_channel).Channels(), this);
	UpdateDisplay();
}


void QuickChannelProperties::OnNameChanged()
{
	if(!m_visible)
	{
		return;
	}

	ModChannelSettings &settings = m_document->GetSoundFile().ChnSettings[m_channel];
	mpt::ustring newNameTmp;
	m_nameEdit.GetWindowText(newNameTmp);
	std::string newName = mpt::ToCharset(m_document->GetSoundFile().GetCharsetInternal(), newNameTmp);

	if(newName != settings.szName)
	{
		PrepareUndo();
		settings.szName = newName;
		m_document->SetModified();
		m_document->UpdateAllViews(nullptr, GeneralHint(m_channel).Channels(), this);
	}
}


void QuickChannelProperties::OnChangeColor()
{
	m_settingColor = true;
	if(auto color = m_colorBtn.PickChannelColor(m_document->GetSoundFile(), m_channel); color.has_value())
	{
		PrepareUndo();
		m_document->GetSoundFile().SetChannelColor(m_channel, *color);
		if(m_document->SupportsChannelColors())
			m_document->SetModified();
		m_document->UpdateAllViews(nullptr, GeneralHint(m_channel).Channels(), this);
	}
	m_settingColor = false;
}


void QuickChannelProperties::OnPickPrevColor()
{
	if(m_channel > 0)
		PickColorFromChannel(m_channel - 1);
}


void QuickChannelProperties::OnPickNextColor()
{
	if(m_channel < m_document->GetNumChannels() - 1)
		PickColorFromChannel(m_channel + 1);
}


void QuickChannelProperties::PickColorFromChannel(CHANNELINDEX channel)
{
	auto &sndFile = m_document->GetSoundFile();
	const uint32 color = sndFile.GetChannelColor(channel);
	if(color != sndFile.GetChannelColor(m_channel))
	{
		PrepareUndo();
		sndFile.SetChannelColor(m_channel, color);
		m_colorBtn.SetColor(color);
		if(m_document->SupportsChannelColors())
			m_document->SetModified();
		m_document->UpdateAllViews(nullptr, GeneralHint(m_channel).Channels(), this);
	}
}


void QuickChannelProperties::OnPrevChannel()
{
	if(m_channel > 0)
	{
		m_channel--;
		UpdateDisplay();
	}
}


void QuickChannelProperties::OnNextChannel()
{
	if(m_channel < m_document->GetNumChannels() - 1)
	{
		m_channel++;
		UpdateDisplay();
	}
}


bool QuickChannelProperties::PreTranslateMessage(int event)
{
	// We handle keypresses before the toolkit has a chance to handle them (for alt etc..)
	if(CMainFrame::GetInputHandler()->HandleKeyEvent(event, kCtxChannelSettings, this))
		return true;  // Mapped to a command, no need to pass the event on.

	return DialogBase::PreTranslateMessage(event);
}


LResult QuickChannelProperties::OnCustomKeyMsg(WParam wParam, LParam)
{
	switch(wParam)
	{
	case kcChnSettingsPrev:
		OnPrevChannel();
		return wParam;
	case kcChnSettingsNext:
		OnNextChannel();
		return wParam;
	case kcChnColorFromPrev:
		OnPickPrevColor();
		return wParam;
	case kcChnColorFromNext:
		OnPickNextColor();
		return wParam;
	case kcChnSettingsClose:
		OnActivate(false);
		return wParam;
	}

	return kcNull;
}


mpt::ustring QuickChannelProperties::GetToolTipText(uint32 id, WindowHandle) const
{
	mpt::ustring text;
	CommandID cmd = kcNull;
	switch(id)
	{
	case IDC_EDIT1:
	case IDC_SLIDER1:
		text = CModDoc::LinearToDecibelsString(m_document->GetSoundFile().ChnSettings[m_channel].nVolume, 64.0);
		break;
	case IDC_EDIT2:
	case IDC_SLIDER2:
		text = CModDoc::PanningToString(m_document->GetSoundFile().ChnSettings[m_channel].nPan, 128);
		break;
	case IDC_BUTTON1:
		text = UL_("Previous Channel");
		cmd = kcChnSettingsPrev;
		break;
	case IDC_BUTTON2:
		text = UL_("Next Channel");
		cmd = kcChnSettingsNext;
		break;
	case IDC_BUTTON5:
		text = UL_("Take color from previous channel");
		cmd = kcChnColorFromPrev;
		break;
	case IDC_BUTTON6:
		text = UL_("Take color from next channel");
		cmd = kcChnColorFromNext;
		break;
	}

	if(cmd != kcNull)
	{
		auto keyText = CMainFrame::GetInputHandler()->m_activeCommandSet->GetKeyTextFromCommand(cmd, 0);
		if(!keyText.empty())
			text += MPT_UFORMAT(" ({})")(keyText);
	}

	return text;
}


/////////////////////////////////////////////////////////////////////////
// Metronome settings

static constexpr float METRONOME_VOLUME_SCALE = 0.2f;

UI_MESSAGE_MAP_BEGIN(MetronomeSettingsDlg, DialogBase)

	UI_COMMAND(IDC_CHECK1,       &MetronomeSettingsDlg::OnToggleMetronome)
	UI_COMMAND(IDC_BUTTON1,      &MetronomeSettingsDlg::OnBrowseMeasure)
	UI_COMMAND(IDC_BUTTON2,      &MetronomeSettingsDlg::OnBrowseBeat)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO1, &MetronomeSettingsDlg::OnSampleChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO2, &MetronomeSettingsDlg::OnSampleChanged)
	UI_NOTIFY(ui::EditKillFocus, IDC_EDIT1,   &MetronomeSettingsDlg::OnSampleChanged)
	UI_NOTIFY(ui::EditKillFocus, IDC_EDIT2,   &MetronomeSettingsDlg::OnSampleChanged)
UI_MESSAGE_MAP_END()


void MetronomeSettingsDlg::DoDataExchange(DataExchange* pDX)
{
	pDX->BindControl(IDC_SLIDER1, m_volumeSlider);
	pDX->BindControl(IDC_COMBO1, m_measureCombo);
	pDX->BindControl(IDC_COMBO2, m_beatCombo);
	pDX->BindControl(IDC_EDIT1, m_measureEdit);
	pDX->BindControl(IDC_EDIT2, m_beatEdit);
	pDX->BindControl(IDC_BUTTON1, m_measureButton);
	pDX->BindControl(IDC_BUTTON2, m_beatButton);
}


MetronomeSettingsDlg::MetronomeSettingsDlg(Wnd *parent)
	: DialogBase{IDD_METRONOME_SETTINGS, parent}
{ }


void MetronomeSettingsDlg::SetSampleInfo(const mpt::PathString &path, ComboBox &combo, Edit &edit, Button &browseButton)
{
	bool enable = false;
	if(path.empty())
	{
		combo.SetCurSel(0);
	} else if(path == TrackerSettings::GetDefaultMetronomeSample())
	{
		combo.SetCurSel(1);
	} else
	{
		combo.SetCurSel(2);
		edit.SetWindowText(path.ToUnicode());
		enable = true;
	}
	edit.EnableWindow(enable);
	browseButton.EnableWindow(enable);
}


bool MetronomeSettingsDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();
	CheckDlgButton(IDC_CHECK1, TrackerSettings::Instance().metronomeEnabled ? ui::CheckOn : ui::CheckOff);
	m_volumeSlider.SetRange(static_cast<int>(-48 / METRONOME_VOLUME_SCALE), 0, true);
	m_volumeSlider.SetPos(mpt::saturate_round<int>(TrackerSettings::Instance().metronomeVolume / METRONOME_VOLUME_SCALE));
	m_volumeSlider.SetTicFreq(static_cast<int>(3 / METRONOME_VOLUME_SCALE));
	static const mpt::uchar *Options[] = {UL_("Off"), UL_("Default (Sine)"), UL_("Custom Sample")};
	for(const mpt::uchar *option : Options)
	{
		m_measureCombo.AddString(option);
		m_beatCombo.AddString(option);
	}
	SetSampleInfo(TrackerSettings::Instance().metronomeSampleMeasure, m_measureCombo, m_measureEdit, m_measureButton);
	SetSampleInfo(TrackerSettings::Instance().metronomeSampleBeat, m_beatCombo, m_beatEdit, m_beatButton);
	GetDlgItem(IDC_VOLUME)->SetWindowText(GetVolumeString());
	return true;
}


mpt::ustring MetronomeSettingsDlg::GetVolumeString() const
{
	mpt::ustring s = (m_volumeSlider.GetPos() >= 0) ? UL_("+") : UL_("");
	s += ui::Format(UL_("%.2f dB"), m_volumeSlider.GetPos() * METRONOME_VOLUME_SCALE);
	return s;
}


void MetronomeSettingsDlg::OnHScroll(uint32, uint32, Wnd *bar)
{
	if(bar == static_cast<Wnd *>(&m_volumeSlider))
	{
		TrackerSettings::Instance().metronomeVolume = m_volumeSlider.GetPos() * METRONOME_VOLUME_SCALE;
		CMainFrame::GetMainFrame()->UpdateMetronomeVolume();
		GetDlgItem(IDC_VOLUME)->SetWindowText(GetVolumeString());
	}
}


void MetronomeSettingsDlg::OnToggleMetronome()
{
	TrackerSettings::Instance().metronomeEnabled = IsDlgButtonChecked(IDC_CHECK1) != ui::CheckOff;
	CMainFrame::GetMainFrame()->UpdateMetronomeSamples();
}


bool MetronomeSettingsDlg::GetSampleInfo(Setting<mpt::PathString> &path, ComboBox &combo, Edit &edit, Button &browseButton)
{
	mpt::ustring s;
	bool modified = false;
	bool enable = false;
	switch(combo.GetCurSel())
	{
	case 0:
		if(path != mpt::PathString{})
		{
			modified = true;
			path = {};
		}
		break;
	case 1:
		if(path != TrackerSettings::GetDefaultMetronomeSample())
		{
			modified = true;
			path = TrackerSettings::GetDefaultMetronomeSample();
		}
		break;
	case 2:
		edit.GetWindowText(s);
		if(auto newPath = mpt::PathString::FromUnicode(s); path != newPath)
		{
			path = newPath;
			modified = true;
		}
		enable = true;
		break;
	}
	edit.EnableWindow(enable);
	browseButton.EnableWindow(enable);
	return modified;
}


void MetronomeSettingsDlg::OnSampleChanged()
{
	bool modified = GetSampleInfo(TrackerSettings::Instance().metronomeSampleMeasure, m_measureCombo, m_measureEdit, m_measureButton);
	modified |= GetSampleInfo(TrackerSettings::Instance().metronomeSampleBeat, m_beatCombo, m_beatEdit, m_beatButton);
	if(modified)
		CMainFrame::GetMainFrame()->LoadMetronomeSamples();
}


mpt::PathString MetronomeSettingsDlg::BrowseForSample(const mpt::PathString &path)
{
	static int lastIndex = 0;
	FileDialog dlg = OpenFileDialog()
		.EnableAudioPreview()
		.ExtensionFilter(ConstructSampleFormatFileFilter(false))
		.WorkingDirectory(path.empty() ? TrackerSettings::Instance().PathSamples.GetWorkingDir() : path.GetDirectoryWithDrive())
		.DefaultFilename(path.GetFilename())
		.FilterIndex(&lastIndex);
	if(!dlg.Show(this))
		return {};
	TrackerSettings::Instance().PathSamples.SetWorkingDir(dlg.GetWorkingDirectory());
	return dlg.GetFirstFile();
}


void MetronomeSettingsDlg::OnBrowseMeasure()
{
	auto newPath = BrowseForSample(TrackerSettings::Instance().metronomeSampleMeasure);
	if(newPath.empty())
		return;
	m_measureEdit.SetWindowText(newPath.ToUnicode());
	OnSampleChanged();
}


void MetronomeSettingsDlg::OnBrowseBeat()
{
	auto newPath = BrowseForSample(TrackerSettings::Instance().metronomeSampleBeat);
	if(newPath.empty())
		return;
	m_beatEdit.SetWindowText(newPath.ToUnicode());
	OnSampleChanged();
}


mpt::ustring MetronomeSettingsDlg::GetToolTipText(uint32 id, WindowHandle) const
{
	mpt::ustring s;
	switch(id)
	{
	case IDC_SLIDER1:
		s = GetVolumeString();
		break;
	}

	return s;
}


OPENMPT_NAMESPACE_END
