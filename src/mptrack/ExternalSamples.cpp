/*
 * ExternalSamples.cpp
 * -------------------
 * Purpose: Dialogs for locating missing external samples and handling modified samples
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "ui/Ui.h"
#include "ExternalSamples.h"
#include "FileDialog.h"
#include "FolderScanner.h"
#include "Moddoc.h"
#include "MPTrackUtil.h"
#include "Reporting.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "UpdateHints.h"

OPENMPT_NAMESPACE_BEGIN

UI_MESSAGE_MAP_BEGIN(MissingExternalSamplesDlg, ResizableDialog)
	UI_NOTIFY(ui::ListDblClick, IDC_LIST1, &MissingExternalSamplesDlg::OnSetPath)
	UI_COMMAND(IDC_BUTTON1,         &MissingExternalSamplesDlg::OnScanFolder)
UI_MESSAGE_MAP_END()


void MissingExternalSamplesDlg::DoDataExchange(DataExchange *pDX)
{
	ResizableDialog::DoDataExchange(pDX);
	pDX->BindControl(IDC_LIST1, m_List);
}


MissingExternalSamplesDlg::MissingExternalSamplesDlg(CModDoc &modDoc, Wnd *parent)
    : ResizableDialog(IDD_MISSINGSAMPLES, parent)
    , m_modDoc(modDoc)
    , m_sndFile(modDoc.GetSoundFile())
{
}


bool MissingExternalSamplesDlg::OnInitDialog()
{
	ResizableDialog::OnInitDialog();

	// Initialize table
	const CListCtrlEx::Header headers[] =
	{
		{ UL_("Sample"),            128, ui::ListColumnLeft },
		{ UL_("External Filename"), 308, ui::ListColumnLeft },
	};
	m_List.SetHeaders(headers);
	m_List.SetExtendedStyle(m_List.GetExtendedStyle() | ui::ListStyleFullRowSelect);

	GenerateList();
	SetWindowText(UL_("Missing External Samples - ") + m_modDoc.GetPathNameMpt().GetFilename().ToUnicode());

	return true;
}


void MissingExternalSamplesDlg::GenerateList()
{
	m_List.SetRedraw(false);
	m_List.DeleteAllItems();
	mpt::ustring s;
	for(SAMPLEINDEX smp = 1; smp <= m_sndFile.GetNumSamples(); smp++)
	{
		if(m_sndFile.IsExternalSampleMissing(smp))
		{
			s = ui::Format(UL_("%02u: "), smp);
			s += mpt::ToUnicode(m_sndFile.GetCharsetInternal(), m_sndFile.GetSampleName(smp));
			int insertAt = m_List.InsertItem(m_List.GetItemCount(), s);
			if(insertAt == -1)
				continue;
			m_List.SetItemText(insertAt, 1, m_sndFile.GetSamplePath(smp).ToUnicode());
			m_List.SetItemData(insertAt, smp);
		}
	}
	m_List.SetRedraw(true);

	// Yay, we managed to find all samples!
	if(!m_List.GetItemCount())
		OnOK();
}


void MissingExternalSamplesDlg::OnSetPath(NotifyHeader *, LResult *)
{
	const int item = m_List.GetSelectionMark();
	if(item == -1) return;
	const SAMPLEINDEX smp = static_cast<SAMPLEINDEX>(m_List.GetItemData(item));

	const mpt::PathString path = m_modDoc.GetSoundFile().GetSamplePath(smp);
	FileDialog dlg = OpenFileDialog()
		.ExtensionFilter(UL_("All Samples|*.wav;*.flac|All files(*.*)|*.*||"));  // Only show samples that we actually can save as well.
	if(TrackerSettings::Instance().previewInFileDialogs)
		dlg.EnableAudioPreview();
	if(path.empty())
		dlg.WorkingDirectory(TrackerSettings::Instance().PathSamples.GetWorkingDir());
	else
		dlg.DefaultFilename(path);
	if(!dlg.Show()) return;
	TrackerSettings::Instance().PathSamples.SetWorkingDir(dlg.GetWorkingDirectory());

	SetSample(smp, dlg.GetFirstFile());
	m_modDoc.UpdateAllViews(nullptr, SampleHint(smp).Info().Names().Data());
	GenerateList();
}


void MissingExternalSamplesDlg::OnScanFolder()
{
	if(m_isScanning)
	{
		m_isScanning = false;
		return;
	}

	BrowseForFolder dlg(TrackerSettings::Instance().PathSamples.GetWorkingDir(), UL_("Select a folder to search for missing samples..."));
	dlg.AddPlace(m_modDoc.GetPathNameMpt().GetDirectoryWithDrive());
	if(dlg.Show())
	{
		TrackerSettings::Instance().PathSamples.SetWorkingDir(dlg.GetDirectory());

		FolderScanner scan(dlg.GetDirectory(), FolderScanner::kOnlyFiles | FolderScanner::kFindInSubDirectories);
		mpt::PathString fileName;

		m_isScanning = true;
		SetDlgItemText(IDC_BUTTON1, UL_("&Cancel"));
		GetDlgItem(IDOK)->EnableWindow(false);
		BeginWaitCursor();

		uint64 lastTick = Util::GetTickCount64();
		int foundFiles = 0;

		bool anyMissing = true;
		while(scan.Next(fileName) && m_isScanning && anyMissing)
		{
			anyMissing = false;
			for(SAMPLEINDEX smp = 1; smp <= m_sndFile.GetNumSamples(); smp++)
			{
				if(m_sndFile.IsExternalSampleMissing(smp))
				{
					if(!mpt::PathCompareNoCase(m_sndFile.GetSamplePath(smp).GetFilename(), fileName.GetFilename()))
					{
						if(SetSample(smp, fileName))
						{
							foundFiles++;
						}
					} else
					{
						anyMissing = true;
					}
				}
			}

			const uint64 tick = Util::GetTickCount64();
			if(tick < lastTick || tick > lastTick + 100)
			{
				lastTick = tick;
				SetDlgItemText(IDC_STATIC1, fileName.ToUnicode());
				ui::PumpMessages();
			}
		}
		EndWaitCursor();
		GetDlgItem(IDOK)->EnableWindow(true);
		SetDlgItemText(IDC_BUTTON1, UL_("&Scan Folder..."));

		m_modDoc.UpdateAllViews(nullptr, SampleHint().Info().Data().Names());

		if(foundFiles)
		{
			SetDlgItemText(IDC_STATIC1, MPT_UFORMAT("{} sample paths were relocated.")(foundFiles));
		} else
		{
			SetDlgItemText(IDC_STATIC1, UL_("No matching sample names found."));
		}
		m_isScanning = false;
		GenerateList();
	}

}


bool MissingExternalSamplesDlg::SetSample(SAMPLEINDEX smp, const mpt::PathString &fileName)
{
	m_modDoc.GetSampleUndo().PrepareUndo(smp, sundo_replace, "Replace");
	const mpt::PathString oldPath = m_sndFile.GetSamplePath(smp);
	if(!m_sndFile.LoadExternalSample(smp, fileName))
	{
		Reporting::Information(UL_("Unable to load sample:\n") + fileName.ToUnicode());
		m_modDoc.GetSampleUndo().RemoveLastUndoStep(smp);
		return false;
	} else
	{
		// Maybe we just put the file into its regular place, in which case the module has not really been modified.
		if(oldPath != fileName)
		{
			m_modDoc.SetModified();
		}
		return true;
	}
}


UI_MESSAGE_MAP_BEGIN(ModifiedExternalSamplesDlg, ResizableDialog)
	UI_COMMAND(IDC_SAVE,                  &ModifiedExternalSamplesDlg::OnSaveSelected)
	UI_COMMAND(IDC_CHECK1,                &ModifiedExternalSamplesDlg::OnCheckAll)
	UI_NOTIFY(ui::ListItemChanged, IDC_LIST1, &ModifiedExternalSamplesDlg::OnSelectionChanged)
UI_MESSAGE_MAP_END()


void ModifiedExternalSamplesDlg::DoDataExchange(DataExchange *pDX)
{
	ResizableDialog::DoDataExchange(pDX);
	pDX->BindControl(IDC_LIST1, m_List);
}


ModifiedExternalSamplesDlg::ModifiedExternalSamplesDlg(CModDoc &modDoc, Wnd *parent)
    : ResizableDialog(IDD_MODIFIEDSAMPLES, parent)
    , m_modDoc(modDoc)
    , m_sndFile(modDoc.GetSoundFile())
{
}


bool ModifiedExternalSamplesDlg::OnInitDialog()
{
	ResizableDialog::OnInitDialog();

	// Initialize table
	const CListCtrlEx::Header headers[] =
	{
		{UL_("Sample"),            120, ui::ListColumnLeft},
		{UL_("Status"),            54,  ui::ListColumnLeft},
		{UL_("External Filename"), 262, ui::ListColumnLeft},
	};
	m_List.SetHeaders(headers);
	m_List.SetExtendedStyle(m_List.GetExtendedStyle() | ui::ListStyleFullRowSelect | ui::ListStyleCheckBoxes);

	GenerateList();
	SetWindowText(UL_("Modified External Samples - ") + m_modDoc.GetPathNameMpt().GetFilename().ToUnicode());

	return true;
}


void ModifiedExternalSamplesDlg::GenerateList()
{
	m_List.SetRedraw(false);
	m_List.DeleteAllItems();
	mpt::ustring s;
	mpt::ustring status;
	for(SAMPLEINDEX smp = 1; smp <= m_sndFile.GetNumSamples(); smp++)
	{
		if(!m_sndFile.GetSample(smp).uFlags[SMP_KEEPONDISK])
			continue;

		if(m_sndFile.GetSample(smp).uFlags[SMP_MODIFIED])
			status = UL_("modified");
		else if(!m_sndFile.GetSample(smp).HasSampleData())
			continue;  // Sample was already missing when the file was loaded, nothing we can do here
		else if(!FileSystem::IsFile(m_sndFile.GetSamplePath(smp)))
			status = UL_("missing");
		else
			continue;

		s = ui::Format(UL_("%02u: "), smp);
		s += mpt::ToUnicode(m_sndFile.GetCharsetInternal(), m_sndFile.GetSampleName(smp));
		int insertAt = m_List.InsertItem(m_List.GetItemCount(), s);
		if(insertAt == -1)
			continue;
		m_List.SetItemText(insertAt, 1, status.c_str());
		m_List.SetItemText(insertAt, 2, m_sndFile.GetSamplePath(smp).ToUnicode());
		m_List.SetCheck(insertAt, true);
		m_List.SetItemData(insertAt, smp);
	}
	m_List.SetRedraw(true);

	CheckDlgButton(IDC_CHECK1, ui::CheckOn);
	OnSelectionChanged(nullptr, nullptr);

	// Nothing modified?
	if(!m_List.GetItemCount())
		OnOK();
}


void ModifiedExternalSamplesDlg::OnCheckAll()
{
	const bool check = IsDlgButtonChecked(IDC_CHECK1) ? true : false;
	const int count = m_List.GetItemCount();
	for(int i = 0; i < count; i++)
	{
		m_List.SetCheck(i, check);
	}
}


void ModifiedExternalSamplesDlg::OnSelectionChanged(NotifyHeader *, LResult *)
{
	int numChecked = 0;
	const int count = m_List.GetItemCount();
	for(int i = 0; i < count; i++)
	{
		if(m_List.GetCheck(i))
			numChecked++;
	}
	const mpt::uchar *embedText, *saveText;
	if(numChecked == count)
	{
		embedText = UL_("&Embed All");
		saveText = UL_("&Save All");
	} else if(!numChecked)
	{
		embedText = UL_("&Embed None");
		saveText = UL_("&Save None");
	} else
	{
		embedText = UL_("&Embed Selected");
		saveText = UL_("&Save Selected");
	}

	GetDlgItem(IDOK)->SetWindowText(embedText);
	GetDlgItem(IDC_SAVE)->SetWindowText(saveText);
}


void ModifiedExternalSamplesDlg::Execute(bool doSave)
{
	ScopedLogCapturer log(m_modDoc, UL_("Modified Samples"), this);

	bool ok = true;
	const int count = m_List.GetItemCount();
	for(int i = 0; i < count; i++)
	{
		if(!m_List.GetCheck(i))
			continue;

		SAMPLEINDEX smp = static_cast<SAMPLEINDEX>(m_List.GetItemData(i));
		if(doSave)
		{
			ok &= m_modDoc.SaveSample(smp);
		} else
		{
			m_sndFile.GetSample(smp).uFlags.reset(SMP_KEEPONDISK);
			m_modDoc.SetModified();
		}
	}
	
	if(ok)
		ResizableDialog::OnOK();
	else
		ResizableDialog::OnCancel();
}


OPENMPT_NAMESPACE_END
