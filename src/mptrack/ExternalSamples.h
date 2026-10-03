/*
 * ExternalSamples.h
 * -----------------
 * Purpose: Dialogs for locating missing external samples and handling modified samples
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "CListCtrl.h"
#include "ResizableDialog.h"
#include "Snd_defs.h"

OPENMPT_NAMESPACE_BEGIN

class CModDoc;
class CTrackerSoundFile;

class MissingExternalSamplesDlg : public ResizableDialog
{
protected:
	CModDoc &m_modDoc;
	CTrackerSoundFile &m_sndFile;
	CListCtrlEx m_List;
	bool m_isScanning = false;

public:
	MissingExternalSamplesDlg(CModDoc &modDoc, Wnd *parent);

protected:
	void GenerateList();
	bool SetSample(SAMPLEINDEX smp, const mpt::PathString &fileName);
	
	void DoDataExchange(DataExchange *pDX) override;
	bool OnInitDialog() override;

	void OnSetPath(NotifyHeader *, LResult *);
	void OnScanFolder();

	UI_DECLARE_MESSAGE_MAP()
};


class ModifiedExternalSamplesDlg : public ResizableDialog
{
protected:
	CModDoc &m_modDoc;
	CTrackerSoundFile &m_sndFile;
	CListCtrlEx m_List;

public:
	ModifiedExternalSamplesDlg(CModDoc &modDoc, Wnd *parent);

protected:
	void GenerateList();
	void Execute(bool doSave);

	void DoDataExchange(DataExchange *pDX) override;
	bool OnInitDialog() override;
	void OnOK() override { Execute(false); }

	void OnSaveSelected() { Execute(true); }
	void OnCheckAll();
	void OnSelectionChanged(NotifyHeader *pNMHDR, LResult *pResult);

	UI_DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
