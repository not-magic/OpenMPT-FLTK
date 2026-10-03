// MFC replacement on FLTK. A collection of equally sized images, such as the icons of a toolbar.

#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "Painter.h"

#include <vector>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{

class ImageList
{
public:
	ImageList() = default;

	// Sets the size of a single image and removes all images
	void Create(int imageWidth, int imageHeight);
	// Splits a horizontal strip of images and appends them. Returns the index of the first new image.
	int AddStrip(const Bitmap &strip);
	// Appends a single image
	int Add(const Bitmap &image);
	void RemoveAll() { m_images.clear(); }

	int GetImageCount() const { return static_cast<int>(m_images.size()); }
	int GetImageWidth() const noexcept { return m_imageWidth; }
	int GetImageHeight() const noexcept { return m_imageHeight; }
	const Bitmap *GetImage(int index) const;

	// Draws an image at the position (relative to the origin of the painter)
	void Draw(Painter &painter, int index, Point position) const;

private:
	int m_imageWidth = 0;
	int m_imageHeight = 0;
	std::vector<Bitmap> m_images;
};

}  // namespace ui


OPENMPT_NAMESPACE_END
