/*
 * Image.cpp
 * ---------
 * Purpose: Bitmap and Vector image file handling using GDI+.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/Image.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "Image.h"
#include "../common/FileReader.h"

#include <FL/Fl_PNG_Image.H>

#include <algorithm>
#include <cmath>


OPENMPT_NAMESPACE_BEGIN


RawImage::RawImage(uint32 width, uint32 height)
    : width(width)
    , height(height)
    , pixels(static_cast<std::size_t>(width) * height)
{
	MPT_ASSERT(width > 0);
	MPT_ASSERT(height > 0);
}


namespace
{

// Bilinear resampling of a rectangular part of an image into a new image
std::unique_ptr<RawImage> ResizeRegion(const RawImage &src, uint32 srcX, uint32 srcY, uint32 srcWidth, uint32 srcHeight, uint32 dstWidth, uint32 dstHeight)
{
	auto result = std::make_unique<RawImage>(dstWidth, dstHeight);
	const double scaleX = static_cast<double>(srcWidth) / dstWidth;
	const double scaleY = static_cast<double>(srcHeight) / dstHeight;
	for(uint32 y = 0; y < dstHeight; ++y)
	{
		for(uint32 x = 0; x < dstWidth; ++x)
		{
			// Average the source area that is covered by the destination pixel
			const double left = x * scaleX;
			const double right = std::max((x + 1) * scaleX, left + 1.0);
			const double top = y * scaleY;
			const double bottom = std::max((y + 1) * scaleY, top + 1.0);
			double sumR = 0, sumG = 0, sumB = 0, sumA = 0, sumWeight = 0;
			for(uint32 sy = static_cast<uint32>(top); sy < std::min<uint32>(srcHeight, static_cast<uint32>(std::ceil(bottom))); ++sy)
			{
				const double weightY = std::min<double>(sy + 1, bottom) - std::max<double>(sy, top);
				for(uint32 sx = static_cast<uint32>(left); sx < std::min<uint32>(srcWidth, static_cast<uint32>(std::ceil(right))); ++sx)
				{
					const double weight = weightY * (std::min<double>(sx + 1, right) - std::max<double>(sx, left));
					const RawImage::Pixel &p = src(srcX + sx, srcY + sy);
					const double alphaWeight = weight * p.a;
					sumR += p.r * alphaWeight;
					sumG += p.g * alphaWeight;
					sumB += p.b * alphaWeight;
					sumA += alphaWeight;
					sumWeight += weight;
				}
			}
			RawImage::Pixel &out = (*result)(x, y);
			if(sumA > 0.0 && sumWeight > 0.0)
			{
				out.r = static_cast<uint8>(std::clamp(sumR / sumA + 0.5, 0.0, 255.0));
				out.g = static_cast<uint8>(std::clamp(sumG / sumA + 0.5, 0.0, 255.0));
				out.b = static_cast<uint8>(std::clamp(sumB / sumA + 0.5, 0.0, 255.0));
				out.a = static_cast<uint8>(std::clamp(sumA / sumWeight + 0.5, 0.0, 255.0));
			}
		}
	}
	return result;
}


std::unique_ptr<RawImage> ScaleImage(std::unique_ptr<RawImage> image, double scaling, int spriteWidth, int spriteHeight)
{
	if(scaling == 1.0)
		return image;
	if(spriteWidth <= 0 || spriteHeight <= 0)
	{
		spriteWidth = static_cast<int>(image->Width());
		spriteHeight = static_cast<int>(image->Height());
	}
	const uint32 scaledSpriteWidth = std::max<uint32>(1, static_cast<uint32>(std::lround(spriteWidth * scaling)));
	const uint32 scaledSpriteHeight = std::max<uint32>(1, static_cast<uint32>(std::lround(spriteHeight * scaling)));
	const uint32 columns = std::max<uint32>(1, image->Width() / spriteWidth);
	const uint32 rows = std::max<uint32>(1, image->Height() / spriteHeight);
	auto result = std::make_unique<RawImage>(scaledSpriteWidth * columns, scaledSpriteHeight * rows);
	for(uint32 row = 0; row < rows; ++row)
	{
		for(uint32 column = 0; column < columns; ++column)
		{
			const auto sprite = ResizeRegion(*image, column * spriteWidth, row * spriteHeight, spriteWidth, spriteHeight, scaledSpriteWidth, scaledSpriteHeight);
			for(uint32 y = 0; y < scaledSpriteHeight; ++y)
			{
				for(uint32 x = 0; x < scaledSpriteWidth; ++x)
					(*result)(column * scaledSpriteWidth + x, row * scaledSpriteHeight + y) = (*sprite)(x, y);
			}
		}
	}
	return result;
}

}  // namespace


std::unique_ptr<RawImage> LoadPixelImage(mpt::const_byte_span file, double scaling, int spriteWidth, int spriteHeight)
{
	if(file.empty())
		throw bad_image();
	Fl_PNG_Image png(nullptr, reinterpret_cast<const unsigned char *>(file.data()), static_cast<int>(file.size()));
	if(png.fail() || png.w() <= 0 || png.h() <= 0 || png.d() < 1 || png.data() == nullptr || png.count() < 1)
		throw bad_image();
	auto image = std::make_unique<RawImage>(png.w(), png.h());
	const int depth = png.d();
	const int lineStride = png.ld() ? png.ld() : png.w() * depth;
	const unsigned char *data = reinterpret_cast<const unsigned char *>(png.data()[0]);
	for(int y = 0; y < png.h(); ++y)
	{
		const unsigned char *row = data + static_cast<std::ptrdiff_t>(y) * lineStride;
		for(int x = 0; x < png.w(); ++x)
		{
			const unsigned char *p = row + x * depth;
			RawImage::Pixel &out = (*image)(x, y);
			switch(depth)
			{
			case 1:
				out = RawImage::Pixel(p[0], p[0], p[0], 255);
				break;
			case 2:
				out = RawImage::Pixel(p[0], p[0], p[0], p[1]);
				break;
			case 3:
				out = RawImage::Pixel(p[0], p[1], p[2], 255);
				break;
			default:
				out = RawImage::Pixel(p[0], p[1], p[2], p[3]);
				break;
			}
		}
	}
	return ScaleImage(std::move(image), scaling, spriteWidth, spriteHeight);
}


std::unique_ptr<RawImage> LoadPixelImage(FileReader file, double scaling, int spriteWidth, int spriteHeight)
{
	FileReader::PinnedView view = file.GetPinnedView();
	return LoadPixelImage(view.span(), scaling, spriteWidth, spriteHeight);
}


ui::Bitmap ToBitmap(const RawImage &image)
{
	ui::Bitmap bitmap(static_cast<int>(image.Width()), static_cast<int>(image.Height()));
	bitmap.SetHasAlpha(true);
	uint32 *out = bitmap.GetPixels();
	for(const RawImage::Pixel &p : image.Pixels())
		*out++ = (static_cast<uint32>(p.a) << 24) | (static_cast<uint32>(p.r) << 16) | (static_cast<uint32>(p.g) << 8) | p.b;
	return bitmap;
}


OPENMPT_NAMESPACE_END
