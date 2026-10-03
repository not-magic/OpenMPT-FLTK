// Editor-side IMixPlugin interface from openmpt/soundlib/plugins/PlugInterface.h,
// which libopenmpt only builds for the tracker.

#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../soundlib/Snd_defs.h"
#include "../soundlib/plugins/PluginStructs.h"

#include <utility>

OPENMPT_NAMESPACE_BEGIN

class CAbstractVstEditor;
class CModDoc;
class CSoundFile;
class CVstPluginManager;
class IMixPlugin;
struct VSTPluginLib;

class PluginUi
{
public:
	explicit PluginUi(IMixPlugin &plugin) : m_plugin(plugin) { }
	explicit PluginUi(const IMixPlugin &plugin) : m_plugin(const_cast<IMixPlugin &>(plugin)) { }

	CModDoc *GetModDoc() const;
	PLUGINDEX GetSlot() const;
	// Moves the plugin to another slot of m_MixPlugins; the caller moves the SNDMIXPLUGIN data
	void SetSlot(PLUGINDEX slotIndex) const;

	mpt::ustring GetDefaultEffectName() const;
	std::pair<PlugParamValue, PlugParamValue> GetParamUIRange(PlugParamIndex paramIndex) const;
	PlugParamValue GetScaledUIParam(PlugParamIndex paramIndex) const;
	void SetScaledUIParam(PlugParamIndex paramIndex, PlugParamValue value) const;
	mpt::ustring GetParamName(PlugParamIndex paramIndex) const;
	mpt::ustring GetParamLabel(PlugParamIndex paramIndex) const;
	mpt::ustring GetParamDisplay(PlugParamIndex paramIndex) const;
	mpt::ustring GetFormattedParamName(PlugParamIndex paramIndex) const;
	mpt::ustring GetFormattedParamValue(PlugParamIndex paramIndex) const;

	int32 GetNumPrograms() const;
	int32 GetCurrentProgram() const;
	void SetCurrentProgram(int32 programIndex) const;
	mpt::ustring GetCurrentProgramName() const;
	void SetCurrentProgramName(const mpt::ustring &name) const;
	mpt::ustring GetProgramName(int32 programIndex) const;
	mpt::ustring GetFormattedProgramName(int32 index) const;

	bool HasEditor() const;
	CAbstractVstEditor *GetEditor() const;
	void ToggleEditor() const;
	void CloseEditor() const;
	void SetEditorPos(int32 x, int32 y) const;
	void GetEditorPos(int32 &x, int32 &y) const;

	// Notify OpenMPT that a parameter was changed from a plugin GUI and set the document as modified
	void AutomateParameter(PlugParamIndex paramIndex) const;
	void SetModified() const;

	bool SaveProgram() const;
	bool LoadProgram(mpt::PathString fileName = mpt::PathString()) const;

	// Editors must be closed before the plugins of a module are released
	static void CloseAllEditors(const CSoundFile &sndFile);
	static void DestroyPlugin(SNDMIXPLUGIN &mixPlugin);
	static void OnEditorDestroyed(const CAbstractVstEditor &editor);

	// Adds the plugins that only exist in the tracker, such as MIDI Input / Output
	static void RegisterTrackerPlugins(CVstPluginManager &manager);
	// User tags and vendor of a plugin library, which libopenmpt does not store
	static mpt::ustring GetLibraryTags(const VSTPluginLib &library);
	static void SetLibraryTags(const VSTPluginLib &library, mpt::ustring tags);
	static mpt::ustring GetLibraryVendor(const VSTPluginLib &library);

private:
	IMixPlugin &m_plugin;
};

OPENMPT_NAMESPACE_END
