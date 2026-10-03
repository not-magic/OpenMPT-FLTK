// FLTK port of openmpt/mptrack/FileDialog.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "FileDialog.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Mptrack.h"
#include "TrackerSettings.h"

#include <FL/Fl_Native_File_Chooser.H>


OPENMPT_NAMESPACE_BEGIN


namespace
{

std::string ToUtf8(const mpt::ustring &str)
{
	return mpt::transcode<std::string>(mpt::common_encoding::utf8, str);
}

std::string ToUtf8(const mpt::PathString &path)
{
	return path.ToUTF8();
}

// "Description|*.a;*.b|..." to the "Description\t*.{a,b}\n..." format of FLTK
std::string ConvertFilter(const mpt::ustring &filter)
{
	std::vector<std::string> parts;
	const std::string text = ToUtf8(filter);
	size_t start = 0;
	while(start <= text.size())
	{
		const size_t end = text.find('|', start);
		parts.push_back(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
		if(end == std::string::npos)
			break;
		start = end + 1;
	}
	std::string result;
	for(size_t i = 0; i + 1 < parts.size(); i += 2)
	{
		if(parts[i].empty() && parts[i + 1].empty())
			continue;
		std::vector<std::string> patterns;
		size_t p = 0;
		while(p <= parts[i + 1].size())
		{
			const size_t semi = parts[i + 1].find(';', p);
			std::string pattern = parts[i + 1].substr(p, semi == std::string::npos ? std::string::npos : semi - p);
			if(pattern.starts_with("*."))
				pattern.erase(0, 2);
			if(!pattern.empty())
				patterns.push_back(pattern);
			if(semi == std::string::npos)
				break;
			p = semi + 1;
		}
		std::string glob = "*";
		if(patterns.size() == 1 && patterns[0] != "*")
			glob = "*." + patterns[0];
		else if(patterns.size() > 1)
		{
			glob = "*.{";
			for(size_t n = 0; n < patterns.size(); ++n)
				glob += (n ? "," : "") + patterns[n];
			glob += "}";
		}
		result += parts[i] + "\t" + glob + "\n";
	}
	return result;
}

}  // namespace


bool FileDialog::Show(Wnd *)
{
	m_filenames.clear();

	Fl_Native_File_Chooser chooser(m_load ? (m_multiSelect ? Fl_Native_File_Chooser::BROWSE_MULTI_FILE : Fl_Native_File_Chooser::BROWSE_FILE) : Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
	const std::string caption = ToUtf8(m_caption);
	if(!caption.empty())
		chooser.title(caption.c_str());
	const std::string filter = ConvertFilter(m_extFilter);
	if(!filter.empty())
	{
		chooser.filter(filter.c_str());
		if(m_filterIndex != nullptr)
			chooser.filter_value(*m_filterIndex > 0 ? *m_filterIndex - 1 : 0);
	}
	const std::string directory = ToUtf8(m_workingDirectory);
	if(!directory.empty())
		chooser.directory(directory.c_str());
	const std::string defaultName = ToUtf8(m_defaultFilename);
	if(!defaultName.empty())
		chooser.preset_file(defaultName.c_str());
	chooser.options(m_load ? 0 : Fl_Native_File_Chooser::SAVEAS_CONFIRM | Fl_Native_File_Chooser::USE_FILTER_EXT);

	BypassInputHandler bih;
	if(chooser.show() != 0)
		return false;

	if(m_filterIndex != nullptr)
		*m_filterIndex = chooser.filter_value() + 1;

	for(int i = 0; i < chooser.count(); ++i)
	{
		if(const char *name = chooser.filename(i))
			m_filenames.push_back(mpt::PathString::FromUTF8(name));
	}
	if(m_filenames.empty())
		return false;

	if(!m_load && !m_defaultExtension.empty() && m_filenames.front().GetFilenameExtension().empty())
		m_filenames.front() = m_filenames.front() + mpt::PathString::FromUnicode(UL_(".") + m_defaultExtension);

	m_workingDirectory = m_filenames.front().GetDirectoryWithDrive();
	m_extension = m_filenames.front().GetFilenameExtension().ToUnicode();
	if(!m_extension.empty())
		m_extension.erase(0, 1);
	return true;
}


bool BrowseForFolder::Show(Wnd *)
{
	BypassInputHandler bih;
	Fl_Native_File_Chooser chooser(Fl_Native_File_Chooser::BROWSE_DIRECTORY);
	const std::string caption = ToUtf8(m_caption);
	if(!caption.empty())
		chooser.title(caption.c_str());
	const std::string directory = ToUtf8(m_workingDirectory);
	if(!directory.empty())
		chooser.directory(directory.c_str());
	if(chooser.show() != 0 || chooser.filename() == nullptr)
		return false;
	m_workingDirectory = mpt::PathString::FromUTF8(chooser.filename());
	return true;
}


OPENMPT_NAMESPACE_END
