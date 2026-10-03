// MFC replacement on FLTK. Immediate-mode 2D drawing API with GDI-like semantics, implemented on top of FLTK.

#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../Geometry.h"

#include <memory>
#include <vector>

#include <FL/Enumerations.H>
#include <FL/platform_types.h>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{


// Background modes of text
constexpr int TRANSPARENT = 1;
constexpr int OPAQUE = 2;


enum TextFormat : uint32
{
	TextLeft = 0x0000,
	TextCenter = 0x0001,
	TextRight = 0x0002,
	TextTop = 0x0000,
	TextVCenter = 0x0004,
	TextBottom = 0x0008,
	TextSingleLine = 0x0010,
	TextNoClip = 0x0020,
	TextEndEllipsis = 0x0040,
	TextCalcRect = 0x0080,
	TextNoPrefix = 0x0100,
};


// Colors of the desktop theme
enum EtchedEdge : uint32
{
	EdgeLeft = 1,
	EdgeTop = 2,
	EdgeRight = 4,
	EdgeBottom = 8,
};


enum class SysColor : uint8
{
	ButtonFace,
	ButtonShadow,
	ButtonHighlight,
	ButtonText,
	Window,
	WindowText,
	Highlight,
	HighlightText,
	GrayText,
	Shadow3d,
	Highlight3d,
	Face3d,
};

ColorRef GetSystemColor(SysColor color);



struct Font
{
	Fl_Font face = FL_HELVETICA;
	Fl_Fontsize size = 14;
	Font() = default;
	Font(Fl_Font face_, Fl_Fontsize size_) : face(face_), size(size_) { }
};


// The font of controls and dialogs
inline Font GetGuiFont() { return Font(FL_HELVETICA, 12); }

// Looks up a font by family name. If the family is not installed, a built-in font is used
// (with fixed character width if isFixedPitch is set).
Font CreateFont(const mpt::ustring &name, int pixelSize, bool isBold = false, bool isItalic = false, bool isFixedPitch = false);

// A font with a fixed character width
inline Font GetFixedFont(int size, bool isBold = false) { return Font(isBold ? FL_COURIER_BOLD : FL_COURIER, size); }


// A pixel buffer that can be drawn by a Painter and edited directly.
class Bitmap
{
public:
	Bitmap() = default;
	Bitmap(int width, int height) { Create(width, height); }

	void Create(int width, int height);
	bool IsValid() const noexcept { return m_width > 0 && m_height > 0; }
	int GetWidth() const noexcept { return m_width; }
	int GetHeight() const noexcept { return m_height; }
	// 0xAARRGGBB per pixel, row 0 is the top row. Alpha is only used if HasAlpha() is set.
	uint32 *GetPixels() noexcept { return m_pixels.data(); }
	const uint32 *GetPixels() const noexcept { return m_pixels.data(); }
	bool HasAlpha() const noexcept { return m_hasAlpha; }
	void SetHasAlpha(bool hasAlpha) noexcept { m_hasAlpha = hasAlpha; }
	ColorRef GetPixel(int x, int y) const noexcept;
	void SetPixel(int x, int y, ColorRef color) noexcept;
	void Fill(ColorRef color) noexcept;
	void FillRect(const Rect &rect, ColorRef color) noexcept;

private:
	int m_width = 0;
	int m_height = 0;
	bool m_hasAlpha = false;
	std::vector<uint32> m_pixels;
};


// A warning sign (yellow triangle with an exclamation mark)
Bitmap CreateWarningIcon(int size);
// An information sign (blue circle with an i)
Bitmap CreateInfoIcon(int size);


class Painter
{
public:
	explicit Painter(Point origin = {});
	~Painter();

	Painter(const Painter &) = delete;
	Painter &operator=(const Painter &) = delete;

	void SetPenColor(ColorRef color) noexcept { m_penColor = color; }
	void SetBrushColor(ColorRef color) noexcept { m_brushColor = color; }
	void SetTextColor(ColorRef color) noexcept { m_textColor = color; }
	void SetBkColor(ColorRef color) noexcept { m_bkColor = color; }
	void SetBkTransparent(bool isTransparent) noexcept { m_isBkTransparent = isTransparent; }
	void SetFont(const Font &font) noexcept { m_font = font; }
	void SetPenWidth(int width) noexcept { m_penWidth = width; }
	void SetPenDotted(bool isDotted) noexcept { m_isPenDotted = isDotted; }
	ColorRef GetTextColor() const noexcept { return m_textColor; }
	ColorRef GetBkColor() const noexcept { return m_bkColor; }
	const Font &GetFont() const noexcept { return m_font; }

	// Calls that keep the shape of the GDI functions they replace
	void FillRect(const Rect *rect, ColorRef color) { FillSolidRect(*rect, color); }
	void DrawFocusRect(const Rect *rect) { DrawFocusRect(*rect); }
	void InvertRect(const Rect *rect) { InvertRect(*rect); }
	int DrawText(const mpt::ustring &text, int, Rect *rect, uint32 format = TextLeft) { return DrawText(text, *rect, format); }
	Font SelectObject(const Font &font) noexcept { const Font previous = m_font; m_font = font; return previous; }
	void SetBkMode(int mode) noexcept { m_isBkTransparent = (mode == TRANSPARENT); }
	void IntersectClipRect(const Rect *rect) { PushClip(*rect); }
	void SetDCPenColor(ColorRef color) noexcept { m_penColor = color; }
	void SetDCBrushColor(ColorRef color) noexcept { m_brushColor = color; }

	void FillSolidRect(const Rect &rect, ColorRef color);
	void FillRect(const Rect &rect, ColorRef color) { FillSolidRect(rect, color); }
	// Whether any part of the rectangle is inside the area that is being drawn
	bool RectVisible(const Rect &rect) const;
	void FillSolidRect(int x, int y, int width, int height, ColorRef color) { FillSolidRect(Rect(x, y, x + width, y + height), color); }
	void FrameRect(const Rect &rect, ColorRef color);
	void Draw3dRect(const Rect &rect, ColorRef topLeft, ColorRef bottomRight);
	// Two-pixel etched line along the chosen sides of the rectangle (see EtchedEdge)
	void DrawEtchedEdge(const Rect &rect, uint32 edges);
	// Two-pixel raised or sunken border; returns the area inside of the border
	Rect DrawBevel(const Rect &rect, bool isRaised, bool isFilled);
	void DrawFocusRect(const Rect &rect);
	void InvertRect(const Rect &rect);
	void Rectangle(const Rect &rect);
	void Rectangle(const Rect *rect) { Rectangle(*rect); }
	void Ellipse(const Rect &rect);
	void Ellipse(const Rect *rect) { Ellipse(*rect); }
	void Polygon(const Point *points, int count);
	void Polyline(const Point *points, int count);

	Point MoveTo(int x, int y) noexcept;
	Point MoveTo(Point pt) noexcept { return MoveTo(pt.x, pt.y); }
	void LineTo(int x, int y);
	void LineTo(Point pt) { LineTo(pt.x, pt.y); }
	void DrawLine(int x1, int y1, int x2, int y2);

	Size GetTextExtent(const mpt::ustring &text) const;
	Size GetTextExtent(const char *text, int length) const;
	int GetTextHeight() const;
	int GetTextAscent() const;
	// Returns the height of the drawn text. With TextCalcRect, only rect is adjusted and nothing is drawn.
	int DrawText(const mpt::ustring &text, Rect &rect, uint32 format = TextLeft);
	int DrawText(const char *text, int length, Rect &rect, uint32 format = TextLeft);
	void TextOut(int x, int y, const mpt::ustring &text);
	void TextOut(int x, int y, const char *text, int length);

	// Copies a part of a bitmap, 1:1 or scaled
	void DrawBitmap(const Bitmap &bitmap, int destX, int destY, int srcX, int srcY, int width, int height);
	void StretchBitmap(const Bitmap &bitmap, const Rect &dest);

	// Copies a part of an offscreen buffer to the target
	void DrawOffscreen(Fl_Offscreen offscreen, int destX, int destY, int width, int height, int srcX, int srcY);

	void PushClip(const Rect &rect);
	void PopClip();
	Rect GetClipBox() const;

	Point GetOrigin() const noexcept { return m_origin; }

private:
	void Apply(ColorRef color) const;
	void ApplyFont() const;

	Point m_origin;
	Point m_position;
	ColorRef m_penColor = RGB(0, 0, 0);
	ColorRef m_brushColor = RGB(255, 255, 255);
	ColorRef m_textColor = RGB(0, 0, 0);
	ColorRef m_bkColor = RGB(255, 255, 255);
	bool m_isBkTransparent = false;
	int m_penWidth = 1;
	bool m_isPenDotted = false;
	Font m_font;
	int m_clipDepth = 0;
};


// Draws into an offscreen buffer instead of the screen for the lifetime of the object
class OffscreenBuffer
{
public:
	OffscreenBuffer() = default;
	OffscreenBuffer(int width, int height) { Create(width, height); }
	~OffscreenBuffer() { Destroy(); }

	OffscreenBuffer(const OffscreenBuffer &) = delete;
	OffscreenBuffer &operator=(const OffscreenBuffer &) = delete;

	void Create(int width, int height);
	void Destroy();
	bool IsValid() const noexcept { return m_offscreen != 0; }
	int GetWidth() const noexcept { return m_width; }
	int GetHeight() const noexcept { return m_height; }
	Fl_Offscreen GetHandle() const noexcept { return m_offscreen; }

	// Copies the content into a bitmap; only valid between Begin() and End()
	Bitmap ReadPixels() const;
	// Redirects all FLTK drawing to this buffer until End() is called
	void Begin();
	void End();

private:
	Fl_Offscreen m_offscreen = 0;
	int m_width = 0;
	int m_height = 0;
};


}  // namespace ui


OPENMPT_NAMESPACE_END
