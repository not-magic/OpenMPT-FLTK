// MFC replacement on FLTK. Access to the clipboard with named data formats.

#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include <map>
#include <string_view>
#include <vector>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{

enum ClipboardFormat : uint32
{
	ClipboardText = 1,
	ClipboardWave = 12,
	ClipboardUnicodeText = 13,
	ClipboardFileList = 15,
	ClipboardFirstCustom = 0xC000,
};

// Returns the ID of a named clipboard format, creating it if necessary
uint32 RegisterClipboardFormat(const mpt::ustring &name);
bool IsClipboardFormatAvailable(uint32 format);


class Clipboard
{
public:
	// Open clipboard for writing (size > 0) or reading (size == 0).
	Clipboard(uint32 clipFormat, size_t size = 0);
	~Clipboard();

	Clipboard(const Clipboard &) = delete;
	Clipboard &operator=(const Clipboard &) = delete;

	bool IsValid() const { return !m_data.empty(); }

	template <typename T>
	T *As()
	{
		return mpt::byte_cast<T *>(m_data.data());
	}

	mpt::byte_span Get() { return mpt::as_span(m_data); }

	std::string_view GetString() const
	{
		if(!m_data.empty())
			return {mpt::byte_cast<const char *>(m_data.data()), m_data.size()};
		else
			return {};
	}

	std::u16string_view GetWideString() const;

	Clipboard &operator=(mpt::const_byte_span data);

	template <typename T>
	Clipboard &operator=(const T &v)
	{
		return *this = mpt::as_raw_memory(v);
	}

	// Makes the data available to others
	void Close();

private:
	uint32 m_format;
	std::vector<std::byte> m_data;
	bool m_isWriting = false;
};

}  // namespace ui


OPENMPT_NAMESPACE_END
