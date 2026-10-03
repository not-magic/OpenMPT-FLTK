// Editor-side IMixPlugin interface from openmpt/soundlib/plugins/PlugInterface.cpp,
// which libopenmpt only builds for the tracker.

#include "stdafx.h"
#include "PluginUi.h"

#include "AbstractVstEditor.h"
#include "DefaultVstEditor.h"
#include "FileDialog.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Reporting.h"
#include "TrackerSettings.h"
#include "VstPresets.h"
#include "WindowMessages.h"
#include "plugins/LFOPluginEditor.h"
#include "plugins/MidiInOut.h"
#include "../common/mptFileIO.h"
#include "../soundlib/Sndfile.h"
#include "../soundlib/mod_specifications.h"
#include "../soundlib/plugins/LFOPlugin.h"
#include "../soundlib/plugins/PlugInterface.h"
#include "../soundlib/plugins/PluginManager.h"
#include "openmpt_ext/plugins/PluginInfo.h"
#include "mpt/io_file/inputfile.hpp"
#include "mpt/io_file/outputfile.hpp"
#include "mpt/io_file_read/inputfile_filecursor.hpp"

#include <FL/Fl.H>

#include <map>

OPENMPT_NAMESPACE_BEGIN

namespace
{

// Only accessed from the GUI thread
std::map<const IMixPlugin *, CAbstractVstEditor *> g_pluginEditors;
// Programs selected through PluginUi for plugins whose programs libopenmpt does not know about
std::map<const IMixPlugin *, int32> g_pluginPrograms;
std::map<const VSTPluginLib *, mpt::ustring> g_libraryTags;

// CVstPluginManager keeps its plugin list protected
struct PluginListAccess : CVstPluginManager
{
	static auto FindPluginList() { return &PluginListAccess::pluginList; }
};

CAbstractVstEditor *CreateEditor(IMixPlugin &plugin)
{
	try
	{
		if(auto *midiPlugin = dynamic_cast<MidiInOut *>(&plugin))
			return midiPlugin->OpenEditor();
		if(auto *lfoPlugin = dynamic_cast<LFOPlugin *>(&plugin))
			return new LFOPluginEditor(*lfoPlugin);
		return new CDefaultVstEditor(plugin);
	} catch(mpt::out_of_memory e)
	{
		mpt::delete_out_of_memory(e);
		return nullptr;
	}
}

}  // namespace


CModDoc *PluginUi::GetModDoc() const
{
	return TrackerSoundFile(m_plugin.GetSoundFile()).GetpModDoc();
}


PLUGINDEX PluginUi::GetSlot() const
{
	const CSoundFile &sndFile = m_plugin.GetSoundFile();
	for(PLUGINDEX slotIndex = 0; slotIndex < MAX_MIXPLUGINS; ++slotIndex)
	{
		if(sndFile.m_MixPlugins[slotIndex].pMixPlugin == &m_plugin)
			return slotIndex;
	}
	return PLUGINDEX_INVALID;
}


void PluginUi::SetSlot(PLUGINDEX slotIndex) const
{
	CAbstractVstEditor::SetPluginSlot(m_plugin, slotIndex);
}


mpt::ustring PluginUi::GetDefaultEffectName() const
{
	if(auto *midiPlugin = dynamic_cast<MidiInOut *>(&m_plugin))
		return midiPlugin->GetDefaultEffectName();
	if(dynamic_cast<LFOPlugin *>(&m_plugin))
		return U_("LFO");
	return PluginInfo::FindDefaultEffectName(m_plugin).value_or(mpt::ustring());
}


std::pair<PlugParamValue, PlugParamValue> PluginUi::GetParamUIRange(PlugParamIndex paramIndex) const
{
	if(auto *lfoPlugin = dynamic_cast<LFOPlugin *>(&m_plugin))
		return LFOPluginEditor::FindParamUIRange(*lfoPlugin, paramIndex);
	return PluginInfo::FindParamUIRange(m_plugin, paramIndex).value_or(std::make_pair(0.0f, 1.0f));
}


PlugParamValue PluginUi::GetScaledUIParam(PlugParamIndex paramIndex) const
{
	const auto [paramMin, paramMax] = GetParamUIRange(paramIndex);
	return (std::clamp(m_plugin.GetParameter(paramIndex), paramMin, paramMax) - paramMin) / (paramMax - paramMin);
}


void PluginUi::SetScaledUIParam(PlugParamIndex paramIndex, PlugParamValue value) const
{
	const auto [paramMin, paramMax] = GetParamUIRange(paramIndex);
	m_plugin.SetParameter(paramIndex, paramMin + std::clamp(value, 0.0f, 1.0f) * (paramMax - paramMin));
}


mpt::ustring PluginUi::GetParamName(PlugParamIndex paramIndex) const
{
	if(auto *midiPlugin = dynamic_cast<MidiInOut *>(&m_plugin))
		return midiPlugin->GetParamName(paramIndex);
	if(auto *lfoPlugin = dynamic_cast<LFOPlugin *>(&m_plugin))
		return LFOPluginEditor::FindParamName(*lfoPlugin, paramIndex);
	return PluginInfo::FindParamName(m_plugin, paramIndex).value_or(mpt::ustring());
}


mpt::ustring PluginUi::GetParamLabel(PlugParamIndex paramIndex) const
{
	if(dynamic_cast<MidiInOut *>(&m_plugin))
		return mpt::ustring();
	if(auto *lfoPlugin = dynamic_cast<LFOPlugin *>(&m_plugin))
		return LFOPluginEditor::FindParamLabel(*lfoPlugin, paramIndex);
	return PluginInfo::FindParamLabel(m_plugin, paramIndex).value_or(mpt::ustring());
}


mpt::ustring PluginUi::GetParamDisplay(PlugParamIndex paramIndex) const
{
	if(auto *midiPlugin = dynamic_cast<MidiInOut *>(&m_plugin))
		return midiPlugin->GetParamDisplay(paramIndex);
	if(auto *lfoPlugin = dynamic_cast<LFOPlugin *>(&m_plugin))
		return LFOPluginEditor::FindParamDisplay(*lfoPlugin, paramIndex);
	return PluginInfo::FindParamDisplay(m_plugin, paramIndex).value_or(mpt::ufmt::fix(m_plugin.GetParameter(paramIndex), 2));
}


mpt::ustring PluginUi::GetFormattedParamName(PlugParamIndex paramIndex) const
{
	const mpt::ustring paramName = GetParamName(paramIndex);
	if(paramName.empty())
		return MPT_UFORMAT("{}: Parameter {}")(mpt::ufmt::dec0<2>(paramIndex), mpt::ufmt::dec0<2>(paramIndex));
	return MPT_UFORMAT("{}: {}")(mpt::ufmt::dec0<2>(paramIndex), paramName);
}


mpt::ustring PluginUi::GetFormattedParamValue(PlugParamIndex paramIndex) const
{
	return mpt::trim(GetParamDisplay(paramIndex)) + U_(" ") + mpt::trim(GetParamLabel(paramIndex));
}


int32 PluginUi::GetNumPrograms() const
{
	return std::max(m_plugin.GetNumPrograms(), PluginInfo::FindNumPrograms(m_plugin));
}


int32 PluginUi::GetCurrentProgram() const
{
	if(const auto programIndex = g_pluginPrograms.find(&m_plugin); programIndex != g_pluginPrograms.end())
		return programIndex->second;
	return m_plugin.GetCurrentProgram();
}


void PluginUi::SetCurrentProgram(int32 programIndex) const
{
	if(PluginInfo::SetProgram(m_plugin, programIndex))
		g_pluginPrograms[&m_plugin] = programIndex;
	else
		m_plugin.SetCurrentProgram(programIndex);
}


mpt::ustring PluginUi::GetCurrentProgramName() const
{
	if(auto *midiPlugin = dynamic_cast<MidiInOut *>(&m_plugin))
		return midiPlugin->GetCurrentProgramName();
	return GetProgramName(GetCurrentProgram());
}


void PluginUi::SetCurrentProgramName(const mpt::ustring &name) const
{
	if(auto *midiPlugin = dynamic_cast<MidiInOut *>(&m_plugin))
		midiPlugin->SetCurrentProgramName(name);
}


mpt::ustring PluginUi::GetProgramName(int32 programIndex) const
{
	if(auto *midiPlugin = dynamic_cast<MidiInOut *>(&m_plugin))
		return midiPlugin->GetProgramName(programIndex);
	return PluginInfo::FindProgramName(m_plugin, programIndex);
}


mpt::ustring PluginUi::GetFormattedProgramName(int32 programIndex) const
{
	const mpt::ustring rawName = GetProgramName(programIndex);
	// Programs are counted from 1, as most MIDI hardware / software does
	const int32 programNumber = programIndex + 1;
	if(rawName.empty() || static_cast<unsigned char>(rawName[0]) < ' ')
		return MPT_UFORMAT("{} - Program {}")(mpt::ufmt::dec0<2>(programNumber), programNumber);
	return MPT_UFORMAT("{} - {}")(mpt::ufmt::dec0<2>(programNumber), rawName);
}


bool PluginUi::HasEditor() const
{
	return dynamic_cast<MidiInOut *>(&m_plugin) || dynamic_cast<LFOPlugin *>(&m_plugin);
}


CAbstractVstEditor *PluginUi::GetEditor() const
{
	const auto editor = g_pluginEditors.find(&m_plugin);
	return editor != g_pluginEditors.end() ? editor->second : nullptr;
}


void PluginUi::ToggleEditor() const
{
	// Guards against re-entrance while the editor window is being created
	static bool s_isInitializing = false;
	if(s_isInitializing)
		return;
	s_isInitializing = true;

	if(GetEditor())
	{
		CloseEditor();
	} else if(CAbstractVstEditor *editor = CreateEditor(m_plugin); editor != nullptr)
	{
		g_pluginEditors[&m_plugin] = editor;
		editor->OpenEditor(CMainFrame::GetMainFrame());
	}
	s_isInitializing = false;
}


void PluginUi::CloseEditor() const
{
	CAbstractVstEditor *editor = GetEditor();
	if(!editor)
		return;
	if(editor->IsWindow())
		editor->DoClose();
	// Closing the window may already have destroyed the editor
	if(CAbstractVstEditor *remaining = GetEditor())
		delete remaining;
	g_pluginEditors.erase(&m_plugin);
}


void PluginUi::SetEditorPos(int32 x, int32 y) const
{
	if(const PLUGINDEX slotIndex = GetSlot(); slotIndex != PLUGINDEX_INVALID)
	{
		SNDMIXPLUGIN &mixPlugin = m_plugin.GetSoundFile().m_MixPlugins[slotIndex];
		mixPlugin.editorX = x;
		mixPlugin.editorY = y;
	}
}


void PluginUi::GetEditorPos(int32 &x, int32 &y) const
{
	if(const PLUGINDEX slotIndex = GetSlot(); slotIndex != PLUGINDEX_INVALID)
	{
		const SNDMIXPLUGIN &mixPlugin = m_plugin.GetSoundFile().m_MixPlugins[slotIndex];
		x = mixPlugin.editorX;
		y = mixPlugin.editorY;
	}
}


void PluginUi::AutomateParameter(PlugParamIndex paramIndex) const
{
	CModDoc *modDoc = GetModDoc();
	if(modDoc == nullptr)
		return;

	const PLUGINDEX slotIndex = GetSlot();
	if(m_plugin.m_recordAutomation)
		modDoc->RecordParamChange(slotIndex, paramIndex);

	modDoc->SendNotifyMessageToAllViews(MSG_MOD_PLUGPARAMAUTOMATE, slotIndex, paramIndex);

	CAbstractVstEditor *editor = GetEditor();
	if(!editor || !editor->IsWindow())
		return;

	SetModified();

	if(Fl::event_state(FL_SHIFT) && TrackerSettings::Instance().midiMappingInPluginEditor)
		CMainFrame::GetMainFrame()->PostMessage(MSG_MOD_MIDIMAPPING, slotIndex, paramIndex);

	if(const int macroToLearn = editor->GetLearnMacro(); macroToLearn > -1)
	{
		modDoc->LearnMacro(macroToLearn, paramIndex);
		editor->SetLearnMacro(-1);
	}
}


void PluginUi::SetModified() const
{
	CModDoc *modDoc = GetModDoc();
	if(modDoc != nullptr && m_plugin.GetSoundFile().GetModSpecifications().supportsPlugins)
		modDoc->SetModified();
}


bool PluginUi::SaveProgram() const
{
	const VSTPluginLib &factory = m_plugin.GetPluginFactory();
	mpt::PathString defaultDir = TrackerSettings::Instance().PathPluginPresets.GetWorkingDir();
	const bool useDefaultDir = !defaultDir.empty();
	if(!useDefaultDir && FileSystem::IsFile(factory.dllPath))
		defaultDir = factory.dllPath.GetDirectoryWithDrive();

	const mpt::ustring progName = mpt::SanitizePathComponent(factory.libraryName.ToUnicode() + U_(" - ") + GetCurrentProgramName());

	FileDialog dlg = SaveFileDialog()
		.DefaultExtension("fxb")
		.DefaultFilename(progName)
		.ExtensionFilter("VST Plugin Programs (*.fxp)|*.fxp|"
			"VST Plugin Banks (*.fxb)|*.fxb||")
		.WorkingDirectory(defaultDir);
	if(!dlg.Show(GetEditor()))
		return false;

	if(useDefaultDir)
		TrackerSettings::Instance().PathPluginPresets.SetWorkingDir(dlg.GetWorkingDirectory());

	const bool isBank = (dlg.GetExtension() == P_("fxb"));
	try
	{
		mpt::IO::SafeOutputFile sf(dlg.GetFirstFile(), std::ios::binary, mpt::IO::FlushModeFromBool(TrackerSettings::Instance().MiscFlushFileBuffersOnSave));
		mpt::IO::ofstream &f = sf;
		f.exceptions(f.exceptions() | std::ios::badbit | std::ios::failbit);
		if(f.good() && VSTPresets::SaveFile(f, m_plugin, isBank))
			return true;
	} catch(const std::exception &)
	{
	}
	Reporting::Error("Error saving preset.", GetEditor());
	return false;
}


bool PluginUi::LoadProgram(mpt::PathString fileName) const
{
	const VSTPluginLib &factory = m_plugin.GetPluginFactory();
	mpt::PathString defaultDir = TrackerSettings::Instance().PathPluginPresets.GetWorkingDir();
	const bool useDefaultDir = !defaultDir.empty();
	if(!useDefaultDir && FileSystem::IsFile(factory.dllPath))
		defaultDir = factory.dllPath.GetDirectoryWithDrive();

	if(fileName.empty())
	{
		FileDialog dlg = OpenFileDialog()
			.DefaultExtension("fxp")
			.ExtensionFilter("VST Plugin Programs and Banks (*.fxp,*.fxb)|*.fxp;*.fxb|"
				"VST Plugin Programs (*.fxp)|*.fxp|"
				"VST Plugin Banks (*.fxb)|*.fxb|"
				"All Files|*.*||")
			.WorkingDirectory(defaultDir);
		if(!dlg.Show(GetEditor()))
			return false;

		if(useDefaultDir)
			TrackerSettings::Instance().PathPluginPresets.SetWorkingDir(dlg.GetWorkingDirectory());
		fileName = dlg.GetFirstFile();
	}

	const char *errorStr = nullptr;
	mpt::IO::InputFile inputFile(fileName, TrackerSettings::Instance().MiscCacheCompleteFileBeforeLoading);
	if(inputFile.IsValid())
	{
		FileReader file = GetFileReader(inputFile);
		errorStr = VSTPresets::GetErrorMessage(VSTPresets::LoadFile(file, m_plugin));
	} else
	{
		errorStr = "Can't open file.";
	}

	if(errorStr != nullptr)
	{
		Reporting::Error(errorStr, GetEditor());
		return false;
	}
	SetModified();
	return true;
}


void PluginUi::CloseAllEditors(const CSoundFile &sndFile)
{
	for(const auto &mixPlugin : sndFile.m_MixPlugins)
	{
		if(mixPlugin.pMixPlugin)
		{
			PluginUi(*mixPlugin.pMixPlugin).CloseEditor();
			g_pluginPrograms.erase(mixPlugin.pMixPlugin);
		}
	}
}


void PluginUi::DestroyPlugin(SNDMIXPLUGIN &mixPlugin)
{
	if(mixPlugin.pMixPlugin)
	{
		PluginUi(*mixPlugin.pMixPlugin).CloseEditor();
		g_pluginPrograms.erase(mixPlugin.pMixPlugin);
	}
	TrackerCriticalSection cs;
	mixPlugin.Destroy();
}


void PluginUi::OnEditorDestroyed(const CAbstractVstEditor &editor)
{
	for(auto it = g_pluginEditors.begin(); it != g_pluginEditors.end(); ++it)
	{
		if(it->second == &editor)
		{
			g_pluginEditors.erase(it);
			return;
		}
	}
}

void PluginUi::RegisterTrackerPlugins(CVstPluginManager &manager)
{
	auto &pluginList = manager.*PluginListAccess::FindPluginList();
	auto &midiPlugin = pluginList.emplace_back(std::make_unique<VSTPluginLib>(MidiInOut::Create, true, mpt::PathString(), P_("MIDI Input Output")));
	midiPlugin->pluginId1 = PLUGMAGIC('V', 'S', 't', 'P');
	midiPlugin->pluginId2 = PLUGMAGIC('M', 'M', 'I', 'D');
	midiPlugin->category = PluginCategory::Synth;
	midiPlugin->isInstrument = true;
}


mpt::ustring PluginUi::GetLibraryTags(const VSTPluginLib &library)
{
	const auto tags = g_libraryTags.find(&library);
	return tags != g_libraryTags.end() ? tags->second : mpt::ustring();
}


void PluginUi::SetLibraryTags(const VSTPluginLib &library, mpt::ustring tags)
{
	g_libraryTags[&library] = std::move(tags);
}


mpt::ustring PluginUi::GetLibraryVendor(const VSTPluginLib &library)
{
	if(library.isBuiltIn && library.pluginId1 != kDmoMagic)
		return U_("OpenMPT Project");
	return mpt::ustring();
}


OPENMPT_NAMESPACE_END
