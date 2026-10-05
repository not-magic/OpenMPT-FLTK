/*
 * SampleConfigDlg.cpp
 * -------------------
 * Purpose: Implementation of the sample/instrument editor settings dialog.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/SampleConfigDlg.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "SampleConfigDlg.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "resource.h"
#include "TrackerSettings.h"


OPENMPT_NAMESPACE_BEGIN

UI_MESSAGE_MAP_BEGIN(COptionsSampleEditor, PropertyPage)
	UI_NOTIFY(ui::EditChange, IDC_EDIT_UNDOSIZE,       &COptionsSampleEditor::OnUndoSizeChanged)
	UI_NOTIFY(ui::EditChange, IDC_EDIT_FINETUNE,       &COptionsSampleEditor::OnSettingsChanged)
	UI_NOTIFY(ui::EditChange, IDC_FLAC_COMPRESSION,    &COptionsSampleEditor::OnSettingsChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_DEFAULT_FORMAT,  &COptionsSampleEditor::OnSettingsChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_VOLUME_HANDLING, &COptionsSampleEditor::OnSettingsChanged)
	UI_COMMAND(IDC_RADIO1,                &COptionsSampleEditor::OnSettingsChanged)
	UI_COMMAND(IDC_RADIO2,                &COptionsSampleEditor::OnSettingsChanged)
	UI_COMMAND(IDC_RADIO3,                &COptionsSampleEditor::OnSettingsChanged)
	UI_COMMAND(IDC_RADIO4,                &COptionsSampleEditor::OnSettingsChanged)
	UI_COMMAND(IDC_RADIO5,                &COptionsSampleEditor::OnSettingsChanged)
	UI_COMMAND(IDC_RADIO6,                &COptionsSampleEditor::OnSettingsChanged)
	UI_COMMAND(IDC_COMPRESS_ITI,          &COptionsSampleEditor::OnSettingsChanged)
	UI_COMMAND(IDC_PREVIEW_SAMPLES,       &COptionsSampleEditor::OnSettingsChanged)
	UI_COMMAND(IDC_NORMALIZE,             &COptionsSampleEditor::OnSettingsChanged)
	UI_COMMAND(IDC_CURSORINHEX,           &COptionsSampleEditor::OnSettingsChanged)
UI_MESSAGE_MAP_END()


void COptionsSampleEditor::DoDataExchange(DataExchange* pDX)
{
	PropertyPage::DoDataExchange(pDX);
	pDX->BindControl(IDC_DEFAULT_FORMAT, m_cbnDefaultSampleFormat);
	pDX->BindControl(IDC_VOLUME_HANDLING, m_cbnDefaultVolumeHandling);
	pDX->BindControl(IDC_EDIT_UNDOSIZE, m_undoBufferEdit);
	pDX->BindControl(IDC_EDIT_FINETUNE, m_finetuneEdit);
}


COptionsSampleEditor::COptionsSampleEditor() : PropertyPage{IDD_OPTIONS_SAMPLEEDITOR}
{
	m_undoBufferEdit.SetAccessibleSuffix(UL_("%"));
	m_finetuneEdit.SetAccessibleSuffix(UL_("cents"));
}


bool COptionsSampleEditor::OnInitDialog()
{
	PropertyPage::OnInitDialog();
	SetDlgItemInt(IDC_EDIT_UNDOSIZE, TrackerSettings::Instance().m_SampleUndoBufferSize.Get().GetSizeInPercent());
	SetDlgItemInt(IDC_EDIT_FINETUNE, TrackerSettings::Instance().m_nFinetuneStep);
	m_undoBufferEdit.SetRange32(0, 100);
	m_finetuneEdit.SetRange32(1, 200);
	RecalcUndoSize();

	m_cbnDefaultSampleFormat.SetItemData(m_cbnDefaultSampleFormat.AddString(UL_("FLAC")), dfFLAC);
	m_cbnDefaultSampleFormat.SetItemData(m_cbnDefaultSampleFormat.AddString(UL_("WAV")), dfWAV);
	m_cbnDefaultSampleFormat.SetItemData(m_cbnDefaultSampleFormat.AddString(UL_("RAW")), dfRAW);
	m_cbnDefaultSampleFormat.SetItemData(m_cbnDefaultSampleFormat.AddString(UL_("S3I")), dfS3I);
	m_cbnDefaultSampleFormat.SetItemData(m_cbnDefaultSampleFormat.AddString(UL_("IFF")), dfIFF);
	m_cbnDefaultSampleFormat.SetCurSel(TrackerSettings::Instance().m_defaultSampleFormat);

	HSlider *slider = static_cast<HSlider *>(GetDlgItem(IDC_SLIDER1));
	slider->SetRange(0, 8);
	slider->SetTicFreq(1);
	slider->SetPos(TrackerSettings::Instance().m_FLACCompressionLevel);

	CheckRadioButton(IDC_RADIO1, IDC_RADIO3, IDC_RADIO1 + TrackerSettings::Instance().sampleEditorKeyBehaviour);
	CheckRadioButton(IDC_RADIO4, IDC_RADIO6, IDC_RADIO4 + static_cast<int>(TrackerSettings::Instance().m_followSamplePlayCursor.Get()));

	CheckDlgButton(IDC_COMPRESS_ITI, TrackerSettings::Instance().compressITI ? ui::CheckOn : ui::CheckOff);

	m_cbnDefaultVolumeHandling.SetItemData(m_cbnDefaultVolumeHandling.AddString(UL_("MIDI volume")), PLUGIN_VOLUMEHANDLING_MIDI);
	m_cbnDefaultVolumeHandling.SetItemData(m_cbnDefaultVolumeHandling.AddString(UL_("Dry/Wet ratio")), PLUGIN_VOLUMEHANDLING_DRYWET);
	m_cbnDefaultVolumeHandling.SetItemData(m_cbnDefaultVolumeHandling.AddString(UL_("None")), PLUGIN_VOLUMEHANDLING_IGNORE);
	m_cbnDefaultVolumeHandling.SetCurSel(TrackerSettings::Instance().DefaultPlugVolumeHandling);

	CheckDlgButton(IDC_PREVIEW_SAMPLES, TrackerSettings::Instance().previewInFileDialogs ? ui::CheckOn : ui::CheckOff);
	CheckDlgButton(IDC_NORMALIZE, TrackerSettings::Instance().m_MayNormalizeSamplesOnLoad ? ui::CheckOn : ui::CheckOff);
	CheckDlgButton(IDC_CURSORINHEX, TrackerSettings::Instance().cursorPositionInHex ? ui::CheckOn : ui::CheckOff);

	return true;
}


void COptionsSampleEditor::OnOK()
{
	PropertyPage::OnOK();

	TrackerSettings::Instance().m_nFinetuneStep = GetDlgItemInt(IDC_EDIT_FINETUNE);
	TrackerSettings::Instance().m_SampleUndoBufferSize = SampleUndoBufferSize(GetDlgItemInt(IDC_EDIT_UNDOSIZE));
	TrackerSettings::Instance().m_defaultSampleFormat = static_cast<SampleEditorDefaultFormat>(m_cbnDefaultSampleFormat.GetItemData(m_cbnDefaultSampleFormat.GetCurSel()));
	TrackerSettings::Instance().m_followSamplePlayCursor = static_cast<FollowSamplePlayCursor>(GetCheckedRadioButton(IDC_RADIO4, IDC_RADIO6) - IDC_RADIO4);
	TrackerSettings::Instance().m_FLACCompressionLevel = static_cast<HSlider *>(GetDlgItem(IDC_SLIDER1))->GetPos();
	TrackerSettings::Instance().sampleEditorKeyBehaviour = static_cast<SampleEditorKeyBehaviour>(GetCheckedRadioButton(IDC_RADIO1, IDC_RADIO3) - IDC_RADIO1);
	TrackerSettings::Instance().compressITI = IsDlgButtonChecked(IDC_COMPRESS_ITI) != ui::CheckOff;
	TrackerSettings::Instance().DefaultPlugVolumeHandling = static_cast<PlugVolumeHandling>(m_cbnDefaultVolumeHandling.GetItemData(m_cbnDefaultVolumeHandling.GetCurSel()));
	TrackerSettings::Instance().previewInFileDialogs = IsDlgButtonChecked(IDC_PREVIEW_SAMPLES) != ui::CheckOff;
	TrackerSettings::Instance().m_MayNormalizeSamplesOnLoad = IsDlgButtonChecked(IDC_NORMALIZE) != ui::CheckOff;
	TrackerSettings::Instance().cursorPositionInHex = IsDlgButtonChecked(IDC_CURSORINHEX) != ui::CheckOff;

	for(auto modDoc : theApp.GetOpenDocuments())
	{
		modDoc->GetSampleUndo().RestrictBufferSize();
	}
}


bool COptionsSampleEditor::OnSetActive()
{
	CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_SAMPLEDITOR;
	return PropertyPage::OnSetActive();
}


void COptionsSampleEditor::OnUndoSizeChanged()
{
	RecalcUndoSize();
	OnSettingsChanged();
}


void COptionsSampleEditor::RecalcUndoSize()
{
	uint32 sizePercent = GetDlgItemInt(IDC_EDIT_UNDOSIZE);
	uint32 sizeMB = mpt::saturate_cast<uint32>(SampleUndoBufferSize(sizePercent).GetSizeInBytes() >> 20);
	mpt::ustring text = UL_("% of physical memory (");
	if(sizePercent)
		text += ui::Format(UL_("%u MiB)"), sizeMB);
	else
		text.append(UL_("disabled)"));
	SetDlgItemText(IDC_UNDOSIZE, text);
}

OPENMPT_NAMESPACE_END
