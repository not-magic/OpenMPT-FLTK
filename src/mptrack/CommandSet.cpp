// FLTK port of openmpt/mptrack/CommandSet.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "CommandSet.h"
#include "DefaultKeyBindings.h"
#include "resource.h"
#include "Mptrack.h"	// For ErrorBox
#include "../soundlib/mod_specifications.h"
#include "../soundlib/Tables.h"
#include "../mptrack/Reporting.h"
#include "mpt/io_file/fstream.hpp"
#include "mpt/io_file/outputfile.hpp"
#include "../common/mptFileIO.h"
#include <sstream>
#include "TrackerSettings.h"
#include "mpt/parse/parse.hpp"
#include "mpt/string/utility.hpp"


OPENMPT_NAMESPACE_BEGIN

namespace
{

constexpr CommandID ModifierCommands[] =
{
	kcSelect, kcCopySelect, kcChordModifier, kcSetSpacing
};

constexpr std::pair<CommandID, CommandID> NoteRanges[] =
{
	{kcVPStartNotes, kcVPStartNoteStops},
	{kcSampStartNotes, kcSampStartNoteStops},
	{kcInstrumentStartNotes, kcInstrumentStartNoteStops},
	{kcTreeViewStartNotes, kcTreeViewStartNoteStops},
	{kcInsNoteMapStartNotes, kcInsNoteMapStartNoteStops},
	{kcVSTGUIStartNotes, kcVSTGUIStartNoteStops},
	{kcCommentsStartNotes, kcCommentsStartNoteStops},
};

};  // namespace

#ifdef MPT_ALL_LOGGING
#define MPT_COMMANDSET_LOGGING
#endif

#ifdef MPT_COMMANDSET_LOGGING
#define LOG_COMMANDSET(x) MPT_LOG_GLOBAL(LogDebug, "CommandSet", (x))
#else
#define LOG_COMMANDSET(x) do { } while(0)
#endif


CCommandSet::CCommandSet()
{
	// Which key binding rules to enforce?
	m_enforceRule[krPreventDuplicate]             = true;
	m_enforceRule[krDeleteOldOnConflict]          = true;
	m_enforceRule[krAllowNavigationWithSelection] = true;
	m_enforceRule[krAllowSelectionWithNavigation] = true;
	m_enforceRule[krAllowSelectCopySelectCombos]  = true;
	m_enforceRule[krLockNotesToChords]            = true;
	m_enforceRule[krNoteOffOnKeyRelease]          = true;
	m_enforceRule[krPropagateNotes]               = true;
	m_enforceRule[krReassignDigitsToOctaves]      = false;
	m_enforceRule[krAutoSelectOff]                = true;
	m_enforceRule[krAutoSpacing]                  = true;
	m_enforceRule[krCheckModifiers]               = true;
	m_enforceRule[krPropagateSampleManipulation]  = true;
//	enforceRule[krCheckContextHierarchy]          = true;

	SetupCommands();
	SetupContextHierarchy();
}


// Setup

KeyCommand::KeyCommand(uint32 uid, const mpt::ustring &commandName, std::vector<KeyCombination> keys)
    : kcList{std::move(keys)}
    , name{commandName}
    , UID{uid}
{
}

static constexpr struct
{
	uint32 uid = 0;  // ID | Hidden | Dummy
	CommandID cmd = kcNull;
	const mpt::uchar *description = nullptr;
} CommandDefinitions[] =
// clang-format off
{
	{KeyCommand::Dummy, kcNull, UL_("")},
	{1001, kcPatternRecord, UL_("Enable Recording")},
	{1002, kcPatternPlayRow, UL_("Play Row")},
	{1003, kcCursorCopy, UL_("Quick Copy")},
	{1004, kcCursorPaste, UL_("Quick Paste")},
	{1005, kcChannelMute, UL_("Mute Current Channel")},
	{1006, kcChannelSolo, UL_("Solo Current Channel")},
	{1007, kcTransposeUp, UL_("Transpose +1")},
	{1008, kcTransposeDown, UL_("Transpose -1")},
	{1009, kcTransposeOctUp, UL_("Transpose +1 Octave")},
	{1010, kcTransposeOctDown, UL_("Transpose -1 Octave")},
	{1011, kcSelectChannel, UL_("Select Channel / Select All")},
	{1012, kcPatternAmplify, UL_("Amplify Selection")},
	{1013, kcPatternSetInstrument, UL_("Apply current instrument")},
	{1014, kcPatternInterpolateVol, UL_("Interpolate Volume")},
	{1015, kcPatternInterpolateEffect, UL_("Interpolate Effect")},
	{1016, kcPatternVisualizeEffect, UL_("Open Effect Visualizer")},
	{1017, kcPatternJumpDownh1, UL_("Jump down by measure")},
	{1018, kcPatternJumpUph1, UL_("Jump up by measure")},
	{1019, kcPatternSnapDownh1, UL_("Snap down to measure")},
	{1020, kcPatternSnapUph1, UL_("Snap up to measure")},
	{1021, kcViewGeneral, UL_("View General")},
	{1022, kcViewPattern, UL_("View Pattern")},
	{1023, kcViewSamples, UL_("View Samples")},
	{1024, kcViewInstruments, UL_("View Instruments")},
	{1025, kcViewComments, UL_("View Comments")},
	{1026, kcPlayPatternFromCursor, UL_("Play Pattern from Cursor")},
	{1027, kcPlayPatternFromStart, UL_("Play Pattern from Start")},
	{1028, kcPlaySongFromCursor, UL_("Play Song from Cursor")},
	{1029, kcPlaySongFromStart, UL_("Play Song from Start")},
	{1030, kcPlayPauseSong, UL_("Play Song / Pause Song")},
	{1031, kcPauseSong, UL_("Pause Song")},
	{1032, kcPrevInstrument, UL_("Previous Instrument")},
	{1033, kcNextInstrument, UL_("Next Instrument")},
	{1034, kcPrevOrder, UL_("Previous Order")},
	{1035, kcNextOrder, UL_("Next Order")},
	{1036, kcPrevOctave, UL_("Previous Octave")},
	{1037, kcNextOctave, UL_("Next Octave")},
	{1038, kcNavigateDown, UL_("Navigate down by 1 row")},
	{1039, kcNavigateUp, UL_("Navigate up by 1 row")},
	{1040, kcNavigateLeft, UL_("Navigate left")},
	{1041, kcNavigateRight, UL_("Navigate right")},
	{1042, kcNavigateNextChan, UL_("Navigate to next channel")},
	{1043, kcNavigatePrevChan, UL_("Navigate to previous channel")},
	{1044, kcHomeHorizontal, UL_("Go to first channel")},
	{1045, kcHomeVertical, UL_("Go to first row")},
	{1046, kcHomeAbsolute, UL_("Go to first row of first channel")},
	{1047, kcEndHorizontal, UL_("Go to last channel")},
	{1048, kcEndVertical, UL_("Go to last row")},
	{1049, kcEndAbsolute, UL_("Go to last row of last channel")},
	{1050, kcSelect, UL_("Selection key")},
	{1051, kcCopySelect, UL_("Copy select key")},
	{KeyCommand::Hidden, kcSelectOff, UL_("Deselect")},
	{KeyCommand::Hidden, kcCopySelectOff, UL_("Copy deselect key")},
	{1054, kcNextPattern, UL_("Next Pattern")},
	{1055, kcPrevPattern, UL_("Previous Pattern")},
	//{1056, kcClearSelection, UL_("Wipe selection")},
	{1057, kcClearRow, UL_("Clear Row")},
	{1058, kcClearField, UL_("Clear Field")},
	{1059, kcClearRowStep, UL_("Clear Row and Step")},
	{1060, kcClearFieldStep, UL_("Clear Field and Step")},
	{1061, kcDeleteRow, UL_("Delete Row(s)")},
	{1062, kcShowNoteProperties, UL_("Show Note Properties")},
	{1063, kcShowEditMenu, UL_("Show Context (Right-Click) Menu")},
	{1064, kcVPNoteC_0, UL_("Base octave C")},
	{1065, kcVPNoteCS0, UL_("Base octave C#")},
	{1066, kcVPNoteD_0, UL_("Base octave D")},
	{1067, kcVPNoteDS0, UL_("Base octave D#")},
	{1068, kcVPNoteE_0, UL_("Base octave E")},
	{1069, kcVPNoteF_0, UL_("Base octave F")},
	{1070, kcVPNoteFS0, UL_("Base octave F#")},
	{1071, kcVPNoteG_0, UL_("Base octave G")},
	{1072, kcVPNoteGS0, UL_("Base octave G#")},
	{1073, kcVPNoteA_1, UL_("Base octave A")},
	{1074, kcVPNoteAS1, UL_("Base octave A#")},
	{1075, kcVPNoteB_1, UL_("Base octave B")},
	{1076, kcVPNoteC_1, UL_("Base octave +1 C")},
	{1077, kcVPNoteCS1, UL_("Base octave +1 C#")},
	{1078, kcVPNoteD_1, UL_("Base octave +1 D")},
	{1079, kcVPNoteDS1, UL_("Base octave +1 D#")},
	{1080, kcVPNoteE_1, UL_("Base octave +1 E")},
	{1081, kcVPNoteF_1, UL_("Base octave +1 F")},
	{1082, kcVPNoteFS1, UL_("Base octave +1 F#")},
	{1083, kcVPNoteG_1, UL_("Base octave +1 G")},
	{1084, kcVPNoteGS1, UL_("Base octave +1 G#")},
	{1085, kcVPNoteA_2, UL_("Base octave +1 A")},
	{1086, kcVPNoteAS2, UL_("Base octave +1 A#")},
	{1087, kcVPNoteB_2, UL_("Base octave +1 B")},
	{1088, kcVPNoteC_2, UL_("Base octave +2 C")},
	{1089, kcVPNoteCS2, UL_("Base octave +2 C#")},
	{1090, kcVPNoteD_2, UL_("Base octave +2 D")},
	{1091, kcVPNoteDS2, UL_("Base octave +2 D#")},
	{1092, kcVPNoteE_2, UL_("Base octave +2 E")},
	{1093, kcVPNoteF_2, UL_("Base octave +2 F")},
	{1094, kcVPNoteFS2, UL_("Base octave +2 F#")},
	{1095, kcVPNoteG_2, UL_("Base octave +2 G")},
	{1096, kcVPNoteGS2, UL_("Base octave +2 G#")},
	{1097, kcVPNoteA_3, UL_("Base octave +2 A")},
	{2070, kcVPNoteAS3, UL_("Base octave +2 A#")},
	{2071, kcVPNoteB_3, UL_("Base octave +2 B")},
	{2072, kcVPNoteC_3, UL_("Base octave +3 C")},
	{2073, kcVPNoteCS3, UL_("Base octave +3 C#")},
	{2074, kcVPNoteD_3, UL_("Base octave +3 D")},
	{2075, kcVPNoteDS3, UL_("Base octave +3 D#")},
	{2076, kcVPNoteE_3, UL_("Base octave +3 E")},
	{2077, kcVPNoteF_3, UL_("Base octave +3 F")},
	{2078, kcVPNoteFS3, UL_("Base octave +3 F#")},
	{2079, kcVPNoteG_3, UL_("Base octave +3 G")},
	{2080, kcVPNoteGS3, UL_("Base octave +3 G#")},
	{2081, kcVPNoteA_4, UL_("Base octave +3 A")},
	{2082, kcVPNoteAS4, UL_("Base octave +3 A#")},
	{2083, kcVPNoteB_4, UL_("Base octave +3 B")},
	{2084, kcVPNoteC_4, UL_("Base octave +4 C")},
	{2085, kcVPNoteCS4, UL_("Base octave +4 C#")},
	{2086, kcVPNoteD_4, UL_("Base octave +4 D")},
	{2087, kcVPNoteDS4, UL_("Base octave +4 D#")},
	{2088, kcVPNoteE_4, UL_("Base octave +4 E")},
	{2089, kcVPNoteF_4, UL_("Base octave +4 F")},
	{2090, kcVPNoteFS4, UL_("Base octave +4 F#")},
	{2091, kcVPNoteG_4, UL_("Base octave +4 G")},
	{2092, kcVPNoteGS4, UL_("Base octave +4 G#")},
	{2093, kcVPNoteA_5, UL_("Base octave +4 A")},
	{2094, kcVPNoteAS5, UL_("Base octave +4 A#")},
	{2095, kcVPNoteB_5, UL_("Base octave +4 B")},
	{KeyCommand::Hidden, kcVPNoteStopC_0, UL_("Stop base octave C")},
	{KeyCommand::Hidden, kcVPNoteStopCS0, UL_("Stop base octave C#")},
	{KeyCommand::Hidden, kcVPNoteStopD_0, UL_("Stop base octave D")},
	{KeyCommand::Hidden, kcVPNoteStopDS0, UL_("Stop base octave D#")},
	{KeyCommand::Hidden, kcVPNoteStopE_0, UL_("Stop base octave E")},
	{KeyCommand::Hidden, kcVPNoteStopF_0, UL_("Stop base octave F")},
	{KeyCommand::Hidden, kcVPNoteStopFS0, UL_("Stop base octave F#")},
	{KeyCommand::Hidden, kcVPNoteStopG_0, UL_("Stop base octave G")},
	{KeyCommand::Hidden, kcVPNoteStopGS0, UL_("Stop base octave G#")},
	{KeyCommand::Hidden, kcVPNoteStopA_1, UL_("Stop base octave A")},
	{KeyCommand::Hidden, kcVPNoteStopAS1, UL_("Stop base octave A#")},
	{KeyCommand::Hidden, kcVPNoteStopB_1, UL_("Stop base octave B")},
	{KeyCommand::Hidden, kcVPNoteStopC_1, UL_("Stop base octave +1 C")},
	{KeyCommand::Hidden, kcVPNoteStopCS1, UL_("Stop base octave +1 C#")},
	{KeyCommand::Hidden, kcVPNoteStopD_1, UL_("Stop base octave +1 D")},
	{KeyCommand::Hidden, kcVPNoteStopDS1, UL_("Stop base octave +1 D#")},
	{KeyCommand::Hidden, kcVPNoteStopE_1, UL_("Stop base octave +1 E")},
	{KeyCommand::Hidden, kcVPNoteStopF_1, UL_("Stop base octave +1 F")},
	{KeyCommand::Hidden, kcVPNoteStopFS1, UL_("Stop base octave +1 F#")},
	{KeyCommand::Hidden, kcVPNoteStopG_1, UL_("Stop base octave +1 G")},
	{KeyCommand::Hidden, kcVPNoteStopGS1, UL_("Stop base octave +1 G#")},
	{KeyCommand::Hidden, kcVPNoteStopA_2, UL_("Stop base octave +1 A")},
	{KeyCommand::Hidden, kcVPNoteStopAS2, UL_("Stop base octave +1 A#")},
	{KeyCommand::Hidden, kcVPNoteStopB_2, UL_("Stop base octave +1 B")},
	{KeyCommand::Hidden, kcVPNoteStopC_2, UL_("Stop base octave +2 C")},
	{KeyCommand::Hidden, kcVPNoteStopCS2, UL_("Stop base octave +2 C#")},
	{KeyCommand::Hidden, kcVPNoteStopD_2, UL_("Stop base octave +2 D")},
	{KeyCommand::Hidden, kcVPNoteStopDS2, UL_("Stop base octave +2 D#")},
	{KeyCommand::Hidden, kcVPNoteStopE_2, UL_("Stop base octave +2 E")},
	{KeyCommand::Hidden, kcVPNoteStopF_2, UL_("Stop base octave +2 F")},
	{KeyCommand::Hidden, kcVPNoteStopFS2, UL_("Stop base octave +2 F#")},
	{KeyCommand::Hidden, kcVPNoteStopG_2, UL_("Stop base octave +2 G")},
	{KeyCommand::Hidden, kcVPNoteStopGS2, UL_("Stop base octave +2 G#")},
	{KeyCommand::Hidden, kcVPNoteStopA_3, UL_("Stop base octave +2 A")},
	{KeyCommand::Hidden, kcVPNoteStopAS3, UL_("Stop base octave +2 A#")},
	{KeyCommand::Hidden, kcVPNoteStopB_3, UL_("Stop base octave +2 B")},
	{KeyCommand::Hidden, kcVPNoteStopC_3, UL_("Stop base octave +3 C")},
	{KeyCommand::Hidden, kcVPNoteStopCS3, UL_("Stop base octave +3 C#")},
	{KeyCommand::Hidden, kcVPNoteStopD_3, UL_("Stop base octave +3 D")},
	{KeyCommand::Hidden, kcVPNoteStopDS3, UL_("Stop base octave +3 D#")},
	{KeyCommand::Hidden, kcVPNoteStopE_3, UL_("Stop base octave +3 E")},
	{KeyCommand::Hidden, kcVPNoteStopF_3, UL_("Stop base octave +3 F")},
	{KeyCommand::Hidden, kcVPNoteStopFS3, UL_("Stop base octave +3 F#")},
	{KeyCommand::Hidden, kcVPNoteStopG_3, UL_("Stop base octave +3 G")},
	{KeyCommand::Hidden, kcVPNoteStopGS3, UL_("Stop base octave +3 G#")},
	{KeyCommand::Hidden, kcVPNoteStopA_4, UL_("Stop base octave +3 A")},
	{KeyCommand::Hidden, kcVPNoteStopAS4, UL_("Stop base octave +3 A#")},
	{KeyCommand::Hidden, kcVPNoteStopB_4, UL_("Stop base octave +3 B")},
	{KeyCommand::Hidden, kcVPNoteStopC_4, UL_("Stop base octave +4 C")},
	{KeyCommand::Hidden, kcVPNoteStopCS4, UL_("Stop base octave +4 C#")},
	{KeyCommand::Hidden, kcVPNoteStopD_4, UL_("Stop base octave +4 D")},
	{KeyCommand::Hidden, kcVPNoteStopDS4, UL_("Stop base octave +4 D#")},
	{KeyCommand::Hidden, kcVPNoteStopE_4, UL_("Stop base octave +4 E")},
	{KeyCommand::Hidden, kcVPNoteStopF_4, UL_("Stop base octave +4 F")},
	{KeyCommand::Hidden, kcVPNoteStopFS4, UL_("Stop base octave +4 F#")},
	{KeyCommand::Hidden, kcVPNoteStopG_4, UL_("Stop base octave +4 G")},
	{KeyCommand::Hidden, kcVPNoteStopGS4, UL_("Stop base octave +4 G#")},
	{KeyCommand::Hidden, kcVPNoteStopA_5, UL_("Stop base octave +4 A")},
	{KeyCommand::Hidden, kcVPNoteStopAS5, UL_("Stop base octave +4 A#")},
	{KeyCommand::Hidden, kcVPNoteStopB_5, UL_("Stop base octave +4 B")},
	{KeyCommand::Hidden, kcVPChordC_0, UL_("Base octave chord C")},
	{KeyCommand::Hidden, kcVPChordCS0, UL_("Base octave chord C#")},
	{KeyCommand::Hidden, kcVPChordD_0, UL_("Base octave chord D")},
	{KeyCommand::Hidden, kcVPChordDS0, UL_("Base octave chord D#")},
	{KeyCommand::Hidden, kcVPChordE_0, UL_("Base octave chord E")},
	{KeyCommand::Hidden, kcVPChordF_0, UL_("Base octave chord F")},
	{KeyCommand::Hidden, kcVPChordFS0, UL_("Base octave chord F#")},
	{KeyCommand::Hidden, kcVPChordG_0, UL_("Base octave chord G")},
	{KeyCommand::Hidden, kcVPChordGS0, UL_("Base octave chord G#")},
	{KeyCommand::Hidden, kcVPChordA_1, UL_("Base octave chord A")},
	{KeyCommand::Hidden, kcVPChordAS1, UL_("Base octave chord A#")},
	{KeyCommand::Hidden, kcVPChordB_1, UL_("Base octave chord B")},
	{KeyCommand::Hidden, kcVPChordC_1, UL_("Base octave +1 chord C")},
	{KeyCommand::Hidden, kcVPChordCS1, UL_("Base octave +1 chord C#")},
	{KeyCommand::Hidden, kcVPChordD_1, UL_("Base octave +1 chord D")},
	{KeyCommand::Hidden, kcVPChordDS1, UL_("Base octave +1 chord D#")},
	{KeyCommand::Hidden, kcVPChordE_1, UL_("Base octave +1 chord E")},
	{KeyCommand::Hidden, kcVPChordF_1, UL_("Base octave +1 chord F")},
	{KeyCommand::Hidden, kcVPChordFS1, UL_("Base octave +1 chord F#")},
	{KeyCommand::Hidden, kcVPChordG_1, UL_("Base octave +1 chord G")},
	{KeyCommand::Hidden, kcVPChordGS1, UL_("Base octave +1 chord G#")},
	{KeyCommand::Hidden, kcVPChordA_2, UL_("Base octave +1 chord A")},
	{KeyCommand::Hidden, kcVPChordAS2, UL_("Base octave +1 chord A#")},
	{KeyCommand::Hidden, kcVPChordB_2, UL_("Base octave +1 chord B")},
	{KeyCommand::Hidden, kcVPChordC_2, UL_("Base octave +2 chord C")},
	{KeyCommand::Hidden, kcVPChordCS2, UL_("Base octave +2 chord C#")},
	{KeyCommand::Hidden, kcVPChordD_2, UL_("Base octave +2 chord D")},
	{KeyCommand::Hidden, kcVPChordDS2, UL_("Base octave +2 chord D#")},
	{KeyCommand::Hidden, kcVPChordE_2, UL_("Base octave +2 chord E")},
	{KeyCommand::Hidden, kcVPChordF_2, UL_("Base octave +2 chord F")},
	{KeyCommand::Hidden, kcVPChordFS2, UL_("Base octave +2 chord F#")},
	{KeyCommand::Hidden, kcVPChordG_2, UL_("Base octave +2 chord G")},
	{KeyCommand::Hidden, kcVPChordGS2, UL_("Base octave +2 chord G#")},
	{KeyCommand::Hidden, kcVPChordA_3, UL_("Base octave +2 chord A")},
	{KeyCommand::Hidden, kcVPChordAS3, UL_("Base octave +2 chord A#")},
	{KeyCommand::Hidden, kcVPChordB_3, UL_("Base octave +2 chord B")},
	{KeyCommand::Hidden, kcVPChordC_3, UL_("Base octave +3 chord C")},
	{KeyCommand::Hidden, kcVPChordCS3, UL_("Base octave +3 chord C#")},
	{KeyCommand::Hidden, kcVPChordD_3, UL_("Base octave +3 chord D")},
	{KeyCommand::Hidden, kcVPChordDS3, UL_("Base octave +3 chord D#")},
	{KeyCommand::Hidden, kcVPChordE_3, UL_("Base octave +3 chord E")},
	{KeyCommand::Hidden, kcVPChordF_3, UL_("Base octave +3 chord F")},
	{KeyCommand::Hidden, kcVPChordFS3, UL_("Base octave +3 chord F#")},
	{KeyCommand::Hidden, kcVPChordG_3, UL_("Base octave +3 chord G")},
	{KeyCommand::Hidden, kcVPChordGS3, UL_("Base octave +3 chord G#")},
	{KeyCommand::Hidden, kcVPChordA_4, UL_("Base octave +3 chord A")},
	{KeyCommand::Hidden, kcVPChordAS4, UL_("Base octave +3 chord A#")},
	{KeyCommand::Hidden, kcVPChordB_4, UL_("Base octave +3 chord B")},
	{KeyCommand::Hidden, kcVPChordC_4, UL_("Base octave +4 chord C")},
	{KeyCommand::Hidden, kcVPChordCS4, UL_("Base octave +4 chord C#")},
	{KeyCommand::Hidden, kcVPChordD_4, UL_("Base octave +4 chord D")},
	{KeyCommand::Hidden, kcVPChordDS4, UL_("Base octave +4 chord D#")},
	{KeyCommand::Hidden, kcVPChordE_4, UL_("Base octave +4 chord E")},
	{KeyCommand::Hidden, kcVPChordF_4, UL_("Base octave +4 chord F")},
	{KeyCommand::Hidden, kcVPChordFS4, UL_("Base octave +4 chord F#")},
	{KeyCommand::Hidden, kcVPChordG_4, UL_("Base octave +4 chord G")},
	{KeyCommand::Hidden, kcVPChordGS4, UL_("Base octave +4 chord G#")},
	{KeyCommand::Hidden, kcVPChordA_5, UL_("Base octave +4 chord A")},
	{KeyCommand::Hidden, kcVPChordAS5, UL_("Base octave +4 chord A#")},
	{KeyCommand::Hidden, kcVPChordB_5, UL_("Base octave +4 chord B")},
	{KeyCommand::Hidden, kcVPChordStopC_0, UL_("Stop base octave chord C")},
	{KeyCommand::Hidden, kcVPChordStopCS0, UL_("Stop base octave chord C#")},
	{KeyCommand::Hidden, kcVPChordStopD_0, UL_("Stop base octave chord D")},
	{KeyCommand::Hidden, kcVPChordStopDS0, UL_("Stop base octave chord D#")},
	{KeyCommand::Hidden, kcVPChordStopE_0, UL_("Stop base octave chord E")},
	{KeyCommand::Hidden, kcVPChordStopF_0, UL_("Stop base octave chord F")},
	{KeyCommand::Hidden, kcVPChordStopFS0, UL_("Stop base octave chord F#")},
	{KeyCommand::Hidden, kcVPChordStopG_0, UL_("Stop base octave chord G")},
	{KeyCommand::Hidden, kcVPChordStopGS0, UL_("Stop base octave chord G#")},
	{KeyCommand::Hidden, kcVPChordStopA_1, UL_("Stop base octave chord A")},
	{KeyCommand::Hidden, kcVPChordStopAS1, UL_("Stop base octave chord A#")},
	{KeyCommand::Hidden, kcVPChordStopB_1, UL_("Stop base octave chord B")},
	{KeyCommand::Hidden, kcVPChordStopC_1, UL_("Stop base octave +1 chord C")},
	{KeyCommand::Hidden, kcVPChordStopCS1, UL_("Stop base octave +1 chord C#")},
	{KeyCommand::Hidden, kcVPChordStopD_1, UL_("Stop base octave +1 chord D")},
	{KeyCommand::Hidden, kcVPChordStopDS1, UL_("Stop base octave +1 chord D#")},
	{KeyCommand::Hidden, kcVPChordStopE_1, UL_("Stop base octave +1 chord E")},
	{KeyCommand::Hidden, kcVPChordStopF_1, UL_("Stop base octave +1 chord F")},
	{KeyCommand::Hidden, kcVPChordStopFS1, UL_("Stop base octave +1 chord F#")},
	{KeyCommand::Hidden, kcVPChordStopG_1, UL_("Stop base octave +1 chord G")},
	{KeyCommand::Hidden, kcVPChordStopGS1, UL_("Stop base octave +1 chord G#")},
	{KeyCommand::Hidden, kcVPChordStopA_2, UL_("Stop base octave +1 chord A")},
	{KeyCommand::Hidden, kcVPChordStopAS2, UL_("Stop base octave +1 chord A#")},
	{KeyCommand::Hidden, kcVPChordStopB_2, UL_("Stop base octave +1 chord B")},
	{KeyCommand::Hidden, kcVPChordStopC_2, UL_("Stop base octave +2 chord C")},
	{KeyCommand::Hidden, kcVPChordStopCS2, UL_("Stop base octave +2 chord C#")},
	{KeyCommand::Hidden, kcVPChordStopD_2, UL_("Stop base octave +2 chord D")},
	{KeyCommand::Hidden, kcVPChordStopDS2, UL_("Stop base octave +2 chord D#")},
	{KeyCommand::Hidden, kcVPChordStopE_2, UL_("Stop base octave +2 chord E")},
	{KeyCommand::Hidden, kcVPChordStopF_2, UL_("Stop base octave +2 chord F")},
	{KeyCommand::Hidden, kcVPChordStopFS2, UL_("Stop base octave +2 chord F#")},
	{KeyCommand::Hidden, kcVPChordStopG_2, UL_("Stop base octave +2 chord G")},
	{KeyCommand::Hidden, kcVPChordStopGS2, UL_("Stop base octave +2 chord G#")},
	{KeyCommand::Hidden, kcVPChordStopA_3, UL_("Stop base octave +2 chord A")},
	{KeyCommand::Hidden, kcVPChordStopAS3, UL_("Stop base octave +2 chord A#")},
	{KeyCommand::Hidden, kcVPChordStopB_3, UL_("Stop base octave +2 chord B")},
	{KeyCommand::Hidden, kcVPChordStopC_3, UL_("Stop base octave +3 chord C")},
	{KeyCommand::Hidden, kcVPChordStopCS3, UL_("Stop base octave +3 chord C#")},
	{KeyCommand::Hidden, kcVPChordStopD_3, UL_("Stop base octave +3 chord D")},
	{KeyCommand::Hidden, kcVPChordStopDS3, UL_("Stop base octave +3 chord D#")},
	{KeyCommand::Hidden, kcVPChordStopE_3, UL_("Stop base octave +3 chord E")},
	{KeyCommand::Hidden, kcVPChordStopF_3, UL_("Stop base octave +3 chord F")},
	{KeyCommand::Hidden, kcVPChordStopFS3, UL_("Stop base octave +3 chord F#")},
	{KeyCommand::Hidden, kcVPChordStopG_3, UL_("Stop base octave +3 chord G")},
	{KeyCommand::Hidden, kcVPChordStopGS3, UL_("Stop base octave +3 chord G#")},
	{KeyCommand::Hidden, kcVPChordStopA_4, UL_("Stop base octave +3 chord A")},
	{KeyCommand::Hidden, kcVPChordStopAS4, UL_("Stop base octave +3 chord A#")},
	{KeyCommand::Hidden, kcVPChordStopB_4, UL_("Stop base octave +3 chord B")},
	{KeyCommand::Hidden, kcVPChordStopC_4, UL_("Stop base octave +4 chord C")},
	{KeyCommand::Hidden, kcVPChordStopCS4, UL_("Stop base octave +4 chord C#")},
	{KeyCommand::Hidden, kcVPChordStopD_4, UL_("Stop base octave +4 chord D")},
	{KeyCommand::Hidden, kcVPChordStopDS4, UL_("Stop base octave +4 chord D#")},
	{KeyCommand::Hidden, kcVPChordStopE_4, UL_("Stop base octave +4 chord E")},
	{KeyCommand::Hidden, kcVPChordStopF_4, UL_("Stop base octave +4 chord F")},
	{KeyCommand::Hidden, kcVPChordStopFS4, UL_("Stop base octave +4 chord F#")},
	{KeyCommand::Hidden, kcVPChordStopG_4, UL_("Stop base octave +4 chord G")},
	{KeyCommand::Hidden, kcVPChordStopGS4, UL_("Stop base octave +4 chord G#")},
	{KeyCommand::Hidden, kcVPChordStopA_5, UL_("Stop base octave +4 chord A")},
	{KeyCommand::Hidden, kcVPChordStopAS5, UL_("Stop base octave +4 chord A#")},
	{KeyCommand::Hidden, kcVPChordStopB_5, UL_("Stop base octave +4 chord B")},
	{1200, kcNoteCut, UL_("Note Cut")},
	{1201, kcNoteOff, UL_("Note Off")},
	{1202, kcSetIns0, UL_("Set instrument digit 0")},
	{1203, kcSetIns1, UL_("Set instrument digit 1")},
	{1204, kcSetIns2, UL_("Set instrument digit 2")},
	{1205, kcSetIns3, UL_("Set instrument digit 3")},
	{1206, kcSetIns4, UL_("Set instrument digit 4")},
	{1207, kcSetIns5, UL_("Set instrument digit 5")},
	{1208, kcSetIns6, UL_("Set instrument digit 6")},
	{1209, kcSetIns7, UL_("Set instrument digit 7")},
	{1210, kcSetIns8, UL_("Set instrument digit 8")},
	{1211, kcSetIns9, UL_("Set instrument digit 9")},
	{1212, kcSetOctave0, UL_("Set octave 0")},
	{1213, kcSetOctave1, UL_("Set octave 1")},
	{1214, kcSetOctave2, UL_("Set octave 2")},
	{1215, kcSetOctave3, UL_("Set octave 3")},
	{1216, kcSetOctave4, UL_("Set octave 4")},
	{1217, kcSetOctave5, UL_("Set octave 5")},
	{1218, kcSetOctave6, UL_("Set octave 6")},
	{1219, kcSetOctave7, UL_("Set octave 7")},
	{1220, kcSetOctave8, UL_("Set octave 8")},
	{1221, kcSetOctave9, UL_("Set octave 9")},
	{1222, kcSetVolume0, UL_("Set volume digit 0")},
	{1223, kcSetVolume1, UL_("Set volume digit 1")},
	{1224, kcSetVolume2, UL_("Set volume digit 2")},
	{1225, kcSetVolume3, UL_("Set volume digit 3")},
	{1226, kcSetVolume4, UL_("Set volume digit 4")},
	{1227, kcSetVolume5, UL_("Set volume digit 5")},
	{1228, kcSetVolume6, UL_("Set volume digit 6")},
	{1229, kcSetVolume7, UL_("Set volume digit 7")},
	{1230, kcSetVolume8, UL_("Set volume digit 8")},
	{1231, kcSetVolume9, UL_("Set volume digit 9")},
	{1232, kcSetVolumeVol, UL_("Volume Command - Volume")},
	{1233, kcSetVolumePan, UL_("Volume Command - Panning")},
	{1234, kcSetVolumeVolSlideUp, UL_("Volume Command - Volume Slide Up")},
	{1235, kcSetVolumeVolSlideDown, UL_("Volume Command - Volume Slide Down")},
	{1236, kcSetVolumeFineVolUp, UL_("Volume Command - Fine Volume Slide Up")},
	{1237, kcSetVolumeFineVolDown, UL_("Volume Command - Fine Volume Slide Down")},
	{1238, kcSetVolumeVibratoSpd, UL_("Volume Command - Vibrato Speed")},
	{1239, kcSetVolumeVibrato, UL_("Volume Command - Vibrato Depth")},
	{1240, kcSetVolumeXMPanLeft, UL_("Volume Command - XM Pan Slide Left")},
	{1241, kcSetVolumeXMPanRight, UL_("Volume Command - XM Pan Slide Right")},
	{1242, kcSetVolumePortamento, UL_("Volume Command - Tone Portamento")},
	{1243, kcSetVolumeITPortaUp, UL_("Volume Command - Portamento Up")},
	{1244, kcSetVolumeITPortaDown, UL_("Volume Command - Portamento Down")},
	{KeyCommand::Hidden, kcSetVolumeITUnused, UL_("Volume Command - Unused")},
	{1246, kcSetVolumeITOffset, UL_("Volume Command - Offset")},
	{1247, kcSetFXParam0, UL_("Effect Parameter Digit 0")},
	{1248, kcSetFXParam1, UL_("Effect Parameter Digit 1")},
	{1249, kcSetFXParam2, UL_("Effect Parameter Digit 2")},
	{1250, kcSetFXParam3, UL_("Effect Parameter Digit 3")},
	{1251, kcSetFXParam4, UL_("Effect Parameter Digit 4")},
	{1252, kcSetFXParam5, UL_("Effect Parameter Digit 5")},
	{1253, kcSetFXParam6, UL_("Effect Parameter Digit 6")},
	{1254, kcSetFXParam7, UL_("Effect Parameter Digit 7")},
	{1255, kcSetFXParam8, UL_("Effect Parameter Digit 8")},
	{1256, kcSetFXParam9, UL_("Effect Parameter Digit 9")},
	{1257, kcSetFXParamA, UL_("Effect Parameter Digit A")},
	{1258, kcSetFXParamB, UL_("Effect Parameter Digit B")},
	{1259, kcSetFXParamC, UL_("Effect Parameter Digit C")},
	{1260, kcSetFXParamD, UL_("Effect Parameter Digit D")},
	{1261, kcSetFXParamE, UL_("Effect Parameter Digit E")},
	{1262, kcSetFXParamF, UL_("Effect Parameter Digit F")},
	{KeyCommand::Hidden, kcSetFXarp, UL_("FX Arpeggio")},
	{KeyCommand::Hidden, kcSetFXportUp, UL_("FX Portamento Up")},
	{KeyCommand::Hidden, kcSetFXportDown, UL_("FX Portamento Down")},
	{KeyCommand::Hidden, kcSetFXport, UL_("FX Tone Portamento")},
	{KeyCommand::Hidden, kcSetFXvibrato, UL_("FX Vibrato")},
	{KeyCommand::Hidden, kcSetFXportSlide, UL_("FX Portamento + Volume Slide")},
	{KeyCommand::Hidden, kcSetFXvibSlide, UL_("FX Vibrato + Volume Slide")},
	{KeyCommand::Hidden, kcSetFXtremolo, UL_("FX Tremolo")},
	{KeyCommand::Hidden, kcSetFXpan, UL_("FX Pan")},
	{KeyCommand::Hidden, kcSetFXoffset, UL_("FX Offset")},
	{KeyCommand::Hidden, kcSetFXvolSlide, UL_("FX Volume Slide")},
	{KeyCommand::Hidden, kcSetFXgotoOrd, UL_("FX Pattern Jump")},
	{KeyCommand::Hidden, kcSetFXsetVol, UL_("FX Set Volume")},
	{KeyCommand::Hidden, kcSetFXgotoRow, UL_("FX Break To Row")},
	{KeyCommand::Hidden, kcSetFXretrig, UL_("FX Retrigger")},
	{KeyCommand::Hidden, kcSetFXspeed, UL_("FX Set Speed")},
	{KeyCommand::Hidden, kcSetFXtempo, UL_("FX Set Tempo")},
	{KeyCommand::Hidden, kcSetFXtremor, UL_("FX Tremor")},
	{KeyCommand::Hidden, kcSetFXextendedMOD, UL_("FX Extended MOD Commands")},
	{KeyCommand::Hidden, kcSetFXextendedS3M, UL_("FX Extended S3M Commands")},
	{KeyCommand::Hidden, kcSetFXchannelVol, UL_("FX Set Channel Volume")},
	{KeyCommand::Hidden, kcSetFXchannelVols, UL_("FX Channel Volume Slide")},
	{KeyCommand::Hidden, kcSetFXglobalVol, UL_("FX Set Global Volume")},
	{KeyCommand::Hidden, kcSetFXglobalVols, UL_("FX Global Volume Slide")},
	{KeyCommand::Hidden, kcSetFXkeyoff, UL_("FX Key Off (XM)")},
	{KeyCommand::Hidden, kcSetFXfineVib, UL_("FX Fine Vibrato")},
	{KeyCommand::Hidden, kcSetFXpanbrello, UL_("FX Panbrello")},
	{KeyCommand::Hidden, kcSetFXextendedXM, UL_("FX Extended XM Commands")},
	{KeyCommand::Hidden, kcSetFXpanSlide, UL_("FX Pan Slide")},
	{KeyCommand::Hidden, kcSetFXsetEnvPos, UL_("FX Set Envelope Position (XM)")},
	{KeyCommand::Hidden, kcSetFXmacro, UL_("FX MIDI Macro")},
	{KeyCommand::Hidden, kcSetFXDummy, UL_("FX Dummy") },
	{1294, kcSetFXmacroSlide, UL_("Smooth MIDI Macro Slide")},
	{1295, kcSetFXdelaycut, UL_("Combined Note Delay and Note Cut")},
	{KeyCommand::Hidden, kcPatternJumpDownh1Select, UL_("Jump down by measure select")},
	{KeyCommand::Hidden, kcPatternJumpUph1Select, UL_("Jump up by measure select")},
	{KeyCommand::Hidden, kcPatternSnapDownh1Select, UL_("Snap down to measure select")},
	{KeyCommand::Hidden, kcPatternSnapUph1Select, UL_("Snap up to measure select")},
	{KeyCommand::Hidden, kcNavigateDownSelect, UL_("Select to Down")},
	{KeyCommand::Hidden, kcNavigateUpSelect, UL_("Select to Up")},
	{KeyCommand::Hidden, kcNavigateLeftSelect, UL_("Select to Left")},
	{KeyCommand::Hidden, kcNavigateRightSelect, UL_("Select to Right")},
	{KeyCommand::Hidden, kcNavigateNextChanSelect, UL_("Select to Next Channel")},
	{KeyCommand::Hidden, kcNavigatePrevChanSelect, UL_("Select to Previous Channel")},
	{KeyCommand::Hidden, kcHomeHorizontalSelect, UL_("Select to First Channel")},
	{KeyCommand::Hidden, kcHomeVerticalSelect, UL_("Select to First Row")},
	{KeyCommand::Hidden, kcHomeAbsoluteSelect, UL_("Selecto to First Row / Channel")},
	{KeyCommand::Hidden, kcEndHorizontalSelect, UL_("Select to Last Channel")},
	{KeyCommand::Hidden, kcEndVerticalSelect, UL_("Select to Last Row")},
	{KeyCommand::Hidden, kcEndAbsoluteSelect, UL_("Select to Last Row /channel")},
	{KeyCommand::Hidden, kcSelectWithNav, UL_("kcSelectWithNav")},
	{KeyCommand::Hidden, kcSelectOffWithNav, UL_("kcSelectOffWithNav")},
	{KeyCommand::Hidden, kcCopySelectWithNav, UL_("kcCopySelectWithNav")},
	{KeyCommand::Hidden, kcCopySelectOffWithNav, UL_("kcCopySelectOffWithNav")},
	{1316 | KeyCommand::Dummy, kcChordModifier, UL_("Chord Modifier")},
	{1317 | KeyCommand::Dummy, kcSetSpacing, UL_("Edit Step Modifier")},
	{KeyCommand::Hidden, kcSetSpacing0, UL_("Set Edit Step to 0")},
	{KeyCommand::Hidden, kcSetSpacing1, UL_("Set Edit Step to 1")},
	{KeyCommand::Hidden, kcSetSpacing2, UL_("Set Edit Step to 2")},
	{KeyCommand::Hidden, kcSetSpacing3, UL_("Set Edit Step to 3")},
	{KeyCommand::Hidden, kcSetSpacing4, UL_("Set Edit Step to 4")},
	{KeyCommand::Hidden, kcSetSpacing5, UL_("Set Edit Step to 5")},
	{KeyCommand::Hidden, kcSetSpacing6, UL_("Set Edit Step to 6")},
	{KeyCommand::Hidden, kcSetSpacing7, UL_("Set Edit Step to 7")},
	{KeyCommand::Hidden, kcSetSpacing8, UL_("Set Edit Step to 8")},
	{KeyCommand::Hidden, kcSetSpacing9, UL_("Set Edit Step to 9")},
	{KeyCommand::Hidden, kcCopySelectWithSelect, UL_("kcCopySelectWithSelect")},
	{KeyCommand::Hidden, kcCopySelectOffWithSelect, UL_("kcCopySelectOffWithSelect")},
	{KeyCommand::Hidden, kcSelectWithCopySelect, UL_("kcSelectWithCopySelect")},
	{KeyCommand::Hidden, kcSelectOffWithCopySelect, UL_("kcSelectOffWithCopySelect")},
	/*
	{1332, kcCopy, UL_("Copy pattern data")},
	{1333, kcCut, UL_("Cut pattern data")},
	{1334, kcPaste, UL_("Paste pattern data")},
	{1335, kcMixPaste, UL_("Mix-paste pattern data")},
	{1336, kcSelectAll, UL_("Select all pattern data")},
	{CommandStruct::Hidden, kcSelectCol, UL_("Select Channel / Select All")},
	*/
	{1338, kcPatternJumpDownh2, UL_("Jump down by beat")},
	{1339, kcPatternJumpUph2, UL_("Jump up by beat")},
	{1340, kcPatternSnapDownh2, UL_("Snap down to beat")},
	{1341, kcPatternSnapUph2, UL_("Snap up to beat")},
	{KeyCommand::Hidden, kcPatternJumpDownh2Select, UL_("Jump down by beat select")},
	{KeyCommand::Hidden, kcPatternJumpUph2Select, UL_("Jump up by beat select")},
	{KeyCommand::Hidden, kcPatternSnapDownh2Select, UL_("Snap down to beat select")},
	{KeyCommand::Hidden, kcPatternSnapUph2Select, UL_("Snap up to beat select")},
	{1346, kcFileOpen, UL_("File/Open")},
	{1347, kcFileNew, UL_("File/New")},
	{1348, kcFileClose, UL_("File/Close")},
	{1349, kcFileSave, UL_("File/Save")},
	{1350, kcFileSaveAs, UL_("File/Save As")},
	{1351, kcFileSaveAsWave, UL_("File/Stream Export")},
	{1352 | KeyCommand::Hidden, kcFileSaveAsMP3, UL_("File/Stream Export")}, // Legacy
	{1353, kcFileSaveMidi, UL_("File/Export as MIDI")},
	{1354, kcFileImportMidiLib, UL_("File/Import MIDI Library")},
	{1355, kcFileAddSoundBank, UL_("File/Add Sound Bank")},
	{1359, kcEditUndo, UL_("Undo")},
	{1360, kcEditCut, UL_("Cut")},
	{1361, kcEditCopy, UL_("Copy")},
	{1362, kcEditPaste, UL_("Paste")},
	{1363, kcEditMixPaste, UL_("Mix Paste")},
	{1364, kcEditSelectAll, UL_("Select All")},
	{1365, kcEditFind, UL_("Find / Replace")},
	{1366, kcEditFindNext, UL_("Find Next")},
	{1367, kcViewMain, UL_("Toggle Main Toolbar")},
	{1368, kcViewTree, UL_("Toggle Tree View")},
	{1369, kcViewOptions, UL_("View Options")},
	{1370, kcHelp, UL_("Help")},
	/*
	{1370, kcWindowNew, UL_("New Window")},
	{1371, kcWindowCascade, UL_("Cascade Windows")},
	{1372, kcWindowTileHorz, UL_("Tile Windows Horizontally")},
	{1373, kcWindowTileVert, UL_("Tile Windows Vertically")},
	*/
	{1374, kcEstimateSongLength, UL_("Estimate Song Length")},
	{1375, kcStopSong, UL_("Stop Song")},
	{1376, kcMidiRecord, UL_("Toggle MIDI Record")},
	{1377, kcDeleteWholeRow, UL_("Delete Row(s) (All Channels)")},
	{1378, kcInsertRow, UL_("Insert Row(s)")},
	{1379, kcInsertWholeRow, UL_("Insert Row(s) (All Channels)")},
	{1380, kcSampleTrim, UL_("Trim sample around loop points")},
	{1381, kcSampleReverse, UL_("Reverse Sample")},
	{1382, kcSampleDelete, UL_("Delete Sample Selection")},
	{1383, kcSampleSilence, UL_("Silence Sample Selection")},
	{1384, kcSampleNormalize, UL_("Normalize Sample")},
	{1385, kcSampleAmplify, UL_("Amplify Sample")},
	{1386, kcSampleZoomUp, UL_("Zoom In")},
	{1387, kcSampleZoomDown, UL_("Zoom Out")},
	{1660, kcPatternGrowSelection, UL_("Grow selection")},
	{1661, kcPatternShrinkSelection, UL_("Shrink selection")},
	{1662, kcTogglePluginEditor, UL_("Toggle channel's plugin editor")},
	{1663, kcToggleFollowSong, UL_("Toggle follow song")},
	{1664, kcClearFieldITStyle, UL_("Clear Field (IT Style)")},
	{1665, kcClearFieldStepITStyle, UL_("Clear Field and Step (IT Style)")},
	{1666, kcSetFXextension, UL_("Parameter Extension Command")},
	{1667 | KeyCommand::Hidden, kcNoteCutOld, UL_("Note Cut")},  // Legacy
	{1668 | KeyCommand::Hidden, kcNoteOffOld, UL_("Note Off")},  // Legacy
	{1669, kcViewAddPlugin, UL_("View Plugin Manager")},
	{1670, kcViewChannelManager, UL_("View Channel Manager")},
	{1671, kcCopyAndLoseSelection, UL_("Copy and lose selection")},
	{1672, kcNewPattern, UL_("Insert New Pattern")},
	{1673, kcSampleLoad, UL_("Load Sample")},
	{1674, kcSampleSave, UL_("Save Sample")},
	{1675, kcSampleNew, UL_("New Sample")},
	//{CommandStruct::Hidden, kcSampleCtrlLoad, UL_("Load Sample")},
	//{CommandStruct::Hidden, kcSampleCtrlSave, UL_("Save Sample")},
	//{CommandStruct::Hidden, kcSampleCtrlNew, UL_("New Sample")},
	{KeyCommand::Hidden, kcInstrumentLoad, UL_("Load Instrument")},
	{KeyCommand::Hidden, kcInstrumentSave, UL_("Save Instrument")},
	{KeyCommand::Hidden, kcInstrumentNew, UL_("New Instrument")},
	{KeyCommand::Hidden, kcInstrumentCtrlLoad, UL_("Load Instrument")},
	{KeyCommand::Hidden, kcInstrumentCtrlSave, UL_("Save Instrument")},
	{KeyCommand::Hidden, kcInstrumentCtrlNew, UL_("New Instrument")},
	{1685, kcSwitchToOrderList, UL_("Switch to Order List")},
	{1686, kcEditMixPasteITStyle, UL_("Mix Paste (IT Style)")},
	{1687, kcApproxRealBPM, UL_("Show approx. real BPM")},
	{KeyCommand::Hidden, kcNavigateDownBySpacingSelect, UL_("Up-By-Spacing-Select")},
	{KeyCommand::Hidden, kcNavigateUpBySpacingSelect, UL_("Down-By-Spacing-Select")},
	{1691, kcNavigateDownBySpacing, UL_("Navigate down by spacing")},
	{1692, kcNavigateUpBySpacing, UL_("Navigate up by spacing")},
	{1693, kcPrevDocument, UL_("Previous Document")},
	{1694, kcNextDocument, UL_("Next Document")},
	{1763, kcVSTGUIPrevPreset, UL_("Previous Plugin Preset")},
	{1764, kcVSTGUINextPreset, UL_("Next Plugin Preset")},
	{1765, kcVSTGUIRandParams, UL_("Randomize Plugin Parameters")},
	{1766, kcPatternGoto, UL_("Go to row/channel/...")},
	{KeyCommand::Hidden, kcPatternOpenRandomizer, UL_("Pattern Randomizer")},  // while there's not randomizer yet, let's just disable it for now
	{1768, kcPatternInterpolateNote, UL_("Interpolate Note")},
	{KeyCommand::Hidden, kcViewGraph, UL_("View Graph")},  // while there's no graph yet, let's just disable it for now
	{1770, kcToggleChanMuteOnPatTransition, UL_("(Un)mute channel on pattern transition")},
	{1771, kcChannelUnmuteAll, UL_("Unmute all channels")},
	{1772, kcShowPatternProperties, UL_("Show Pattern Properties")},
	{1773, kcShowMacroConfig, UL_("View Zxx Macro Configuration")},
	{1775, kcViewSongProperties, UL_("View Song Properties")},
	{1776, kcChangeLoopStatus, UL_("Toggle Loop Pattern")},
	{1777, kcFileExportCompat, UL_("File/Compatibility Export")},
	{1778, kcUnmuteAllChnOnPatTransition, UL_("Unmute all channels on pattern transition")},
	{1779, kcSoloChnOnPatTransition, UL_("Solo channel on pattern transition")},
	{1780, kcTimeAtRow, UL_("Show playback time at current row")},
	{1781, kcViewMIDImapping, UL_("View MIDI Mapping")},
	{1782, kcVSTGUIPrevPresetJump, UL_("Plugin Preset -10")},
	{1783, kcVSTGUINextPresetJump, UL_("Plugin Preset +10")},
	{1784, kcSampleInvert, UL_("Invert Sample Phase")},
	{1785, kcSampleSignUnsign, UL_("Signed / Unsigned Conversion")},
	{1786, kcChannelReset, UL_("Reset Channel")},
	{1787, kcToggleOverflowPaste, UL_("Toggle Overflow Paste")},
	{1788, kcNotePC, UL_("Parameter Control")},
	{1789, kcNotePCS, UL_("Parameter Control (smooth)")},
	{1790, kcSampleRemoveDCOffset, UL_("Remove DC Offset")},
	{1791, kcNoteFade, UL_("Note Fade")},
	{1792 | KeyCommand::Hidden, kcNoteFadeOld, UL_("Note Fade")},  // Legacy
	{1793, kcEditPasteFlood, UL_("Paste Flood")},
	{1794, kcOrderlistNavigateLeft, UL_("Previous Order")},
	{1795, kcOrderlistNavigateRight, UL_("Next Order")},
	{1796, kcOrderlistNavigateFirst, UL_("First Order")},
	{1797, kcOrderlistNavigateLast, UL_("Last Order")},
	{KeyCommand::Hidden, kcOrderlistNavigateLeftSelect, UL_("Left-Select")},
	{KeyCommand::Hidden, kcOrderlistNavigateRightSelect, UL_("Right-Select")},
	{KeyCommand::Hidden, kcOrderlistNavigateFirstSelect, UL_("First-Select")},
	{KeyCommand::Hidden, kcOrderlistNavigateLastSelect, UL_("Last-Select")},
	{1802, kcOrderlistEditDelete, UL_("Delete Order")},
	{1803, kcOrderlistEditInsert, UL_("Insert Order")},
	{1804, kcOrderlistEditPattern, UL_("Edit Pattern")},
	{1805, kcOrderlistSwitchToPatternView, UL_("Switch to pattern editor")},
	{1806, kcDuplicatePattern, UL_("Duplicate Pattern")},
	{1807, kcOrderlistPat0, UL_("Pattern index digit 0")},
	{1808, kcOrderlistPat1, UL_("Pattern index digit 1")},
	{1809, kcOrderlistPat2, UL_("Pattern index digit 2")},
	{1810, kcOrderlistPat3, UL_("Pattern index digit 3")},
	{1811, kcOrderlistPat4, UL_("Pattern index digit 4")},
	{1812, kcOrderlistPat5, UL_("Pattern index digit 5")},
	{1813, kcOrderlistPat6, UL_("Pattern index digit 6")},
	{1814, kcOrderlistPat7, UL_("Pattern index digit 7")},
	{1815, kcOrderlistPat8, UL_("Pattern index digit 8")},
	{1816, kcOrderlistPat9, UL_("Pattern index digit 9")},
	{1817, kcOrderlistPatPlus, UL_("Increase Pattern Index")},
	{1818, kcOrderlistPatMinus, UL_("Decrease Pattern Index")},
	{1819, kcShowSplitKeyboardSettings, UL_("Split Keyboard Settings dialog")},
	{1820, kcEditPushForwardPaste, UL_("Push Forward Paste (Insert)")},
	{1821, kcInstrumentEnvelopePointMoveLeft, UL_("Move Envelope Point Left")},
	{1822, kcInstrumentEnvelopePointMoveRight, UL_("Move Envelope Point Right")},
	{1823, kcInstrumentEnvelopePointMoveUp, UL_("Move envelope Point Up")},
	{1824, kcInstrumentEnvelopePointMoveDown, UL_("Move Envelope Point Down")},
	{1825, kcInstrumentEnvelopePointPrev, UL_("Select Previous Envelope Point")},
	{1826, kcInstrumentEnvelopePointNext, UL_("Select Next Envelope Point")},
	{1827, kcInstrumentEnvelopePointInsert, UL_("Insert Envelope Point")},
	{1828, kcInstrumentEnvelopePointRemove, UL_("Remove Envelope Point")},
	{1829, kcInstrumentEnvelopeSetLoopStart, UL_("Set Loop Start")},
	{1830, kcInstrumentEnvelopeSetLoopEnd, UL_("Set Loop End")},
	{1831, kcInstrumentEnvelopeSetSustainLoopStart, UL_("Set Sustain Loop Start")},
	{1832, kcInstrumentEnvelopeSetSustainLoopEnd, UL_("Set Sustain Loop End")},
	{1833, kcInstrumentEnvelopeToggleReleaseNode, UL_("Toggle Release Mode")},
	{1834, kcInstrumentEnvelopePointMoveUp8, UL_("Move Envelope Point Up (Coarse)")},
	{1835, kcInstrumentEnvelopePointMoveDown8, UL_("Move Envelope Point Down (Coarse)")},
	{1836, kcPatternEditPCNotePlugin, UL_("Toggle PC Event/Instrument Plugin Editor")},
	{1837, kcInstrumentEnvelopeZoomIn, UL_("Zoom In")},
	{1838, kcInstrumentEnvelopeZoomOut, UL_("Zoom Out")},
	{1839, kcVSTGUIToggleRecordParams, UL_("Toggle Parameter Recording")},
	{1840, kcVSTGUIToggleSendKeysToPlug, UL_("Pass Key Presses to Plugin")},
	{1841, kcVSTGUIBypassPlug, UL_("Bypass Plugin")},
	{1842, kcInsNoteMapTransposeDown, UL_("Transpose -1 (Note Map)")},
	{1843, kcInsNoteMapTransposeUp, UL_("Transpose +1 (Note Map)")},
	{1844, kcInsNoteMapTransposeOctDown, UL_("Transpose -1 Octave (Note Map)")},
	{1845, kcInsNoteMapTransposeOctUp, UL_("Transpose +1 Octave (Note Map)")},
	{1846, kcInsNoteMapCopyCurrentNote, UL_("Map all notes to selected note")},
	{1847, kcInsNoteMapCopyCurrentSample, UL_("Map all notes to selected sample")},
	{1848, kcInsNoteMapReset, UL_("Reset Note Mapping")},
	{1849, kcInsNoteMapEditSample, UL_("Edit Current Sample")},
	{1850, kcInsNoteMapEditSampleMap, UL_("Edit Sample Map")},
	{1851, kcInstrumentCtrlDuplicate, UL_("Duplicate Instrument")},
	{1852, kcPanic, UL_("Panic")},
	{1853, kcOrderlistPatIgnore, UL_("Separator (+++) Index")},
	{1854, kcOrderlistPatInvalid, UL_("Stop (---) Index")},
	{1855, kcViewEditHistory, UL_("View Edit History")},
	{1856, kcSampleQuickFade, UL_("Quick Fade")},
	{1857, kcSampleXFade, UL_("Crossfade Sample Loop")},
	{1858, kcSelectBeat, UL_("Select Beat")},
	{1859, kcSelectMeasure, UL_("Select Measure")},
	{1860, kcFileSaveTemplate, UL_("File/Save As Template")},
	{1861, kcIncreaseSpacing, UL_("Increase Edit Step")},
	{1862, kcDecreaseSpacing, UL_("Decrease Edit Step")},
	{1863, kcSampleAutotune, UL_("Tune Sample to given Note")},
	{1864, kcFileCloseAll, UL_("File/Close All")},
	{KeyCommand::Hidden, kcSetOctaveStop0, UL_("Set Octave 0")},
	{KeyCommand::Hidden, kcSetOctaveStop1, UL_("Set Octave 1")},
	{KeyCommand::Hidden, kcSetOctaveStop2, UL_("Set Octave 2")},
	{KeyCommand::Hidden, kcSetOctaveStop3, UL_("Set Octave 3")},
	{KeyCommand::Hidden, kcSetOctaveStop4, UL_("Set Octave 4")},
	{KeyCommand::Hidden, kcSetOctaveStop5, UL_("Set Octave 5")},
	{KeyCommand::Hidden, kcSetOctaveStop6, UL_("Set Octave 6")},
	{KeyCommand::Hidden, kcSetOctaveStop7, UL_("Set Octave 7")},
	{KeyCommand::Hidden, kcSetOctaveStop8, UL_("Set Octave 8")},
	{KeyCommand::Hidden, kcSetOctaveStop9, UL_("Set Octave 9")},
	{1875, kcOrderlistLockPlayback, UL_("Lock Playback to Selection")},
	{1876, kcOrderlistUnlockPlayback, UL_("Unlock Playback")},
	{1877, kcChannelSettings, UL_("Quick Channel Settings")},
	{1878, kcChnSettingsPrev, UL_("Previous Channel")},
	{1879, kcChnSettingsNext, UL_("Next Channel")},
	{1880, kcChnSettingsClose, UL_("Switch to Pattern Editor")},
	{1881, kcTransposeCustom, UL_("Transpose Custom")},
	{1882, kcSampleZoomSelection, UL_("Zoom into Selection")},
	{1883, kcChannelRecordSelect, UL_("Channel Record Select")},
	{1884, kcChannelSplitRecordSelect, UL_("Channel Split Record Select")},
	{1885, kcDataEntryUp, UL_("Data Entry +1")},
	{1886, kcDataEntryDown, UL_("Data Entry -1")},
	{1887, kcSample8Bit, UL_("Convert to 8-bit / 16-bit")},
	{1888, kcSampleMonoMix, UL_("Convert to Mono (Mix)")},
	{1889, kcSampleMonoLeft, UL_("Convert to Mono (Left Channel)")},
	{1890, kcSampleMonoRight, UL_("Convert to Mono (Right Channel)")},
	{1891, kcSampleMonoSplit, UL_("Convert to Mono (Split Sample)")},
	{1892, kcQuantizeSettings, UL_("Quantize Settings")},
	{1893, kcDataEntryUpCoarse, UL_("Data Entry Up (Coarse)")},
	{1894, kcDataEntryDownCoarse, UL_("Data Entry Down (Coarse)")},
	{1895, kcToggleNoteOffRecordPC, UL_("Toggle Note Off record (PC keyboard)")},
	{1896, kcToggleNoteOffRecordMIDI, UL_("Toggle Note Off record (MIDI)")},
	{1897, kcFindInstrument, UL_("Pick up nearest instrument number")},
	{1898, kcPlaySongFromPattern, UL_("Play Song from Pattern Start")},
	{1899, kcVSTGUIToggleRecordMIDIOut, UL_("Record MIDI Out to Pattern Editor")},
	{1900, kcToggleClipboardManager, UL_("Toggle Clipboard Manager")},
	{1901, kcClipboardPrev, UL_("Cycle to Previous Clipboard")},
	{1902, kcClipboardNext, UL_("Cycle to Next Clipboard")},
	{1903, kcSelectRow, UL_("Select Row")},
	{1904, kcSelectEvent, UL_("Select Event")},
	{1905, kcEditRedo, UL_("Redo")},
	{1906, kcFileAppend, UL_("File/Append Module")},
	{1907, kcSampleTransposeUp, UL_("Transpose +1")},
	{1908, kcSampleTransposeDown, UL_("Transpose -1")},
	{1909, kcSampleTransposeOctUp, UL_("Transpose +1 Octave")},
	{1910, kcSampleTransposeOctDown, UL_("Transpose -1 Octave")},
	{1911, kcPatternInterpolateInstr, UL_("Interpolate Instrument")},
	{1912, kcDummyShortcut, UL_("Dummy Shortcut")},
	{1913, kcSampleUpsample, UL_("Upsample")},
	{1914, kcSampleDownsample, UL_("Downsample")},
	{1915, kcSampleResample, UL_("Resample")},
	{1916, kcSampleCenterLoopStart, UL_("Center loop start in view")},
	{1917, kcSampleCenterLoopEnd, UL_("Center loop end in view")},
	{1918, kcSampleCenterSustainStart, UL_("Center sustain loop start in view")},
	{1919, kcSampleCenterSustainEnd, UL_("Center sustain loop end in view")},
	{1920, kcInstrumentEnvelopeLoad, UL_("Load Envelope")},
	{1921, kcInstrumentEnvelopeSave, UL_("Save Envelope")},
	{1922, kcChannelTranspose, UL_("Transpose Channel")},
	{1923, kcChannelDuplicate, UL_("Duplicate Channel")},
	// Reserved range 1924...1949 for kcStartSampleCues...kcEndSampleCues (generated below)
	{1950, kcOrderlistEditCopyOrders, UL_("Copy Orders")},
	{KeyCommand::Hidden, kcTreeViewStopPreview, UL_("Stop sample preview")},
	{1952, kcSampleDuplicate, UL_("Duplicate Sample")},
	{1953, kcSampleSliceCuePoints, UL_("Slice at cue points")},
	{1954, kcInstrumentEnvelopeScale, UL_("Scale Envelope Points")},
	{1955, kcInsNoteMapRemove, UL_("Remove All Samples")},
	{1956, kcInstrumentEnvelopeSelectLoopStart, UL_("Select Envelope Loop Start")},
	{1957, kcInstrumentEnvelopeSelectLoopEnd, UL_("Select Envelope Loop End")},
	{1958, kcInstrumentEnvelopeSelectSustainStart, UL_("Select Envelope Sustain Start")},
	{1959, kcInstrumentEnvelopeSelectSustainEnd, UL_("Select Envelope Sustain End")},
	{1960, kcInstrumentEnvelopePointMoveLeftCoarse, UL_("Move envelope point left (Coarse)")},
	{1961, kcInstrumentEnvelopePointMoveRightCoarse, UL_("Move envelope point right (Coarse)")},
	{1962, kcSampleCenterSampleStart, UL_("Zoom into sample start")},
	{1963, kcSampleCenterSampleEnd, UL_("Zoom into sample end")},
	{1964, kcSampleTrimToLoopEnd, UL_("Trim to loop end")},
	{1965, kcLockPlaybackToRows, UL_("Lock Playback to Rows")},
	{1966, kcSwitchToInstrLibrary, UL_("Switch To Instrument Library")},
	{1967, kcPatternSetInstrumentNotEmpty, UL_("Apply current instrument to existing only")},
	{1968, kcSelectColumn, UL_("Select Column")},
	{1969, kcSampleStereoSep, UL_("Change Stereo Separation")},
	{1970, kcTransposeCustomQuick, UL_("Transpose Custom (Quick)")},
	{1971, kcPrevEntryInColumn, UL_("Jump to previous entry in column")},
	{1972, kcNextEntryInColumn, UL_("Jump to next entry in column")},
	{1973, kcViewTempoSwing, UL_("View Global Tempo Swing Settings")},
	{1974, kcChordEditor, UL_("Show Chord Editor")},
	{1975, kcToggleLoopSong, UL_("Toggle Loop Song")},
	{1976, kcInstrumentEnvelopeSwitchToVolume, UL_("Switch to Volume Envelope")},
	{1977, kcInstrumentEnvelopeSwitchToPanning, UL_("Switch to Panning Envelope")},
	{1978, kcInstrumentEnvelopeSwitchToPitch, UL_("Switch to Pitch / Filter Envelope")},
	{1979, kcInstrumentEnvelopeToggleVolume, UL_("Toggle Volume Envelope")},
	{1980, kcInstrumentEnvelopeTogglePanning, UL_("Toggle Panning Envelope")},
	{1981, kcInstrumentEnvelopeTogglePitch, UL_("Toggle Pitch Envelope")},
	{1982, kcInstrumentEnvelopeToggleFilter, UL_("Toggle Filter Envelope")},
	{1983, kcInstrumentEnvelopeToggleLoop, UL_("Toggle Envelope Loop")},
	{1984, kcInstrumentEnvelopeToggleSustain, UL_("Toggle Envelope Sustain Loop")},
	{1985, kcInstrumentEnvelopeToggleCarry, UL_("Toggle Envelope Carry")},
	{1986, kcSampleInitializeOPL, UL_("Initialize OPL Instrument")},
	{1987, kcFileSaveCopy, UL_("File/Save Copy")},
	{1988, kcMergePatterns, UL_("Merge Patterns")},
	{1989, kcSplitPattern, UL_("Split Pattern")},
	{1990, kcSampleToggleDrawing, UL_("Toggle Sample Drawing")},
	{1991, kcSampleResize, UL_("Add Silence / Create Sample")},
	{1992, kcSampleGrid, UL_("Configure Sample Grid")},
	{1993, kcLoseSelection, UL_("Lose Selection")},
	{1994, kcCutPatternChannel, UL_("Cut to Pattern Channel Clipboard")},
	{1995, kcCutPattern, UL_("Cut to Pattern Clipboard")},
	{1996, kcCopyPatternChannel, UL_("Copy to Pattern Channel Clipboard")},
	{1997, kcCopyPattern, UL_("Copy to Pattern Clipboard")},
	{1998, kcPastePatternChannel, UL_("Paste from Pattern Channel Clipboard")},
	{1999, kcPastePattern, UL_("Paste from Pattern Clipboard")},
	{2000, kcToggleSmpInsList, UL_("Toggle between lists")},
	{2001, kcExecuteSmpInsListItem, UL_("Open item in editor")},
	{2002, kcDeleteRowGlobal, UL_("Delete Row(s) (Global)")},
	{2003, kcDeleteWholeRowGlobal, UL_("Delete Row(s) (All Channels, Global)")},
	{2004, kcInsertRowGlobal, UL_("Insert Row(s) (Global)")},
	{2005, kcInsertWholeRowGlobal, UL_("Insert Row(s) (All Channels, Global)")},
	{2006, kcPrevSequence, UL_("Previous Sequence")},
	{2007, kcNextSequence, UL_("Next Sequence")},
	{2008, kcChnColorFromPrev , UL_("Pick Color from Previous Channel")},
	{2009, kcChnColorFromNext , UL_("Pick Color from Next Channel") },
	{2010, kcChannelMoveLeft, UL_("Move Channels to Left")},
	{2011, kcChannelMoveRight, UL_("Move Channels to Right")},
	{2012, kcSampleConvertPingPongLoop, UL_("Convert Ping-Pong Loop to Unidirectional") },
	{2013, kcSampleConvertPingPongSustain, UL_("Convert Ping-Pong Sustain Loop to Unidirectional") },
	{2014, kcChannelAddBefore, UL_("Add Channel Before Current")},
	{2015, kcChannelAddAfter, UL_("Add Channel After Current") },
	{2016, kcChannelRemove, UL_("Remove Channel") },
	{2017, kcSetFXFinetune, UL_("Finetune") },
	{2018, kcSetFXFinetuneSmooth, UL_("Finetune (Smooth)")},
	{2019, kcOrderlistEditInsertSeparator, UL_("Insert Separator") },
	{2020, kcTempoIncrease, UL_("Increase Tempo")},
	{2021, kcTempoDecrease, UL_("Decrease Tempo")},
	{2022, kcTempoIncreaseFine, UL_("Increase Tempo (Fine)")},
	{2023, kcTempoDecreaseFine, UL_("Decrease Tempo (Fine)")},
	{2024, kcSpeedIncrease, UL_("Increase Ticks per Row")},
	{2025, kcSpeedDecrease, UL_("Decrease Ticks per Row")},
	{2026, kcRenameSmpInsListItem, UL_("Rename Item")},
	{2027, kcShowChannelCtxMenu, UL_("Show Channel Context (Right-Click) Menu")},
	{2028, kcShowChannelPluginCtxMenu, UL_("Show Channel Plugin Context (Right-Click) Menu")},
	{2029, kcViewToggle, UL_("Toggle Between Upper / Lower View") },
	{2030, kcFileSaveOPL, UL_("File/Export OPL Register Dump") },
	{2031, kcSampleLoadRaw, UL_("Load Raw Sample")},
	{2032, kcTogglePatternPlayRow, UL_("Toggle Row Playback when Navigating")},
	{2033, kcInsNoteMapTransposeSamples, UL_("Transpose Samples / Reset Map") },
	{KeyCommand::Hidden, kcPrevEntryInColumnSelect, UL_("Select to previous entry in column")},
	{KeyCommand::Hidden, kcNextEntryInColumnSelect, UL_("Select to next entry in column")},
	{2034, kcTreeViewOpen, UL_("Open / View Item")},
	{2035, kcTreeViewPlay, UL_("Play Item")},
	{2036, kcTreeViewInsert, UL_("Insert Item")},
	{2037, kcTreeViewDuplicate, UL_("Duplicate Item")},
	{2038, kcTreeViewDelete, UL_("Delete Item")},
	{2039, kcTreeViewRename, UL_("Rename Item / Send To Editor")},
	{2040, kcTreeViewFind, UL_("Find in Instrument Library")},
	{2041, kcTreeViewSortByName, UL_("Sort Instrument Library By Name")},
	{2042, kcTreeViewSortByDate, UL_("Sort Instrument Library By Date")},
	{2043, kcTreeViewSortBySize, UL_("Sort Instrument Library By Size")},
	{2044, kcTreeViewSendToEditorInsertNew, UL_("Send To Editor (Insert New)")},
	{2045, kcPlayStopSong, UL_("Play Song / Stop Song")},
	{2046, kcTreeViewDeletePermanently, UL_("Delete Item Permanently")},
	{2047, kcSampleToggleFollowPlayCursor, UL_("Toggle Follow Sample Play Cursor")},
	{2048, kcPatternScrollLeft, UL_("Scroll Left")},
	{2049, kcPatternScrollRight, UL_("Scroll Right") },
	{2050, kcPatternScrollUp, UL_("Scroll Up")},
	{2051, kcPatternScrollDown, UL_("Scroll Down")},
	{2052, kcPlaySongFromCursorPause, UL_("Play Song from Cursor / Pause")},
	{2053, kcPlaySongFromPatternPause, UL_("Play Song from Pattern Start / Pause")},
	{2054, kcSampleFinetuneUp, UL_("Increment Finetune")},
	{2055, kcSampleFinetuneDown, UL_("Decrement Finetune")},
	{2056, kcTreeViewSwitchViews, UL_("Switch between Upper / Lower Tree View")},
	{2057, kcTreeViewFolderUp, UL_("Go to Parent Folder")},
	{KeyCommand::Hidden, kcTransposeUpStop, UL_("Stop Transpose +1")},
	{KeyCommand::Hidden, kcTransposeDownStop, UL_("Stop Transpose -1")},
	{KeyCommand::Hidden, kcTransposeOctUpStop, UL_("Stop Transpose +1 Octave")},
	{KeyCommand::Hidden, kcTransposeOctDownStop, UL_("Stop Transpose -1 Octave")},
	{KeyCommand::Hidden, kcTransposeCustomStop, UL_("Stop Transpose Custom")},
	{KeyCommand::Hidden, kcTransposeCustomQuickStop, UL_("Stop Transpose Custom (Quick)")},
	{KeyCommand::Hidden, kcDataEntryUpStop, UL_("Stop Data Entry +1")},
	{KeyCommand::Hidden, kcDataEntryDownStop, UL_("Stop Data Entry -1")},
	{KeyCommand::Hidden, kcDataEntryUpCoarseStop, UL_("Stop Data Entry Up (Coarse)")},
	{KeyCommand::Hidden, kcDataEntryDownCoarseStop, UL_("Stop Data Entry Down (Coarse)")},
	{2058, kcPrevOrderAtMeasureEnd, UL_("Previous Order (Transition at end of current measure)")},
	{2059, kcNextOrderAtMeasureEnd, UL_("Next Order (Transition at end of current measure)")},
	{2060, kcPrevOrderAtBeatEnd, UL_("Previous Order (Transition at end of current beat)")},
	{2061, kcNextOrderAtBeatEnd, UL_("Next Order (Transition at end of current beat)")},
	{2062, kcPrevOrderAtRowEnd, UL_("Previous Order (Transition at end of current row)")},
	{2063, kcNextOrderAtRowEnd, UL_("Next Order (Transition at end of current row)")},
	{2064, kcOrderlistQueueAtPatternEnd, UL_("Queue Pattern (Transition at end of current pattern)")},
	{2065, kcOrderlistQueueAtMeasureEnd, UL_("Queue Pattern (Transition at end of current measure)")},
	{2066, kcOrderlistQueueAtBeatEnd, UL_("Queue Pattern (Transition at end of current beat)")},
	{2067, kcOrderlistQueueAtRowEnd, UL_("Queue Pattern (Transition at end of current row)")},
	{2068, kcSampleConvertNormalLoopToSustain, UL_("Convert Normal Loop to Sustain Loop")},
	{2069, kcSampleConvertSustainLoopToNormal, UL_("Convert Sustain Loop to Normal Loop")},
	{2096, kcGotoNoteColumn, UL_("Go to note column")},
	{2097, kcGotoInstrColumn, UL_("Go to instrument column")},
	{2098, kcGotoVolumeColumn, UL_("Go to volume effect column")},
	{2099, kcGotoCommandColumn, UL_("Go to effect command column")},
	{2100, kcGotoParamColumn, UL_("Go to effect parameter column")},
	{2101, kcContextMenu, UL_("Open Context Menu")},
	{2102, kcOrderlistStreamExport, UL_("Stream Export")},
	{2103, kcToggleVisibilityInstrColumn, UL_("Toggle Instrument Column Visibility")},
	{2104, kcToggleVisibilityVolumeColumn, UL_("Toggle Volume Column Visibility")},
	{2105, kcToggleVisibilityEffectColumn, UL_("Toggle Effect Column Visibility")},
	{2106, kcFileOpenTemplate, UL_("File/Open Template")},
	{2107, kcSetVolumeA, UL_("Set volume digit A")},
	{2108, kcSetVolumeB, UL_("Set volume digit B")},
	{2109, kcSetVolumeC, UL_("Set volume digit C")},
	{2110, kcSetVolumeD, UL_("Set volume digit D")},
	{2111, kcSetVolumeE, UL_("Set volume digit E")},
	{2112, kcSetVolumeF, UL_("Set volume digit F")},
	{2113, kcToggleOctaveTransposeMIDI, UL_("Toggle Apply Octave Transpose to incoming MIDI Notes")},
	{2114, kcToggleContinueSongOnMIDINote, UL_("Toggle Continue Song when MIDI Note is received")},
	{2115, kcToggleContinueSongOnMIDIPlayEvents, UL_("Toggle Respond to Play / Continue / Stop Song MIDI messages")},
	{2116, kcToggleRecordMIDIVelocity, UL_("Toggle Record MIDI Velocity")},
	{2117, kcToggleRecordMIDIPitchBend, UL_("Toggle Record MIDI Pitch Bend")},
	{2118, kcToggleRecordMIDICCs, UL_("Toggle Record MIDI CCs")},
	{2119, kcToggleMetronome, UL_("Toggle Metronome")},
	{2120, kcPatternExpand, UL_("Expand Pattern")},
	{2121, kcPatternShrink, UL_("Shrink Pattern")},
	{2122, kcSampleToggleNormalLoop, UL_("Toggle Loop")},
	{2123, kcSampleToggleSustainLoop, UL_("Toggle Sustain Loop")},
	{2124, kcSampleSendSelectionToNew, UL_("Send Selection to New Sample Slot")},
	{2125, kcTreeViewMoveUp, UL_("Move Item Up")},
	{2126, kcTreeViewMoveDown, UL_("Move Item Down")},
	{2127, kcSampleSliceGrid, UL_("Slice at grid") },
	{2128, kcSampleNormalizeAll, UL_("Normalize All Samples")},
	{2129, kcSampleRemoveDCOffsetAll, UL_("Remove DC Offset from All Samples")},
	{2130, kcSampleResampleAll, UL_("Resample All Samples")},
	{2131, kcSample8BitAll, UL_("Convert All Samples to 8-Bit")},
	{2132, kcSampleStereo, UL_("Convert to Stereo")},
};
// clang-format on


// Get command descriptions etc.. loaded up.
void CCommandSet::SetupCommands()
{
	for(const auto &def : CommandDefinitions)
	{
		m_commands[def.cmd] = {def.uid, def.description};
	}

	for(int j = kcStartSampleCues; j <= kcEndSampleCues; j++)
	{
		mpt::ustring s = MPT_UFORMAT("Preview / Set Sample Cue {}")(j - kcStartSampleCues + 1);
		m_commands[j] = {static_cast<uint32>(1924 + j - kcStartSampleCues), s};
	}
	static_assert(1924 + kcEndSampleCues - kcStartSampleCues < 1950);

	// Automatically generated note entry keys in non-pattern contexts
	for(const auto &[contextStartNotes, contextStopNotes] : NoteRanges)
	{
		if(contextStartNotes == kcVPStartNotes)
			continue;

		for(int i = kcVPStartNotes; i <= kcVPEndNotes; i++)
		{
			m_commands[i - kcVPStartNotes + contextStartNotes] = {KeyCommand::Hidden, m_commands[i].name};
		}
		for(int i = kcVPStartNoteStops; i <= kcVPEndNoteStops; i++)
		{
			m_commands[i - kcVPStartNoteStops + contextStopNotes] = {KeyCommand::Hidden, m_commands[i].name};
		}
	}

#ifdef MPT_BUILD_DEBUG
	// Ensure that every visible command has a unique ID, and all commands have a valid context
	for(CommandID i = kcFirst; i < kcNumCommands; i = static_cast<CommandID>(i + 1))
	{
		MPT_ASSERT(ContextFromCommand(i) != kCtxUnknownContext);
		if(m_commands[i].ID() != 0 || !m_commands[i].IsHidden())
		{
			for(size_t j = i + 1; j < kcNumCommands; j++)
			{
				if(m_commands[i].ID() == m_commands[j].ID())
				{
					LOG_COMMANDSET(MPT_UFORMAT("Duplicate or unset command UID: {}\n")(m_commands[i].ID()));
					MPT_ASSERT_NOTREACHED();
				}
			}
		}
	}
#endif  // MPT_BUILD_DEBUG
}


// Command Manipulation


mpt::ustring CCommandSet::Add(KeyCombination kc, CommandID cmd, bool overwrite, int pos, bool checkEventConflict)
{
	kc.Context(ContextFromCommand(cmd));
	auto &kcList = m_commands[cmd].kcList;

	// Avoid duplicate
	if(mpt::contains(kcList, kc))
		return mpt::ustring{};

	// Check that this keycombination isn't already assigned (in this context), except for dummy keys
	mpt::ustring report;
	if(auto conflictCmd = IsConflicting(kc, cmd, checkEventConflict); conflictCmd.first != kcNull)
	{
		if(!overwrite)
		{
			return mpt::ustring{};
		} else
		{
			report = FormatConflict(kc, conflictCmd.first, conflictCmd.second);
			LOG_COMMANDSET(mpt::ToUnicode(report));
		}
	}

	kcList.insert((pos < 0) ? kcList.end() : (kcList.begin() + pos), kc);

	//enfore rules on CommandSet
	EnforceAll(kc, cmd, true);
	return report;
}


mpt::ustring CCommandSet::FormatConflict(KeyCombination kc, CommandID conflictCommand, KeyCombination conflictCombination) const
{
	if(IsCrossContextConflict(kc, conflictCombination))
		return UL_("May conflict with ") + GetCommandText(conflictCommand) + UL_(" in ") + conflictCombination.GetContextText();
	else
		return UL_("Conflicts with ") + GetCommandText(conflictCommand) + UL_(" in same context");
}

std::pair<CommandID, KeyCombination> CCommandSet::IsConflicting(KeyCombination kc, CommandID cmd, bool checkEventConflict, bool checkSameCommand) const
{
	if(m_commands[cmd].IsDummy())  // no need to search if we are adding a dummy key
		return {kcNull, KeyCombination()};
	
	for(int pass = 0; pass < 2; pass++)
	{
		// In the first pass, only look for conflicts in the same context, since
		// such conflicts are errors. Cross-context conflicts only emit warnings.
		for(int curCmd = kcFirst; curCmd < kcNumCommands; curCmd++)
		{
			if(m_commands[curCmd].IsDummy() || (!checkSameCommand && cmd == curCmd))
				continue;

			for(auto &curKc : m_commands[curCmd].kcList)
			{
				if(pass == 0 && curKc.Context() != kc.Context())
					continue;

				if(KeyCombinationConflict(curKc, kc, checkEventConflict))
				{
					return {(CommandID)curCmd, curKc};
				}
			}
		}
	}

	return std::make_pair(kcNull, KeyCombination());
}


void CCommandSet::Remove(int pos, CommandID cmd)
{
	if(pos >= 0 && static_cast<size_t>(pos) < m_commands[cmd].kcList.size())
	{
		Remove(m_commands[cmd].kcList[pos], cmd);
	}

	LOG_COMMANDSET(UL_("Failed to remove a key: keychoice out of range."));
}


void CCommandSet::Remove(KeyCombination kc, CommandID cmd)
{
	auto &kcList = m_commands[cmd].kcList;
	auto index = std::find(kcList.begin(), kcList.end(), kc);
	if (index != kcList.end())
	{
		kcList.erase(index);
		LOG_COMMANDSET(UL_("Removed a key"));
		EnforceAll(kc, cmd, false);
	} else
	{
		LOG_COMMANDSET(UL_("Failed to remove a key as it was not found"));
	}
}


void CCommandSet::EnforceAll(KeyCombination inKc, CommandID inCmd, bool adding)
{
	//World's biggest, most confusing method. :)
	//Needs refactoring. Maybe make lots of Rule subclasses, each with their own Enforce() method?
	KeyCombination curKc;	// for looping through key combinations
	KeyCombination newKc;	// for adding new key combinations

	if(m_enforceRule[krAllowNavigationWithSelection])
	{
		// When we get a new navigation command key, we need to
		// make sure this navigation will work when any selection key is pressed
		if(inCmd >= kcStartPatNavigation && inCmd <= kcEndPatNavigation)
		{//Check that it is a nav cmd
			CommandID cmdNavSelection = (CommandID)(kcStartPatNavigationSelect + (inCmd-kcStartPatNavigation));
			for(auto &kc :m_commands[kcSelect].kcList)
			{//for all selection modifiers
				newKc = inKc;
				newKc.Modifier(kc);	//Add selection modifier's modifiers to this command
				if(adding)
				{
					LOG_COMMANDSET(MPT_UFORMAT("Enforcing rule krAllowNavigationWithSelection - adding key with modifier:{} to command: {}")(newKc.Modifier().GetRaw(), cmdNavSelection));
					Add(newKc, cmdNavSelection, false);
				} else
				{
					LOG_COMMANDSET(MPT_UFORMAT("Enforcing rule krAllowNavigationWithSelection - removing key with modifier:{} to command: {}")(newKc.Modifier().GetRaw(), cmdNavSelection));
					Remove(newKc, cmdNavSelection);
				}
			}
		}
		// Same applies for orderlist navigation
		else if(inCmd >= kcStartOrderlistNavigation && inCmd <= kcEndOrderlistNavigation)
		{//Check that it is a nav cmd
			CommandID cmdNavSelection = (CommandID)(kcStartOrderlistNavigationSelect+ (inCmd-kcStartOrderlistNavigation));
			for(auto &kc : m_commands[kcSelect].kcList)
			{//for all selection modifiers
				newKc = inKc;
				newKc.AddModifier(kc);	//Add selection modifier's modifiers to this command
				if(adding)
				{
					LOG_COMMANDSET(MPT_UFORMAT("Enforcing rule krAllowNavigationWithSelection - adding key with modifier:{} to command: {}")(newKc.Modifier().GetRaw(), cmdNavSelection));
					Add(newKc, cmdNavSelection, false);
				} else
				{
					LOG_COMMANDSET(MPT_UFORMAT("Enforcing rule krAllowNavigationWithSelection - removing key with modifier:{} to command: {}")(newKc.Modifier().GetRaw(), cmdNavSelection));
					Remove(newKc, cmdNavSelection);
				}
			}
		}
		// When we get a new selection key, we need to make sure that
		// all navigation commands will work with this selection key pressed
		else if(inCmd == kcSelect)
		{
			// check that is is a selection
			for(int curCmd=kcStartPatNavigation; curCmd<=kcEndPatNavigation; curCmd++)
			{
				// for all nav commands
				for(auto &kc : m_commands[curCmd].kcList)
				{
					// for all keys for this command
					CommandID cmdNavSelection = (CommandID)(kcStartPatNavigationSelect + (curCmd-kcStartPatNavigation));
					newKc = kc;					// get all properties from the current nav cmd key
					newKc.AddModifier(inKc);	// and the new selection modifier
					if(adding)
					{
						LOG_COMMANDSET(MPT_UFORMAT("Enforcing rule krAllowNavigationWithSelection - adding key:{} with modifier:{} to command: {}")(curCmd, inKc.Modifier().GetRaw(), cmdNavSelection));
						Add(newKc, cmdNavSelection, false);
					} else
					{
						LOG_COMMANDSET(MPT_UFORMAT("Enforcing rule krAllowNavigationWithSelection - removing key:{} with modifier:{} to command: {}")(curCmd, inKc.Modifier().GetRaw(), cmdNavSelection));
						Remove(newKc, cmdNavSelection);
					}
				}
			} // end all nav commands
			for(int curCmd = kcStartOrderlistNavigation; curCmd <= kcEndOrderlistNavigation; curCmd++)
			{// for all nav commands
				for(auto &kc : m_commands[curCmd].kcList)
				{// for all keys for this command
					CommandID cmdNavSelection = (CommandID)(kcStartOrderlistNavigationSelect+ (curCmd-kcStartOrderlistNavigation));
					newKc = kc;					// get all properties from the current nav cmd key
					newKc.AddModifier(inKc);	// and the new selection modifier
					if(adding)
					{
						LOG_COMMANDSET(MPT_UFORMAT("Enforcing rule krAllowNavigationWithSelection - adding key:{} with modifier:{} to command: {}")(curCmd, inKc.Modifier().GetRaw(), cmdNavSelection));
						Add(newKc, cmdNavSelection, false);
					} else
					{
						LOG_COMMANDSET(MPT_UFORMAT("Enforcing rule krAllowNavigationWithSelection - removing key:{} with modifier:{} to command: {}")(curCmd, inKc.Modifier().GetRaw(), cmdNavSelection));
						Remove(newKc, cmdNavSelection);
					}
				}
			} // end all nav commands
		}
	} // end krAllowNavigationWithSelection

	if(m_enforceRule[krAllowSelectionWithNavigation])
	{
		KeyCombination newKcSel;

		// When we get a new navigation command key, we need to ensure
		// all selection keys will work even when this new selection key is pressed
		if(inCmd >= kcStartPatNavigation && inCmd <= kcEndPatNavigation)
		{//if this is a navigation command
			for(auto &kc : m_commands[kcSelect].kcList)
			{//for all deselection modifiers
				newKcSel = kc;				// get all properties from the selection key
				newKcSel.AddModifier(inKc);	// add modifiers from the new nav command
				if(adding)
				{
					LOG_COMMANDSET(UL_("Enforcing rule krAllowSelectionWithNavigation: adding  removing kcSelectWithNav and kcSelectOffWithNav"));
					Add(newKcSel, kcSelectWithNav, false);
				} else
				{
					LOG_COMMANDSET(UL_("Enforcing rule krAllowSelectionWithNavigation: removing kcSelectWithNav and kcSelectOffWithNav"));
					Remove(newKcSel, kcSelectWithNav);
				}
			}
		}
		// Same for orderlist navigation
		if(inCmd >= kcStartOrderlistNavigation && inCmd <= kcEndOrderlistNavigation)
		{//if this is a navigation command
			for(auto &kc : m_commands[kcSelect].kcList)
			{//for all deselection modifiers
				newKcSel = kc;				// get all properties from the selection key
				newKcSel.AddModifier(inKc);	// add modifiers from the new nav command
				if(adding)
				{
					LOG_COMMANDSET(UL_("Enforcing rule krAllowSelectionWithNavigation: adding  removing kcSelectWithNav and kcSelectOffWithNav"));
					Add(newKcSel, kcSelectWithNav, false);
				} else
				{
					LOG_COMMANDSET(UL_("Enforcing rule krAllowSelectionWithNavigation: removing kcSelectWithNav and kcSelectOffWithNav"));
					Remove(newKcSel, kcSelectWithNav);
				}
			}
		}
		// When we get a new selection key, we need to ensure it will work even when
		// any navigation key is pressed
		else if(inCmd == kcSelect)
		{
			for(int curCmd = kcStartPatNavigation; curCmd <= kcEndPatNavigation; curCmd++)
			{//for all nav commands
				for(auto &kc : m_commands[curCmd].kcList)
				{// for all keys for this command
					newKcSel = inKc;			// get all properties from the selection key
					newKcSel.AddModifier(kc);	//add the nav keys' modifiers
					if(adding)
					{
						LOG_COMMANDSET(MPT_UFORMAT("Enforcing rule krAllowSelectionWithNavigation - adding key:{} with modifier:{} to command: {}")(curCmd, inKc.Modifier().GetRaw(), kcSelectWithNav));
						Add(newKcSel, kcSelectWithNav, false);
					} else
					{
						LOG_COMMANDSET(MPT_UFORMAT("Enforcing rule krAllowSelectionWithNavigation - removing key:{} with modifier:{} to command: {}")(curCmd, inKc.Modifier().GetRaw(), kcSelectWithNav));
						Remove(newKcSel, kcSelectWithNav);
					}
				}
			} // end all nav commands

			for(int curCmd = kcStartOrderlistNavigation; curCmd <= kcEndOrderlistNavigation; curCmd++)
			{//for all nav commands
				for(auto &kc : m_commands[curCmd].kcList)
				{// for all keys for this command
					newKcSel=inKc;				// get all properties from the selection key
					newKcSel.AddModifier(kc);	//add the nav keys' modifiers
					if(adding)
					{
						LOG_COMMANDSET(MPT_UFORMAT("Enforcing rule krAllowSelectionWithNavigation - adding key:{} with modifier:{} to command: {}")(curCmd, inKc.Modifier().GetRaw(), kcSelectWithNav));
						Add(newKcSel, kcSelectWithNav, false);
					} else
					{
						LOG_COMMANDSET(MPT_UFORMAT("Enforcing rule krAllowSelectionWithNavigation - removing key:{} with modifier:{} to command: {}")(curCmd, inKc.Modifier().GetRaw(), kcSelectWithNav));
						Remove(newKcSel, kcSelectWithNav);
					}
				}
			} // end all nav commands
		}

	}

	// if we add a selector or a copy selector, we need it to switch off when we release the key.
	if (m_enforceRule[krAutoSelectOff])
	{
		KeyCombination newKcDeSel;
		CommandID cmdOff = kcNull;
		switch (inCmd)
		{
			case kcSelect:					cmdOff = kcSelectOff;				break;
			case kcSelectWithNav:			cmdOff = kcSelectOffWithNav;		break;
			case kcCopySelect:				cmdOff = kcCopySelectOff;			break;
			case kcCopySelectWithNav:		cmdOff = kcCopySelectOffWithNav;	break;
			case kcSelectWithCopySelect:	cmdOff = kcSelectOffWithCopySelect; break;
			case kcCopySelectWithSelect:	cmdOff = kcCopySelectOffWithSelect; break;
			default: break;
		}

		if(cmdOff != kcNull)
		{
			newKcDeSel = inKc;
			newKcDeSel.EventType(kKeyEventUp);

			// Register key-up when releasing any of the modifiers.
			// Otherwise, select key combos might get stuck. Example:
			// [Ctrl Down] [Alt Down] [Ctrl Up] [Alt Up] After this action, copy select (Ctrl+Drag) would still be activated without this code.
			const uint32 maxMod = TrackerSettings::Instance().MiscDistinguishModifiers ? MaxMod : (MaxMod & ~(ui::KeyModRightShift | ui::KeyModRightControl | ui::KeyModRightAlt));
			for(uint32 i = 0; i <= maxMod; i++)
			{
				// Avoid Windows key, so that it won't detected as being actively used
				if(i & ui::KeyModExtended)
					continue;
				newKcDeSel.Modifier(static_cast<Modifiers>(i));
				//newKcDeSel.mod&=~CodeToModifier(inKc.code);		//<-- Need to get rid of right modifier!!

				if (adding)
					Add(newKcDeSel, cmdOff, false);
				else
					Remove(newKcDeSel, cmdOff);
			}
		}

	}
	// Allow combinations of copyselect and select
	if(m_enforceRule[krAllowSelectCopySelectCombos])
	{
		KeyCombination newKcSel, newKcCopySel;
		if(inCmd==kcSelect)
		{
			// On getting a new selection key, make this selection key work with all copy selects' modifiers
			// On getting a new selection key, make all copyselects work with this key's modifiers
			for(auto &kc : m_commands[kcCopySelect].kcList)
			{
				newKcSel=inKc;
				newKcSel.AddModifier(kc);
				newKcCopySel = kc;
				newKcCopySel.AddModifier(inKc);
				LOG_COMMANDSET(UL_("Enforcing rule krAllowSelectCopySelectCombos"));
				if(adding)
				{
					Add(newKcSel, kcSelectWithCopySelect, false);
					Add(newKcCopySel, kcCopySelectWithSelect, false);
				} else
				{
					Remove(newKcSel, kcSelectWithCopySelect);
					Remove(newKcCopySel, kcCopySelectWithSelect);
				}
			}
		}
		if(inCmd == kcCopySelect)
		{
			// On getting a new copyselection key, make this copyselection key work with all selects' modifiers
			// On getting a new copyselection key, make all selects work with this key's modifiers
			for(auto &kc : m_commands[kcSelect].kcList)
			{
				newKcSel = kc;
				newKcSel.AddModifier(inKc);
				newKcCopySel = inKc;
				newKcCopySel.AddModifier(kc);
				LOG_COMMANDSET(UL_("Enforcing rule krAllowSelectCopySelectCombos"));
				if(adding)
				{
					Add(newKcSel, kcSelectWithCopySelect, false);
					Add(newKcCopySel, kcCopySelectWithSelect, false);
				} else
				{
					Remove(newKcSel, kcSelectWithCopySelect);
					Remove(newKcCopySel, kcCopySelectWithSelect);
				}
			}
		}
	}


	// Lock Notes to Chords
	if (m_enforceRule[krLockNotesToChords])
	{
		if (inCmd>=kcVPStartNotes && inCmd<=kcVPEndNotes)
		{
			int noteOffset = inCmd - kcVPStartNotes;
			for(auto &kc : m_commands[kcChordModifier].kcList)
			{//for all chord modifier keys
				newKc = inKc;
				newKc.AddModifier(kc);
				if (adding)
				{
					LOG_COMMANDSET(UL_("Enforcing rule krLockNotesToChords: auto adding in a chord command"));
					Add(newKc, (CommandID)(kcVPStartChords+noteOffset), false);
				}
				else
				{
					LOG_COMMANDSET(UL_("Enforcing rule krLockNotesToChords: auto removing a chord command"));
					Remove(newKc, (CommandID)(kcVPStartChords+noteOffset));
				}
			}
		}
		if (inCmd==kcChordModifier)
		{
			int noteOffset;
			for (int curCmd=kcVPStartNotes; curCmd<=kcVPEndNotes; curCmd++)
			{//for all notes
				for(auto kc : m_commands[curCmd].kcList)
				{//for all keys for this note
					noteOffset = curCmd - kcVPStartNotes;
					kc.AddModifier(inKc);
					if (adding)
					{
						LOG_COMMANDSET(UL_("Enforcing rule krLockNotesToChords: auto adding in a chord command"));
						Add(kc, (CommandID)(kcVPStartChords+noteOffset), false);
					}
					else
					{
						LOG_COMMANDSET(UL_("Enforcing rule krLockNotesToChords: auto removing a chord command"));
						Remove(kc, (CommandID)(kcVPStartChords+noteOffset));
					}
				}
			}
		}
	}


	// Auto set note off on release
	if (m_enforceRule[krNoteOffOnKeyRelease])
	{
		int cmdOffset = int32_min;
		if(inCmd >= kcVPStartNotes && inCmd <= kcVPEndNotes)
			cmdOffset = kcVPStartNoteStops - kcVPStartNotes;
		else if(inCmd >= kcVPStartChords && inCmd <= kcVPEndChords)
			cmdOffset = kcVPStartChordStops - kcVPStartChords;
		else if(inCmd >= kcSetOctave0 && inCmd <= kcSetOctave9)
			cmdOffset = kcSetOctaveStop0 - kcSetOctave0;
		else if(inCmd >= kcBeginTranspose && inCmd <= kcEndTranspose)
			cmdOffset = kcBeginTransposeStop - kcBeginTranspose;

		if(cmdOffset != int32_min)
		{
			newKc = inKc;
			newKc.EventType(kKeyEventUp);
			if (adding)
			{
				LOG_COMMANDSET(UL_("Enforcing rule krNoteOffOnKeyRelease: adding note off command"));
				Add(newKc, static_cast<CommandID>(inCmd + cmdOffset), false);
			} else
			{
				LOG_COMMANDSET(UL_("Enforcing rule krNoteOffOnKeyRelease: removing note off command"));
				Remove(newKc, static_cast<CommandID>(inCmd + cmdOffset));
			}
		}
	}

	// Reassign freed number keys to octaves
	if (m_enforceRule[krReassignDigitsToOctaves] && !adding)
	{
		if ( (inKc.Modifier() == ModNone) &&	//no modifier
			 ( (inKc.Context() == kCtxViewPatternsNote) || (inKc.Context() == kCtxViewPatterns) ) && //note scope or pattern scope
			 ( ('0'<=inKc.KeyCode() && inKc.KeyCode()<='9') || (ui::Key_NUMPAD0<=inKc.KeyCode() && inKc.KeyCode()<=ui::Key_NUMPAD9) ) )  //is number key
		{
				newKc = KeyCombination(kCtxViewPatternsNote, ModNone, inKc.KeyCode(), kKeyEventDown);
				int offset = ('0'<=inKc.KeyCode() && inKc.KeyCode()<='9') ? newKc.KeyCode()-'0' : newKc.KeyCode()-ui::Key_NUMPAD0;
				Add(newKc, (CommandID)(kcSetOctave0 + (newKc.KeyCode()-offset)), false);
		}
	}
	// Add spacing
	if (m_enforceRule[krAutoSpacing])
	{
		if (inCmd == kcSetSpacing && adding)
		{
			newKc = KeyCombination(kCtxViewPatterns, inKc.Modifier(), 0, kKeyEventDown);
			for (char i = 0; i <= 9; i++)
			{
				newKc.KeyCode('0' + i);
				Add(newKc, (CommandID)(kcSetSpacing0 + i), false);
				newKc.KeyCode(ui::Key_NUMPAD0 + i);
				Add(newKc, (CommandID)(kcSetSpacing0 + i), false);
			}
		} else if (!adding && (inCmd < kcSetSpacing || inCmd > kcSetSpacing9))
		{
			// Re-add combinations that might have been overwritten by another command
			if(('0' <= inKc.KeyCode() && inKc.KeyCode() <= '9') || (ui::Key_NUMPAD0 <= inKc.KeyCode() && inKc.KeyCode() <= ui::Key_NUMPAD9))
			{
				for(const auto &spacing : m_commands[kcSetSpacing].kcList)
				{
					newKc = KeyCombination(kCtxViewPatterns, spacing.Modifier(), inKc.KeyCode(), spacing.EventType());
					if('0' <= inKc.KeyCode() && inKc.KeyCode() <= '9')
						Add(newKc, static_cast<CommandID>(kcSetSpacing0 + inKc.KeyCode() - '0'), false);
					else if(ui::Key_NUMPAD0 <= inKc.KeyCode() && inKc.KeyCode() <= ui::Key_NUMPAD9)
						Add(newKc, static_cast<CommandID>(kcSetSpacing0 + inKc.KeyCode() - ui::Key_NUMPAD0), false);
				}
			}

		}
	}
	if (m_enforceRule[krPropagateNotes])
	{
		if((inCmd >= kcVPStartNotes && inCmd <= kcVPEndNotes) || (inCmd >= kcVPStartNoteStops && inCmd <= kcVPEndNoteStops))
		{
			const bool areNoteStarts = (inCmd >= kcVPStartNotes && inCmd <= kcVPEndNotes);
			const auto startNote = areNoteStarts ? kcVPStartNotes : kcVPStartNoteStops;
			const auto noteOffset = inCmd - startNote;
			for(const auto &range : NoteRanges)
			{
				const auto contextStartNote = areNoteStarts ? range.first : range.second;
				const auto context = ContextFromCommand(contextStartNote);

				if(contextStartNote == startNote)
					continue;

				newKc = inKc;
				newKc.Context(context);

				if(adding)
				{
					LOG_COMMANDSET(UL_("Enforcing rule krPropagateNotes: adding Note on/off"));
					Add(newKc, static_cast<CommandID>(contextStartNote + noteOffset), false);
				} else
				{
					LOG_COMMANDSET(UL_("Enforcing rule krPropagateNotes: removing Note on/off"));
					Remove(newKc, static_cast<CommandID>(contextStartNote + noteOffset));
				}
			}
		} else if(inCmd == kcNoteCut || inCmd == kcNoteOff || inCmd == kcNoteFade)
		{
			// Stop preview in instrument browser
			KeyCombination newKcTree = inKc;
			newKcTree.Context(kCtxViewTree);
			if(adding)
			{
				Add(newKcTree, kcTreeViewStopPreview, false);
			} else
			{
				Remove(newKcTree, kcTreeViewStopPreview);
			}
		}
	}
	if (m_enforceRule[krCheckModifiers])
	{
		// for all commands that must be modifiers
		for (auto curCmd : ModifierCommands)
		{
			//for all of this command's key combinations
			for (auto &kc : m_commands[curCmd].kcList)
			{
				if(!kc.IsModifierCombination())
				{
					//replace with dummy
					kc.Modifier(ModShift);
					kc.KeyCode(0);
					kc.EventType(kKeyEventNone);
				}
			}

		}
	}
	if (m_enforceRule[krPropagateSampleManipulation])
	{
		static constexpr CommandID propagateCmds[] = {kcSampleLoad, kcSampleSave, kcSampleNew};
		static constexpr CommandID translatedCmds[] = {kcInstrumentLoad, kcInstrumentSave, kcInstrumentNew};
		if(const auto propCmd = std::find(std::begin(propagateCmds), std::end(propagateCmds), inCmd); propCmd != std::end(propagateCmds))
		{
			//propagate to InstrumentView
			const auto newCmd = translatedCmds[std::distance(std::begin(propagateCmds), propCmd)];
			if(kcFirst <= inCmd && inCmd < kcNumCommands)
			{
				m_commands[newCmd].kcList.reserve(m_commands[inCmd].kcList.size());
				for(auto kc : m_commands[inCmd].kcList)
				{
					kc.Context(kCtxViewInstruments);
					m_commands[newCmd].kcList.push_back(kc);
				}
			}
		}

	}

/*	if (enforceRule[krFoldEffectColumnAnd])
	{
		if (inKc.ctx == kCtxViewPatternsFX) {
			KeyCombination newKc = inKc;
			newKc.ctx = kCtxViewPatternsFXparam;
			if (adding)	{
				Add(newKc, inCmd, false);
			} else {
				Remove(newKc, inCmd);
			}
		}
		if (inKc.ctx == kCtxViewPatternsFXparam) {
			KeyCombination newKc = inKc;
			newKc.ctx = kCtxViewPatternsFX;
			if (adding)	{
				Add(newKc, inCmd, false);
			} else {
				Remove(newKc, inCmd);
			}
		}
	}
*/
}


//Generate a keymap from a command set
void CCommandSet::GenKeyMap(KeyMap &km)
{
	std::vector<KeyEventType> eventTypes;
	std::vector<InputTargetContext> contexts;

	km.clear();

	const bool allowDupes = TrackerSettings::Instance().MiscAllowMultipleCommandsPerKey;

	// Copy commandlist content into map:
	for(uint32 cmd = kcFirst; cmd < kcNumCommands; cmd++)
	{
		if(m_commands[cmd].IsDummy())
			continue;

		for(auto curKc : m_commands[cmd].kcList)
		{
			eventTypes.clear();
			contexts.clear();

			// Handle keyEventType mask.
			if(curKc.EventType() & kKeyEventDown)
				eventTypes.push_back(kKeyEventDown);
			if(curKc.EventType() & kKeyEventUp)
				eventTypes.push_back(kKeyEventUp);
			if(curKc.EventType() & kKeyEventRepeat)
				eventTypes.push_back(kKeyEventRepeat);
			//MPT_ASSERT(eventTypes.GetSize()>0);

			// Handle super-contexts (contexts that represent a set of sub contexts)
			if(curKc.Context() == kCtxViewPatterns)
				contexts.insert(contexts.end(), {kCtxViewPatternsNote, kCtxViewPatternsIns, kCtxViewPatternsVol, kCtxViewPatternsFX, kCtxViewPatternsFXparam});
			else if(curKc.Context() == kCtxCtrlPatterns)
				contexts.push_back(kCtxCtrlOrderlist);
			else
				contexts.push_back(curKc.Context());

			for(auto ctx : contexts)
			{
				for(auto event : eventTypes)
				{
					KeyCombination kc(ctx, curKc.Modifier(), curKc.KeyCode(), event);
					if(!allowDupes)
					{
						KeyMapRange dupes = km.equal_range(kc);
						km.erase(dupes.first, dupes.second);
					}
					km.insert(std::make_pair(kc, static_cast<CommandID>(cmd)));
				}
			}
		}
	}
}


void CCommandSet::Copy(const CCommandSet &source)
{
	m_currentModSpecs = source.m_currentModSpecs;
	std::copy(std::begin(source.m_commands), std::end(source.m_commands), std::begin(m_commands));
}


// Export

bool CCommandSet::SaveFile(const mpt::PathString &filename)
{

/* Layout:
//----( Context1 Text (id) )----
ctx:UID:Description:Modifier:Key:EventMask
ctx:UID:Description:Modifier:Key:EventMask
...
//----( Context2 Text (id) )----
...
*/

	mpt::IO::SafeOutputFile sf(filename, std::ios::out, mpt::IO::FlushModeFromBool(TrackerSettings::Instance().MiscFlushFileBuffersOnSave));
	mpt::IO::ofstream& f = sf;
	if(!f)
	{
		ErrorBox(IDS_CANT_OPEN_FILE_FOR_WRITING);
		return false;
	}
	f << "//----------------- OpenMPT key binding definition file  ---------------\n"
	     "//- Format is:                                                         -\n"
	     "//- Context:Command ID:Modifiers:Key:KeypressEventType     //Comments  -\n"
	     "//----------------------------------------------------------------------\n"
	     "version:" << mpt::ToCharset(mpt::Charset::ASCII, Version::Current().ToUString()) << "\n";

	for(int ctx = 0; ctx < kCtxMaxInputContexts; ctx++)
	{
		f << "\n//----( " << mpt::ToCharset(mpt::Charset::UTF8, KeyCombination::GetContextText((InputTargetContext)ctx)) << " )------------\n";

		for(int cmd = kcFirst; cmd < kcNumCommands; cmd++)
		{
			if(m_commands[cmd].IsHidden())
				continue;

			for(const auto &kc : m_commands[cmd].kcList)
			{
				if(kc.Context() != ctx)
					continue;  // Sort by context

				f << ctx << ":"  // Context technically no longer needed here, just kept for backwards compatibility
					<< m_commands[cmd].ID() << ":"
					<< static_cast<int>(kc.Modifier().GetRaw()) << ":"
					<< kc.KeyCode();
				if(cmd >= kcVPStartNotes && cmd <= kcVPEndNotes)
				{
					f << "/" << ui::MapVirtualKeyToScanCode(kc.KeyCode());
				}
				f << ":"
					<< static_cast<int>(kc.EventType().GetRaw()) << "\t\t//"
					<< mpt::ToCharset(mpt::Charset::UTF8, GetCommandText((CommandID)cmd)) << ": "
					<< mpt::ToCharset(mpt::Charset::UTF8, kc.GetKeyText()) << " ("
					<< mpt::ToCharset(mpt::Charset::UTF8, kc.GetKeyEventText()) << ")\n";
			}
		}
	}

	return true;
}


bool CCommandSet::LoadFile(std::istream &iStrm, const mpt::ustring &filenameDescription)
{
	Version keymapVersion;
	KeyCombination kc;
	char s[1024];
	std::string curLine;
	std::vector<std::string> tokens;
	int l = 0;

	for(auto &cmd : m_commands)
		cmd.kcList.clear();

	mpt::ustring errText;
	int errorCount = 0;

	const std::string whitespace(" \n\r\t");
	while(iStrm.getline(s, std::size(s)))
	{
		curLine = s;
		l++;

		// Cut everything after a //, trim whitespace
		auto pos = curLine.find("//");
		if(pos != std::string::npos)
			curLine.resize(pos);
		pos = curLine.find_first_not_of(whitespace);
		if(pos == std::string::npos)
			continue;
		curLine.erase(0, pos);
		pos = curLine.find_last_not_of(whitespace);
		if(pos != std::string::npos)
			curLine.resize(pos + 1);

		if (curLine.empty())
			continue;

		tokens = mpt::split(curLine, std::string(":"));
		if(tokens.size() == 2 && !mpt::CompareNoCaseAscii(tokens[0], "version"))
		{
			// This line indicates the version of this keymap file (e.g. "version:1" on older versions, in newer version the OpenMPT version that was used to save the file)
			keymapVersion = Version::Parse(mpt::ToUnicode(mpt::Charset::ASCII, tokens[1]));
			if(keymapVersion > Version::Current())
			{
				errText += MPT_UFORMAT("Keymap was saved with OpenMPT {}, but you are running OpenMPT {}.\n")(keymapVersion, Version::Current());
			}
			continue;
		}

		// Format: ctx:UID:Description:Modifier:Key:EventMask
		CommandID cmd = kcNumCommands;
		if(tokens.size() >= 5)
		{
			cmd = FindCmd(mpt::parse<uint32>(tokens[1]));

			// Modifier
			kc.Modifier(static_cast<Modifiers>(mpt::parse<int>(tokens[2])));

			// Virtual Key code / Scan code
			uint32 vk = 0;
			auto scPos = tokens[3].find('/');
			if(scPos != std::string::npos)
			{
				// Scan code present
				const uint32 sc = mpt::parse<uint32>(tokens[3].substr(scPos + 1));
				vk = ui::MapScanCodeToVirtualKey(sc);
			}
			if(vk == 0)
			{
				vk = mpt::parse<uint32>(tokens[3]);
			}
			kc.KeyCode(vk);

			// Event
			kc.EventType(static_cast<KeyEventType>(mpt::parse<int>(tokens[4])));
		}

		// Error checking
		if(cmd < kcFirst || cmd >= kcNumCommands || tokens.size() < 4)
		{
			errorCount++;
			if (errorCount < 10)
			{
				if(tokens.size() < 4)
					errText += ui::Format(UL_("Line %d was not understood.\n"), l);
				else
					errText += ui::Format(UL_("Line %d contained an unknown command.\n"), l);
			} else if (errorCount == 10)
			{
				errText += UL_("Too many errors detected, not reporting any more.\n");
			}
		} else
		{
			Add(kc, cmd, true, -1, true);
		}
	}

	ApplyDefaultKeybindings(KeyboardPreset::MPT, keymapVersion);

	// Fix up old keymaps containing legacy commands that have been merged into other commands
	static constexpr std::pair<CommandID, CommandID> MergeCommands[] =
	{
		{kcFileSaveAsMP3, kcFileSaveAsWave},
		{kcNoteCutOld, kcNoteCut},
		{kcNoteOffOld, kcNoteOff},
		{kcNoteFadeOld, kcNoteFade},
	};
	for(const auto [from, to] : MergeCommands)
	{
		m_commands[to].kcList.insert(m_commands[to].kcList.end(), m_commands[from].kcList.begin(), m_commands[from].kcList.end());
		m_commands[from].kcList.clear();
	}

	if(!errText.empty())
	{
		Reporting::Warning(MPT_UFORMAT("The following problems have been encountered while trying to load the key binding file {}:\n{}")
			(mpt::ToUnicode(filenameDescription), errText));
	}

	return true;
}


bool CCommandSet::LoadFile(const mpt::PathString &filename)
{
	mpt::IO::ifstream fin(filename);
	if(fin.fail())
	{
		Reporting::Warning(MPT_UFORMAT("Can't open key bindings file {} for reading. Default key bindings will be used.")(filename));
		return false;
	} else
	{
		return LoadFile(fin, filename.ToUnicode());
	}
}


void CCommandSet::LoadDefaultKeymap(KeyboardPreset preset)
{
	for(auto &cmd : m_commands)
		cmd.kcList.clear();
	ApplyDefaultKeybindings(KeyboardPreset::MPT);

	if(preset == KeyboardPreset::MPT)
		return;

	const auto defaults = (preset == KeyboardPreset::IT) ? mpt::as_span(DefaultKeybindingsIT) : mpt::as_span(DefaultKeybindingsFT2);
	// Remove all pre-populated notes
	for(CommandID cmd = kcVPStartNotes; cmd <= kcVPEndNotes; cmd = static_cast<CommandID>(cmd + 1))
	{
		for(const auto &kc : m_commands[cmd].kcList)
		{
			EnforceAll(kc, cmd, false);
		}
		m_commands[cmd].kcList.clear();
	}
	// Also remove any other keys that are going to be overwritten
	for (const auto &key : defaults)
	{
		for(const auto &kc : m_commands[key.cmd].kcList)
		{
			EnforceAll(kc, key.cmd, false);
		}
		m_commands[key.cmd].kcList.clear();
	}
	ApplyDefaultKeybindings(preset);
}


void CCommandSet::ApplyDefaultKeybindings(KeyboardPreset preset, const Version onlyCommandsAfterVersion)
{
	if(m_currentModSpecs)
	{
		const auto specs = m_currentModSpecs;
		m_currentModSpecs = nullptr;
		QuickChange_SetEffects(*specs);
	}

	mpt::span<const DefaultKeybinding> defaults;
	switch(preset)
	{
	case KeyboardPreset::MPT: defaults = DefaultKeybindings; break;
	case KeyboardPreset::IT: defaults = DefaultKeybindingsIT; break;
	case KeyboardPreset::FT2: defaults = DefaultKeybindingsFT2; break;
	}

	const bool onlyNewShortcuts = onlyCommandsAfterVersion != Version{};
	CommandID lastAdded = kcNull;
	for(const auto &kb : defaults)
	{
		if(onlyNewShortcuts)
		{
			if(kb.addedInVersion <= onlyCommandsAfterVersion)
				continue;

			// Do not map shortcuts that already have custom keys assigned.
			// In particular with default note keys, this can create awkward keymaps when loading
			// e.g. an IT-style keymap and it contains two keys mapped to the same notes.
			if(kb.cmd != lastAdded && GetKeyListSize(kb.cmd) != 0)
				continue;
		}

		KeyCombination kc;
		kc.Context(ContextFromCommand(kb.cmd));
		kc.Modifier(kb.modifiers);
		kc.EventType(kb.events);

		if(kb.key & 0x8000)
		{
			// Virtual Key code / Scan code
			const uint32 vk = ui::MapScanCodeToVirtualKey(kb.key & 0x7FFF);
			if(vk)
				kc.KeyCode(vk);
			else
				kc.KeyCode(kb.key & 0x7FFF);
		} else
		{
			kc.KeyCode(kb.key);
		}

		if(auto conflictCmd = IsConflicting(kc, kb.cmd, false); conflictCmd.first != kcNull)
		{
			if(!onlyNewShortcuts && conflictCmd.second.Context() == kc.Context())
				continue;
			// Allow cross-context conflicts in case the newly added shortcut is in a more generic context
			// - unless the conflicting shortcut is the reserved dummy shortcut (which was used to prevent
			// default shortcuts from being added back before default key binding versioning was added).
			if(conflictCmd.first == kcDummyShortcut)
				continue;
			if(onlyNewShortcuts && !m_isParentContext[kc.Context()][conflictCmd.second.Context()])
				continue;
		}

		m_commands[kb.cmd].kcList.push_back(kc);
		EnforceAll(kc, kb.cmd, true);
		lastAdded = kb.cmd;
	}
}


CommandID CCommandSet::FindCmd(uint32 uid) const
{
	for(int i = kcFirst; i < kcNumCommands; i++)
	{
		if(m_commands[i].ID() == uid)
			return static_cast<CommandID>(i);
	}
	return kcNull;
}


mpt::ustring KeyCombination::GetContextText(InputTargetContext ctx)
{
	switch(ctx)
	{
		case kCtxAllContexts:			return UL_("Global Context");
		case kCtxViewGeneral:			return UL_("General Context [bottom]");
		case kCtxViewPatterns:			return UL_("Pattern Context [bottom]");
		case kCtxViewPatternsNote:		return UL_("Pattern Context [bottom] - Note Col");
		case kCtxViewPatternsIns:		return UL_("Pattern Context [bottom] - Ins Col");
		case kCtxViewPatternsVol:		return UL_("Pattern Context [bottom] - Vol Col");
		case kCtxViewPatternsFX:		return UL_("Pattern Context [bottom] - FX Col");
		case kCtxViewPatternsFXparam:	return UL_("Pattern Context [bottom] - Param Col");
		case kCtxViewSamples:			return UL_("Sample Context [bottom]");
		case kCtxViewInstruments:		return UL_("Instrument Context [bottom]");
		case kCtxViewComments:			return UL_("Comments Context [bottom]");
		case kCtxViewTree:				return UL_("Tree View");
		case kCtxCtrlGeneral:			return UL_("General Context [top]");
		case kCtxCtrlPatterns:			return UL_("Pattern Context [top]");
		case kCtxCtrlSamples:			return UL_("Sample Context [top]");
		case kCtxCtrlInstruments:		return UL_("Instrument Context [top]");
		case kCtxCtrlComments:			return UL_("Comments Context [top]");
		case kCtxCtrlOrderlist:			return UL_("Orderlist");
		case kCtxVSTGUI:				return UL_("Plugin GUI Context");
		case kCtxChannelSettings:		return UL_("Quick Channel Settings Context");
		case kCtxInsNoteMap:			return UL_("Instrument Note Map");
		case kCtxUnknownContext:
		default:						return UL_("Unknown Context");
	}
}


mpt::ustring KeyCombination::GetKeyEventText(FlagSet<KeyEventType> event)
{
	mpt::ustring text;

	bool first = true;
	if (event & kKeyEventDown)
	{
		first=false;
		text.append(UL_("KeyDown"));
	}
	if (event & kKeyEventRepeat)
	{
		if (!first) text.append(UL_("|"));
		text.append(UL_("KeyHold"));
		first=false;
	}
	if (event & kKeyEventUp)
	{
		if (!first) text.append(UL_("|"));
		text.append(UL_("KeyUp"));
	}

	return text;
}


mpt::ustring KeyCombination::GetModifierText(FlagSet<Modifiers> mod)
{
	mpt::ustring text;
	if (mod[ModShift]) text.append(UL_("Shift+"));
	if (mod[ModCtrl]) text.append(UL_("Ctrl+"));
	if (mod[ModAlt]) text.append(UL_("Alt+"));
	if (mod[ModRShift]) text.append(UL_("RShift+"));
	if (mod[ModRCtrl]) text.append(UL_("RCtrl+"));
	if (mod[ModRAlt]) text.append(UL_("RAlt+"));
	if (mod[ModWin]) text.append(UL_("Win+")); // Feature: use Windows keys as modifier keys
	if (mod[ModMidi]) text.append(UL_("MIDI"));
	return text;
}


mpt::ustring KeyCombination::GetKeyText(FlagSet<Modifiers> mod, uint32 code)
{
	mpt::ustring keyText = GetModifierText(mod);
	if(mod[ModMidi])
	{
		if(code < 0x80)
			keyText += ui::Format(UL_(" CC %u"), code);
		else
			keyText += MPT_UFORMAT(" {}{}")(mpt::ustring(NoteNamesSharp[(code & 0x7F) % 12]), (code & 0x7F) / 12);
	} else
	{
		keyText.append(ui::GetKeyName(code, IsExtended(code)));
	}
	//HACK:
	if (keyText == UL_("Ctrl+CTRL"))			keyText = UL_("Ctrl");
	else if (keyText == UL_("Alt+ALT"))		keyText = UL_("Alt");
	else if (keyText == UL_("Shift+SHIFT"))	keyText = UL_("Shift");
	else if (keyText == UL_("RCtrl+CTRL"))	keyText = UL_("RCtrl");
	else if (keyText == UL_("RAlt+ALT"))		keyText = UL_("RAlt");
	else if (keyText == UL_("RShift+SHIFT"))	keyText = UL_("RShift");

	return keyText;
}


bool KeyCombination::IsModifierCombination() const
{
	return Modifier() &&
		(KeyCode() == ui::Key_SHIFT || KeyCode() == ui::Key_CONTROL || KeyCode() == ui::Key_MENU || KeyCode() == 0
		|| KeyCode() == ui::Key_LWIN || KeyCode() == ui::Key_RWIN);  // Feature: use Windows keys as modifier keys

}


mpt::ustring CCommandSet::GetKeyTextFromCommand(CommandID c, uint32 key) const
{
	if(key < m_commands[c].kcList.size())
		return m_commands[c].kcList[0].GetKeyText();
	if(key != uint32_max)
		return mpt::ustring();
	mpt::ustring keys;
	bool addSeparator = false;
	for(auto &item : m_commands[c].kcList)
	{
		if(addSeparator)
			keys += UL_("; ");
		else
			addSeparator = true;
		keys += item.GetKeyText();
	}
	return keys;
}


// Quick Changes - modify many commands with one call.

bool CCommandSet::QuickChange_NotesRepeat(bool repeat)
{
	for (CommandID cmd = kcVPStartNotes; cmd <= kcVPEndNotes; cmd=(CommandID)(cmd + 1))		//for all notes
	{
		for(auto &kc : m_commands[cmd].kcList)
		{
			if(repeat)
				kc.EventType(kc.EventType() | kKeyEventRepeat);
			else
				kc.EventType(kc.EventType() & ~kKeyEventRepeat);
		}
	}
	return true;
}


bool CCommandSet::QuickChange_SetEffects(const CModSpecifications &modSpecs)
{
	// Is this already the active key configuration?
	if(&modSpecs == m_currentModSpecs)
	{
		return false;
	}
	m_currentModSpecs = &modSpecs;

	KeyCombination kc(kCtxViewPatternsFX, ModNone, 0, kKeyEventDown | kKeyEventRepeat);

	for(CommandID cmd = kcSetFXStart; cmd <= kcSetFXEnd; cmd = static_cast<CommandID>(cmd + 1))
	{
		char effect = modSpecs.GetEffectLetter(static_cast<ModCommand::COMMAND>(cmd - kcSetFXStart + 1));
		if(effect >= 'A' && effect <= 'Z')
		{
			// VkKeyScanEx needs lowercase letters
			effect = effect - 'A' + 'a';
		} else if(effect == ' ')
		{
			// We don't want to enter "empty" effects in IT / S3M
			effect = '?';
		} else if(cmd >= kcSetFXuserBegin && cmd <= kcSetFXuserEnd)
		{
			// Don't map effects that use non-alphanumeric effect letters (such as # or \), they are set up manually instead
			continue;
		}

		// Remove all old choices
		for(int p = GetKeyListSize(cmd) - 1; p >= 0; --p)
		{
			Remove(p, cmd);
		}

		if(effect != '?')
		{
			const uint32 effectKey = ui::FindVirtualKeyForCharacter(effect);
			if(effectKey != 0)
			{
				kc.KeyCode(effectKey);
				// Don't add modifier keys, since on French keyboards, numbers are input using Shift.
				// We don't really want that behaviour here, and I'm sure we don't want that in other cases on other layouts as well.
				kc.Modifier(ModNone);
				Add(kc, cmd, true);
			}

			if (effect >= '0' && effect <= '9')		// For numbers, ensure numpad works too
			{
				kc.KeyCode(ui::Key_NUMPAD0 + (effect - '0'));
				Add(kc, cmd, true);
			}
		}
	}

	return true;
}

// Stupid MFC crap: for some reason VK code isn't enough to get correct string with GetKeyName.
// We also need to figure out the correct "extended" bit.
bool KeyCombination::IsExtended(uint32 code)
{
	if (code==ui::Key_SNAPSHOT)	//print screen
		return true;
	if (code>=ui::Key_PRIOR && code<=ui::Key_DOWN) //pgup, pg down, home, end,  cursor keys,
		return true;
	if (code>=ui::Key_INSERT && code<=ui::Key_DELETE) // ins, del
		return true;
	if (code>=ui::Key_LWIN && code<=ui::Key_APPS) //winkeys & application key
		return true;
	if (code==ui::Key_DIVIDE)	//Numpad '/'
		return true;
	if (code==ui::Key_NUMLOCK)	//print screen
		return true;
	if (code>=0xA0 && code<=0xA5) //attempt for RL mods
		return true;

	return false;
}


void CCommandSet::SetupContextHierarchy()
{
	// For now much be fully expanded (i.e. don't rely on grandparent relationships).
	m_isParentContext[kCtxAllContexts].set(kCtxViewGeneral);
	m_isParentContext[kCtxAllContexts].set(kCtxViewPatterns);
	m_isParentContext[kCtxAllContexts].set(kCtxViewPatternsNote);
	m_isParentContext[kCtxAllContexts].set(kCtxViewPatternsIns);
	m_isParentContext[kCtxAllContexts].set(kCtxViewPatternsVol);
	m_isParentContext[kCtxAllContexts].set(kCtxViewPatternsFX);
	m_isParentContext[kCtxAllContexts].set(kCtxViewPatternsFXparam);
	m_isParentContext[kCtxAllContexts].set(kCtxViewSamples);
	m_isParentContext[kCtxAllContexts].set(kCtxViewInstruments);
	m_isParentContext[kCtxAllContexts].set(kCtxViewComments);
	m_isParentContext[kCtxAllContexts].set(kCtxViewTree);
	m_isParentContext[kCtxAllContexts].set(kCtxInsNoteMap);
	m_isParentContext[kCtxAllContexts].set(kCtxVSTGUI);
	m_isParentContext[kCtxAllContexts].set(kCtxCtrlGeneral);
	m_isParentContext[kCtxAllContexts].set(kCtxCtrlPatterns);
	m_isParentContext[kCtxAllContexts].set(kCtxCtrlSamples);
	m_isParentContext[kCtxAllContexts].set(kCtxCtrlInstruments);
	m_isParentContext[kCtxAllContexts].set(kCtxCtrlComments);
	m_isParentContext[kCtxAllContexts].set(kCtxCtrlSamples);
	m_isParentContext[kCtxAllContexts].set(kCtxCtrlOrderlist);
	m_isParentContext[kCtxAllContexts].set(kCtxChannelSettings);

	m_isParentContext[kCtxViewPatterns].set(kCtxViewPatternsNote);
	m_isParentContext[kCtxViewPatterns].set(kCtxViewPatternsIns);
	m_isParentContext[kCtxViewPatterns].set(kCtxViewPatternsVol);
	m_isParentContext[kCtxViewPatterns].set(kCtxViewPatternsFX);
	m_isParentContext[kCtxViewPatterns].set(kCtxViewPatternsFXparam);
	m_isParentContext[kCtxCtrlPatterns].set(kCtxCtrlOrderlist);

}


bool CCommandSet::KeyCombinationConflict(KeyCombination kc1, KeyCombination kc2, bool checkEventConflict) const
{
	bool modConflict      = (kc1.Modifier()==kc2.Modifier());
	bool codeConflict     = (kc1.KeyCode()==kc2.KeyCode());
	bool eventConflict    = ((kc1.EventType()&kc2.EventType()));
	bool ctxConflict      = (kc1.Context() == kc2.Context());
	bool crossCxtConflict = IsCrossContextConflict(kc1, kc2);

	bool conflict = modConflict && codeConflict && (eventConflict || !checkEventConflict) &&
		(ctxConflict || crossCxtConflict);

	return conflict;
}


bool CCommandSet::IsCrossContextConflict(KeyCombination kc1, KeyCombination kc2) const
{
	return m_isParentContext[kc1.Context()][kc2.Context()] || m_isParentContext[kc2.Context()][kc1.Context()];
}


InputTargetContext CCommandSet::ContextFromCommand(CommandID cmd)
{
	static constexpr std::tuple<InputTargetContext, CommandID, CommandID> ContextCommandRanges[] =
	{
		{kCtxAllContexts,         kcGlobalStart,              kcGlobalEnd             },
		{kCtxCtrlOrderlist,       kcStartOrderlistCommands,   kcEndOrderlistCommands  },
		{kCtxChannelSettings,     kcStartChnSettingsCommands, kcEndChnSettingsCommands},
		{kCtxViewPatterns,        kcStartPatternGeneral,      kcEndPatternGeneral     },
		{kCtxViewPatternsNote,    kcStartNoteColumn,          kcEndNoteColumn         },
		{kCtxViewPatternsIns,     kcSetIns0,                  kcSetIns9               },
		{kCtxViewPatternsVol,     kcSetVolumeStart,           kcSetVolumeEnd          },
		{kCtxViewPatternsFX,      kcSetFXStart,               kcSetFXEnd              },
		{kCtxViewPatternsFXparam, kcSetFXParam0,              kcSetFXParamF           },
		{kCtxViewSamples,         kcStartSampleView,          kcEndSampleView         },
		{kCtxCtrlInstruments,     kcStartInstrumentCtrl,      kcEndInstrumentCtrl,    },
		{kCtxInsNoteMap,          kcStartInsNoteMap,          kcEndInsNoteMap         },
		{kCtxViewInstruments,     kcStartInsEnvelopeEdit,     kcEndInsEnvelopeEdit    },
		{kCtxViewComments,        kcStartCommentsCommands,    kcEndCommentsCommands   },
		{kCtxVSTGUI,              kcStartVSTGUICommands,      kcEndVSTGUICommands     },
		{kCtxViewTree,            kcStartTreeViewCommands,    kcEndTreeViewCommands   },
	};
	for(const auto &[context, first, last] : ContextCommandRanges)
	{
		if(mpt::is_in_range(cmd, first, last))
			return context;
	}
	MPT_ASSERT_NOTREACHED();
	return kCtxUnknownContext;
}


bool CCommandSet::MustBeModifierKey(CommandID id)
{
	return mpt::contains(ModifierCommands, id);
}


OPENMPT_NAMESPACE_END
