// FLTK port of openmpt/mptrack/WelcomeDialog.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "WelcomeDialog.h"
#include "CommandSet.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Mptrack.h"
#include "resource.h"
#include "SelectPluginDialog.h"
#include "TrackerSettings.h"
#include "WindowMessages.h"
#include "../common/mptStringBuffer.h"
#include "mpt/string/utility.hpp"


OPENMPT_NAMESPACE_BEGIN

UI_MESSAGE_MAP_BEGIN(WelcomeDlg, DialogBase)
	UI_COMMAND(IDC_BUTTON1, &WelcomeDlg::OnOptions)
	UI_COMMAND(IDC_BUTTON2, &WelcomeDlg::OnScanPlugins)
UI_MESSAGE_MAP_END()


WelcomeDlg::WelcomeDlg(Wnd *parent)
{
	Create(IDD_WECLOME, parent);
	CenterWindow(parent);
}


static mpt::PathString GetFullKeyPath(const char *keyFile)
{
	return theApp.GetInstallPkgPath() + P_("extraKeymaps/") + mpt::PathString::FromUTF8(keyFile) + P_(".mkb");
}


bool WelcomeDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();

#ifdef MPT_WITH_VST
	if(const char *vstPathEnv = std::getenv("VST_PATH"); vstPathEnv != nullptr && FileSystem::IsDirectory(mpt::PathString::FromUTF8(vstPathEnv)))
	{
		m_vstPath = mpt::PathString::FromUTF8(vstPathEnv);
	} else if(const mpt::PathString homeVstPath = FileSystem::FindHomeDirectory() + P_(".vst/"); FileSystem::IsDirectory(homeVstPath))
	{
		m_vstPath = homeVstPath;
	}
	SetDlgItemText(IDC_EDIT2, mpt::ToUnicode(TrackerSettings::Instance().defaultArtist.Get()));
	if(!m_vstPath.empty())
	{
		SetDlgItemText(IDC_EDIT1, m_vstPath.ToUnicode());
		if(TrackerSettings::Instance().PathPlugins.GetDefaultDir().empty())
		{
			TrackerSettings::Instance().PathPlugins.SetDefaultDir(m_vstPath);
		}
	} else
#endif // MPT_WITH_VST
	{
		SetDlgItemText(IDC_EDIT1, UL_("No plugin path found!"));
		GetDlgItem(IDC_BUTTON2)->EnableWindow(false);
	}

	const char *keyFile = nullptr;
	const mpt::uchar *keyFileName = nullptr;
	ComboBox *combo = (ComboBox *)GetDlgItem(IDC_COMBO1);
	combo->AddString(UL_("OpenMPT / Chromatic (Default)"));
	combo->SetCurSel(0);

	const char *localeName = std::getenv("LC_ALL");
	if(localeName == nullptr || *localeName == '\0')
		localeName = std::getenv("LANG");
	const std::string_view locale = localeName ? localeName : "";
	if(locale.starts_with("de"))
	{
		keyFile = "DE_jojo";
		keyFileName = UL_("German");
	} else if(locale.starts_with("es") && !locale.starts_with("es_ES"))
	{
		// Spanish latin-american keymap, so we ignore Spain.
		keyFile = "es-LA_mpt_(jmkz)";
		keyFileName = UL_("Spanish");
	} else if(locale.starts_with("fr"))
	{
		keyFile = "FR_mpt_(legovitch)";
		keyFileName = UL_("French");
	} else if(locale.starts_with("nb") || locale.starts_with("nn") || locale.starts_with("no"))
	{
		keyFile = "NO_mpt_classic_(rakib)";
		keyFileName = UL_("Norwegian");
	}
	if(keyFile != nullptr)
	{
		if(FileSystem::IsFile(GetFullKeyPath(keyFile)))
		{
			int i = combo->AddString(UL_("OpenMPT / Chromatic (") + mpt::ustring(keyFileName) + UL_(")"));
			combo->SetItemDataPtr(i, (void *)keyFile);
			combo->SetCurSel(i);

			// As this is presented as the default, load it right now, even if the user closes the dialog through the close button
			auto cmdSet = std::make_unique<CCommandSet>();
			cmdSet->LoadFile(GetFullKeyPath(keyFile));
			CMainFrame::GetInputHandler()->SetNewCommandSet(*cmdSet);
		}
	}
	combo->SetItemDataPtr(combo->AddString(UL_("Impulse Tracker")), (void*)("US_mpt-it2_classic"));
	combo->SetItemDataPtr(combo->AddString(UL_("FastTracker 2")), (void*)("US_mpt-ft2_classic"));

	CheckDlgButton(IDC_CHECK1, ui::CheckOn);
	CheckDlgButton(IDC_CHECK3, ui::CheckOn);
	CheckDlgButton(IDC_CHECK2, (TrackerSettings::Instance().patternFont.Get().name == PATTERNFONT_LARGE) ? ui::CheckOn : ui::CheckOff);

	ShowWindow(true);

	return true;
}


void WelcomeDlg::OnOptions()
{
	OnOK();
	CMainFrame::GetMainFrame()->PostCommand(ID_VIEW_OPTIONS);
}


void WelcomeDlg::OnScanPlugins()
{
#ifdef MPT_WITH_VST
	CSelectPluginDlg::ScanPlugins(m_vstPath, this);
#endif // MPT_WITH_VST
}


void WelcomeDlg::OnOK()
{
	DialogBase::OnOK();

	if(IsDlgButtonChecked(IDC_CHECK2) != ui::CheckOff)
	{
		FontSetting font = TrackerSettings::Instance().patternFont;
		font.name = PATTERNFONT_LARGE;
		TrackerSettings::Instance().patternFont = font;
	}

	ComboBox *combo = (ComboBox *)GetDlgItem(IDC_COMBO1);
	const char *keyFile = static_cast<char *>(combo->GetItemDataPtr(combo->GetCurSel()));
	auto cmdSet = std::make_unique<CCommandSet>();
	if(keyFile != nullptr)
		cmdSet->LoadFile(GetFullKeyPath(keyFile));
	else
		cmdSet->LoadDefaultKeymap();
	CMainFrame::GetInputHandler()->SetNewCommandSet(*cmdSet);


	theApp.GetSettings().Flush();

	CMainFrame::GetMainFrame()->PostMessage(MSG_MOD_INVALIDATEPATTERNS, HINT_MPTOPTIONS);

	DestroyWindow();
}

void WelcomeDlg::OnCancel()
{
	DialogBase::OnCancel();
	DestroyWindow();
}

OPENMPT_NAMESPACE_END
