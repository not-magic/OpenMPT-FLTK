/*
 * PluginComboBox.h
 * ----------------
 * Purpose: Combo box utilities for plugin and plugin parameter lists
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"
#include "UpdateHints.h"
#include "../soundlib/Snd_defs.h"

#include <optional>

OPENMPT_NAMESPACE_BEGIN

class CTrackerSoundFile;
class IMixPlugin;
struct SNDMIXPLUGIN;

class PluginComboBox : public ComboBox
{
public:
	enum Flags
	{
		ShowNoPlugin      = 0x01,
		ShowEmptySlots    = 0x02,
		ShowInputs        = 0x04,
		ShowLibraryNames  = 0x08,
		DoNotResetContent = 0x10,
	};

	struct Config
	{
		friend class PluginComboBox;

		Config() = default;
		explicit Config(FlagSet<PluginComboBox::Flags> flags) : m_flags{flags} {}
		Config(UpdateHint hint, const HintObject *sender = nullptr) : m_updateHintSender{sender}, m_hint{hint} {}

		Config& Hint(UpdateHint hint, const HintObject *sender = nullptr) { m_updateHintSender = sender; m_hint = hint; return *this; }
		Config& Flags(FlagSet<PluginComboBox::Flags> flags) { m_flags = flags ; return *this; }
		Config& CurrentSelection(PLUGINDEX selection) { m_curSelection = selection; return *this; }
		Config& FirstPlugin(PLUGINDEX firstPlugin) { m_firstPlugin = firstPlugin; return *this; }

	protected:
		const HintObject *m_updateHintSender = nullptr;
		std::optional<UpdateHint> m_hint;
		std::optional<FlagSet<PluginComboBox::Flags>> m_flags;
		std::optional<PLUGINDEX> m_curSelection;
		PLUGINDEX m_firstPlugin = 0;
	};

	int Update(const Config config, const CTrackerSoundFile &sndFile);
	// Set zero-based plugin index, or PLUGINDEX_INVALID for "no plugin" choice
	void SetSelection(PLUGINDEX plugin);
	// Return 0-based plugin index, or PLUGINDEX_INVALID if "no plugin" is selected or no selection is made.
	std::optional<PLUGINDEX> GetSelection() const;
	
	int GetRawSelection() const { return GetCurSel(); }
	void SetRawSelection(int sel) { SetCurSel(sel); }

protected:
	using ComboBox::GetCurSel;
	using ComboBox::SetCurSel;

	FlagSet<Flags> m_flags;
};

DECLARE_FLAGSET(PluginComboBox::Flags)

void AddPluginParameternamesToCombobox(ComboBox &CBox, SNDMIXPLUGIN &plugarray);
void AddPluginParameternamesToCombobox(ComboBox &CBox, IMixPlugin &plug);

OPENMPT_NAMESPACE_END
