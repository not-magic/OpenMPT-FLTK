/*
 * MPTrack.h
 * ---------
 * Purpose: OpenMPT core application class.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/Mptrack.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "../common/mptRandom.h"
#include "../misc/mptMutex.h"
#include "../soundlib/MIDIMacros.h"
#include "../soundlib/modcommand.h"

#include <future>

OPENMPT_NAMESPACE_BEGIN

class CModDoc;
class CModDocTemplate;
class CVstPluginManager;
struct UpdateHint;
namespace SoundDevice
{
class Manager;
}  // namespace SoundDevice
struct AllSoundDeviceComponents;
class CDLSBank;
class DebugSettings;
class TrackerSettings;
class CachedIniFileSettingsBackend;
template <typename Backend> class FileSettingsContainer;
using IniFileSettingsBackend = CachedIniFileSettingsBackend;
using IniFileSettingsContainer = FileSettingsContainer<CachedIniFileSettingsBackend>;
class SettingsContainer;
namespace mpt
{
namespace Wine
{
class VersionContext;
class Context;
}  // namespace Wine
}  // namespace mpt


/////////////////////////////////////////////////////////////////////////////
// 16-colors DIB, one byte per pixel with the palette index in the low four bits, top row first
struct MODPLUGDIB
{
	int width = 0;
	int height = 0;
	ColorRef palette[16] = {};
	std::vector<uint8> pixels;
};


/////////////////////////////////////////////////////////////////////////////
// Midi Library

// 128 melodic instruments + 128 percussions
// std::nullopt = default, empty string = unassigned by user
using MidiLibrary = std::array<std::optional<mpt::PathString>, 128 * 2>;


//////////////////////////////////////////////////////////////////////////
// Dragon Droppings

enum DragonDropType
{
	DRAGONDROP_NOTHING = 0,  // |------< Drop Type >-------------|---< dropItem >----|---< dropParam >---|
	DRAGONDROP_DLS,          // | Instrument from a DLS bank     |     DLS Bank #    |   DLS Instrument  |
	DRAGONDROP_SAMPLE,       // | Sample from a song             |     Sample #      |       NULL        |
	DRAGONDROP_INSTRUMENT,   // | Instrument from a song         |     Instrument #  |       NULL        |
	DRAGONDROP_SOUNDFILE,    // | File from instrument library   |        ?          |     File Name     |
	DRAGONDROP_MIDIINSTR,    // | File from midi library         | Midi Program/Perc |     File Name     |
	DRAGONDROP_PATTERN,      // | Pattern from a song            |      Pattern #    |       NULL        |
	DRAGONDROP_ORDER,        // | Pattern index in a song        |       Order #     |       NULL        |
	DRAGONDROP_SONG,         // | Song file (mod/s3m/xm/it)      |       0           |     File Name     |
	DRAGONDROP_SEQUENCE      // | Sequence (a set of orders)     |    Sequence #     |       NULL        |
};

struct DRAGONDROP
{
	enum class InsertType
	{
		Unspecified,
		Replace,
		InsertNew
	};

	const CTrackerSoundFile *sndFile = nullptr;
	DragonDropType dropType = DRAGONDROP_NOTHING;
	InsertType insertType = InsertType::Unspecified;
	uint32 dropItem = 0;
	LParam dropParam = 0;

	mpt::PathString GetPath() const
	{
		const mpt::PathString *const path = reinterpret_cast<const mpt::PathString *>(dropParam);
		MPT_ASSERT(path);
		return path ? *path : mpt::PathString();
	}
};


/////////////////////////////////////////////////////////////////////////////
// CTrackApp:
// See mptrack.cpp for the implementation of this class
//

class CMPTCommandLineInfo;

class CTrackApp : public AppBase
{
	friend class CMainFrame;
	// static data
protected:
	static MODTYPE m_nDefaultDocType;
	static MidiLibrary midiLibrary;

public:
	static std::vector<std::unique_ptr<CDLSBank>> gpDLSBanks;

protected:

	mpt::recursive_mutex_with_lock_count m_GlobalMutex;

	mpt::log::GlobalLogger m_GlobalLogger{};

	mpt::PathString m_InstallPath;         // i.e. "C:\Program Files\OpenMPT\" (installer mode) or "G:\OpenMPT\" (portable mode)
	mpt::PathString m_InstallBinPath;      // i.e. "C:\Program Files\OpenMPT\bin\" (multi-arch mode) or InstallPath (legacy mode)
	mpt::PathString m_InstallBinArchPath;  // i.e. "C:\Program Files\OpenMPT\bin\amd64\" (multi-arch mode) or InstallPath (legacy mode)
	mpt::PathString m_InstallPkgPath;      // i.e. "C:\Program Files\OpenMPT\" (installer mode) or "G:\OpenMPT\" (portable mode)

	mpt::PathString m_ConfigPath;  // InstallPath (portable mode) or "%AppData%\OpenMPT\"

	mpt::PathString m_szConfigFileName;
	mpt::PathString m_PluginStateFileName;
	mpt::PathString m_szPluginCacheFileName;

	bool m_bInstallerMode = false;
	bool m_bPortableMode = false;
	bool m_bSourceTreeMode = false;

	uint32 m_GuiThreadId = 0;

	std::unique_ptr<mpt::random_device> m_RD;
	std::unique_ptr<mpt::thread_safe_prng<mpt::default_prng>> m_PRNG;

	std::unique_ptr<IniFileSettingsBackend> m_pSettingsIniFile;
	std::unique_ptr<SettingsContainer> m_pSettings;
	std::unique_ptr<DebugSettings> m_pDebugSettings;
	std::unique_ptr<TrackerSettings> m_pTrackerSettings;

	std::unique_ptr<IniFileSettingsBackend> m_pSongSettingsIniFile;
	std::unique_ptr<SettingsContainer> m_pSongSettings;

	std::unique_ptr<IniFileSettingsContainer> m_pPluginState;
	std::unique_ptr<IniFileSettingsContainer> m_pPluginCache;


	CModDocTemplate *m_pModTemplate = nullptr;  // owned by AppBase

	std::unique_ptr<AllSoundDeviceComponents> m_pAllSoundDeviceComponents;
	std::unique_ptr<SoundDevice::Manager> m_pSoundDevicesManager;

	std::future<std::vector<std::unique_ptr<CDLSBank>>> m_scannedDlsBanks;
	std::atomic<bool> m_scannedDlsBanksAvailable = false;

	CVstPluginManager *m_pPluginManager = nullptr;

	// Default macro configuration
	MIDIMacroConfig m_MidiCfg;

	uint32 m_dwLastPluginIdleCall = 0;

public:
	CTrackApp();
	~CTrackApp();

	CModDoc *NewDocument(MODTYPE newType = MOD_TYPE_NONE);
	Document *OpenTemplateFile(const mpt::PathString &file) const;
	void AddToRecentFileList(const mpt::PathString &path) override;
	/// Removes item from MRU-list; most recent item has index zero.
	void RemoveMruItem(const size_t item);
	void RemoveMruItem(const mpt::PathString &path);

	int GetOpenDocumentCount() const;
	std::vector<CModDoc *> GetOpenDocuments() const;

public:
	bool IsMultiArchInstall() const { return m_InstallPath == m_InstallBinArchPath; }
	mpt::PathString GetInstallPath() const { return m_InstallPath; }                // i.e. "C:\Program Files\OpenMPT\" (installer mode) or "G:\OpenMPT\" (portable mode)
	mpt::PathString GetInstallBinPath() const { return m_InstallBinPath; }          // i.e. "C:\Program Files\OpenMPT\bin\" (multi-arch mode) or InstallPath (legacy mode)
	mpt::PathString GetInstallBinArchPath() const { return m_InstallBinArchPath; }  // i.e. "C:\Program Files\OpenMPT\bin\amd64\" (multi-arch mode) or InstallPath (legacy mode)
	mpt::PathString GetInstallPkgPath() const { return m_InstallPkgPath; }          // i.e. "C:\Program Files\OpenMPT\" (installer mode) or "G:\OpenMPT\" (portable mode)

	static MODTYPE GetDefaultDocType() { return m_nDefaultDocType; }
	static void SetDefaultDocType(MODTYPE n) { m_nDefaultDocType = n; }
	static MidiLibrary &GetMidiLibrary() { return midiLibrary; }
	static void ImportMidiConfig(const mpt::PathString &filename, bool hideWarning = false);
	static void ExportMidiConfig(const mpt::PathString &filename);
	static void ImportMidiConfig(SettingsContainer &file, const mpt::PathString &path, bool forgetSettings = false);
	static void ExportMidiConfig(SettingsContainer &file);
	static std::future<std::vector<std::unique_ptr<CDLSBank>>> LoadDefaultDLSBanks();
	static void SaveDefaultDLSBanks();
	static void RemoveDLSBank(uint32 nBank);
	static bool AddDLSBank(const mpt::PathString &filename);
	static bool OpenURL(const char *url);         // UTF8
	static bool OpenURL(const std::string &url);  // UTF8
	static bool OpenURL(const mpt::ustring &url);
	static bool OpenURL(const mpt::PathString &url);
	static bool OpenFile(const mpt::PathString &file) { return OpenURL(file); };
	static bool OpenDirectory(const mpt::PathString &directory);

	// Retrieve the user-supplied MIDI port name for a MIDI input or output port.
	mpt::ustring GetFriendlyMIDIPortName(const mpt::ustring &deviceName, bool isInputPort, bool addDeviceName = true);

	void UpdateAllViews(UpdateHint hint, HintObject *pHint = nullptr);
	void PostMessageToAllViews(uint32 uMsg, WParam wParam = 0, LParam lParam = 0);

public:
	inline mpt::recursive_mutex_with_lock_count &GetGlobalMutexRef() { return m_GlobalMutex; }
	bool InGuiThread() const { return mpt::log::Trace::GetCurrentThreadId() == m_GuiThreadId; }
	mpt::random_device &RandomDevice() { return *m_RD; }
	mpt::thread_safe_prng<mpt::default_prng> &PRNG() { return *m_PRNG; }
	CModDocTemplate *GetModDocTemplate() const { return m_pModTemplate; }
	CVstPluginManager *GetPluginManager() const { return m_pPluginManager; }
	SoundDevice::Manager *GetSoundDevicesManager() const { return m_pSoundDevicesManager.get(); }
	void GetDefaultMidiMacro(MIDIMacroConfig &cfg) const { cfg = m_MidiCfg; }
	void SetDefaultMidiMacro(const MIDIMacroConfig &cfg) { m_MidiCfg = cfg; }
	mpt::PathString GetConfigDirectory() const { return m_ConfigPath; }
	mpt::PathString GetConfigFileName() const { return m_szConfigFileName; }
	SettingsContainer *GetpSettings()
	{
		return m_pSettings.get();
	}
	SettingsContainer &GetSettings()
	{
		MPT_ASSERT(m_pSettings);
		return *m_pSettings;
	}
	TrackerSettings &GetTrackerSettings()
	{
		MPT_ASSERT(m_pTrackerSettings);
		return *m_pTrackerSettings;
	}
	bool IsInstallerMode() const
	{
		return m_bInstallerMode;
	}
	bool IsPortableMode() const
	{
		return m_bPortableMode;
	}
	bool IsSourceTreeMode() const
	{
		return m_bSourceTreeMode;
	}
	mpt::ustring GetInstallationMode() const
	{
		if(IsInstallerMode())
		{
			return MPT_USTRING("installer");
		}
		if(IsPortableMode())
		{
			return MPT_USTRING("portable");
		}
		if(IsSourceTreeMode())
		{
			return MPT_USTRING("sourcetree");
		}
		return MPT_USTRING("unknown");
	}

	SettingsContainer &GetPluginState();
	SettingsContainer &GetPluginCache();
	SettingsContainer &GetSongSettings();
	const mpt::PathString &GetSongSettingsFilename() const;

	/// Returns path to config folder including trailing '\'.
	mpt::PathString GetConfigPath() const { return m_ConfigPath; }
	mpt::PathString GetUserTemplatesPath() const;
	mpt::PathString GetExampleSongsPath() const;
	void SetupPaths(bool overridePortable);
	void CreatePaths();


	// Relative / absolute paths conversion
	mpt::PathString PathAbsoluteToInstallRelative(const mpt::PathString &path) { return mpt::AbsolutePathToRelative(path, GetInstallPath()); }
	mpt::PathString PathInstallRelativeToAbsolute(const mpt::PathString &path) { return mpt::RelativePathToAbsolute(path, GetInstallPath()); }
	mpt::PathString PathAbsoluteToInstallBinArchRelative(const mpt::PathString &path) { return mpt::AbsolutePathToRelative(path, GetInstallBinArchPath()); }
	mpt::PathString PathInstallBinArchRelativeToAbsolute(const mpt::PathString &path) { return mpt::RelativePathToAbsolute(path, GetInstallBinArchPath()); }

	static void OpenModulesDialog(std::vector<mpt::PathString> &files, const mpt::PathString &overridePath = mpt::PathString());

public:
	// Get name of resampling mode. addTaps = true also adds the number of taps the filter uses.
	static mpt::ustring GetResamplingModeName(ResamplingMode mode, int length, bool addTaps);

	// Overrides
protected:
	bool InitInstance() override;
	bool InitInstanceEarly(CMPTCommandLineInfo &cmdInfo);
	bool InitInstanceLate(CMPTCommandLineInfo &cmdInfo);
	bool InitInstanceImpl(CMPTCommandLineInfo &cmdInfo);
	int Run() override;
	int ExitInstance() override;
	int ExitInstanceImpl();
	bool OnIdle(int idleCount) override;

	// Implementation
	void OnFileNew() { NewDocument(); }
	void OnFileNewMOD_Amiga() { NewDocument(MOD_TYPE_MOD); }
	void OnFileNewMOD_PC() { NewDocument(MOD_TYPE_MOD_PC); }
	void OnFileNewS3M() { NewDocument(MOD_TYPE_S3M); }
	void OnFileNewXM() { NewDocument(MOD_TYPE_XM); }
	void OnFileNewIT() { NewDocument(MOD_TYPE_IT); }
	void OnFileNewMPT() { NewDocument(MOD_TYPE_MPT); }

	void OnFileOpen();
	void OnAppAbout();

	void OnFileCloseAll();
	void OnUpdateAnyDocsOpen(CmdUI *cmd);

	UI_DECLARE_MESSAGE_MAP()

	size_t AddScannedDLSBanks();

	void InitializeDXPlugins();
	void UninitializeDXPlugins();

	bool MoveConfigFile(const mpt::PathString &fileName, mpt::PathString subDir = {}, mpt::PathString newFileName = {});

};


extern CTrackApp theApp;


//////////////////////////////////////////////////////////////////
// More Bitmap Helpers

class CFastBitmap
{
protected:
	static constexpr uint8 BLEND_OFFSET = 0x80;

	std::vector<uint8> m_pixels;
	std::vector<uint8> m_rgbxPixels;
	ColorRef m_palette[256] = {};
	uint8 m_paletteRgbx[256][4] = {};
	int m_width = 0;
	int m_height = 0;
	uint32 m_nTextColor = 0, m_nBkColor = 0;
	const MODPLUGDIB *m_pTextDib = nullptr;
	uint8 m_nBlendOffset = 0;
	bool m_isPaletteDirty = true;
	uint8 m_n4BitPalette[16] = {{}};
	uint8 m_nXShiftFactor = 0;

public:
	CFastBitmap() = default;

public:
	void Init(const MODPLUGDIB *textDib = nullptr);
	void Blit(ui::Painter &painter, int x, int y, int cx, int cy);
	void Blit(ui::Painter &painter, const Rect &rect) { Blit(painter, rect.left, rect.top, rect.Width(), rect.Height()); }
	void SetTextColor(int nText, int nBk = -1)
	{
		m_nTextColor = nText;
		if(nBk >= 0)
			m_nBkColor = nBk;
	}
	void SetTextBkColor(uint32 nBk) { m_nBkColor = nBk; }
	void SetColor(uint32 nIndex, ColorRef cr);
	void SetAllColors(uint32 nBaseIndex, uint32 nColors, ColorRef *pcr);
	void TextBlt(int x, int y, int cx, int cy, int srcx, int srcy, const MODPLUGDIB *dib = nullptr);
	void SetBlendMode(bool enable) { m_nBlendOffset = enable ? BLEND_OFFSET : 0; }
	bool GetBlendMode() const { return m_nBlendOffset != 0; }
	void SetBlendColor(ColorRef cr);
	void SetSize(int x, int y);
	int GetWidth() const { return m_width; }
};


///////////////////////////////////////////////////
// 4-bit DIB Drawing functions
void DibBlt(ui::Painter &painter, int x, int y, int sizex, int sizey, int srcx, int srcy, const MODPLUGDIB *dib);
// Loads a 4-bit bitmap resource
std::unique_ptr<MODPLUGDIB> LoadDib(uint32 resourceId);

// Other bitmap functions
void DrawButtonRect(ui::Painter &painter, int lineWidth, const Rect &rect, const mpt::ustring &text = {}, bool disabled = false, bool pushed = false, uint32 textFormat = (ui::TextCenter | ui::TextVCenter), uint32 topMargin = 0);
void DrawButtonRect(ui::Painter &painter, int lineWidth, const ui::Font &font, const Rect &rect, const mpt::ustring &text = {}, bool disabled = false, bool pushed = false, uint32 textFormat = (ui::TextCenter | ui::TextVCenter), uint32 topMargin = 0);

// Misc functions
void ErrorBox(uint32 nStringID, Wnd *p = nullptr);

// Append note names in range [noteStart, noteEnd] to given combobox. Index starts from 0.
void AppendNotesToControl(ComboBox &combobox, ModCommand::NOTE noteStart, ModCommand::NOTE noteEnd);

// Append note names to combo box.
// If nInstr is given, instrument-specific note names are used instead of default note names.
// A custom note range may also be specified using the noteStart and noteEnd parameters.
// If they are left out, only notes that are available in the module type, plus any supported "special notes" are added.
void AppendNotesToControlEx(ComboBox &combobox, const CTrackerSoundFile &sndFile, INSTRUMENTINDEX nInstr = MAX_INSTRUMENTS, ModCommand::NOTE noteStart = 0, ModCommand::NOTE noteEnd = 0);

// Get window text (e.g. edit box content) as a mpt::ustring
mpt::ustring GetWindowTextString(const Wnd &wnd);

mpt::ustring FormatFileSize(uint64 fileSize);

mpt::ustring FormatOrderRow(uint32 value);

bool ValidateMacroString(Edit &wnd, const std::string_view prevMacro, bool isParametric, bool allowVariables, bool allowMultiline);

mpt::ustring ConstructSampleFormatFileFilter(bool includeRaw);


///////////////////////////////////////////////////
// Tables

extern const mpt::uchar *szSpecialNoteNamesMPT[];
extern const mpt::uchar *szSpecialNoteShortDesc[];
extern const char *szHexChar;

// Defined in load_mid.cpp
extern const char *szMidiProgramNames[128];
extern const char *szMidiPercussionNames[61];  // notes 25..85
extern const char *szMidiGroupNames[17];       // 16 groups + Percussions

/////////////////////////////////////////////////////////////////////////////



OPENMPT_NAMESPACE_END
