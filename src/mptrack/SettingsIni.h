// FLTK port of openmpt/mptrack/SettingsIni.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "Settings.h"

#include "mpt/base/integer.hpp"
#include "mpt/base/span.hpp"
#include "openmpt_ext/common/AtomicFileExt.h"
#include "mpt/string/types.hpp"

#include <list>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include <cstddef>


OPENMPT_NAMESPACE_BEGIN


#define MPT_SETTINGS_INI_CASE_INSENSITIVE 1


// Version 1:
//  *  encoding: ANSI

// Version 2:
//  *  encoding: UTF-16LE with BOM

// Version 3:
//  *  encoding: UTF-8 with BOM (Wine-only, not supported by Windows)

// Version 1 / 2 / 3:
//  *  no header
//  *  no escaping
//  *  string quoting with " or ' to support whitespace
//  *  case-insensitive

// Version 4:
//  *  encoding: any Unicode encoding with BOM, ANSI/Locale without BOM
//  *  header:
//      * [!Type]
//        !Format=org.openmpt.fileformat.ini
//        !VersionMajor=4
//        !VersionMinor=0
//        !VersionPatch=0
//  *  escaping:
//      *  ^xff / ^uffff / ^Uffffffff
//         Unicode codepoint ([0x00..0x1f], [0x80..0x9f])
//      *  ^^ / ^; / ^[ / ^] / ^= / ^" / ^'
//         Significant syntax elements
//  *  string quoting with " or ' to support whitespace
//  *  optionally case-sensitive or case-insensitive
//  *  ; Comments on separate lines


struct IniVersion
{
	uint8 major = 0;
	uint8 minor = 0;
	uint8 patch = 0;
};

class IniFileHelpers
{
protected:
	static IniVersion ProbeVersion(const std::vector<mpt::ustring> &lines);
	static std::list<std::pair<SettingPath, SettingValue>> CreateIniHeader(IniVersion version);
	static mpt::ustring FormatValueAsIni(const SettingValue &value);
	static SettingValue ParseValueFromIni(const mpt::ustring &str, const SettingValue &def);
};


class IniFileBase
{
protected:
	const mpt::PathString filename;
	AtomicSharedFile file;
	std::optional<Caching> sync_default;
protected:
	IniFileBase(mpt::PathString filename_, std::optional<Caching> sync_hint);
	~IniFileBase() = default;
public:
	const mpt::PathString &Filename() const;
	mpt::PathString GetFilename() const;
public:
	void MakeBackup(const mpt::PathString &tag, std::optional<Caching> sync_hint);
};


class CachedIniFileSettingsBackend
	: public ISettingsBackend<SettingsBatching::All>
	, public IniFileBase
	, protected IniFileHelpers
{
private:
	struct comments
	{
		std::vector<mpt::ustring> before;
		std::vector<mpt::ustring> after;
	};
private:
	std::map<mpt::ustring, std::optional<std::map<mpt::ustring, std::optional<mpt::ustring>>>> cache;
	std::map<std::pair<mpt::ustring, mpt::ustring>, comments> comments;
#if MPT_SETTINGS_INI_CASE_INSENSITIVE
	const CaseSensitivity case_sensitivity;
	std::map<std::pair<mpt::ustring, mpt::ustring>, std::pair<mpt::ustring, mpt::ustring>> casemap;
#endif
private:
	std::vector<mpt::ustring> ReadFileAsLines();
	void ReadLinesIntoCache(const std::vector<mpt::ustring> &lines);
	void ReadLinesIntoCache(IniVersion version, const std::vector<mpt::ustring> &lines);
	void ReadFileIntoCache();
	void MergeSettingsIntoCache(const std::set<mpt::ustring> &removeSections, const std::map<SettingPath, std::optional<SettingValue>> &settings);
	void WriteCacheIntoFile(std::optional<Caching> sync_hint);
#if MPT_SETTINGS_INI_CASE_INSENSITIVE
	void VerifyCasemap() const;
#endif
private:
	void Init();
public:
#if MPT_SETTINGS_INI_CASE_INSENSITIVE
	CachedIniFileSettingsBackend(mpt::PathString filename_);
	CachedIniFileSettingsBackend(mpt::PathString filename_, std::optional<Caching> sync_hint);
	CachedIniFileSettingsBackend(mpt::PathString filename_, std::optional<Caching> sync_hint, CaseSensitivity case_sensitivity_);
	CachedIniFileSettingsBackend(mpt::PathString filename_, CaseSensitivity case_sensitivity_);
	CachedIniFileSettingsBackend(mpt::PathString filename_, CaseSensitivity case_sensitivity_, std::optional<Caching> sync_hint);
#else
	CachedIniFileSettingsBackend(mpt::PathString filename_, std::optional<Caching> sync_hint = std::nullopt);
#endif
	~CachedIniFileSettingsBackend() override;
public:
	virtual CaseSensitivity GetCaseSensitivity() const override;
	virtual void InvalidateCache() override;
	virtual SettingValue ReadSetting(const SettingPath &path, const SettingValue &def) const override;
	virtual void WriteAllSettings(const std::set<mpt::ustring> &removeSections, const std::map<SettingPath, std::optional<SettingValue>> &settings, std::optional<Caching> sync_hint) override;
};


using IniFileSettingsBackend = CachedIniFileSettingsBackend;

using IniFileSettingsContainer = FileSettingsContainer<CachedIniFileSettingsBackend>;


OPENMPT_NAMESPACE_END
