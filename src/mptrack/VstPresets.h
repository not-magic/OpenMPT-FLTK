// FLTK port of openmpt/mptrack/VstPresets.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include <iosfwd>
#include "../common/FileReaderFwd.h"

OPENMPT_NAMESPACE_BEGIN

class IMixPlugin;

class VSTPresets
{
public:
	enum ErrorCode
	{
		noError,
		invalidFile,
		wrongPlugin,
		wrongParameters,
		outOfMemory,
	};

	static ErrorCode LoadFile(FileReader &file, IMixPlugin &plugin);
	static bool SaveFile(std::ostream &, IMixPlugin &plugin, bool bank);
	static const char *GetErrorMessage(ErrorCode code);

protected:
	static void SaveProgram(std::ostream &f, IMixPlugin &plugin);

};

OPENMPT_NAMESPACE_END
