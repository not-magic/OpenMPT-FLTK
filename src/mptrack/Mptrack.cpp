/*
 * MPTrack.cpp
 * -----------
 * Purpose: OpenMPT core application class.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/Mptrack.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "Mptrack.h"
#include "AboutDialog.h"
#include "AutoSaver.h"
#include "Childfrm.h"
#include "CloseMainDialog.h"
#include "DialogBase.h"
#include "FileDialog.h"
#include "FolderScanner.h"
#include "Globals.h"
#include "Image.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "ModDocTemplate.h"
#include "MPTrackUtil.h"
#include "PlugNotFoundDlg.h"
#include "Reporting.h"
#include "resource.h"
#include <FL/Fl_ICO_Image.H>
#include <FL/Fl_Window.H>
#include "SettingsIni.h"
#include "TrackerSettings.h"
#include "WelcomeDialog.h"
#include "mpt/random/crand.hpp"
#include "../common/mptStringBuffer.h"
#include "../common/version.h"
#include "../misc/mptOS.h"
#include "DlsBankExt.h"
#include "../soundlib/plugins/PluginManager.h"
#include "../test/test.h"
#include "mpt/arch/arch.hpp"
#include "mpt/io_file/outputfile.hpp"
#include "mpt/random/seed.hpp"
#include "mpt/string/utility.hpp"
#include "openmpt/sounddevice/SoundDeviceManager.hpp"
#include "PluginUi.h"

#include <filesystem>
#include <thread>

#include <spawn.h>

extern char **environ;




OPENMPT_NAMESPACE_BEGIN

/////////////////////////////////////////////////////////////////////////////
// The one and only CTrackApp object

CTrackApp theApp;

const mpt::uchar *szSpecialNoteNamesMPT[] = {UL_("PCs"), UL_("PC"), UL_("~~ (Note Fade)"), UL_("^^ (Note Cut)"), UL_("== (Note Off)")};
const mpt::uchar *szSpecialNoteShortDesc[] = {UL_("Param Control (Smooth)"), UL_("Param Control"), UL_("Note Fade"), UL_("Note Cut"), UL_("Note Off")};

// Make sure that special note arrays include string for every note.
static_assert(NOTE_MAX_SPECIAL - NOTE_MIN_SPECIAL + 1 == mpt::array_size<decltype(szSpecialNoteNamesMPT)>::size);
static_assert(mpt::array_size<decltype(szSpecialNoteShortDesc)>::size == mpt::array_size<decltype(szSpecialNoteNamesMPT)>::size);

const char *szHexChar = "0123456789ABCDEF";




#if defined(MPT_WITH_PORTAUDIO)
class ComponentPortAudio
	: public ComponentBuiltin
{
	MPT_DECLARE_COMPONENT_MEMBERS(ComponentPortAudio, "PortAudio")
public:
	ComponentPortAudio() = default;
	virtual ~ComponentPortAudio() = default;
};
#endif // MPT_WITH_PORTAUDIO

#if defined(MPT_WITH_PULSEAUDIO)
class ComponentPulseaudio
	: public ComponentBuiltin
{
	MPT_DECLARE_COMPONENT_MEMBERS(ComponentPulseaudio, "Pulseaudio")
public:
	ComponentPulseaudio() = default;
	virtual ~ComponentPulseaudio() = default;
};
#endif // MPT_WITH_PULSEAUDIO

#if defined(MPT_WITH_PULSEAUDIO) && defined(MPT_WITH_PULSEAUDIOSIMPLE)
class ComponentPulseaudioSimple
	: public ComponentBuiltin
{
	MPT_DECLARE_COMPONENT_MEMBERS(ComponentPulseaudioSimple, "PulseaudioSimple")
public:
	ComponentPulseaudioSimple() = default;
	virtual ~ComponentPulseaudioSimple() = default;
};
#endif // MPT_WITH_PULSEAUDIO && MPT_WITH_PULSEAUDIOSIMPLE

#if defined(MPT_WITH_RTAUDIO)
class ComponentRtAudio
	: public ComponentBuiltin
{
	MPT_DECLARE_COMPONENT_MEMBERS(ComponentRtAudio, "RtAudio")
public:
	ComponentRtAudio() = default;
	virtual ~ComponentRtAudio() = default;
};
#endif // MPT_WITH_RTAUDIO


struct AllSoundDeviceComponents
{
#if defined(MPT_WITH_PULSEAUDIO) && defined(MPT_ENABLE_PULSEAUDIO_FULL)
	ComponentHandle<ComponentPulseaudio> m_Pulseaudio;
#endif // MPT_WITH_PULSEAUDIO && MPT_ENABLE_PULSEAUDIO_FULL
#if defined(MPT_WITH_PULSEAUDIO) && defined(MPT_WITH_PULSEAUDIOSIMPLE)
	ComponentHandle<ComponentPulseaudioSimple> m_PulseaudioSimple;
#endif // MPT_WITH_PULSEAUDIO && MPT_WITH_PULSEAUDIOSIMPLE
#ifdef MPT_WITH_PORTAUDIO
	ComponentHandle<ComponentPortAudio> m_PortAudio;
#endif // MPT_WITH_PORTAUDIO
#ifdef MPT_WITH_RTAUDIO
	ComponentHandle<ComponentRtAudio> m_RtAudio;
#endif // MPT_WITH_RTAUDIO
	operator SoundDevice::EnabledBackends() const
	{
		SoundDevice::EnabledBackends result;
#if defined(MPT_WITH_PULSEAUDIO) && defined(MPT_ENABLE_PULSEAUDIO_FULL)
		result.Pulseaudio = IsComponentAvailable(m_Pulseaudio);
#endif // MPT_WITH_PULSEAUDIO && MPT_ENABLE_PULSEAUDIO_FULL
#if defined(MPT_WITH_PULSEAUDIO) && defined(MPT_WITH_PULSEAUDIOSIMPLE)
		result.PulseaudioSimple = IsComponentAvailable(m_PulseaudioSimple);
#endif // MPT_WITH_PULSEAUDIO && MPT_WITH_PULSEAUDIOSIMPLE
#ifdef MPT_WITH_PORTAUDIO
		result.PortAudio = IsComponentAvailable(m_PortAudio);
#endif // MPT_WITH_PORTAUDIO
#ifdef MPT_WITH_RTAUDIO
		result.RtAudio = IsComponentAvailable(m_RtAudio);
#endif // MPT_WITH_RTAUDIO
		return result;
	}
};


void CTrackApp::OnFileCloseAll()
{
	if(!(TrackerSettings::Instance().patternSetup & PatternSetup::NoCustomCloseDialog))
	{
		// Show modified documents window
		CloseMainDialog dlg;
		if(dlg.DoModal() != IDOK)
		{
			return;
		}
	}

	for(auto &doc : GetOpenDocuments())
	{
		doc->SafeFileClose();
	}
}


void CTrackApp::OnUpdateAnyDocsOpen(CmdUI *cmd)
{
	cmd->Enable(GetModDocTemplate() != nullptr && !GetModDocTemplate()->empty());
}


int CTrackApp::GetOpenDocumentCount() const
{
	return GetModDocTemplate() ? static_cast<int>(GetModDocTemplate()->size()) : 0;
}


// Retrieve a list of all open modules.
std::vector<CModDoc *> CTrackApp::GetOpenDocuments() const
{
	std::vector<CModDoc *> documents;
	if(auto *pDocTmpl = GetModDocTemplate())
	{
		for(auto *doc : *pDocTmpl)
		{
			documents.push_back(doc);
		}
	}

	return documents;
}


void CTrackApp::UpdateAllViews(UpdateHint hint, HintObject *pHint)
{
	if(auto *pDocTmpl = GetModDocTemplate())
	{
		for(auto &doc : *pDocTmpl)
		{
			doc->UpdateAllViews(nullptr, hint, pHint);
		}
	}
}


void CTrackApp::PostMessageToAllViews(uint32 uMsg, WParam wParam, LParam lParam)
{
	if(auto *pDocTmpl = GetModDocTemplate())
	{
		for(auto &doc : *pDocTmpl)
		{
			doc->PostMessageToAllViews(uMsg, wParam, lParam);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
// Command Line options

class CMPTCommandLineInfo
{
public:
	std::vector<mpt::PathString> m_fileNames;
	bool m_showSplash = true;
	bool m_noDls = false, m_noPlugins = false, m_noAssembly = false, m_noSysCheck = false,
		m_portable = false, m_autoPlay = false;
#ifdef ENABLE_TESTS
	bool m_noTests = false;
#endif

public:
	explicit CMPTCommandLineInfo(const std::vector<std::string> &arguments)
	{
		std::error_code ec;
		const mpt::PathString workingDir = mpt::PathString::FromUTF8(std::filesystem::current_path(ec).string()).WithTrailingSlash();
		for(std::size_t i = 1; i < arguments.size(); ++i)
		{
			const std::string &argument = arguments[i];
			if(argument.size() > 1 && argument[0] == '-')
			{
				ParseFlag(mpt::ToLowerCaseAscii(argument.substr(argument[1] == '-' ? 2 : 1)));
			} else
			{
				m_fileNames.push_back(mpt::RelativePathToAbsolute(mpt::PathString::FromUTF8(argument), workingDir));
			}
		}
	}

private:
	void ParseFlag(const std::string &flag)
	{
		if(flag == "nologo") m_showSplash = false;
		else if(flag == "nodls") m_noDls = true;
		else if(flag == "noplugs") m_noPlugins = true;
		else if(flag == "portable") m_portable = true;
		else if(flag == "noassembly") m_noAssembly = true;
		else if(flag == "nosyscheck") m_noSysCheck = true;
		else if(flag == "play") m_autoPlay = true;
#ifdef ENABLE_TESTS
		else if(flag == "notests") m_noTests = true;
#endif
	}
};


// Splash Screen

static void StartSplashScreen();
static void StopSplashScreen();
static void TimeoutSplashScreen();


/////////////////////////////////////////////////////////////////////////////
// Midi Library

MidiLibrary CTrackApp::midiLibrary;

void CTrackApp::ImportMidiConfig(const mpt::PathString &filename, bool hideWarning)
{
	if(filename.empty()) return;

	if(CDLSBank::IsDLSBank(filename))
	{
		ConfirmAnswer result = cnfYes;
		if(!hideWarning)
		{
			result = Reporting::Confirm("You are about to replace the current MIDI library:\n"
				"Do you want to replace only the missing instruments? (recommended)",
				"Warning", true);
		}
		if(result == cnfCancel) return;
		const bool replaceAll = (result == cnfNo);
		CDLSBank dlsbank;
		if (dlsbank.Open(filename))
		{
			for(uint32 ins = 0; ins < 256; ins++)
			{
				if(replaceAll || !midiLibrary[ins] || midiLibrary[ins]->empty())
				{
					uint32 prog = (ins < 128) ? ins : 0xFF;
					uint32 key = (ins < 128) ? 0xFF : ins & 0x7F;
					if(dlsbank.FindInstrument(ins >= 128, 0xFFFF, prog, key))
					{
						midiLibrary[ins] = filename;
					}
				}
			}
		}
		return;
	}

	IniFileSettingsContainer file(filename);
	ImportMidiConfig(file, filename.GetDirectoryWithDrive());
}


static mpt::PathString GetUltraSoundPatchDir(SettingsContainer &file, const mpt::ustring &iniSection, const mpt::PathString &path, bool forgetSettings)
{
	mpt::PathString patchDir = file.Read<mpt::PathString>(iniSection, UL_("PatchDir"), {});
	if(forgetSettings)
		file.Forget(UL_("Ultrasound"), UL_("PatchDir"));
	if(patchDir.empty() || patchDir == P_(".\\"))
		patchDir = path;
	if(!patchDir.empty())
		patchDir = patchDir.WithTrailingSlash();
	return patchDir;
}

void CTrackApp::ImportMidiConfig(SettingsContainer &file, const mpt::PathString &path, bool forgetSettings)
{
	const mpt::PathString patchDir = GetUltraSoundPatchDir(file, UL_("Ultrasound"), path, forgetSettings);
	for(uint32 prog = 0; prog < 256; prog++)
	{
		mpt::ustring key = MPT_UFORMAT("{}{}")((prog < 128) ? UL_("Midi") : UL_("Perc"), prog & 0x7F);
		mpt::PathString filename = file.Read<mpt::PathString>(UL_("Midi Library"), key, mpt::PathString());
		// Check for ULTRASND.INI
		if(filename.empty())
		{
			mpt::ustring section = (prog < 128) ? UL_("Melodic Patches") : UL_("Drum Patches");
			key = mpt::ufmt::val(prog & 0x7f);
			filename = file.Read<mpt::PathString>(section, key, mpt::PathString());
			if(forgetSettings) file.Forget(section, key);
			if(filename.empty())
			{
				section = (prog < 128) ? UL_("Melodic Bank 0") : UL_("Drum Bank 0");
				filename = file.Read<mpt::PathString>(section, key, mpt::PathString());
				if(forgetSettings) file.Forget(section, key);
			}
			const mpt::PathString localPatchDir = GetUltraSoundPatchDir(file, section, patchDir, forgetSettings);
			if(!filename.empty())
			{
				filename = localPatchDir + filename + P_(".pat");
			}
		}

		if(filename == P_("*empty"))
			midiLibrary[prog] = P_("");
		else if(!filename.empty())
			midiLibrary[prog] = theApp.PathInstallRelativeToAbsolute(filename);
	}
}


void CTrackApp::ExportMidiConfig(const mpt::PathString &filename)
{
	if(filename.empty())
		return;
	IniFileSettingsContainer file(filename);
	ExportMidiConfig(file);
}

void CTrackApp::ExportMidiConfig(SettingsContainer &file)
{
	for(uint32 prog = 0; prog < 256; prog++)
	{
		const mpt::ustring key = MPT_UFORMAT("{}{}")((prog < 128) ? UL_("Midi") : UL_("Perc"), prog & 0x7F);
		if(!midiLibrary[prog])
		{
			file.Forget(UL_("Midi Library"), key);
			continue;
		}

		mpt::PathString fileName = *midiLibrary[prog];
		if(midiLibrary[prog]->empty())
			fileName = P_("*empty");
		else if(theApp.IsPortableMode())
			fileName = theApp.PathAbsoluteToInstallRelative(fileName);
		file.Write<mpt::PathString>(UL_("Midi Library"), key, fileName);
	}
}


/////////////////////////////////////////////////////////////////////////////
// DLS Banks support

std::vector<std::unique_ptr<CDLSBank>> CTrackApp::gpDLSBanks;


struct CompareLessPathStringNoCase
{
	inline bool operator()(const mpt::PathString &l, const mpt::PathString &r) const
	{
		return mpt::PathCompareNoCase(l, r) < 0;
	}
};

std::future<std::vector<std::unique_ptr<CDLSBank>>> CTrackApp::LoadDefaultDLSBanks()
{
	std::set<mpt::PathString, CompareLessPathStringNoCase> paths;

	uint32 numBanks = theApp.GetSettings().Read<uint32>(UL_("DLS Banks"), UL_("NumBanks"), 0);
	for(uint32 i = 0; i < numBanks; i++)
	{
		mpt::PathString path = theApp.GetSettings().Read<mpt::PathString>(UL_("DLS Banks"), MPT_UFORMAT("Bank{}")(i + 1), mpt::PathString());
		paths.insert(theApp.PathInstallRelativeToAbsolute(path));
	}

	if(paths.empty())
		return {};

	return std::async(std::launch::async, [paths = std::move(paths)]()
	{
		std::vector<std::unique_ptr<CDLSBank>> banks;
		banks.reserve(paths.size());
		for(const auto &filename : paths)
		{
			if(filename.empty() || !CDLSBank::IsDLSBank(filename))
				continue;
			try
			{
				auto bank = std::make_unique<CDLSBank>();
				if(bank->Open(filename))
				{
					banks.push_back(std::move(bank));
					continue;
				}
			} catch(mpt::out_of_memory e)
			{
				mpt::delete_out_of_memory(e);
			} catch(const std::exception &)
			{
			}
		}
		// Avoid the overhead of future::wait_for(0) until future::is_ready is finally non-experimental
		theApp.m_scannedDlsBanksAvailable = true;
		return banks;
	});
}


void CTrackApp::SaveDefaultDLSBanks()
{
	uint32 nBanks = 0;
	for(const auto &bank : gpDLSBanks)
	{
		if(!bank || bank->GetFileName().empty())
			continue;

		mpt::PathString path = bank->GetFileName();
		if(theApp.IsPortableMode())
		{
			path = theApp.PathAbsoluteToInstallRelative(path);
		}

		mpt::ustring key = MPT_UFORMAT("Bank{}")(nBanks + 1);
		theApp.GetSettings().Write<mpt::PathString>(UL_("DLS Banks"), key, path);
		nBanks++;

	}
	theApp.GetSettings().Write<uint32>(UL_("DLS Banks"), UL_("NumBanks"), nBanks);
}


void CTrackApp::RemoveDLSBank(uint32 nBank)
{
	if(nBank < gpDLSBanks.size())
		gpDLSBanks[nBank] = nullptr;
}


bool CTrackApp::AddDLSBank(const mpt::PathString &filename)
{
	if(filename.empty() || !CDLSBank::IsDLSBank(filename)) return false;
	// Check for dupes
	for(const auto &bank : gpDLSBanks)
	{
		if(bank && !mpt::PathCompareNoCase(filename, bank->GetFileName()))
			return true;
	}
	try
	{
		auto bank = std::make_unique<CDLSBank>();
		if(bank->Open(filename))
		{
			gpDLSBanks.push_back(std::move(bank));
			return true;
		}
	} catch(mpt::out_of_memory e)
	{
		mpt::delete_out_of_memory(e);
	} catch(const std::exception &)
	{
	}
	return false;
}


size_t CTrackApp::AddScannedDLSBanks()
{
	if(!m_scannedDlsBanks.valid())
		return 0;

	size_t numAdded = 0;
	auto scannedBanks = m_scannedDlsBanks.get();
	gpDLSBanks.reserve(gpDLSBanks.size() + scannedBanks.size());
	const size_t existingBanks = gpDLSBanks.size();
	for(auto &bank : scannedBanks)
	{
		if(std::find_if(gpDLSBanks.begin(), gpDLSBanks.begin() + existingBanks, [&bank](const auto &other) { return other && *bank == *other; }) == gpDLSBanks.begin() + existingBanks)
		{
			gpDLSBanks.push_back(std::move(bank));
			numAdded++;
		}
	}
	return numAdded;
}


/////////////////////////////////////////////////////////////////////////////
// CTrackApp

MODTYPE CTrackApp::m_nDefaultDocType = MOD_TYPE_IT;

UI_MESSAGE_MAP_BEGIN(CTrackApp, AppBase)
	UI_COMMAND(ID_FILE_NEW,       &CTrackApp::OnFileNew)
	UI_COMMAND(ID_FILE_NEWMOD,    &CTrackApp::OnFileNewMOD_Amiga)
	UI_COMMAND(ID_FILE_NEWMOD_PC, &CTrackApp::OnFileNewMOD_PC)
	UI_COMMAND(ID_FILE_NEWS3M,    &CTrackApp::OnFileNewS3M)
	UI_COMMAND(ID_FILE_NEWXM,     &CTrackApp::OnFileNewXM)
	UI_COMMAND(ID_FILE_NEWIT,     &CTrackApp::OnFileNewIT)
	UI_COMMAND(ID_NEW_MPT,        &CTrackApp::OnFileNewMPT)
	UI_COMMAND(ID_FILE_OPEN,      &CTrackApp::OnFileOpen)
	UI_COMMAND(ID_FILE_CLOSEALL,  &CTrackApp::OnFileCloseAll)
	UI_COMMAND(ID_APP_ABOUT,      &CTrackApp::OnAppAbout)
	UI_UPDATE_COMMAND(ID_FILE_CLOSEALL, &CTrackApp::OnUpdateAnyDocsOpen)
UI_MESSAGE_MAP_END()

/////////////////////////////////////////////////////////////////////////////
// CTrackApp construction

CTrackApp::CTrackApp() = default;


CTrackApp::~CTrackApp() = default;


Document *CTrackApp::OpenTemplateFile(const mpt::PathString &file) const
{
	return GetModDocTemplate()->OpenTemplateFile(file);
}


void CTrackApp::AddToRecentFileList(const mpt::PathString &path)
{
	// path may refer to an mruFiles entry, which RemoveMruItem erases
	const mpt::PathString pathCopy = path;
	RemoveMruItem(pathCopy);
	TrackerSettings::Instance().mruFiles.insert(TrackerSettings::Instance().mruFiles.begin(), pathCopy);
	if(TrackerSettings::Instance().mruFiles.size() > TrackerSettings::Instance().mruListLength)
	{
		TrackerSettings::Instance().mruFiles.resize(TrackerSettings::Instance().mruListLength);
	}
	CMainFrame::GetMainFrame()->UpdateMRUList();
}


void CTrackApp::RemoveMruItem(const size_t item)
{
	if(item < TrackerSettings::Instance().mruFiles.size())
	{
		TrackerSettings::Instance().mruFiles.erase(TrackerSettings::Instance().mruFiles.begin() + item);
		CMainFrame::GetMainFrame()->UpdateMRUList();
	}
}


void CTrackApp::RemoveMruItem(const mpt::PathString &path)
{
	auto &mruFiles = TrackerSettings::Instance().mruFiles;
	for(auto i = mruFiles.begin(); i != mruFiles.end(); i++)
	{
		if(!mpt::PathCompareNoCase(*i, path))
		{
			mruFiles.erase(i);
			break;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
// CTrackApp initialization


namespace Tracker
{
mpt::recursive_mutex_with_lock_count & GetGlobalMutexRef()
{
	return theApp.GetGlobalMutexRef();
}
} // namespace Tracker


SettingsContainer &CTrackApp::GetPluginState()
{
	MPT_ASSERT(m_pPluginState);
	return *m_pPluginState;
}


SettingsContainer &CTrackApp::GetPluginCache()
{
	MPT_ASSERT(m_pPluginCache);
	return *m_pPluginCache;
}


SettingsContainer &CTrackApp::GetSongSettings()
{
	MPT_ASSERT(m_pSongSettings);
	return *m_pSongSettings;
}


const mpt::PathString &CTrackApp::GetSongSettingsFilename() const
{
	return m_pSongSettingsIniFile->Filename();
}


// Move a config file called fileName from the App's directory (or one of its sub directories specified by subDir) to
// %APPDATA%. If specified, it will be renamed to newFileName. Existing files are never overwritten.
// Returns true on success.
bool CTrackApp::MoveConfigFile(const mpt::PathString &fileName, mpt::PathString subDir, mpt::PathString newFileName)
{
	const mpt::PathString oldPath = GetInstallPath() + subDir + fileName;
	mpt::PathString newPath = GetConfigPath() + subDir;
	if(!newFileName.empty())
		newPath += newFileName;
	else
		newPath += fileName;

	if(!FileSystem::IsFile(newPath) && FileSystem::IsFile(oldPath))
	{
		return Util::MoveFile(oldPath, newPath) != 0;
	}
	return false;
}


// Set up paths were configuration data is written to. Set overridePortable to true if application's own directory should always be used.
void CTrackApp::SetupPaths(bool overridePortable)
{
	const mpt::PathString exePath = FileSystem::FindApplicationDirectory();

	// When running from the build directory, the package template is in the openmpt submodule
	bool modeSourceProject = false;
	std::error_code ec;
	std::filesystem::path sourceRoot = FileSystem::ToFilesystemPath(exePath);
	for(int level = 0; level < 8 && sourceRoot.has_parent_path() && sourceRoot != sourceRoot.parent_path(); ++level)
	{
		sourceRoot = sourceRoot.parent_path();
		if(std::filesystem::is_directory(sourceRoot / "openmpt" / "packageTemplate", ec))
		{
			modeSourceProject = true;
			break;
		}
	}
	if(modeSourceProject)
	{
		m_InstallPath = FileSystem::FromFilesystemPath(sourceRoot).WithTrailingSlash();
		m_InstallBinPath = exePath;
		m_InstallBinArchPath = exePath;
		m_InstallPkgPath = FileSystem::FromFilesystemPath(sourceRoot / "openmpt" / "packageTemplate").WithTrailingSlash();
	} else
	{
		m_InstallPath = exePath;
		m_InstallBinPath = exePath;
		m_InstallBinArchPath = exePath;
		m_InstallPkgPath = exePath;
	}

	// Determine paths and portable mode.
	const mpt::PathString configPathPortable = modeSourceProject ? exePath : m_InstallPath;
	const mpt::PathString configDir = FileSystem::FindConfigDirectory();
	const mpt::PathString configPathUser = configDir.empty() ? mpt::PathString() : (configDir + P_("OpenMPT")).WithTrailingSlash();

	// Check if the user has configured portable mode.
	const bool configPortableFlag = FileSystem::IsFile(configPathPortable + P_("OpenMPT.portable"));
	const bool portableMode = overridePortable || configPortableFlag || configPathUser.empty();

	m_ConfigPath = portableMode ? configPathPortable : configPathUser;

	// Set up default file locations
	m_szConfigFileName = m_ConfigPath + P_("mptrack.ini"); // config file
	m_PluginStateFileName = m_ConfigPath + P_("PluginState.ini"); // state of plugin loader for crash recovery
	m_szPluginCacheFileName = m_ConfigPath + P_("plugin.cache"); // plugin cache

	m_bInstallerMode = !modeSourceProject && !portableMode;
	m_bPortableMode = portableMode;
	m_bSourceTreeMode = modeSourceProject;
}


void CTrackApp::CreatePaths()
{
	// Create missing directories
	if(!FileSystem::IsDirectory(m_ConfigPath))
	{
		Util::CreateDirectory(m_ConfigPath);
	}
}


mpt::PathString CTrackApp::GetUserTemplatesPath() const
{
	return GetConfigPath() + P_("TemplateModules/");
}


mpt::PathString CTrackApp::GetExampleSongsPath() const
{
	return GetInstallPkgPath() + P_("ExampleSongs/");
}


bool CTrackApp::InitInstanceEarly(CMPTCommandLineInfo &cmdInfo)
{
	if(!AppBase::InitInstance())
	{
		return false;
	}

	// Set up paths to store configuration in
	SetupPaths(cmdInfo.m_portable);

	return true;
}


bool CTrackApp::InitInstanceImpl(CMPTCommandLineInfo &cmdInfo)
{
	m_GuiThreadId = mpt::log::Trace::GetCurrentThreadId();

	mpt::log::Trace::SetThreadId(mpt::log::Trace::ThreadKindGUI, m_GuiThreadId);

	MPT_LOG_GLOBAL(LogInformation, "", UL_("OpenMPT Start"));

	// create the tracker-global random device
	m_RD = std::make_unique<mpt::random_device>();
	// create and seed the traker-global best PRNG with the random device
	m_PRNG = std::make_unique<mpt::thread_safe_prng<mpt::default_prng> >(mpt::make_prng<mpt::default_prng>(RandomDevice()));
	// additionally, seed the C rand() PRNG, just in case any third party library calls rand()
	mpt::crand::reseed(RandomDevice());

	#ifdef MPT_ENABLE_ARCH_INTRINSICS
	#endif // MPT_ENABLE_ARCH_INTRINSICS

	// Create paths to store configuration in
	CreatePaths();

	m_pSettingsIniFile = std::make_unique<IniFileSettingsBackend>(m_szConfigFileName);
	m_pSettings = std::make_unique<SettingsContainer>(m_pSettingsIniFile.get());

	m_pDebugSettings = std::make_unique<DebugSettings>(*m_pSettings);

	m_pTrackerSettings = std::make_unique<TrackerSettings>(*m_pSettings);

	MPT_LOG_GLOBAL(LogInformation, "", UL_("OpenMPT settings initialized."));

	m_pSongSettingsIniFile = std::make_unique<IniFileSettingsBackend>(GetConfigPath() + P_("SongSettings.ini"));
	m_pSongSettings = std::make_unique<SettingsContainer>(m_pSongSettingsIniFile.get());

	m_pPluginState = std::make_unique<IniFileSettingsContainer>(m_PluginStateFileName);
	m_pPluginCache = std::make_unique<IniFileSettingsContainer>(m_szPluginCacheFileName);

	{
		static constexpr int iconEntryIds[] = {0, 1, 2, 4, 5};
		static std::vector<std::unique_ptr<Fl_ICO_Image>> appIcons;
		static std::vector<const Fl_RGB_Image *> appIconPtrs;
		const mpt::const_byte_span iconData = GetResource(IDR_MAINFRAME);
		for(const int iconEntryId : iconEntryIds)
		{
			auto icon = std::make_unique<Fl_ICO_Image>(nullptr, iconEntryId, reinterpret_cast<const unsigned char *>(iconData.data()), iconData.size());
			if(icon->fail() == 0)
			{
				appIconPtrs.push_back(icon.get());
				appIcons.push_back(std::move(icon));
			}
		}
		Fl_Window::default_xclass("OpenMPT");
		Fl_Window::default_icons(appIconPtrs.data(), static_cast<int>(appIconPtrs.size()));
	}

	// create main frame window
	CMainFrame *pMainFrame = new CMainFrame();
	SetMainWnd(pMainFrame);
	{
		Menu mainMenu;
		if(!mainMenu.LoadMenu(IDR_MAINFRAME))
			return false;
		pMainFrame->SetMenu(mainMenu);
	}
	if(!pMainFrame->OnCreate())
	{
		return false;
	}

	// Show splash screen
	if(cmdInfo.m_showSplash && TrackerSettings::Instance().m_ShowSplashScreen)
	{
		StartSplashScreen();
	}

	// Register document templates
	{
		auto modTemplate = std::make_unique<CModDocTemplate>(
			IDR_MODULETYPE,
			[]() -> Document * { return new CModDoc(); },
			[](Document &) -> ChildFrameBase * { return new CChildFrame(); });
		m_pModTemplate = modTemplate.get();
		AddDocTemplate(std::move(modTemplate));
	}

	// Load sound APIs
	// requires TrackerSettings
	m_pAllSoundDeviceComponents = std::make_unique<AllSoundDeviceComponents>();
	SoundDevice::SysInfo sysInfo(mpt::osinfo::get_class());
	SoundDevice::AppInfo appInfo;
	appInfo.SetName(UL_("OpenMPT"));
	appInfo.BoostedThreadRealtimePosix = TrackerSettings::Instance().SoundBoostedThreadRealtimePosix;
	appInfo.BoostedThreadNicenessPosix = TrackerSettings::Instance().SoundBoostedThreadNicenessPosix;
	appInfo.BoostedThreadRtprioPosix = TrackerSettings::Instance().SoundBoostedThreadRtprioPosix;
	appInfo.AllowDeferredProcessing = TrackerSettings::Instance().SoundAllowDeferredProcessing;
	std::vector<std::shared_ptr<SoundDevice::IDevicesEnumerator>> deviceEnumerators = SoundDevice::Manager::GetEnabledEnumerators(*m_pAllSoundDeviceComponents);
	m_pSoundDevicesManager = std::make_unique<SoundDevice::Manager>(m_GlobalLogger, sysInfo, appInfo, std::move(deviceEnumerators));
	m_pTrackerSettings->MigrateOldSoundDeviceSettings(*m_pSoundDevicesManager);

	// Set default note names
	CTrackerSoundFile::SetDefaultNoteNames();

	// Load Soundfonts and default MIDI Library
	if(!cmdInfo.m_noDls)
	{
		m_scannedDlsBanks = LoadDefaultDLSBanks();
	}
	// Load user-defined MIDI Library
	ImportMidiConfig(theApp.GetSettings(), {}, true);

	// Initialize Plugins
	if(!cmdInfo.m_noPlugins)
	{
		InitializeDXPlugins();
	}

	// Initialize CMainFrame
	pMainFrame->Initialize();

	// Open the files specified on the command line
	bool isShellSuccess = cmdInfo.m_fileNames.empty();
	for(const auto &filename : cmdInfo.m_fileNames)
	{
		isShellSuccess |= (OpenDocumentFile(filename) != nullptr);
	}
	if(!isShellSuccess)
	{
		StopSplashScreen();
		return false;
	}

	pMainFrame->UpdateDocumentCount();
	pMainFrame->OnShowWindow(true, 0);
	pMainFrame->ShowWindow(true);

	// Perform startup tasks.

	if(TrackerSettings::Instance().FirstRun)
	{
		new WelcomeDlg(pMainFrame);
	}

	theApp.GetSettings().Flush();

	if(TrackerSettings::Instance().m_SoundSettingsOpenDeviceAtStartup)
	{
		pMainFrame->InitPreview();
		pMainFrame->PreparePreview(NOTE_NOTECUT, 0);
		pMainFrame->PlayPreview();
	}

	if(cmdInfo.m_autoPlay)
	{
		pMainFrame->PlayMod(pMainFrame->GetActiveDoc());
	}

	return true;
}


bool CTrackApp::InitInstance()
{
	CMPTCommandLineInfo cmdInfo(GetCommandLine());
	if(!InitInstanceEarly(cmdInfo))
	{
		return false;
	}
	return InitInstanceLate(cmdInfo);
}


bool CTrackApp::InitInstanceLate(CMPTCommandLineInfo &cmdInfo)
{
	return InitInstanceImpl(cmdInfo);
}


int CTrackApp::Run()
{
	return AppBase::Run();
}


int CTrackApp::ExitInstance()
{
	return ExitInstanceImpl();
}


int CTrackApp::ExitInstanceImpl()
{
	// Uninitialize Plugins
	UninitializeDXPlugins();

	ExportMidiConfig(theApp.GetSettings());
	AddScannedDLSBanks();
	SaveDefaultDLSBanks();
	gpDLSBanks.clear();

	m_pSoundDevicesManager.reset();
	m_pAllSoundDeviceComponents.reset();

	m_pPluginCache.reset();
	m_pPluginState.reset();

	m_pSongSettings.reset();
	m_pSongSettingsIniFile.reset();

	m_pTrackerSettings.reset();
	m_pDebugSettings.reset();
	m_pSettings.reset();
	m_pSettingsIniFile.reset();

	m_PRNG.reset();
	m_RD.reset();

#ifdef USE_PROFILER
	Profiler::Update();
	Reporting::Information(Profiler::DumpProfiles());
#endif

	return AppBase::ExitInstance();
}


////////////////////////////////////////////////////////////////////////////////
// App Messages


CModDoc *CTrackApp::NewDocument(MODTYPE newType)
{
	// Build from template
	if(newType == MOD_TYPE_NONE)
	{
		const mpt::PathString templateFile = TrackerSettings::Instance().defaultTemplateFile;
		if(TrackerSettings::Instance().defaultNewFileAction == nfDefaultTemplate && !templateFile.empty())
		{
			// Template file can be either a filename inside one of the preset and user TemplateModules folders, or a full path.
			const mpt::PathString dirs[] = { GetUserTemplatesPath(), GetInstallPkgPath() + P_("TemplateModules/"), mpt::PathString() };
			for(const auto &dir : dirs)
			{
				if(FileSystem::IsFile(dir + templateFile))
				{
					if(CModDoc *modDoc = static_cast<CModDoc *>(m_pModTemplate->OpenTemplateFile(dir + templateFile)))
					{
						return modDoc;
					}
				}
			}
		}


		// Default module type
		newType = TrackerSettings::Instance().defaultModType;

		// Get active document to make the new module of the same type
		CModDoc *pModDoc = CMainFrame::GetMainFrame()->GetActiveDoc();
		if(pModDoc != nullptr && TrackerSettings::Instance().defaultNewFileAction == nfSameAsCurrent)
		{
			newType = pModDoc->GetSoundFile().GetBestSaveFormat();
		}
	}

	SetDefaultDocType(newType);
	return static_cast<CModDoc *>(m_pModTemplate->OpenDocumentFile(mpt::PathString()));
}


void CTrackApp::OpenModulesDialog(std::vector<mpt::PathString> &files, const mpt::PathString &overridePath)
{
	files.clear();

	static constexpr std::string_view commonExts[] = {"mod", "s3m", "xm", "it", "mptm", "mo3", "oxm", "nst", "stk", "m15", "pt36", "mid", "rmi", "smf", "wav", "mdz", "s3z", "xmz", "itz", "mdr"};
	std::string exts, extsWithoutCommon;
	for(const auto &ext : CSoundFile::GetSupportedExtensions(true))
	{
		const auto filter = std::string("*.") + ext + std::string(";");
		exts += filter;
		if(!mpt::contains(commonExts, ext))
			extsWithoutCommon += filter;
	}

	static int nFilterIndex = 0;
	FileDialog dlg = OpenFileDialog()
		.AllowMultiSelect()
		.ExtensionFilter("All Modules (*.mptm,*.mod,*.xm,*.s3m,*.it,...)|" + exts + ";mod.*"
		"|"
		"Compressed Modules (*.mdz,*.s3z,*.xmz,*.itz,*.mo3,*.oxm,...)|*.mdz;*.s3z;*.xmz;*.itz;*.mdr;*.zip;*.rar;*.lha;*.pma;*.lzs;*.gz;*.mo3;*.oxm"
		"|"
		"ProTracker Modules (*.mod,*.nst)|*.mod;mod.*;*.mdz;*.nst;*.m15;*.stk;*.pt36|"
		"Scream Tracker Modules (*.s3m,*.stm)|*.s3m;*.stm;*.s3z;*.stx|"
		"FastTracker Modules (*.xm)|*.xm;*.xmz|"
		"Impulse Tracker Modules (*.it)|*.it;*.itz|"
		"OpenMPT Modules (*.mptm)|*.mptm;*.mptmz|"
		"Other Modules (*.mtm,*.okt,*.mdl,*.669,*.far,...)|" + extsWithoutCommon + "|"
		"Wave Files (*.wav)|*.wav|"
		"MIDI Files (*.mid,*.rmi)|*.mid;*.rmi;*.smf|"
		"All Files (*.*)|*.*||")
		.WorkingDirectory(overridePath.empty() ? TrackerSettings::Instance().PathSongs.GetWorkingDir() : overridePath)
		.FilterIndex(&nFilterIndex);
	if(!dlg.Show()) return;

	if(overridePath.empty())
		TrackerSettings::Instance().PathSongs.SetWorkingDir(dlg.GetWorkingDirectory());

	files = dlg.GetFilenames();
}


void CTrackApp::OnFileOpen()
{
	FileDialog::PathList files;
	OpenModulesDialog(files);
	for(const auto &file : files)
	{
		OpenDocumentFile(file);
	}
}


// App command to run the dialog
void CTrackApp::OnAppAbout()
{
	if (CAboutDlg::instance) return;
	CAboutDlg::instance = new CAboutDlg();
	CAboutDlg::instance->Create(IDD_ABOUTBOX, GetMainWnd());
}


/////////////////////////////////////////////////////////////////////////////
// Splash Screen

class CSplashScreen : public DialogBase
{
protected:
	ui::Bitmap m_image;

public:
	~CSplashScreen();
	bool OnInitDialog() override;
	void OnOK() override;
	void OnCancel() override { OnOK(); }
	void OnPaint(ui::Painter &dc) override;

	UI_DECLARE_MESSAGE_MAP()
};

UI_MESSAGE_MAP_BEGIN(CSplashScreen, DialogBase)
UI_MESSAGE_MAP_END()

static CSplashScreen *gpSplashScreen = nullptr;

static uint64 gSplashScreenStartTime = 0;


CSplashScreen::~CSplashScreen()
{
	gpSplashScreen = nullptr;
}


void CSplashScreen::OnPaint(ui::Painter &dc)
{
	if(m_image.IsValid())
		dc.StretchBitmap(m_image, Rect(0, 0, w(), h()));
}


bool CSplashScreen::OnInitDialog()
{
	DialogBase::OnInitDialog();

	try
	{
		m_image = ToBitmap(*LoadPixelImage(GetResource(IDB_SPLASHNOFOLDFIN)));
	} catch(const bad_image &)
	{
		return false;
	}

	Rect rect;
	GetWindowRect(&rect);
	const int width = ui::ScalePixels(m_image.GetWidth(), this) / 2;
	const int height = ui::ScalePixels(m_image.GetHeight(), this) / 2;
	SetWindowPos(nullptr,
		rect.left - ((width - rect.Width()) / 2),
		rect.top - ((height - rect.Height()) / 2),
		width,
		height,
		ui::PosNoZOrder);

	return true;
}


void CSplashScreen::OnOK()
{
	StopSplashScreen();
}


static void StartSplashScreen()
{
	if(!gpSplashScreen)
	{
		gpSplashScreen = new CSplashScreen();
		gpSplashScreen->Create(IDD_SPLASHSCREEN, theApp.GetMainWnd());
		gpSplashScreen->ShowWindow(true);
		gpSplashScreen->BeginWaitCursor();
		gSplashScreenStartTime = Util::GetTickCount64();
	}
}


static void StopSplashScreen()
{
	if(gpSplashScreen)
	{
		gpSplashScreen->EndWaitCursor();
		gpSplashScreen->DestroyWindow();
		delete gpSplashScreen;
		gpSplashScreen = nullptr;
	}
}


static void TimeoutSplashScreen()
{
	if(gpSplashScreen)
	{
		if(Util::GetTickCount64() - gSplashScreenStartTime > 100)
		{
			StopSplashScreen();
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
// Idle-time processing

bool CTrackApp::OnIdle(int32 lCount)
{
	bool b = AppBase::OnIdle(lCount);

	TimeoutSplashScreen();

	if(CMainFrame::GetMainFrame())
	{
		CMainFrame::GetMainFrame()->IdleHandlerSounddevice();

		if(m_scannedDlsBanksAvailable)
		{
			if(AddScannedDLSBanks())
				CMainFrame::GetMainFrame()->RefreshDlsBanks();
		}
	}

	// Call plugins idle routine for open editor
	if (m_pPluginManager)
	{
		uint32 curTime = static_cast<uint32>(Util::GetTickCount64());
		//rewbs.vstCompliance: call @ 50Hz
		if (curTime - m_dwLastPluginIdleCall > 20 || curTime < m_dwLastPluginIdleCall)
		{
			m_pPluginManager->OnIdle();
			// Each module has its own plugin manager in libopenmpt
			for(CModDoc *modDoc : GetOpenDocuments())
			{
				if(const auto &pluginManager = modDoc->GetSoundFile().m_PluginManager)
					pluginManager->OnIdle();
			}
			m_dwLastPluginIdleCall = curTime;
		}
	}

	return b;
}


/////////////////////////////////////////////////////////////////////////////
// DIB


void DibBlt(ui::Painter &painter, int x, int y, int sizex, int sizey, int srcx, int srcy, const MODPLUGDIB *dib)
{
	if(!dib || dib->pixels.empty())
		return;
	ui::Bitmap bitmap(sizex, sizey);
	for(int row = 0; row < sizey; ++row)
	{
		const int sy = srcy + row;
		if(sy < 0 || sy >= dib->height)
			continue;
		for(int column = 0; column < sizex; ++column)
		{
			const int sx = srcx + column;
			if(sx < 0 || sx >= dib->width)
				continue;
			bitmap.SetPixel(column, row, dib->palette[dib->pixels[static_cast<std::size_t>(sy) * dib->width + sx] & 0x0F]);
		}
	}
	painter.DrawBitmap(bitmap, x, y, 0, 0, sizex, sizey);
}


std::unique_ptr<MODPLUGDIB> LoadDib(uint32 resourceId)
{
	mpt::const_byte_span data = GetResource(resourceId);
	if(data.size() < 54)
		return nullptr;
	const uint8 *bytes = mpt::byte_cast<const uint8 *>(data.data());
	std::size_t offset = 0;
	if(bytes[0] == 'B' && bytes[1] == 'M')
		offset = 14;
	const auto read32 = [&](std::size_t pos) -> int32
	{
		return static_cast<int32>(bytes[pos] | (bytes[pos + 1] << 8) | (bytes[pos + 2] << 16) | (static_cast<uint32>(bytes[pos + 3]) << 24));
	};
	const uint32 headerSize = static_cast<uint32>(read32(offset));
	const int32 width = read32(offset + 4);
	const int32 rawHeight = read32(offset + 8);
	const uint16 bitCount = static_cast<uint16>(bytes[offset + 14] | (bytes[offset + 15] << 8));
	if(bitCount != 4 || width <= 0 || rawHeight == 0)
		return nullptr;
	const bool isBottomUp = rawHeight > 0;
	const int height = std::abs(rawHeight);
	auto dib = std::make_unique<MODPLUGDIB>();
	dib->width = width;
	dib->height = height;
	const std::size_t paletteOffset = offset + headerSize;
	for(int i = 0; i < 16; ++i)
		dib->palette[i] = RGB(bytes[paletteOffset + i * 4 + 2], bytes[paletteOffset + i * 4 + 1], bytes[paletteOffset + i * 4 + 0]);
	const std::size_t pixelOffset = paletteOffset + 16 * 4;
	const std::size_t stride = (((static_cast<std::size_t>(width) + 1) / 2) + 3) & ~static_cast<std::size_t>(3);
	if(pixelOffset + stride * height > data.size())
		return nullptr;
	dib->pixels.resize(static_cast<std::size_t>(width) * height);
	for(int row = 0; row < height; ++row)
	{
		const int sourceRow = isBottomUp ? height - 1 - row : row;
		const uint8 *source = bytes + pixelOffset + stride * sourceRow;
		for(int column = 0; column < width; ++column)
		{
			const uint8 packed = source[column >> 1];
			dib->pixels[static_cast<std::size_t>(row) * width + column] = (column & 1) ? (packed & 0x0F) : (packed >> 4);
		}
	}
	return dib;
}


void DrawButtonRect(ui::Painter &painter, int lineWidth, const Rect &rect, const mpt::ustring &text, bool disabled, bool pushed, uint32 textFormat, uint32 topMargin)
{
	DrawButtonRect(painter, lineWidth, CMainFrame::GetGUIFont(), rect, text, disabled, pushed, textFormat, topMargin);
}


void DrawButtonRect(ui::Painter &painter, int lineWidth, const ui::Font &font, const Rect &rect, const mpt::ustring &text, bool disabled, bool pushed, uint32 textFormat, uint32 topMargin)
{
	Rect buttonRect = rect;
	const ColorRef colorHighlight = ui::GetSystemColor(ui::SysColor::ButtonHighlight);
	const ColorRef colorShadow = ui::GetSystemColor(ui::SysColor::ButtonShadow);
	painter.FillSolidRect(buttonRect, ui::GetSystemColor(ui::SysColor::ButtonFace));
	if(lineWidth != 1)
	{
		// Draw "real" buttons in Hi-DPI mode
		painter.Draw3dRect(buttonRect, pushed ? colorShadow : colorHighlight, pushed ? colorHighlight : colorShadow);
		buttonRect.DeflateRect(1, 1);
		painter.Draw3dRect(buttonRect, pushed ? colorShadow : colorHighlight, pushed ? colorHighlight : colorShadow);
	} else
	{
		painter.SetPenColor(pushed ? colorShadow : colorHighlight);
		painter.MoveTo(buttonRect.left, buttonRect.bottom - 1);
		painter.LineTo(buttonRect.left, buttonRect.top);
		painter.LineTo(buttonRect.right - 1, buttonRect.top);
		painter.SetPenColor(pushed ? colorHighlight : colorShadow);
		painter.LineTo(buttonRect.right - 1, buttonRect.bottom - 1);
		painter.LineTo(buttonRect.left, buttonRect.bottom - 1);
	}

	if(!text.empty())
	{
		buttonRect = rect;
		buttonRect.DeflateRect(lineWidth, lineWidth);
		if(pushed)
		{
			buttonRect.top += lineWidth;
			buttonRect.left += lineWidth;
		}
		painter.SetTextColor(ui::GetSystemColor(disabled ? ui::SysColor::GrayText : ui::SysColor::ButtonText));
		painter.SetBkTransparent(true);
		buttonRect.top += topMargin;
		painter.SetFont(font);
		painter.DrawText(text, buttonRect, textFormat | ui::TextSingleLine | ui::TextNoPrefix);
	}
}


//////////////////////////////////////////////////////////////////////////////////
// Misc functions


void ErrorBox(uint32 nStringID, Wnd *parent)
{
	mpt::ustring str = LoadResourceString(nStringID);
	const bool resourceLoaded = !str.empty();
	if(!resourceLoaded)
	{
		str = ui::Format(UL_("Resource string %u not found."), nStringID);
	}
	MPT_ASSERT(resourceLoaded);
	Reporting::Error(str, UL_("Error!"), parent);
}


mpt::ustring GetWindowTextString(const Wnd &wnd)
{
	mpt::ustring result;
	wnd.GetWindowText(result);
	return result;
}


mpt::ustring FormatFileSize(uint64 fileSize)
{
	static constexpr std::array<const mpt::uchar *, 4> Unit = {UL_(" B"), UL_(" KB"), UL_(" MB"), UL_(" GB")};
	double size = static_cast<double>(fileSize);
	for(int i = 0; i < 4; i++)
	{
		if(size < 1024.0 || i == 3)
		{
			// Variable-length formatting may decide on a whim to switch to scientific formatting, so used a fixed width and trim manually...
			mpt::ustring s = mpt::ufmt::fix(size, 2);
			if(i == 0)
			{
				if(const auto pos = s.rfind(UL_('.')); pos != mpt::ustring::npos)
					s.erase(pos);
			}
			return s + Unit[i];
		}
		size /= 1024.0;
	}
	return UL_("");
}


mpt::ustring FormatOrderRow(uint32 value)
{
	const bool rowAndOrderNumbersHex = TrackerSettings::Instance().patternSetup & PatternSetup::RowAndOrderNumbersHex;
	if(rowAndOrderNumbersHex)
		return mpt::ufmt::HEX0<2>(value);
	else
		return mpt::ufmt::val(value);
}


////////////////////////////////////////////////////////////////////////////////
// CFastBitmap 8-bit output / 4-bit input
// useful for lots of small blits with color mapping
// combined in one big blit

void CFastBitmap::Init(const MODPLUGDIB *textDib)
{
	m_nBlendOffset = 0;
	m_pTextDib = textDib;
	m_nTextColor = 0;
	m_nBkColor = 1;
	m_width = 0;
	m_height = 0;
	m_n4BitPalette[0] = (uint8)m_nTextColor;
	m_n4BitPalette[4] = MODCOLOR_SEPSHADOW;
	m_n4BitPalette[12] = MODCOLOR_SEPFACE;
	m_n4BitPalette[14] = MODCOLOR_SEPHILITE;
	m_n4BitPalette[15] = (uint8)m_nBkColor;
}


void CFastBitmap::Blit(ui::Painter &painter, int x, int y, int cx, int cy)
{
	cx = std::min(cx, m_width);
	cy = std::min(cy, m_height);
	const Rect clip = painter.GetClipBox();
	const int first_column = std::max(0, clip.left - x);
	const int end_column = std::min(cx, clip.right - x);
	const int first_row = std::max(0, clip.top - y);
	const int end_row = std::min(cy, clip.bottom - y);
	if(first_column >= end_column || first_row >= end_row)
		return;

	if(m_isPaletteDirty)
	{
		for(int i = 0; i < 256; ++i)
		{
			m_paletteRgbx[i][0] = GetRValue(m_palette[i]);
			m_paletteRgbx[i][1] = GetGValue(m_palette[i]);
			m_paletteRgbx[i][2] = GetBValue(m_palette[i]);
		}
		m_isPaletteDirty = false;
	}

	const int width = end_column - first_column;
	const int height = end_row - first_row;
	m_rgbxPixels.resize(static_cast<std::size_t>(width) * height * 4);
	uint8 *out = m_rgbxPixels.data();
	for(int row = first_row; row < end_row; ++row)
	{
		const uint8 *source = &m_pixels[static_cast<std::size_t>(row) * m_width + first_column];
		for(int column = 0; column < width; ++column, out += 4)
			std::memcpy(out, m_paletteRgbx[source[column]], 4);
	}
	painter.DrawRgbx(m_rgbxPixels.data(), x + first_column, y + first_row, width, height);
}


void CFastBitmap::SetColor(uint32 nIndex, ColorRef cr)
{
	if (nIndex < 256)
	{
		m_palette[nIndex] = cr;
		m_isPaletteDirty = true;
	}
}


void CFastBitmap::SetAllColors(uint32 nBaseIndex, uint32 nColors, ColorRef *pcr)
{
	for (uint32 i=0; i<nColors; i++)
	{
		SetColor(nBaseIndex+i, pcr[i]);
	}
}


void CFastBitmap::SetBlendColor(ColorRef cr)
{
	uint32 r = GetRValue(cr);
	uint32 g = GetGValue(cr);
	uint32 b = GetBValue(cr);
	for (uint32 i=0; i<BLEND_OFFSET; i++)
	{
		uint32 m = (GetRValue(m_palette[i]) >> 2)
				+ (GetGValue(m_palette[i]) >> 1)
				+ (GetBValue(m_palette[i]) >> 2);
		m_isPaletteDirty = true;
		m_palette[i|BLEND_OFFSET] = RGB(static_cast<uint8>((m + r)>>1), static_cast<uint8>((m + g)>>1), static_cast<uint8>((m + b)>>1));
	}
}


// Monochrome 4-bit bitmap (0=text, !0 = back)
void CFastBitmap::TextBlt(int x, int y, int cx, int cy, int srcx, int srcy, const MODPLUGDIB *dib)
{
	m_n4BitPalette[0] = (uint8)m_nTextColor;
	m_n4BitPalette[15] = (uint8)m_nBkColor;
	if (x < 0)
	{
		cx += x;
		x = 0;
	}
	if (y < 0)
	{
		cy += y;
		y = 0;
	}
	if ((x >= m_width) || (y >= m_height)) return;
	if (x+cx >= m_width) cx = m_width - x;
	if (y+cy >= m_height) cy = m_height - y;
	if (!dib) dib = m_pTextDib;
	if ((cx <= 0) || (cy <= 0) || (!dib)) return;
	for (int iy=0; iy<cy; iy++)
	{
		const int sourceRow = srcy + iy;
		if(sourceRow < 0 || sourceRow >= dib->height)
			break;
		const uint8 *source = &dib->pixels[static_cast<std::size_t>(sourceRow) * dib->width];
		uint8 *dest = &m_pixels[static_cast<std::size_t>(y + iy) * m_width + x];
		for (int ix=0; ix<cx; ix++)
		{
			const int sourceColumn = srcx + ix;
			if(sourceColumn < 0 || sourceColumn >= dib->width)
				continue;
			dest[ix] = m_n4BitPalette[source[sourceColumn] & 0x0F] + m_nBlendOffset;
		}
	}
}


void CFastBitmap::SetSize(int x, int y)
{
	if(x > 4)
	{
		// Compute the required shift factor for obtaining a power-of-two bitmap width
		m_nXShiftFactor = 1;
		x--;
		while(x >>= 1)
		{
			m_nXShiftFactor++;
		}
	} else
	{
		// Bitmaps rows are aligned to 4 bytes, so let this bitmap be exactly 4 pixels wide.
		m_nXShiftFactor = 2;
	}

	x = (1 << m_nXShiftFactor);
	if(m_pixels.size() != static_cast<size_t>(y << m_nXShiftFactor)) m_pixels.resize(y << m_nXShiftFactor);
	m_width = x;
	m_height = y;
}


///////////////////////////////////////////////////////////////////////////////////
//
// Restore / save plugin list
//

static const auto PLUGFORMAT_FILENAME = MPT_UFORMAT("Plugin{}");
static const auto PLUGFORMAT_TAGS = MPT_UFORMAT("Plugin{}.Tags");
static const auto PLUGFORMAT_TAGS_BUILTIN = MPT_UFORMAT("Plugin{}{}.Tags");
static const auto PLUGFORMAT_LIBNAME = MPT_UFORMAT("Plugin{}.LibraryName");
static const auto PLUGFORMAT_SHELLID = MPT_UFORMAT("Plugin{}.ShellPluginID");

void CTrackApp::InitializeDXPlugins()
{
	m_pPluginManager = new CVstPluginManager;
	PluginUi::RegisterTrackerPlugins(*m_pPluginManager);
	const size_t numPlugins = GetSettings().Read<int32>(UL_("VST Plugins"), UL_("NumPlugins"), 0);

	bool maskCrashes = TrackerSettings::Instance().BrokenPluginsWorkaroundVSTMaskAllCrashes;

	std::vector<VSTPluginLib *> nonFoundPlugs;
	const mpt::PathString failedPlugin = GetPluginState().Read<mpt::PathString>(UL_("VST Plugins"), UL_("FailedPlugin"));
	ConfirmAnswer skipFailed = cnfCancel;

	Dialog pluginScanDlg;
	Wnd *textWnd = nullptr;
	uint64 scanStart = Util::GetTickCount64();

	// Read tags for built-in plugins
	for(auto &plug : *m_pPluginManager)
	{
		PluginUi::SetLibraryTags(*plug, GetSettings().Read<mpt::ustring>(UL_("VST Plugins"), PLUGFORMAT_TAGS_BUILTIN(mpt::ufmt::HEX0<8>(plug->pluginId1), mpt::ufmt::HEX0<8>(plug->pluginId2))));
	}

	// Restructured plugin cache
	if(TrackerSettings::Instance().PreviousSettingsVersion < MPT_V("1.27.00.15"))
	{
		Util::DeleteFile(m_szPluginCacheFileName);
		GetPluginCache().InvalidateCache();
	}

	m_pPluginManager->reserve(numPlugins);
	auto scanFormat = MPT_UFORMAT("Scanning Plugin {} / {}...\n{}");
	for(size_t plug = 0; plug < numPlugins; plug++)
	{
		const mpt::PathString plugPath = PathInstallRelativeToAbsolute(GetSettings().Read<mpt::PathString>(UL_("VST Plugins"), PLUGFORMAT_FILENAME(plug)));
		if(plugPath.empty())
			continue;

		if(!(&pluginScanDlg) && Util::GetTickCount64() >= scanStart + 2000)
		{
			// If this is taking too long, show the user what they're waiting for.
			pluginScanDlg.Create(IDD_SCANPLUGINS, gpSplashScreen);
			pluginScanDlg.ShowWindow(true);
			pluginScanDlg.CenterWindow(gpSplashScreen);
			textWnd = pluginScanDlg.GetDlgItem(IDC_SCANTEXT);
		} else if((&pluginScanDlg) && Util::GetTickCount64() >= scanStart + 30)
		{
			textWnd->SetWindowText(scanFormat(plug + 1, numPlugins + 1, plugPath));
				ui::PumpMessages();
			scanStart = Util::GetTickCount64();
		}

		if(plugPath == failedPlugin)
		{
			GetPluginState().Remove(UL_("VST Plugins"), UL_("FailedPlugin"));
			GetPluginState().Flush(TrackerSettings::Instance().BrokenPluginsWorkaroundSyncStartupCrashRecovery ? Caching::WriteThrough : Caching::WriteBack);
			if(skipFailed == cnfCancel)
			{
				const mpt::ustring text = MPT_UFORMAT("The following plugin has previously crashed OpenMPT during initialisation:\n\n{}\n\nDo you still want to load it?")
					(failedPlugin.ToUnicode());
				skipFailed = Reporting::Confirm(text, false, true, &pluginScanDlg);
			}

			if(skipFailed == cnfNo)
				continue;
		}

		const uint32 shellPluginID = mpt::parse_hex<uint32>(GetSettings().Read<mpt::ustring>(UL_("VST Plugins"), PLUGFORMAT_SHELLID(plug)));
		bool plugFound = true;
		for(VSTPluginLib *lib : m_pPluginManager->AddPlugin(plugPath, maskCrashes, true, &plugFound, shellPluginID))
		{
			if(lib->libraryName == P_("MIDI Input Output") && lib->pluginId1 == PLUGMAGIC('V', 's', 't', 'P') && lib->pluginId2 == PLUGMAGIC('M', 'M', 'I', 'D') && !lib->shellPluginID)
			{
				// This appears to be an old version of our MIDI I/O plugin, which is now built right into the main executable.
				m_pPluginManager->RemovePlugin(lib);
				continue;
			}
			if(!plugFound)
				nonFoundPlugs.push_back(lib);
			if(shellPluginID && lib->shellPluginID != shellPluginID)
				continue;

			PluginUi::SetLibraryTags(*lib, GetSettings().Read<mpt::ustring>(UL_("VST Plugins"), PLUGFORMAT_TAGS(plug)));
			if(shellPluginID != 0)
			{
				if(mpt::PathString libName = GetSettings().Read<mpt::PathString>(UL_("VST Plugins"), PLUGFORMAT_LIBNAME(plug)); !libName.empty())
					lib->libraryName = std::move(libName);
			}

		}
	}
	GetPluginCache().Flush();
	if((&pluginScanDlg))
	{
		pluginScanDlg.DestroyWindow();
	}
	if(!nonFoundPlugs.empty())
	{
		PlugNotFoundDialog(nonFoundPlugs, nullptr).DoModal();
	}
}


void CTrackApp::UninitializeDXPlugins()
{
	if(!m_pPluginManager) return;

	size_t plugIndex = 0;
	for(auto &plug : *m_pPluginManager)
	{
		if(!plug->isBuiltIn)
		{
			mpt::PathString plugPath;
			if(theApp.IsPortableMode())
				plugPath = PathAbsoluteToInstallRelative(plug->dllPath);
			else
				plugPath = plug->dllPath;
			
			const auto libName = PLUGFORMAT_LIBNAME(plugIndex), shellID = PLUGFORMAT_SHELLID(plugIndex);
			if(plug->shellPluginID != 0)
			{
				theApp.GetSettings().Write(UL_("VST Plugins"), libName, plug->libraryName);
				theApp.GetSettings().Write(UL_("VST Plugins"), shellID, mpt::ufmt::HEX0<8>(plug->shellPluginID));
			} else
			{
				theApp.GetSettings().Remove(UL_("VST Plugins"), libName);
				theApp.GetSettings().Remove(UL_("VST Plugins"), shellID);
			}

			theApp.GetSettings().Write<mpt::PathString>(UL_("VST Plugins"), PLUGFORMAT_FILENAME(plugIndex), plugPath);
			theApp.GetSettings().Write(UL_("VST Plugins"), PLUGFORMAT_TAGS(plugIndex), PluginUi::GetLibraryTags(*plug));

			plugIndex++;
		} else
		{
			theApp.GetSettings().Write(UL_("VST Plugins"), PLUGFORMAT_TAGS_BUILTIN(mpt::ufmt::HEX0<8>(plug->pluginId1), mpt::ufmt::HEX0<8>(plug->pluginId2)), PluginUi::GetLibraryTags(*plug));
		}
	}
	theApp.GetSettings().Write(UL_("VST Plugins"), UL_("NumPlugins"), static_cast<uint32>(plugIndex));

	delete m_pPluginManager;
	m_pPluginManager = nullptr;
}


///////////////////////////////////////////////////////////////////////////////////
// Internet-related functions

bool CTrackApp::OpenURL(const char *url)
{
	if(!url) return false;
	return OpenURL(mpt::PathString::FromUTF8(url));
}

bool CTrackApp::OpenURL(const std::string &url)
{
	return OpenURL(mpt::PathString::FromUTF8(url));
}

bool CTrackApp::OpenURL(const mpt::ustring &url)
{
	return OpenURL(mpt::PathString::FromUnicode(url));
}

bool CTrackApp::OpenURL(const mpt::PathString &lpszURL)
{
	if(lpszURL.empty())
		return false;
	const std::string url = lpszURL.ToUTF8();
	const char *const arguments[] = {"xdg-open", url.c_str(), nullptr};
	pid_t pid = 0;
	return posix_spawnp(&pid, "xdg-open", nullptr, nullptr, const_cast<char *const *>(arguments), environ) == 0;
}

bool CTrackApp::OpenDirectory(const mpt::PathString &directory)
{
	if(FileSystem::IsFile(directory))
		return OpenURL(directory.GetDirectoryWithDrive());
	else
		return OpenURL(directory);
}


mpt::ustring CTrackApp::GetResamplingModeName(ResamplingMode mode, int length, bool addTaps)
{
	mpt::ustring result;
	switch(mode)
	{
	case SRCMODE_NEAREST:
		result = (length > 1) ? UL_("No Interpolation") : UL_("None") ;
		break;
	case SRCMODE_LINEAR:
		result = UL_("Linear");
		break;
	case SRCMODE_CUBIC:
		result = UL_("Cubic");
		break;
	case SRCMODE_SINC8:
		result = UL_("Sinc");
		break;
	case SRCMODE_SINC8LP:
		result = UL_("Sinc");
		break;
	default:
		MPT_ASSERT_NOTREACHED();
		break;
	}
	if(Resampling::HasAA(mode))
	{
		result += (length > 1) ? UL_(" + Low-Pass") : UL_(" + LP");
	}
	if(addTaps)
	{
		result += MPT_UFORMAT(" ({} tap{})")(Resampling::Length(mode), (Resampling::Length(mode) != 1) ? mpt::ustring(UL_("s")) : mpt::ustring(UL_("")));
	}
	return result;
}


mpt::ustring CTrackApp::GetFriendlyMIDIPortName(const mpt::ustring &deviceName, bool isInputPort, bool addDeviceName)
{
	auto friendlyName = GetSettings().Read<mpt::ustring>(isInputPort ? UL_("MIDI Input Ports") : UL_("MIDI Output Ports"), deviceName, deviceName);
	if(friendlyName.empty())
		return deviceName;
	else if(addDeviceName && friendlyName != deviceName)
		return friendlyName + UL_(" (") + deviceName + UL_(")");
	else
		return friendlyName;
}


bool ValidateMacroString(Edit &wnd, const std::string_view prevMacro, bool isParametric, bool allowVariables, bool allowMultiline)
{
	mpt::ustring macroStrT;
	wnd.GetWindowText(macroStrT);
	std::string macroStr = mpt::ToCharset(mpt::Charset::ASCII, macroStrT);

	bool allowed = true, caseChange = false;
	for(char &c : macroStr)
	{
		if(c >= 'G' && c <= 'Z')  // Potentially an allowed variable; lowercase it
		{
			caseChange = true;
			c = c - 'A' + 'a';
		}

		if(c == 'k')  // Previously, 'K' was used for MIDI channel
		{
			caseChange = true;
			c = 'c';
		} else if(c >= 'a' && c <= 'c' && !allowVariables)
		{
			caseChange = true;
			c = c - 'a' + 'A';
		} else if(c >= 'd' && c <= 'f')  // abc can be variables, but def can be fixed
		{
			caseChange = true;
			c = c - 'a' + 'A';
		} else if((c >= 'a' && c <= 'c') || c == 'h' || c == 'm' || c == 'n' || c == 'o' || c == 'p' || c == 's' || c == 'u' || c == 'v' || c == 'x' || c == 'y')
		{
			if(!allowVariables)
			{
				allowed = false;
				break;
			}
		} else if(c == 'z')
		{
			if(!isParametric || !allowVariables)
			{
				allowed = false;
				break;
			}
		} else if(c == '\r' || c == '\n')
		{
			if(!allowMultiline)
			{
				allowed = false;
				break;
			}
		} else if(!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || c == ' '))
		{
			allowed = false;
			break;
		}
	}

	if(!allowed)
	{
		// Replace text and keep cursor position if we just typed in an invalid character
		if(prevMacro != std::string_view{macroStr})
		{
			int start, end;
			wnd.GetSel(start, end);
			wnd.SetWindowText(mpt::ToUnicode(mpt::Charset::ASCII, static_cast<std::string>(prevMacro)));
			wnd.SetSel(start - 1, end - 1);
			ui::Beep();
		}
		return false;
	} else
	{
		if(caseChange)
		{
			// Replace text and keep cursor position if there was a case conversion
			int start, end;
			wnd.GetSel(start, end);
			wnd.SetWindowText(mpt::ToUnicode(mpt::Charset::ASCII, static_cast<std::string>(macroStr)));
			wnd.SetSel(start, end);
		}
		return true;
	}
}


static constexpr std::pair<const mpt::uchar*, const mpt::uchar*> SampleFormats[]
{
	{ UL_("Wave Files (*.wav)"), UL_("*.wav") },
	{ UL_("FLAC Files (*.flac,*.oga)"), UL_("*.flac;*.oga") },
#if defined(MPT_WITH_OPUSFILE)
	{ UL_("Opus Files (*.opus,*.oga)"), UL_("*.opus;*.oga") },
#endif  // MPT_WITH_OPUSFILE
#if defined(MPT_WITH_VORBISFILE) || defined(MPT_WITH_STBVORBIS)
	{ UL_("Ogg Vorbis Files (*.ogg,*.oga)"), UL_("*.ogg;*.oga") },
#endif  // VORBIS
#if defined(MPT_ENABLE_MP3_SAMPLES)
	{ UL_("MPEG Files (*.mp1,*.mp2,*.mp3)"), UL_("*.mp1;*.mp2;*.mp3") },
#endif  // MPT_ENABLE_MP3_SAMPLES
	{ UL_("XI Samples (*.xi)"), UL_("*.xi") },
	{ UL_("Impulse Tracker Samples (*.its)"), UL_("*.its") },
	{ UL_("Scream Tracker Samples (*.s3i,*.smp)"), UL_("*.s3i;*.smp") },
	{ UL_("OPL Instruments (*.sb0,*.sb2,*.sbi)"), UL_("*.sb0;*.sb2;*.sbi") },
	{ UL_("GF1 Patches (*.pat)"), UL_("*.pat") },
	{ UL_("Wave64 Files (*.w64)"), UL_("*.w64") },
	{ UL_("CAF Files (*.wav)"), UL_("*.caf") },
	{ UL_("AIFF Files (*.aiff,*.8svx)"), UL_("*.aif;*.aiff;*.iff;*.8sv;*.8svx;*.svx") },
	{ UL_("Sun Audio (*.au,*.snd)"), UL_("*.au;*.snd") },
	{ UL_("SNES BRR Files (*.brr)"), UL_("*.brr") },
};


mpt::ustring ConstructSampleFormatFileFilter(bool includeRaw)
{
	mpt::ustring s = UL_("All Samples (*.wav,*.flac,*.xi,*.its,*.s3i,*.sbi,...)|");
	bool first = true;
	for (const auto& [name, exts] : SampleFormats)
	{
		if (!first)
			s += UL_(";");
		else
			first = false;
		s += exts;
	}
	if (includeRaw)
	{
		s += UL_(";*.raw;*.snd;*.pcm;*.sam");
	}
	s += UL_("|");
	for (const auto& [name, exts] : SampleFormats)
	{
		s += mpt::ustring(name) + UL_("|");
		s += mpt::ustring(exts) + UL_("|");
	}
	if (includeRaw)
	{
		s += UL_("Raw Samples (*.raw,*.snd,*.pcm,*.sam)|*.raw;*.snd;*.pcm;*.sam|");
	}
	s += UL_("All Files (*.*)|*.*||");
	return s;
}



OPENMPT_NAMESPACE_END
