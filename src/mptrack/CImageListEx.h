/*
 * CImageListEx.h
 * --------------
 * Purpose: A class that extends MFC's CImageList to handle alpha-blended images properly. Also provided 1-bit transparency fallback when needed.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

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
