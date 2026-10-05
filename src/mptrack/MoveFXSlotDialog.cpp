/*
 * MoveFXSlotDialog.h
 * ------------------
 * Purpose: Implementationof OpenMPT's move plugin dialog.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/MoveFXSlotDialog.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "MoveFXSlotDialog.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "resource.h"


OPENMPT_NAMESPACE_BEGIN


void CMoveFXSlotDialog::DoDataExchange(DataExchange* pDX)
{
	DialogBase::DoDataExchange(pDX);
	pDX->BindControl(IDC_COMBO1, m_CbnEmptySlots);
}


CMoveFXSlotDialog::CMoveFXSlotDialog(Wnd *pParent, PLUGINDEX currentSlot, const std::vector<PLUGINDEX> &emptySlots, PLUGINDEX defaultIndex, bool clone, bool hasChain) :
	DialogBase(IDD_MOVEFXSLOT, pParent),
	m_EmptySlots(emptySlots),
	m_nDefaultSlot(defaultIndex),
	moveChain(hasChain)
{
	if(clone)
	{
		m_csPrompt = ui::Format(UL_("Clone plugin in slot %d to the following empty slot:"), currentSlot + 1);
		m_csTitle = UL_("Clone To Slot...");
		m_csChain = UL_("&Clone follow-up plugin chain if possible");
	} else
	{
		m_csPrompt = ui::Format(UL_("Move plugin in slot %d to the following empty slot:"), currentSlot + 1);
		m_csTitle = UL_("Move To Slot...");
		m_csChain = UL_("&Move follow-up plugin chain if possible");
	}
}


bool CMoveFXSlotDialog::OnInitDialog()
{
	DialogBase::OnInitDialog();
	SetDlgItemText(IDC_STATIC1, m_csPrompt);
	SetDlgItemText(IDC_CHECK1, m_csChain);
	SetWindowText(m_csTitle);

	if(m_EmptySlots.empty())
	{
		Reporting::Error("No empty plugin slots are availabe.");
		OnCancel();
		return true;
	}

	mpt::ustring slotText;
	std::size_t defaultSlot = 0;
	bool foundDefault = false;
	for(size_t nSlot = 0; nSlot < m_EmptySlots.size(); nSlot++)
	{
		slotText = ui::Format(UL_("FX%d"), m_EmptySlots[nSlot] + 1);
		m_CbnEmptySlots.SetItemData(m_CbnEmptySlots.AddString(slotText), nSlot);
		if(m_EmptySlots[nSlot] >= m_nDefaultSlot && !foundDefault)
		{
			defaultSlot = nSlot;
			foundDefault = true;
		}
	}
	m_CbnEmptySlots.SetCurSel(static_cast<int>(defaultSlot));

	GetDlgItem(IDC_CHECK1)->EnableWindow(moveChain ? true : false);
	CheckDlgButton(IDC_CHECK1, moveChain ? ui::CheckOn : ui::CheckOff);

	return true;
}


void CMoveFXSlotDialog::OnOK()
{
	m_nToSlot = m_CbnEmptySlots.GetItemData(m_CbnEmptySlots.GetCurSel());
	moveChain = IsDlgButtonChecked(IDC_CHECK1) != ui::CheckOff;
	DialogBase::OnOK();
}


OPENMPT_NAMESPACE_END
