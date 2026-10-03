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
