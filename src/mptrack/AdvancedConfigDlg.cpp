/*
 * AdvancedConfigDlg.cpp
 * ---------------------
 * Purpose: Implementation of the advanced settings dialog.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "ui/Ui.h"
#include "AdvancedConfigDlg.h"
#include "dlg_misc.h"
#include "Mainfrm.h"
#include "Mptrack.h"
#include "resource.h"
#include "Settings.h"
#include "TrackerSettings.h"
#include "WindowMessages.h"

OPENMPT_NAMESPACE_BEGIN

UI_MESSAGE_MAP_BEGIN(COptionsAdvanced, PropertyPage)
	UI_NOTIFY(ui::ListDblClick, IDC_LIST1, &COptionsAdvanced::OnOptionDblClick)
	UI_NOTIFY(ui::EditChange, IDC_EDIT1, &COptionsAdvanced::OnFindStringChanged)
	UI_COMMAND(IDC_BUTTON1, &COptionsAdvanced::OnSaveNow)
UI_MESSAGE_MAP_END()

void COptionsAdvanced::DoDataExchange(DataExchange* pDX)
{
	PropertyPage::DoDataExchange(pDX);
	pDX->BindControl(IDC_LIST1, m_List);
}


COptionsAdvanced::COptionsAdvanced() : PropertyPage{IDD_OPTIONS_ADVANCED}, m_List{m_indexToPath} {}

COptionsAdvanced::~COptionsAdvanced() {}


bool COptionsAdvanced::PreTranslateMessage(int event)
{
	if(event == FL_KEYBOARD && ui::KeyFromEvent() == ui::Key_RETURN)
	{
		OnOptionDblClick(nullptr, nullptr);
		return true;
	}
	return false;
}


bool COptionsAdvanced::OnInitDialog()
{
	PropertyPage::OnInitDialog();

	m_List.SetExtendedStyle(m_List.GetExtendedStyle() | ui::ListStyleGridLines | ui::ListStyleFullRowSelect);

	static constexpr ListCtrl::Header headers[] =
	{
		{ UL_("Setting"), 220, ui::ListColumnLeft },
		{ UL_("Type"),    40,  ui::ListColumnLeft },
		{ UL_("Value"),   120, ui::ListColumnLeft },
		{ UL_("Default"), 66,  ui::ListColumnLeft },
	};
	m_List.SetHeaders(headers);

	ReInit();
	return true;
}


void COptionsAdvanced::ReInit()
{
	m_List.SetRedraw(false);
	m_List.DeleteAllItems();
	m_List.SetItemCount(static_cast<int>(theApp.GetSettings().size()));

	m_indexToPath.clear();
	m_indexToPath.reserve(theApp.GetSettings().size());

	const mpt::ustring findStr = mpt::ToUnicode(mpt::ToLowerCaseLocale(GetDlgItem(IDC_EDIT1)->GetWindowText()));

	int i = 0;
	for(const auto &[path, state] : theApp.GetSettings())
	{
		if(!state.has_value())
		{
			continue;
		}
		const SettingValue &value = state.value().GetRefValue();
		const SettingValue &defaultValue = state.value().GetRefDefault();

		if(!findStr.empty())
		{
			mpt::ustring str = path.FormatAsString() + UL_("=") + value.FormatValueAsString();
			str = mpt::ToLowerCaseLocale(str);
			if(str.find(findStr) == mpt::ustring::npos)
			{
				continue;
			}
		}

		const int index = m_List.InsertItem(i++, path.FormatAsString(), -1, m_indexToPath.size());
		m_List.SetItemText(index, 1, mpt::ToUnicode(value.FormatTypeAsString()));
		m_List.SetItemText(index, 2, mpt::ToUnicode(value.FormatValueAsString()));
		m_List.SetItemText(index, 3, mpt::ToUnicode(defaultValue.FormatValueAsString()));
		m_indexToPath.push_back(path);
	}

	m_List.SetItemCount(i);
	m_List.SetRedraw(true);
	m_List.Invalidate(false);
}


void COptionsAdvanced::OnOK()
{
	CTrackerSoundFile::SetDefaultNoteNames();
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if (pMainFrm) pMainFrm->PostMessage(MSG_MOD_INVALIDATEPATTERNS, HINT_MPTOPTIONS);
	PropertyPage::OnOK();
}


bool COptionsAdvanced::OnSetActive()
{
	ReInit();
	CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_ADVANCED;
	return PropertyPage::OnSetActive();
}


ColorRef CAdvancedSettingsList::OnGetCellBkColor(int nRow, int /* nColumn */ )
{
	const bool isDefault = theApp.GetSettings().GetMap().find(m_indexToPath[GetItemData(nRow)])->second.value().IsDefault();
	ColorRef defColor = GetBkColor();
	ColorRef txtColor = GetTextColor();
	ColorRef modColor = RGB(GetRValue(defColor) * 0.9 + GetRValue(txtColor) * 0.1, GetGValue(defColor) * 0.9 + GetGValue(txtColor) * 0.1, GetBValue(defColor) * 0.9 + GetBValue(txtColor) * 0.1);
	return isDefault ? defColor : modColor;
}


ColorRef CAdvancedSettingsList::OnGetCellTextColor(int nRow, int nColumn)
{
	return ListCtrl::OnGetCellTextColor(nRow, nColumn);
}


void COptionsAdvanced::OnOptionDblClick(NotifyHeader *, LResult *)
{
	const int index = m_List.GetSelectionMark();
	if(index < 0)
		return;
	const SettingPath path = m_indexToPath[m_List.GetItemData(index)];
	SettingValue val = theApp.GetSettings().GetMap().find(path)->second.value();
	if(val.GetType() == SettingTypeBool)
	{
		val = !val.as<bool>();
	} else
	{
		CInputDlg inputDlg(this, UL_("Enter new value for ") + mpt::ToUnicode(path.FormatAsString()), mpt::ToUnicode(val.FormatValueAsString()));
		if(inputDlg.DoModal() != IDOK)
		{
			return;
		}
		val.SetFromString(inputDlg.resultAsString);
	}
	theApp.GetSettings().Write(path, val);
	m_List.SetItemText(index, 2, mpt::ToUnicode(val.FormatValueAsString()));
	m_List.SetSelectionMark(index);
	OnSettingsChanged();
}


void COptionsAdvanced::OnSaveNow()
{
	TrackerSettings::Instance().SaveSettings();
	theApp.GetSettings().Flush();
}


OPENMPT_NAMESPACE_END
