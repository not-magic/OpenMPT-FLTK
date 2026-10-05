/*
 * CloseMainDialog.cpp
 * -------------------
 * Purpose: Dialog showing a list of unsaved documents, with the ability to choose which documents should be saved or not.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/CloseMainDialog.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "CloseMainDialog.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "resource.h"


OPENMPT_NAMESPACE_BEGIN


UI_MESSAGE_MAP_BEGIN(CloseMainDialog, ResizableDialog)
	UI_COMMAND(IDC_BUTTON1, &CloseMainDialog::OnSaveAll)
	UI_COMMAND(IDC_BUTTON2, &CloseMainDialog::OnSaveNone)
	UI_COMMAND(IDC_CHECK1,  &CloseMainDialog::OnSwitchFullPaths)
UI_MESSAGE_MAP_END()


void CloseMainDialog::DoDataExchange(DataExchange* pDX)
{
	ResizableDialog::DoDataExchange(pDX);
	pDX->BindControl(IDC_LIST1, m_List);
}


CloseMainDialog::CloseMainDialog() : ResizableDialog(IDD_CLOSEDOCUMENTS) {}


mpt::ustring CloseMainDialog::FormatTitle(const CModDoc &modDoc, bool fullPath)
{
	return MPT_UFORMAT("{} ({})")
		(mpt::ToUnicode(modDoc.GetSoundFile().GetCharsetInternal(), modDoc.GetSoundFile().GetTitle()),
		(!fullPath || modDoc.GetPathNameMpt().empty()) ? modDoc.GetTitle() : modDoc.GetPathNameMpt().ToUnicode());
}


bool CloseMainDialog::OnInitDialog()
{
	ResizableDialog::OnInitDialog();

	// Create list of unsaved documents
	m_List.ResetContent();

	CheckDlgButton(IDC_CHECK1, ui::CheckOn);

	m_List.SetRedraw(false);
	for(CModDoc *modDoc : theApp.GetOpenDocuments())
	{
		if(modDoc->IsModified())
		{
			int item = m_List.AddString(FormatTitle(*modDoc, true));
			m_List.SetItemDataPtr(item, modDoc);
			m_List.SetSel(item, true);
		}
	}
	m_List.SetRedraw(true);

	if(m_List.GetCount() == 0)
	{
		// No modified documents...
		OnOK();
	}

	return true;
}


void CloseMainDialog::OnOK()
{
	const int count = m_List.GetCount();
	for(int i = 0; i < count; i++)
	{
		CModDoc *modDoc = static_cast<CModDoc *>(m_List.GetItemDataPtr(i));
		MPT_ASSERT(modDoc != nullptr);
		if(m_List.GetSel(i))
		{
			modDoc->ActivateWindow();
			if(modDoc->DoFileSave() == false)
			{
				// If something went wrong, or if the user decided to cancel saving (when using "Save As"), we'll better not proceed...
				OnCancel();
				return;
			}
		} else
		{
			modDoc->SetModified(false);
		}
	}

	ResizableDialog::OnOK();
}


void CloseMainDialog::OnSaveAll()
{
	if(m_List.GetCount() == 1)
		m_List.SetSel(0, true);	// SelItemRange can't select one item: https://jeffpar.github.io/kbarchive/kb/129/Q129428/
	else
		m_List.SelItemRange(true, 0, m_List.GetCount() - 1);
	OnOK();
}


void CloseMainDialog::OnSaveNone()
{
	if(m_List.GetCount() == 1)
		m_List.SetSel(0, false);	// SelItemRange can't select one item: https://jeffpar.github.io/kbarchive/kb/129/Q129428/
	else
		m_List.SelItemRange(false, 0, m_List.GetCount() - 1);
	OnOK();
}


// Switch between full path / filename only display
void CloseMainDialog::OnSwitchFullPaths()
{
	const int count = m_List.GetCount();
	const bool fullPath = (IsDlgButtonChecked(IDC_CHECK1) == ui::CheckOn);
	m_List.SetRedraw(false);
	for(int i = 0; i < count; i++)
	{
		CModDoc *modDoc = static_cast<CModDoc *>(m_List.GetItemDataPtr(i));
		int item = m_List.InsertString(i + 1, FormatTitle(*modDoc, fullPath));
		m_List.SetItemDataPtr(item, modDoc);
		m_List.SetSel(item, m_List.GetSel(i));
		m_List.DeleteString(i);
	}
	m_List.SetRedraw(true);
}

OPENMPT_NAMESPACE_END
