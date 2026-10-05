/*
 * CListCtrl.h
 * -----------
 * Purpose: A class that extends MFC's CListCtrl with some more functionality and to handle unicode strings in ANSI builds.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/CListCtrl.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"
#include "ui/ListCtrl.h"


OPENMPT_NAMESPACE_BEGIN

using CListCtrlEx = ui::ListCtrl;
using CMFCListCtrlEx = ui::ListCtrl;

OPENMPT_NAMESPACE_END
