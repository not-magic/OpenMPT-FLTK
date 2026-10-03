/*
 * CImageListEx.h
 * --------------
 * Purpose: Image list that loads its images from a PNG resource, with optional recolouring.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


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
