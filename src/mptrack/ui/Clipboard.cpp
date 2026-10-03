/*
 * Clipboard.cpp
 * -------------
 * Purpose: Access to the clipboard with named data formats.
 * Notes  : The data is kept inside the application; text is also copied to the system clipboard.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "Clipboard.h"

#include <FL/Fl.H>

#include <algorithm>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{


namespace
{

std::map<uint32, std::vector<std::byte>> &Storage()
{
	static std::map<uint32, std::vector<std::byte>> storage;
	return storage;
}

std::map<std::string, uint32> &Registry()
{
	static std::map<std::string, uint32> registry;
	return registry;
}

}  // namespace


uint32 RegisterClipboardFormat(const mpt::ustring &name)
{
	const std::string key = mpt::transcode<std::string>(mpt::common_encoding::utf8, name);
	auto &registry = Registry();
	const auto it = registry.find(key);
	if(it != registry.end())
		return it->second;
	const uint32 id = ClipboardFirstCustom + static_cast<uint32>(registry.size());
	registry[key] = id;
	return id;
}


bool IsClipboardFormatAvailable(uint32 format)
{
	const auto &storage = Storage();
	const auto it = storage.find(format);
	if(it != storage.end() && !it->second.empty())
		return true;
	// Plain text may come from other applications
	if(format == ClipboardText || format == ClipboardUnicodeText)
		return Fl::clipboard_contains(Fl::clipboard_plain_text) != 0;
	return false;
}


Clipboard::Clipboard(uint32 clipFormat, size_t size)
    : m_format(clipFormat)
{
	if(size > 0)
	{
		m_isWriting = true;
		m_data.assign(size, std::byte{0});
	} else
	{
		const auto &storage = Storage();
		const auto it = storage.find(m_format);
		if(it != storage.end())
			m_data = it->second;
	}
}


Clipboard::~Clipboard()
{
	Close();
}


std::u16string_view Clipboard::GetWideString() const
{
	if(m_format == ClipboardUnicodeText && m_data.size() >= sizeof(char16_t))
		return {reinterpret_cast<const char16_t *>(m_data.data()), m_data.size() / sizeof(char16_t)};
	return {};
}


Clipboard &Clipboard::operator=(mpt::const_byte_span data)
{
	MPT_ASSERT(m_data.size() >= data.size());
	std::copy(data.begin(), data.end(), m_data.begin());
	return *this;
}


void Clipboard::Close()
{
	if(!m_isWriting)
		return;
	m_isWriting = false;
	auto &storage = Storage();
	if(m_format == ClipboardText || m_format == ClipboardUnicodeText)
	{
		// A text that was written replaces both text formats
		storage.erase(ClipboardText);
		storage.erase(ClipboardUnicodeText);
		std::string text;
		if(m_format == ClipboardText)
		{
			text.assign(reinterpret_cast<const char *>(m_data.data()), m_data.size());
			const std::size_t end = text.find('\0');
			if(end != std::string::npos)
				text.resize(end);
		} else
		{
			std::u16string wide(reinterpret_cast<const char16_t *>(m_data.data()), m_data.size() / sizeof(char16_t));
			const std::size_t end = wide.find(u'\0');
			if(end != std::u16string::npos)
				wide.resize(end);
			text = mpt::transcode<std::string>(mpt::common_encoding::utf8, mpt::transcode<mpt::ustring>(wide));
		}
		Fl::copy(text.c_str(), static_cast<int>(text.size()), 1);
	}
	storage[m_format] = std::move(m_data);
	m_data.clear();
}


}  // namespace ui


OPENMPT_NAMESPACE_END
