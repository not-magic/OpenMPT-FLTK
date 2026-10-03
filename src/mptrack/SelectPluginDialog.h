// FLTK port of openmpt/mptrack/SelectPluginDialog.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"
#include "ResizableDialog.h"
#include "../common/ComponentManager.h"
#include "../soundlib/Snd_defs.h"

OPENMPT_NAMESPACE_BEGIN

class CModDoc;
struct SNDMIXPLUGIN;
struct VSTPluginLib;
class ComponentPluginBridge_x86;
class ComponentPluginBridgeLegacy_x86;
class ComponentPluginBridge_amd64;
class ComponentPluginBridgeLegacy_amd64;
#if defined(MPT_WITH_WINDOWS10)
class ComponentPluginBridge_arm;
class ComponentPluginBridgeLegacy_arm;
class ComponentPluginBridge_arm64;
class ComponentPluginBridgeLegacy_arm64;
#endif  // MPT_WITH_WINDOWS10

class CSelectPluginDlg : public ResizableDialog
{
protected:
	SNDMIXPLUGIN *m_pPlugin = nullptr;
	CModDoc *m_pModDoc = nullptr;
	TreeCtrl m_treePlugins;
	Button m_chkBridge;
	Button m_chkShare;
	Button m_chkLegacyBridge;
	mpt::ustring m_nameFilter;
#ifdef MPT_WITH_VST
	ComponentHandle<ComponentPluginBridge_x86> pluginBridge_x86;
	ComponentHandle<ComponentPluginBridgeLegacy_x86> pluginBridgeLegacy_x86;
	ComponentHandle<ComponentPluginBridge_amd64> pluginBridge_amd64;
	ComponentHandle<ComponentPluginBridgeLegacy_amd64> pluginBridgeLegacy_amd64;
#if defined(MPT_WITH_WINDOWS10)
	ComponentHandle<ComponentPluginBridge_arm> pluginBridge_arm;
	ComponentHandle<ComponentPluginBridgeLegacy_arm> pluginBridgeLegacy_arm;
	ComponentHandle<ComponentPluginBridge_arm64> pluginBridge_arm64;
	ComponentHandle<ComponentPluginBridgeLegacy_arm64> pluginBridgeLegacy_arm64;
#endif  // MPT_WITH_WINDOWS10
#endif  // !MPT_WITH_VST
	PLUGINDEX m_nPlugSlot = 0;

public:
	CSelectPluginDlg(CModDoc *pModDoc, PLUGINDEX pluginSlot, Wnd *parent);
	~CSelectPluginDlg();

	static VSTPluginLib *ScanPlugins(const mpt::PathString &path, Wnd *parent);
	static bool VerifyPlugin(VSTPluginLib *plug, Wnd *parent);

protected:
	TreeItemHandle AddTreeItem(const mpt::ustring &title, int image, bool sort, TreeItemHandle hParent = ui::TreeRoot, LParam lParam = NULL);

	VSTPluginLib *GetSelectedPlugin();
	void SaveWindowPos() const;

	void ReloadMissingPlugins(const VSTPluginLib &lib) const;

	void UpdatePluginsList(const VSTPluginLib *forceSelect = nullptr);

	void DoDataExchange(DataExchange *pDX) override;
	bool OnInitDialog() override;
	void OnDPIChanged() override;
	void OnOK() override;
	void OnCancel() override;
	bool PreTranslateMessage(int event) override;

	UI_DECLARE_MESSAGE_MAP()
	void OnAddPlugin();
	void OnScanFolder();
	void OnRemovePlugin();
	void OnNameFilterChanged();
	void OnSetBridge();
	void OnSelChanged(NotifyHeader *pNotifyStruct, LResult *result);
	void OnSelDblClk(NotifyHeader *pNotifyStruct, LResult *result);
	void OnPluginTagsChanged();
};

OPENMPT_NAMESPACE_END
