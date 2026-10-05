/*
 * ColorPickerButton.h
 * -------------------
 * Purpose: A button for picking UI colors
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/ColorPickerButton.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"
#include "../soundlib/Snd_defs.h"

OPENMPT_NAMESPACE_BEGIN

class CTrackerSoundFile;

class ColorPickerButton : public Button
{
public:
	void SetColor(ColorRef color);
	std::optional<ColorRef> PickChannelColor(const CTrackerSoundFile &sndFile, CHANNELINDEX chn);
	std::optional<ColorRef> PickPatternColor(const mpt::span<ColorRef> patternColors, PATTERNINDEX pat);

protected:
	void draw() override;

	std::optional<ColorRef> PickColor(mpt::span<ColorRef> colors, std::map<ColorRef, int> usedColors);

	ColorRef m_color = 0;
};

OPENMPT_NAMESPACE_END
