/*
 * Image.h
 * -------
 * Purpose: Bitmap image file handling.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "../common/FileReaderFwd.h"

#include <memory>
#include <stdexcept>
#include <vector>


OPENMPT_NAMESPACE_BEGIN


class bad_image : public std::runtime_error { public: bad_image() : std::runtime_error("") { } };


// Straight (non-premultiplied) 8-bit RGBA image
class RawImage
{
public:
	struct Pixel
	{
		uint8 r;
		uint8 g;
		uint8 b;
		uint8 a;
		constexpr Pixel() noexcept
		    : r(0), g(0), b(0), a(0) {}
		constexpr Pixel(uint8 r, uint8 g, uint8 b, uint8 a) noexcept
		    : r(r), g(g), b(b), a(a) {}
		constexpr Pixel(ColorRef color) noexcept
		    : r(GetRValue(color)), g(GetGValue(color)), b(GetBValue(color)), a(0) {}
	};
private:
	uint32 width;
	uint32 height;
	std::vector<Pixel> pixels;
public:
	RawImage(uint32 width, uint32 height);
public:
	constexpr uint32 Width() const noexcept { return width; }
	constexpr uint32 Height() const noexcept { return height; }
	MPT_ATTR_ALWAYSINLINE MPT_INLINE_FORCE Pixel &operator()(uint32 x, uint32 y) noexcept { return pixels[y * width + x]; }
	MPT_ATTR_ALWAYSINLINE MPT_INLINE_FORCE const Pixel &operator()(uint32 x, uint32 y) const noexcept { return pixels[y * width + x]; }
	std::vector<Pixel> &Pixels() { return pixels; }
	const std::vector<Pixel> &Pixels() const { return pixels; }
};


// Decodes a PNG image. If spriteWidth and spriteHeight are given, the image is a grid of sprites that are scaled individually.
// Throws bad_image if the file cannot be decoded.
std::unique_ptr<RawImage> LoadPixelImage(mpt::const_byte_span file, double scaling = 1.0, int spriteWidth = 0, int spriteHeight = 0);
std::unique_ptr<RawImage> LoadPixelImage(FileReader file, double scaling = 1.0, int spriteWidth = 0, int spriteHeight = 0);

// Converts the image to a bitmap that keeps the alpha channel
ui::Bitmap ToBitmap(const RawImage &image);


OPENMPT_NAMESPACE_END
