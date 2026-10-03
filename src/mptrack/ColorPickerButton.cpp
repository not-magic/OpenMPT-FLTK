// FLTK port of openmpt/mptrack/ColorPickerButton.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "ColorPickerButton.h"
#include "Sndfile.h"

#include <FL/Fl_Color_Chooser.H>
#include <FL/fl_ask.H>

#include <algorithm>


OPENMPT_NAMESPACE_BEGIN


void ColorPickerButton::SetColor(ColorRef color)
{
	m_color = color;
	SetWindowText(MPT_UFORMAT("Colour: {}% red, {}% green, {}% blue")
		(Util::muldivr(GetRValue(color), 100, 255), Util::muldivr(GetGValue(color), 100, 255), Util::muldivr(GetBValue(color), 100, 255)));
	Invalidate(false);
}


std::optional<ColorRef> ColorPickerButton::PickChannelColor(const CTrackerSoundFile &sndFile, CHANNELINDEX chn)
{
	static std::array<ColorRef, 16> colors = {0};
	// Build a set of currently used channel colors to be displayed in the color picker.
	// Channels that are close to the currently edited channel are preferred.
	std::map<ColorRef, int> usedColors;
	for(CHANNELINDEX i = 0; i < sndFile.GetNumChannels(); i++)
	{
		const auto color = sndFile.GetChannelColor(i);
		if(color == CTrackerSoundFile::INVALID_CHANNEL_COLOR)
			continue;
		const int distance = std::abs(static_cast<int>(i) - chn);
		usedColors[color] = usedColors.count(color) ? std::min(distance, usedColors[color]) : distance;
	}
	return PickColor(colors, usedColors);
}


std::optional<ColorRef> ColorPickerButton::PickPatternColor(const mpt::span<ColorRef> patternColors, PATTERNINDEX pat)
{
	static std::array<ColorRef, 16> colors = {0};
	// Build a set of currently used pattern colors to be displayed in the color picker.
	// Pattern that are close to the currently edited pattern are preferred.
	std::map<ColorRef, int> usedColors;
	for(size_t i = 0; i < patternColors.size(); i++)
	{
		auto color = patternColors[i];
		if(color == CPattern::INVALID_COLOR)
			continue;
		const int distance = std::abs(static_cast<int>(i) - pat);
		usedColors[color] = usedColors.count(color) ? std::min(distance, usedColors[color]) : distance;
	}
	return PickColor(colors, usedColors);
}


std::optional<ColorRef> ColorPickerButton::PickColor(mpt::span<ColorRef> colors, std::map<ColorRef, int> usedColors)
{
	std::vector<std::pair<ColorRef, int>> sortedColors(usedColors.begin(), usedColors.end());
	std::sort(sortedColors.begin(), sortedColors.end(), [](const auto &l, const auto &r) { return l.second < r.second; });

	size_t numColors = std::min(colors.size(), sortedColors.size());
	if(numColors < colors.size())
	{
		// Try to keep as many currently unused colors as possible by shifting them to the end
		std::stable_sort(colors.begin(), colors.end(), [&usedColors](ColorRef l, ColorRef r) { return usedColors.count(l) > usedColors.count(r); });
	}
	auto col = sortedColors.begin();
	for(size_t i = 0; i < numColors; i++)
	{
		colors[i] = (col++)->first;
	}

	uchar red = GetRValue(m_color);
	uchar green = GetGValue(m_color);
	uchar blue = GetBValue(m_color);
	if(fl_color_chooser("Choose a colour", red, green, blue) != 0)
	{
		const ColorRef result = RGB(red, green, blue);
		SetColor(result);
		return result;
	}

	return {};
}


void ColorPickerButton::draw()
{
	ui::Painter painter(Point(x(), y()));
	Rect rect(0, 0, w(), h());
	const bool isPressed = value() != 0;
	painter.Draw3dRect(rect, ui::GetSystemColor(isPressed ? ui::SysColor::ButtonShadow : ui::SysColor::ButtonHighlight), ui::GetSystemColor(isPressed ? ui::SysColor::ButtonHighlight : ui::SysColor::ButtonShadow));
	rect.DeflateRect(1, 1);
	if(m_color == CTrackerSoundFile::INVALID_CHANNEL_COLOR || !active())
		painter.FillSolidRect(rect, ui::GetSystemColor(ui::SysColor::ButtonFace));
	else
		painter.FillSolidRect(rect, m_color);
	if(Fl::focus() == this)
	{
		rect.DeflateRect(1, 1);
		painter.DrawFocusRect(rect);
	}
}


OPENMPT_NAMESPACE_END
