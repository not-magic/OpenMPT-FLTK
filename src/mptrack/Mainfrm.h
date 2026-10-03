// FLTK port of openmpt/mptrack/Mainfrm.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"
#include "CImageListEx.h"
#include "Mainbar.h"
#include "Notification.h"
#include "openmpt/soundbase/Dither.hpp"
#include "Settings.h"
#include "openmpt_ext/sndlib/TrackerCriticalSection.h"
#include "../soundlib/AudioReadTarget.h"
#include "../soundlib/Sndfile.h"
#include "mpt/audio/span.hpp"
#include "mpt/mutex/mutex.hpp"
#include "openmpt/sounddevice/SoundDevice.hpp"
#include "openmpt/sounddevice/SoundDeviceBuffer.hpp"
#include "openmpt_ext/misc/mptClockExt.h"


#include <functional>

class RtMidiIn;

OPENMPT_NAMESPACE_BEGIN

class CAutoSaver;
class CDLSBank;
class CInputHandler;
class CModDoc;
class QuickStartDlg;
struct UpdateCheckResult;
struct UpdateHint;
struct MODPLUGDIB;
enum class MidiSetup: int32;
enum class MainToolBarItem : uint32;
enum SoundDeviceStopMode : int;
namespace SoundDevice {
class Base;
class ICallback;
} // namerspace SoundDevice

#define MAINFRAME_TITLE UL_("Open ModPlug Tracker")


#define NUM_VUMETER_PENS		32


// Tab Order
enum OptionsPage
{
	OPTIONS_PAGE_DEFAULT = 0,
	OPTIONS_PAGE_GENERAL = OPTIONS_PAGE_DEFAULT,
	OPTIONS_PAGE_SOUNDCARD,
	OPTIONS_PAGE_MIXER,
	OPTIONS_PAGE_PLAYER,
	OPTIONS_PAGE_SAMPLEDITOR,
	OPTIONS_PAGE_KEYBOARD,
	OPTIONS_PAGE_COLORS,
	OPTIONS_PAGE_MIDI,
	OPTIONS_PAGE_PATHS,
	OPTIONS_PAGE_UPDATE,
	OPTIONS_PAGE_ADVANCED,
	OPTIONS_PAGE_WINE,
};


/////////////////////////////////////////////////////////////////////////
// Player position notification

#define MAX_UPDATE_HISTORY		2000 // 2 seconds with 1 ms updates

template<> inline SettingValue ToSettingValue(const ui::WindowPlacement &val)
{
	return SettingValue(EncodeBinarySetting<ui::WindowPlacement>(val), "WindowPlacement");
}
template<> inline ui::WindowPlacement FromSettingValue(const SettingValue &val)
{
	MPT_ASSERT(val.GetTypeTag() == "WindowPlacement");
	return DecodeBinarySetting<ui::WindowPlacement>(val.as<std::vector<std::byte> >());
}


class VUMeter
	: public IMonitorInput
	, public IMonitorOutput
{
public:
	static constexpr std::size_t maxChannels = 4;
	static const float dynamicRange; // corresponds to the current implementation of the UI widget diplaying the result
	struct Channel
	{
		int32 peak = 0;
		bool clipped = false;
	};
private:
	Channel channels[maxChannels];
	int32 decayParam;
	void Process(Channel &channel, MixSampleInt sample);
	void Process(Channel &channel, MixSampleFloat sample);
public:
	VUMeter() : decayParam(0) { SetDecaySpeedDecibelPerSecond(88.0f); }
	void SetDecaySpeedDecibelPerSecond(float decibelPerSecond);
public:
	const Channel & operator [] (std::size_t channel) const { return channels[channel]; }
	void Process(mpt::audio_span_interleaved<const MixSampleInt> buffer);
	void Process(mpt::audio_span_planar<const MixSampleInt> buffer);
	void Process(mpt::audio_span_interleaved<const MixSampleFloat> buffer);
	void Process(mpt::audio_span_planar<const MixSampleFloat> buffer);
	void Decay(int32 secondsNum, int32 secondsDen);
	void ResetClipped();
};


class CMainFrame
	: public MainFrameBase
	, public SoundDevice::CallbackBufferHandler<DithersOpenMPT>
	, public SoundDevice::IMessageReceiver
{
	// static data
public:

	// Globals
	static OptionsPage m_nLastOptionsPage;

	// Drawing resources
	ui::Font m_hCommentsFont;
	static ui::Font m_hGUIFont;
	static ColorRef penDarkGray, penHalfDarkGray, penGray99;
	static constexpr Fl_Cursor curDragging = FL_CURSOR_HAND;
	static constexpr Fl_Cursor curNoDrop = FL_CURSOR_CROSS;
	static constexpr Fl_Cursor curArrow = FL_CURSOR_ARROW;
	static constexpr Fl_Cursor curNoDrop2 = FL_CURSOR_CROSS;
	static constexpr Fl_Cursor curVSplit = FL_CURSOR_NS;
	static std::unique_ptr<MODPLUGDIB> bmpNotes;
	static ColorRef gcolrefVuMeter[NUM_VUMETER_PENS * 2];	// General tab VU meters

public:

	// Low-Level Audio
	TrackerCriticalSection m_SoundDeviceFillBufferCriticalSection;
	Util::MultimediaClock m_SoundDeviceClock;
	SoundDevice::IBase *gpSoundDevice = nullptr;
	uintptr_t m_NotifyTimer = 0;
	VUMeter m_VUMeterInput;
	VUMeter m_VUMeterOutput;

	uint32 m_AudioThreadId = 0;
	bool m_InNotifyHandler = false;

	// Midi Input
public:
	struct MidiInData
	{
		std::unique_ptr<RtMidiIn> input;
		mpt::mutex dataMutex;
		bool isOpen = false;
	};

	MidiInData midiInData;

public:
	CImageListEx m_MiscIcons, m_MiscIconsDisabled;			// Misc Icons
	CImageListEx m_PatternIcons, m_PatternIconsDisabled;	// Pattern icons (includes some from sample editor as well...)
	CImageListEx m_EnvelopeIcons;							// Instrument editor icons
	CImageListEx m_SampleIcons;								// Sample editor icons

protected:
	CModTreeBar m_wndTree;
	ui::StatusBar m_wndStatusBar;
	CMainToolBar m_wndToolBar;
	CTrackerSoundFile *m_pSndFile = nullptr; // != NULL only when currently playing or rendering
	WindowHandle m_hWndMidi = nullptr;
	std::function<void(mpt::const_byte_span)> m_midiSysExCallback;
	samplecount_t m_dwTimeSec = 0;
	uintptr_t m_nTimer = 0;
	uint32 m_nAvgMixChn = 0, m_nMixChn = 0;
	uint32 m_currentSpeed = 0;
	// Misc
	class COptionsSoundcard *m_SoundCardOptionsDialog = nullptr;
	Fl_Widget *m_previousFocusWidget = nullptr;
	bool m_bOptionsLocked = false;

	// Notification Buffer
	mpt::mutex m_NotificationBufferMutex; // to avoid deadlocks, this mutex should only be taken as a innermost lock, i.e. do not block on anything while holding this mutex
	Util::fixed_size_queue<Notification, MAX_UPDATE_HISTORY> m_NotifyBuffer;

	// Instrument preview in tree view
	CTrackerSoundFile m_WaveFile;
	ModSample m_metronomeMeasure{}, m_metronomeBeat{};

	mpt::ustring m_userText, m_infoText, m_xInfoText;

	mpt::heap_value<CAutoSaver> m_AutoSaver;
	mpt::heap_value<CInputHandler> m_InputHandler;

public:
	bool m_bModTreeHasFocus = false;

public:
	CMainFrame();
	// Creates the child windows and loads the resources
	bool OnCreate();
	void Initialize();


// Low-Level Audio
public:
	static void UpdateDspEffects(CTrackerSoundFile &sndFile, bool reset=false);
	static void UpdateAudioParameters(CTrackerSoundFile &sndFile, bool reset=false);

	// from SoundDevice::IBufferHandler
	uint64 SoundCallbackGetReferenceClockNowNanoseconds() const override;
	void SoundCallbackPreStart() override;
	void SoundCallbackPostStop() override;
	bool SoundCallbackIsLockedByCurrentThread() const override;
	void SoundCallbackLock() override;
	uint64 SoundCallbackLockedGetReferenceClockNowNanoseconds() const override;
	void SoundCallbackLockedProcessPrepare(SoundDevice::TimeInfo timeInfo) override;
	void SoundCallbackLockedCallback(SoundDevice::CallbackBuffer<DithersOpenMPT> &buffer) override;
	void SoundCallbackLockedProcessDone(SoundDevice::TimeInfo timeInfo) override;
	void SoundCallbackUnlock() override;

	// from SoundDevice::IMessageReceiver
	void SoundDeviceMessage(LogLevel level, const mpt::ustring &str) override;

	bool InGuiThread() const noexcept;
	bool InAudioThread() const noexcept { return mpt::log::Trace::GetCurrentThreadId() == m_AudioThreadId; }
	bool InNotifyHandler() const noexcept { return m_InNotifyHandler; }

	bool audioOpenDevice();
	void audioCloseDevice();
	bool IsAudioDeviceOpen() const;
	bool DoNotification(uint32 dwSamplesRead, int64 streamPosition);

// Midi Input Functions
public:
	bool midiOpenDevice(bool showSettings = true);
	void midiCloseDevice();
	void SetMidiRecordWnd(WindowHandle hwnd, std::function<void(mpt::const_byte_span)> sysExCallback = {})
	{
		m_hWndMidi = hwnd;
		m_midiSysExCallback = std::move(sysExCallback);
	}
	WindowHandle GetMidiRecordWnd() const { return m_hWndMidi; }
	auto GetMidiSysexCallback() const { return m_midiSysExCallback; }
	void LoadMetronomeSamples();
	void UpdateMetronomeSamples();
	void UpdateMetronomeVolume();

	static int ApplyVolumeRelatedSettings(const uint32 &dwParam1, const uint8 midivolume);

// static functions
public:
	static CMainFrame *GetMainFrame() noexcept;
	static void UpdateColors();
	static ui::Font GetGUIFont() { return m_hGUIFont; }
	// Names of the available MIDI input devices
	static std::vector<mpt::ustring> GetMidiInputDeviceNames();

	// Misc functions
public:
	ui::Font &GetCommentsFont() { return m_hCommentsFont; }

	void SetUserText(const mpt::ustring &text);
	void SetInfoText(const mpt::ustring &text);
	void SetXInfoText(const mpt::ustring &text);
	void SetHelpText(const mpt::ustring &text);
	mpt::ustring GetHelpText() const;
	uint32 GetBaseOctave() const;
	CModDoc *GetActiveDoc() const;
	View *GetActiveView() const;
	void OnDocumentCreated(CModDoc *pModDoc);
	void OnDocumentClosed(CModDoc *pModDoc);
	void UpdateTree(CModDoc *pModDoc, UpdateHint hint, HintObject *pHint = nullptr);
	void RefreshDlsBanks();
	static CInputHandler *GetInputHandler();
	void SetElapsedTime(double t) { m_dwTimeSec = mpt::saturate_trunc<samplecount_t>(t * 10.0); }


	CModTree *GetUpperTreeview() { return m_wndTree.m_pModTree; }
	CModTree *GetLowerTreeview() { return m_wndTree.m_pModTreeData; }
	bool SetTreeSoundfile(FileReader &file) { return m_wndTree.SetTreeSoundfile(file); }

	void CreateExampleModulesMenu();
	void CreateTemplateModulesMenu();
	static std::pair<Menu *, int> FindMenuItemByCommand(Menu &menu, uint32 commandID);

	// Creates submenu whose items are filenames of files in both
	// AppDirectory\folderName\ (usually C:\Program Files\OpenMPT\folderName\)
	// and
	// ConfigDirectory\folderName (usually %appdata%\OpenMPT\folderName\)
	// [in] maxCount: Maximum number of items allowed in the menu
	// [out] paths: Receives the full paths of the files added to the menu.
	// [in] folderName: Name of the folder
	// [in] idRangeBegin: First ID for the menu item.
	static Menu CreateFileMenu(const size_t maxCount, std::vector<mpt::PathString> &paths, const mpt::PathString &folderName, const uint16 idRangeBegin);

// Player functions
public:

	// High-level synchronous playback functions, do not hold AudioCriticalSection while calling these
	bool PreparePlayback();
	bool StartPlayback();
	void StopPlayback();
	bool RestartPlayback();
	bool PausePlayback();
	static bool IsValidSoundFile(CTrackerSoundFile &sndFile) { return sndFile.GetType() != MOD_TYPE_NONE; }
	static bool IsValidSoundFile(CTrackerSoundFile *pSndFile) { return pSndFile && pSndFile->GetType(); }
	void SetPlaybackSoundFile(CTrackerSoundFile *pSndFile);
	void UnsetPlaybackSoundFile();
	void GenerateStopNotification();

	bool PlayMod(CModDoc *);
	bool StopMod(CModDoc *pDoc = nullptr);
	bool PauseMod(CModDoc *pDoc = nullptr);

	bool StopSoundFile(CTrackerSoundFile *);
	bool PlaySoundFile(CTrackerSoundFile *);
	bool PlaySoundFile(const mpt::PathString &filename, ModCommand::NOTE note, int volume = -1);
	bool PlaySoundFile(CTrackerSoundFile &sndFile, INSTRUMENTINDEX nInstrument, SAMPLEINDEX nSample, ModCommand::NOTE note, int volume = -1);
	bool PlayDLSInstrument(const CDLSBank &bank, uint32 instr, uint32 region, ModCommand::NOTE note, int volume = -1);

	void InitPreview();
	void PreparePreview(ModCommand::NOTE note, int volume);
	void StopPreview() { StopSoundFile(&m_WaveFile); }
	void PlayPreview() { PlaySoundFile(&m_WaveFile); }

	inline bool IsPlaying() const { return m_pSndFile != nullptr; }
	// Return currently playing module (nullptr if none is playing)
	inline CModDoc *GetModPlaying() const { return m_pSndFile ? m_pSndFile->GetpModDoc() : nullptr; }
	// Return currently playing module (nullptr if none is playing)
	inline CTrackerSoundFile *GetSoundFilePlaying() const { return m_pSndFile; }
	void InitRenderer(CTrackerSoundFile *);
	void StopRenderer(CTrackerSoundFile *);
	void SwitchToActiveView();

	void IdleHandlerSounddevice();

	void ResetSoundCard();
	void SetupSoundCard(SoundDevice::Settings deviceSettings, SoundDevice::Identifier deviceIdentifier, SoundDeviceStopMode stoppedMode, bool forceReset = false);
	void SetupMiscOptions();
	void SetupPlayer();

	void SetupMidi(FlagSet<MidiSetup> d, uint32 n);
	WindowHandle GetFollowSong() const;
	WindowHandle GetFollowSong(const CModDoc *pDoc) const { return (pDoc == GetModPlaying()) ? GetFollowSong() : nullptr; }
	void ResetNotificationBuffer();

	// Notify accessbility software that it should read out updated UI elements
	void NotifyAccessibilityUpdate(Wnd &source);

	void UpdateDocumentCount();

// Overrides
protected:
	bool PreTranslateMessage(int event) override;
	void OnDestroy() override;
	void OnUpdateFrameTitle(bool bAddToTitle) override;

	/// Opens either template or example menu item.
	void OpenMenuItemFile(const uint32 nId, const bool isTemplateFile);

	void ShowToolbarMenu(Point screenPt);
	void AddToolBarMenuEntries(Menu &menu) const;

	void RecreateImageLists();
	void SetupStatusBarSizes();

public:
	void UpdateMRUList();

// Implementation
public:
	~CMainFrame() override;
	void RecalcLayout(bool notify = true) override;

	void OnTimerGUI();
	void OnTimerNotify();

// Message map functions
public:
	void OnAddDlsBank();
	void OnImportMidiLib();
	void OnViewOptions();
	void OnHelp();
protected:
	void OnClose() override;
	void OnTimer(uintptr_t) override;

	void OnBarCheck(uint32 id);
	void OnUpdateControlBarMenu(CmdUI *cmdUI);
	void UpdateLastFocusedItem();

	void OnPluginManager();
	void OnClipboardManager();

	LResult OnViewMIDIMapping(WParam wParam, LParam lParam);
	void OnUpdateTime(CmdUI *pCmdUI);
	void OnUpdateUser(CmdUI *pCmdUI);
	void OnUpdateInfo(CmdUI *pCmdUI);
	void OnUpdateXInfo(CmdUI *pCmdUI);
	void OnUpdateMidiRecord(CmdUI *pCmdUI);
	void OnPlayerPause();
	void OnMidiRecord();
	void OnPrevOctave();
	void OnNextOctave();
	void OnPanic();
	void OnReportBug();
	bool OnInternetLink(uint32 nID);
	LResult OnUpdatePosition(WParam, LParam lParam);
	LResult OnUpdateViews(WParam modDoc, LParam hint);
	LResult OnSetModified(WParam modDoc, LParam);
	void OnOpenTemplateModule(uint32 nId);
	void OnExampleSong(uint32 nId);
	void OnOpenMRUItem(uint32 nId);
	void OnUpdateMRUItem(CmdUI *cmd);
	LResult OnInvalidatePatterns(WParam, LParam);
	LResult OnCustomKeyMsg(WParam, LParam);
	void OnInternetUpdate();
	void OnUpdateAvailable();
	void OnShowSettingsFolder();
	bool CanDropFiles(Point) const override { return true; }
	void OnDropFiles(const std::vector<mpt::PathString> &files) override;

	void OnToggleMainBarShowOctave();
	void OnToggleMainBarShowTempo();
	void OnToggleMainBarShowSpeed();
	void OnToggleMainBarShowRowsPerBeat();
	void OnToggleMainBarShowGlobalVolume();
	void OnToggleMainBarShowVUMeter();
	void OnToggleMainBarShowFileIcons();
	void OnToggleMainBarShowEditIcons();
	void OnToggleMainBarShowPlayIcons();
	void OnToggleMainBarShowMiscIcons();
	void OnToggleMainBarItem(MainToolBarItem item, uint32 menuID);
	void OnToggleTreeViewOnLeft();

	void OnCreateMixerDump();
	void OnVerifyMixerDump();
	void OnConvertMixerDumpToText();

	UI_DECLARE_MESSAGE_MAP()
public:
	bool UpdateEffectKeys(const CModDoc *modDoc);
	void OnShowWindow(bool bShow, uint32 nStatus);

	// Defines maximum number of items in example modules menu.
	static constexpr size_t nMaxItemsInExampleModulesMenu = 50;
	static constexpr size_t nMaxItemsInTemplateModulesMenu = 50;

private:
	/// Array of paths of example modules that are available from help menu.
	std::vector<mpt::PathString> m_ExampleModulePaths;
	/// Array of paths of template modules that are available from file menu.
	std::vector<mpt::PathString> m_TemplateModulePaths;

	std::unique_ptr<QuickStartDlg> m_quickStartDlg;
};


/////////////////////////////////////////////////////////////////////////////



OPENMPT_NAMESPACE_END
