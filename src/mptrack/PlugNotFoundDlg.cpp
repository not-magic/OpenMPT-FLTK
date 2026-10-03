/*
 * PlugNotFoundDlg.cpp
 * -------------------
 * Purpose: Dialog for handling missing plugins
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "ui/Ui.h"
#include "PlugNotFoundDlg.h"
#include "../soundlib/plugins/PluginManager.h"
#include "Mptrack.h"
#include "resource.h"

OPENMPT_NAMESPACE_BEGIN

UI_MESSAGE_MAP_BEGIN(PlugNotFoundDialog, ResizableDialog)
	UI_COMMAND(IDC_BUTTON_REMOVE, &PlugNotFoundDialog::OnRemove)
UI_MESSAGE_MAP_END()


void PlugNotFoundDialog::DoDataExchange(DataExchange *pDX)
{
	ResizableDialog::DoDataExchange(pDX);
	pDX->BindControl(IDC_LIST1, m_List);
}


PlugNotFoundDialog::PlugNotFoundDialog(std::vector<VSTPluginLib *> &plugins, Wnd *parent)
	: ResizableDialog(IDD_MISSINGPLUGS, parent)
	, m_plugins(plugins)
{
}


bool PlugNotFoundDialog::OnInitDialog()
{
	ResizableDialog::OnInitDialog();

	// Initialize table
	const CListCtrlEx::Header headers[] =
	{
		{ UL_("Plugin"),		128, ui::ListColumnLeft },
		{ UL_("File Path"),	308, ui::ListColumnLeft },
	};
	m_List.SetHeaders(headers);
	m_List.SetExtendedStyle(m_List.GetExtendedStyle() | ui::ListStyleFullRowSelect | ui::ListStyleCheckBoxes);

	for(const auto &plug : m_plugins)
	{
		const int insertAt = m_List.InsertItem(m_List.GetItemCount(), plug->libraryName.ToUnicode());
		if(insertAt == -1)
			continue;
		m_List.SetItemText(insertAt, 1, plug->dllPath.ToUnicode());
		m_List.SetItemDataPtr(insertAt, plug);
		m_List.SetCheck(insertAt);
	}
	m_List.SetFocus();

	return true;
}


void PlugNotFoundDialog::OnRemove()
{
	const int plugs = m_List.GetItemCount();
	for(int i = 0; i < plugs; i++)
	{
		if(m_List.GetCheck(i))
		{
			theApp.GetPluginManager()->RemovePlugin(static_cast<VSTPluginLib *>(m_List.GetItemDataPtr(i)));
		}
	}
	OnOK();
}

OPENMPT_NAMESPACE_END
