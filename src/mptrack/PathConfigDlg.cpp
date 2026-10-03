/*
 * PathConfigDlg.cpp
 * -----------------
 * Purpose: Default paths and auto save setup dialog.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "ui/Ui.h"
#include "PathConfigDlg.h"
#include "FileDialog.h"
#include "Mainfrm.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "mpt/fs/fs.hpp"

OPENMPT_NAMESPACE_BEGIN

static constexpr std::pair<ConfigurableDirectory TrackerSettings::*, int> PathSettings[] =
{
	{ &TrackerSettings::PathSongs, IDC_OPTIONS_DIR_MODS },
	{ &TrackerSettings::PathSamples, IDC_OPTIONS_DIR_SAMPS },
	{ &TrackerSettings::PathInstruments, IDC_OPTIONS_DIR_INSTS },
	{ &TrackerSettings::PathPlugins, IDC_OPTIONS_DIR_VSTS },
	{ &TrackerSettings::PathPluginPresets, IDC_OPTIONS_DIR_VSTPRESETS },
	{ &TrackerSettings::AutosavePath, IDC_AUTOSAVE_PATH },
};


PathConfigDlg::PathConfigDlg()
	: PropertyPage{IDD_OPTIONS_AUTOSAVE}
{
	m_accessibleEdits[0].SetAccessibleSuffix(UL_("minutes"));
	m_accessibleEdits[1].SetAccessibleSuffix(UL_("backups"));
	m_accessibleEdits[2].SetAccessibleSuffix(UL_("days"));
	m_browseButtons[0].SetAccessibleText(UL_("Browse for song folder..."));
	m_browseButtons[1].SetAccessibleText(UL_("Browse for sample folder..."));
	m_browseButtons[2].SetAccessibleText(UL_("Browse for instrument folder..."));
	m_browseButtons[3].SetAccessibleText(UL_("Browse for VST plugin folder..."));
	m_browseButtons[4].SetAccessibleText(UL_("Browse for VST preset folder..."));
	m_browseButtons[5].SetAccessibleText(UL_("Browse for auto save folder..."));
}


void PathConfigDlg::DoDataExchange(DataExchange* pDX)
{
	PropertyPage::DoDataExchange(pDX);
	pDX->BindControl(IDC_AUTOSAVE_INTERVAL, m_accessibleEdits[0]);
	pDX->BindControl(IDC_AUTOSAVE_HISTORY, m_accessibleEdits[1]);
	pDX->BindControl(IDC_EDIT1, m_accessibleEdits[2]);
	pDX->BindControl(IDC_BUTTON_CHANGE_MODDIR, m_browseButtons[0]);
	pDX->BindControl(IDC_BUTTON_CHANGE_SAMPDIR, m_browseButtons[1]);
	pDX->BindControl(IDC_BUTTON_CHANGE_INSTRDIR, m_browseButtons[2]);
	pDX->BindControl(IDC_BUTTON_CHANGE_VSTDIR, m_browseButtons[3]);
	pDX->BindControl(IDC_BUTTON_CHANGE_VSTPRESETSDIR, m_browseButtons[4]);
	pDX->BindControl(IDC_AUTOSAVE_BROWSE, m_browseButtons[5]);
}

UI_MESSAGE_MAP_BEGIN(PathConfigDlg, PropertyPage)
	// Paths
	UI_NOTIFY(ui::EditChange, IDC_OPTIONS_DIR_MODS,          &PathConfigDlg::OnSettingsChanged)
	UI_NOTIFY(ui::EditChange, IDC_OPTIONS_DIR_SAMPS,         &PathConfigDlg::OnSettingsChanged)
	UI_NOTIFY(ui::EditChange, IDC_OPTIONS_DIR_INSTS,         &PathConfigDlg::OnSettingsChanged)
	UI_NOTIFY(ui::EditChange, IDC_OPTIONS_DIR_VSTPRESETS,    &PathConfigDlg::OnSettingsChanged)
	UI_COMMAND(IDC_BUTTON_CHANGE_MODDIR,        &PathConfigDlg::OnBrowseSongs)
	UI_COMMAND(IDC_BUTTON_CHANGE_SAMPDIR,       &PathConfigDlg::OnBrowseSamples)
	UI_COMMAND(IDC_BUTTON_CHANGE_INSTRDIR,      &PathConfigDlg::OnBrowseInstruments)
	UI_COMMAND(IDC_BUTTON_CHANGE_VSTDIR,        &PathConfigDlg::OnBrowsePlugins)
	UI_COMMAND(IDC_BUTTON_CHANGE_VSTPRESETSDIR, &PathConfigDlg::OnBrowsePresets)

	// Autosave
	UI_COMMAND(IDC_CHECK1,                &PathConfigDlg::OnSettingsChanged)
	UI_COMMAND(IDC_CHECK2,                &PathConfigDlg::OnAutosaveRetention)
	UI_COMMAND(IDC_CHECK3,                &PathConfigDlg::OnSettingsChanged)
	UI_COMMAND(IDC_AUTOSAVE_BROWSE,       &PathConfigDlg::OnBrowseAutosavePath)
	UI_COMMAND(IDC_AUTOSAVE_ENABLE,       &PathConfigDlg::OnAutosaveEnable)
	UI_COMMAND(IDC_AUTOSAVE_USEORIGDIR,   &PathConfigDlg::OnAutosaveUseOrigDir)
	UI_COMMAND(IDC_AUTOSAVE_USECUSTOMDIR, &PathConfigDlg::OnAutosaveUseOrigDir)
	UI_NOTIFY(ui::EditUpdate, IDC_AUTOSAVE_PATH,       &PathConfigDlg::OnSettingsChanged)
	UI_NOTIFY(ui::EditUpdate, IDC_AUTOSAVE_HISTORY,    &PathConfigDlg::OnSettingsChanged)
	UI_NOTIFY(ui::EditUpdate, IDC_AUTOSAVE_INTERVAL,   &PathConfigDlg::OnSettingsChanged)
	UI_NOTIFY(ui::EditUpdate, IDC_EDIT1,               &PathConfigDlg::OnSettingsChanged)
UI_MESSAGE_MAP_END()


bool PathConfigDlg::OnInitDialog()
{
	PropertyPage::OnInitDialog();

	const auto &settings = TrackerSettings::Instance();
	// Default paths
	for(const auto & [path, id] : PathSettings)
	{
		SetDlgItemText(id, (settings.*path).GetDefaultDir().ToUnicode());
	}

	// Autosave
	CheckDlgButton(IDC_CHECK1, settings.CreateBackupFiles ? ui::CheckOn : ui::CheckOff);

	for(Spinner &spinner : m_accessibleEdits)
		spinner.SetRange32(1, int32_max);
	CheckDlgButton(IDC_AUTOSAVE_ENABLE, settings.AutosaveEnabled ? ui::CheckOn : ui::CheckOff);
	SetDlgItemInt(IDC_AUTOSAVE_HISTORY, settings.AutosaveHistoryDepth);
	SetDlgItemInt(IDC_AUTOSAVE_INTERVAL, settings.AutosaveIntervalMinutes);
	SetDlgItemInt(IDC_EDIT1, settings.AutosaveRetentionTimeDays ? settings.AutosaveRetentionTimeDays : 30);
	CheckDlgButton(IDC_CHECK2, settings.AutosaveRetentionTimeDays > 0);
	CheckDlgButton(IDC_AUTOSAVE_USEORIGDIR, settings.AutosaveUseOriginalPath ? ui::CheckOn : ui::CheckOff);
	CheckDlgButton(IDC_AUTOSAVE_USECUSTOMDIR, settings.AutosaveUseOriginalPath ? ui::CheckOff : ui::CheckOn);
	CheckDlgButton(IDC_CHECK3, settings.AutosaveDeletePermanently ? ui::CheckOn : ui::CheckOff);
	//enable/disable stuff as appropriate
	OnAutosaveEnable();
	OnAutosaveUseOrigDir();
	OnAutosaveRetention();

	return true;
}


mpt::PathString PathConfigDlg::GetPath(int id)
{
	return mpt::PathString::FromUnicode(GetWindowTextString(*GetDlgItem(id))).WithTrailingSlash();
}


void PathConfigDlg::OnOK()
{
	auto &settings = TrackerSettings::Instance();
	// Default paths
	for(const auto &path : PathSettings)
	{
		(settings.*path.first).SetDefaultDir(GetPath(path.second));
	}

	// Autosave
	settings.CreateBackupFiles = IsDlgButtonChecked(IDC_CHECK1) != ui::CheckOff;

	settings.AutosaveEnabled = IsDlgButtonChecked(IDC_AUTOSAVE_ENABLE) != ui::CheckOff;
	settings.AutosaveHistoryDepth = GetDlgItemInt(IDC_AUTOSAVE_HISTORY);
	settings.AutosaveIntervalMinutes = GetDlgItemInt(IDC_AUTOSAVE_INTERVAL);
	if(IsDlgButtonChecked(IDC_CHECK2))
		settings.AutosaveRetentionTimeDays = std::max<uint32>(GetDlgItemInt(IDC_EDIT1), 1);
	else
		settings.AutosaveRetentionTimeDays = 0;
	settings.AutosaveUseOriginalPath = IsDlgButtonChecked(IDC_AUTOSAVE_USEORIGDIR) != ui::CheckOff;
	settings.AutosaveDeletePermanently = IsDlgButtonChecked(IDC_CHECK3) != ui::CheckOff;

	PropertyPage::OnOK();
}


void PathConfigDlg::BrowseFolder(uint32 nID)
{
	const mpt::uchar *prompt = (nID == IDC_AUTOSAVE_PATH)
		? UL_("Select a folder to store autosaved files in...")
		: UL_("Select a default folder...");
	BrowseForFolder dlg(GetPath(nID), prompt);
	if(dlg.Show(this))
	{
		SetDlgItemText(nID, dlg.GetDirectory().ToUnicode());
		OnSettingsChanged();
	}
}


void PathConfigDlg::OnAutosaveEnable()
{
	const bool enabled = IsDlgButtonChecked(IDC_AUTOSAVE_ENABLE);
	static constexpr uint32 AutoSaveDlgItems[] =
	{
		IDC_AUTOSAVE_INTERVAL, IDC_AUTOSAVE_HISTORY, IDC_AUTOSAVE_USEORIGDIR, IDC_AUTOSAVE_USECUSTOMDIR,
		IDC_AUTOSAVE_PATH, IDC_AUTOSAVE_BROWSE, IDC_CHECK2, IDC_CHECK3, IDC_EDIT1, IDC_AUTOSAVE_INTERVAL, IDC_AUTOSAVE_HISTORY
	};
	for(uint32 id : AutoSaveDlgItems)
	{
		GetDlgItem(id)->EnableWindow(enabled);
	}
	OnSettingsChanged();
	return;
}


void PathConfigDlg::OnAutosaveUseOrigDir()
{
	if(IsDlgButtonChecked(IDC_AUTOSAVE_ENABLE))
	{
		const bool enabled = IsDlgButtonChecked(IDC_AUTOSAVE_USEORIGDIR) ? false : true;
		static constexpr uint32 AutoSaveDirDlgItems[] = {IDC_AUTOSAVE_PATH, IDC_AUTOSAVE_BROWSE, IDC_CHECK2, IDC_EDIT1};
		for(uint32 id : AutoSaveDirDlgItems)
		{
			GetDlgItem(id)->EnableWindow(enabled);
		}
		OnSettingsChanged();
	}
}


void PathConfigDlg::OnAutosaveRetention()
{
	const bool enabled = (IsDlgButtonChecked(IDC_AUTOSAVE_ENABLE) && IsDlgButtonChecked(IDC_CHECK2)) ? true : false;
	GetDlgItem(IDC_EDIT1)->EnableWindow(enabled);
}


void PathConfigDlg::OnSettingsChanged()
{
	SetModified(true);
}


bool PathConfigDlg::OnSetActive()
{
	CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_PATHS;
	return PropertyPage::OnSetActive();
}


void PathConfigDlg::OnBrowseAutosavePath() { BrowseFolder(IDC_AUTOSAVE_PATH); }
void PathConfigDlg::OnBrowseSongs() { BrowseFolder(IDC_OPTIONS_DIR_MODS); }
void PathConfigDlg::OnBrowseSamples() { BrowseFolder(IDC_OPTIONS_DIR_SAMPS); }
void PathConfigDlg::OnBrowseInstruments() { BrowseFolder(IDC_OPTIONS_DIR_INSTS); }
void PathConfigDlg::OnBrowsePlugins() { BrowseFolder(IDC_OPTIONS_DIR_VSTS); }
void PathConfigDlg::OnBrowsePresets() { BrowseFolder(IDC_OPTIONS_DIR_VSTPRESETS); }


OPENMPT_NAMESPACE_END
