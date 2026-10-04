// MFC replacement on FLTK. Immediate-mode 2D drawing API with GDI-like semantics, implemented on top of FLTK.

#include "stdafx.h"
#include "Painter.h"
#include "../UiTypes.h"

#include <FL/fl_draw.H>
#include <FL/Fl.H>
#include <FL/Fl_Image.H>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <utility>
#include <vector>
#include <string>
#include <string_view>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{


namespace
{

// Splits text into lines at '\n', dropping a '\r' preceding it
std::vector<std::string_view> SplitLines(std::string_view text)
{
	std::vector<std::string_view> lines;
	std::size_t start = 0;
	while(true)
	{
		const std::size_t end = text.find('\n', start);
		if(end == std::string_view::npos)
		{
			lines.push_back(text.substr(start));
			break;
		}
		std::size_t lineEnd = end;
		if(lineEnd > start && text[lineEnd - 1] == '\r')
			--lineEnd;
		lines.push_back(text.substr(start, lineEnd - start));
		start = end + 1;
	}
	return lines;
}


std::string FitWithEllipsis(std::string_view line, int maxWidth)
{
	if(static_cast<int>(fl_width(line.data(), static_cast<int>(line.size()))) <= maxWidth)
		return std::string(line);
	static constexpr std::string_view ellipsis = "...";
	std::size_t length = line.size();
	while(length > 0)
	{
		--length;
		// Do not cut in the middle of a UTF-8 sequence
		while(length > 0 && (static_cast<unsigned char>(line[length]) & 0xC0) == 0x80)
			--length;
		std::string candidate(line.substr(0, length));
		candidate += ellipsis;
		if(static_cast<int>(fl_width(candidate.data(), static_cast<int>(candidate.size()))) <= maxWidth)
			return candidate;
	}
	return std::string(ellipsis);
}

}  // namespace


ColorRef GetSystemColor(SysColor color)
{
	Fl_Color flColor = FL_BACKGROUND_COLOR;
	switch(color)
	{
	case SysColor::ButtonFace:
	case SysColor::Face3d:
		flColor = FL_BACKGROUND_COLOR;
		break;
	case SysColor::ButtonShadow:
	case SysColor::Shadow3d:
		flColor = FL_DARK2;
		break;
	case SysColor::ButtonHighlight:
	case SysColor::Highlight3d:
		flColor = FL_LIGHT3;
		break;
	case SysColor::ButtonText:
	case SysColor::WindowText:
		flColor = FL_FOREGROUND_COLOR;
		break;
	case SysColor::Window:
		flColor = FL_BACKGROUND2_COLOR;
		break;
	case SysColor::Highlight:
		flColor = FL_SELECTION_COLOR;
		break;
	case SysColor::HighlightText:
		flColor = FL_WHITE;
		break;
	case SysColor::GrayText:
		flColor = FL_INACTIVE_COLOR;
		break;
	}
	uchar r = 0, g = 0, b = 0;
	Fl::get_color(flColor, r, g, b);
	return RGB(r, g, b);
}


void Bitmap::Create(int width, int height)
{
	m_width = std::max(width, 0);
	m_height = std::max(height, 0);
	m_pixels.assign(static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height), 0);
}


ColorRef Bitmap::GetPixel(int x, int y) const noexcept
{
	if(x < 0 || y < 0 || x >= m_width || y >= m_height)
		return 0;
	const uint32 pixel = m_pixels[static_cast<std::size_t>(y) * m_width + x];
	return RGB(static_cast<uint8>(pixel >> 16), static_cast<uint8>(pixel >> 8), static_cast<uint8>(pixel));
}


void Bitmap::FillRect(const Rect &rect, ColorRef color) noexcept
{
	const int left = std::max(rect.left, 0), top = std::max(rect.top, 0);
	const int right = std::min(rect.right, m_width), bottom = std::min(rect.bottom, m_height);
	for(int y = top; y < bottom; ++y)
	{
		for(int x = left; x < right; ++x)
			SetPixel(x, y, color);
	}
}


void Bitmap::SetPixel(int x, int y, ColorRef color) noexcept
{
	if(x < 0 || y < 0 || x >= m_width || y >= m_height)
		return;
	m_pixels[static_cast<std::size_t>(y) * m_width + x] = 0xFF000000u | (static_cast<uint32>(GetRValue(color)) << 16) | (static_cast<uint32>(GetGValue(color)) << 8) | GetBValue(color);
}


void Bitmap::Fill(ColorRef color) noexcept
{
	const uint32 pixel = 0xFF000000u | (static_cast<uint32>(GetRValue(color)) << 16) | (static_cast<uint32>(GetGValue(color)) << 8) | GetBValue(color);
	std::fill(m_pixels.begin(), m_pixels.end(), pixel);
}


Painter::Painter(Point origin)
	: m_origin(origin)
{
}


Painter::~Painter()
{
	while(m_clipDepth > 0)
		PopClip();
}


void Painter::Apply(ColorRef color) const
{
	fl_color(GetRValue(color), GetGValue(color), GetBValue(color));
}


Bitmap CreateWarningIcon(int size)
{
	Bitmap bitmap(size, size);
	bitmap.SetHasAlpha(true);
	uint32 *pixels = bitmap.GetPixels();
	const double half = size / 2.0;
	for(int y = 0; y < size; ++y)
	{
		for(int x = 0; x < size; ++x)
		{
			uint32 pixel = 0;
			const double top = size * 0.1;
			const double bottom = size * 0.92;
			if(y >= top && y <= bottom)
			{
				const double extent = (y - top) / (bottom - top) * half * 0.95;
				const double distance = std::abs(x + 0.5 - half);
				if(distance <= extent)
				{
					const bool isEdge = distance > extent - 1.5 || y > bottom - 1.5;
					const bool isBar = distance < size * 0.06 && y > size * 0.4 && y < size * 0.68;
					const bool isDot = distance < size * 0.07 && y > size * 0.74 && y < size * 0.84;
					if(isEdge || isBar || isDot)
						pixel = 0xFF000000u;
					else
						pixel = 0xFFF2C811u;
				}
			}
			pixels[static_cast<size_t>(y) * size + x] = pixel;
		}
	}
	return bitmap;
}


Bitmap CreateInfoIcon(int size)
{
	Bitmap bitmap(size, size);
	bitmap.SetHasAlpha(true);
	uint32 *pixels = bitmap.GetPixels();
	const double radius = size / 2.0 - 0.5;
	for(int y = 0; y < size; ++y)
	{
		for(int x = 0; x < size; ++x)
		{
			uint32 pixel = 0;
			const double dx = x + 0.5 - size / 2.0, dy = y + 0.5 - size / 2.0;
			const double distance = std::sqrt(dx * dx + dy * dy);
			if(distance <= radius)
			{
				const bool isStem = std::abs(dx) < size * 0.07 && y > size * 0.42 && y < size * 0.78;
				const bool isDot = std::abs(dx) < size * 0.08 && y > size * 0.2 && y < size * 0.34;
				pixel = (isStem || isDot) ? 0xFFFFFFFFu : 0xFF2A6FD6u;
			}
			pixels[static_cast<size_t>(y) * size + x] = pixel;
		}
	}
	return bitmap;
}


Font CreateFont(const mpt::ustring &name, int pixelSize, bool isBold, bool isItalic, bool isFixedPitch)
{
	static std::vector<std::pair<std::string, Fl_Font>> families;
	static bool isEnumerated = false;
	if(!isEnumerated)
	{
		isEnumerated = true;
		const Fl_Font count = Fl::set_fonts(nullptr);
		for(Fl_Font i = 0; i < count; ++i)
		{
			int attributes = 0;
			const char *fontName = Fl::get_font_name(i, &attributes);
			if(fontName != nullptr && attributes == 0)
				families.emplace_back(fontName, i);
		}
	}
	const std::string wanted = mpt::transcode<std::string>(mpt::common_encoding::utf8, name);
	for(const auto &[family, face] : families)
	{
		if(family.size() == wanted.size() && std::equal(family.begin(), family.end(), wanted.begin(), [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); }))
			return Font(face + (isBold ? 1 : 0) + (isItalic ? 2 : 0), pixelSize);
	}
	const Fl_Font base = isFixedPitch ? FL_COURIER : FL_HELVETICA;
	return Font(base + (isBold ? 1 : 0) + (isItalic ? 2 : 0), pixelSize);
}


void Painter::ApplyFont() const
{
	fl_font(m_font.face, m_font.size);
}


bool Painter::RectVisible(const Rect &rect) const
{
	Rect shifted = rect;
	shifted.OffsetRect(m_origin.x, m_origin.y);
	int x = 0, y = 0, width = 0, height = 0;
	fl_clip_box(shifted.left, shifted.top, shifted.Width(), shifted.Height(), x, y, width, height);
	return width > 0 && height > 0;
}


void Painter::FillSolidRect(const Rect &rect, ColorRef color)
{
	if(rect.IsRectEmpty())
		return;
	Apply(color);
	fl_rectf(rect.left + m_origin.x, rect.top + m_origin.y, rect.Width(), rect.Height());
}


void Painter::FrameRect(const Rect &rect, ColorRef color)
{
	if(rect.IsRectEmpty())
		return;
	Apply(color);
	fl_line_style(FL_SOLID, 0);
	fl_rect(rect.left + m_origin.x, rect.top + m_origin.y, rect.Width(), rect.Height());
}


void Painter::Draw3dRect(const Rect &rect, ColorRef topLeft, ColorRef bottomRight)
{
	if(rect.IsRectEmpty())
		return;
	FillSolidRect(Rect(rect.left, rect.top, rect.right - 1, rect.top + 1), topLeft);
	FillSolidRect(Rect(rect.left, rect.top, rect.left + 1, rect.bottom - 1), topLeft);
	FillSolidRect(Rect(rect.right - 1, rect.top, rect.right, rect.bottom), bottomRight);
	FillSolidRect(Rect(rect.left, rect.bottom - 1, rect.right, rect.bottom), bottomRight);
}


void Painter::DrawFocusRect(const Rect &rect)
{
	if(rect.IsRectEmpty())
		return;
	Apply(RGB(0, 0, 0));
	fl_line_style(FL_DOT, 1);
	fl_rect(rect.left + m_origin.x, rect.top + m_origin.y, rect.Width(), rect.Height());
	fl_line_style(FL_SOLID, 0);
}


void Painter::DrawEtchedEdge(const Rect &rect, uint32 edges)
{
	const ColorRef shadow = GetSystemColor(SysColor::Shadow3d);
	const ColorRef highlight = GetSystemColor(SysColor::Highlight3d);
	const int right = rect.right - 1, bottom = rect.bottom - 1;
	if(edges & EdgeLeft)
	{
		FillSolidRect(Rect(rect.left, rect.top, rect.left + 1, rect.bottom), shadow);
		FillSolidRect(Rect(rect.left + 1, rect.top, rect.left + 2, rect.bottom), highlight);
	}
	if(edges & EdgeTop)
	{
		FillSolidRect(Rect(rect.left, rect.top, rect.right, rect.top + 1), shadow);
		FillSolidRect(Rect(rect.left, rect.top + 1, rect.right, rect.top + 2), highlight);
	}
	if(edges & EdgeRight)
	{
		FillSolidRect(Rect(right, rect.top, right + 1, rect.bottom), shadow);
		FillSolidRect(Rect(right - 1, rect.top, right, rect.bottom), highlight);
	}
	if(edges & EdgeBottom)
	{
		FillSolidRect(Rect(rect.left, bottom, rect.right, bottom + 1), shadow);
		FillSolidRect(Rect(rect.left, bottom - 1, rect.right, bottom), highlight);
	}
}


Rect Painter::DrawBevel(const Rect &rect, bool isRaised, bool isFilled)
{
	const ColorRef highlight = GetSystemColor(SysColor::Highlight3d);
	const ColorRef shadow = GetSystemColor(SysColor::Shadow3d);
	const ColorRef face = GetSystemColor(SysColor::Face3d);
	const ColorRef darkShadow = RGB(0x69, 0x69, 0x69);
	Rect inner = rect;
	if(isRaised)
		Draw3dRect(inner, highlight, darkShadow);
	else
		Draw3dRect(inner, shadow, highlight);
	inner.DeflateRect(1, 1);
	if(isRaised)
		Draw3dRect(inner, face, shadow);
	else
		Draw3dRect(inner, darkShadow, face);
	inner.DeflateRect(1, 1);
	if(isFilled)
		FillSolidRect(inner, face);
	return inner;
}


void Painter::InvertRect(const Rect &rect)
{
	if(rect.IsRectEmpty())
		return;
	const int x = rect.left + m_origin.x;
	const int y = rect.top + m_origin.y;
	std::vector<uchar> rgb(static_cast<std::size_t>(rect.Width()) * static_cast<std::size_t>(rect.Height()) * 3);
	if(fl_read_image(rgb.data(), x, y, rect.Width(), rect.Height(), 0) == nullptr)
		return;
	for(auto &component : rgb)
		component = static_cast<uchar>(255 - component);
	fl_draw_image(rgb.data(), x, y, rect.Width(), rect.Height(), 3, 0);
}


void Painter::Rectangle(const Rect &rect)
{
	if(rect.IsRectEmpty())
		return;
	FillSolidRect(Rect(rect.left, rect.top, rect.right, rect.bottom), m_brushColor);
	FrameRect(rect, m_penColor);
}


void Painter::Ellipse(const Rect &rect)
{
	if(rect.IsRectEmpty())
		return;
	Apply(m_brushColor);
	fl_pie(rect.left + m_origin.x, rect.top + m_origin.y, rect.Width(), rect.Height(), 0.0, 360.0);
	Apply(m_penColor);
	fl_arc(rect.left + m_origin.x, rect.top + m_origin.y, rect.Width(), rect.Height(), 0.0, 360.0);
}


void Painter::Polygon(const Point *points, int count)
{
	if(count < 3)
		return;
	Apply(m_brushColor);
	fl_begin_polygon();
	for(int i = 0; i < count; ++i)
		fl_vertex(points[i].x + m_origin.x, points[i].y + m_origin.y);
	fl_end_polygon();
	Apply(m_penColor);
	fl_begin_loop();
	for(int i = 0; i < count; ++i)
		fl_vertex(points[i].x + m_origin.x, points[i].y + m_origin.y);
	fl_end_loop();
}


void Painter::Polyline(const Point *points, int count)
{
	if(count < 2)
		return;
	Apply(m_penColor);
	fl_line_style(FL_SOLID, m_penWidth > 1 ? m_penWidth : 0);
	fl_begin_line();
	for(int i = 0; i < count; ++i)
		fl_vertex(points[i].x + m_origin.x, points[i].y + m_origin.y);
	fl_end_line();
	fl_line_style(FL_SOLID, 0);
}


Point Painter::MoveTo(int x, int y) noexcept
{
	const Point previous = m_position;
	m_position = {x, y};
	return previous;
}


void Painter::LineTo(int x, int y)
{
	DrawLine(m_position.x, m_position.y, x, y);
	m_position = {x, y};
}


void Painter::DrawLine(int x1, int y1, int x2, int y2)
{
	Apply(m_penColor);
	fl_line_style(m_isPenDotted ? FL_DOT : FL_SOLID, m_penWidth > 1 ? m_penWidth : 0);
	fl_line(x1 + m_origin.x, y1 + m_origin.y, x2 + m_origin.x, y2 + m_origin.y);
	fl_line_style(FL_SOLID, 0);
}


Size Painter::GetTextExtent(const char *text, int length) const
{
	ApplyFont();
	const std::vector<std::string_view> lines = SplitLines(std::string_view(text, static_cast<std::size_t>(length)));
	int maxWidth = 0;
	for(const auto &line : lines)
		maxWidth = std::max(maxWidth, static_cast<int>(fl_width(line.data(), static_cast<int>(line.size()))));
	return {maxWidth, fl_height() * static_cast<int>(lines.size())};
}


Size Painter::GetTextExtent(const mpt::ustring &text) const
{
	return GetTextExtent(ToFl(text), static_cast<int>(text.size()));
}


int Painter::GetTextHeight() const
{
	ApplyFont();
	return fl_height();
}


int Painter::GetTextAscent() const
{
	ApplyFont();
	return fl_height() - fl_descent();
}


int Painter::DrawText(const mpt::ustring &text, Rect &rect, uint32 format)
{
	return DrawText(ToFl(text), static_cast<int>(text.size()), rect, format);
}


int Painter::DrawText(const char *text, int length, Rect &rect, uint32 format)
{
	ApplyFont();
	std::vector<std::string_view> lines;
	if(format & TextSingleLine)
		lines.emplace_back(text, static_cast<std::size_t>(length));
	else
		lines = SplitLines(std::string_view(text, static_cast<std::size_t>(length)));

	const int lineHeight = fl_height();
	const int totalHeight = lineHeight * static_cast<int>(lines.size());

	if(format & TextCalcRect)
	{
		int maxWidth = 0;
		for(const auto &line : lines)
			maxWidth = std::max(maxWidth, static_cast<int>(fl_width(line.data(), static_cast<int>(line.size()))));
		rect.right = rect.left + maxWidth;
		rect.bottom = rect.top + totalHeight;
		return totalHeight;
	}

	int y = rect.top;
	if(format & TextVCenter)
		y = rect.top + (rect.Height() - totalHeight) / 2;
	else if(format & TextBottom)
		y = rect.bottom - totalHeight;

	const bool useClip = !(format & TextNoClip);
	if(useClip)
		PushClip(rect);

	for(const auto &line : lines)
	{
		std::string fitted;
		std::string_view drawn = line;
		if((format & TextEndEllipsis) && !line.empty())
		{
			fitted = FitWithEllipsis(line, rect.Width());
			drawn = fitted;
		}
		const int width = static_cast<int>(fl_width(drawn.data(), static_cast<int>(drawn.size())));
		int x = rect.left;
		if(format & TextCenter)
			x = rect.left + (rect.Width() - width) / 2;
		else if(format & TextRight)
			x = rect.right - width;

		if(!m_isBkTransparent)
			FillSolidRect(Rect(x, y, x + width, y + lineHeight), m_bkColor);
		Apply(m_textColor);
		fl_draw(drawn.data(), static_cast<int>(drawn.size()), x + m_origin.x, y + fl_height() - fl_descent() + m_origin.y);
		y += lineHeight;
	}

	if(useClip)
		PopClip();
	return totalHeight;
}


void Painter::TextOut(int x, int y, const char *text, int length)
{
	ApplyFont();
	const int width = static_cast<int>(fl_width(text, length));
	const int height = fl_height();
	if(!m_isBkTransparent)
		FillSolidRect(Rect(x, y, x + width, y + height), m_bkColor);
	Apply(m_textColor);
	fl_draw(text, length, x + m_origin.x, y + height - fl_descent() + m_origin.y);
}


void Painter::TextOut(int x, int y, const mpt::ustring &text)
{
	TextOut(x, y, ToFl(text), static_cast<int>(text.size()));
}


void Painter::DrawBitmap(const Bitmap &bitmap, int destX, int destY, int srcX, int srcY, int width, int height)
{
	if(!bitmap.IsValid())
		return;
	// Clip to the source bitmap
	if(srcX < 0)
	{
		destX -= srcX;
		width += srcX;
		srcX = 0;
	}
	if(srcY < 0)
	{
		destY -= srcY;
		height += srcY;
		srcY = 0;
	}
	width = std::min(width, bitmap.GetWidth() - srcX);
	height = std::min(height, bitmap.GetHeight() - srcY);
	if(width <= 0 || height <= 0)
		return;

	const uint32 *pixels = bitmap.GetPixels();
	if(bitmap.HasAlpha())
	{
		std::vector<uchar> rgba(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
		for(int y = 0; y < height; ++y)
		{
			const uint32 *row = pixels + static_cast<std::size_t>(srcY + y) * bitmap.GetWidth() + srcX;
			uchar *out = rgba.data() + static_cast<std::size_t>(y) * width * 4;
			for(int x = 0; x < width; ++x)
			{
				out[0] = static_cast<uchar>(row[x] >> 16);
				out[1] = static_cast<uchar>(row[x] >> 8);
				out[2] = static_cast<uchar>(row[x]);
				out[3] = static_cast<uchar>(row[x] >> 24);
				out += 4;
			}
		}
		Fl_RGB_Image image(rgba.data(), width, height, 4);
		image.draw(destX + m_origin.x, destY + m_origin.y);
		return;
	}
	std::vector<uchar> rgb(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3);
	for(int y = 0; y < height; ++y)
	{
		const uint32 *row = pixels + static_cast<std::size_t>(srcY + y) * bitmap.GetWidth() + srcX;
		uchar *out = rgb.data() + static_cast<std::size_t>(y) * width * 3;
		for(int x = 0; x < width; ++x)
		{
			out[0] = static_cast<uchar>(row[x] >> 16);
			out[1] = static_cast<uchar>(row[x] >> 8);
			out[2] = static_cast<uchar>(row[x]);
			out += 3;
		}
	}
	fl_draw_image(rgb.data(), destX + m_origin.x, destY + m_origin.y, width, height, 3, 0);
}


void Painter::DrawRgbx(const uint8 *pixels, int destX, int destY, int width, int height)
{
	fl_draw_image(pixels, destX + m_origin.x, destY + m_origin.y, width, height, 4, 0);
}


void Painter::StretchBitmap(const Bitmap &bitmap, const Rect &dest)
{
	if(!bitmap.IsValid() || dest.IsRectEmpty())
		return;
	const int destWidth = dest.Width();
	const int destHeight = dest.Height();
	std::vector<uchar> rgb(static_cast<std::size_t>(destWidth) * static_cast<std::size_t>(destHeight) * 3);
	const uint32 *pixels = bitmap.GetPixels();
	for(int y = 0; y < destHeight; ++y)
	{
		const int srcY = std::min(y * bitmap.GetHeight() / destHeight, bitmap.GetHeight() - 1);
		uchar *out = rgb.data() + static_cast<std::size_t>(y) * destWidth * 3;
		for(int x = 0; x < destWidth; ++x)
		{
			const int srcX = std::min(x * bitmap.GetWidth() / destWidth, bitmap.GetWidth() - 1);
			const uint32 pixel = pixels[static_cast<std::size_t>(srcY) * bitmap.GetWidth() + srcX];
			out[0] = static_cast<uchar>(pixel >> 16);
			out[1] = static_cast<uchar>(pixel >> 8);
			out[2] = static_cast<uchar>(pixel);
			out += 3;
		}
	}
	fl_draw_image(rgb.data(), dest.left + m_origin.x, dest.top + m_origin.y, destWidth, destHeight, 3, 0);
}


void Painter::DrawOffscreen(Fl_Offscreen offscreen, int destX, int destY, int width, int height, int srcX, int srcY)
{
	fl_copy_offscreen(destX + m_origin.x, destY + m_origin.y, width, height, offscreen, srcX, srcY);
}


void Painter::PushClip(const Rect &rect)
{
	fl_push_clip(rect.left + m_origin.x, rect.top + m_origin.y, std::max(rect.Width(), 0), std::max(rect.Height(), 0));
	++m_clipDepth;
}


void Painter::PopClip()
{
	if(m_clipDepth > 0)
	{
		fl_pop_clip();
		--m_clipDepth;
	}
}


Rect Painter::GetClipBox() const
{
	int x = 0, y = 0, w = 0, h = 0;
	fl_clip_box(-0x10000, -0x10000, 0x20000, 0x20000, x, y, w, h);
	return Rect(x - m_origin.x, y - m_origin.y, x - m_origin.x + w, y - m_origin.y + h);
}


void OffscreenBuffer::Create(int width, int height)
{
	Destroy();
	m_width = std::max(width, 1);
	m_height = std::max(height, 1);
	m_offscreen = fl_create_offscreen(m_width, m_height);
}


void OffscreenBuffer::Destroy()
{
	if(m_offscreen)
	{
		fl_delete_offscreen(m_offscreen);
		m_offscreen = 0;
	}
	m_width = m_height = 0;
}


Bitmap OffscreenBuffer::ReadPixels() const
{
	Bitmap bitmap(m_width, m_height);
	std::vector<uchar> rgb(static_cast<size_t>(m_width) * m_height * 3);
	if(fl_read_image(rgb.data(), 0, 0, m_width, m_height) != nullptr)
	{
		uint32 *out = bitmap.GetPixels();
		for(size_t i = 0; i < static_cast<size_t>(m_width) * m_height; ++i)
			out[i] = 0xFF000000u | (static_cast<uint32>(rgb[i * 3]) << 16) | (static_cast<uint32>(rgb[i * 3 + 1]) << 8) | rgb[i * 3 + 2];
	}
	return bitmap;
}


void OffscreenBuffer::Begin()
{
	if(m_offscreen)
		fl_begin_offscreen(m_offscreen);
}


void OffscreenBuffer::End()
{
	if(m_offscreen)
		fl_end_offscreen();
}


}  // namespace ui


OPENMPT_NAMESPACE_END
