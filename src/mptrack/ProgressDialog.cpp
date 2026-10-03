/*
 * ProgressDialog.cpp
 * ------------------
 * Purpose: An abortable, progress-indicating dialog, e.g. for showing conversion progress.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "ui/Ui.h"
#include "ProgressDialog.h"
#include "Mptrack.h"
#include "resource.h"

OPENMPT_NAMESPACE_BEGIN

UI_MESSAGE_MAP_BEGIN(CProgressDialog, DialogBase)
	UI_COMMAND(IDC_BUTTON1,	&CProgressDialog::Run)
UI_MESSAGE_MAP_END()

CProgressDialog::CProgressDialog(Wnd *parent, uint32 resourceID)
	: DialogBase{resourceID <= 0 ? IDD_PROGRESS : resourceID, parent}
	, m_customDialog{resourceID > 0}
{ }

CProgressDialog::~CProgressDialog()
{
	if(GetFrameWindow() != nullptr)
	{
		// This should only happen if this dialog gets destroyed as part of stack unwinding
		EndDialog(IDCANCEL);
	}
}

bool CProgressDialog::OnInitDialog()
{
	DialogBase::OnInitDialog();
	if(!m_customDialog)
		PostCommand(IDC_BUTTON1);
	return true;
}


void CProgressDialog::SetTitle(const mpt::ustring &title)
{
	SetWindowText(title);
}



void CProgressDialog::SetAbortText(const mpt::ustring &abort)
{
	SetDlgItemText(IDCANCEL, abort);
}


void CProgressDialog::SetText(const mpt::ustring &text)
{
	SetDlgItemText(IDC_TEXT1, text);
}


void CProgressDialog::SetRange(uint64 min, uint64 max)
{
	MPT_ASSERT(min <= max);
	m_min = min;
	m_max = max;
	m_shift = 0;
	// Is the range too big for 32-bit values?
	while(max > int32_max)
	{
		m_shift++;
		max >>= 1;
	}
	if(ProgressBar *bar = dynamic_cast<ProgressBar *>(GetDlgItem(IDC_PROGRESS1)))
		bar->SetRange(static_cast<int>(m_min >> m_shift), static_cast<int>(m_max >> m_shift));
}


void CProgressDialog::SetProgress(uint64 progress)
{
	if(ProgressBar *bar = dynamic_cast<ProgressBar *>(GetDlgItem(IDC_PROGRESS1)))
		bar->SetPos(static_cast<int>(progress >> m_shift));
}


void CProgressDialog::ProcessMessages()
{
	Fl::check();
}

OPENMPT_NAMESPACE_END
