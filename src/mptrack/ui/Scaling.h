/*
 * Scaling.h
 * ---------
 * Purpose: Helpers for scaling pixel sizes. FLTK applies the screen scale factor itself, so all logical coordinates are at 96 DPI.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"


OPENMPT_NAMESPACE_BEGIN


namespace ui
{

class Wnd;

constexpr uint32 kLogicalDpi = 96;

constexpr uint32 GetDpiForWindow(const Wnd *) noexcept
{
	return kLogicalDpi;
}

constexpr int ScalePixels(int pixels, const Wnd *) noexcept
{
	return pixels;
}

constexpr int ScalePixelsInv(int pixels, const Wnd *) noexcept
{
	return pixels;
}

constexpr uint32 GetDpiForWindow(const Wnd &) noexcept { return kLogicalDpi; }
constexpr int ScalePixels(int pixels, const Wnd &) noexcept { return pixels; }
constexpr int ScalePixelsInv(int pixels, const Wnd &) noexcept { return pixels; }

}  // namespace ui


OPENMPT_NAMESPACE_END
