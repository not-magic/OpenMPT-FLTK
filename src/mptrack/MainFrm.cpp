// FLTK port of openmpt/mptrack/MainFrm.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "Mainfrm.h"
#include "AdvancedConfigDlg.h"
#include "AutoSaver.h"
#include "ChannelManagerDlg.h"
#include "Childfrm.h"
#include "CloseMainDialog.h"
#include "ColorConfigDlg.h"
#include "dlg_misc.h"
#include "DlsBankExt.h"
#include "FileDialog.h"
#include "FolderScanner.h"
#include "GeneralConfigDlg.h"
#include "Globals.h"
#include "ImageLists.h"
#include "InputHandler.h"
#include "KeyConfigDlg.h"
#include "Moddoc.h"
#include "ModDocTemplate.h"
#include "Mpdlgs.h"
#include "Mptrack.h"
#include "PathConfigDlg.h"
#include "PatternClipboard.h"
#include "PatternFont.h"
#include "ProgressDialog.h"
#include "QuickStartDialog.h"
#include "Reporting.h"
#include "View_gen.h"
#include "View_ins.h"
#include "View_pat.h"
#include "View_smp.h"
#include "view_com.h"
#include "resource.h"
#include "SampleConfigDlg.h"
#include "SelectPluginDialog.h"
#include "WindowMessages.h"

#include "../common/FileReader.h"
#include "../common/mptFileIO.h"
#include "../common/Profiler.h"
#include "../common/version.h"
#include "../soundlib/AudioReadTarget.h"
#include "../soundlib/Tables.h"
#include "../soundlib/PlaybackTest.h"
#include "mpt/audio/span.hpp"
#include "mpt/base/alloc.hpp"
#include "mpt/io_file/fstream.hpp"
#include "mpt/io_file/inputfile.hpp"
#include "mpt/io_file_read/inputfile_filecursor.hpp"
#include "mpt/profiler/clock.hpp"
#include "mpt/profiler/frequency.hpp"
#include "mpt/profiler/profiler.hpp"
#include "mpt/string/utility.hpp"
#include "openmpt/sounddevice/SoundDevice.hpp"
#include "openmpt/sounddevice/SoundDeviceBuffer.hpp"
#include "openmpt/sounddevice/SoundDeviceManager.hpp"
#include "../include/rtmidi/RtMidi.h"

#ifdef MPT_ENABLE_PLAYBACK_TEST_MENU
#include "../unarchiver/ungzip.h"
#include "../misc/GzipWriter.h"
#endif



OPENMPT_NAMESPACE_BEGIN

static constexpr uint32 TIMERID_GUI = 1;
static constexpr uint32 TIMERID_NOTIFY = 2;

static constexpr uint32 MPTTIMER_PERIOD = 100;

/////////////////////////////////////////////////////////////////////////////
// CMainFrame


UI_MESSAGE_MAP_BEGIN(CMainFrame, MainFrameBase)

	UI_COMMAND(ID_VIEW_OPTIONS,      &CMainFrame::OnViewOptions)
	UI_COMMAND(ID_PLUGIN_SETUP,      &CMainFrame::OnPluginManager)
	UI_COMMAND(ID_CLIPBOARD_MANAGER, &CMainFrame::OnClipboardManager)
	//ON_COMMAND(ID_HELP,              &MainFrameBase::OnHelp)

	UI_COMMAND(ID_REPORT_BUG,     &CMainFrame::OnReportBug)
	UI_COMMAND(ID_NEXTOCTAVE,     &CMainFrame::OnNextOctave)
	UI_COMMAND(ID_PREVOCTAVE,     &CMainFrame::OnPrevOctave)
	UI_COMMAND(ID_ADD_SOUNDBANK,  &CMainFrame::OnAddDlsBank)
	UI_COMMAND(ID_IMPORT_MIDILIB, &CMainFrame::OnImportMidiLib)
	UI_COMMAND(ID_MIDI_RECORD,    &CMainFrame::OnMidiRecord)
	UI_COMMAND(ID_PANIC,          &CMainFrame::OnPanic)
	UI_COMMAND(ID_PLAYER_PAUSE,   &CMainFrame::OnPlayerPause)

	UI_COMMAND(IDD_TREEVIEW,         &CMainFrame::OnBarCheck)
	UI_COMMAND(ID_NETLINK_MODPLUG,   &CMainFrame::OnInternetLink)
	UI_COMMAND(ID_NETLINK_FLTK_GITHUB, &CMainFrame::OnInternetLink)
	UI_COMMAND(ID_NETLINK_TOP_PICKS, &CMainFrame::OnInternetLink)

	UI_MESSAGE(MSG_MOD_UPDATEPOSITION,     &CMainFrame::OnUpdatePosition)
	UI_MESSAGE(MSG_MOD_INVALIDATEPATTERNS, &CMainFrame::OnInvalidatePatterns)
	UI_MESSAGE(MSG_MOD_KEYCOMMAND,         &CMainFrame::OnCustomKeyMsg)
	UI_MESSAGE(MSG_MOD_MIDIMAPPING,        &CMainFrame::OnViewMIDIMapping)
	UI_MESSAGE(MSG_MOD_UPDATEVIEWS,        &CMainFrame::OnUpdateViews)
	UI_MESSAGE(MSG_MOD_SETMODIFIED,        &CMainFrame::OnSetModified)
	UI_COMMAND(ID_UPDATE_AVAILABLE, &CMainFrame::OnUpdateAvailable)
	UI_COMMAND(ID_HELP_SHOWSETTINGSFOLDER,   &CMainFrame::OnShowSettingsFolder)
	UI_COMMAND(ID_HELPSHOW,                  &CMainFrame::OnHelp)

	UI_COMMAND(ID_MAINBAR_SHOW_OCTAVE,       &CMainFrame::OnToggleMainBarShowOctave)
	UI_COMMAND(ID_MAINBAR_SHOW_TEMPO,        &CMainFrame::OnToggleMainBarShowTempo)
	UI_COMMAND(ID_MAINBAR_SHOW_SPEED,        &CMainFrame::OnToggleMainBarShowSpeed)
	UI_COMMAND(ID_MAINBAR_SHOW_ROWSPERBEAT,  &CMainFrame::OnToggleMainBarShowRowsPerBeat)
	UI_COMMAND(ID_MAINBAR_SHOW_GLOBALVOLUME, &CMainFrame::OnToggleMainBarShowGlobalVolume)
	UI_COMMAND(ID_MAINBAR_SHOW_VUMETER,      &CMainFrame::OnToggleMainBarShowVUMeter)
	UI_COMMAND(ID_MAINBAR_SHOW_FILE_ICONS,   &CMainFrame::OnToggleMainBarShowFileIcons)
	UI_COMMAND(ID_MAINBAR_SHOW_EDIT_ICONS,   &CMainFrame::OnToggleMainBarShowEditIcons)
	UI_COMMAND(ID_MAINBAR_SHOW_PLAY_ICONS,   &CMainFrame::OnToggleMainBarShowPlayIcons)
	UI_COMMAND(ID_MAINBAR_SHOW_MISC_ICONS,   &CMainFrame::OnToggleMainBarShowMiscIcons)

	UI_COMMAND(ID_TREEVIEW_ON_LEFT,          &CMainFrame::OnToggleTreeViewOnLeft)

#ifdef MPT_ENABLE_PLAYBACK_TEST_MENU
	UI_COMMAND(ID_CREATE_MIXERDUMP,  &CMainFrame::OnCreateMixerDump)
	UI_COMMAND(ID_VERIFY_MIXERDUMP,  &CMainFrame::OnVerifyMixerDump)
	UI_COMMAND(ID_CONVERT_MIXERDUMP, &CMainFrame::OnConvertMixerDumpToText)
#endif // ENABLE_PLAYBACK_TEST_MENU

	UI_COMMAND_RANGE(ID_FILE_OPENTEMPLATE, ID_FILE_OPENTEMPLATE_LASTINRANGE, &CMainFrame::OnOpenTemplateModule)
	UI_COMMAND_RANGE(ID_EXAMPLE_MODULES, ID_EXAMPLE_MODULES_LASTINRANGE, &CMainFrame::OnExampleSong)
	UI_COMMAND_RANGE(ID_MRU_LIST_FIRST, ID_MRU_LIST_LAST, &CMainFrame::OnOpenMRUItem)
	
	UI_UPDATE_COMMAND(ID_MRU_LIST_FIRST,  &CMainFrame::OnUpdateMRUItem)
	UI_UPDATE_COMMAND(ID_MIDI_RECORD,     &CMainFrame::OnUpdateMidiRecord)
	UI_UPDATE_COMMAND(ID_INDICATOR_TIME,  &CMainFrame::OnUpdateTime)
	UI_UPDATE_COMMAND(ID_INDICATOR_USER,  &CMainFrame::OnUpdateUser)
	UI_UPDATE_COMMAND(ID_INDICATOR_INFO,  &CMainFrame::OnUpdateInfo)
	UI_UPDATE_COMMAND(ID_INDICATOR_XINFO, &CMainFrame::OnUpdateXInfo)
	UI_UPDATE_COMMAND(IDD_TREEVIEW,       &CMainFrame::OnUpdateControlBarMenu)
UI_MESSAGE_MAP_END()

// Globals
OptionsPage CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_DEFAULT;

// GDI
ui::Font CMainFrame::m_hGUIFont;
ColorRef CMainFrame::penDarkGray = 0;
ColorRef CMainFrame::penGray99 = 0;
ColorRef CMainFrame::penHalfDarkGray = 0;

std::unique_ptr<MODPLUGDIB> CMainFrame::bmpNotes;
ColorRef CMainFrame::gcolrefVuMeter[NUM_VUMETER_PENS * 2];

static constexpr uint32 StatusBarIndicators[] =
{
	ID_SEPARATOR,  // status line indicator
	ID_INDICATOR_XINFO,
	ID_INDICATOR_INFO,
	ID_INDICATOR_USER,
	ID_INDICATOR_TIME,
};


/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction
CMainFrame::CMainFrame()
	: SoundDevice::CallbackBufferHandler<DithersOpenMPT>(theApp.PRNG())
	, m_SoundDeviceFillBufferCriticalSection(TrackerCriticalSection::InitialState::Unlocked)
	, m_InputHandler{this}
{
	MemsetZero(gcolrefVuMeter);
	Fl::watch_widget_pointer(m_previousFocusWidget);
}


void CMainFrame::Initialize()
{

	//Adding version number to the frame title
	mpt::ustring title = GetTitle();
	title += UL_(" ") + mpt::ufmt::val(Version::Current());
	title += mpt::ToUnicode(mpt::Charset::ASCII, OPENMPT_BUILD_VARIANT_MONIKER);
	if(Build::IsDebugBuild())
	{
		title += UL_(" DEBUG");
	}
	#ifndef MPT_WITH_VST
		title += UL_(" NO_VST");
	#endif
		title += UL_(" NO_DMO");
	SetTitle(title);
	OnUpdateFrameTitle(false);

	// Check for valid sound device
	SoundDevice::Identifier dev = TrackerSettings::Instance().GetSoundDeviceIdentifier();
	if(!theApp.GetSoundDevicesManager()->FindDeviceInfo(dev).IsValid())
	{
		dev = theApp.GetSoundDevicesManager()->FindDeviceInfoBestMatch(dev).GetIdentifier();
		TrackerSettings::Instance().SetSoundDeviceIdentifier(dev);
	}

	// Setup timer
	OnUpdateUser(nullptr);
	m_nTimer = SetTimer(TIMERID_GUI, MPTTIMER_PERIOD);

	// Update the tree
	m_wndTree.Init();

	CreateExampleModulesMenu();
	CreateTemplateModulesMenu();
	UpdateMRUList();

	auto [toolbarMenu, toolbarMenuStartPos] = FindMenuItemByCommand(*GetMenu(), ID_VIEW_TOOLBAR);
	MPT_ASSERT(toolbarMenu);
	if(toolbarMenu)
	{
		toolbarMenu->DeleteMenu(toolbarMenuStartPos, true);
		AddToolBarMenuEntries(*toolbarMenu);
	}

	m_InputHandler->UpdateMainMenu();

	LoadMetronomeSamples();

#ifdef MPT_ENABLE_PLAYBACK_TEST_MENU
	Menu debugMenu;
	debugMenu.CreatePopupMenu();
	debugMenu.AppendMenu(ui::MenuItemString, ID_CREATE_MIXERDUMP, UL_("Create Mixer Dump for &File(s)..."));
	debugMenu.AppendMenu(ui::MenuItemString, ID_VERIFY_MIXERDUMP, UL_("&Verify File(s)..."));
	debugMenu.AppendMenu(ui::MenuItemString, ID_CONVERT_MIXERDUMP, UL_("Convert Mixer Dump to &TSV..."));
	GetMenu()->AppendMenu(ui::MenuItemString | ui::MenuItemPopup, debugMenu, UL_("Debug"));
	SetMenu(*GetMenu());
#endif  // ENABLE_PLAYBACK_TEST_MENU
}


CMainFrame::~CMainFrame()
{
	Fl::release_widget_pointer(m_previousFocusWidget);
	CChannelManagerDlg::DestroySharedInstance();
	m_metronomeMeasure.FreeSample();
	m_metronomeBeat.FreeSample();
}


bool CMainFrame::OnCreate()
{
	RecreateImageLists();

	penDarkGray = ui::GetSystemColor(ui::SysColor::ButtonShadow);
	penGray99 = RGB(0x99, 0x99, 0x99);
	penHalfDarkGray = ui::GetSystemColor(ui::SysColor::ButtonShadow);

	// Pattern note bitmap
	bmpNotes = LoadDib(IDB_PATTERNVIEW);

	// Toolbars
	if(!m_wndToolBar.Create(this))
		return false;
	DockBar(&m_wndStatusBar, DockSide::Bottom, ui::ScalePixels(22, this));
	m_wndTree.CreateChild(*this, Rect(0, 0, m_wndTree.GetDesiredWidth(), 100), IDD_TREEVIEW);
	DockBar(&m_wndTree, TrackerSettings::Instance().treeViewOnLeft ? DockSide::Left : DockSide::Right, m_wndTree.GetDesiredWidth());
	m_wndTree.SetBarOnLeft(TrackerSettings::Instance().treeViewOnLeft);
	SetupStatusBarSizes();
	m_wndToolBar.Init(this);
	m_wndTree.RecalcLayout();

	UpdateColors();

	if(TrackerSettings::Instance().midiSetup & MidiSetup::EnableMidiInOnStartup)
		midiOpenDevice(false);

	return true;
}


void CMainFrame::RecreateImageLists()
{
	// Toolbar and other icons
	const double scaling = ui::GetDpiForWindow(this) / 96.0;
	static constexpr int miscIconsInvert[] = {IMAGE_PATTERNS, IMAGE_OPLINSTRACTIVE, IMAGE_OPLINSTRMUTE};
	static constexpr int patternIconsInvert[] = {TIMAGE_PREVIEW, TIMAGE_MACROEDITOR, TIMAGE_PATTERN_OVERFLOWPASTE, TIMAGE_PATTERN_PLUGINS, TIMAGE_SAMPLE_AMPLIFY, TIMAGE_SAMPLE_UNSIGN};
	static constexpr int envelopeIconsInvert[] = {IIMAGE_VOLENV, IIMAGE_PANENV, IIMAGE_CHECKED, IIMAGE_VOLSWITCH, IIMAGE_PANSWITCH, IIMAGE_PITCHSWITCH, IIMAGE_FILTERSWITCH, IIMAGE_NOPITCHSWITCH, IIMAGE_NOFILTERSWITCH, IIMAGE_GRID};
	m_MiscIcons.Create(IDB_IMAGELIST, 16, 16, scaling, false, miscIconsInvert);
	m_MiscIconsDisabled.Create(IDB_IMAGELIST, 16, 16, scaling, true, miscIconsInvert);
	m_PatternIcons.Create(IDB_PATTERNS, 16, 16, scaling, false, patternIconsInvert);
	m_PatternIconsDisabled.Create(IDB_PATTERNS, 16, 16, scaling, true, patternIconsInvert);
	m_EnvelopeIcons.Create(IDB_ENVTOOLBAR, 20, 18, scaling, false, envelopeIconsInvert);
	m_SampleIcons.Create(IDB_SMPTOOLBAR, 20, 18, scaling, false);

	m_hGUIFont = ui::GetGuiFont();
}


void CMainFrame::SetupStatusBarSizes()
{
	ui::Painter measure;
	measure.SetFont(m_hGUIFont);
	std::vector<std::pair<uint32, int>> panes;
	for(const uint32 indicator : StatusBarIndicators)
	{
		if(indicator == ID_SEPARATOR)
			panes.emplace_back(indicator, 0);
		else
			panes.emplace_back(indicator, measure.GetTextExtent(LoadResourceString(indicator)).cx);
	}
	m_wndStatusBar.SetPanes(panes);
}


void CMainFrame::OnDestroy()
{
	// Kill Timer
	if(m_nTimer)
	{
		KillTimer(m_nTimer);
		m_nTimer = 0;
	}
	if(midiInData.isOpen)
		midiCloseDevice();
	// Delete bitmaps
	bmpNotes.reset();

	PatternFont::DeleteFontData();

	MainFrameBase::OnDestroy();
}


void CMainFrame::OnClose()
{
	MPT_TRACE_SCOPE();
	if(!(TrackerSettings::Instance().patternSetup & PatternSetup::NoCustomCloseDialog))
	{
		// Show modified documents window
		CloseMainDialog dlg;
		if(dlg.DoModal() != IDOK)
		{
			return;
		}
	}


	CChildFrame *pMDIActive = (CChildFrame *)MDIGetActive();

	BeginWaitCursor();
	if (IsPlaying()) PauseMod();
	if (pMDIActive) pMDIActive->SavePosition(true);

	if(gpSoundDevice)
	{
		gpSoundDevice->Stop();
		gpSoundDevice->Close();
		delete gpSoundDevice;
		gpSoundDevice = nullptr;
	}

	// Save Settings
	TrackerSettings::Instance().SaveSettings();

	if(m_InputHandler->m_activeCommandSet)
	{
		m_InputHandler->m_activeCommandSet->SaveFile(TrackerSettings::Instance().m_szKbdFile);
	}


	EndWaitCursor();
	MainFrameBase::OnClose();
}


// Drop files from the desktop
void CMainFrame::OnDropFiles(const std::vector<mpt::PathString> &files)
{
	SetForegroundWindow();
#ifdef MPT_BUILD_DEBUG
	const bool scanAll = CInputHandler::GetModifierMask().test_all(ModCtrl | ModShift | ModAlt);
#endif
	for(const mpt::PathString &file : files)
	{
#ifdef MPT_BUILD_DEBUG
		// Debug Hack: Quickly scan a folder containing module files (without running out of window handles ;)
		if(scanAll && FileSystem::IsDirectory(file))
		{
			FolderScanner scanner(file, FolderScanner::kOnlyFiles | FolderScanner::kFindInSubDirectories);
			mpt::PathString scanName;
			size_t failed = 0, total = 0;
			while(scanner.Next(scanName))
			{
				mpt::IO::InputFile inputFile(scanName, TrackerSettings::Instance().MiscCacheCompleteFileBeforeLoading);
				if(!inputFile.IsValid())
					continue;
				SetHelpText(scanName.GetFilename().ToUnicode());
				auto sndFile = std::make_unique<CTrackerSoundFile>();
				MPT_LOG_GLOBAL(LogDebug, "info", UL_("Loading ") + scanName.ToUnicode());
				if(!sndFile->Create(GetFileReader(inputFile), CSoundFile::loadCompleteModule, nullptr))
				{
					MPT_LOG_GLOBAL(LogDebug, "info", UL_("FAILED: ") + scanName.ToUnicode());
					failed++;
				}
				total++;
			}
			SetHelpText(UL_(""));
			Reporting::Information(MPT_UFORMAT("Scanned {} files, {} failed")(total, failed));
			continue;
		} else if(scanAll)
		{
			mpt::IO::InputFile inputFile(file, TrackerSettings::Instance().MiscCacheCompleteFileBeforeLoading);
			if(!inputFile.IsValid())
				continue;
			auto sndFile = std::make_unique<CTrackerSoundFile>();
			SetHelpText(file.GetFilename().ToUnicode());
			MPT_LOG_GLOBAL(LogDebug, "info", UL_("Loading ") + file.ToUnicode());
			if(!sndFile->Create(GetFileReader(inputFile), CSoundFile::loadCompleteModule, nullptr))
				MPT_LOG_GLOBAL(LogDebug, "info", UL_("FAILED: ") + file.ToUnicode());
			SetHelpText(UL_(""));
			continue;
		}
#endif
		theApp.OpenDocumentFile(file);
	}
}


void CMainFrame::UpdateLastFocusedItem()
{
	// Keeps track of the last focussed GUI item. This solves various focus issues when switching between
	// CModControlDlg / CModScrollView via keyboard shortcuts, or when switching to another application and back.
	// See https://bugs.openmpt.org/view.php?id=1795 / https://bugs.openmpt.org/view.php?id=1799 / https://bugs.openmpt.org/view.php?id=1800
	Fl_Widget *const gainFocusWidget = Fl::focus();
	Fl_Widget *const lostFocusWidget = m_previousFocusWidget;
	m_previousFocusWidget = gainFocusWidget;
	if(lostFocusWidget == gainFocusWidget || lostFocusWidget == nullptr)
		return;

	const WindowHandle mainWnd = this;
	CModControlDlg *parentCtrl = nullptr;
	CModScrollView *parentScroll = nullptr;
	WindowHandle lostFocusWnd = nullptr;
	for(Fl_Widget *widget = lostFocusWidget; widget != nullptr && widget != GetWidget(); widget = widget->parent())
	{
		Wnd *wnd = dynamic_cast<Wnd *>(widget);
		if(wnd == nullptr)
			continue;
		if(lostFocusWnd == nullptr)
			lostFocusWnd = wnd;
		if(parentCtrl = dynamic_cast<CModControlDlg *>(wnd); parentCtrl != nullptr)
			break;
		else if(parentScroll = dynamic_cast<CModScrollView *>(wnd); parentScroll != nullptr)
			break;
	}
	if(!parentCtrl && !parentScroll)
		return;

	// Focus was lost inside an MDI view. Check if the new focus item inside the same view.
	// If both are part of the same view, store the new focus item, otherwise the old one.
	bool sameParent = false;
	WindowHandle gainFocusWnd = nullptr;
	for(Fl_Widget *widget = gainFocusWidget; widget != nullptr && widget != GetWidget(); widget = widget->parent())
	{
		Wnd *wnd = dynamic_cast<Wnd *>(widget);
		if(wnd == nullptr)
			continue;
		if(gainFocusWnd == nullptr)
			gainFocusWnd = wnd;
		if((parentCtrl && wnd == parentCtrl) || (parentScroll && wnd == parentScroll))
		{
			sameParent = true;
			break;
		}
	}

	WindowHandle lastFocus = sameParent ? gainFocusWnd : lostFocusWnd;
	if(lastFocus == nullptr || mainWnd == nullptr)
		return;
	if(parentCtrl && parentCtrl != lastFocus)
		parentCtrl->SaveLastFocusItem(lastFocus);
	else if(parentScroll && parentScroll != lastFocus)
		parentScroll->SaveLastFocusItem(lastFocus);
}


bool CMainFrame::PreTranslateMessage(int event)
{
	// Right-click menu to disable/enable tree view and main toolbar when right-clicking on either the menu strip or main toolbar
	if(event == FL_PUSH && Fl::event_button() == FL_RIGHT_MOUSE)
	{
		const Point pt = ui::GetCursorPosition();
		Rect frameRect;
		GetWindowRect(frameRect);
		Wnd *wndUnderMouse = ui::WindowFromPoint(pt);
		if((pt.y - frameRect.top < m_menuBarHeight) || (wndUnderMouse != nullptr && (wndUnderMouse == &m_wndToolBar || m_wndToolBar.IsChild(wndUnderMouse))))
		{
			ShowToolbarMenu(pt);
			return true;
		}
	}

	// We handle keypresses before the toolkit has a chance to handle them (for alt etc..)
	if(event == FL_KEYBOARD || event == FL_KEYUP)
	{
		CInputHandler *ih = CMainFrame::GetInputHandler();
		const auto keyEvent = ih->Translate(ui::KeyFromEvent(), 1, (event == FL_KEYUP) ? ui::KeyFlagRelease : (ui::IsKeyRepeat() ? ui::KeyFlagRepeat : 0));
		if(ih->KeyEvent(kCtxAllContexts, keyEvent) != kcNull)
			return true;  // Mapped to a command, no need to pass message on.
	}

	return MainFrameBase::PreTranslateMessage(event);
}


void CMainFrame::OnUpdateFrameTitle(bool isAddToTitle)
{
	mpt::ustring documentTitle;
	if(ChildFrameBase *activeChild = MDIGetActive())
	{
		Document *document = isAddToTitle ? activeChild->GetActiveDocument() : nullptr;
		if(document != nullptr)
		{
			documentTitle = document->GetTitle();
			if(document->IsModified())
				documentTitle.push_back(UL_('*'));
		} else
		{
			documentTitle = activeChild->GetTitle();
		}
	}
	UpdateFrameTitleForDocument(documentTitle);
}


void CMainFrame::RecalcLayout(bool notify)
{
	MainFrameBase::RecalcLayout(notify);
	if(m_quickStartDlg)
	{
		m_quickStartDlg->UpdateHeight();
		m_quickStartDlg->CenterWindow(this);
	}
}


bool CMainFrame::InGuiThread() const noexcept
{
	return theApp.InGuiThread();
}


CMainFrame *CMainFrame::GetMainFrame() noexcept
{
	return static_cast<CMainFrame *>(theApp.GetMainWnd());
}


/////////////////////////////////////////////////////////////////////////////
// CMainFrame Sound Library


void CMainFrame::OnBarCheck(uint32 id)
{
	Wnd *bar = (id == IDD_TREEVIEW) ? static_cast<Wnd *>(&m_wndTree) : static_cast<Wnd *>(&m_wndToolBar);
	ShowControlBar(bar, !bar->IsWindowVisible());
}


void CMainFrame::OnUpdateControlBarMenu(CmdUI *cmdUI)
{
	const Wnd *bar = (cmdUI->id == IDD_TREEVIEW) ? static_cast<const Wnd *>(&m_wndTree) : static_cast<const Wnd *>(&m_wndToolBar);
	cmdUI->SetCheck(bar->IsWindowVisible() ? 1 : 0);
	cmdUI->Enable(true);
}


void CMainFrame::OnTimerNotify()
{
	MPT_TRACE_SCOPE();
	MPT_ASSERT(InGuiThread());
	MPT_ASSERT(!InNotifyHandler());
	m_InNotifyHandler = true;
	Notification PendingNotification;
	bool found = false;
	int64 currenttotalsamples = 0;
	if(gpSoundDevice)
	{
		currenttotalsamples = gpSoundDevice->GetStreamPosition().Frames;
	}
	{
		// advance to the newest notification, drop the obsolete ones
		mpt::lock_guard<mpt::mutex> lock(m_NotificationBufferMutex);
		const Notification * pnotify = nullptr;
		const Notification * p = m_NotifyBuffer.peek_p();
		if(p && currenttotalsamples >= p->timestampSamples)
		{
			pnotify = p;
			while(m_NotifyBuffer.peek_next_p() && currenttotalsamples >= m_NotifyBuffer.peek_next_p()->timestampSamples)
			{
				m_NotifyBuffer.pop();
				p = m_NotifyBuffer.peek_p();
				pnotify = p;
			}
		}
		if(pnotify)
		{
			PendingNotification = *pnotify; // copy notification so that we can free the buffer
			found = true;
			{
				m_NotifyBuffer.pop();
			}
		}
	}
	if(found)
	{
		OnUpdatePosition(0, (LParam)&PendingNotification);
	}
	m_InNotifyHandler = false;
	MPT_ASSERT(!InNotifyHandler());
}


void CMainFrame::SoundDeviceMessage(LogLevel level, const mpt::ustring &str)
{
	MPT_TRACE();
	Reporting::Message(level, str);
}


void CMainFrame::SoundCallbackPreStart()
{
	MPT_TRACE();
	m_SoundDeviceClock.SetResolution(1);
}


void CMainFrame::SoundCallbackPostStop()
{
	MPT_TRACE();
	m_SoundDeviceClock.SetResolution(0);
}


uint64 CMainFrame::SoundCallbackGetReferenceClockNowNanoseconds() const
{
	MPT_TRACE();
	MPT_ASSERT(!InAudioThread());
	return m_SoundDeviceClock.NowNanoseconds();
}


uint64 CMainFrame::SoundCallbackLockedGetReferenceClockNowNanoseconds() const
{
	MPT_TRACE();
	MPT_ASSERT(InAudioThread());
	return m_SoundDeviceClock.NowNanoseconds();
}


bool CMainFrame::SoundCallbackIsLockedByCurrentThread() const
{
	MPT_TRACE();
	return theApp.GetGlobalMutexRef().IsLockedByCurrentThread();
}


void CMainFrame::SoundCallbackLock()
{
	MPT_TRACE_SCOPE();
	Tracker::YieldToLockWaiters();
	m_SoundDeviceFillBufferCriticalSection.Enter();
	MPT_ASSERT_ALWAYS(m_pSndFile != nullptr);
	m_AudioThreadId = mpt::log::Trace::GetCurrentThreadId();
	mpt::log::Trace::SetThreadId(mpt::log::Trace::ThreadKindAudio, m_AudioThreadId);
}


void CMainFrame::SoundCallbackUnlock()
{
	MPT_TRACE_SCOPE();
	MPT_ASSERT_ALWAYS(m_pSndFile != nullptr);
	m_AudioThreadId = 0;
	m_SoundDeviceFillBufferCriticalSection.Leave();
}


class BufferInputWrapper
	: public IAudioSource
{
private:
	SoundDevice::CallbackBuffer<DithersOpenMPT> &callbackBuffer;
public:
	inline BufferInputWrapper(SoundDevice::CallbackBuffer<DithersOpenMPT> &callbackBuffer_)
		: callbackBuffer(callbackBuffer_)
	{
		return;
	}
	inline void Process(mpt::audio_span_planar<MixSampleInt> buffer) override
	{
		callbackBuffer.template ReadFixedPoint<MixSampleIntTraits::mix_fractional_bits>(buffer);
	}
	inline void Process(mpt::audio_span_planar<MixSampleFloat> buffer) override
	{
		callbackBuffer.Read(buffer);
	}
};


class BufferOutputWrapper
	: public IAudioTarget
{
private:
	SoundDevice::CallbackBuffer<DithersOpenMPT> &callbackBuffer;
public:
	inline BufferOutputWrapper(SoundDevice::CallbackBuffer<DithersOpenMPT> &callbackBuffer_)
		: callbackBuffer(callbackBuffer_)
	{
		return;
	}
	inline void Process(mpt::audio_span_interleaved<MixSampleInt> buffer) override
	{
		callbackBuffer.template WriteFixedPoint<MixSampleIntTraits::mix_fractional_bits>(buffer);
	}
	inline void Process(mpt::audio_span_interleaved<MixSampleFloat> buffer) override
	{
		callbackBuffer.Write(buffer);
	}
};


void CMainFrame::SoundCallbackLockedProcessPrepare(SoundDevice::TimeInfo timeInfo)
{
	MPT_TRACE_SCOPE();
	MPT_ASSERT(InAudioThread());
	TimingInfo timingInfo;
	timingInfo.OutputLatency = timeInfo.Latency;
	timingInfo.StreamFrames = timeInfo.SyncPointStreamFrames;
	timingInfo.SystemTimestamp = timeInfo.SyncPointSystemTimestamp;
	timingInfo.Speed = timeInfo.Speed;
	m_pSndFile->m_TimingInfo = timingInfo;
}


void CMainFrame::SoundCallbackLockedCallback(SoundDevice::CallbackBuffer<DithersOpenMPT> &buffer)
{
	MPT_TRACE_SCOPE();
	MPT_ASSERT(InAudioThread());
	OPENMPT_PROFILE_FUNCTION(Profiler::Audio);
	BufferInputWrapper source(buffer);
	BufferOutputWrapper target(buffer);
	MPT_ASSERT(buffer.GetNumFrames() <= std::numeric_limits<samplecount_t>::max());
	samplecount_t framesToRender = static_cast<samplecount_t>(buffer.GetNumFrames());
	MPT_ASSERT(framesToRender > 0);
	samplecount_t renderedFrames = m_pSndFile->Render(framesToRender, target, source, std::ref(m_VUMeterOutput), std::ref(m_VUMeterInput));
	MPT_ASSERT(renderedFrames <= framesToRender);
	[[maybe_unused]] samplecount_t remainingFrames = framesToRender - renderedFrames;
	MPT_ASSERT(remainingFrames >= 0); // remaining buffer is filled with silence automatically
}


void CMainFrame::SoundCallbackLockedProcessDone(SoundDevice::TimeInfo timeInfo)
{
	MPT_TRACE_SCOPE();
	MPT_ASSERT(InAudioThread());
	OPENMPT_PROFILE_FUNCTION(Profiler::Notify);
	MPT_ASSERT((timeInfo.RenderStreamPositionAfter.Frames - timeInfo.RenderStreamPositionBefore.Frames) < std::numeric_limits<samplecount_t>::max());
	samplecount_t framesRendered = static_cast<samplecount_t>(timeInfo.RenderStreamPositionAfter.Frames - timeInfo.RenderStreamPositionBefore.Frames);
	int64 streamPosition = timeInfo.RenderStreamPositionAfter.Frames;
	DoNotification(framesRendered, streamPosition);
	//m_pSndFile->m_TimingInfo = TimingInfo(); // reset
}


bool CMainFrame::IsAudioDeviceOpen() const
{
	MPT_TRACE_SCOPE();
	return gpSoundDevice && gpSoundDevice->IsOpen();
}


bool CMainFrame::audioOpenDevice()
{
	MPT_TRACE_SCOPE();
	const SoundDevice::Identifier deviceIdentifier = TrackerSettings::Instance().GetSoundDeviceIdentifier();
	if(!TrackerSettings::Instance().GetMixerSettings().IsValid())
	{
		Reporting::Error(MPT_UFORMAT("Unable to open sound device '{}': Invalid mixer settings.")(deviceIdentifier));
		return false;
	}
	if(gpSoundDevice && (gpSoundDevice->GetDeviceInfo().GetIdentifier() != deviceIdentifier))
	{
		gpSoundDevice->Stop();
		gpSoundDevice->Close();
		delete gpSoundDevice;
		gpSoundDevice = nullptr;
	}
	if(IsAudioDeviceOpen())
	{
		return true;
	}
	if(!gpSoundDevice)
	{
		gpSoundDevice = theApp.GetSoundDevicesManager()->CreateSoundDevice(deviceIdentifier);
	}
	if(!gpSoundDevice)
	{
		Reporting::Error(MPT_UFORMAT("Unable to open sound device '{}': Could not find sound device.")(deviceIdentifier));
		return false;
	}
	gpSoundDevice->SetMessageReceiver(this);
	gpSoundDevice->SetCallback(this);
	SoundDevice::Settings deviceSettings = TrackerSettings::Instance().GetSoundDeviceSettings(deviceIdentifier);
	if(!gpSoundDevice->Open(deviceSettings))
	{
		if(!gpSoundDevice->IsAvailable())
		{
			Reporting::Error(MPT_UFORMAT("Unable to open sound device '{}': Device not available.")(gpSoundDevice->GetDeviceInfo().GetDisplayName()));
		} else
		{
			Reporting::Error(MPT_UFORMAT("Unable to open sound device '{}'.")(gpSoundDevice->GetDeviceInfo().GetDisplayName()));
		}
		return false;
	}
	SampleFormat actualSampleFormat = gpSoundDevice->GetActualSampleFormat();
	deviceSettings.sampleFormat = actualSampleFormat;
	Dithers().SetMode(deviceSettings.DitherType, deviceSettings.Channels);
	TrackerSettings::Instance().MixerSamplerate = gpSoundDevice->GetSettings().Samplerate;
	TrackerSettings::Instance().SetSoundDeviceSettings(deviceIdentifier, deviceSettings);
	return true;
}


void CMainFrame::audioCloseDevice()
{
	MPT_TRACE_SCOPE();
	if(gpSoundDevice)
	{
		gpSoundDevice->Close();
	}
	if(m_NotifyTimer)
	{
		KillTimer(m_NotifyTimer);
		m_NotifyTimer = 0;
	}
	ResetNotificationBuffer();
}


void VUMeter::Process(Channel &c, MixSampleInt sample)
{
	c.peak = std::max(c.peak, std::abs(sample));
	if(sample < MixSampleIntTraits::mix_clip_min || MixSampleIntTraits::mix_clip_max < sample)
	{
		c.clipped = true;
	}
}


void VUMeter::Process(Channel &c, MixSampleFloat sample)
{
	Process(c, SC::ConvertToFixedPoint<MixSampleInt, MixSampleFloat, MixSampleIntTraits::mix_fractional_bits>()(sample));
}


void VUMeter::Process(mpt::audio_span_interleaved<const MixSampleInt> buffer)
{
	for(std::size_t frame = 0; frame < buffer.size_frames(); ++frame)
	{
		for(std::size_t channel = 0; channel < std::min(buffer.size_channels(), maxChannels); ++channel)
		{
			Process(channels[channel], buffer(channel, frame));
		}
	}
	for(std::size_t channel = std::min(buffer.size_channels(), maxChannels); channel < maxChannels; ++channel)
	{
		channels[channel] = Channel();
	}
}


void VUMeter::Process(mpt::audio_span_interleaved<const MixSampleFloat> buffer)
{
	for(std::size_t frame = 0; frame < buffer.size_frames(); ++frame)
	{
		for(std::size_t channel = 0; channel < std::min(buffer.size_channels(), maxChannels); ++channel)
		{
			Process(channels[channel], buffer(channel, frame));
		}
	}
	for(std::size_t channel = std::min(buffer.size_channels(), maxChannels); channel < maxChannels; ++channel)
	{
		channels[channel] = Channel();
	}
}


void VUMeter::Process(mpt::audio_span_planar<const MixSampleInt> buffer)
{
	for(std::size_t frame = 0; frame < buffer.size_frames(); ++frame)
	{
		for(std::size_t channel = 0; channel < std::min(buffer.size_channels(), maxChannels); ++channel)
		{
			Process(channels[channel], buffer(channel, frame));
		}
	}
	for(std::size_t channel = std::min(buffer.size_channels(), maxChannels); channel < maxChannels; ++channel)
	{
		channels[channel] = Channel();
	}
}


void VUMeter::Process(mpt::audio_span_planar<const MixSampleFloat> buffer)
{
	for(std::size_t frame = 0; frame < buffer.size_frames(); ++frame)
	{
		for(std::size_t channel = 0; channel < std::min(buffer.size_channels(), maxChannels); ++channel)
		{
			Process(channels[channel], buffer(channel, frame));
		}
	}
	for(std::size_t channel = std::min(buffer.size_channels(), maxChannels); channel < maxChannels; ++channel)
	{
		channels[channel] = Channel();
	}
}


const float VUMeter::dynamicRange = 48.0f; // corresponds to the current implementation of the UI widget displaying the result


void VUMeter::SetDecaySpeedDecibelPerSecond(float decibelPerSecond)
{
	float linearDecayRate = decibelPerSecond / dynamicRange;
	decayParam = mpt::saturate_round<int32>(linearDecayRate * static_cast<float>(MixSampleIntTraits::mix_clip_max));
}


void VUMeter::Decay(int32 secondsNum, int32 secondsDen)
{
	int32 decay = Util::muldivr(decayParam, secondsNum, secondsDen);
	for(std::size_t channel = 0; channel < maxChannels; ++channel)
	{
		channels[channel].peak = std::max(channels[channel].peak - decay, 0);
	}
}


void VUMeter::ResetClipped()
{
	for(std::size_t channel = 0; channel < maxChannels; ++channel)
	{
		channels[channel].clipped = false;
	}
}


static void SetVUMeter(std::array<uint32, 4> &masterVU, const VUMeter &vumeter)
{
	for(std::size_t channel = 0; channel < VUMeter::maxChannels; ++channel)
	{
		masterVU[channel] = Clamp(vumeter[channel].peak >> 11, 0, 0x10000);
		if(vumeter[channel].clipped)
		{
			masterVU[channel] |= Notification::ClipVU;
		}
	}
}


bool CMainFrame::DoNotification(uint32 dwSamplesRead, int64 streamPosition)
{
	MPT_TRACE_SCOPE();
	MPT_ASSERT(InAudioThread());
	if(!m_pSndFile) return false;

	FlagSet<Notification::Type> notifyType(Notification::Default);
	Notification::Item notifyItem = 0;

	if(CModDoc *modDoc = m_pSndFile->GetpModDoc())
	{
		notifyType = modDoc->GetNotificationType();
		notifyItem = modDoc->GetNotificationItem();
	}

	// Add an entry to the notification history

	Notification notification(notifyType, notifyItem, streamPosition, m_pSndFile->m_PlayState.m_nRow, m_pSndFile->m_PlayState.m_nTickCount, m_pSndFile->m_PlayState.TicksOnRow(), m_pSndFile->m_PlayState.m_nCurrentOrder, m_pSndFile->m_PlayState.m_nPattern, m_pSndFile->GetMixStat(), static_cast<uint8>(m_pSndFile->m_MixerSettings.gnChannels), static_cast<uint8>(m_pSndFile->m_MixerSettings.NumInputChannels));

	m_pSndFile->ResetMixStat();

	if(m_pSndFile->m_PlayState.m_flags[SONG_ENDREACHED]) notification.type.set(Notification::EOS);

	if(notifyType[Notification::Sample])
	{
		// Sample positions
		const SAMPLEINDEX smp = notifyItem;
		if(smp > 0 && smp <= m_pSndFile->GetNumSamples() && m_pSndFile->GetSample(smp).HasSampleData())
		{
			for(CHANNELINDEX k = 0; k < MAX_CHANNELS; k++)
			{
				const ModChannel &chn = m_pSndFile->m_PlayState.Chn[k];
				if(chn.pModSample == &m_pSndFile->GetSample(smp) && chn.nLength != 0	// Correct sample is set up on this channel
					&& (!chn.dwFlags[CHN_NOTEFADE] || chn.nFadeOutVol))					// And it hasn't completely faded out yet, so it's still playing
				{
					notification.pos[k] = chn.position.GetInt();
				} else
				{
					notification.pos[k] = Notification::PosInvalid;
				}
			}
		} else
		{
			// Can't generate a valid notification.
			notification.type.reset(Notification::Sample);
		}
	} else if(notifyType[Notification::VolEnv | Notification::PanEnv | Notification::PitchEnv])
	{
		// Instrument envelopes
		const INSTRUMENTINDEX ins = notifyItem;

		EnvelopeType notifyEnv = ENV_VOLUME;
		if(notifyType[Notification::PitchEnv])
			notifyEnv = ENV_PITCH;
		else if(notifyType[Notification::PanEnv])
			notifyEnv = ENV_PANNING;

		if(ins > 0 && ins <= m_pSndFile->GetNumInstruments() && m_pSndFile->Instruments[ins] != nullptr)
		{
			for(CHANNELINDEX k = 0; k < MAX_CHANNELS; k++)
			{
				const ModChannel &chn = m_pSndFile->m_PlayState.Chn[k];
				SmpLength pos = Notification::PosInvalid;

				if(chn.pModInstrument == m_pSndFile->Instruments[ins]				// Correct instrument is set up on this channel
					&& (chn.nLength || chn.pModInstrument->HasValidMIDIChannel())	// And it's playing something (sample or instrument plugin)
					&& (!chn.dwFlags[CHN_NOTEFADE] || chn.nFadeOutVol))				// And it hasn't completely faded out yet, so it's still playing
				{
					const ModChannel::EnvInfo &chnEnv = chn.GetEnvelope(notifyEnv);
					if(chnEnv.flags[ENV_ENABLED])
					{
						pos = chnEnv.nEnvPosition;
						if(m_pSndFile->m_playBehaviour[kITEnvelopePositionHandling])
						{
							// Impulse Tracker envelope handling (see e.g. CSoundFile::IncrementEnvelopePosition in SndMix.cpp for details)
							if(pos > 0)
								pos--;
							else
								pos = Notification::PosInvalid;	// Envelope isn't playing yet (e.g. when disabling it right when triggering a note)
						}
					}
				}
				notification.pos[k] = pos;
			}
		} else
		{
			// Can't generate a valid notification.
			notification.type.reset(Notification::VolEnv | Notification::PanEnv | Notification::PitchEnv);
		}
	} else if(notifyType[Notification::VUMeters])
	{
		// Pattern channel VU meters
		for(CHANNELINDEX k = 0; k < m_pSndFile->GetNumChannels(); k++)
		{
			uint32 vul = m_pSndFile->m_PlayState.Chn[k].nLeftVU;
			uint32 vur = m_pSndFile->m_PlayState.Chn[k].nRightVU;
			notification.pos[k] = (vul << 8) | (vur);
		}
	}

	{
		// Master VU meter
		SetVUMeter(notification.masterVUin, m_VUMeterInput);
		SetVUMeter(notification.masterVUout, m_VUMeterOutput);
		m_VUMeterInput.Decay(dwSamplesRead, m_pSndFile->m_MixerSettings.gdwMixingFreq);
		m_VUMeterOutput.Decay(dwSamplesRead, m_pSndFile->m_MixerSettings.gdwMixingFreq);

	}

	{
		mpt::lock_guard<mpt::mutex> lock(m_NotificationBufferMutex);
		if(m_NotifyBuffer.write_size() == 0)
		{
			MPT_ASSERT(0);
			return false; // drop notification
		}
		m_NotifyBuffer.push(notification);
	}

	return true;
}


void CMainFrame::UpdateDspEffects(CTrackerSoundFile &sndFile, bool reset)
{
	TrackerCriticalSection cs;
#ifndef NO_REVERB
	sndFile.m_Reverb.m_Settings = TrackerSettings::Instance().m_ReverbSettings;
#endif
	sndFile.m_Surround.m_Settings = TrackerSettings::Instance().m_SurroundSettings;
	sndFile.m_MegaBass.m_Settings = TrackerSettings::Instance().m_MegaBassSettings;
	sndFile.SetEQGains(TrackerSettings::Instance().m_EqSettings.Gains, TrackerSettings::Instance().m_EqSettings.Freqs, reset);
	sndFile.m_BitCrush.m_Settings = TrackerSettings::Instance().m_BitCrushSettings;
	sndFile.SetDspEffects(TrackerSettings::Instance().MixerDSPMask);
	sndFile.InitPlayer(reset);
}


void CMainFrame::UpdateAudioParameters(CTrackerSoundFile &sndFile, bool reset)
{
	TrackerCriticalSection cs;
	if (TrackerSettings::Instance().patternSetup & PatternSetup::IgnoreMutedChannels)
		TrackerSettings::Instance().MixerFlags |= SNDMIX_MUTECHNMODE;
	else
		TrackerSettings::Instance().MixerFlags &= ~SNDMIX_MUTECHNMODE;
	sndFile.SetMixerSettings(TrackerSettings::Instance().GetMixerSettings());
	sndFile.SetResamplerSettings(TrackerSettings::Instance().GetResamplerSettings());
	UpdateDspEffects(sndFile, false); // reset done in next line
	sndFile.InitPlayer(reset);
}


/////////////////////////////////////////////////////////////////////////////
// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	MainFrameBase::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	MainFrameBase::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMainFrame static helpers


void CMainFrame::UpdateColors()
{
	const auto &colors = TrackerSettings::Instance().rgbCustomColors;
	// Generel tab VU meters
	for(uint32 i = 0; i < NUM_VUMETER_PENS * 2; i++)
	{
		int r0,g0,b0, r1,g1,b1;
		int r, g, b;
		int y;

		y = (i >= NUM_VUMETER_PENS) ? (i-NUM_VUMETER_PENS) : i;
		if (y < (NUM_VUMETER_PENS/2))
		{
			r0 = GetRValue(colors[MODCOLOR_VUMETER_LO]);
			g0 = GetGValue(colors[MODCOLOR_VUMETER_LO]);
			b0 = GetBValue(colors[MODCOLOR_VUMETER_LO]);
			r1 = GetRValue(colors[MODCOLOR_VUMETER_MED]);
			g1 = GetGValue(colors[MODCOLOR_VUMETER_MED]);
			b1 = GetBValue(colors[MODCOLOR_VUMETER_MED]);
		} else
		{
			y -= (NUM_VUMETER_PENS/2);
			r0 = GetRValue(colors[MODCOLOR_VUMETER_MED]);
			g0 = GetGValue(colors[MODCOLOR_VUMETER_MED]);
			b0 = GetBValue(colors[MODCOLOR_VUMETER_MED]);
			r1 = GetRValue(colors[MODCOLOR_VUMETER_HI]);
			g1 = GetGValue(colors[MODCOLOR_VUMETER_HI]);
			b1 = GetBValue(colors[MODCOLOR_VUMETER_HI]);
		}
		r = r0 + ((r1 - r0) * y) / (NUM_VUMETER_PENS/2);
		g = g0 + ((g1 - g0) * y) / (NUM_VUMETER_PENS/2);
		b = b0 + ((b1 - b0) * y) / (NUM_VUMETER_PENS/2);
		if (i >= NUM_VUMETER_PENS)
		{
			r = (r*2)/5;
			g = (g*2)/5;
			b = (b*2)/5;
		}
		gcolrefVuMeter[i] = RGB(r, g, b);
	}
	CMainFrame *mainFrm = GetMainFrame();
	if(mainFrm != nullptr)
	{
		mainFrm->m_wndToolBar.m_VuMeter.Invalidate();
	}
}


/////////////////////////////////////////////////////////////////////////////
// CMainFrame operations


uint32 CMainFrame::GetBaseOctave() const
{
	return m_wndToolBar.GetBaseOctave();
}


void CMainFrame::ResetNotificationBuffer()
{
	MPT_TRACE();
	mpt::lock_guard<mpt::mutex> lock(m_NotificationBufferMutex);
	m_NotifyBuffer.clear();
}


bool CMainFrame::PreparePlayback()
{
	MPT_TRACE_SCOPE();
	// open the audio device to update needed TrackerSettings mixer parameters
	if(!audioOpenDevice()) return false;
	return true;
}


bool CMainFrame::StartPlayback()
{
	MPT_TRACE_SCOPE();
	if(!m_pSndFile) return false; // nothing to play
	if(!IsAudioDeviceOpen()) return false;
	Dithers().Reset();
	if(!gpSoundDevice->Start()) return false;
	if(!m_NotifyTimer)
	{
		if(TrackerSettings::Instance().GUIUpdateInterval.Get() > 0)
		{
			m_NotifyTimer = SetTimer(TIMERID_NOTIFY, TrackerSettings::Instance().GUIUpdateInterval);
		} else
		{
			m_NotifyTimer = SetTimer(TIMERID_NOTIFY, std::max(int(1), mpt::saturate_round<int>(gpSoundDevice->GetEffectiveBufferAttributes().UpdateInterval * 1000.0)));
		}
	}
	return true;
}


void CMainFrame::StopPlayback()
{
	MPT_TRACE_SCOPE();
	if(!IsAudioDeviceOpen()) return;
	gpSoundDevice->Stop();
	if(m_NotifyTimer)
	{
		KillTimer(m_NotifyTimer);
		m_NotifyTimer = 0;
	}
	ResetNotificationBuffer();
	if(!gpSoundDevice->GetDeviceCaps().CanKeepDeviceRunning || TrackerSettings::Instance().m_SoundSettingsStopMode == SoundDeviceStopModeClosed)
	{
		audioCloseDevice();
	}
}


bool CMainFrame::RestartPlayback()
{
	MPT_TRACE_SCOPE();
	if(!m_pSndFile) return false; // nothing to play
	if(!IsAudioDeviceOpen()) return false;
	if(!gpSoundDevice->IsPlaying()) return false;
	gpSoundDevice->StopAndAvoidPlayingSilence();
	if(m_NotifyTimer)
	{
		KillTimer(m_NotifyTimer);
		m_NotifyTimer = 0;
	}
	ResetNotificationBuffer();
	return StartPlayback();
}


bool CMainFrame::PausePlayback()
{
	MPT_TRACE_SCOPE();
	if(!IsAudioDeviceOpen()) return false;
	gpSoundDevice->Stop();
	if(m_NotifyTimer)
	{
		KillTimer(m_NotifyTimer);
		m_NotifyTimer = 0;
	}
	ResetNotificationBuffer();
	return true;
}


void CMainFrame::GenerateStopNotification()
{
	Notification mn(Notification::Stop);
	SendMessage(MSG_MOD_UPDATEPOSITION, 0, reinterpret_cast<LParam>(&mn));
}


void CMainFrame::UnsetPlaybackSoundFile()
{
	MPT_ASSERT_ALWAYS(!gpSoundDevice || !gpSoundDevice->IsPlaying());
	if(m_pSndFile)
	{
		TrackerCriticalSection cs;
		m_pSndFile->SuspendPlugins();
		m_pSndFile->m_PlayState.m_flags.reset(SONG_PAUSED);
		if(m_pSndFile == &m_WaveFile)
		{
			// Unload previewed instrument
			m_WaveFile.Destroy();
		} else
		{
			// Stop sample preview channels
			for(ModChannel &chn : m_pSndFile->m_PlayState.BackgroundChannels(*m_pSndFile))
			{
				if(chn.isPreviewNote)
				{
					chn.nLength = 0;
					chn.position.Set(0);
				}
			}
		}
		cs.Leave();
		if(m_pSndFile->GetpModDoc())
		{
			m_wndTree.UpdatePlayPos(m_pSndFile->GetpModDoc(), nullptr);
		}
	}
	m_pSndFile = nullptr;
	m_wndToolBar.SetCurrentSong(nullptr);
	ResetNotificationBuffer();
}


void CMainFrame::SetPlaybackSoundFile(CTrackerSoundFile *pSndFile)
{
	MPT_ASSERT_ALWAYS(pSndFile);
	m_pSndFile = pSndFile;
}


bool CMainFrame::PlayMod(CModDoc *pModDoc)
{
	MPT_ASSERT_ALWAYS(!theApp.GetGlobalMutexRef().IsLockedByCurrentThread());
	if(!pModDoc) return false;
	CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
	if(!IsValidSoundFile(sndFile)) return false;

	// if something is playing, pause it
	PausePlayback();
	GenerateStopNotification();

	UnsetPlaybackSoundFile();

	// open audio device if not already open
	if (!PreparePlayback()) return false;

	// set mixing parameters in CSoundFile
	UpdateAudioParameters(sndFile);

	SetPlaybackSoundFile(&sndFile);

	const bool bPaused = m_pSndFile->IsPaused();
	const bool bPatLoop = m_pSndFile->m_PlayState.m_flags[SONG_PATTERNLOOP];

	m_pSndFile->m_PlayState.m_flags.reset(SONG_FADINGSONG | SONG_ENDREACHED);

	if(!bPatLoop && bPaused) sndFile.m_PlayState.m_flags.set(SONG_PAUSED);
	sndFile.SetRepeatCount((TrackerSettings::Instance().gbLoopSong) ? -1 : 0);

	sndFile.InitPlayer(true);
	sndFile.ResumePlugins();

	m_wndToolBar.SetCurrentSong(m_pSndFile);

	m_VUMeterInput = VUMeter();
	m_VUMeterOutput = VUMeter();

	UpdateMetronomeSamples();

	if(!StartPlayback())
	{
		UnsetPlaybackSoundFile();
		return false;
	}

	return true;
}


bool CMainFrame::PauseMod(CModDoc *pModDoc)
{
	MPT_ASSERT_ALWAYS(!theApp.GetGlobalMutexRef().IsLockedByCurrentThread());
	if(pModDoc && (pModDoc != GetModPlaying())) return false;
	if(!IsPlaying()) return true;

	PausePlayback();
	GenerateStopNotification();

	UnsetPlaybackSoundFile();

	StopPlayback();

	return true;
}


bool CMainFrame::StopMod(CModDoc *pModDoc)
{
	MPT_ASSERT_ALWAYS(!theApp.GetGlobalMutexRef().IsLockedByCurrentThread());
	if(pModDoc && (pModDoc != GetModPlaying())) return false;
	if(!IsPlaying()) return true;

	PausePlayback();
	SetElapsedTime(0);
	GenerateStopNotification();

	m_pSndFile->ResetPlayPos();
	m_pSndFile->ResetChannels();
	UnsetPlaybackSoundFile();

	StopPlayback();

	return true;
}


bool CMainFrame::StopSoundFile(CTrackerSoundFile *pSndFile)
{
	MPT_ASSERT_ALWAYS(!theApp.GetGlobalMutexRef().IsLockedByCurrentThread());
	if(!IsValidSoundFile(pSndFile)) return false;
	if(pSndFile != m_pSndFile) return false;
	if(!IsPlaying()) return true;

	PausePlayback();
	GenerateStopNotification();

	UnsetPlaybackSoundFile();

	StopPlayback();

	return true;
}


bool CMainFrame::PlaySoundFile(CTrackerSoundFile *pSndFile)
{
	MPT_ASSERT_ALWAYS(!theApp.GetGlobalMutexRef().IsLockedByCurrentThread());
	if(!IsValidSoundFile(pSndFile)) return false;

	PausePlayback();
	GenerateStopNotification();

	if(m_pSndFile != pSndFile)
	{
		UnsetPlaybackSoundFile();
	}

	if(!PreparePlayback())
	{
		UnsetPlaybackSoundFile();
		return false;
	}

	UpdateAudioParameters(*pSndFile);

	SetPlaybackSoundFile(pSndFile);

	m_pSndFile->InitPlayer(true);

	if(!StartPlayback())
	{
		UnsetPlaybackSoundFile();
		return false;
	}

	return true;
}


bool CMainFrame::PlayDLSInstrument(const CDLSBank &bank, uint32 instr, uint32 region, ModCommand::NOTE note, int volume)
{
	bool ok = false;
	BeginWaitCursor();
	{
		TrackerCriticalSection cs;
		if(ModCommand::IsNote(note))
		{
			InitPreview();
			if(bank.ExtractInstrument(m_WaveFile, 1, instr, region))
			{
				PreparePreview(note, volume);
				ok = true;
			}
		} else
		{
			PreparePreview(NOTE_NOTECUT, volume);
			ok = true;
		}
	}
	EndWaitCursor();
	if(!ok)
	{
		PausePlayback();
		UnsetPlaybackSoundFile();
		StopPlayback();
		return false;
	}
	if(IsPlaying() && (m_pSndFile == &m_WaveFile))
	{
		return true;
	}
	return PlaySoundFile(&m_WaveFile);
}


bool CMainFrame::PlaySoundFile(const mpt::PathString &filename, ModCommand::NOTE note, int volume)
{
	bool ok = false;
	BeginWaitCursor();
	{
		TrackerCriticalSection cs;
		static mpt::PathString prevFile;
		// Did we already load this file for previewing? Don't load it again if the preview is still running.
		ok = (prevFile == filename && m_pSndFile == &m_WaveFile);

		if(!ok && !filename.empty())
		{
			mpt::IO::InputFile f(filename, TrackerSettings::Instance().MiscCacheCompleteFileBeforeLoading);
			if(f.IsValid())
			{
				FileReader file = GetFileReader(f);
				if(file.IsValid())
				{
					InitPreview();
					m_WaveFile.m_PlayState.m_flags.set(SONG_PAUSED);
					// Avoid hanging audio while reading file - we have removed all sample and instrument references before,
					// so it's safe to replace the sample / instrument now.
					cs.Leave();
					ok = m_WaveFile.ReadInstrumentFromFile(1, file, TrackerSettings::Instance().m_MayNormalizeSamplesOnLoad);
					cs.Enter();
					if(!ok)
					{
						// Try reading as sample if reading as instrument fails
						ok = m_WaveFile.ReadSampleFromFile(1, file, TrackerSettings::Instance().m_MayNormalizeSamplesOnLoad);
						m_WaveFile.AllocateInstrument(1, 1);
					}
				}
			}
		}
		if(ok)
		{
			// Write notes to pattern. Also done if we have previously loaded this file, since we might be previewing another note now.
			PreparePreview(note, volume);
			prevFile = filename;
		}
	}
	EndWaitCursor();
	if(!ok)
	{
		PausePlayback();
		UnsetPlaybackSoundFile();
		StopPlayback();
		return false;
	}
	if(IsPlaying() && (m_pSndFile == &m_WaveFile))
	{
		return true;
	}
	return PlaySoundFile(&m_WaveFile);
}


bool CMainFrame::PlaySoundFile(CTrackerSoundFile &sndFile, INSTRUMENTINDEX nInstrument, SAMPLEINDEX nSample, ModCommand::NOTE note, int volume)
{
	bool ok = false;
	BeginWaitCursor();
	{
		TrackerCriticalSection cs;
		InitPreview();
		m_WaveFile.ChangeModTypeTo(sndFile.GetType(), false);
		m_WaveFile.m_playBehaviour = sndFile.m_playBehaviour;
		if ((nInstrument) && (nInstrument <= sndFile.GetNumInstruments()))
		{
			m_WaveFile.m_nInstruments = 1;
			m_WaveFile.m_nSamples = 32;
		} else
		{
			m_WaveFile.m_nInstruments = 0;
			m_WaveFile.m_nSamples = 1;
		}
		if (nInstrument != INSTRUMENTINDEX_INVALID && nInstrument <= sndFile.GetNumInstruments())
		{
			m_WaveFile.ReadInstrumentFromSong(1, sndFile, nInstrument);
		} else if(nSample != SAMPLEINDEX_INVALID && nSample <= sndFile.GetNumSamples())
		{
			m_WaveFile.ReadSampleFromSong(1, sndFile, nSample);
		}
		PreparePreview(note, volume);
		ok = true;
	}
	EndWaitCursor();
	if(!ok)
	{
		PausePlayback();
		UnsetPlaybackSoundFile();
		StopPlayback();
		return false;
	}
	if(IsPlaying() && (m_pSndFile == &m_WaveFile))
	{
		return true;
	}
	return PlaySoundFile(&m_WaveFile);
}


void CMainFrame::InitPreview()
{
	m_WaveFile.Destroy();
	m_WaveFile.Create(MOD_TYPE_MPT, 2);
	m_WaveFile.Order().SetDefaultTempoInt(125);
	m_WaveFile.Order().SetDefaultSpeed(6);
	m_WaveFile.m_nInstruments = 1;
	m_WaveFile.m_nTempoMode = TempoMode::Classic;
	m_WaveFile.Order().assign(1, 0);
	m_WaveFile.Patterns.Insert(0, 2);
	m_WaveFile.m_SongFlags = SONG_LINEARSLIDES;
}


void CMainFrame::PreparePreview(ModCommand::NOTE note, int volume)
{
	if(!ModCommand::IsNote(note))
	{
		for(auto &chn : m_WaveFile.m_PlayState.Chn)
		{
			chn.nFadeOutVol = 0;
			chn.dwFlags.set(CHN_NOTEFADE | CHN_FASTVOLRAMP);
		}
		return;
	}
	m_WaveFile.m_PlayState.m_flags.reset(SONG_PAUSED);
	m_WaveFile.SetRepeatCount(-1);
	m_WaveFile.ResetPlayPos();

	const CModDoc *activeDoc = GetActiveDoc();
	if(activeDoc != nullptr && (TrackerSettings::Instance().patternSetup & PatternSetup::NoLoudSamplePreview))
	{
		m_WaveFile.SetMixLevels(activeDoc->GetSoundFile().GetMixLevels());
		m_WaveFile.m_nSamplePreAmp = activeDoc->GetSoundFile().m_nSamplePreAmp;
	} else
	{
		// Preview at 0dB
		m_WaveFile.SetMixLevels(MixLevels::v1_17RC3);
		m_WaveFile.m_nSamplePreAmp = static_cast<uint32>(m_WaveFile.GetPlayConfig().getNormalSamplePreAmp());
	}

	// Avoid global volume ramping when trying samples in the treeview.
	m_WaveFile.m_nDefaultGlobalVolume = m_WaveFile.m_PlayState.m_nGlobalVolume = (volume > 0) ? volume : MAX_GLOBAL_VOLUME;

	if(m_WaveFile.Patterns.IsValidPat(0))
	{
		auto m = m_WaveFile.Patterns[0].GetRow(0);
		if(m_WaveFile.GetNumSamples() > 0)
		{
			m[0].note = note;
			m[0].instr = 1;
		}
		// Infinite loop on second row
		m = m_WaveFile.Patterns[0].GetRow(1);
		m[0].command = CMD_POSITIONJUMP;
		m[1].command = CMD_PATTERNBREAK;
		m[1].param = 1;
	}
	m_WaveFile.InitPlayer(true);
}


WindowHandle CMainFrame::GetFollowSong() const
{
	return GetModPlaying() ? GetModPlaying()->GetFollowWnd() : NULL;
}


void CMainFrame::IdleHandlerSounddevice()
{
	MPT_TRACE_SCOPE();
	if(gpSoundDevice)
	{
		const FlagSet<SoundDevice::RequestFlags> requestFlags = gpSoundDevice->GetRequestFlags();
		if(requestFlags[SoundDevice::RequestFlagClose])
		{
			StopPlayback();
			audioCloseDevice();
		} else if(requestFlags[SoundDevice::RequestFlagReset])
		{
			ResetSoundCard();
		} else if(requestFlags[SoundDevice::RequestFlagRestart])
		{
			RestartPlayback();
		} else
		{
			gpSoundDevice->OnIdle();
		}
	}
}


void CMainFrame::ResetSoundCard()
{
	MPT_TRACE_SCOPE();
	CMainFrame::SetupSoundCard(TrackerSettings::Instance().GetSoundDeviceSettings(TrackerSettings::Instance().GetSoundDeviceIdentifier()), TrackerSettings::Instance().GetSoundDeviceIdentifier(), TrackerSettings::Instance().m_SoundSettingsStopMode, true);
}


void CMainFrame::SetupSoundCard(SoundDevice::Settings deviceSettings, SoundDevice::Identifier deviceIdentifier, SoundDeviceStopMode stoppedMode, bool forceReset)
{
	MPT_TRACE_SCOPE();
	if(forceReset
		|| (TrackerSettings::Instance().GetSoundDeviceIdentifier() != deviceIdentifier)
		|| (TrackerSettings::Instance().GetSoundDeviceSettings(deviceIdentifier) != deviceSettings)
		|| (TrackerSettings::Instance().m_SoundSettingsStopMode != stoppedMode)
		)
	{
		CModDoc *pActiveMod = nullptr;
		if(IsPlaying())
		{
			if ((m_pSndFile) && (!m_pSndFile->IsPaused())) pActiveMod = GetModPlaying();
			PauseMod();
		}
		if(gpSoundDevice)
		{
			gpSoundDevice->Close();
		}
		TrackerSettings::Instance().m_SoundSettingsStopMode = stoppedMode;
		switch(stoppedMode)
		{
			case SoundDeviceStopModeClosed:
				deviceSettings.KeepDeviceRunning = true;
				break;
			case SoundDeviceStopModeStopped:
				deviceSettings.KeepDeviceRunning = false;
				break;
			case SoundDeviceStopModePlaying:
				deviceSettings.KeepDeviceRunning = true;
				break;
		}
		TrackerSettings::Instance().SetSoundDeviceIdentifier(deviceIdentifier);
		TrackerSettings::Instance().SetSoundDeviceSettings(deviceIdentifier, deviceSettings);
		TrackerSettings::Instance().MixerOutputChannels = deviceSettings.Channels;
		TrackerSettings::Instance().MixerNumInputChannels = deviceSettings.InputChannels;
		TrackerSettings::Instance().MixerSamplerate = deviceSettings.Samplerate;
		if(pActiveMod)
		{
			PlayMod(pActiveMod);
		}
		UpdateWindow();
	} else
	{
		// No need to restart playback
		TrackerCriticalSection cs;
		if(GetSoundFilePlaying()) UpdateAudioParameters(*GetSoundFilePlaying(), false);
	}
}


void CMainFrame::SetupPlayer()
{
	TrackerCriticalSection cs;
	if(GetSoundFilePlaying()) UpdateAudioParameters(*GetSoundFilePlaying(), false);
}


void CMainFrame::SetupMiscOptions()
{
	const FlagSet<PatternSetup> patternSetup = TrackerSettings::Instance().patternSetup;
	if(patternSetup[PatternSetup::IgnoreMutedChannels])
		TrackerSettings::Instance().MixerFlags |= SNDMIX_MUTECHNMODE;
	else
		TrackerSettings::Instance().MixerFlags &= ~SNDMIX_MUTECHNMODE;
	{
		TrackerCriticalSection cs;
		if(GetSoundFilePlaying()) UpdateAudioParameters(*GetSoundFilePlaying());
	}

	m_wndToolBar.SetFlat(patternSetup[PatternSetup::FlatToolbarButtons]);

	UpdateTree(nullptr, UpdateHint().MPTOptions());
	theApp.UpdateAllViews(UpdateHint().MPTOptions());
}


void CMainFrame::SetupMidi(FlagSet<MidiSetup> d, uint32 n)
{
	bool deviceChanged = (TrackerSettings::Instance().m_nMidiDevice != n);
	TrackerSettings::Instance().midiSetup = d;
	TrackerSettings::Instance().SetMIDIDevice(n);
	if(deviceChanged && midiInData.isOpen)
	{
		// Device has changed, close the old one.
		midiCloseDevice();
		midiOpenDevice();
	}
}


void CMainFrame::SetUserText(const mpt::ustring &text)
{
	if(!text.empty() || !m_userText.empty())
	{
		m_userText = text;
		OnUpdateUser(nullptr);
	}
}


void CMainFrame::SetInfoText(const mpt::ustring &text)
{
	if(!text.empty() || !m_infoText.empty())
	{
		m_infoText = text;
		OnUpdateInfo(nullptr);
	}
}


void CMainFrame::SetXInfoText(const mpt::ustring &text)
{
	if(!text.empty() || !m_xInfoText.empty())
	{
		m_xInfoText = text;
		OnUpdateInfo(nullptr);
	}
}


void CMainFrame::SetHelpText(const mpt::ustring &text)
{
	m_wndStatusBar.SetPaneText(0, text);
}


mpt::ustring CMainFrame::GetHelpText() const
{
	return m_wndStatusBar.GetPaneText(0);
}


void CMainFrame::OnDocumentCreated(CModDoc *pModDoc)
{
	m_wndTree.OnDocumentCreated(pModDoc);
	UpdateMRUList();
	UpdateDocumentCount();
}


void CMainFrame::OnDocumentClosed(CModDoc *pModDoc)
{
	if (pModDoc == GetModPlaying()) PauseMod();

	m_wndTree.OnDocumentClosed(pModDoc);
	// We don't do UpdateDocumentCount() here because the document still exists at this point in time - the document template informs us instead.
}


void CMainFrame::UpdateTree(CModDoc *pModDoc, UpdateHint hint, HintObject *pHint)
{
	m_wndTree.OnUpdate(pModDoc, hint, pHint);
}


void CMainFrame::RefreshDlsBanks()
{
	m_wndTree.RefreshDlsBanks();
}


/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers


class CPropertySheetMPT : public PropertySheet
{
	using PropertySheet::PropertySheet;

	bool PreTranslateMessage(int event) override
	{
		if((event == FL_KEYBOARD || event == FL_KEYUP) && DialogBase::HandleGlobalKeyMessage(ui::KeyFromEvent(), (event == FL_KEYUP) ? ui::KeyFlagRelease : 0))
			return true;

		return PropertySheet::PreTranslateMessage(event);
	}

	bool OnInitDialog() override
	{
		return PropertySheet::OnInitDialog();
	}

	UI_DECLARE_MESSAGE_MAP()
};

UI_MESSAGE_MAP_BEGIN(CPropertySheetMPT, PropertySheet)
UI_MESSAGE_MAP_END()


void CMainFrame::OnViewOptions()
{
	if (m_bOptionsLocked)
		return;

	CPropertySheetMPT dlg(UL_("OpenMPT Setup"), this, m_nLastOptionsPage);
	struct Pages
	{
		COptionsGeneral general;
		COptionsSoundcard sounddlg{TrackerSettings::Instance().m_SoundDeviceIdentifier};
		COptionsSampleEditor smpeditor;
		COptionsKeyboard keyboard;
		COptionsColors colors;
		COptionsMixer mixerdlg;
		COptionsPlayer dspdlg;
		CMidiSetupDlg mididlg{TrackerSettings::Instance().midiSetup, TrackerSettings::Instance().GetCurrentMIDIDevice()};
		PathConfigDlg pathsdlg;
		COptionsAdvanced advanced;
		COptionsWine winedlg;
	};
	mpt::heap_value<Pages> pages;
	dlg.AddPage(&pages->general);
	dlg.AddPage(&pages->sounddlg);
	dlg.AddPage(&pages->mixerdlg);
	dlg.AddPage(&pages->dspdlg);
	dlg.AddPage(&pages->smpeditor);
	dlg.AddPage(&pages->keyboard);
	dlg.AddPage(&pages->colors);
	dlg.AddPage(&pages->mididlg);
	dlg.AddPage(&pages->pathsdlg);
	dlg.AddPage(&pages->advanced);
	m_bOptionsLocked = true;
	m_SoundCardOptionsDialog = &pages->sounddlg;

	dlg.DoModal();

	m_SoundCardOptionsDialog = nullptr;
	m_bOptionsLocked = false;
	m_wndTree.OnOptionsChanged();
}


void CMainFrame::OnPluginManager()
{
	PLUGINDEX nPlugslot = PLUGINDEX_INVALID;
	CModDoc* pModDoc = GetActiveDoc();

	if (pModDoc)
	{
		CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();
		//Find empty plugin slot
		for (PLUGINDEX nPlug = 0; nPlug < MAX_MIXPLUGINS; nPlug++)
		{
			if (sndFile.m_MixPlugins[nPlug].pMixPlugin == nullptr)
			{
				nPlugslot = nPlug;
				break;
			}
		}
	}

	CSelectPluginDlg dlg(GetActiveDoc(), nPlugslot, this);
	if(dlg.DoModal() == IDOK && pModDoc)
	{
		pModDoc->SetModified();
		//Refresh views
		pModDoc->UpdateAllViews(nullptr, PluginHint().Info().Names().ModType());
		//Refresh Controls
		CChildFrame *pActiveChild = (CChildFrame *)MDIGetActive();
		pActiveChild->ForceRefresh();
	}
}


void CMainFrame::OnClipboardManager()
{
	PatternClipboardDialog::Show();
}


void CMainFrame::OnAddDlsBank()
{
	FileDialog dlg = OpenFileDialog()
		.AllowMultiSelect()
		.ExtensionFilter("All Sound Banks|*.dls;*.sbk;*.sf2;*.sf3;*.sf4;*.mss|"
			"Downloadable Sounds Banks (*.dls)|*.dls;*.mss|"
			"SoundFont 2.0 Banks (*.sf2)|*.sbk;*.sf2;*.sf3;*.sf4|"
			"All Files (*.*)|*.*||");
	if(!dlg.Show()) return;

	BeginWaitCursor();
	bool ok = true;
	for(const auto &file : dlg.GetFilenames())
	{
		ok &= CTrackApp::AddDLSBank(file);
	}
	if(!ok)
	{
		Reporting::Error("At least one selected file was not a valid sound bank.");
	}
	m_wndTree.RefreshDlsBanks();
	EndWaitCursor();
}


void CMainFrame::OnImportMidiLib()
{
	FileDialog dlg = OpenFileDialog()
		.ExtensionFilter("Text and INI files (*.txt,*.ini)|*.txt;*.ini;*.dls;*.sf2;*.sf3;*.sf4;*.sbk|"
			"Downloadable Sound Banks (*.dls)|*.dls;*.mss|"
			"SoundFont 2.0 banks (*.sf2)|*.sbk;*.sf2;*.sf3;*.sf4|"
			"Gravis UltraSound (ultrasnd.ini)|ultrasnd.ini|"
			"All Files (*.*)|*.*||");
	if(!dlg.Show()) return;

	BeginWaitCursor();
	CTrackApp::ImportMidiConfig(dlg.GetFirstFile());
	m_wndTree.RefreshMidiLibrary();
	EndWaitCursor();
}


void CMainFrame::OnTimer(uintptr_t timerID)
{
	switch(timerID)
	{
		case TIMERID_GUI:
			OnTimerGUI();
			break;
		case TIMERID_NOTIFY:
			OnTimerNotify();
			break;
	}
}


void CMainFrame::OnTimerGUI()
{
	IdleHandlerSounddevice();
	UpdateLastFocusedItem();

	// Display Time in status bar
	if(m_pSndFile != nullptr && m_pSndFile->GetSampleRate() != 0)
	{
		samplecount_t time = Util::muldivr(m_pSndFile->GetTotalSampleCount(), 10, m_pSndFile->GetSampleRate());
		if(time != m_dwTimeSec)
		{
			m_dwTimeSec = time;
			m_nAvgMixChn = m_nMixChn;
			OnUpdateTime(nullptr);
		}
	}

	if(m_AutoSaver->IsEnabled())
	{
		bool success = m_AutoSaver->DoSave();
		if(!success)  // autosave failure; bring up options.
		{
			CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_PATHS;
			OnViewOptions();
		}
	}

	if(m_SoundCardOptionsDialog)
	{
		m_SoundCardOptionsDialog->UpdateStatistics();
	}

	{
		mpt::profiler::estimate_all_frequencies();
	}

#ifdef USE_PROFILER
	{

		Profiler::Update();

		Wnd * cwnd = Wnd::FromHandle(this->m_hWndMDIClient);
		CClientDC dc(cwnd);

		int height = 16;
		int width = 256;

		std::vector<std::string> catnames = Profiler::GetCategoryNames();
		std::vector<double> cats = Profiler::DumpCategories();

		for(int i=0; i<Profiler::CategoriesCount; i++)
		{
			dc.FillSolidRect(0, i * height, (int)(width * cats[i]), height, RGB(255,0,0));
			dc.FillSolidRect((int)(width * cats[i]), i * height, width - (int)(width * cats[i]), height, RGB(192,192,192));
			RECT rect;
			cwnd->GetClientRect(&rect);
			rect.left += width;
			rect.top += i * height;
			mpt::ustring s = MPT_UFORMAT("{}{} {}")(mpt::ufmt::right(6, mpt::ufmt::fix(cats[i] * 100.0, 3)), TEXT("%"), mpt::ToUnicode(mpt::Charset::ASCII, catnames[i]));
			dc.DrawText(s, -1, &rect, ui::TextLeft);
		}

		mpt::ustring text;

		dc.SetBkMode(OPAQUE);
		dc.SetBkColor(RGB(192,192,192));

		text += mpt::ToUnicode(mpt::Charset::ASCII, Profiler::DumpProfiles());

		{
			auto measure =
				[&](auto s, auto f) {
					mpt::profiler::default_clock::rep sum{};
					std::size_t count = 1000;
					volatile std::invoke_result<decltype(f)>::type dummy = 0;
					for (std::size_t i = 0; i < count; ++i) {
						mpt::profiler::default_clock::rep beg = mpt::profiler::default_clock::now_raw();
						dummy = std::invoke(f);
						mpt::profiler::default_clock::rep end = mpt::profiler::default_clock::now_raw();
						sum += (end - beg);
					}
					text += MPT_UFORMAT("{} {}\r\n")(mpt::ufmt::dec0<8>(sum / count), s);
				};
			uint32 dummy{};
			measure(TEXT("default       "), []() { return mpt::profiler::default_clock::now_raw(); });
			measure(TEXT("highres       "), []() { return mpt::profiler::highres_clock::now_raw(); });
			measure(TEXT("exact         "), []() { return mpt::profiler::exact_clock::now_raw(); });
			measure(TEXT("fast          "), []() { return mpt::profiler::fast_clock::now_raw(); });
#if MPT_PROFILER_CLOCK_TSC_RUNTIME
			measure(TEXT("tsc rt        "), []() { return mpt::profiler::clock::tsc_runtime::now_raw(); });
			measure(TEXT("tsc rt relaxed"), []() { return mpt::profiler::clock::tsc_runtime::now_raw(std::memory_order_relaxed); });
			measure(TEXT("tsc rt consume"), []() { return mpt::profiler::clock::tsc_runtime::now_raw(std::memory_order_consume); });
			measure(TEXT("tsc rt acquire"), []() { return mpt::profiler::clock::tsc_runtime::now_raw(std::memory_order_acquire); });
			measure(TEXT("tsc rt release"), []() { return mpt::profiler::clock::tsc_runtime::now_raw(std::memory_order_release); });
			measure(TEXT("tsc rt acq_rel"), []() { return mpt::profiler::clock::tsc_runtime::now_raw(std::memory_order_acq_rel); });
			measure(TEXT("tsc rt seq_cst"), []() { return mpt::profiler::clock::tsc_runtime::now_raw(std::memory_order_seq_cst); });
			measure(TEXT("tscprt relaxed"), [&]() { return mpt::profiler::clock::tsc_runtime::now_raw(std::memory_order_relaxed, &dummy); });
			measure(TEXT("tscprt consume"), [&]() { return mpt::profiler::clock::tsc_runtime::now_raw(std::memory_order_consume, &dummy); });
			measure(TEXT("tscprt acquire"), [&]() { return mpt::profiler::clock::tsc_runtime::now_raw(std::memory_order_acquire, &dummy); });
			measure(TEXT("tscprt release"), [&]() { return mpt::profiler::clock::tsc_runtime::now_raw(std::memory_order_release, &dummy); });
			measure(TEXT("tscprt acq_rel"), [&]() { return mpt::profiler::clock::tsc_runtime::now_raw(std::memory_order_acq_rel, &dummy); });
			measure(TEXT("tscprt seq_cst"), [&]() { return mpt::profiler::clock::tsc_runtime::now_raw(std::memory_order_seq_cst, &dummy); });
#endif
#if MPT_PROFILER_CLOCK_TSC
			measure(TEXT("tsc           "), []() { return mpt::profiler::clock::tsc::now_raw(); });
			measure(TEXT("tsc  relaxed  "), []() { return mpt::profiler::clock::tsc::now_raw(std::memory_order_relaxed); });
			measure(TEXT("tsc  consume  "), []() { return mpt::profiler::clock::tsc::now_raw(std::memory_order_consume); });
			measure(TEXT("tsc  acquire  "), []() { return mpt::profiler::clock::tsc::now_raw(std::memory_order_acquire); });
			measure(TEXT("tsc  release  "), []() { return mpt::profiler::clock::tsc::now_raw(std::memory_order_release); });
			measure(TEXT("tsc  acq_rel  "), []() { return mpt::profiler::clock::tsc::now_raw(std::memory_order_acq_rel); });
			measure(TEXT("tsc  seq_cst  "), []() { return mpt::profiler::clock::tsc::now_raw(std::memory_order_seq_cst); });
#endif
#if MPT_PROFILER_CLOCK_QUERY_PERFORMANCE_COUNTER
			measure(TEXT("qpc           "), []() { return mpt::profiler::clock::query_performance_counter::now_raw(); });
#endif
#if MPT_PROFILER_CLOCK_GET_TICK_COUNT
			measure(TEXT("gtc           "), []() { return mpt::profiler::clock::get_tick_count::now_raw(); });
#endif
#if MPT_PROFILER_CLOCK_STD_CHRONO
			measure(TEXT("stdhighres    "), []() { return mpt::profiler::clock::chrono_high_resolution::now_raw(); });
			measure(TEXT("stdsteady     "), []() { return mpt::profiler::clock::chrono_steady::now_raw(); });
			measure(TEXT("stdsystem     "), []() { return mpt::profiler::clock::chrono_system::now_raw(); });
#endif
#if MPT_PROFILER_CLOCK_SYSTEM
			measure(TEXT("system        "), []() { return mpt::profiler::clock::system::now_raw(); });
#endif
		}
		{
			auto format_frequency =
				[](mpt::somefloat64 f) {
					if (f >= 1'000'000'000.0_sf64) {
						return MPT_UFORMAT("{} GHz")(mpt::ufmt::fix(f * 0.000'000'001, 3));
					} else if (f >= 1'000'000.0_sf64) {
						return MPT_UFORMAT("{} MHz")(mpt::ufmt::fix(f * 0.000'001, 3));
					} else if (f >= 1'000.0_sf64) {
						return MPT_UFORMAT("{} kHz")(mpt::ufmt::fix(f * 0.001, 3));
					} else {
						return MPT_UFORMAT("{} Hz")(mpt::ufmt::fix(f * 1.0, 3));
					}
				};
			text += MPT_UFORMAT("{} Frequency: {}\r\n")(TEXT("default   "), format_frequency(mpt::profiler::get_frequency<mpt::profiler::default_clock>().value_or(0.0_sf64)));
			text += MPT_UFORMAT("{} Frequency: {}\r\n")(TEXT("highres   "), format_frequency(mpt::profiler::get_frequency<mpt::profiler::highres_clock>().value_or(0.0_sf64)));
			text += MPT_UFORMAT("{} Frequency: {}\r\n")(TEXT("exact     "), format_frequency(mpt::profiler::get_frequency<mpt::profiler::exact_clock>().value_or(0.0_sf64)));
			text += MPT_UFORMAT("{} Frequency: {}\r\n")(TEXT("fast      "), format_frequency(mpt::profiler::get_frequency<mpt::profiler::fast_clock>().value_or(0.0_sf64)));
#if MPT_PROFILER_CLOCK_TSC_RUNTIME
			text += MPT_UFORMAT("{} Frequency: {}\r\n")(TEXT("TSCruntime"), format_frequency(mpt::profiler::get_frequency<mpt::profiler::clock::tsc_runtime>().value_or(0.0_sf64)));
#endif
#if MPT_PROFILER_CLOCK_TSC
			text += MPT_UFORMAT("{} Frequency: {}\r\n")(TEXT("TSC       "), format_frequency(mpt::profiler::get_frequency<mpt::profiler::clock::tsc>().value_or(0.0_sf64)));
#endif
#if MPT_PROFILER_CLOCK_QUERY_PERFORMANCE_COUNTER
			text += MPT_UFORMAT("{} Frequency: {}\r\n")(TEXT("QPC       "), format_frequency(mpt::profiler::get_frequency<mpt::profiler::clock::query_performance_counter>().value_or(0.0_sf64)));
#endif
#if MPT_PROFILER_CLOCK_GET_TICK_COUNT
			text += MPT_UFORMAT("{} Frequency: {}\r\n")(TEXT("GTC       "), format_frequency(mpt::profiler::get_frequency<mpt::profiler::clock::get_tick_count>().value_or(0.0_sf64)));
#endif
#if MPT_PROFILER_CLOCK_STD_CHRONO
			text += MPT_UFORMAT("{} Frequency: {}\r\n")(TEXT("stdhighres"), format_frequency(mpt::profiler::get_frequency<mpt::profiler::clock::chrono_high_resolution>().value_or(0.0_sf64)));
			text += MPT_UFORMAT("{} Frequency: {}\r\n")(TEXT("stdsteady "), format_frequency(mpt::profiler::get_frequency<mpt::profiler::clock::chrono_steady>().value_or(0.0_sf64)));
			text += MPT_UFORMAT("{} Frequency: {}\r\n")(TEXT("stdsystem "), format_frequency(mpt::profiler::get_frequency<mpt::profiler::clock::chrono_system>().value_or(0.0_sf64)));
#endif
#if MPT_PROFILER_CLOCK_SYSTEM
			text += MPT_UFORMAT("{} Frequency: {}\r\n")(TEXT("system    "), format_frequency(mpt::profiler::get_frequency<mpt::profiler::clock::system>().value_or(0.0_sf64)));
#endif
		}
		
		RECT rect;
		cwnd->GetClientRect(&rect);
		rect.top += Profiler::CategoriesCount * height;
		dc.DrawText(text, -1, &rect, ui::TextLeft);

		cwnd->Detach();

	}
#endif // USE_PROFILER

}


CModDoc *CMainFrame::GetActiveDoc() const
{
	ChildFrameBase *pMDIActive = MDIGetActive();
	if (pMDIActive)
	{
		return static_cast<CModDoc *>(pMDIActive->GetActiveDocument());
	}
	return nullptr;
}


View *CMainFrame::GetActiveView() const
{
	ChildFrameBase *pMDIActive = MDIGetActive();
	if (pMDIActive)
	{
		return pMDIActive->GetActiveView();
	}

	return nullptr;
}


void CMainFrame::SwitchToActiveView()
{
	Wnd *wnd = GetActiveView();
	if (wnd)
	{
		// Hack: If the upper view is active, we only get the "container" (the dialog view with the tabs), not the view itself.
		if(auto *controlView = dynamic_cast<CModControlView *>(wnd))
		{
			wnd = controlView->GetCurrentControlDlg();
		}
		wnd->SetFocus();
	}
}


void CMainFrame::OnUpdateTime(CmdUI *)
{
	const auto timeSec = m_dwTimeSec / 10u;
	mpt::ustring s = ui::Format(UL_("%u:%02u:%02u.%u"), timeSec / 3600u, (timeSec / 60u) % 60u, timeSec % 60u, m_dwTimeSec % 10u);

	if(m_pSndFile != nullptr && m_pSndFile != &m_WaveFile && !m_pSndFile->IsPaused())
	{
		PATTERNINDEX nPat = m_pSndFile->m_PlayState.m_nPattern;
		if(m_pSndFile->Patterns.IsValidIndex(nPat))
		{
			if(nPat < 10) s += UL_(" ");
			if(nPat < 100) s += UL_(" ");
			s += ui::Format(UL_(" [%u]"), nPat);
		}
		s += ui::Format(UL_(" %uch"), m_nAvgMixChn);
	}
	m_wndStatusBar.SetPaneText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_TIME), s);
}


void CMainFrame::OnUpdateUser(CmdUI *)
{
	m_wndStatusBar.SetPaneText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_USER), m_userText);
}


void CMainFrame::OnUpdateInfo(CmdUI *)
{
	m_wndStatusBar.SetPaneText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_INFO), m_infoText);
}


void CMainFrame::OnUpdateXInfo(CmdUI *)
{
	m_wndStatusBar.SetPaneText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_XINFO), m_xInfoText);
}

void CMainFrame::OnPlayerPause()
{
	if (GetModPlaying())
	{
		GetModPlaying()->OnPlayerPause();
	} else
	{
		PauseMod();
	}
}


void CMainFrame::OpenMenuItemFile(const uint32 nId, const bool isTemplateFile)
{
	const uint32 nIdBegin = (isTemplateFile) ? ID_FILE_OPENTEMPLATE : ID_EXAMPLE_MODULES;
	const std::vector<mpt::PathString> &vecFilePaths = (isTemplateFile) ? m_TemplateModulePaths : m_ExampleModulePaths;

	const uint32 nIndex = nId - nIdBegin;
	if (nIndex < vecFilePaths.size())
	{
		const mpt::PathString& sPath = vecFilePaths[nIndex];
		const bool bExists = FileSystem::IsFile(sPath);
		Document *pDoc = nullptr;
		if(bExists)
		{
			pDoc = theApp.GetModDocTemplate()->OpenTemplateFile(sPath, !isTemplateFile);
		}
		if(!pDoc)
		{
			Reporting::Notification(UL_("The file '") + sPath.ToUnicode() + UL_("' ") + (bExists ? UL_("exists but can't be read.") : UL_("does not exist.")));
		}
	} else
	{
		MPT_ASSERT(nId == uint32(isTemplateFile ? ID_FILE_OPENTEMPLATE_LASTINRANGE : ID_EXAMPLE_MODULES_LASTINRANGE));
		FileDialog::PathList files;
		theApp.OpenModulesDialog(files, isTemplateFile ? theApp.GetUserTemplatesPath() : theApp.GetExampleSongsPath());
		for(const auto &file : files)
		{
			theApp.OpenDocumentFile(file);
		}
	}
}


void CMainFrame::OnOpenTemplateModule(uint32 nId)
{
	OpenMenuItemFile(nId, true/*open template menu file*/);
}


void CMainFrame::OnExampleSong(uint32 nId)
{
	OpenMenuItemFile(nId, false/*open example menu file*/);
}


void CMainFrame::OnOpenMRUItem(uint32 nId)
{
	// Copy, since opening the file reorders mruFiles
	const mpt::PathString path = TrackerSettings::Instance().mruFiles[nId - ID_MRU_LIST_FIRST];
	theApp.OpenDocumentFile(path);
}


void CMainFrame::OnUpdateMRUItem(CmdUI *cmd)
{
	cmd->Enable(!TrackerSettings::Instance().mruFiles.empty());
}


LResult CMainFrame::OnInvalidatePatterns(WParam, LParam)
{
	theApp.UpdateAllViews(UpdateHint().MPTOptions());
	return true;
}


LResult CMainFrame::OnUpdatePosition(WParam, LParam lParam)
{
	OPENMPT_PROFILE_FUNCTION(Profiler::GUI);
	m_VUMeterOutput.SetDecaySpeedDecibelPerSecond(TrackerSettings::Instance().VuMeterDecaySpeedDecibelPerSecond); // update in notification update in order to avoid querying the settings framework from inside audio thread
	m_VUMeterInput.SetDecaySpeedDecibelPerSecond(TrackerSettings::Instance().VuMeterDecaySpeedDecibelPerSecond); // update in notification update in order to avoid querying the settings framework from inside audio thread
	Notification *pnotify = (Notification *)lParam;
	if (pnotify)
	{
		if(pnotify->type[Notification::EOS])
		{
			PostCommand(ID_PLAYER_STOP);
			m_currentSpeed = 0;
		}
		//Log("OnUpdatePosition: row=%d time=%lu\n", pnotify->nRow, pnotify->TimestampSamples);
		if(CModDoc *modDoc = GetModPlaying(); modDoc != nullptr)
		{
			m_wndTree.UpdatePlayPos(modDoc, pnotify);
			if (GetFollowSong())
				GetFollowSong()->SendMessage(MSG_MOD_UPDATEPOSITION, 0, lParam);
			if(m_pSndFile->m_pluginDryWetRatioChanged.any())
			{
				for(PLUGINDEX i = 0; i < MAX_MIXPLUGINS; i++)
				{
					if(m_pSndFile->m_pluginDryWetRatioChanged[i])
						modDoc->PostMessageToAllViews(MSG_MOD_PLUGINDRYWETRATIOCHANGED, i);
				}
				m_pSndFile->m_pluginDryWetRatioChanged.reset();
			}
			// Update envelope views if speed has changed
			if(m_pSndFile->m_PlayState.m_nMusicSpeed != m_currentSpeed)
			{
				m_currentSpeed = m_pSndFile->m_PlayState.m_nMusicSpeed;
				modDoc->UpdateAllViews(InstrumentHint().Envelope());
			}
		}
		m_nMixChn = pnotify->mixedChannels;

		bool duplicateMono = false;
		if(pnotify->masterVUinChannels == 1 && pnotify->masterVUoutChannels > 1)
		{
			duplicateMono = true;
		} else if(pnotify->masterVUoutChannels == 1 && pnotify->masterVUinChannels > 1)
		{
			duplicateMono = true;
		}
		uint8 countChan = 0;
		uint32 vu[VUMeter::maxChannels * 2];
		MemsetZero(vu);
		std::copy(pnotify->masterVUin.begin(), pnotify->masterVUin.begin() + pnotify->masterVUinChannels, vu + countChan);

		countChan += pnotify->masterVUinChannels;
		if(pnotify->masterVUinChannels == 1 && duplicateMono)
		{
			std::copy(pnotify->masterVUin.begin(), pnotify->masterVUin.begin() + 1, vu + countChan);
			countChan += 1;
		}

		std::copy(pnotify->masterVUout.begin(), pnotify->masterVUout.begin() + pnotify->masterVUoutChannels, vu + countChan);
		countChan += pnotify->masterVUoutChannels;
		if(pnotify->masterVUoutChannels == 1 && duplicateMono)
		{
			std::copy(pnotify->masterVUout.begin(), pnotify->masterVUout.begin() + 1, vu + countChan);
			countChan += 1;
		}

		m_wndToolBar.m_VuMeter.SetVuMeter(countChan, vu, pnotify->type[Notification::Stop]);

		m_wndToolBar.SetCurrentSong(m_pSndFile);
	}
	return 0;
}


LResult CMainFrame::OnUpdateViews(WParam modDoc, LParam hint)
{
	CModDoc *doc = reinterpret_cast<CModDoc *>(modDoc);
	CModDocTemplate *pDocTmpl = theApp.GetModDocTemplate();
	if(pDocTmpl && pDocTmpl->DocumentExists(doc))
	{
		// Since this message is potentially posted, we first need to verify if the document still exists
		doc->UpdateAllViews(nullptr, UpdateHint::FromLPARAM(hint));
	}
	return 0;
}


LResult CMainFrame::OnSetModified(WParam modDoc, LParam)
{
	CModDoc *doc = reinterpret_cast<CModDoc *>(modDoc);
	CModDocTemplate *pDocTmpl = theApp.GetModDocTemplate();
	if(pDocTmpl && pDocTmpl->DocumentExists(doc))
	{
		// Since this message is potentially posted, we first need to verify if the document still exists
		doc->SetModifiedFlag(doc->GetModifiedAtomic());
		doc->UpdateFrameCounts();
	}
	return 0;
}


void CMainFrame::OnPanic()
{
	// "Panic button." At the moment, it just resets all VSTi and sample notes.
	if(GetModPlaying())
		GetModPlaying()->OnPanic();
}


void CMainFrame::OnPrevOctave()
{
	uint32 n = GetBaseOctave();
	if (n > MIN_BASEOCTAVE) m_wndToolBar.SetBaseOctave(n-1);
}


void CMainFrame::OnNextOctave()
{
	uint32 n = GetBaseOctave();
	if (n < MAX_BASEOCTAVE) m_wndToolBar.SetBaseOctave(n+1);
}


void CMainFrame::OnReportBug()
{
	CTrackApp::OpenURL("https://github.com/not-magic/OpenMPT-FLTK/issues");
}


bool CMainFrame::OnInternetLink(uint32 nID)
{
	mpt::ustring url;
	switch(nID)
	{
	case ID_NETLINK_MODPLUG:	url = Build::GetURL(Build::Url::Website); break;
	case ID_NETLINK_FLTK_GITHUB:	url = UL_("https://github.com/not-magic/OpenMPT-FLTK"); break;
	case ID_NETLINK_TOP_PICKS:	url = Build::GetURL(Build::Url::TopPicks); break;
	}
	if(!url.empty())
	{
		return CTrackApp::OpenURL(url) ? true : false;
	}
	return false;
}


void CMainFrame::ShowToolbarMenu(Point screenPt)
{
	Menu menu;
	AddToolBarMenuEntries(menu);
	menu.TrackPopupMenu(screenPt, this);
}


void CMainFrame::AddToolBarMenuEntries(Menu &menu) const
{
	menu.AppendMenu(ui::MenuItemString, ID_VIEW_TOOLBAR, m_InputHandler->GetMenuText(ID_VIEW_TOOLBAR));
	menu.AppendMenu(ui::MenuItemString, IDD_TREEVIEW, m_InputHandler->GetMenuText(IDD_TREEVIEW));
	menu.AppendMenu(ui::MenuItemString | (TrackerSettings::Instance().treeViewOnLeft ? ui::MenuItemChecked : 0), ID_TREEVIEW_ON_LEFT, UL_("Tree View on &Left"));

	const FlagSet<MainToolBarItem> visibleItems = TrackerSettings::Instance().mainToolBarVisibleItems.Get();

	Menu subMenu;

	subMenu.AppendMenu(ui::MenuItemString | (visibleItems[MainToolBarItem::IconsFile] ? ui::MenuItemChecked : 0), ID_MAINBAR_SHOW_FILE_ICONS, UL_("&File Icons"));
	subMenu.AppendMenu(ui::MenuItemString | (visibleItems[MainToolBarItem::IconsEdit] ? ui::MenuItemChecked : 0), ID_MAINBAR_SHOW_EDIT_ICONS, UL_("&Edit Icons"));
	subMenu.AppendMenu(ui::MenuItemString | (visibleItems[MainToolBarItem::IconsPlayback] ? ui::MenuItemChecked : 0), ID_MAINBAR_SHOW_PLAY_ICONS, UL_("&Play / Record Icons"));
	subMenu.AppendMenu(ui::MenuItemString | (visibleItems[MainToolBarItem::Octave] ? ui::MenuItemChecked : 0), ID_MAINBAR_SHOW_OCTAVE, UL_("Base &Octave"));
	subMenu.AppendMenu(ui::MenuItemString | (visibleItems[MainToolBarItem::Tempo] ? ui::MenuItemChecked : 0), ID_MAINBAR_SHOW_TEMPO, UL_("&Tempo"));
	subMenu.AppendMenu(ui::MenuItemString | (visibleItems[MainToolBarItem::Speed] ? ui::MenuItemChecked : 0), ID_MAINBAR_SHOW_SPEED, UL_("Ticks/&Row"));
	subMenu.AppendMenu(ui::MenuItemString | (visibleItems[MainToolBarItem::RowsPerBeat] ? ui::MenuItemChecked : 0), ID_MAINBAR_SHOW_ROWSPERBEAT, UL_("Rows Per &Beat"));
	subMenu.AppendMenu(ui::MenuItemString | (visibleItems[MainToolBarItem::GlobalVolume] ? ui::MenuItemChecked : 0), ID_MAINBAR_SHOW_GLOBALVOLUME, UL_("&Global Volume"));
	subMenu.AppendMenu(ui::MenuItemString | (visibleItems[MainToolBarItem::IconsMisc] ? ui::MenuItemChecked : 0), ID_MAINBAR_SHOW_MISC_ICONS, UL_("&Misc Icons"));
	subMenu.AppendMenu(ui::MenuItemString | (visibleItems[MainToolBarItem::VUMeter] ? ui::MenuItemChecked : 0), ID_MAINBAR_SHOW_VUMETER, UL_("&VU Meters"));
	menu.AppendMenu(ui::MenuItemPopup, subMenu, UL_("Main Toolbar &Items"));
}


void CMainFrame::OnToggleMainBarShowOctave() { OnToggleMainBarItem(MainToolBarItem::Octave, ID_MAINBAR_SHOW_OCTAVE); }
void CMainFrame::OnToggleMainBarShowTempo() { OnToggleMainBarItem(MainToolBarItem::Tempo, ID_MAINBAR_SHOW_TEMPO); }
void CMainFrame::OnToggleMainBarShowSpeed() { OnToggleMainBarItem(MainToolBarItem::Speed, ID_MAINBAR_SHOW_SPEED); }
void CMainFrame::OnToggleMainBarShowRowsPerBeat() { OnToggleMainBarItem(MainToolBarItem::RowsPerBeat, ID_MAINBAR_SHOW_ROWSPERBEAT); }
void CMainFrame::OnToggleMainBarShowGlobalVolume() { OnToggleMainBarItem(MainToolBarItem::GlobalVolume, ID_MAINBAR_SHOW_GLOBALVOLUME); }
void CMainFrame::OnToggleMainBarShowVUMeter() { OnToggleMainBarItem(MainToolBarItem::VUMeter, ID_MAINBAR_SHOW_VUMETER); }
void CMainFrame::OnToggleMainBarShowFileIcons() { OnToggleMainBarItem(MainToolBarItem::IconsFile, ID_MAINBAR_SHOW_FILE_ICONS); }
void CMainFrame::OnToggleMainBarShowEditIcons() { OnToggleMainBarItem(MainToolBarItem::IconsEdit, ID_MAINBAR_SHOW_EDIT_ICONS); }
void CMainFrame::OnToggleMainBarShowPlayIcons() { OnToggleMainBarItem(MainToolBarItem::IconsPlayback, ID_MAINBAR_SHOW_PLAY_ICONS); }
void CMainFrame::OnToggleMainBarShowMiscIcons() { OnToggleMainBarItem(MainToolBarItem::IconsMisc, ID_MAINBAR_SHOW_MISC_ICONS); }

void CMainFrame::OnToggleMainBarItem(MainToolBarItem item, uint32 menuID)
{
	const bool visible = m_wndToolBar.ToggleVisibility(item);
	GetMenu()->CheckMenuItem(menuID, false | (visible ? ui::MenuItemChecked : 0));
}


void CMainFrame::OnToggleTreeViewOnLeft()
{
	const bool left = !TrackerSettings::Instance().treeViewOnLeft;
	TrackerSettings::Instance().treeViewOnLeft = left;
	m_wndTree.SetBarOnLeft(left);
	RecalcLayout();
	GetMenu()->CheckMenuItem(ID_TREEVIEW_ON_LEFT, false | (left ? ui::MenuItemChecked : 0));
}


LResult CMainFrame::OnCustomKeyMsg(WParam wParam, LParam lParam)
{
	CommandID cmd = static_cast<CommandID>(wParam);
	switch(cmd)
	{
		case kcViewTree: OnBarCheck(IDD_TREEVIEW); break;
		case kcViewOptions: OnViewOptions(); break;
		case kcViewMain: OnBarCheck(ID_VIEW_TOOLBAR); break;
		case kcFileImportMidiLib: OnImportMidiLib(); break;
		case kcFileAddSoundBank: OnAddDlsBank(); break;
		case kcPauseSong: OnPlayerPause(); break;
		case kcPrevOctave: OnPrevOctave(); break;
		case kcNextOctave: OnNextOctave(); break;
		case kcFileNew: theApp.OnFileNew(); break;
		case kcFileOpen: theApp.OnFileOpen(); break;
		case kcMidiRecord: OnMidiRecord(); break;
		case kcHelp: OnHelp(); break;
		case kcViewAddPlugin: OnPluginManager(); break;
		case kcNextDocument: MDINext(); break;
		case kcPrevDocument: MDIPrev(); break;
		case kcFileCloseAll:
			if(Fl::first_window() != GetWidget())
				return kcNull;
			theApp.OnFileCloseAll();
			break;

		//D'oh!! moddoc isn't a Wnd so we have to handle its messages and pass them on.

		case kcFileAppend:
		case kcFileSaveAs:
		case kcFileSaveCopy:
		case kcFileSaveAsWave:
		case kcFileSaveMidi:
		case kcFileSaveOPL:
		case kcFileExportCompat:
		case kcFileClose:
		case kcFileSave:
		case kcViewGeneral:
		case kcViewPattern:
		case kcViewSamples:
		case kcViewInstruments:
		case kcViewComments:
		case kcViewGraph: //rewbs.graph
		case kcViewSongProperties:
		case kcViewTempoSwing:
		case kcViewMIDImapping:
		case kcViewEditHistory:
		case kcViewChannelManager:
		case kcPlayPatternFromCursor:
		case kcPlayPatternFromStart:
		case kcPlaySongFromCursor:
		case kcPlaySongFromStart:
		case kcPlayPauseSong:
		case kcPlayStopSong:
		case kcPlaySongFromPattern:
		case kcPlaySongFromCursorPause:
		case kcPlaySongFromPatternPause:
		case kcStopSong:
		case kcToggleLoopSong:
		case kcPanic:
		case kcEstimateSongLength:
		case kcApproxRealBPM:
		case kcTempoIncrease:
		case kcTempoDecrease:
		case kcTempoIncreaseFine:
		case kcTempoDecreaseFine:
		case kcSpeedIncrease:
		case kcSpeedDecrease:
		case kcViewToggle:
			if(CModDoc *modDoc = GetActiveDoc())
				return modDoc->OnCustomKeyMsg(wParam, lParam);
			else if(cmd == kcPlayPauseSong || cmd == kcPlayStopSong || cmd == kcStopSong)
				StopPreview();
			else
				return kcNull;
			break;

		case kcSwitchToInstrLibrary:
			if(!m_wndTree.IsWindowVisible())
				return kcNull;
			if(m_bModTreeHasFocus)
				SwitchToActiveView();
			else
				m_wndTree.SetFocus();
			break;

		default:
			// If handled neither by MainFrame nor by ModDoc, send it to the active view
			// Note: MDIGetActive() will return a valid view even if we are currently in a modal dialog!
			ChildFrameBase *pMDIActive = MDIGetActive();
			Wnd *wnd = nullptr;
			if(pMDIActive)
			{
				wnd = pMDIActive->GetActiveView();
				// Hack: If the upper view is active, we only get the "container" (the dialog view with the tabs), not the view itself.
				if(auto *controlView = dynamic_cast<CModControlView *>(wnd))
				{
					wnd = controlView->GetCurrentControlDlg();
				}
			}

			// Backup solution for order navigation if the currently active view is not a pattern view, but a module is playing
			// Note: This should also work if the currently active window is not a ChildFrameBase, hence it happens before the GetActiveWindow() check.
			if(mpt::is_in_range(cmd, kcPrevNextOrderStart, kcPrevNextOrderEnd)
				&& m_pSndFile && m_pSndFile->GetpModDoc()
				&& wnd != nullptr
				&& dynamic_cast<CViewPattern *>(wnd) == nullptr)
			{
				TrackerCriticalSection cs;

				ORDERINDEX order = m_pSndFile->m_PlayState.m_nCurrentOrder;
				if(cmd == kcPrevOrder || cmd == kcPrevOrderAtMeasureEnd || cmd == kcPrevOrderAtBeatEnd || cmd == kcPrevOrderAtRowEnd)
					order = m_pSndFile->Order().GetPreviousOrderIgnoringSkips(order);
				else
					order = m_pSndFile->Order().GetNextOrderIgnoringSkips(order);
				
				if(order == m_pSndFile->m_PlayState.m_nCurrentOrder)
					return wParam;

				ResetNotificationBuffer();
				switch(wParam)
				{
				case kcPrevOrder:
				case kcNextOrder:
					m_pSndFile->GetpModDoc()->SetElapsedTime(order, 0, !m_pSndFile->m_PlayState.m_flags[SONG_PAUSED | SONG_STEP]);
					break;
				case kcPrevOrderAtMeasureEnd:
				case kcNextOrderAtMeasureEnd:
					m_pSndFile->m_PlayState.m_seqOverrideMode = OrderTransitionMode::AtMeasureEnd;
					m_pSndFile->m_PlayState.m_nSeqOverride = order;
					break;
				case kcPrevOrderAtBeatEnd:
				case kcNextOrderAtBeatEnd:
					m_pSndFile->m_PlayState.m_seqOverrideMode = OrderTransitionMode::AtBeatEnd;
					m_pSndFile->m_PlayState.m_nSeqOverride = order;
					break;
				case kcPrevOrderAtRowEnd:
				case kcNextOrderAtRowEnd:
					m_pSndFile->m_PlayState.m_seqOverrideMode = OrderTransitionMode::AtRowEnd;
					m_pSndFile->m_PlayState.m_nSeqOverride = order;
					break;
				}
				return wParam;
			}

			if(wnd && Fl::first_window() == GetWidget())
				return wnd->SendMessage(MSG_MOD_KEYCOMMAND, wParam, lParam);
			return kcNull;
	}

	return wParam;
}


void CMainFrame::InitRenderer(CTrackerSoundFile *pSndFile)
{
	TrackerCriticalSection cs;
	pSndFile->m_bIsRendering = true;
	pSndFile->SuspendPlugins();
	pSndFile->ResumePlugins();
}


void CMainFrame::StopRenderer(CTrackerSoundFile *pSndFile)
{
	TrackerCriticalSection cs;
	pSndFile->SuspendPlugins();
	pSndFile->m_bIsRendering = false;
}


CInputHandler *CMainFrame::GetInputHandler()
{
	if(CMainFrame *mainFrm = GetMainFrame())
		return mainFrm->m_InputHandler.get();
	return nullptr;
}


// We have switched focus to a new module - might need to update effect keys to reflect module type
bool CMainFrame::UpdateEffectKeys(const CModDoc *modDoc)
{
	if(modDoc != nullptr)
	{
		return m_InputHandler->SetEffectLetters(modDoc->GetSoundFile().GetModSpecifications());
	}
	return false;
}


void CMainFrame::OnShowWindow(bool bShow, uint32 /*nStatus*/)
{
	static bool firstShow = true;
	if(bShow && !IsWindowVisible() && firstShow)
	{
		firstShow = false;
		WINDOWPLACEMENT wpl;
		GetWindowPlacement(&wpl);
		wpl = theApp.GetSettings().Read<WINDOWPLACEMENT>(UL_("Display"), UL_("WindowPlacement"), wpl);
		SetWindowPlacement(&wpl);
	}
}


void CMainFrame::OnUpdateAvailable()
{
}


void CMainFrame::OnShowSettingsFolder()
{
	theApp.OpenDirectory(theApp.GetConfigPath());
}



class CUpdateCheckProgressDialog
	: public CProgressDialog
{
public:
	CUpdateCheckProgressDialog(Wnd *parent)
		: CProgressDialog(parent)
	{
		return;
	}
	void Run() override
	{
	}
};

static std::unique_ptr<CUpdateCheckProgressDialog> g_UpdateCheckProgressDialog = nullptr;




void CMainFrame::OnHelp()
{
	View *view = GetActiveView();
	const char *page = "";
	if(m_bOptionsLocked)
	{
		switch(m_nLastOptionsPage)
		{
			case OPTIONS_PAGE_GENERAL:		page = "::/Setup_General.html"; break;
			case OPTIONS_PAGE_SOUNDCARD:	page = "::/Setup_Soundcard.html"; break;
			case OPTIONS_PAGE_MIXER:		page = "::/Setup_Mixer.html"; break;
			case OPTIONS_PAGE_PLAYER:		page = "::/Setup_DSP.html"; break;
			case OPTIONS_PAGE_SAMPLEDITOR:	page = "::/Setup_Samples.html"; break;
			case OPTIONS_PAGE_KEYBOARD:		page = "::/Setup_Keyboard.html"; break;
			case OPTIONS_PAGE_COLORS:		page = "::/Setup_Display.html"; break;
			case OPTIONS_PAGE_MIDI:			page = "::/Setup_MIDI.html"; break;
			case OPTIONS_PAGE_PATHS:		page = "::/Setup_Paths_Auto_Save.html"; break;
			case OPTIONS_PAGE_UPDATE:		page = "::/Setup_Update.html"; break;
			case OPTIONS_PAGE_ADVANCED:		page = "::/Setup_Advanced.html"; break;
			case OPTIONS_PAGE_WINE:			page = "::/Setup_Wine.html"; break;
		}
	} else if(view != nullptr)
	{
		if(dynamic_cast<CViewGlobals *>(view))
			page = "::/General.html";
		else if(dynamic_cast<CViewPattern *>(view))
			page = "::/Patterns.html";
		else if(dynamic_cast<CViewSample *>(view))
			page = "::/Samples.html";
		else if(dynamic_cast<CViewInstrument *>(view))
			page = "::/Instruments.html";
		else if(dynamic_cast<CViewComments *>(view))
			page = "::/Comments.html";
		else if(auto *controlView = dynamic_cast<CModControlView *>(view))
		{
			switch(controlView->GetActivePage())
			{
				case CModControlView::Page::Globals: page = "::/General.html"; break;
				case CModControlView::Page::Patterns: page = "::/Patterns.html"; break;
				case CModControlView::Page::Samples: page = "::/Samples.html"; break;
				case CModControlView::Page::Instruments: page = "::/Instruments.html"; break;
				case CModControlView::Page::Unknown: /* nothing */ break;
				case CModControlView::Page::NumPages: /* nothing */ break;
			}
		}
	}

	// Manual pages are named like "::/Setup_Keyboard.html"
	mpt::ustring pageName = mpt::transcode<mpt::ustring>(mpt::common_encoding::utf8, std::string(page));
	if(pageName.empty())
		pageName = UL_("::/Table_of_Contents.html");
	pageName = mpt::replace(pageName, mpt::ustring(UL_("::/")), mpt::ustring());
	pageName = mpt::replace(pageName, mpt::ustring(UL_(".html")), mpt::ustring());
	if(!CTrackApp::OpenURL(UL_("https://wiki.openmpt.org/Manual:_") + pageName))
	{
		Reporting::Error(UL_("Could not open the online manual."));
	}
}


LResult CMainFrame::OnViewMIDIMapping(WParam wParam, LParam lParam)
{
	static bool inMapper = false;
	if(!inMapper)
	{
		inMapper = true;
		CModDoc *doc = GetActiveDoc();
		if(doc != nullptr)
			doc->ViewMIDIMapping(static_cast<PLUGINDEX>(wParam), static_cast<PlugParamIndex>(lParam));
		inMapper = false;
	}
	return 0;
}


void CMainFrame::LoadMetronomeSamples()
{
	const std::tuple<mpt::PathString, ModSample &, const int, const int> metronomeSamples[] =
	{
		{TrackerSettings::Instance().metronomeSampleMeasure, m_metronomeMeasure, 4, 256},
		{TrackerSettings::Instance().metronomeSampleBeat, m_metronomeBeat, 2, 192},
	};
	TrackerCriticalSection cs;
	for(auto &[path, sample, speed, amp] : metronomeSamples)
	{
		sample.FreeSample();
		if(path.empty())
			continue;
		if(path != TrackerSettings::GetDefaultMetronomeSample())
		{
			mpt::IO::InputFile inputFile(path, TrackerSettings::Instance().MiscCacheCompleteFileBeforeLoading);
			if(inputFile.IsValid())
			{
				FileReader file = GetFileReader(inputFile);
				SAMPLEINDEX srcSmp = m_WaveFile.GetNextFreeSample();
				if(srcSmp == SAMPLEINDEX_INVALID)
					srcSmp = 1;
				if(m_WaveFile.ReadSampleFromFile(srcSmp, file))
				{
					std::swap(sample, m_WaveFile.GetSample(srcSmp));
					sample.Convert(m_WaveFile.GetType(), MOD_TYPE_MPT);
				}
			}
		}
		if(!sample.HasSampleData())
		{
			sample.Initialize(MOD_TYPE_MPT);
			sample.nC5Speed = 8363 * 16;
			sample.nLength = 4096;
			sample.uFlags.set(CHN_16BIT);
			if(sample.AllocateSample())
			{
				int16_t *sampleData = sample.sample16();
				for(SmpLength i = 0; i < sample.nLength; i++)
				{
					sampleData[i] = static_cast<int16>(Util::muldiv(ITSinusTable[(i * speed) % std::size(ITSinusTable)] * amp, 4096 * 4096 - i * i, 4096 * 4096));
				}
			}
		}
	}
	UpdateMetronomeVolume();
	UpdateMetronomeSamples();
}


void CMainFrame::UpdateMetronomeSamples()
{
	if(!m_pSndFile)
		return;
	ModSample *measure = nullptr, *beat = nullptr;
	if(TrackerSettings::Instance().metronomeEnabled)
	{
		measure = &m_metronomeMeasure;
		beat = &m_metronomeBeat;
	}
	TrackerCriticalSection cs;
	m_pSndFile->SetMetronomeSamples(measure, beat);
}


void CMainFrame::UpdateMetronomeVolume()
{
	const float linear = CModDoc::DecibelsToLinear(TrackerSettings::Instance().metronomeVolume, 256.0f);
	const uint16 volume = std::clamp(mpt::saturate_round<uint16>(linear), uint16(0), uint16(256));
	TrackerCriticalSection cs;
	m_metronomeBeat.nVolume = m_metronomeMeasure.nVolume = volume;
}


Menu CMainFrame::CreateFileMenu(const size_t maxCount, std::vector<mpt::PathString>& paths, const mpt::PathString &folderName, const uint16 idRangeBegin)
{
	paths.clear();

	for(size_t i = 0; i < 2; i++)  // 0: app items, 1: user items
	{
		// To avoid duplicates, check whether app path and config path are the same.
		if(i == 1 && mpt::PathCompareNoCase(theApp.GetInstallPath(), theApp.GetConfigPath()) == 0)
			break;

		mpt::PathString basePath;
		basePath = (i == 0) ? theApp.GetInstallPath() : theApp.GetConfigPath();
		basePath += folderName;
		if(!FileSystem::IsDirectory(basePath))
			continue;

		FolderScanner scanner(basePath, FolderScanner::kOnlyFiles);
		mpt::PathString fileName;
		while(scanner.Next(fileName))
		{
			paths.push_back(std::move(fileName));
		}
	}
	std::sort(paths.begin(), paths.end(), [](const mpt::PathString &left, const mpt::PathString &right)
	{
		return Util::IsNaturalLess(left.ToUnicode(), right.ToUnicode());
	});

	Menu menu;
	uintptr_t filesAdded = 0;
	for(const auto &fileName : paths)
	{
		mpt::ustring file = fileName.GetFilename().ToUnicode();
		file = mpt::replace(file, mpt::ustring(UL_("&")), mpt::ustring(UL_("&&")));
		menu.AppendMenu(ui::MenuItemString, static_cast<uint32>(idRangeBegin + filesAdded), file);
		filesAdded++;
		if(filesAdded >= maxCount)
			break;
	}

	if(filesAdded == 0)
	{
		menu.AppendMenu(ui::MenuItemString | ui::MenuItemGrayed, 0, UL_("No items found"));
	} else
	{
		menu.AppendMenu(ui::MenuItemSeparator);
		menu.AppendMenu(ui::MenuItemString, static_cast<uint32>(idRangeBegin + maxCount), UL_("&Browse..."));
	}

	return menu;
}


void CMainFrame::CreateExampleModulesMenu()
{
	static_assert(nMaxItemsInExampleModulesMenu == ID_EXAMPLE_MODULES_LASTINRANGE - ID_EXAMPLE_MODULES,
				  "Make sure that there's a proper range for menu commands in resources.");
	const Menu menu = CreateFileMenu(nMaxItemsInExampleModulesMenu, m_ExampleModulePaths, P_("ExampleSongs/"), ID_EXAMPLE_MODULES);
	auto [parentMenu, position] = FindMenuItemByCommand(*GetMenu(), ID_EXAMPLE_MODULES);
	if(parentMenu)
	{
		parentMenu->RemoveMenu(position, true);
		parentMenu->InsertMenu(position, ui::MenuItemPopup, menu, m_InputHandler->GetMenuText(ID_EXAMPLE_MODULES));
		SetMenu(*GetMenu());
	} else
		MPT_ASSERT_NOTREACHED();
}


std::pair<Menu *, int> CMainFrame::FindMenuItemByCommand(Menu &menu, uint32 commandID)
{
	const int numItems = menu.GetMenuItemCount();
	for(int item = 0; item < numItems; item++)
	{
		if(menu.GetMenuItemID(item) == commandID)
			return {&menu, item};
		Menu *subMenu = menu.GetSubMenu(item);
		if(subMenu != nullptr)
		{
			if(auto result = FindMenuItemByCommand(*subMenu, commandID); result.first != nullptr)
				return result;
		}
	}
	return {};
}


void CMainFrame::CreateTemplateModulesMenu()
{
	static_assert(nMaxItemsInTemplateModulesMenu == ID_FILE_OPENTEMPLATE_LASTINRANGE - ID_FILE_OPENTEMPLATE,
				  "Make sure that there's a proper range for menu commands in resources.");
	const Menu menu = CreateFileMenu(nMaxItemsInTemplateModulesMenu, m_TemplateModulePaths, P_("TemplateModules/"), ID_FILE_OPENTEMPLATE);
	auto [fileMenu, position] = FindMenuItemByCommand(*GetMenu(), ID_FILE_OPEN);
	if(fileMenu)
	{
		MPT_VERIFY(fileMenu->RemoveMenu(position + 1, true));
		MPT_VERIFY(fileMenu->InsertMenu(position + 1, ui::MenuItemPopup, menu, m_InputHandler->GetMenuText(ID_FILE_OPENTEMPLATE)));
		SetMenu(*GetMenu());
	}
	else
		MPT_ASSERT_NOTREACHED();
}


void CMainFrame::UpdateMRUList()
{
	const auto [pMenu, firstMenu] = FindMenuItemByCommand(*GetMenu(), ID_MRU_LIST_FIRST);
	MPT_ASSERT(pMenu);
	if(!pMenu)
		return;

	for(int i = ID_MRU_LIST_FIRST; i <= ID_MRU_LIST_LAST; i++)
	{
		pMenu->DeleteMenu(i, false);
	}

	if(TrackerSettings::Instance().mruFiles.empty())
	{
		// MFC will automatically ignore if we set ui::MenuItemGrayed here because of CFrameWnd::m_bAutoMenuEnable.
		// So we will have to install a ON_UPDATE_COMMAND_UI callback...
		pMenu->InsertMenu(firstMenu, ui::MenuItemString | ui::MenuItemGrayed, ID_MRU_LIST_FIRST, UL_("Recent File"));
	} else
	{
		const mpt::PathString workDir = TrackerSettings::Instance().PathSongs.GetWorkingDir();
		const int entries = mpt::saturate_cast<int>(TrackerSettings::Instance().mruFiles.size());
		for(int i = 0; i < entries; i++)
		{
			mpt::ustring s = mpt::ufmt::val(i + 1) + UL_(" ");
			// Add mnemonics
			if(i < 9)
			{
				s = UL_("&") + s;
			} else if(i == 9)
			{
				s = UL_("1&0 ");
			}

			const mpt::PathString &pathMPT = TrackerSettings::Instance().mruFiles[i];
			mpt::ustring path = pathMPT.ToUnicode();
			if(!mpt::PathCompareNoCase(workDir, pathMPT.GetDirectoryWithDrive()))
			{
				// Only show filename
				path = path.substr(workDir.ToUnicode().length());
			} else if(path.length() > 30)	// Magic number experimentally determined to be equal to MFC's behaviour
			{
				// Shorten path ("C:\Foo\VeryLongString...\Bar.it" => "C:\Foo\...\Bar.it")
				size_t start = path.find_first_of(UL_("\\/"), path.find_first_of(UL_("\\/")) + 1);
				size_t end = path.find_last_of(UL_("\\/"));
				if(start < end)
				{
					path = path.substr(0, start + 1) + UL_("...") + path.substr(end);
				}
			}
			path = mpt::replace(path, mpt::ustring(UL_("&")), mpt::ustring(UL_("&&")));
			s += path;
			pMenu->InsertMenu(firstMenu + i, ui::MenuItemString, ID_MRU_LIST_FIRST + i, s);
		}
	}
	SetMenu(*GetMenu());
}


void CMainFrame::UpdateDocumentCount()
{
	const bool isLoaded = m_quickStartDlg != nullptr;
	const bool shouldLoad = !theApp.GetOpenDocumentCount();
	if(shouldLoad && !isLoaded)
	{
		m_quickStartDlg = std::make_unique<QuickStartDlg>(m_TemplateModulePaths, m_ExampleModulePaths, this);
		m_quickStartDlg->CenterWindow(this);
		m_quickStartDlg->ShowWindow(true);
	} else if(isLoaded && !shouldLoad)
	{
		// Documents are opened from the dialog's own list callbacks, which still use it after this returns
		if(Fl_Window *frame = m_quickStartDlg->GetFrameWindow())
			frame->hide();
		Fl::delete_widget(m_quickStartDlg.release());
	}
}


#ifdef MPT_ENABLE_PLAYBACK_TEST_MENU
void CMainFrame::OnCreateMixerDump()
{
	std::string exts;
	for(const auto &ext : CSoundFile::GetSupportedExtensions(true))
	{
		exts += std::string("*.") + ext + std::string(";");
	}

	FileDialog dlg = OpenFileDialog()
		.AllowMultiSelect()
		.ExtensionFilter("Module Files (*.it,*.xm,...)|" + exts + "|All files(*.*)|*.*||");
	if(!dlg.Show(this))
		return;
	for(const auto &fileName : dlg.GetFilenames())
	{
		MPT_LOG_GLOBAL(LogDebug, "info", UL_("Loading ") + fileName.ToUnicode());
		auto sndFile = std::make_unique<CTrackerSoundFile>();
		mpt::IO::InputFile f(fileName);
		if(!f.IsValid())
			continue;
		if(!sndFile->Create(GetFileReader(f)))
			continue;
		auto playTest = sndFile->CreatePlaybackTest(PlaybackTestSettings{});
		mpt::IO::ofstream outFile(fileName + P_(".testdata.gz"), std::ios::binary | std::ios::trunc);
		if(outFile)
		{
			std::ostringstream outStream;
			playTest.Serialize(outStream);
			#ifdef MPT_WITH_ZLIB
				std::string outData = std::move(outStream).str();
				WriteGzip(outFile, outData, fileName.GetFilename().ToUnicode() + UL_(".testdata"));
			#else
				// miniz doesn't have gzip convenience functions
				outFile << std::move(outStream).str();
			#endif
		}
	}
}


void CMainFrame::OnVerifyMixerDump()
{
	FileDialog dlg = OpenFileDialog()
		.AllowMultiSelect()
		.ExtensionFilter("Test Data|*.testdata;*.testdata.gz|All files(*.*)|*.*||");
	if(!dlg.Show(this))
		return;
	for(const auto &fileName : dlg.GetFilenames())
	{
		MPT_LOG_GLOBAL(LogDebug, "info", UL_("Loading ") + fileName.ToUnicode());
		try
		{
			auto modFileName = fileName;
			if(!mpt::PathCompareNoCase(modFileName.GetFilenameExtension(), P_(".gz")))
				modFileName = modFileName.ReplaceExtension({});
			if(!mpt::PathCompareNoCase(modFileName.GetFilenameExtension(), P_(".testdata")))
				modFileName = modFileName.ReplaceExtension({});

			mpt::IO::InputFile testFile{fileName};
			if(!testFile.IsValid())
				throw std::runtime_error{"Cannot open test data file: " + fileName.ToUTF8()};

			mpt::IO::InputFile modFile{modFileName};
			if(!modFile.IsValid())
				throw std::runtime_error{"Cannot open module data file: " + modFileName.ToUTF8()};

			FileReader testFileReader = GetFileReader(testFile);
			CGzipArchive archive{testFileReader};
			if(archive.IsArchive())
			{
				if(!archive.ExtractFile(0))
					throw std::runtime_error{"Cannot extract test data file!"};
				testFileReader = archive.GetOutputFile();
			}

			PlaybackTest playTest{testFileReader};
			auto sndFile = std::make_unique<CTrackerSoundFile>();
			sndFile->Create(GetFileReader(modFile));

			const auto result = PlaybackTest::Compare(playTest, sndFile->CreatePlaybackTest(playTest.GetSettings()));
			if(!result.empty())
			{
				InfoDialog infoDlg{this};
				infoDlg.SetCaption(UL_("Test results for ") + fileName.ToUnicode());
				mpt::ustring content;
				for(const auto &line : result)
				{
					content += mpt::ToUnicode(line) + UL_("\r\n");
				}
				infoDlg.SetContent(content);
				infoDlg.DoModal();
			}
		} catch(const std::exception &e)
		{
			Reporting::Error(MPT_UFORMAT("Cannot convert {}: {}")(fileName.ToUnicode(), mpt::ToUnicode(mpt::Charset::UTF8, e.what())), this);
		}
	}
}


void CMainFrame::OnConvertMixerDumpToText()
{
	FileDialog dlg = OpenFileDialog()
		.AllowMultiSelect()
		.ExtensionFilter("Test Data|*.testdata;*.testdata.gz|All files(*.*)|*.*||");
	if(!dlg.Show(this))
		return;
	for(const auto &fileName : dlg.GetFilenames())
	{
		MPT_LOG_GLOBAL(LogDebug, "info", UL_("Loading ") + fileName.ToUnicode());
		try
		{
			mpt::IO::InputFile testFile{fileName};
			if(!testFile.IsValid())
				throw std::runtime_error{"Cannot open test data file: " + fileName.ToUTF8()};

			FileReader testFileReader = GetFileReader(testFile);
			CGzipArchive archive{testFileReader};
			if(archive.IsArchive())
			{
				if(!archive.ExtractFile(0))
					throw std::runtime_error{"Cannot extract test data file!"};
				testFileReader = archive.GetOutputFile();
			}

			PlaybackTest playTest{testFileReader};
			mpt::IO::ofstream output(fileName.ReplaceExtension(P_(".tsv")), std::ios::binary);
			playTest.ToTSV(output);
		} catch(const std::exception &e)
		{
			Reporting::Error(MPT_UFORMAT("Cannot convert {}: {}")(fileName.ToUnicode(), mpt::ToUnicode(mpt::Charset::UTF8, e.what())), this);
		}
	}
}

#endif  // ENABLE_PLAYBACK_TEST_MENU


void CMainFrame::NotifyAccessibilityUpdate(Wnd &)
{
}


OPENMPT_NAMESPACE_END
