/*
 * PluginInfo.h
 * ------------
 * Purpose: Parameter and program names of the built-in plugins, which upstream only compiles into the tracker build.
 * Notes  : Values are derived from the public, normalized parameter values.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../soundlib/Snd_defs.h"
#include "../soundlib/plugins/PluginStructs.h"

#include <optional>
#include <utility>

OPENMPT_NAMESPACE_BEGIN

class IMixPlugin;

namespace PluginInfo
{

// All of these return std::nullopt for plugins they do not know about
std::optional<mpt::ustring> FindDefaultEffectName(const IMixPlugin &plugin);
std::optional<mpt::ustring> FindParamName(IMixPlugin &plugin, PlugParamIndex paramIndex);
std::optional<mpt::ustring> FindParamLabel(IMixPlugin &plugin, PlugParamIndex paramIndex);
std::optional<mpt::ustring> FindParamDisplay(IMixPlugin &plugin, PlugParamIndex paramIndex);
std::optional<std::pair<PlugParamValue, PlugParamValue>> FindParamUIRange(IMixPlugin &plugin, PlugParamIndex paramIndex);

// Programs that libopenmpt does not expose, such as the I3DL2Reverb presets
int32 FindNumPrograms(const IMixPlugin &plugin);
mpt::ustring FindProgramName(const IMixPlugin &plugin, int32 programIndex);
bool SetProgram(IMixPlugin &plugin, int32 programIndex);

}  // namespace PluginInfo

OPENMPT_NAMESPACE_END
