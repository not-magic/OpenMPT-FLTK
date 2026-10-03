// Program entry point, replacing the MFC application startup of openmpt/mptrack/Mptrack.cpp.

#include "stdafx.h"
#include "ui/Ui.h"
#include "Mptrack.h"


OPENMPT_NAMESPACE_BEGIN

MPT_ATTR_NOINLINE MPT_DECL_NOINLINE void AssertHandler(const mpt::source_location &loc, const char *expr, const char *msg)
{
	std::fprintf(stderr, "%s(%u): %s [%s]\n", loc.file_name() ? loc.file_name() : "", static_cast<unsigned>(loc.line()), msg ? msg : (std::string("ASSERT(") + expr + ") failed").c_str(), loc.function_name() ? loc.function_name() : "");
	if(!msg)
		std::abort();
}

OPENMPT_NAMESPACE_END


int main(int argc, char **argv)
{
	OPENMPT_NAMESPACE::ui::AppBase &app = OPENMPT_NAMESPACE::theApp;
	app.SetCommandLine(argc, argv);
	// Enables Fl::awake, which delivers messages posted during initialization
	Fl::lock();
	if(!app.InitInstance())
		return 1;
	const int runResult = app.Run();
	const int exitResult = app.ExitInstance();
	return runResult != 0 ? runResult : exitResult;
}
