// FLTK port of openmpt/mptrack/PathConfigDlg.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

OPENMPT_NAMESPACE_BEGIN

class PathConfigDlg : public PropertyPage
{

public:
	PathConfigDlg();

protected:
	void DoDataExchange(DataExchange* pDX) override;    // DDX/DDV support

	void OnOK() override;
	bool OnInitDialog() override;

	void OnAutosaveEnable();
	void OnAutosaveUseOrigDir();
	void OnAutosaveRetention();
	void OnBrowseAutosavePath();
	void OnBrowseSongs();
	void OnBrowseSamples();
	void OnBrowseInstruments();
	void OnBrowsePlugins();
	void OnBrowsePresets();

	void OnSettingsChanged();
	bool OnSetActive() override;

	void BrowseFolder(uint32 nID);

	mpt::PathString GetPath(int id);

	UI_DECLARE_MESSAGE_MAP()

	std::array<Spinner, 3> m_accessibleEdits;
	std::array<Button, 6> m_browseButtons;
};

OPENMPT_NAMESPACE_END
