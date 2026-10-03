// FLTK port of openmpt/mptrack/CImageListEx.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"
#include "mpt/base/span.hpp"
#include "ui/ImageList.h"

OPENMPT_NAMESPACE_BEGIN

class CImageListEx : public ImageList
{
public:
	bool Create(uint32 resourceID, int cx, int cy, double scaling, bool disabled, const mpt::span<const int> invertImages = {});
};

OPENMPT_NAMESPACE_END
