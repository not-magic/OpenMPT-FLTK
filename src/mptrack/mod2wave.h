// FLTK port of openmpt/mptrack/mod2wave.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "DialogBase.h"
#include "ProgressDialog.h"
#include "Settings.h"
#include "openmpt/streamencoder/StreamEncoder.hpp"
#include "StreamEncoderSettings.h"
#include "../soundlib/Snd_defs.h"


OPENMPT_NAMESPACE_BEGIN

class CModDoc;
class CTrackerSoundFile;
struct SubSong;

struct CWaveConvertSettings
{
	std::vector<EncoderFactoryBase*> EncoderFactories;
	std::vector<std::unique_ptr<EncoderSettingsConf>> EncoderSettings;

	Setting<mpt::ustring> EncoderName;
	std::size_t EncoderIndex;

	StoredTags storedTags;
	FileTags Tags;

	int repeatCount = 1;
	ORDERINDEX minOrder = ORDERINDEX_INVALID, maxOrder = ORDERINDEX_INVALID;
	SAMPLEINDEX sampleSlot = 0;

	bool normalize = false;
	bool silencePlugBuffers = false;
	bool outputToSample = false;

	std::size_t FindEncoder(const mpt::ustring &name) const;
	void SelectEncoder(std::size_t index);
	EncoderFactoryBase *GetEncoderFactory() const;
	const Encoder::Traits *GetTraits() const;
	EncoderSettingsConf &GetEncoderSettings() const;
	Encoder::Settings GetEncoderSettingsWithDetails() const;
	CWaveConvertSettings(SettingsContainer &conf, const std::vector<EncoderFactoryBase*> &encFactories);
};


class CWaveConvert : public DialogBase
{
public:
	CWaveConvertSettings m_Settings;
	CTrackerSoundFile &m_SndFile;
	uint64 m_dwSongLimit = 0;
	std::vector<SubSong> m_subSongs;

	bool m_bGivePlugsIdleTime = false;
	bool m_bChannelMode = false;     // Render by channel
	bool m_bInstrumentMode = false;  // Render by instrument

private:
	const Encoder::Traits *encTraits = nullptr;

	ComboBox m_CbnFileType, m_CbnSampleRate, m_CbnChannels, m_CbnDither, m_CbnSampleFormat, m_CbnSampleSlot;
	Spinner m_SpinLoopCount, m_SpinMinOrder, m_SpinMaxOrder, m_SpinSubsongIndex;

	Edit m_EditTitle, m_EditAuthor, m_EditURL, m_EditAlbum, m_EditYear;
	ComboBox m_CbnGenre;
	Edit m_EditGenre;
	size_t m_selectedSong = 0;
	const ORDERINDEX m_nNumOrders;
	bool m_locked = true; 

public:
	CWaveConvert(Wnd *parent, ORDERINDEX minOrder, ORDERINDEX maxOrder, ORDERINDEX numOrders, CModDoc &modDoc, const std::vector<EncoderFactoryBase *> &encFactories);
	~CWaveConvert();

private:
	void FillFileTypes();
	void FillSamplerates();
	void FillChannels();
	void FillFormats();
	void FillDither();
	void FillTags();

	void LoadTags();

	void SaveEncoderSettings();
	void SaveTags();

	void UpdateSubsongName();
	void UpdateDialog();

	bool OnInitDialog() override;
	void DoDataExchange(DataExchange *pDX) override;
	void OnOK() override;

	void OnCheckTimeLimit();
	void OnCheckChannelMode();
	void OnCheckInstrMode();
	void OnFileTypeChanged();
	void OnSamplerateChanged();
	void OnChannelsChanged();
	void OnDitherChanged();
	void OnFormatChanged();
	void OnSubsongChanged();
	void OnPlayerOptions();
	void OnExportModeChanged();
	void OnSampleSlotChanged();

	UI_DECLARE_MESSAGE_MAP()
};


class CDoWaveConvert: public CProgressDialog
{
public:
	uint64 m_dwSongLimit = 0;
	bool m_bGivePlugsIdleTime = false;

public:
	CDoWaveConvert(CTrackerSoundFile &sndFile, std::ostream &f, const mpt::ustring &caption, const CWaveConvertSettings &settings, const SubSong &subSong, Wnd *parent = nullptr)
		: CProgressDialog(parent)
		, m_Settings(settings)
		, m_SndFile(sndFile)
		, fileStream(f)
		, caption(caption)
		, m_subSong(subSong)
	{ }

	void Run() override;

private:
	const CWaveConvertSettings &m_Settings;
	CTrackerSoundFile &m_SndFile;
	std::ostream &fileStream;
	const mpt::ustring &caption;
	const SubSong &m_subSong;
};


OPENMPT_NAMESPACE_END
