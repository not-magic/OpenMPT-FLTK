/*
 * Vstplug.h
 * ---------
 * Purpose: Plugin handling (loading and processing plugins)
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/Vstplug.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#ifdef MPT_WITH_VST

#include "../soundlib/Snd_defs.h"
#include "../soundlib/plugins/PlugInterface.h"
#include "../soundlib/Mixer.h"
#include <VstDefinitions.h>
#include "plugins/VstEventQueue.h"


OPENMPT_NAMESPACE_BEGIN


class CTrackerSoundFile;
struct SNDMIXPLUGIN;
struct VSTPluginLib;
enum class PluginCategory : uint8;


class CVstPlugin final : public IMidiPlugin
{
protected:

	const bool m_maskCrashes;
	HMODULE m_hLibrary = nullptr;
	Vst::AEffect &m_Effect;
	Vst::ProcessProc m_pProcessFP = nullptr;  // Function pointer to AEffect processReplacing if supported, else process.

	double lastBarStartPos = -1.0;
	uint32 m_nSampleRate;

	bool m_isVst2 : 1;
	bool m_isInstrument : 1;
	bool m_isInitialized : 1;
	bool m_needIdle : 1;
	bool m_positionChanged : 1;

	VstEventQueue vstEvents;  // MIDI events that should be sent to the plugin

	Vst::VstTimeInfo timeInfo{};

public:

	const bool isBridged : 1;  // True if our built-in plugin bridge is being used.

private:

	ExceptionHandler::Context m_Ectx;

public:
	bool MaskCrashes() noexcept;

public:
	template <typename Tfn> static uint32 SETryOrError(bool maskCrashes, Tfn fn);

private:
	template <typename Tfn> uint32 SETryOrError(Tfn fn);

public:
	CVstPlugin(bool maskCrashes, HMODULE hLibrary, VSTPluginLib &factory, SNDMIXPLUGIN &mixPlugin, Vst::AEffect &effect, CTrackerSoundFile &sndFile);
	~CVstPlugin();

	CVstPlugin(const CVstPlugin &) = delete;
	CVstPlugin &operator=(const CVstPlugin &) = delete;

	enum class BridgeMode
	{
		Automatic,
		ForceBridgeWithFallback,
		DetectRequiredBridgeMode,
	};

	struct LoadResult
	{
		struct ShellPlugin
		{
			std::string name;
			uint32 shellPluginID = 0;
		};

		Vst::AEffect *effect = nullptr;
		Vst::MainProc mainProc = nullptr;
		HMODULE library = nullptr;
		int32 magic = 0, uniqueID = 0;
		std::vector<ShellPlugin> shellPlugins;
	};

	static LoadResult LoadPlugin(bool maskCrashes, VSTPluginLib &plugin, BridgeMode bridgeMode, unsigned long &exception);
	static bool SelectShellPlugin(bool maskCrashes, LoadResult &loadResult, const VSTPluginLib &plugin);
	static void GetPluginMetadata(bool maskCrashes, LoadResult &loadResult, VSTPluginLib &plugin);

protected:
	static std::pair<Vst::AEffect *, Vst::MainProc> LoadPluginInternal(bool maskCrashes, VSTPluginLib &plugin, HMODULE &library, BridgeMode bridgeMode);

	void Initialize();

public:
	int32 GetUID() const override;
	int32 GetVersion() const override;
	void Idle() override;
	uint32 GetLatency() const override { return m_Effect.initialDelay; }

	// Check if programs should be stored as chunks or parameters
	bool ProgramsAreChunks() const override { return (m_Effect.flags & Vst::effFlagsProgramChunks) != 0; }
	ChunkData GetChunk(bool isBank) override;
	void SetChunk(const ChunkData &chunk, bool isBank) override;
	// Old JUCE versions set effFlagsNoSoundInStop even when the shouldn't (see various ValhallaDSP reverb plugins). While the user cannot change the plugin bypass setting manually yet, play safe with VST plugins and do not optimize.
	bool ShouldProcessSilence() override { return true; }

	int32 GetNumPrograms() const override;
	int32 GetCurrentProgram() override;
	void SetCurrentProgram(int32 nIndex) override;

	PlugParamIndex GetNumParameters() const override;
	PlugParamValue GetParameter(PlugParamIndex nIndex) override;
	void SetParameter(PlugParamIndex nIndex, PlugParamValue fValue, PlayState * = nullptr, CHANNELINDEX = CHANNELINDEX_INVALID) override;

	mpt::ustring GetCurrentProgramName() override;
	void SetCurrentProgramName(const mpt::ustring &name) override;
	mpt::ustring GetProgramName(int32 program) override;

	mpt::ustring GetParamName(PlugParamIndex param) override;
	mpt::ustring GetParamLabel(PlugParamIndex param) override { return GetParamPropertyString(param, Vst::effGetParamLabel); };
	mpt::ustring GetParamDisplay(PlugParamIndex param) override { return GetParamPropertyString(param, Vst::effGetParamDisplay); };

	static intptr_t DispatchSEH(bool maskCrashes, Vst::AEffect &effect, Vst::VstOpcodeToPlugin opCode, int32 index, intptr_t value, void *ptr, float opt, unsigned long &exception);
	intptr_t Dispatch(Vst::VstOpcodeToPlugin opCode, int32 index, intptr_t value, void *ptr, float opt);

	bool HasEditor() const override { return (m_Effect.flags & Vst::effFlagsHasEditor) != 0; }
	CAbstractVstEditor *OpenEditor() override;
	mpt::ustring GetDefaultEffectName() override;

	void Bypass(bool bypass = true) override;

	static bool IsInstrument(Vst::AEffect &effect);
	bool IsInstrument() const override;
	bool CanRecieveMidiEvents() override;

	void CacheProgramNames(int32 firstProg, int32 lastProg) override;
	void CacheParameterNames(int32 firstParam, int32 lastParam) override;

public:
	void SaveAllParameters() override;
	void RestoreAllParameters(int32 program) override;
	void Process(float *pOutL, float *pOutR, uint32 numFrames) override;
	using IMixPlugin::MidiSend;
	bool MidiSend(mpt::const_byte_span midiData) override;
	void HardAllNotesOff() override;
	void NotifySongPlaying(bool playing) override;

	void Resume() override;
	void Suspend() override;
	void PositionChanged() override;

	// Check whether a VST parameter can be automated
	bool CanAutomateParameter(PlugParamIndex index);

	int GetNumInputChannels() const override { return m_Effect.numInputs; }
	int GetNumOutputChannels() const override { return m_Effect.numOutputs; }

	void BeginSetProgram(int32 program) override;
	void EndSetProgram() override;
	void BeginGetProgram(int32 program) override;
	void EndGetProgram() override;

protected:
	// Helper function for retreiving parameter name / label / display
	mpt::ustring GetParamPropertyString(PlugParamIndex param, Vst::VstOpcodeToPlugin opcode);

	// Set up input / output buffers.
	bool InitializeIOBuffers();

	// Process incoming and outgoing VST events.
	void ProcessVSTEvents();
	void ReceiveVSTEvents(const Vst::VstEvents *events);

	void ReportPlugException(const mpt::ustring &text) const;

public:
	static intptr_t VSTCALLBACK MasterCallBack(Vst::AEffect *effect, Vst::VstOpcodeToHost opcode, int32 index, intptr_t value, void *ptr, float opt);
protected:
	intptr_t VstFileSelector(bool destructor, Vst::VstFileSelect &fileSel);
};


OPENMPT_NAMESPACE_END

#endif // MPT_WITH_VST
