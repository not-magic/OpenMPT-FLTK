/*
 * Geometry.h
 * ----------
 * Purpose: Basic integer geometry and colour value types used by the GUI code.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include <algorithm>


OPENMPT_NAMESPACE_BEGIN


// 0x00BBGGRR
using ColorRef = uint32;

constexpr ColorRef RGB(uint8 r, uint8 g, uint8 b) noexcept
{
	return static_cast<ColorRef>(r) | (static_cast<ColorRef>(g) << 8) | (static_cast<ColorRef>(b) << 16);
}
constexpr uint8 GetRValue(ColorRef c) noexcept { return static_cast<uint8>(c & 0xFF); }
constexpr uint8 GetGValue(ColorRef c) noexcept { return static_cast<uint8>((c >> 8) & 0xFF); }
constexpr uint8 GetBValue(ColorRef c) noexcept { return static_cast<uint8>((c >> 16) & 0xFF); }


struct Size
{
	int cx = 0;
	int cy = 0;

	constexpr Size() noexcept = default;
	constexpr Size(int width, int height) noexcept : cx(width), cy(height) { }
	constexpr bool operator==(const Size &other) const noexcept { return cx == other.cx && cy == other.cy; }
	constexpr bool operator!=(const Size &other) const noexcept { return !(*this == other); }
};


struct Point
{
	int x = 0;
	int y = 0;

	constexpr Point() noexcept = default;
	constexpr Point(int x_, int y_) noexcept : x(x_), y(y_) { }
	constexpr void SetPoint(int x_, int y_) noexcept { x = x_; y = y_; }
	constexpr bool operator==(const Point &other) const noexcept { return x == other.x && y == other.y; }
	constexpr bool operator!=(const Point &other) const noexcept { return !(*this == other); }
	constexpr Point operator+(const Point &other) const noexcept { return {x + other.x, y + other.y}; }
	constexpr Point operator+(const Size &size) const noexcept { return {x + size.cx, y + size.cy}; }
	constexpr Point operator-(const Size &size) const noexcept { return {x - size.cx, y - size.cy}; }
	constexpr Point &operator+=(const Size &size) noexcept { x += size.cx; y += size.cy; return *this; }
	constexpr Point &operator-=(const Size &size) noexcept { x -= size.cx; y -= size.cy; return *this; }
	constexpr Size operator-(const Point &other) const noexcept { return {x - other.x, y - other.y}; }
	void Offset(int dx, int dy) noexcept { x += dx; y += dy; }
};


// Half-open rectangle: right and bottom are exclusive.
struct Rect
{
	int left = 0;
	int top = 0;
	int right = 0;
	int bottom = 0;

	constexpr Rect() noexcept = default;
	constexpr Rect(int left_, int top_, int right_, int bottom_) noexcept : left(left_), top(top_), right(right_), bottom(bottom_) { }
	constexpr Rect(Point topLeft, Point bottomRight) noexcept : left(topLeft.x), top(topLeft.y), right(bottomRight.x), bottom(bottomRight.y) { }
	constexpr Rect(Point topLeft, Size size) noexcept : left(topLeft.x), top(topLeft.y), right(topLeft.x + size.cx), bottom(topLeft.y + size.cy) { }

	constexpr int Width() const noexcept { return right - left; }
	constexpr int Height() const noexcept { return bottom - top; }
	constexpr Size Dimensions() const noexcept { return {Width(), Height()}; }
	constexpr Point TopLeft() const noexcept { return {left, top}; }
	constexpr Point BottomRight() const noexcept { return {right, bottom}; }
	constexpr Point CenterPoint() const noexcept { return {(left + right) / 2, (top + bottom) / 2}; }
	constexpr bool IsRectEmpty() const noexcept { return left >= right || top >= bottom; }
	constexpr bool PtInRect(Point pt) const noexcept { return pt.x >= left && pt.x < right && pt.y >= top && pt.y < bottom; }
	constexpr bool operator==(const Rect &other) const noexcept { return left == other.left && top == other.top && right == other.right && bottom == other.bottom; }
	constexpr bool operator!=(const Rect &other) const noexcept { return !(*this == other); }

	void SetRect(int left_, int top_, int right_, int bottom_) noexcept { left = left_; top = top_; right = right_; bottom = bottom_; }
	void SetRectEmpty() noexcept { left = top = right = bottom = 0; }
	void MoveToX(int x) noexcept { right = x + Width(); left = x; }
	void MoveToY(int y) noexcept { bottom = y + Height(); top = y; }
	void MoveToXY(int x, int y) noexcept { MoveToX(x); MoveToY(y); }
	void OffsetRect(int dx, int dy) noexcept { left += dx; right += dx; top += dy; bottom += dy; }
	void OffsetRect(Point pt) noexcept { OffsetRect(pt.x, pt.y); }
	void InflateRect(int dx, int dy) noexcept { left -= dx; right += dx; top -= dy; bottom += dy; }
	void InflateRect(int l, int t, int r, int b) noexcept { left -= l; top -= t; right += r; bottom += b; }
	void DeflateRect(int dx, int dy) noexcept { InflateRect(-dx, -dy); }
	void DeflateRect(int l, int t, int r, int b) noexcept { InflateRect(-l, -t, -r, -b); }
	void NormalizeRect() noexcept
	{
		if(left > right) std::swap(left, right);
		if(top > bottom) std::swap(top, bottom);
	}
	Rect &operator&=(const Rect &other) noexcept { IntersectRect(*this, other); return *this; }
	Rect operator|(const Rect &other) const noexcept { Rect result; result.UnionRect(*this, other); return result; }
	bool IntersectRect(const Rect &a, const Rect &b) noexcept
	{
		left = std::max(a.left, b.left);
		top = std::max(a.top, b.top);
		right = std::min(a.right, b.right);
		bottom = std::min(a.bottom, b.bottom);
		if(IsRectEmpty())
		{
			SetRectEmpty();
			return false;
		}
		return true;
	}
	// Removes b from a if what remains is a rectangle, otherwise the result is a
	bool SubtractRect(const Rect &a, const Rect &b) noexcept
	{
		*this = a;
		Rect intersection;
		if(!intersection.IntersectRect(a, b))
			return !IsRectEmpty();
		if(intersection.top <= a.top && intersection.bottom >= a.bottom)
		{
			if(intersection.left <= a.left) left = intersection.right;
			else if(intersection.right >= a.right) right = intersection.left;
		} else if(intersection.left <= a.left && intersection.right >= a.right)
		{
			if(intersection.top <= a.top) top = intersection.bottom;
			else if(intersection.bottom >= a.bottom) bottom = intersection.top;
		}
		return !IsRectEmpty();
	}
	void UnionRect(const Rect &a, const Rect &b) noexcept
	{
		left = std::min(a.left, b.left);
		top = std::min(a.top, b.top);
		right = std::max(a.right, b.right);
		bottom = std::max(a.bottom, b.bottom);
	}
};


// Names of the types the code was written with
using RECT = Rect;
using POINT = Point;
using SIZE = Size;

OPENMPT_NAMESPACE_END
