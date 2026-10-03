// FLTK port of openmpt/mptrack/MIDIMacroDialog.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "ColourEdit.h"
#include "DialogBase.h"
#include "PluginComboBox.h"
#include "../common/misc_util.h"
#include "../soundlib/MIDIMacros.h"
#include "mpt/base/alloc.hpp"

OPENMPT_NAMESPACE_BEGIN

class CTrackerSoundFile;

class CMidiMacroSetup : public DialogBase
{
protected:
	ComboBox m_CbnSFx, m_CbnSFxPreset, m_CbnZxx, m_CbnZxxPreset, m_CbnMacroParam, m_CbnMacroCC;
	PluginComboBox m_CbnMacroPlug;
	Edit m_EditSFx, m_EditZxx;
	struct MacroEdit
	{
		ui::Button Button;
		CColourEdit Value;
		CColourEdit Type;
		ui::Button ShowAll;
	};
	std::vector<MacroEdit> m_EditMacro = std::vector<MacroEdit>(static_cast<int>(kSFxMacros));

	CTrackerSoundFile &m_SndFile;

public:
	CMidiMacroSetup(CTrackerSoundFile &sndFile, Wnd *parent = nullptr);
private:
	mpt::heap_value<MIDIMacroConfig> m_vMidiCfg;
public:
	MIDIMacroConfig & m_MidiCfg;

protected:
	bool OnInitDialog() override;
	void DoDataExchange(DataExchange* pDX) override;

	void UpdateMacroList(int macro=-1);
	void ToggleBoxes(uint32 preset, uint32 sfx);
	void UpdateDialog();
	void OnSetAsDefault();
	void OnResetCfg();
	void OnMacroHelp();
	void OnSFxChanged();
	void OnSFxPresetChanged();
	void OnZxxPresetChanged();
	void OnSFxEditChanged();
	void OnZxxEditChanged();
	void UpdateZxxSelection();
	void OnPlugChanged();
	void OnPlugParamChanged();
	void OnCCChanged();

	void OnViewAllParams(uint32 id);
	void OnSetSFx(uint32 id);
	UI_DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
