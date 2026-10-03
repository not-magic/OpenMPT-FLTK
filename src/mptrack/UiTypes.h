// Basic types shared by all GUI code, replacing the Win32 types used by openmpt/mptrack.

#pragma once

#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <string>
#include <thread>

#include "openmpt/all/BuildSettings.hpp"

#include <cstdint>

class Fl_Widget;
class Fl_Window;
class Fl_Group;


OPENMPT_NAMESPACE_BEGIN


namespace ui
{
class Wnd;
}  // namespace ui
using namespace ui;


using WindowHandle = ui::Wnd *;

using WParam = std::uintptr_t;
using LParam = std::intptr_t;
using LResult = std::intptr_t;


// Base class for objects that are passed around as update hints.
class HintObject
{
public:
	virtual ~HintObject() = default;
};


// Converts between UTF-8 strings as used by FLTK and mpt::ustring.
inline const char *ToFl(const mpt::ustring &str) noexcept
{
	return reinterpret_cast<const char *>(str.c_str());
}


// C-string helpers for fixed-size character buffers
inline std::size_t _tcslen(const mpt::uchar *s) { return std::char_traits<mpt::uchar>::length(s); }
inline int _tcscmp(const mpt::uchar *a, const mpt::uchar *b) { return std::char_traits<mpt::uchar>::compare(a, b, std::min(_tcslen(a), _tcslen(b)) + 1); }
inline int _tcsncmp(const mpt::uchar *a, const mpt::uchar *b, std::size_t n) { return std::char_traits<mpt::uchar>::compare(a, b, n); }
inline mpt::uchar *_tcscpy(mpt::uchar *dest, const mpt::uchar *src) { return std::char_traits<mpt::uchar>::copy(dest, src, _tcslen(src) + 1); }
inline mpt::uchar *_tcscat(mpt::uchar *dest, const mpt::uchar *src) { _tcscpy(dest + _tcslen(dest), src); return dest; }
inline int _tstoi(const mpt::uchar *s) { return std::atoi(reinterpret_cast<const char *>(s)); }
inline int _ttoi(const mpt::uchar *s) { return _tstoi(s); }
inline int _tstoi(const mpt::ustring &s) { return _tstoi(s.c_str()); }
inline int _ttoi(const mpt::ustring &s) { return _tstoi(s.c_str()); }
inline double _tstof(const mpt::uchar *s) { return std::atof(reinterpret_cast<const char *>(s)); }

inline int MulDiv(int number, int numerator, int denominator) noexcept
{
	return denominator == 0 ? -1 : static_cast<int>((static_cast<int64>(number) * numerator + denominator / 2) / denominator);
}
inline void Sleep(uint32 milliseconds) { std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); }

// Writes printf-style text into a buffer that holds at least 1024 characters
int wsprintf(mpt::uchar *buffer, const mpt::uchar *format, ...);

OPENMPT_NAMESPACE_END


// Evaluates the expression in all builds and asserts in debug builds
#define MPT_VERIFY(expression) \
	do \
	{ \
		const bool mptVerifyResult = static_cast<bool>(expression); \
		MPT_ASSERT(mptVerifyResult); \
		MPT_UNUSED(mptVerifyResult); \
	} while(0)
