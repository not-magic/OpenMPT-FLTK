// FLTK port of openmpt/mptrack/Moddoc.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "Moddoc.h"
#include "ChannelManagerDlg.h"
#include "Childfrm.h"
#include "CleanupSong.h"
#include "dlg_misc.h"
#include "ExternalSamples.h"
#include "FileDialog.h"
#include "Globals.h"
#include "InputHandler.h"
#include "Mptrack.h"
#include "Mainfrm.h"
#include "MIDIMacroDialog.h"
#include "MIDIMappingDialog.h"
#include "mod2midi.h"
#include "mod2wave.h"
#include "ModDocTemplate.h"
#include "Mpdlgs.h"
#include "Reporting.h"
#include "resource.h"
#include "openmpt/streamencoder/StreamEncoderAU.hpp"
#include "openmpt/streamencoder/StreamEncoderFLAC.hpp"
#include "openmpt/streamencoder/StreamEncoderMP3.hpp"
#include "openmpt/streamencoder/StreamEncoderOpus.hpp"
#include "openmpt/streamencoder/StreamEncoderRAW.hpp"
#include "openmpt/streamencoder/StreamEncoderVorbis.hpp"
#include "openmpt/streamencoder/StreamEncoderWAV.hpp"
#include "TempoSwingDialog.h"
#include "WindowMessages.h"
#include "../common/mptStringBuffer.h"
#include "openmpt_ext/common/mptFileTemporaryExt.h"
#include "../common/mptFileIO.h"
#include "../common/version.h"
#include "../common/FileReader.h"
#include "../soundlib/mod_specifications.h"
#include "../soundlib/OPL.h"
#include "../soundlib/plugins/PlugInterface.h"
#include "../soundlib/modsmp_ctrl.h"
#include "../tracklib/SampleEdit.h"

#include "AbstractVstEditor.h"

#include "mpt/binary/hex.hpp"
#include "mpt/base/numbers.hpp"
#include "mpt/io_file/inputfile.hpp"
#include "mpt/io_file_read/inputfile_filecursor.hpp"
#include "mpt/io_file/outputfile.hpp"
#include "mpt/io/io.hpp"
#include "mpt/io/io_stdstream.hpp"
#include "PluginUi.h"
#include "MIDIMacrosExt.h"

#include <sstream>


OPENMPT_NAMESPACE_BEGIN


const mpt::uchar FileFilterMOD[]	= UL_("ProTracker Modules (*.mod)|*.mod||");
const mpt::uchar FileFilterXM[]	= UL_("FastTracker Modules (*.xm)|*.xm||");
const mpt::uchar FileFilterS3M[] = UL_("Scream Tracker Modules (*.s3m)|*.s3m||");
const mpt::uchar FileFilterIT[]	= UL_("Impulse Tracker Modules (*.it)|*.it||");
const mpt::uchar FileFilterMPT[] = UL_("OpenMPT Modules (*.mptm)|*.mptm||");
const mpt::uchar FileFilterNone[] = UL_("");

const mpt::ustring ModTypeToFilter(const CTrackerSoundFile& sndFile)
{
	const MODTYPE modtype = sndFile.GetType();
	switch(modtype)
	{
		case MOD_TYPE_MOD: return FileFilterMOD;
		case MOD_TYPE_XM: return FileFilterXM;
		case MOD_TYPE_S3M: return FileFilterS3M;
		case MOD_TYPE_IT: return FileFilterIT;
		case MOD_TYPE_MPT: return FileFilterMPT;
		default: return FileFilterNone;
	}
}

/////////////////////////////////////////////////////////////////////////////
// CModDoc


UI_MESSAGE_MAP_BEGIN(CModDoc, Document)
	UI_COMMAND(ID_FILE_SAVE_COPY,		&CModDoc::OnSaveCopy)
	UI_COMMAND(ID_FILE_SAVEASTEMPLATE,	&CModDoc::OnSaveTemplateModule)
	UI_COMMAND(ID_FILE_SAVEASWAVE,		static_cast<void (CModDoc::*)()>(&CModDoc::OnFileWaveConvert))
	UI_COMMAND(ID_FILE_SAVEMIDI,		&CModDoc::OnFileMidiConvert)
	UI_COMMAND(ID_FILE_SAVEOPL,			&CModDoc::OnFileOPLExport)
	UI_COMMAND(ID_FILE_SAVECOMPAT,		&CModDoc::OnFileCompatibilitySave)
	UI_COMMAND(ID_FILE_APPENDMODULE,	&CModDoc::OnAppendModule)
	UI_COMMAND(ID_PLAYER_PLAY,			&CModDoc::OnPlayerPlay)
	UI_COMMAND(ID_PLAYER_PAUSE,			&CModDoc::OnPlayerPause)
	UI_COMMAND(ID_PLAYER_STOP,			&CModDoc::OnPlayerStop)
	UI_COMMAND(ID_PLAYER_PLAYFROMSTART,	&CModDoc::OnPlayerPlayFromStart)
	UI_COMMAND(ID_VIEW_SONGPROPERTIES,	&CModDoc::OnSongProperties)
	UI_COMMAND(ID_VIEW_GLOBALS,			&CModDoc::OnEditGlobals)
	UI_COMMAND(ID_VIEW_PATTERNS,		&CModDoc::OnEditPatterns)
	UI_COMMAND(ID_VIEW_SAMPLES,			&CModDoc::OnEditSamples)
	UI_COMMAND(ID_VIEW_INSTRUMENTS,		&CModDoc::OnEditInstruments)
	UI_COMMAND(ID_VIEW_COMMENTS,		&CModDoc::OnEditComments)
	UI_COMMAND(ID_VIEW_EDITHISTORY,		&CModDoc::OnViewEditHistory)
	UI_COMMAND(ID_VIEW_MIDIMAPPING,		&CModDoc::OnViewMIDIMapping)
	UI_COMMAND(ID_VIEW_MPTHACKS,		&CModDoc::OnViewMPTHacks)
	UI_COMMAND(ID_EDIT_CLEANUP,			&CModDoc::OnShowCleanup)
	UI_COMMAND(ID_EDIT_SAMPLETRIMMER,	&CModDoc::OnShowSampleTrimmer)
	UI_COMMAND(ID_PATTERN_MIDIMACRO,	&CModDoc::OnSetupZxxMacros)
	UI_COMMAND(ID_CHANNEL_MANAGER,		&CModDoc::OnChannelManager)

	UI_COMMAND(ID_ESTIMATESONGLENGTH,	&CModDoc::OnEstimateSongLength)
	UI_COMMAND(ID_APPROX_BPM,			&CModDoc::OnApproximateBPM)
	UI_COMMAND(ID_PATTERN_PLAY,			&CModDoc::OnPatternPlay)
	UI_COMMAND(ID_PATTERN_PLAYNOLOOP,	&CModDoc::OnPatternPlayNoLoop)
	UI_COMMAND(ID_PATTERN_RESTART,		static_cast<void (CModDoc::*)()>(&CModDoc::OnPatternRestart))
	UI_UPDATE_COMMAND(ID_VIEW_INSTRUMENTS,		&CModDoc::OnUpdateXMITMPTOnly)
	UI_UPDATE_COMMAND(ID_PATTERN_MIDIMACRO,		&CModDoc::OnUpdateXMITMPTOnly)
	UI_UPDATE_COMMAND(ID_VIEW_MIDIMAPPING,		&CModDoc::OnUpdateHasMIDIMappings)
	UI_UPDATE_COMMAND(ID_VIEW_EDITHISTORY,		&CModDoc::OnUpdateHasEditHistory)
	UI_UPDATE_COMMAND(ID_FILE_SAVECOMPAT,		&CModDoc::OnUpdateCompatExportableOnly)
UI_MESSAGE_MAP_END()


/////////////////////////////////////////////////////////////////////////////
// CModDoc construction/destruction

CModDoc::CModDoc()
	: m_notifyType(Notification::Default)
	, m_PatternUndo(*this)
	, m_SampleUndo(*this)
	, m_InstrumentUndo(*this)
{
	// Set the creation date of this file (or the load time if we're loading an existing file)
	m_creationTime = mpt::chrono::default_system_clock::now();

	ReinitRecordState();

	CMainFrame::UpdateAudioParameters(m_SndFile, true);
}


CModDoc::~CModDoc()
{
	ClearLog();
}


void CModDoc::SetModified(bool modified)
{
	m_modifiedAutosave = modified;
	if(m_isModifiedAtomic.exchange(modified) != modified)
	{
		// Update window titles in GUI thread
		CMainFrame::GetMainFrame()->SendNotifyMessage(MSG_MOD_SETMODIFIED, reinterpret_cast<WParam>(this), 0);
	}
}


// Return "modified since last autosave" status and reset it until the next SetModified() (as this is only used for polling during autosave)
bool CModDoc::ModifiedSinceLastAutosave()
{
	return m_modifiedAutosave.exchange(false);
}


bool CModDoc::OnNewDocument()
{
	if (!Document::OnNewDocument()) return false;

	m_SndFile.Create(FileReader(), CSoundFile::loadCompleteModule, this);
	InitializeMod();

	ReinitRecordState();
	SetModified(false);
	return true;
}


bool CModDoc::OnOpenDocument(const mpt::PathString &filename)
{

	ScopedLogCapturer logcapturer(*this);

	if(filename.empty()) return OnNewDocument();

	BeginWaitCursor();

	{

		MPT_LOG_GLOBAL(LogDebug, "Loader", UL_("Open..."));

		mpt::IO::InputFile f(filename, TrackerSettings::Instance().MiscCacheCompleteFileBeforeLoading);
		if (f.IsValid())
		{
			FileReader file = GetFileReader(f);
			MPT_ASSERT(GetPathNameMpt().empty());
			SetPathName(filename, false);	// Path is not set yet, but loaders processing external samples/instruments (ITP/MPTM) need this for relative paths.
			try
			{
				if(!m_SndFile.Create(file, CSoundFile::loadCompleteModule, this))
				{
					EndWaitCursor();
					return false;
				}
			} catch(mpt::out_of_memory e)
			{
				mpt::delete_out_of_memory(e);
				EndWaitCursor();
				AddToLog(LogError, UL_("Out of Memory"));
				return false;
			} catch(const std::exception &)
			{
				EndWaitCursor();
				return false;
			}
		}

		MPT_LOG_GLOBAL(LogDebug, "Loader", UL_("Open."));

	}

	EndWaitCursor();

	logcapturer.ShowLog(
		MPT_UFORMAT("File: {}\nLast saved with: {}, you are using OpenMPT {}\n\n")
		(filename, m_SndFile.m_modFormat.madeWithTracker, Version::Current()));

	if((GetModType() == MOD_TYPE_NONE) || (!m_SndFile.GetNumChannels()))
		return false;

	bool noColors = true;
	for(CHANNELINDEX chn = 0; chn < GetNumChannels(); ++chn)
	{
		noColors = noColors && !m_SndFile.HasChannelColor(chn);
	}
	if(noColors)
	{
		SetDefaultChannelColors();
	}

	// Convert to MOD/S3M/XM/IT
	switch(m_SndFile.GetType())
	{
	case MOD_TYPE_MOD:
	case MOD_TYPE_S3M:
	case MOD_TYPE_XM:
	case MOD_TYPE_IT:
	case MOD_TYPE_MPT:
		break;
	default:
		m_SndFile.ChangeModTypeTo(m_SndFile.GetBestSaveFormat(), false);
		m_SndFile.m_SongFlags.set(SONG_IMPORTED);
		break;
	}
	// If the file was packed in some kind of container (e.g. ZIP, or simply a format like MO3), prompt for new file extension as well
	// Same if MOD_TYPE_XXX does not indicate actual song format
	if(m_SndFile.GetContainerType() != ModContainerType::None || m_SndFile.m_SongFlags[SONG_IMPORTED])
	{
		m_ShowSavedialog = true;
	}

	ReinitRecordState();

	if(TrackerSettings::Instance().rememberSongWindows)
		DeserializeViews();

	// This is only needed when opening a module with stored window positions.
	// The MDI child is activated before it has an active view and thus there is no CModDoc associated with it.
	CMainFrame::GetMainFrame()->UpdateEffectKeys(this);
	auto instance = CChannelManagerDlg::sharedInstance();
	if(instance != nullptr)
	{
		instance->SetDocument(this);
	}

	// Show warning if file was made with more recent version of OpenMPT except
	if(m_SndFile.m_dwLastSavedWithVersion.WithoutTestNumber() > Version::Current())
	{
		Reporting::Notification(MPT_UFORMAT("Warning: this song was last saved with a more recent version of OpenMPT.\r\nSong saved with: v{}. Current version: v{}.\r\n")(
			m_SndFile.m_dwLastSavedWithVersion,
			Version::Current()));
	}

	SetModified(false);
	m_bHasValidPath = true;

	// Check if there are any missing samples, and if there are, show a dialog to relocate them.
	for(SAMPLEINDEX smp = 1; smp <= GetNumSamples(); smp++)
	{
		if(m_SndFile.IsExternalSampleMissing(smp))
		{
			MissingExternalSamplesDlg dlg(*this, CMainFrame::GetMainFrame());
			dlg.DoModal();
			break;
		}
	}

	return true;
}


bool CModDoc::OnSaveDocument(const mpt::PathString &filename, const bool setPath)
{
	ScopedLogCapturer logcapturer(*this);
	if(filename.empty())
		return false;

	BeginWaitCursor();
	const bool ok = SaveFile(filename, true);
	if(ok && m_SndFile.m_SongFlags[SONG_IMPORTED])
	{
		const auto formatName = m_SndFile.GetModSpecifications().GetFileExtensionUpper();
		if(!(GetModType() & (MOD_TYPE_MOD | MOD_TYPE_S3M)))
		{
			// Check if any non-supported playback behaviours are enabled due to being imported from a different format
			// File saving code will omit those flags automatically, so it's okay if we check and reset them after a successful save only
			const auto supportedBehaviours = m_SndFile.GetSupportedPlaybackBehaviour(GetModType());
			bool showWarning = true;
			for(size_t i = 0; i < kMaxPlayBehaviours; i++)
			{
				if(m_SndFile.m_playBehaviour[i] && !supportedBehaviours[i])
				{
					if(showWarning)
					{
						AddToLog(LogWarning, MPT_UFORMAT("Some imported Compatibility Settings that are not supported by the {} format have been disabled. Verify that the module still sounds as intended.")(formatName));
						showWarning = false;
					}
					m_SndFile.m_playBehaviour.reset(i);
				}
			}
		}

		for(INSTRUMENTINDEX i = 1; i <= GetNumInstruments(); i++)
		{
			if(m_SndFile.Instruments[i] && m_SndFile.Instruments[i]->synth.HasScripts())
			{
				AddToLog(LogWarning, MPT_UFORMAT("Scripted instruments are not supported by the {} format and will not be exported.")(formatName));
				break;
			}
		}
		if(!m_SndFile.m_globalScript.empty())
			AddToLog(LogWarning, MPT_UFORMAT("Global instrument scripts are not supported by the {} format and will not be exported.")(formatName));
	}
	EndWaitCursor();

	if(ok)
	{
		if(setPath)
		{
			// Set new path for this file, unless we are saving a template or a copy, in which case we want to keep the old file path.
			SetPathName(filename);
		}
		logcapturer.ShowLog(true);
		if(TrackerSettings::Instance().rememberSongWindows)
			SerializeViews();
	} else
	{
		ErrorBox(IDS_ERR_SAVESONG, CMainFrame::GetMainFrame());
	}
	return ok;
}


bool CModDoc::SaveFile(const mpt::PathString &filename, bool allowRelativeSamplePaths)
{
	try
	{
		mpt::IO::SafeOutputFile sf(filename, std::ios::binary, mpt::IO::FlushModeFromBool(TrackerSettings::Instance().MiscFlushFileBuffersOnSave));
		mpt::IO::ofstream &f = sf;
		if(!f)
			return false;

		f.exceptions(f.exceptions() | std::ios::badbit | std::ios::failbit);
		FixNullStrings();
		switch(m_SndFile.GetBestSaveFormat())
		{
		case MOD_TYPE_MOD: return m_SndFile.SaveMod(f);
		case MOD_TYPE_S3M: return m_SndFile.SaveS3M(f);
		case MOD_TYPE_XM:  return m_SndFile.SaveXM(f);
		case MOD_TYPE_IT:  return m_SndFile.SaveIT(f, allowRelativeSamplePaths ? filename : mpt::PathString{});
		case MOD_TYPE_MPT: return m_SndFile.SaveIT(f, allowRelativeSamplePaths ? filename : mpt::PathString{});
		default: MPT_ASSERT_NOTREACHED();
		}
	} catch(const std::exception &)
	{
	}
	return false;
}


bool CModDoc::SaveModified()
{
	if(m_SndFile.GetType() == MOD_TYPE_MPT && !SaveAllSamples())
		return false;
	if(!IsModified())
		return true;
	const mpt::PathString &pathName = GetPathName();
	const mpt::ustring name = pathName.empty() ? GetTitle() : pathName.GetFilename().ToUnicode();
	switch(Reporting::Confirm(MPT_UFORMAT("Save changes to {}?")(name), true, false, CMainFrame::GetMainFrame()))
	{
	case cnfYes: return DoFileSave();
	case cnfNo: return true;
	default: return false;
	}
}


bool CModDoc::SaveAllSamples(bool showPrompt)
{
	if(showPrompt)
	{
		ModifiedExternalSamplesDlg dlg(*this, CMainFrame::GetMainFrame());
		return dlg.DoModal() == IDOK;
	} else
	{
		bool ok = true;
		for(SAMPLEINDEX smp = 1; smp <= GetNumSamples(); smp++)
		{
			ok &= SaveSample(smp);
		}
		return ok;
	}
}


bool CModDoc::SaveSample(SAMPLEINDEX smp)
{
	bool success = false;
	if(smp > 0 && smp <= GetNumSamples())
	{
		const mpt::PathString filename = m_SndFile.GetSamplePath(smp);
		if(!filename.empty())
		{
			auto &sample = m_SndFile.GetSample(smp);
			const auto ext = filename.GetFilenameExtension().ToUnicode().substr(1);
			const auto format = FromSettingValue<SampleEditorDefaultFormat>(ext);

			try
			{
				mpt::IO::SafeOutputFile sf(filename, std::ios::binary, mpt::IO::FlushModeFromBool(TrackerSettings::Instance().MiscFlushFileBuffersOnSave));
				if(sf)
				{
					mpt::IO::ofstream &f = sf;
					f.exceptions(f.exceptions() | std::ios::badbit | std::ios::failbit);

					if(sample.uFlags[CHN_ADLIB] || format == dfS3I)
						success = m_SndFile.SaveS3ISample(smp, f);
					else if(format != dfWAV)
						success = m_SndFile.SaveFLACSample(smp, f);
					else
						success = m_SndFile.SaveWAVSample(smp, f);
				}
			} catch(const std::exception &)
			{
				success = false;
			}

			if(success)
				sample.uFlags.reset(SMP_MODIFIED);
			else
				AddToLog(LogError, MPT_UFORMAT("Unable to save sample {}: {}")(smp, filename));
		}
	}
	return success;
}


void CModDoc::OnCloseDocument()
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if(pMainFrm) pMainFrm->OnDocumentClosed(this);
	Document::OnCloseDocument();
}


void CModDoc::DeleteContents()
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if (pMainFrm) pMainFrm->StopMod(this);
	m_SndFile.Destroy();
	ReinitRecordState();
}


bool CModDoc::DoSave(const mpt::PathString &filename, bool setPath)
{
	const mpt::PathString docFileName = GetPathNameMpt();
	const mpt::ustring defaultExtension = m_SndFile.GetModSpecifications().GetFileExtension();

	switch(m_SndFile.GetBestSaveFormat())
	{
	case MOD_TYPE_MOD:
		MsgBoxHidable(ModSaveHint);
		break;
	case MOD_TYPE_S3M:
		break;
	case MOD_TYPE_XM:
		MsgBoxHidable(XMCompatibilityExportTip);
		break;
	case MOD_TYPE_IT:
		MsgBoxHidable(ItCompatibilityExportTip);
		break;
	case MOD_TYPE_MPT:
		break;
	default:
		ErrorBox(IDS_ERR_SAVESONG, CMainFrame::GetMainFrame());
		return false;
	}

	mpt::PathString ext = P_(".") + mpt::PathString::FromUnicode(defaultExtension);

	mpt::PathString saveFileName;

	if(filename.empty() || m_ShowSavedialog)
	{
		mpt::PathString drive = docFileName.GetDrive();
		mpt::PathString dir = docFileName.GetDirectory();
		mpt::PathString fileName = docFileName.GetFilenameBase();
		if(fileName.empty())
		{
			fileName = mpt::PathString::FromUnicode(GetTitle()).AsSanitizedComponent();
		}
		mpt::PathString defaultSaveName = drive + dir + fileName + ext;

		FileDialog dlg = SaveFileDialog()
			.DefaultExtension(defaultExtension)
			.DefaultFilename(defaultSaveName)
			.ExtensionFilter(ModTypeToFilter(m_SndFile))
			.WorkingDirectory(TrackerSettings::Instance().PathSongs.GetWorkingDir());
		if(!dlg.Show()) return false;

		TrackerSettings::Instance().PathSongs.SetWorkingDir(dlg.GetWorkingDirectory());

		saveFileName = dlg.GetFirstFile();
	} else
	{
		saveFileName = filename;
	}

	// Do we need to create a backup file ?
	if((TrackerSettings::Instance().CreateBackupFiles)
		&& (IsModified()) && (!mpt::PathCompareNoCase(saveFileName, docFileName)))
	{
		if(FileSystem::IsFile(saveFileName))
		{
			mpt::PathString backupFileName = saveFileName.ReplaceExtension(P_(".bak"));
			if(FileSystem::IsFile(backupFileName))
			{
				Util::DeleteFile(backupFileName);
			}
			Util::MoveFile(saveFileName, backupFileName);
		}
	}
	if(OnSaveDocument(saveFileName, setPath))
	{
		// Don't clear modified flag when saving as copy
		if(setPath)
			SetModified(false);
		m_SndFile.m_SongFlags.reset(SONG_IMPORTED);
		m_bHasValidPath = true;
		m_ShowSavedialog = false;
		CMainFrame::GetMainFrame()->UpdateTree(this, GeneralHint().General()); // Update treeview (e.g. filename might have changed)
		return true;
	} else
	{
		return false;
	}
}


void CModDoc::OnAppendModule()
{
	FileDialog::PathList files;
	CTrackApp::OpenModulesDialog(files);

	ScopedLogCapturer logcapture(*this, UL_("Append Failures"));
	try
	{
		auto source = std::make_unique<CTrackerSoundFile>();
		for(const auto &file : files)
		{
			mpt::IO::InputFile f(file, TrackerSettings::Instance().MiscCacheCompleteFileBeforeLoading);
			if(!f.IsValid())
			{
				AddToLog("Unable to open source file!");
				continue;
			}
			try
			{
				if(!source->Create(GetFileReader(f), CSoundFile::loadCompleteModule))
				{
					AddToLog("Unable to open source file!");
					continue;
				}
			} catch(const std::exception &)
			{
				AddToLog("Unable to open source file!");
				continue;
			}
			AppendModule(*source);
			source->Destroy();
			SetModified();
		}
	} catch(mpt::out_of_memory e)
	{
		mpt::delete_out_of_memory(e);
		AddToLog("Out of memory.");
		return;
	}
	
	UpdateAllViews(nullptr, SequenceHint().Data().ModType());
}


void CModDoc::InitializeMod()
{
	const auto defaultType = CTrackApp::GetDefaultDocType();
	m_SndFile.ChangeModTypeTo((defaultType == MOD_TYPE_MOD_PC) ? MOD_TYPE_MOD : defaultType);

	theApp.GetDefaultMidiMacro(m_SndFile.m_MidiCfg);
	m_SndFile.m_SongFlags.set(SONG_LINEARSLIDES & m_SndFile.GetModSpecifications().songFlags);

	switch(defaultType)
	{
	case MOD_TYPE_MOD:
		m_SndFile.ChnSettings.resize(4);
		break;
	case MOD_TYPE_MOD_PC:
		m_SndFile.ChnSettings.resize(8);
		break;
	case MOD_TYPE_S3M:
		m_SndFile.ChnSettings.resize(16);
		break;
	default:
		m_SndFile.ChnSettings.resize(32);
		break;
	}

	SetDefaultChannelColors();

	if(GetModType() == MOD_TYPE_MPT)
	{
		m_SndFile.m_nTempoMode = TempoMode::Modern;
		m_SndFile.m_SongFlags.set(SONG_EXFILTERRANGE);
	}
	m_SndFile.SetDefaultPlaybackBehaviour(GetModType());

	// Refresh mix levels now that the correct mod type has been set
	m_SndFile.SetMixLevels(m_SndFile.GetModSpecifications().defaultMixLevels);

	m_SndFile.Order().assign(1, 0);
	if(!m_SndFile.Patterns.IsValidPat(0))
		m_SndFile.Patterns.Insert(0, 64);

	Clear(m_SndFile.m_szNames);

	m_SndFile.Order().SetDefaultTempoInt(125);
	m_SndFile.Order().SetDefaultSpeed(6);

	// Set up mix levels
	m_SndFile.m_PlayState.m_nGlobalVolume = m_SndFile.m_nDefaultGlobalVolume = MAX_GLOBAL_VOLUME;
	m_SndFile.m_nSamplePreAmp = m_SndFile.m_nVSTiVolume = 48;
	// Setup LRRL panning scheme for MODs
	m_SndFile.SetupMODPanning();

	if(GetModType() == MOD_TYPE_MOD)
	{
		const bool isAmiga = (defaultType != MOD_TYPE_MOD_PC);
		m_SndFile.m_SongFlags.set(SONG_ISAMIGA | SONG_AMIGALIMITS | SONG_PT_MODE, isAmiga);
		m_SndFile.m_SongFlags.set(SONG_FORMAT_NO_VOLCOL);
		m_SndFile.m_playBehaviour.set(kMODOneShotLoops, isAmiga);
		m_SndFile.m_playBehaviour.set(kMODSampleSwap, isAmiga);
		m_SndFile.m_playBehaviour.set(kMODOutOfRangeNoteDelay, isAmiga);
		m_SndFile.m_playBehaviour.set(kMODTempoOnSecondTick, isAmiga);
		m_SndFile.m_playBehaviour.set(kFT2MODTremoloRampWaveform);
	}

	m_SndFile.m_szNames[1] = "untitled";
	m_SndFile.m_nSamples = (GetModType() == MOD_TYPE_MOD) ? 31 : 1;

	SampleEdit::ResetSamples(m_SndFile, SampleEdit::SmpResetInit);

	m_SndFile.GetSample(1).Initialize(m_SndFile.GetType());

	if(m_SndFile.GetType() == MOD_TYPE_XM)
	{
		if(ModInstrument *ins = m_SndFile.AllocateInstrument(1, 1))
			InitializeInstrument(ins);
	}

	m_SndFile.m_songArtist = TrackerSettings::Instance().defaultArtist;

	m_SndFile.ResetPlayPos();
}


void CModDoc::InitChannel(CHANNELINDEX chn)
{
	if(chn >= GetNumChannels())
		return;

	SetChannelRecordGroup(chn, RecordGroup::NoGroup);
	m_SndFile.m_PlayState.Chn[chn].Reset(ModChannel::resetTotal, m_SndFile, chn, CTrackerSoundFile::GetChannelMuteFlag());
	m_SndFile.m_bChannelMuteTogglePending[chn] = false;
}


bool CModDoc::SetDefaultChannelColors(CHANNELINDEX minChannel, CHANNELINDEX maxChannel)
{
	LimitMax(minChannel, GetNumChannels());
	LimitMax(maxChannel, GetNumChannels());
	if(maxChannel < minChannel)
		std::swap(minChannel, maxChannel);
	bool modified = false;
	if(TrackerSettings::Instance().defaultRainbowChannelColors != DefaultChannelColors::NoColors)
	{
		const bool rainbow = TrackerSettings::Instance().defaultRainbowChannelColors == DefaultChannelColors::Rainbow;
		CHANNELINDEX numGroups = 0;
		if(rainbow)
		{
			for(CHANNELINDEX i = minChannel + 1u; i < maxChannel; i++)
			{
				if(m_SndFile.ChnSettings[i].szName.empty() || m_SndFile.ChnSettings[i].szName != m_SndFile.ChnSettings[i - 1].szName)
					numGroups++;
			}
		}
		const double hueFactor = rainbow ? (1.5 * mpt::numbers::pi) / std::max(1, numGroups - 1) : 1000.0;  // Three quarters of the color wheel, red to purple
		for(CHANNELINDEX i = minChannel, group = minChannel; i < maxChannel; i++)
		{
			if(i > minChannel && (m_SndFile.ChnSettings[i].szName.empty() || m_SndFile.ChnSettings[i].szName != m_SndFile.ChnSettings[i - 1].szName))
				group++;
			const double hue = group * hueFactor;  // 0...2pi
			const double saturation = 0.3;         // 0...2/3
			const double brightness = 1.2;         // 0...4/3
			const double r = brightness * (1 + saturation * (std::cos(hue) - 1.0));
			const double g = brightness * (1 + saturation * (std::cos(hue - 2.09439) - 1.0));
			const double b = brightness * (1 + saturation * (std::cos(hue + 2.09439) - 1.0));
			const auto color = RGB(mpt::saturate_round<uint8>(r * 255), mpt::saturate_round<uint8>(g * 255), mpt::saturate_round<uint8>(b * 255));
			if(m_SndFile.GetChannelColor(i) != color)
			{
				m_SndFile.SetChannelColor(i, color);
				modified = true;
			}
		}
	} else
	{
		for(CHANNELINDEX i = minChannel; i < maxChannel; i++)
		{
			if(m_SndFile.HasChannelColor(i))
			{
				m_SndFile.SetChannelColor(i, CTrackerSoundFile::INVALID_CHANNEL_COLOR);
				modified = true;
			}
		}
	}
	return modified;
}


void CModDoc::PostMessageToAllViews(uint32 uMsg, WParam wParam, LParam lParam)
{
	for(View *pView : GetViews())
	{
		if(pView != nullptr)
			pView->PostMessage(uMsg, wParam, lParam);
	}
}


void CModDoc::SendNotifyMessageToAllViews(uint32 uMsg, WParam wParam, LParam lParam)
{
	for(View *pView : GetViews())
	{
		if(pView != nullptr)
			pView->SendNotifyMessage(uMsg, wParam, lParam);
	}
}


void CModDoc::SendMessageToActiveView(uint32 uMsg, WParam wParam, LParam lParam)
{
	if(auto *lastActiveFrame = CChildFrame::LastActiveFrame(); lastActiveFrame != nullptr)
	{
		lastActiveFrame->SendMessageToDescendants(uMsg, wParam, lParam);
	}
}


void CModDoc::ViewPattern(uint32 nPat, uint32 nOrd)
{
	SendMessageToActiveView(MSG_MOD_ACTIVATEVIEW, IDD_CONTROL_PATTERNS, ((nPat+1) << 16) | nOrd);
}


void CModDoc::ViewSample(uint32 nSmp)
{
	SendMessageToActiveView(MSG_MOD_ACTIVATEVIEW, IDD_CONTROL_SAMPLES, nSmp);
}


void CModDoc::ViewInstrument(uint32 nIns)
{
	SendMessageToActiveView(MSG_MOD_ACTIVATEVIEW, IDD_CONTROL_INSTRUMENTS, nIns);
}


ScopedLogCapturer::ScopedLogCapturer(CModDoc &modDoc, const mpt::ustring &title, Wnd *parent, bool showLog) :
m_modDoc(modDoc), m_oldLogMode(m_modDoc.GetLogMode()), m_title(title), m_pParent(parent), m_showLog(showLog)
{
	m_modDoc.SetLogMode(LogModeGather);
}


void ScopedLogCapturer::ShowLog(bool force)
{
	if(force || m_oldLogMode == LogModeInstantReporting)
	{
		m_modDoc.ShowLog(m_title, m_pParent);
		m_modDoc.ClearLog();
	}
}


void ScopedLogCapturer::ShowLog(const std::string &preamble, bool force)
{
	if(force || m_oldLogMode == LogModeInstantReporting)
	{
		m_modDoc.ShowLog(mpt::ToUnicode(mpt::Charset::Locale, preamble), m_title, m_pParent);
		m_modDoc.ClearLog();
	}
}


void ScopedLogCapturer::ShowLog(const mpt::ustring &preamble, bool force)
{
	if(force || m_oldLogMode == LogModeInstantReporting)
	{
		m_modDoc.ShowLog(preamble, m_title, m_pParent);
		m_modDoc.ClearLog();
	}
}


ScopedLogCapturer::~ScopedLogCapturer()
{
	if(m_showLog)
		ShowLog();
	else
		m_modDoc.ClearLog();
	m_modDoc.SetLogMode(m_oldLogMode);
}


void CModDoc::AddToLog(LogLevel level, const mpt::ustring &text) const
{
	if(m_LogMode == LogModeGather)
	{
		m_Log.push_back(LogEntry(level, text));
	} else
	{
		if(level < LogDebug)
		{
			Reporting::Message(level, text);
		}
	}
}


mpt::ustring CModDoc::GetLogString() const
{
	mpt::ustring ret;
	for(const auto &i : m_Log)
	{
		ret += i.message;
		ret += UL_("\r\n");
	}
	return ret;
}


LogLevel CModDoc::GetMaxLogLevel() const
{
	LogLevel retval = LogInformation;
	// find the most severe loglevel
	for(const auto &i : m_Log)
	{
		retval = std::min(retval, i.level);
	}
	return retval;
}


void CModDoc::ClearLog()
{
	m_Log.clear();
}


uint32 CModDoc::ShowLog(const mpt::ustring &preamble, const mpt::ustring &title, Wnd *parent)
{
	if(!parent) parent = CMainFrame::GetMainFrame();
	if(GetLog().size() > 0)
	{
		LogLevel level = GetMaxLogLevel();
		if(level < LogDebug)
		{
			mpt::ustring text = preamble + mpt::ToUnicode(GetLogString());
			mpt::ustring actualTitle = (title.length() == 0) ? mpt::ustring(MAINFRAME_TITLE) : title;
			Reporting::Message(level, text, actualTitle, parent);
			return IDOK;
		}
	}
	return IDCANCEL;
}


void CModDoc::ProcessMIDI(uint32 midiData, SAMPLEINDEX smp, INSTRUMENTINDEX ins, IMixPlugin *plugin, InputTargetContext ctx)
{
	static uint8 midiVolume = 127;

	MIDIEvents::EventType event  = MIDIEvents::GetTypeFromEvent(midiData);
	const uint8 channel = MIDIEvents::GetChannelFromEvent(midiData);
	const uint8 midiByte1 = MIDIEvents::GetDataByte1FromEvent(midiData);
	const uint8 midiByte2 = MIDIEvents::GetDataByte2FromEvent(midiData);
	uint8 note  = midiByte1 + NOTE_MIN;
	int vol = midiByte2;

	if((event == MIDIEvents::evNoteOn) && !vol)
		event = MIDIEvents::evNoteOff;  //Convert event to note-off if req'd

	PLUGINDEX mappedIndex = 0;
	PlugParamIndex paramIndex = 0;
	uint16 paramValue = 0;
	bool captured = m_SndFile.GetMIDIMapper().OnMIDImsg(midiData, mappedIndex, paramIndex, paramValue);

	// Handle MIDI messages assigned to shortcuts
	CInputHandler *ih = CMainFrame::GetInputHandler();
	if(ih->HandleMIDIMessage(ctx, midiData) != kcNull
		|| ih->HandleMIDIMessage(kCtxAllContexts, midiData) != kcNull)
	{
		// Mapped to a command, no need to pass message on.
		captured = true;
	}

	if(captured)
	{
		// Event captured by MIDI mapping or shortcut, no need to pass message on.
		return;
	}

	const bool validInstr = (ins > 0 && ins <= GetNumInstruments()), validSample = (smp > 0 && smp <= GetNumSamples());
	switch(event)
	{
	case MIDIEvents::evNoteOff:
		if(m_midiSustainActive[channel])
		{
			m_midiSustainBuffer[channel].push_back(midiData);
			return;
		}
		if(validInstr || validSample)
		{
			LimitMax(note, NOTE_MAX);
			if(m_midiPlayingNotes[channel][note])
				m_midiPlayingNotes[channel][note] = false;
			NoteOff(note, validSample, ins, m_noteChannel[note - NOTE_MIN]);
			return;
		} else if(plugin != nullptr)
		{
			plugin->MidiSend(midiData);
		}
		break;

	case MIDIEvents::evNoteOn:
		if(validInstr || validSample)
		{
			LimitMax(note, NOTE_MAX);
			vol = CMainFrame::ApplyVolumeRelatedSettings(midiData, midiVolume);
			PlayNote(PlayNoteParam(note).Instrument(ins).Sample(smp).Volume(vol).CheckNNA(m_midiPlayingNotes[channel]), &m_noteChannel);
			return;
		} else if(plugin != nullptr)
		{
			plugin->MidiSend(midiData);
		}
		break;

	case MIDIEvents::evControllerChange:
		switch(midiByte1)
		{
		case MIDIEvents::MIDICC_Volume_Coarse:
			midiVolume = midiByte2;
			break;
		case MIDIEvents::MIDICC_HoldPedal_OnOff:
			m_midiSustainActive[channel] = (midiByte2 >= 0x40);
			if(!m_midiSustainActive[channel])
			{
				// Release all notes
				for(const auto offEvent : m_midiSustainBuffer[channel])
				{
					ProcessMIDI(offEvent, 0, ins, plugin, ctx);
				}
				m_midiSustainBuffer[channel].clear();
			}
			break;
		}
		break;

	case MIDIEvents::evPitchBend:
		for(size_t n = NOTE_MIN; n <= NOTE_MAX; n++)
		{
			if(!m_midiPlayingNotes[channel][n])
				continue;
			CHANNELINDEX chn = m_noteChannel[n - NOTE_MIN];
			if(chn != CHANNELINDEX_INVALID)
				m_SndFile.m_PlayState.Chn[chn].SetMIDIPitchBend(midiByte2, midiByte1);
		}
		break;

	default:
		break;
	}

	if((TrackerSettings::Instance().midiSetup & MidiSetup::SendMidiToPlugins) && CMainFrame::GetMainFrame()->GetModPlaying() == this && plugin != nullptr)
	{
		plugin->MidiSend(midiData);
		// Sending midi may modify the plug. For now, if MIDI data is not active sensing or aftertouch messages, set modified.
		if(midiData != MIDIEvents::System(MIDIEvents::sysActiveSense)
			&& event != MIDIEvents::evPolyAftertouch && event != MIDIEvents::evChannelAftertouch
			&& event != MIDIEvents::evPitchBend
			&& m_SndFile.GetModSpecifications().supportsPlugins)
		{
			SetModified();
		}
	}
}


CHANNELINDEX CModDoc::PlayNote(PlayNoteParam &params, NoteToChannelMap *noteChannel)
{
	CHANNELINDEX channel = GetNumChannels();

	ModCommand::NOTE note = params.m_note;
	if(ModCommand::IsNote(ModCommand::NOTE(note)))
	{
		CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
		if(pMainFrm == nullptr || note == NOTE_NONE) return CHANNELINDEX_INVALID;
		if (pMainFrm->GetModPlaying() != this)
		{
			// All notes off when resuming paused playback
			m_SndFile.ResetChannels();

			m_SndFile.m_PlayState.m_flags.set(SONG_PAUSED);
			pMainFrm->PlayMod(this);
		}

		TrackerCriticalSection cs;

		if(params.m_notesPlaying)
			CheckNNA(note, params.m_instr, *params.m_notesPlaying);

		// Find a channel to play on
		channel = FindAvailableChannel();
		ModChannel &chn = m_SndFile.m_PlayState.Chn[channel];
		m_SndFile.StopOldNNA(chn, channel);

		// reset channel properties; in theory the chan is completely unused anyway.
		chn.Reset(ModChannel::resetTotal, m_SndFile, CHANNELINDEX_INVALID, CHN_MUTE);
		chn.nNewNote = chn.nLastNote = static_cast<uint8>(note);
		chn.nVolume = 256;

		if(params.m_instr)
		{
			// Set instrument (or sample if there are no instruments)
			chn.ResetEnvelopes();
			m_SndFile.InstrumentChange(m_SndFile.m_PlayState, channel, params.m_instr);
		} else if(params.m_sample > 0 && params.m_sample <= GetNumSamples())
		{
			// Or set sample explicitly
			ModSample &sample = m_SndFile.GetSample(params.m_sample);
			chn.pCurrentSample = sample.samplev();
			chn.pModInstrument = nullptr;
			chn.pModSample = &sample;
			chn.nFineTune = sample.nFineTune;
			chn.nC5Speed = sample.nC5Speed;
			chn.nLoopStart = sample.nLoopStart;
			chn.nLoopEnd = sample.nLoopEnd;
			chn.dwFlags = (sample.uFlags & (CHN_SAMPLEFLAGS & ~CHN_MUTE));
			chn.nPan = 128;
			if(sample.uFlags[CHN_PANNING]) chn.nPan = sample.nPan;
			chn.UpdateInstrumentVolume(&sample, nullptr);
		}
		chn.nFadeOutVol = 0x10000;
		chn.isPreviewNote = true;
		if(params.m_currentChannel != CHANNELINDEX_INVALID)
			chn.nMasterChn = params.m_currentChannel + 1;
		else
			chn.nMasterChn = 0;

		if(chn.dwFlags[CHN_ADLIB] && chn.pModSample && m_SndFile.m_opl)
		{
			m_SndFile.m_opl->Patch(channel, chn.pModSample->adlib);
		}

		m_SndFile.NoteChange(chn, note, false, true, true, channel);
		if(params.m_volume >= 0)
			chn.nVolume = std::min(params.m_volume, 256);
		if(params.m_panning >= 0)
			chn.nPan = std::min(params.m_panning, 256);
		chn.nnaChannelAge = 0;

		// Handle sample looping.
		if ((params.m_loopStart + 16 < params.m_loopEnd) && (params.m_loopStart >= 0) && (chn.pModSample != nullptr))
		{
			chn.position.Set(params.m_loopStart);
			chn.nLoopStart = params.m_loopStart;
			chn.nLoopEnd = params.m_loopEnd;
			chn.nLength = std::min(params.m_loopEnd, chn.pModSample->nLength);
		}

		// Handle extra-loud flag
		chn.dwFlags.set(CHN_EXTRALOUD, !(TrackerSettings::Instance().patternSetup & PatternSetup::NoLoudSamplePreview) && params.m_sample);

		// Handle custom start position
		if(params.m_sampleOffset > 0 && chn.pModSample)
		{
			chn.position.Set(params.m_sampleOffset);
			// If start position is after loop end, set loop end to sample end so that the sample starts
			// playing.
			if(chn.nLoopEnd < params.m_sampleOffset)
				chn.nLength = chn.nLoopEnd = chn.pModSample->nLength;
		}

		// VSTi preview
		if(params.m_instr > 0 && params.m_instr <= m_SndFile.GetNumInstruments())
		{
			const ModInstrument *pIns = m_SndFile.Instruments[params.m_instr];
			if (pIns && pIns->HasValidMIDIChannel()) // instro sends to a midi chan
			{
				PLUGINDEX nPlugin = 0;
				if (chn.pModInstrument)
					nPlugin = chn.pModInstrument->nMixPlug;					// First try instrument plugin
				if ((!nPlugin || nPlugin > MAX_MIXPLUGINS) && params.m_currentChannel != CHANNELINDEX_INVALID)
					nPlugin = m_SndFile.ChnSettings[params.m_currentChannel].nMixPlugin;	// Then try channel plugin

				if ((nPlugin) && (nPlugin <= MAX_MIXPLUGINS))
				{
					IMixPlugin *pPlugin = m_SndFile.m_MixPlugins[nPlugin - 1].pMixPlugin;
					if(pPlugin != nullptr)
					{
						pPlugin->MidiCommand(*pIns, pIns->NoteMap[note - NOTE_MIN], static_cast<uint16>(chn.nVolume), channel);
					}
				}
			}
		}

		// Remove channel from list of mixed channels to fix https://bugs.openmpt.org/view.php?id=209
		// This is required because a previous note on the same channel might have just stopped playing,
		// but the channel is still in the mix list.
		// Since the channel volume / etc is only updated every tick in CSoundFile::ReadNote, and we
		// do not want to duplicate mixmode-dependant logic here, CSoundFile::CreateStereoMix may already
		// try to mix our newly set up channel at volume 0 if we don't remove it from the list.
		auto mixBegin = std::begin(m_SndFile.m_PlayState.ChnMix);
		auto mixEnd = std::remove(mixBegin, mixBegin + m_SndFile.m_nMixChannels, channel);
		m_SndFile.m_nMixChannels = static_cast<CHANNELINDEX>(std::distance(mixBegin, mixEnd));

		if(noteChannel)
		{
			noteChannel->at(note - NOTE_MIN) = channel;
		}
	} else
	{
		TrackerCriticalSection cs;
		// Apply note cut / off / fade (also on preview channels)
		m_SndFile.NoteChange(m_SndFile.m_PlayState.Chn[channel], note);
		for(ModChannel &chn : m_SndFile.m_PlayState.BackgroundChannels(m_SndFile))
		{
			if(chn.isPreviewNote && (chn.pModSample || chn.pModInstrument))
			{
				m_SndFile.NoteChange(chn, note);
			}
		}
	}
	return channel;
}


bool CModDoc::NoteOff(uint32 note, bool fade, INSTRUMENTINDEX ins, CHANNELINDEX currentChn)
{
	TrackerCriticalSection cs;

	const ModInstrument *pIns = nullptr;
	IMixPlugin *pPlugin = nullptr;
	if(ins != INSTRUMENTINDEX_INVALID && ins <= m_SndFile.GetNumInstruments() && ModCommand::IsNote(ModCommand::NOTE(note)))
	{
		pIns = m_SndFile.Instruments[ins];
		if(pIns && pIns->HasValidMIDIChannel())
		{
			PLUGINDEX plug = pIns->nMixPlug;     // First try instrument VST
			if((!plug || plug > MAX_MIXPLUGINS)  // No good plug yet
				&& currentChn < m_SndFile.ChnSettings.size())
			{
				plug = m_SndFile.ChnSettings[currentChn].nMixPlugin;  // Then try Channel VST
			}

			if(plug && plug <= MAX_MIXPLUGINS)
			{
				pPlugin = m_SndFile.m_MixPlugins[plug - 1].pMixPlugin;
			}
		}
	}

	const FlagSet<ChannelFlags> mask = (fade ? CHN_NOTEFADE : (CHN_NOTEFADE | CHN_KEYOFF));
	const CHANNELINDEX startChn = currentChn != CHANNELINDEX_INVALID ? currentChn : m_SndFile.GetNumChannels();
	const CHANNELINDEX endChn = currentChn != CHANNELINDEX_INVALID ? currentChn + 1 : MAX_CHANNELS;
	ModChannel *pChn = &m_SndFile.m_PlayState.Chn[startChn];
	bool found = false;
	for(CHANNELINDEX i = startChn; i < endChn && !found; i++, pChn++)
	{
		// Fade all channels > m_nChannels which are playing this note and aren't NNA channels.
		if(pPlugin && pChn->pModInstrument == pIns && (currentChn == CHANNELINDEX_INVALID || currentChn == i) && pChn->lastMidiNoteWithoutArp == pIns->NoteMap[note - NOTE_MIN])
		{
			pPlugin->MidiCommand(*pIns, pIns->NoteMap[note - NOTE_MIN] | IMixPlugin::MIDI_NOTE_OFF, 0, currentChn);
			pChn->lastMidiNoteWithoutArp = NOTE_NONE;
			found = true;
		}
		if((pChn->isPreviewNote || i < m_SndFile.GetNumChannels())
			&& !pChn->dwFlags[mask]
			&& (pChn->nLength || pChn->dwFlags[CHN_ADLIB])
			&& (note == pChn->nNewNote || note == NOTE_NONE))
		{
			m_SndFile.KeyOff(*pChn);
			if(!m_SndFile.m_nInstruments) pChn->dwFlags.reset(CHN_LOOP | CHN_PINGPONGFLAG);
			if(fade) pChn->dwFlags.set(CHN_NOTEFADE);
			// Instantly stop samples that would otherwise play forever
			if(pChn->pModInstrument && !pChn->pModInstrument->nFadeOut)
				pChn->nFadeOutVol = 0;
			if(pChn->dwFlags[CHN_ADLIB] && m_SndFile.m_opl)
			{
				m_SndFile.m_opl->NoteOff(i);
			}
			if(note)
				found = true;
		}
	}

	return true;
}


// Apply DNA/NNA settings for note preview. It will also set the specified note to be playing in the playingNotes set.
void CModDoc::CheckNNA(ModCommand::NOTE note, INSTRUMENTINDEX ins, std::bitset<128> &playingNotes)
{
	if(ins > GetNumInstruments() || m_SndFile.Instruments[ins] == nullptr || note >= playingNotes.size())
	{
		return;
	}
	const ModInstrument *pIns = m_SndFile.Instruments[ins];
	for(CHANNELINDEX chn = GetNumChannels(); chn < MAX_CHANNELS; chn++)
	{
		const ModChannel &channel = m_SndFile.m_PlayState.Chn[chn];
		if(channel.pModInstrument == pIns && channel.isPreviewNote && ModCommand::IsNote(channel.nLastNote)
			&& (channel.nLength || pIns->HasValidMIDIChannel()) && !playingNotes[channel.nLastNote])
		{
			CHANNELINDEX nnaChn = m_SndFile.CheckNNA(chn, ins, note, false);
			if(nnaChn != CHANNELINDEX_INVALID)
			{
				// Keep the new NNA channel playing in the same channel slot.
				// That way, we do not need to touch the ChnMix array, and we avoid the same channel being checked twice.
				if(nnaChn != chn)
				{
					m_SndFile.m_PlayState.Chn[chn] = std::move(m_SndFile.m_PlayState.Chn[nnaChn]);
					m_SndFile.m_PlayState.Chn[nnaChn] = {};
				}
				// Avoid clicks if the channel wasn't ramping before.
				m_SndFile.m_PlayState.Chn[chn].dwFlags.set(CHN_FASTVOLRAMP);
				m_SndFile.ProcessRamping(m_SndFile.m_PlayState.Chn[chn]);
			}
		}
	}
	playingNotes.set(note);
}


// Check if a given note of an instrument or sample is playing from the editor.
// If note == 0, just check if an instrument or sample is playing.
bool CModDoc::IsNotePlaying(uint32 note, SAMPLEINDEX nsmp, INSTRUMENTINDEX nins)
{
	for(ModChannel &chn : m_SndFile.m_PlayState.BackgroundChannels(m_SndFile))
	{
		if(chn.isPreviewNote && chn.nLength != 0 && !chn.dwFlags[CHN_NOTEFADE | CHN_KEYOFF | CHN_MUTE]
		   && (note == chn.nNewNote || note == NOTE_NONE)
		   && (chn.pModSample == &m_SndFile.GetSample(nsmp) || !nsmp)
		   && (chn.pModInstrument == m_SndFile.Instruments[nins] || !nins))
			return true;
	}
	return false;
}


bool CModDoc::MuteToggleModifiesDocument() const
{
	return (m_SndFile.GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT | MOD_TYPE_S3M)) && TrackerSettings::Instance().MiscSaveChannelMuteStatus;
}


bool CModDoc::MuteChannel(CHANNELINDEX nChn, bool doMute)
{
	if (nChn >= m_SndFile.GetNumChannels())
	{
		return false;
	}

	// Mark channel as muted in channel settings
	m_SndFile.ChnSettings[nChn].dwFlags.set(CHN_MUTE, doMute);

	const bool success = UpdateChannelMuteStatus(nChn);
	if(success && MuteToggleModifiesDocument())
	{
		SetModified();
	}

	return success;
}


bool CModDoc::UpdateChannelMuteStatus(CHANNELINDEX nChn)
{
	const ChannelFlags muteType = CTrackerSoundFile::GetChannelMuteFlag();

	if(nChn >= m_SndFile.GetNumChannels())
		return false;

	ModChannel &chn = m_SndFile.m_PlayState.Chn[nChn];
	const bool doMute = m_SndFile.ChnSettings[nChn].dwFlags[CHN_MUTE];
	if(doMute)
	{
		// Mute pattern channel
		chn.dwFlags.set(muteType);
		if(m_SndFile.m_opl)
			m_SndFile.m_opl->NoteCut(nChn);
		// Kill VSTi notes on muted channel.
		PLUGINDEX nPlug = m_SndFile.GetBestPlugin(chn, nChn, PrioritiseInstrument, EvenIfMuted);
		if(nPlug > 0 && nPlug <= MAX_MIXPLUGINS)
		{
			IMixPlugin *pPlug = m_SndFile.m_MixPlugins[nPlug - 1].pMixPlugin;
			const ModInstrument* pIns = chn.pModInstrument;
			if(pPlug && pIns)
			{
				pPlug->MidiCommand(*pIns, NOTE_KEYOFF, 0, nChn);
			}
		}
	} else
	{
		// On unmute alway cater for both mute types - this way there's no probs if user changes mute mode.
		chn.dwFlags.reset(CHN_SYNCMUTE | CHN_MUTE);
	}

	// Mute any NNA'd channels
	for (CHANNELINDEX i = m_SndFile.GetNumChannels(); i < MAX_CHANNELS; i++)
	{
		if (m_SndFile.m_PlayState.Chn[i].nMasterChn == nChn + 1u)
		{
			if (doMute)
			{
				m_SndFile.m_PlayState.Chn[i].dwFlags.set(muteType);
				if(m_SndFile.m_opl) m_SndFile.m_opl->NoteCut(i);
			} else
			{
				// On unmute alway cater for both mute types - this way there's no probs if user changes mute mode.
				m_SndFile.m_PlayState.Chn[i].dwFlags.reset(CHN_SYNCMUTE | CHN_MUTE);
			}
		}
	}

	return true;
}


bool CModDoc::IsChannelNoFx(CHANNELINDEX nChn) const
{
	if (nChn >= m_SndFile.GetNumChannels()) return true;
	return m_SndFile.ChnSettings[nChn].dwFlags[CHN_NOFX];
}


bool CModDoc::NoFxChannel(CHANNELINDEX nChn, bool bNoFx, bool updateMix)
{
	if (nChn >= m_SndFile.GetNumChannels()) return false;
	m_SndFile.ChnSettings[nChn].dwFlags.set(CHN_NOFX, bNoFx);
	if(updateMix) m_SndFile.m_PlayState.Chn[nChn].dwFlags.set(CHN_NOFX, bNoFx);
	return true;
}


RecordGroup CModDoc::GetChannelRecordGroup(CHANNELINDEX channel) const
{
	if(channel >= GetNumChannels() || channel >= m_multiRecordGroup.size())
		return RecordGroup::NoGroup;
	
	return m_multiRecordGroup[channel];
}


void CModDoc::SetChannelRecordGroup(CHANNELINDEX channel, RecordGroup recordGroup)
{
	if(channel >= GetNumChannels())
		return;
	if(channel >= m_multiRecordGroup.size())
		m_multiRecordGroup.resize(channel + 1);
	
	m_multiRecordGroup[channel] = recordGroup;
}


void CModDoc::ToggleChannelRecordGroup(CHANNELINDEX channel, RecordGroup recordGroup)
{
	if(channel >= GetNumChannels())
		return;
	if(channel >= m_multiRecordGroup.size())
		m_multiRecordGroup.resize(channel + 1);

	if(m_multiRecordGroup[channel] == recordGroup)
		m_multiRecordGroup[channel] = RecordGroup::NoGroup;
	else
		m_multiRecordGroup[channel] = recordGroup;
}


void CModDoc::ReinitRecordState()
{
	m_multiRecordGroup.clear();
}


bool CModDoc::MuteSample(SAMPLEINDEX nSample, bool bMute)
{
	if ((nSample < 1) || (nSample > m_SndFile.GetNumSamples())) return false;
	m_SndFile.GetSample(nSample).uFlags.set(CHN_MUTE, bMute);
	return true;
}


bool CModDoc::MuteInstrument(INSTRUMENTINDEX nInstr, bool bMute)
{
	if ((nInstr < 1) || (nInstr > m_SndFile.GetNumInstruments()) || (!m_SndFile.Instruments[nInstr])) return false;
	m_SndFile.Instruments[nInstr]->dwFlags.set(INS_MUTE, bMute);
	return true;
}


bool CModDoc::SurroundChannel(CHANNELINDEX nChn, bool surround)
{
	if(nChn >= m_SndFile.GetNumChannels()) return false;

	if(!(m_SndFile.GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT))) surround = false;

	if(surround != m_SndFile.ChnSettings[nChn].dwFlags[CHN_SURROUND])
	{
		// Update channel configuration
		if(m_SndFile.GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT)) SetModified();

		m_SndFile.ChnSettings[nChn].dwFlags.set(CHN_SURROUND, surround);
		if(surround)
		{
			m_SndFile.ChnSettings[nChn].nPan = 128;
		}
	}

	// Update playing channel
	m_SndFile.m_PlayState.Chn[nChn].dwFlags.set(CHN_SURROUND, surround);
	if(surround)
	{
		m_SndFile.m_PlayState.Chn[nChn].nPan = 128;
	}
	return true;
}


bool CModDoc::SetChannelGlobalVolume(CHANNELINDEX nChn, uint16 nVolume)
{
	bool ok = false;
	if(nChn >= m_SndFile.GetNumChannels() || nVolume > 64) return false;
	if(m_SndFile.ChnSettings[nChn].nVolume != nVolume)
	{
		m_SndFile.ChnSettings[nChn].nVolume = static_cast<uint8>(nVolume);
		if(m_SndFile.GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT)) SetModified();
		ok = true;
	}
	m_SndFile.m_PlayState.Chn[nChn].nGlobalVol = static_cast<uint8>(nVolume);
	return ok;
}


bool CModDoc::SetChannelDefaultPan(CHANNELINDEX nChn, uint16 nPan)
{
	bool ok = false;
	if(nChn >= m_SndFile.GetNumChannels() || nPan > 256) return false;
	if(m_SndFile.ChnSettings[nChn].nPan != nPan || m_SndFile.ChnSettings[nChn].dwFlags[CHN_SURROUND])
	{
		m_SndFile.ChnSettings[nChn].nPan = nPan;
		m_SndFile.ChnSettings[nChn].dwFlags.reset(CHN_SURROUND);
		if(m_SndFile.GetType() & (MOD_TYPE_S3M | MOD_TYPE_IT | MOD_TYPE_MPT)) SetModified();
		ok = true;
	}
	m_SndFile.m_PlayState.Chn[nChn].nPan = nPan;
	m_SndFile.m_PlayState.Chn[nChn].dwFlags.reset(CHN_SURROUND);
	return ok;
}


bool CModDoc::IsChannelMuted(CHANNELINDEX nChn) const
{
	if(nChn >= m_SndFile.GetNumChannels()) return true;
	return m_SndFile.ChnSettings[nChn].dwFlags[CHN_MUTE];
}


bool CModDoc::IsSampleMuted(SAMPLEINDEX nSample) const
{
	if(!nSample || nSample > m_SndFile.GetNumSamples()) return false;
	return m_SndFile.GetSample(nSample).uFlags[CHN_MUTE];
}


bool CModDoc::IsInstrumentMuted(INSTRUMENTINDEX nInstr) const
{
	if(!nInstr || nInstr > m_SndFile.GetNumInstruments() || !m_SndFile.Instruments[nInstr]) return false;
	return m_SndFile.Instruments[nInstr]->dwFlags[INS_MUTE];
}


uint32 CModDoc::GetPatternSize(PATTERNINDEX nPat) const
{
	if(m_SndFile.Patterns.IsValidIndex(nPat)) return m_SndFile.Patterns[nPat].GetNumRows();
	return 0;
}


void CModDoc::SetFollowWnd(WindowHandle hwnd)
{
	m_hWndFollow = hwnd;
}


bool CModDoc::IsChildSample(INSTRUMENTINDEX nIns, SAMPLEINDEX nSmp) const
{
	return m_SndFile.IsSampleReferencedByInstrument(nSmp, nIns);
}


// Find an instrument that references the given sample.
// If no such instrument is found, INSTRUMENTINDEX_INVALID is returned.
INSTRUMENTINDEX CModDoc::FindSampleParent(SAMPLEINDEX sample) const
{
	if(sample == 0)
	{
		return INSTRUMENTINDEX_INVALID;
	}
	for(INSTRUMENTINDEX i = 1; i <= m_SndFile.GetNumInstruments(); i++)
	{
		const ModInstrument *pIns = m_SndFile.Instruments[i];
		if(pIns != nullptr)
		{
			for(size_t j = 0; j < NOTE_MAX; j++)
			{
				if(pIns->Keyboard[j] == sample)
				{
					return i;
				}
			}
		}
	}
	return INSTRUMENTINDEX_INVALID;
}


SAMPLEINDEX CModDoc::FindInstrumentChild(INSTRUMENTINDEX nIns) const
{
	if ((!nIns) || (nIns > m_SndFile.GetNumInstruments())) return 0;
	const ModInstrument *pIns = m_SndFile.Instruments[nIns];
	if (pIns)
	{
		for (auto n : pIns->Keyboard)
		{
			if ((n) && (n <= m_SndFile.GetNumSamples())) return n;
		}
	}
	return 0;
}


LResult CModDoc::ActivateView(uint32 nIdView, uint32 dwParam)
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if (!pMainFrm) return 0;
	ChildFrameBase *pMDIActive = pMainFrm->MDIGetActive();
	if (pMDIActive)
	{
		View *pView = pMDIActive->GetActiveView();
		if ((pView) && (pView->GetDocument() == this))
		{
			return static_cast<CChildFrame*>(pMDIActive)->ActivateView(nIdView, dwParam);
		}
	}
	for(View *pView : GetViews())
	{
		if ((pView) && (pView->GetDocument() == this))
		{
			CChildFrame *pChildFrm = (CChildFrame *)pView->GetParentFrame();
			pChildFrm->ActivateFrame();
			return pChildFrm->ActivateView(nIdView, dwParam);
		}
	}
	return 0;
}


// Activate document's window.
void CModDoc::ActivateWindow()
{

	CChildFrame *pChildFrm = GetChildFrame();
	if(pChildFrm) pChildFrm->ActivateFrame();
}


void CModDoc::UpdateAllViews(View *pSender, UpdateHint hint, HintObject *pHint)
{
	// Tunnel our UpdateHint into an LParam
	Document::UpdateAllViews(pSender, hint.AsLPARAM(), pHint);
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if (pMainFrm) pMainFrm->UpdateTree(this, hint, pHint);
	
	if(hint.GetType()[HINT_MODCHANNELS | HINT_MODTYPE])
	{
		auto instance = CChannelManagerDlg::sharedInstance();
		if(instance != nullptr && pHint != instance && instance->GetDocument() == this)
			instance->Update(hint, pHint);
	}
	if(hint.GetType()[HINT_MIXPLUGINS | HINT_PLUGINNAMES])
	{
		for(auto &plug : m_SndFile.m_MixPlugins)
		{
			auto mixPlug = plug.pMixPlugin;
			if(mixPlug != nullptr && PluginUi(*mixPlug).GetEditor() && PluginUi(*mixPlug).GetEditor() != pHint)
			{
				PluginUi(*mixPlug).GetEditor()->UpdateView(hint);
			}
		}
	}
}


void CModDoc::UpdateAllViews(UpdateHint hint)
{
	CMainFrame::GetMainFrame()->SendNotifyMessage(MSG_MOD_UPDATEVIEWS, reinterpret_cast<WParam>(this), hint.AsLPARAM());
}


/////////////////////////////////////////////////////////////////////////////
// CModDoc commands

void CModDoc::OnFileWaveConvert()
{
	OnFileWaveConvert(ORDERINDEX_INVALID, ORDERINDEX_INVALID);
}


void CModDoc::OnFileWaveConvert(ORDERINDEX nMinOrder, ORDERINDEX nMaxOrder, const std::vector<EncoderFactoryBase*> &encFactories)
{
	MPT_ASSERT(!encFactories.empty());

	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();

	if ((!pMainFrm) || (!m_SndFile.GetType()) || encFactories.empty()) return;

	CWaveConvert wsdlg(pMainFrm, nMinOrder, nMaxOrder, m_SndFile.Order().GetLengthTailTrimmed() - 1, *this, encFactories);
	{
		BypassInputHandler bih;
		wsdlg.m_Settings.normalize = TrackerSettings::Instance().ExportNormalize;
		wsdlg.m_Settings.silencePlugBuffers = TrackerSettings::Instance().ExportClearPluginBuffers;
		if (wsdlg.DoModal() != IDOK)
			return;
	}

	EncoderFactoryBase *encFactory = wsdlg.m_Settings.GetEncoderFactory();

	const mpt::PathString extension = encFactory->GetTraits().fileExtension;

	FileDialog dlg = SaveFileDialog()
		.DefaultExtension(extension)
		.DefaultFilename(GetPathNameMpt().GetFilenameBase() + P_(".") + extension)
		.ExtensionFilter(encFactory->GetTraits().fileDescription + UL_(" (*.") + extension.ToUnicode() + UL_(")|*.") + extension.ToUnicode() + UL_("||"))
		.WorkingDirectory(TrackerSettings::Instance().PathExport.GetWorkingDir());
	if(!wsdlg.m_Settings.outputToSample && !dlg.Show())
		return;

	// will set default dir here because there's no setup option for export dir yet (feel free to add one...)
	TrackerSettings::Instance().PathExport.SetDefaultDir(dlg.GetWorkingDirectory(), true);
	TrackerSettings::Instance().ExportNormalize = wsdlg.m_Settings.normalize;
	TrackerSettings::Instance().ExportClearPluginBuffers = wsdlg.m_Settings.silencePlugBuffers;

	mpt::PathString drive, dir, name, ext;
	dlg.GetFirstFile().SplitPath(nullptr, &drive, &dir, &name, &ext);
	const mpt::PathString fileName = drive + dir + name;
	const mpt::PathString fileExt = ext;

	const ORDERINDEX currentOrd = m_SndFile.m_PlayState.m_nCurrentOrder;
	const ROWINDEX  currentRow = m_SndFile.m_PlayState.m_nRow;

	int nRenderPasses = 1;
	// Channel mode
	std::vector<bool> usedChannels;
	std::vector<FlagSet<ChannelFlags>> channelFlags;
	// Instrument mode
	std::vector<bool> instrMuteState;

	// CHN_SYNCMUTE is used with formats where CHN_MUTE would stop processing global effects and could thus mess synchronization between exported channels
	const ChannelFlags muteFlag = m_SndFile.m_playBehaviour[kST3NoMutedChannels] ? CHN_SYNCMUTE : CHN_MUTE;

	// Channel mode: save song in multiple wav files (one for each enabled channels)
	if(wsdlg.m_bChannelMode)
	{
		// Don't save empty channels
		CheckUsedChannels(usedChannels);

		nRenderPasses = m_SndFile.GetNumChannels();
		channelFlags.resize(nRenderPasses, ChannelFlags(0));
		for(CHANNELINDEX i = 0; i < m_SndFile.GetNumChannels(); i++)
		{
			// Save channels' flags
			channelFlags[i] = m_SndFile.ChnSettings[i].dwFlags;
			// Ignore muted channels
			if(channelFlags[i][CHN_MUTE]) usedChannels[i] = false;
			// Mute each channel
			m_SndFile.ChnSettings[i].dwFlags.set(muteFlag);
		}
	}
	// Instrument mode: Same as channel mode, but renders per instrument (or sample)
	if(wsdlg.m_bInstrumentMode)
	{
		if(m_SndFile.GetNumInstruments() == 0)
		{
			nRenderPasses = m_SndFile.GetNumSamples();
			instrMuteState.resize(nRenderPasses, false);
			for(SAMPLEINDEX i = 0; i < m_SndFile.GetNumSamples(); i++)
			{
				instrMuteState[i] = IsSampleMuted(i + 1);
				MuteSample(i + 1, true);
			}
		} else
		{
			nRenderPasses = m_SndFile.GetNumInstruments();
			instrMuteState.resize(nRenderPasses, false);
			for(INSTRUMENTINDEX i = 0; i < m_SndFile.GetNumInstruments(); i++)
			{
				instrMuteState[i] = IsInstrumentMuted(i + 1);
				MuteInstrument(i + 1, true);
			}
		}
	}

	pMainFrm->PauseMod(this);
	int oldRepeat = m_SndFile.GetRepeatCount();

	const SEQUENCEINDEX currentSeq = m_SndFile.Order.GetCurrentSequenceIndex();
	if(wsdlg.m_subSongs.empty())
	{
		// Render selection
		SubSong subsong{};
		subsong.sequence = currentSeq;
		subsong.startOrder = wsdlg.m_Settings.minOrder;
		wsdlg.m_subSongs.push_back(subsong);
	}
	for(size_t subsong = 0; subsong < wsdlg.m_subSongs.size(); subsong++)
	{
		const auto &song = wsdlg.m_subSongs[subsong];
		mpt::ustring fileNameAdd;
		for(int i = 0; i < nRenderPasses; i++)
		{
			mpt::PathString thisName = fileName;
			mpt::ustring caption = UL_("file");
			fileNameAdd.clear();
			if(wsdlg.m_subSongs.size() > 1)
			{
				fileNameAdd = MPT_UFORMAT("-{}")(mpt::ufmt::dec0<2>(subsong + 1));
				mpt::ustring seqName = m_SndFile.Order(song.sequence).GetName();
				if(!seqName.empty())
					fileNameAdd += UL_("-") + seqName;
				const auto startPattern = m_SndFile.Order(song.sequence).PatternAt(song.startOrder);
				const auto orderName = startPattern ? startPattern->GetName() : std::string{};
				if(!orderName.empty())
					fileNameAdd += UL_("-") + mpt::ToUnicode(m_SndFile.GetCharsetInternal(), orderName);
			}

			// Channel mode
			if(wsdlg.m_bChannelMode)
			{
				// Re-mute previously processed channel
				if(i > 0)
					m_SndFile.ChnSettings[i - 1].dwFlags.set(muteFlag);

				// Was this channel actually muted? Don't process it then.
				if(!usedChannels[i])
					continue;

				// Add channel number & name (if available) to path string
				if(!m_SndFile.ChnSettings[i].szName.empty())
				{
					fileNameAdd += MPT_UFORMAT("-{}_{}")(mpt::ufmt::dec0<3>(i + 1), mpt::ToUnicode(m_SndFile.GetCharsetInternal(), m_SndFile.ChnSettings[i].szName));
					caption = MPT_UFORMAT("{}: {}")(i + 1, mpt::ToUnicode(m_SndFile.GetCharsetInternal(), m_SndFile.ChnSettings[i].szName));
				} else
				{
					fileNameAdd += MPT_UFORMAT("-{}")(mpt::ufmt::dec0<3>(i + 1));
					caption = MPT_UFORMAT("channel {}")(i + 1);
				}
				// Unmute channel to process
				m_SndFile.ChnSettings[i].dwFlags.reset(muteFlag);
			}
			// Instrument mode
			if(wsdlg.m_bInstrumentMode)
			{
				if(m_SndFile.GetNumInstruments() == 0)
				{
					// Re-mute previously processed sample
					if(i > 0) MuteSample(static_cast<SAMPLEINDEX>(i), true);

					if(!m_SndFile.GetSample(static_cast<SAMPLEINDEX>(i + 1)).HasSampleData() || !IsSampleUsed(static_cast<SAMPLEINDEX>(i + 1), false) || instrMuteState[i])
						continue;

					// Add sample number & name (if available) to path string
					if(!m_SndFile.m_szNames[i + 1].empty())
					{
						fileNameAdd += MPT_UFORMAT("-{}_{}")(mpt::ufmt::dec0<3>(i + 1), mpt::ToUnicode(m_SndFile.GetCharsetInternal(), m_SndFile.m_szNames[i + 1]));
						caption = MPT_UFORMAT("{}: {}")(i + 1, mpt::ToUnicode(m_SndFile.GetCharsetInternal(), m_SndFile.m_szNames[i + 1]));
					} else
					{
						fileNameAdd += MPT_UFORMAT("-{}")(mpt::ufmt::dec0<3>(i + 1));
						caption = MPT_UFORMAT("sample {}")(i + 1);
					}
					// Unmute sample to process
					MuteSample(static_cast<SAMPLEINDEX>(i + 1), false);
				} else
				{
					// Re-mute previously processed instrument
					if(i > 0) MuteInstrument(static_cast<INSTRUMENTINDEX>(i), true);

					if(m_SndFile.Instruments[i + 1] == nullptr || !IsInstrumentUsed(static_cast<SAMPLEINDEX>(i + 1), false) || instrMuteState[i])
						continue;

					if(!m_SndFile.Instruments[i + 1]->name.empty())
					{
						fileNameAdd += MPT_UFORMAT("-{}_{}")(mpt::ufmt::dec0<3>(i + 1), mpt::ToUnicode(m_SndFile.GetCharsetInternal(), m_SndFile.Instruments[i + 1]->name));
						caption = MPT_UFORMAT("{}: {}")(i + 1, mpt::ToUnicode(m_SndFile.GetCharsetInternal(), m_SndFile.Instruments[i + 1]->name));
					} else
					{
						fileNameAdd += MPT_UFORMAT("-{}")(mpt::ufmt::dec0<3>(i + 1));
						caption = MPT_UFORMAT("instrument {}")(i + 1);
					}
					// Unmute instrument to process
					MuteInstrument(static_cast<SAMPLEINDEX>(i + 1), false);
				}
			}

			if(!fileNameAdd.empty())
			{
				fileNameAdd = mpt::SanitizePathComponent(fileNameAdd);
				thisName += mpt::PathString::FromUnicode(fileNameAdd);
			}
			thisName += fileExt;
			if(wsdlg.m_Settings.outputToSample)
			{
				thisName = mpt::TemporaryPathname{}.GetPathname();
			}

			// Render song (or current channel, or current sample/instrument)
			bool cancel = true;
			try
			{
				mpt::IO::SafeOutputFile safeFileStream(thisName, std::ios::binary, mpt::IO::FlushModeFromBool(TrackerSettings::Instance().MiscFlushFileBuffersOnSave));
				mpt::IO::ofstream &f = safeFileStream;
				f.exceptions(f.exceptions() | std::ios::badbit | std::ios::failbit);

				if(!f)
				{
					Reporting::Error("Could not open file for writing. Is it open in another application?");
				} else
				{
					BypassInputHandler bih;
					CDoWaveConvert dwcdlg(m_SndFile, f, caption, wsdlg.m_Settings, song, pMainFrm);
					dwcdlg.m_bGivePlugsIdleTime = wsdlg.m_bGivePlugsIdleTime;
					dwcdlg.m_dwSongLimit = wsdlg.m_dwSongLimit;
					cancel = dwcdlg.DoModal() != IDOK;
				}
			} catch(const std::exception &)
			{
				Reporting::Error(UL_("Error while writing file!"));
			}

			if(wsdlg.m_Settings.outputToSample)
			{
				if(!cancel)
				{
					mpt::IO::InputFile f(thisName, TrackerSettings::Instance().MiscCacheCompleteFileBeforeLoading);
					if(f.IsValid())
					{
						FileReader file = GetFileReader(f);
						SAMPLEINDEX smp = wsdlg.m_Settings.sampleSlot;
						if(smp == 0 || smp > GetNumSamples()) smp = m_SndFile.GetNextFreeSample();
						if(smp == SAMPLEINDEX_INVALID)
						{
							Reporting::Error(UL_("Too many samples!"));
							cancel = true;
						}
						if(!cancel)
						{
							if(GetNumSamples() < smp) m_SndFile.m_nSamples = smp;
							GetSampleUndo().PrepareUndo(smp, sundo_replace, "Render To Sample");
							if(m_SndFile.ReadSampleFromFile(smp, file, false))
							{
								m_SndFile.m_szNames[smp] = "Render To Sample" + mpt::ToCharset(m_SndFile.GetCharsetInternal(), fileNameAdd);
								UpdateAllViews(nullptr, SampleHint().Info().Data().Names());
								if(m_SndFile.GetNumInstruments() && !IsSampleUsed(smp))
								{
									// Insert new instrument for the generated sample in case it is not referenced by any instruments yet.
									// It should only be already referenced if the user chose to export to an existing sample slot.
									InsertInstrument(smp);
									UpdateAllViews(nullptr, InstrumentHint().Info().Names());
								}
								SetModified();
							} else
							{
								GetSampleUndo().RemoveLastUndoStep(smp);
							}
						}
					}
				}

				// Always clean up after ourselves
				for(int retry = 0; retry < 10; retry++)
				{
					// stupid virus scanners
					if(Util::DeleteFile(thisName) != EACCES)
					{
						break;
					}
					Sleep(10);
				}
			}

			if(cancel) break;
		}
	}

	// Restore channels' flags
	if(wsdlg.m_bChannelMode)
	{
		for(CHANNELINDEX i = 0; i < m_SndFile.GetNumChannels(); i++)
		{
			m_SndFile.ChnSettings[i].dwFlags = channelFlags[i];
		}
	}
	// Restore instruments' / samples' flags
	if(wsdlg.m_bInstrumentMode)
	{
		for(size_t i = 0; i < instrMuteState.size(); i++)
		{
			if(m_SndFile.GetNumInstruments() == 0)
				MuteSample(static_cast<SAMPLEINDEX>(i + 1), instrMuteState[i]);
			else
				MuteInstrument(static_cast<INSTRUMENTINDEX>(i + 1), instrMuteState[i]);
		}
	}

	m_SndFile.Order.SetSequence(currentSeq);
	m_SndFile.SetRepeatCount(oldRepeat);
	m_SndFile.GetLength(eAdjust, GetLengthTarget(currentOrd, currentRow));
	m_SndFile.m_PlayState.m_nNextOrder = currentOrd;
	m_SndFile.m_PlayState.m_nNextRow = currentRow;
	CMainFrame::UpdateAudioParameters(m_SndFile, true);
}


void CModDoc::OnFileWaveConvert(ORDERINDEX nMinOrder, ORDERINDEX nMaxOrder)
{
	WAVEncoder wavencoder;
	FLACEncoder flacencoder;
	AUEncoder auencoder;
	OggOpusEncoder opusencoder;
	VorbisEncoder vorbisencoder;
	MP3Encoder mp3lame(MP3EncoderLame);
	MP3Encoder mp3lamecompatible(MP3EncoderLameCompatible);
	RAWEncoder rawencoder;
	std::vector<EncoderFactoryBase*> encoders;
	if(wavencoder.IsAvailable()) encoders.push_back(&wavencoder);
	if(flacencoder.IsAvailable()) encoders.push_back(&flacencoder);
	if(auencoder.IsAvailable()) encoders.push_back(&auencoder);
	if(rawencoder.IsAvailable()) encoders.push_back(&rawencoder);
	if(opusencoder.IsAvailable()) encoders.push_back(&opusencoder);
	if(vorbisencoder.IsAvailable()) encoders.push_back(&vorbisencoder);
	if(mp3lame.IsAvailable())
	{
		encoders.push_back(&mp3lame);
	}
	if(mp3lamecompatible.IsAvailable()) encoders.push_back(&mp3lamecompatible);
	OnFileWaveConvert(nMinOrder, nMaxOrder, encoders);
}


void CModDoc::OnFileMidiConvert()
{
	CModToMidi mididlg(*this, CMainFrame::GetMainFrame());
	BypassInputHandler bih;
	mididlg.DoModal();
}

//HACK: This is a quick fix. Needs to be better integrated into player and GUI.
void CModDoc::OnFileCompatibilitySave()
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if (!pMainFrm) return;

	mpt::ustring pattern;

	const MODTYPE type = m_SndFile.GetType();
	switch(type)
	{
		case MOD_TYPE_IT:
			pattern = FileFilterIT;
			MsgBoxHidable(CompatExportDefaultWarning);
			break;
		case MOD_TYPE_XM:
			pattern = FileFilterXM;
			MsgBoxHidable(CompatExportDefaultWarning);
			break;
		default:
			// Not available for this format.
			return;
	}

	const mpt::ustring ext = m_SndFile.GetModSpecifications().GetFileExtension();

	mpt::PathString filename;

	{
		mpt::PathString drive;
		mpt::PathString dir;
		mpt::PathString fileName;
		GetPathNameMpt().SplitPath(nullptr, &drive, &dir, &fileName, nullptr);

		filename = drive;
		filename += dir;
		filename += fileName;
		if(!strstr(fileName.ToUTF8().c_str(), "compat"))
			filename += P_(".compat.");
		else
			filename += P_(".");
		filename += mpt::PathString::FromUnicode(ext);
	}

	FileDialog dlg = SaveFileDialog()
		.DefaultExtension(ext)
		.DefaultFilename(filename)
		.ExtensionFilter(pattern)
		.WorkingDirectory(TrackerSettings::Instance().PathSongs.GetWorkingDir());
	if(!dlg.Show()) return;

	filename = dlg.GetFirstFile();
	
	bool ok = false;
	BeginWaitCursor();
	try
	{
		mpt::IO::SafeOutputFile sf(filename, std::ios::binary, mpt::IO::FlushModeFromBool(TrackerSettings::Instance().MiscFlushFileBuffersOnSave));
		mpt::IO::ofstream &f = sf;
		if(f)
		{
			f.exceptions(f.exceptions() | std::ios::badbit | std::ios::failbit);
			ScopedLogCapturer logcapturer(*this);
			FixNullStrings();
			switch(type)
			{
			case MOD_TYPE_XM: ok = m_SndFile.SaveXM(f, true); break;
			case MOD_TYPE_IT: ok = m_SndFile.SaveIT(f, filename, true); break;
			default: MPT_ASSERT_NOTREACHED();
			}
		}
	} catch(const std::exception &)
	{
		ok = false;
	}
	EndWaitCursor();

	if(!ok)
	{
		ErrorBox(IDS_ERR_SAVESONG, CMainFrame::GetMainFrame());
	}
}


void CModDoc::OnPlayerPlay()
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if (pMainFrm)
	{
		CChildFrame *pChildFrm = GetChildFrame();
 		if(pChildFrm->IsPatternView())
		{
			//User has sent play song command: set loop pattern checkbox to false.
			pChildFrm->SendViewMessage(VIEWMSG_PATTERNLOOP, 0);
		}

		const bool isPlaying = (pMainFrm->GetModPlaying() == this);
		if(isPlaying && !m_SndFile.m_PlayState.m_flags[SONG_PAUSED | SONG_STEP])
		{
			OnPlayerPause();
			return;
		}

		TrackerCriticalSection cs;

		// Kill editor voices
		for(ModChannel &chn : m_SndFile.m_PlayState.BackgroundChannels(m_SndFile))
		{
			if(chn.isPreviewNote)
			{
				chn.dwFlags.set(CHN_NOTEFADE | CHN_KEYOFF);
				if(!isPlaying)
					chn.nLength = 0;
			}
		}

		m_SndFile.m_PlayState.m_flags.set(SONG_POSITIONCHANGED);

		if(isPlaying)
		{
			m_SndFile.StopAllVsti();
		}

		cs.Leave();

		m_SndFile.m_PlayState.m_flags.reset(SONG_STEP | SONG_PAUSED | SONG_PATTERNLOOP);
		pMainFrm->PlayMod(this);
	}
}


void CModDoc::OnPlayerPause()
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if (pMainFrm)
	{
		if (pMainFrm->GetModPlaying() == this)
		{
			bool isLooping = m_SndFile.m_PlayState.m_flags[SONG_PATTERNLOOP];
			PATTERNINDEX nPat = m_SndFile.m_PlayState.m_nPattern;
			ROWINDEX nRow = m_SndFile.m_PlayState.m_nRow;
			ROWINDEX nNextRow = m_SndFile.m_PlayState.m_nNextRow;
			pMainFrm->PauseMod();

			if ((isLooping) && (nPat < m_SndFile.Patterns.Size()))
			{
				TrackerCriticalSection cs;

				if ((m_SndFile.m_PlayState.m_nCurrentOrder < m_SndFile.Order().GetLength()) && (m_SndFile.Order()[m_SndFile.m_PlayState.m_nCurrentOrder] == nPat))
				{
					m_SndFile.m_PlayState.m_nNextOrder = m_SndFile.m_PlayState.m_nCurrentOrder;
					m_SndFile.m_PlayState.m_nNextRow = nNextRow;
					m_SndFile.m_PlayState.m_nRow = nRow;
				} else
				{
					for (ORDERINDEX nOrd = 0; nOrd < m_SndFile.Order().GetLength(); nOrd++)
					{
						if (m_SndFile.Order()[nOrd] == PATTERNINDEX_INVALID)
							break;
						if (m_SndFile.Order()[nOrd] == nPat)
						{
							m_SndFile.m_PlayState.m_nCurrentOrder = nOrd;
							m_SndFile.m_PlayState.m_nNextOrder = nOrd;
							m_SndFile.m_PlayState.m_nNextRow = nNextRow;
							m_SndFile.m_PlayState.m_nRow = nRow;
							break;
						}
					}
				}
			}
		} else
		{
			pMainFrm->PauseMod();
		}
	}
}


void CModDoc::OnPlayerStop()
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if (pMainFrm) pMainFrm->StopMod();
}


void CModDoc::OnPlayerPlayFromStart()
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if (pMainFrm)
	{
		CChildFrame *pChildFrm = GetChildFrame();
		if(pChildFrm->IsPatternView())
		{
			//User has sent play song command: set loop pattern checkbox to false.
			pChildFrm->SendViewMessage(VIEWMSG_PATTERNLOOP, 0);
		}

		pMainFrm->PauseMod();
		TrackerCriticalSection cs;
		m_SndFile.m_PlayState.m_flags.reset(SONG_STEP | SONG_PATTERNLOOP);
		m_SndFile.ResetPlayPos();
		//m_SndFile.visitedSongRows.Initialize(true);

		m_SndFile.m_PlayState.m_flags.set(SONG_POSITIONCHANGED);

		cs.Leave();

		pMainFrm->PlayMod(this);
	}
}


void CModDoc::OnEditGlobals()
{
	SendMessageToActiveView(MSG_MOD_ACTIVATEVIEW,	IDD_CONTROL_GLOBALS);
}


void CModDoc::OnEditPatterns()
{
	SendMessageToActiveView(MSG_MOD_ACTIVATEVIEW, IDD_CONTROL_PATTERNS, -1);
}


void CModDoc::OnEditSamples()
{
	SendMessageToActiveView(MSG_MOD_ACTIVATEVIEW, IDD_CONTROL_SAMPLES, -1);
}


void CModDoc::OnEditInstruments()
{
	SendMessageToActiveView(MSG_MOD_ACTIVATEVIEW, IDD_CONTROL_INSTRUMENTS, -1);
}


void CModDoc::OnEditComments()
{
	SendMessageToActiveView(MSG_MOD_ACTIVATEVIEW, IDD_CONTROL_COMMENTS);
}


void CModDoc::OnShowCleanup()
{
	CModCleanupDlg dlg(*this, CMainFrame::GetMainFrame());
	dlg.DoModal();
}


void CModDoc::OnSetupZxxMacros()
{
	CMidiMacroSetup dlg(m_SndFile);
	if(dlg.DoModal() == IDOK)
	{
		if(m_SndFile.m_MidiCfg != dlg.m_MidiCfg)
		{
			m_SndFile.m_MidiCfg = dlg.m_MidiCfg;
			SetModified();
		}
	}
}


// Enable menu item only module types that support MIDI Mappings
void CModDoc::OnUpdateHasMIDIMappings(CmdUI *p)
{
	if(p)
		p->Enable((m_SndFile.GetModSpecifications().MIDIMappingDirectivesMax > 0) ? true : false);
}


// Enable menu item only for IT / MPTM / XM files
void CModDoc::OnUpdateXMITMPTOnly(CmdUI *p)
{
	if (p)
		p->Enable((m_SndFile.GetType() & (MOD_TYPE_XM | MOD_TYPE_IT | MOD_TYPE_MPT)) ? true : false);
}


// Enable menu item only for IT / MPTM files
void CModDoc::OnUpdateHasEditHistory(CmdUI *p)
{
	if (p)
		p->Enable(((m_SndFile.GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT)) || !m_SndFile.GetFileHistory().empty()) ? true : false);
}


// Enable menu item if current module type supports compatibility export
void CModDoc::OnUpdateCompatExportableOnly(CmdUI *p)
{
	if(p)
		p->Enable((m_SndFile.GetType() & (MOD_TYPE_XM | MOD_TYPE_IT)) ? true : false);
}


static mpt::ustring FormatSongLength(double length)
{
	length = mpt::round(length);
	double minutes = std::floor(length / 60.0), seconds = std::fmod(length, 60.0);
	mpt::ustring s;
	s = ui::Format(UL_("%.0fmn%02.0fs"), minutes, seconds);
	return s;
}


void CModDoc::OnEstimateSongLength()
{
	mpt::ustring s = UL_("Approximate song length: ");
	const auto subSongs = m_SndFile.GetAllSubSongs();
	if (subSongs.empty())
	{
		Reporting::Information(UL_("No patterns found!"));
		return;
	}

	std::vector<uint32> songsPerSequence(m_SndFile.Order.GetNumSequences(), 0);
	SEQUENCEINDEX prevSeq = subSongs[0].sequence;
	for(const auto &song : subSongs)
	{
		songsPerSequence[song.sequence]++;
		if(prevSeq != song.sequence)
			prevSeq = SEQUENCEINDEX_INVALID;
	}

	double totalLength = 0.0;
	uint32 songCount = 0;
	// If there are multiple sequences, indent their subsongs
	const mpt::uchar *indent = (prevSeq == SEQUENCEINDEX_INVALID) ? UL_("\t") : UL_("");
	for(const auto &song : subSongs)
	{
		double songLength = song.duration;
		if(subSongs.size() > 1)
		{
			totalLength += songLength;
			if(prevSeq != song.sequence)
			{
				songCount = 0;
				prevSeq = song.sequence;
				if(m_SndFile.Order(prevSeq).GetName().empty())
					s += ui::Format(UL_("\nSequence %u:"), prevSeq + 1u);
				else
					s += ui::Format(UL_("\nSequence %u (%s):"), prevSeq + 1u, mpt::ToUnicode(m_SndFile.Order(prevSeq).GetName()).c_str());
			}
			songCount++;
			if(songsPerSequence[song.sequence] > 1)
				s += ui::Format(UL_("\n%sSong %u, starting at order %s:\t"), indent, songCount, FormatOrderRow(song.startOrder).c_str());
			else
				s.push_back(UL_('\t'));
		}
		if(songLength != std::numeric_limits<double>::infinity())
		{
			songLength = mpt::round(songLength);
			s += FormatSongLength(songLength);
		} else
		{
			s += UL_("Song too long!");
		}
	}
	if(subSongs.size() > 1 && totalLength != std::numeric_limits<double>::infinity())
	{
		s += UL_("\n\nTotal length:\t") + FormatSongLength(totalLength);
	}

	Reporting::Information(s);
}


void CModDoc::OnApproximateBPM()
{
	if(CMainFrame::GetMainFrame()->GetModPlaying() != this)
	{
		m_SndFile.m_PlayState.m_nCurrentRowsPerBeat = m_SndFile.m_nDefaultRowsPerBeat;
		m_SndFile.m_PlayState.m_nCurrentRowsPerMeasure = m_SndFile.m_nDefaultRowsPerMeasure;
	}
	m_SndFile.RecalculateSamplesPerTick();
	const double bpm = m_SndFile.GetCurrentBPM();
	const ROWINDEX rowsPerBeat = m_SndFile.m_PlayState.m_nCurrentRowsPerBeat ? m_SndFile.m_PlayState.m_nCurrentRowsPerBeat : DEFAULT_ROWS_PER_BEAT;

	mpt::ustring s;
	switch(m_SndFile.m_nTempoMode)
	{
		case TempoMode::Alternative:
			s = ui::Format(UL_("Using alternative tempo interpretation.\n\nAssuming:\n. %.8g ticks per second\n. %u ticks per row\n. %u rows per beat\nthe tempo is approximately: %.8g BPM"),
			m_SndFile.m_PlayState.m_nMusicTempo.ToDouble(), m_SndFile.m_PlayState.m_nMusicSpeed, rowsPerBeat, bpm);
			break;

		case TempoMode::Modern:
			s = ui::Format(UL_("Using modern tempo interpretation.\n\nThe tempo is: %.8g BPM"), bpm);
			break;

		case TempoMode::Classic:
		default:
			s = ui::Format(UL_("Using standard tempo interpretation.\n\nAssuming:\n. A mod tempo (tick duration factor) of %.8g\n. %u ticks per row\n. %u rows per beat\nthe tempo is approximately: %.8g BPM"),
			m_SndFile.m_PlayState.m_nMusicTempo.ToDouble(), m_SndFile.m_PlayState.m_nMusicSpeed, rowsPerBeat, bpm);
			break;
	}

	Reporting::Information(s);
}


CChildFrame *CModDoc::GetChildFrame()
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if (!pMainFrm) return nullptr;
	ChildFrameBase *pMDIActive = pMainFrm->MDIGetActive();
	if (pMDIActive)
	{
		View *pView = pMDIActive->GetActiveView();
		if ((pView) && (pView->GetDocument() == this))
			return static_cast<CChildFrame *>(pMDIActive);
	}
	for(View *pView : GetViews())
	{
		if ((pView) && (pView->GetDocument() == this))
			return static_cast<CChildFrame *>(pView->GetParentFrame());
	}

	return nullptr;
}


// Get the currently edited pattern position. Note that ord might be ORDERINDEX_INVALID when editing a pattern that is not present in the order list.
void CModDoc::GetEditPosition(ROWINDEX &row, PATTERNINDEX &pat, ORDERINDEX &ord)
{
	CChildFrame *pChildFrm = GetChildFrame();

	if(pChildFrm->IsPatternView())
	{
		PatternViewState patternViewState;
		pChildFrm->SendViewMessage(VIEWMSG_SAVESTATE, (LParam)(&patternViewState));

		pat = patternViewState.nPattern;
		row = patternViewState.cursor.GetRow();
		ord = patternViewState.nOrder;
	} else
	{
		//patern editor object does not exist (i.e. is not active)  - use saved state.
		PatternViewState &patternViewState = pChildFrm->GetPatternViewState();

		pat = patternViewState.nPattern;
		row = patternViewState.cursor.GetRow();
		ord = patternViewState.nOrder;
	}

	const auto &order = m_SndFile.Order();
	if(order.empty())
	{
		ord = ORDERINDEX_INVALID;
		pat = 0;
		row = 0;
	} else if(ord >= order.size())
	{
		ord = 0;
		pat = m_SndFile.Order()[ord];
	}
	if(!m_SndFile.Patterns.IsValidPat(pat))
	{
		pat = 0;
		row = 0;
	} else if(row >= m_SndFile.Patterns[pat].GetNumRows())
	{
		row = 0;
	}

	//ensure order correlates with pattern.
	if(ord >= order.size() || order[ord] != pat)
	{
		ord = order.FindOrder(pat);
	}
}


////////////////////////////////////////////////////////////////////////////////////////
// Playback


void CModDoc::OnPatternRestart(bool loop)
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	CChildFrame *pChildFrm = GetChildFrame();

	if ((pMainFrm) && (pChildFrm))
	{
		if(pChildFrm->IsPatternView())
		{
			//User has sent play pattern command: set loop pattern checkbox to true.
			pChildFrm->SendViewMessage(VIEWMSG_PATTERNLOOP, loop ? 1 : 0);
		}

		ROWINDEX nRow;
		PATTERNINDEX nPat;
		ORDERINDEX nOrd;
		GetEditPosition(nRow, nPat, nOrd);
		CModDoc *pModPlaying = pMainFrm->GetModPlaying();

		TrackerCriticalSection cs;

		// Cut instruments/samples
		for(auto &chn : m_SndFile.m_PlayState.Chn)
		{
			chn.nPatternLoopCount = 0;
			chn.nPatternLoop = 0;
			chn.nFadeOutVol = 0;
			chn.dwFlags.set(CHN_NOTEFADE | CHN_KEYOFF);
		}
		if ((nOrd < m_SndFile.Order().size()) && (m_SndFile.Order()[nOrd] == nPat)) m_SndFile.m_PlayState.m_nCurrentOrder = m_SndFile.m_PlayState.m_nNextOrder = nOrd;
		m_SndFile.m_PlayState.m_flags.reset(SONG_PAUSED | SONG_STEP);
		if(loop)
			m_SndFile.LoopPattern(nPat);
		else
			m_SndFile.LoopPattern(PATTERNINDEX_INVALID);

		// set playback timer in the status bar (and update channel status)
		SetElapsedTime(nOrd, 0, true);

		if(pModPlaying == this)
		{
			m_SndFile.StopAllVsti();
		}

		cs.Leave();

		if(pModPlaying != this)
		{
			SetNotifications(m_notifyType | Notification::Position | Notification::VUMeters, m_notifyItem);
			SetFollowWnd(pChildFrm->GetHwndView());
			pMainFrm->PlayMod(this); //rewbs.fix2977
		}
	}
	//SwitchToView();
}

void CModDoc::OnPatternPlay()
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	CChildFrame *pChildFrm = GetChildFrame();

	if ((pMainFrm) && (pChildFrm))
	{
		if(pChildFrm->IsPatternView())
		{
			//User has sent play pattern command: set loop pattern checkbox to true.
			pChildFrm->SendViewMessage(VIEWMSG_PATTERNLOOP, 1);
		}

		ROWINDEX nRow;
		PATTERNINDEX nPat;
		ORDERINDEX nOrd;
		GetEditPosition(nRow, nPat, nOrd);
		CModDoc *pModPlaying = pMainFrm->GetModPlaying();

		TrackerCriticalSection cs;

		// Cut instruments/samples
		for(ModChannel &chn : m_SndFile.m_PlayState.BackgroundChannels(m_SndFile))
		{
			chn.dwFlags.set(CHN_NOTEFADE | CHN_KEYOFF);
		}
		if ((nOrd < m_SndFile.Order().size()) && (m_SndFile.Order()[nOrd] == nPat)) m_SndFile.m_PlayState.m_nCurrentOrder = m_SndFile.m_PlayState.m_nNextOrder = nOrd;
		m_SndFile.m_PlayState.m_flags.reset(SONG_PAUSED | SONG_STEP);
		m_SndFile.LoopPattern(nPat);

		// set playback timer in the status bar (and update channel status)
		SetElapsedTime(nOrd, nRow, true);

		if(pModPlaying == this)
		{
			m_SndFile.StopAllVsti();
		}

		cs.Leave();

		if(pModPlaying != this)
		{
			SetNotifications(m_notifyType | Notification::Position | Notification::VUMeters, m_notifyItem);
			SetFollowWnd(pChildFrm->GetHwndView());
			pMainFrm->PlayMod(this);  //rewbs.fix2977
		}
	}
	//SwitchToView();

}

void CModDoc::OnPatternPlayNoLoop()
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	CChildFrame *pChildFrm = GetChildFrame();

	if ((pMainFrm) && (pChildFrm))
	{
		if(pChildFrm->IsPatternView())
		{
			//User has sent play song command: set loop pattern checkbox to false.
			pChildFrm->SendViewMessage(VIEWMSG_PATTERNLOOP, 0);
		}

		ROWINDEX nRow;
		PATTERNINDEX nPat;
		ORDERINDEX nOrd;
		GetEditPosition(nRow, nPat, nOrd);
		CModDoc *pModPlaying = pMainFrm->GetModPlaying();

		TrackerCriticalSection cs;
		// Cut instruments/samples
		for(ModChannel &chn : m_SndFile.m_PlayState.BackgroundChannels(m_SndFile))
		{
			chn.dwFlags.set(CHN_NOTEFADE | CHN_KEYOFF);
		}
		m_SndFile.m_PlayState.m_flags.reset(SONG_PAUSED | SONG_STEP);
		m_SndFile.SetCurrentOrder(nOrd);
		if(nOrd < m_SndFile.Order().size() && m_SndFile.Order()[nOrd] == nPat)
			m_SndFile.DontLoopPattern(nPat, nRow);
		else
			m_SndFile.LoopPattern(nPat);

		// set playback timer in the status bar (and update channel status)
		SetElapsedTime(nOrd, nRow, true);

		if(pModPlaying == this)
		{
			m_SndFile.StopAllVsti();
		}

		cs.Leave();

		if(pModPlaying != this)
		{
			SetNotifications(m_notifyType | Notification::Position | Notification::VUMeters, m_notifyItem);
			SetFollowWnd(pChildFrm->GetHwndView());
			pMainFrm->PlayMod(this);  //rewbs.fix2977
		}
	}
	//SwitchToView();
}


void CModDoc::OnViewEditHistory()
{
	CEditHistoryDlg dlg(CMainFrame::GetMainFrame(), *this);
	dlg.DoModal();
}


void CModDoc::OnViewMPTHacks()
{
	ScopedLogCapturer logcapturer(*this);
	if(!HasMPTHacks())
	{
		AddToLog("No hacks found.");
	}
}


void CModDoc::OnViewTempoSwingSettings()
{
	if(m_SndFile.m_nDefaultRowsPerBeat > 0 && m_SndFile.m_nTempoMode == TempoMode::Modern)
	{
		TempoSwing tempoSwing = m_SndFile.m_tempoSwing;
		tempoSwing.resize(m_SndFile.m_nDefaultRowsPerBeat, TempoSwing::Unity);
		CTempoSwingDlg dlg(CMainFrame::GetMainFrame(), tempoSwing, m_SndFile);
		if(dlg.DoModal() == IDOK)
		{
			SetModified();
			m_SndFile.m_tempoSwing = dlg.m_tempoSwing;
		}
	} else if(GetModType() == MOD_TYPE_MPT)
	{
		Reporting::Error(UL_("Modern tempo mode needs to be enabled in order to edit tempo swing settings."));
		OnSongProperties();
	}
}


LResult CModDoc::OnCustomKeyMsg(WParam wParam, LParam /*lParam*/)
{
	const auto &modSpecs = m_SndFile.GetModSpecifications();
	switch(wParam)
	{
		case kcViewGeneral: OnEditGlobals(); break;
		case kcViewPattern: OnEditPatterns(); break;
		case kcViewSamples: OnEditSamples(); break;
		case kcViewInstruments: OnEditInstruments(); break;
		case kcViewComments: OnEditComments(); break;
		case kcViewSongProperties: OnSongProperties(); break;
		case kcViewTempoSwing: OnViewTempoSwingSettings(); break;
		case kcShowMacroConfig:	OnSetupZxxMacros(); break;
		case kcViewMIDImapping: OnViewMIDIMapping(); break;
		case kcViewEditHistory:	OnViewEditHistory(); break;
		case kcViewChannelManager: OnChannelManager(); break;

		case kcFileSaveAsWave:	OnFileWaveConvert(); break;
		case kcFileSaveMidi:	OnFileMidiConvert(); break;
		case kcFileSaveOPL:		OnFileOPLExport(); break;
		case kcFileExportCompat:  OnFileCompatibilitySave(); break;
		case kcEstimateSongLength: OnEstimateSongLength(); break;
		case kcApproxRealBPM:	OnApproximateBPM(); break;
		case kcFileSave:		DoSave(GetPathNameMpt()); break;
		case kcFileSaveAs:		DoSave(mpt::PathString()); break;
		case kcFileSaveCopy:	OnSaveCopy(); break;
		case kcFileSaveTemplate: OnSaveTemplateModule(); break;
		case kcFileClose:		SafeFileClose(); break;
		case kcFileAppend:		OnAppendModule(); break;

		case kcPlayPatternFromCursor: OnPatternPlay(); break;
		case kcPlayPatternFromStart: OnPatternRestart(); break;
		case kcPlaySongFromCursorPause:
			if(CMainFrame::GetMainFrame()->GetModPlaying() == this && !m_SndFile.m_PlayState.m_flags[SONG_PAUSED | SONG_STEP])
			{
				OnPlayerPause();
				break;
			}
			[[fallthrough]];
		case kcPlaySongFromCursor: OnPatternPlayNoLoop(); break;
		case kcPlaySongFromStart: OnPlayerPlayFromStart(); break;
		case kcPlayStopSong:
			if(CMainFrame::GetMainFrame()->GetModPlaying() == this && !m_SndFile.m_PlayState.m_flags[SONG_PAUSED | SONG_STEP])
			{
				OnPlayerStop();
				break;
			}
			[[fallthrough]];
		case kcPlayPauseSong: OnPlayerPlay(); break;
		case kcPlaySongFromPatternPause:
			if(CMainFrame::GetMainFrame()->GetModPlaying() == this && !m_SndFile.m_PlayState.m_flags[SONG_PAUSED | SONG_STEP])
			{
				OnPlayerPause();
				break;
			}
			[[fallthrough]];
		case kcPlaySongFromPattern: OnPatternRestart(false); break;
		case kcStopSong: OnPlayerStop(); break;
		case kcPanic: OnPanic(); break;
		case kcToggleLoopSong: SetLoopSong(!TrackerSettings::Instance().gbLoopSong); break;

		case kcTempoIncreaseFine:
		case kcTempoIncrease:
			if(auto tempo = m_SndFile.m_PlayState.m_nMusicTempo; tempo < modSpecs.GetTempoMax())
				m_SndFile.m_PlayState.m_nMusicTempo = std::min(modSpecs.GetTempoMax(), tempo + TEMPO(wParam == kcTempoIncrease ? 1.0 : 0.1));
			break;
		case kcTempoDecreaseFine:
		case kcTempoDecrease:
			if(auto tempo = m_SndFile.m_PlayState.m_nMusicTempo; tempo > modSpecs.GetTempoMin())
				m_SndFile.m_PlayState.m_nMusicTempo = std::max(modSpecs.GetTempoMin(), tempo - TEMPO(wParam == kcTempoDecrease ? 1.0 : 0.1));
			break;
		case kcSpeedIncrease:
			if(auto speed = m_SndFile.m_PlayState.m_nMusicSpeed; speed < modSpecs.speedMax)
				m_SndFile.m_PlayState.m_nMusicSpeed = speed + 1;
			break;
		case kcSpeedDecrease:
			if(auto speed = m_SndFile.m_PlayState.m_nMusicSpeed; speed > modSpecs.speedMin)
				m_SndFile.m_PlayState.m_nMusicSpeed = speed -  1;
			break;

		case kcViewToggle:
			if(auto *lastActiveFrame = CChildFrame::LastActiveFrame(); lastActiveFrame != nullptr)
				lastActiveFrame->ToggleViews();
			break;

		default: return kcNull;
	}

	return wParam;
}


void CModDoc::TogglePluginEditor(uint32 plugin, bool onlyThisEditor)
{
	if(plugin < MAX_MIXPLUGINS)
	{
		IMixPlugin *pPlugin = m_SndFile.m_MixPlugins[plugin].pMixPlugin;
		if(pPlugin != nullptr)
		{
			if(onlyThisEditor)
			{
				int32 posX = int32_min, posY = int32_min;
				for(PLUGINDEX i = 0; i < MAX_MIXPLUGINS; i++)
				{
					SNDMIXPLUGIN &otherPlug = m_SndFile.m_MixPlugins[i];
					if(i != plugin && otherPlug.pMixPlugin != nullptr && PluginUi(*otherPlug.pMixPlugin).GetEditor() != nullptr)
					{
						PluginUi(*otherPlug.pMixPlugin).CloseEditor();
						if(otherPlug.editorX != int32_min)
						{
							posX = otherPlug.editorX;
							posY = otherPlug.editorY;
						}
					}
				}
				if(posX != int32_min)
				{
					m_SndFile.m_MixPlugins[plugin].editorX = posX;
					m_SndFile.m_MixPlugins[plugin].editorY = posY;
				}
			}

			PluginUi(*pPlugin).ToggleEditor();
		}
	}
}


void CModDoc::SetLoopSong(bool loop)
{
	TrackerSettings::Instance().gbLoopSong = loop;
	m_SndFile.SetRepeatCount(loop ? -1 : 0);
	theApp.UpdateAllViews(UpdateHint().MPTOptions());
}


void CModDoc::ChangeFileExtension(MODTYPE nNewType)
{
	//Not making path if path is empty(case only(?) for new file)
	if(!GetPathNameMpt().empty())
	{
		mpt::PathString drive;
		mpt::PathString dir;
		mpt::PathString fname;
		mpt::PathString fext;
		GetPathNameMpt().SplitPath(nullptr, &drive, &dir, &fname, &fext);

		mpt::PathString newPath = drive + dir;

		// Catch case where we don't have a filename yet.
		if(fname.empty())
		{
			newPath += mpt::PathString::FromUnicode(GetTitle()).AsSanitizedComponent();
		} else
		{
			newPath += fname;
		}

		newPath += P_(".") + mpt::PathString::FromUnicode(CSoundFile::GetModSpecifications(nNewType).GetFileExtension());

		// Forcing save dialog to appear after extension change - otherwise unnotified file overwriting may occur.
		m_ShowSavedialog = true;

		SetPathName(newPath, false);
	}

	UpdateAllViews(nullptr, UpdateHint().ModType());
}


CHANNELINDEX CModDoc::FindAvailableChannel() const
{
	CHANNELINDEX chn = m_SndFile.GetNNAChannel(CHANNELINDEX_INVALID);
	if(chn != CHANNELINDEX_INVALID)
		return chn;
	else
		return GetNumChannels();
}


void CModDoc::RecordParamChange(PLUGINDEX plugSlot, PlugParamIndex paramIndex)
{
	if(m_hWndFollow)
		m_hWndFollow->PostMessage(MSG_MOD_RECORDPARAM, plugSlot, paramIndex);
}


void CModDoc::LearnMacro(int macroToSet, PlugParamIndex paramToUse)
{
	if(macroToSet < 0 || macroToSet > kSFxMacros)
	{
		return;
	}

	// If macro already exists for this param, inform user and return
	if(auto macro = FindMacroForParam(m_SndFile.m_MidiCfg, paramToUse); macro >= 0)
	{
		mpt::ustring message;
		message = ui::Format(UL_("Parameter %i can already be controlled with macro %X."), static_cast<int>(paramToUse), macro);
		Reporting::Information(message, UL_("Macro exists for this parameter"));
		return;
	}

	// Set new macro
	if(paramToUse < 384)
	{
		m_SndFile.m_MidiCfg.CreateParameteredMacro(macroToSet, kSFxPlugParam, paramToUse);
	} else
	{
		mpt::ustring message;
		message = ui::Format(UL_("Parameter %i beyond controllable range. Use Parameter Control Events to automate this parameter."), static_cast<int>(paramToUse));
		Reporting::Information(message, UL_("Macro not assigned for this parameter"));
		return;
	}

	mpt::ustring message;
	message = ui::Format(UL_("Parameter %i can now be controlled with macro %X."), static_cast<int>(paramToUse), macroToSet);
	Reporting::Information(message, UL_("Macro assigned for this parameter"));

	return;
}


void CModDoc::OnSongProperties()
{
	const bool wasUsingFrequencies = m_SndFile.PeriodsAreFrequencies();
	CModTypeDlg dlg(m_SndFile, CMainFrame::GetMainFrame());
	if(dlg.DoModal() == IDOK)
	{
		UpdateAllViews(nullptr, GeneralHint().General());
		ScopedLogCapturer logcapturer(*this, UL_("Conversion Status"));
		if(dlg.m_nType != GetModType())
		{
			if(!ChangeModType(dlg.m_nType))
				return;
		}

		CHANNELINDEX newChannels = Clamp(dlg.m_nChannels, m_SndFile.GetModSpecifications().channelsMin, m_SndFile.GetModSpecifications().channelsMax);
		if(newChannels != GetNumChannels())
		{
			const bool showCancelInRemoveDlg = m_SndFile.GetModSpecifications().channelsMax >= m_SndFile.GetNumChannels();
			ChangeNumChannels(newChannels, showCancelInRemoveDlg);

			// Force update of pattern highlights / num channels
			UpdateAllViews(nullptr, PatternHint().Data());
			UpdateAllViews(nullptr, GeneralHint().Channels());
		}

		if(wasUsingFrequencies != m_SndFile.PeriodsAreFrequencies())
		{
			for(auto &chn : m_SndFile.m_PlayState.Chn)
			{
				chn.nPeriod = 0;
			}
		}

		SetModified();
	}
}


void CModDoc::ViewMIDIMapping(PLUGINDEX plugin, PlugParamIndex param)
{
	CMIDIMappingDialog dlg(CMainFrame::GetMainFrame(), m_SndFile);
	if(plugin != PLUGINDEX_INVALID)
	{
		dlg.m_Setting.SetPlugIndex(plugin + 1);
		dlg.m_Setting.SetParamIndex(param);
	}
	dlg.DoModal();
}


void CModDoc::OnChannelManager()
{
	CChannelManagerDlg *instance = CChannelManagerDlg::sharedInstanceCreate();
	if(instance != nullptr)
	{
		if(instance->IsDisplayed())
			instance->Hide();
		else
		{
			instance->SetDocument(this);
			instance->Show();
		}
	}
}


// Sets playback timer to playback time at given position.
// At the same time, the playback parameters (global volume, channel volume and stuff like that) are calculated for this position.
// Sample channels positions are only updated if setSamplePos is true *and* the user has chosen to update sample play positions on seek.
void CModDoc::SetElapsedTime(ORDERINDEX nOrd, ROWINDEX nRow, bool setSamplePos)
{
	if(nOrd == ORDERINDEX_INVALID) return;

	double t = m_SndFile.GetPlaybackTimeAt(nOrd, nRow, true, setSamplePos && (TrackerSettings::Instance().patternSetup & PatternSetup::SampleSyncOnSeek));
	if(t < 0)
	{
		// Position is never played regularly, but we may want to continue playing from here nevertheless.
		m_SndFile.m_PlayState.m_nCurrentOrder = m_SndFile.m_PlayState.m_nNextOrder = nOrd;
		m_SndFile.m_PlayState.m_nRow = m_SndFile.m_PlayState.m_nNextRow = nRow;
	}

	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if(pMainFrm != nullptr) pMainFrm->SetElapsedTime(std::max(0.0, t));
}


mpt::ustring CModDoc::GetPatternViewInstrumentName(INSTRUMENTINDEX nInstr,
											  bool bEmptyInsteadOfNoName /* = false*/,
											  bool bIncludeIndex /* = true*/) const
{
	if(nInstr >= MAX_INSTRUMENTS || m_SndFile.GetNumInstruments() == 0 || m_SndFile.Instruments[nInstr] == nullptr)
		return mpt::ustring();

	mpt::ustring displayName, instrumentName, pluginName;

	// Get instrument name.
	instrumentName = mpt::ToUnicode(m_SndFile.GetCharsetInternal(), m_SndFile.GetInstrumentName(nInstr));

	// If instrument name is empty, use name of the sample mapped to C-5.
	if (instrumentName.empty())
	{
		const SAMPLEINDEX nSmp = m_SndFile.Instruments[nInstr]->Keyboard[NOTE_MIDDLEC - 1];
		if (nSmp <= m_SndFile.GetNumSamples() && m_SndFile.GetSample(nSmp).HasSampleData())
			instrumentName = UL_("s: ") + mpt::ToUnicode(m_SndFile.GetCharsetInternal(), m_SndFile.GetSampleName(nSmp));
	}

	// Get plugin name.
	const PLUGINDEX nPlug = m_SndFile.Instruments[nInstr]->nMixPlug;
	if (nPlug > 0 && nPlug < MAX_MIXPLUGINS)
		pluginName = mpt::ToUnicode(m_SndFile.m_MixPlugins[nPlug-1].GetName());

	if (pluginName.empty())
	{
		if(bEmptyInsteadOfNoName && instrumentName.empty())
			return UL_("");
		if(instrumentName.empty())
			instrumentName = UL_("(no name)");
		if (bIncludeIndex)
			displayName = ui::Format(UL_("%02d: %s"), nInstr, instrumentName.c_str());
		else
			displayName = instrumentName;
	} else
	{
		if (bIncludeIndex)
			displayName = ui::Format(UL_("%02d: %s (%s)"), nInstr, instrumentName.c_str(), pluginName.c_str());
		else
			displayName = ui::Format(UL_("%s (%s)"), instrumentName.c_str(), pluginName.c_str());
	}
	return displayName;
}


mpt::ustring CModDoc::FormatSubsongName(const std::vector<SubSong> &songs, size_t subSong)
{
	if(subSong >= songs.size())
		return {};
	const SubSong &song = songs[subSong];
	size_t subsongInSequence = 1;
	for(size_t i = 1; i <= subSong; i++)
	{
		if(songs[subSong - i].sequence == song.sequence)
			subsongInSequence++;
		else
			break;
	}
	const auto sequenceName = m_SndFile.Order(song.sequence).GetName();
	const auto startPattern = m_SndFile.Order(song.sequence).PatternAt(song.startOrder);
	const auto orderName = startPattern ? startPattern->GetName() : std::string{};
	return MPT_UFORMAT("Sequence {}{}, Song {}\nOrder {} to {}{}")(
		song.sequence + 1,
		sequenceName.empty() ? mpt::ustring{} : MPT_UFORMAT(" ({})")(sequenceName),
		subsongInSequence,
		song.startOrder,
		song.endOrder == ORDERINDEX_INVALID ? song.startOrder : song.endOrder,
		orderName.empty() ? mpt::ustring{} : MPT_UFORMAT(" ({})")(mpt::ToUnicode(m_SndFile.GetCharsetInternal(), orderName)));
}


void CModDoc::SafeFileClose()
{
	// Verify that the main window has the focus. This saves us a lot of trouble because active modal dialogs cannot know if their pSndFile pointers are still valid.
	if(CMainFrame::GetMainFrame()->IsChild(Wnd::GetFocus()))
		OnFileClose();
}


// "Panic button". This resets all VSTi, OPL and sample notes.
void CModDoc::OnPanic()
{
	TrackerCriticalSection cs;
	m_SndFile.ResetChannels();
	m_SndFile.StopAllVsti();
}


// Before saving, make sure that every char after the terminating null char is also null.
// Else, garbage might end up in various text strings that wasn't supposed to be there.
void CModDoc::FixNullStrings()
{
	// Macros
	m_SndFile.m_MidiCfg.Sanitize();
}


void CModDoc::OnSaveCopy()
{
	DoSave(mpt::PathString(), false);
}


void CModDoc::OnSaveTemplateModule()
{
	// Create template folder if doesn't exist already.
	const mpt::PathString templateFolder = theApp.GetUserTemplatesPath();
	if (!FileSystem::IsDirectory(templateFolder))
	{
		if (!Util::CreateDirectory(templateFolder))
		{
			Reporting::Notification(MPT_UFORMAT("Error: Unable to create template folder '{}'")( templateFolder));
			return;
		}
	}

	// Generate file name candidate.
	mpt::PathString sName;
	for(size_t i = 0; i < 1000; ++i)
	{
		sName += P_("newTemplate") + mpt::PathString::FromUnicode(mpt::ufmt::val(i));
		sName += P_(".") + mpt::PathString::FromUnicode(m_SndFile.GetModSpecifications().GetFileExtension());
		if (!FileSystem::Exists(templateFolder + sName))
			break;
	}

	// Ask file name from user.
	FileDialog dlg = SaveFileDialog()
		.DefaultExtension(m_SndFile.GetModSpecifications().GetFileExtension())
		.DefaultFilename(sName)
		.ExtensionFilter(ModTypeToFilter(m_SndFile))
		.WorkingDirectory(templateFolder);
	if(!dlg.Show())
		return;

	if (OnSaveDocument(dlg.GetFirstFile(), false))
	{
		// Update template menu.
		CMainFrame::GetMainFrame()->CreateTemplateModulesMenu();
	}
}


// Create an undo point that stores undo data for all existing patterns
void CModDoc::PrepareUndoForAllPatterns(bool storeChannelInfo, const char *description)
{
	bool linkUndo = false;

	PATTERNINDEX lastPat = 0;
	for(PATTERNINDEX pat = 0; pat < m_SndFile.Patterns.Size(); pat++)
	{
		if(m_SndFile.Patterns.IsValidPat(pat)) lastPat = pat;
	}

	for(PATTERNINDEX pat = 0; pat <= lastPat; pat++)
	{
		if(m_SndFile.Patterns.IsValidPat(pat))
		{
			GetPatternUndo().PrepareUndo(pat, 0, 0, GetNumChannels(), m_SndFile.Patterns[pat].GetNumRows(), description, linkUndo, storeChannelInfo && pat == lastPat);
			linkUndo = true;
		}
	}
}



mpt::ustring CModDoc::LinearToDecibelsString(double value, double valueAtZeroDB)
{
	return DecibelsToStrings(LinearToDecibels(value, valueAtZeroDB));
}


double CModDoc::LinearToDecibels(double value, double valueAtZeroDB)
{
	if(value == 0 || valueAtZeroDB == 0)
		return -std::numeric_limits<double>::infinity();

	double changeFactor = value / valueAtZeroDB;
	return 20.0 * std::log10(changeFactor);
}


double CModDoc::DecibelsToLinear(double value, double valueAtZeroDB)
{
	return valueAtZeroDB * std::pow(10.0, value / 20.0);
}


mpt::ustring CModDoc::DecibelsToStrings(double dB)
{
	if(std::isinf(dB))
		return dB < 0 ? UL_("-inf") : UL_("inf");

	mpt::ustring s = (dB >= 0) ? UL_("+") : UL_("");
	s += ui::Format(UL_("%.2f dB"), dB);
	return s;
}


mpt::ustring CModDoc::PanningToString(int32 value, int32 valueAtCenter)
{
	if(value == valueAtCenter)
		return UL_("Center");

	mpt::ustring s;
	s = ui::Format(UL_("%i%% %s"), (std::abs(static_cast<int>(value) - valueAtCenter) * 100) / valueAtCenter, value < valueAtCenter ? UL_("Left") : UL_("Right"));
	return s;
}


// Apply OPL patch changes to live playback
void CModDoc::UpdateOPLInstrument(SAMPLEINDEX smp)
{
	const ModSample &sample = m_SndFile.GetSample(smp);
	if(!sample.uFlags[CHN_ADLIB] || !m_SndFile.m_opl || CMainFrame::GetMainFrame()->GetModPlaying() != this)
		return;

	TrackerCriticalSection cs;
	const auto &patch = sample.adlib;
	for(CHANNELINDEX chn = 0; chn < MAX_CHANNELS; chn++)
	{
		const auto &c = m_SndFile.m_PlayState.Chn[chn];
		if(c.pModSample == &sample && c.IsSamplePlaying())
		{
			m_SndFile.m_opl->Patch(chn, patch);
		}
	}
}


size_t CModDoc::GetSubsongForCurrentEditPos(const std::vector<SubSong> &subsongs) const
{
	const SEQUENCEINDEX seq = m_SndFile.Order.GetCurrentSequenceIndex();
	ORDERINDEX ord = 0;
	if(auto *lastActiveFrame = CChildFrame::LastActiveFrame(); lastActiveFrame != nullptr && lastActiveFrame->GetActiveDocument() == this)
	{
		if(lastActiveFrame->IsPatternView())
			lastActiveFrame->SaveAllViewStates();
		ord = lastActiveFrame->GetPatternViewState().nOrder;
	}

	// Note: This is just an estimation. If subsongs have overlapping order ranges
	// (like in Unreal Engine modules where the first pattern of each subsong is found at the start of the order list),
	// then we may return the wrong subsong index.
	size_t candidate = subsongs.size();
	for(size_t i = 0; i < subsongs.size(); i++)
	{
		const SubSong &subsong = subsongs[i];
		if(subsong.sequence != seq)
			continue;
		if(mpt::is_in_range(ord, subsong.startOrder, subsong.endOrder))
			return i;
		candidate = i;
	}
	return candidate;
}


// Store all view positions t settings file
void CModDoc::SerializeViews() const
{
	const mpt::PathString pathName = theApp.IsPortableMode() ? mpt::AbsolutePathToRelative(GetPathNameMpt(), theApp.GetInstallPath()) : GetPathNameMpt();
	if(pathName.empty())
	{
		return;
	}
	std::ostringstream f(std::ios::out | std::ios::binary);

	const Rect mdiRect = CMainFrame::GetMainFrame()->GetMDIClientRect();
	const int width = mdiRect.Width();
	const int height = mdiRect.Height();

	const int cxScreen = ui::GetScreenSize().cx, cyScreen = ui::GetScreenSize().cy;

	// Document view positions and sizes
	for(View *view : GetViews())
	{
		if(mdiRect.IsRectEmpty())
			break;
		CModControlView *pView = dynamic_cast<CModControlView *>(view);
		if(pView)
		{
			CChildFrame *pChildFrm = (CChildFrame *)pView->GetParentFrame();
			WINDOWPLACEMENT wnd;
			wnd.length = sizeof(WINDOWPLACEMENT);
			pChildFrm->GetWindowPlacement(&wnd);
			const Rect rect = wnd.rcNormalPosition;

			// Write size information
			uint8 windowState = 0;
			if(wnd.showCmd == SW_SHOWMAXIMIZED) windowState = 1;
			else if(wnd.showCmd == SW_SHOWMINIMIZED) windowState = 2;
			mpt::IO::WriteIntLE<uint8>(f, 0);	// Window type
			mpt::IO::WriteIntLE<uint8>(f, windowState);
			mpt::IO::WriteIntLE<int32>(f, Util::muldivr(rect.left, 1 << 30, width));
			mpt::IO::WriteIntLE<int32>(f, Util::muldivr(rect.top, 1 << 30, height));
			mpt::IO::WriteIntLE<int32>(f, Util::muldivr(rect.Width(), 1 << 30, width));
			mpt::IO::WriteIntLE<int32>(f, Util::muldivr(rect.Height(), 1 << 30, height));

			std::string s = pChildFrm->SerializeView();
			mpt::IO::WriteVarInt(f, s.size());
			f << s;
		}
	}
	// Plugin window positions
	for(PLUGINDEX i = 0; i < MAX_MIXPLUGINS; i++)
	{
		if(m_SndFile.m_MixPlugins[i].IsValidPlugin() && m_SndFile.m_MixPlugins[i].editorX != int32_min && cxScreen && cyScreen)
		{
			// Translate screen position into percentage (to make it independent of the actual screen resolution)
			int32 editorX = Util::muldivr(m_SndFile.m_MixPlugins[i].editorX, 1 << 30, cxScreen);
			int32 editorY = Util::muldivr(m_SndFile.m_MixPlugins[i].editorY, 1 << 30, cyScreen);

			mpt::IO::WriteIntLE<uint8>(f, 1);	// Window type
			mpt::IO::WriteIntLE<uint8>(f, 0);	// Version
			mpt::IO::WriteVarInt(f, i);
			mpt::IO::WriteIntLE<int32>(f, editorX);
			mpt::IO::WriteIntLE<int32>(f, editorY);
		}
	}

	SettingsContainer &settings = theApp.GetSongSettings();
	const std::string s = f.str();
	settings.Write(UL_("WindowSettings"), pathName.GetFilename().ToUnicode(), pathName);
	settings.Write(UL_("WindowSettings"), pathName.ToUnicode(), mpt::encode_hex(mpt::as_span(s)));
}


// Restore all view positions from settings file
void CModDoc::DeserializeViews()
{
	mpt::PathString pathName = GetPathNameMpt();
	if(pathName.empty()) return;

	SettingsContainer &settings = theApp.GetSongSettings();
	mpt::ustring s = settings.Read<mpt::ustring>(UL_("WindowSettings"), pathName.ToUnicode());
	if(s.size() < 2)
	{
		// Try relative path
		pathName = mpt::RelativePathToAbsolute(pathName, theApp.GetInstallPath());
		s = settings.Read<mpt::ustring>(UL_("WindowSettings"), pathName.ToUnicode());
		if(s.size() < 2)
		{
			// Try searching for filename instead of full path name
			const mpt::ustring altName = settings.Read<mpt::ustring>(UL_("WindowSettings"), pathName.GetFilename().ToUnicode());
			s = settings.Read<mpt::ustring>(UL_("WindowSettings"), altName);
			if(s.size() < 2) return;
		}
	}
	std::vector<std::byte> bytes = mpt::decode_hex(s);

	FileReader file(mpt::as_span(bytes));

	const Rect mdiRect = CMainFrame::GetMainFrame()->GetMDIClientRect();
	const int width = mdiRect.Width();
	const int height = mdiRect.Height();

	const int cxScreen = ui::GetScreenSize().cx, cyScreen = ui::GetScreenSize().cy;

	CChildFrame *pChildFrm = nullptr;
	if(!GetViews().empty()) pChildFrm = dynamic_cast<CChildFrame *>(GetViews().front()->GetParentFrame());

	bool anyMaximized = false;
	while(file.CanRead(1))
	{
		const uint8 windowType = file.ReadUint8();
		if(windowType == 0)
		{
			// Document view positions and sizes
			const uint8 windowState = file.ReadUint8();
			Rect rect;
			rect.left = Util::muldivr(file.ReadInt32LE(), width, 1 << 30);
			rect.top = Util::muldivr(file.ReadInt32LE(), height, 1 << 30);
			rect.right = rect.left + Util::muldivr(file.ReadInt32LE(), width, 1 << 30);
			rect.bottom = rect.top + Util::muldivr(file.ReadInt32LE(), height, 1 << 30);
			size_t dataSize;
			file.ReadVarInt(dataSize);
			FileReader data = file.ReadChunk(dataSize);

			if(pChildFrm == nullptr)
			{
				CModDocTemplate *pTemplate = static_cast<CModDocTemplate *>(GetDocTemplate());
				
				pChildFrm = static_cast<CChildFrame *>(pTemplate->CreateNewFrame(*this));
				if(pChildFrm != nullptr)
				{
					pChildFrm->InitialUpdateFrame(this, true);
				}
			}
			if(pChildFrm != nullptr)
			{
				if(!mdiRect.IsRectEmpty())
				{
					WINDOWPLACEMENT wnd;
					wnd.length = sizeof(wnd);
					pChildFrm->GetWindowPlacement(&wnd);
					wnd.showCmd = true;
					if(windowState == 1 || anyMaximized)
					{
						// Once a window has been maximized, all following windows have to be marked as maximized as well.
						wnd.showCmd = ui::SW_SHOWMAXIMIZED;
						anyMaximized = true;
					} else if(windowState == 2)
					{
						wnd.showCmd = ui::SW_SHOWMINIMIZED;
					}
					if(rect.left < width && rect.right > 0 && rect.top < height && rect.bottom > 0)
					{
						wnd.rcNormalPosition = Rect(rect.left, rect.top, rect.right, rect.bottom);
					}
					pChildFrm->SetWindowPlacement(&wnd);
				}
				pChildFrm->DeserializeView(data);
				pChildFrm = nullptr;
			}
		} else if(windowType == 1)
		{
			if(file.ReadUint8() != 0)
				break;
			// Plugin window positions
			PLUGINDEX plug = 0;
			if(file.ReadVarInt(plug) && plug < MAX_MIXPLUGINS)
			{
				int32 editorX = file.ReadInt32LE();
				int32 editorY = file.ReadInt32LE();
				if(editorX != int32_min && editorY != int32_min)
				{
					m_SndFile.m_MixPlugins[plug].editorX = Util::muldivr(editorX, cxScreen, 1 << 30);
					m_SndFile.m_MixPlugins[plug].editorY = Util::muldivr(editorY, cyScreen, 1 << 30);
				}
			}
		} else
		{
			// Unknown type
			break;
		}
	}
}

OPENMPT_NAMESPACE_END
