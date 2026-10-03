/*
 * ImageList.cpp
 * -------------
 * Purpose: A collection of equally sized images, such as the icons of a toolbar.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "ImageList.h"


OPENMPT_NAMESPACE_BEGIN


namespace ui
{


void ImageList::Create(int imageWidth, int imageHeight)
{
	m_imageWidth = imageWidth;
	m_imageHeight = imageHeight;
	m_images.clear();
}


int ImageList::AddStrip(const Bitmap &strip)
{
	const int first = GetImageCount();
	if(m_imageWidth <= 0 || m_imageHeight <= 0)
		return first;
	const int columns = strip.GetWidth() / m_imageWidth;
	const int rows = std::max(strip.GetHeight() / m_imageHeight, 1);
	for(int row = 0; row < rows; ++row)
	{
		for(int column = 0; column < columns; ++column)
		{
			Bitmap image(m_imageWidth, m_imageHeight);
			image.SetHasAlpha(strip.HasAlpha());
			for(int y = 0; y < m_imageHeight; ++y)
			{
				for(int x = 0; x < m_imageWidth; ++x)
					image.GetPixels()[y * m_imageWidth + x] = strip.GetPixels()[(row * m_imageHeight + y) * strip.GetWidth() + column * m_imageWidth + x];
			}
			m_images.push_back(std::move(image));
		}
	}
	return first;
}


int ImageList::Add(const Bitmap &image)
{
	m_images.push_back(image);
	return GetImageCount() - 1;
}


const Bitmap *ImageList::GetImage(int index) const
{
	if(index < 0 || index >= GetImageCount())
		return nullptr;
	return &m_images[index];
}


void ImageList::Draw(Painter &painter, int index, Point position) const
{
	if(const Bitmap *image = GetImage(index))
		painter.DrawBitmap(*image, position.x, position.y, 0, 0, image->GetWidth(), image->GetHeight());
}


}  // namespace ui


OPENMPT_NAMESPACE_END
