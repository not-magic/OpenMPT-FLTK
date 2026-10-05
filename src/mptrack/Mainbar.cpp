/*
 * Mainbar.cpp
 * -----------
 * Purpose: Implementation of OpenMPT's window toolbar and parent container of the tree view.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/Mainbar.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "Mainbar.h"
#include "ImageLists.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "View_tre.h"
#include "WindowMessages.h"
#include "../common/mptStringBuffer.h"
#include "../soundlib/mod_specifications.h"

#include <FL/fl_draw.H>


OPENMPT_NAMESPACE_BEGIN


/////////////////////////////////////////////////////////////////////
// CMainToolBar

enum ToolbarItemIndex
{
	FILE_NEW_INDEX = 0,
	FILE_OPEN_INDEX,
	FILE_SAVE_INDEX,
	FILE_DIVIDER_INDEX,

	EDIT_CUT_INDEX,
	EDIT_COPY_INDEX,
	EDIT_PASTE_INDEX,
	EDIT_DIVIDER_INDEX,

	PLAY_MIDIRECORD_INDEX,
	PLAY_STOP_INDEX,
	PLAY_STARTPAUSE_INDEX,
	PLAY_RESTART_INDEX,
	PLAY_DIVIDER_INDEX,

	OCTAVETEXT_INDEX,
	EDITOCTAVE_INDEX,
	OCTAVE_DIVIDER_INDEX,

	TEMPOTEXT_INDEX,
	EDITTEMPO_INDEX,
	TEMPO_DIVIDER_INDEX,

	SPEEDTEXT_INDEX,
	EDITSPEED_INDEX,
	SPEED_DIVIDER_INDEX,

	RPBTEXT_INDEX,
	EDITRPB_INDEX,
	RPB_DIVIDER_INDEX,

	GLOBALVOLTEXT_INDEX,
	EDITGLOBALVOL_INDEX,
	GLOBALVOL_DIVIDER_INDEX,

	MISC_OPTIONS_INDEX,
	MISC_PANIC_INDEX,
	MISC_UPDATE_INDEX,
	MISC_DIVIDER_INDEX,
	
	VUMETER_INDEX,

	NUM_TOOLBAR_INDEX
};

enum
{
	TOOLBAR_IMAGE_PAUSE = 8,
	TOOLBAR_IMAGE_PLAY = 13,
};

#define SCALEPIXELS(x)      (ui::ScalePixels(x, this))

namespace
{
constexpr int kToolBarFontSize = 12;
}
#define VUMETER_WIDTH       SCALEPIXELS(255)
#define VUMETER_HEIGHT      SCALEPIXELS(19)


static uint32 MainButtons[] =
{
	// same order as in the bitmap 'main_toolbar.png'
	ID_FILE_NEW,
	ID_FILE_OPEN,
	ID_FILE_SAVE,
		ID_SEPARATOR,
	ID_EDIT_CUT,
	ID_EDIT_COPY,
	ID_EDIT_PASTE,
		ID_SEPARATOR,
	ID_MIDI_RECORD,
	ID_PLAYER_STOP,
	ID_PLAYER_PAUSE,
	ID_PLAYER_PLAYFROMSTART,
		ID_SEPARATOR,
	ID_SEPARATOR,  // Octave
	ID_SEPARATOR,
		ID_SEPARATOR,  // Divider for vertical mode
	ID_SEPARATOR,  // Tempo
	ID_SEPARATOR,
		ID_SEPARATOR,  // Divider for vertical mode
	ID_SEPARATOR,  // Speed
	ID_SEPARATOR,
		ID_SEPARATOR,  // Divider for vertical mode
	ID_SEPARATOR,  // Rows Per Beat
	ID_SEPARATOR,
		ID_SEPARATOR,  // Divider for vertical mode
	ID_SEPARATOR,  // Global Volume
	ID_SEPARATOR,
		ID_SEPARATOR,
	ID_VIEW_OPTIONS,
	ID_PANIC,
	ID_UPDATE_AVAILABLE,
		ID_SEPARATOR,
	ID_SEPARATOR,  // VU Meter
};

static_assert(std::size(MainButtons) == NUM_TOOLBAR_INDEX);


enum { MAX_MIDI_DEVICES = 256 };

UI_MESSAGE_MAP_BEGIN(CMainToolBar, ToolBar)
	UI_NOTIFY(ui::ToolbarDropDown, 0, &CMainToolBar::OnTbnDropDownToolBar)
	UI_COMMAND_RANGE(ID_SELECT_MIDI_DEVICE, ID_SELECT_MIDI_DEVICE + MAX_MIDI_DEVICES, &CMainToolBar::OnSelectMIDIDevice)

	UI_NOTIFY(ui::EditChange, IDC_EDIT_BASEOCTAVE,   &CMainToolBar::OnOctaveChanged)
	UI_NOTIFY(ui::SpinDeltaPos, IDC_EDIT_CURRENTTEMPO, &CMainToolBar::OnTempoSpinDelta)
	UI_NOTIFY(ui::EditChange, IDC_EDIT_CURRENTSPEED, &CMainToolBar::OnSpeedChanged)
	UI_NOTIFY(ui::EditChange, IDC_EDIT_CURRENTTEMPO, &CMainToolBar::OnTempoChanged)
	UI_NOTIFY(ui::EditChange, IDC_EDIT_RPB,          &CMainToolBar::OnRPBChanged)
	UI_NOTIFY(ui::EditChange, IDC_EDIT_GLOBALVOL,    &CMainToolBar::OnGlobalVolChanged)
UI_MESSAGE_MAP_END()


bool CMainToolBar::Create(Wnd *parent)
{
	if(Fl_Group *group = parent->GetWidget()->as_group())
		group->add(this);
	for(const uint32 id : MainButtons)
	{
		if(id == ID_SEPARATOR)
			AddButton(ID_SEPARATOR, 0, ui::ToolStyleSeparator);
		else
			AddButton(id, 0, ui::ToolStyleButton);
	}
	// Images are in the order of the buttons without separators
	int image = 0;
	for(int i = 0; i < GetButtonCount(); ++i)
	{
		if(!(GetButtonStyle(i) & ui::ToolStyleSeparator))
			SetButtonImage(MainButtons[i], image++);
	}

	// Octave
	m_StaticOctave.SetWindowText(UL_("Octave:"));
	m_StaticOctave.SetCentered(true);
	m_SpinOctave.SetDlgCtrlID(IDC_EDIT_BASEOCTAVE);
	m_SpinOctave.SetRange(MIN_BASEOCTAVE, MAX_BASEOCTAVE);
	// Tempo
	m_StaticTempo.SetWindowText(UL_("Tempo:"));
	m_StaticTempo.SetCentered(true);
	m_StaticTempo.SetDlgCtrlID(IDC_TEXT_CURRENTTEMPO);
	m_SpinTempo.SetDlgCtrlID(IDC_EDIT_CURRENTTEMPO);
	m_SpinTempo.SetLimitText(9);
	// Speed
	m_StaticSpeed.SetWindowText(UL_("Ticks/Row:"));
	m_StaticSpeed.SetCentered(true);
	m_StaticSpeed.SetDlgCtrlID(IDC_TEXT_CURRENTSPEED);
	m_SpinSpeed.SetDlgCtrlID(IDC_EDIT_CURRENTSPEED);
	// Rows per Beat
	m_StaticRowsPerBeat.SetWindowText(UL_("Rows/Beat:"));
	m_StaticRowsPerBeat.SetCentered(true);
	m_StaticRowsPerBeat.SetDlgCtrlID(IDC_TEXT_RPB);
	m_SpinRowsPerBeat.SetDlgCtrlID(IDC_EDIT_RPB);
	// Global Volume
	m_StaticGlobalVolume.SetWindowText(UL_("Global Volume:"));
	m_StaticGlobalVolume.SetCentered(true);
	m_StaticGlobalVolume.SetDlgCtrlID(IDC_TEXT_GLOBALVOL);
	m_SpinGlobalVolume.SetDlgCtrlID(IDC_EDIT_GLOBALVOL);
	static_assert(MAX_GLOBAL_VOLUME <= 999);
	m_SpinGlobalVolume.SetLimitText(3);
	// VU Meter
	m_VuMeter.SetDlgCtrlID(IDC_VUMETER);

	Wnd *controls[] = {&m_StaticOctave, &m_SpinOctave, &m_StaticTempo, &m_SpinTempo, &m_StaticSpeed, &m_SpinSpeed,
		&m_StaticRowsPerBeat, &m_SpinRowsPerBeat, &m_StaticGlobalVolume, &m_SpinGlobalVolume, &m_VuMeter};
	for(Wnd *control : controls)
	{
		Fl_Widget *widget = control->GetWidget();
		widget->labelsize(kToolBarFontSize);
		if(Spinner *spinner = dynamic_cast<Spinner *>(control))
			spinner->textsize(kToolBarFontSize);
		add(widget);
	}

	UpdateSizes();

	// Display everything
	SetWindowText(UL_("Main"));
	SetBaseOctave(4);
	SetCurrentSong(nullptr);

	SetButtonState(ID_UPDATE_AVAILABLE, ui::ToolStateHidden);

	return true;
}


void CMainToolBar::RefreshToolbar()
{
	UpdateControls();
	CMainFrame::GetMainFrame()->RecalcLayout();
}


void CMainToolBar::UpdateSizes()
{
	const int imgSize = ui::ScalePixels(16, this), btnSizeX = ui::ScalePixels(23, this), btnSizeY = ui::ScalePixels(22, this);
	m_ImageList.Create(IDB_MAINBAR, 16, 16, 1.0, false);
	m_ImageListDisabled.Create(IDB_MAINBAR, 16, 16, 1.0, true);

	struct TextWndInfo
	{
		Wnd &wnd;
		const mpt::uchar *measureText;
		int toolbarIndex, id;
	};
	const TextWndInfo TextWnds[] =
	{
		{m_StaticOctave,       UL_("Octave:"),        OCTAVETEXT_INDEX,    ID_SEPARATOR         },
		{m_SpinOctave,         UL_("9"),              EDITOCTAVE_INDEX,    IDC_EDIT_BASEOCTAVE  },
		{m_StaticTempo,        UL_("Tempo"),          TEMPOTEXT_INDEX,     IDC_TEXT_CURRENTTEMPO},
		{m_SpinTempo,          UL_("999.9999"),       EDITTEMPO_INDEX,     IDC_EDIT_CURRENTTEMPO},
		{m_StaticSpeed,        UL_("Ticks/Row:"),     SPEEDTEXT_INDEX,     IDC_TEXT_CURRENTSPEED},
		{m_SpinSpeed,          UL_("999"),            EDITSPEED_INDEX,     IDC_EDIT_CURRENTSPEED},
		{m_StaticRowsPerBeat,  UL_("Rows/Beat:"),     RPBTEXT_INDEX,       IDC_TEXT_RPB         },
		{m_SpinRowsPerBeat,    UL_("9999"),           EDITRPB_INDEX,       IDC_EDIT_RPB         },
		{m_StaticGlobalVolume, UL_("Global Volume:"), GLOBALVOLTEXT_INDEX, IDC_TEXT_GLOBALVOL   },
		{m_SpinGlobalVolume,   UL_("999"),            EDITGLOBALVOL_INDEX, IDC_EDIT_GLOBALVOL   },
	};

	const int textPaddingX = ui::ScalePixels(16, this), textPaddingY = ui::ScalePixels(4, this), textMinHeight = ui::ScalePixels(20, this);
	ui::Painter painter;
	painter.SetFont(ui::Font(FL_HELVETICA, kToolBarFontSize));
	for(auto &info : TextWnds)
	{
		const auto size = painter.GetTextExtent(mpt::ustring(info.measureText));
		const int height = std::max(static_cast<int>(size.cy) + textPaddingY, textMinHeight);
		// Fl_Spinner's buttons are half its height plus two pixels wide
		const int buttonWidth = dynamic_cast<Spinner *>(&info.wnd) ? height / 2 + 2 : 0;
		const int width = size.cx + textPaddingX + buttonWidth;
		info.wnd.SetWindowPos(0, 0, width, height, false, true);
	}

	SetImageList(&m_ImageList);
	SetDisabledImageList(&m_ImageListDisabled);
	SetBitmapSize(Size(imgSize, imgSize));
	SetButtonSize(Size(btnSizeX, btnSizeY));

	// Dropdown menus for New and MIDI buttons
	SetExtendedStyle(GetExtendedStyle() | ui::ToolExtendedDrawDropDownArrows);
	SetButtonStyle(CommandToIndex(ID_FILE_NEW), GetButtonStyle(CommandToIndex(ID_FILE_NEW)) | ui::ToolStyleDropDown);
	SetButtonStyle(CommandToIndex(ID_MIDI_RECORD), GetButtonStyle(CommandToIndex(ID_MIDI_RECORD)) | ui::ToolStyleDropDown);

}


void CMainToolBar::Init(CMainFrame *pMainFrm)
{
	SetFlat(TrackerSettings::Instance().patternSetup & PatternSetup::FlatToolbarButtons);
	UpdateControls();
	pMainFrm->DockBar(this, ui::MainFrameBase::DockSide::Top, CalcLayoutSize().cy + 2);
}


Size CMainToolBar::CalcLayoutSize() const
{
	return CalcFixedSize();
}


void CMainToolBar::UpdateControls()
{
	const FlagSet<MainToolBarItem> visibleItems = TrackerSettings::Instance().mainToolBarVisibleItems.Get();

	SetButtonVisibility(FILE_NEW_INDEX, visibleItems[MainToolBarItem::IconsFile]);
	SetButtonVisibility(FILE_OPEN_INDEX, visibleItems[MainToolBarItem::IconsFile]);
	SetButtonVisibility(FILE_SAVE_INDEX, visibleItems[MainToolBarItem::IconsFile]);
	SetButtonVisibility(FILE_DIVIDER_INDEX, visibleItems[MainToolBarItem::IconsFile]);

	SetButtonVisibility(EDIT_CUT_INDEX, visibleItems[MainToolBarItem::IconsEdit]);
	SetButtonVisibility(EDIT_COPY_INDEX, visibleItems[MainToolBarItem::IconsEdit]);
	SetButtonVisibility(EDIT_PASTE_INDEX, visibleItems[MainToolBarItem::IconsEdit]);
	SetButtonVisibility(EDIT_DIVIDER_INDEX, visibleItems[MainToolBarItem::IconsEdit]);

	SetButtonVisibility(PLAY_MIDIRECORD_INDEX, visibleItems[MainToolBarItem::IconsPlayback]);
	SetButtonVisibility(PLAY_STOP_INDEX, visibleItems[MainToolBarItem::IconsPlayback]);
	SetButtonVisibility(PLAY_STARTPAUSE_INDEX, visibleItems[MainToolBarItem::IconsPlayback]);
	SetButtonVisibility(PLAY_RESTART_INDEX, visibleItems[MainToolBarItem::IconsPlayback]);
	SetButtonVisibility(PLAY_DIVIDER_INDEX, visibleItems[MainToolBarItem::IconsPlayback]);

	SetButtonVisibility(MISC_OPTIONS_INDEX, visibleItems[MainToolBarItem::IconsMisc]);
	SetButtonVisibility(MISC_PANIC_INDEX, visibleItems[MainToolBarItem::IconsMisc]);
	SetButtonVisibility(MISC_DIVIDER_INDEX, visibleItems.test_all(MainToolBarItem::IconsMisc | MainToolBarItem::VUMeter));

	UpdateControl(visibleItems[MainToolBarItem::Octave], m_StaticOctave, OCTAVETEXT_INDEX, ID_SEPARATOR);
	UpdateControl(visibleItems[MainToolBarItem::Octave], m_SpinOctave, EDITOCTAVE_INDEX, IDC_EDIT_BASEOCTAVE);
	SetButtonVisibility(OCTAVE_DIVIDER_INDEX, false);

	UpdateControl(visibleItems[MainToolBarItem::Tempo], m_StaticTempo, TEMPOTEXT_INDEX, IDC_TEXT_CURRENTTEMPO);
	UpdateControl(visibleItems[MainToolBarItem::Tempo], m_SpinTempo, EDITTEMPO_INDEX, IDC_EDIT_CURRENTTEMPO);
	SetButtonVisibility(TEMPO_DIVIDER_INDEX, false);

	UpdateControl(visibleItems[MainToolBarItem::Speed], m_StaticSpeed, SPEEDTEXT_INDEX, IDC_TEXT_CURRENTSPEED);
	UpdateControl(visibleItems[MainToolBarItem::Speed], m_SpinSpeed, EDITSPEED_INDEX, IDC_EDIT_CURRENTSPEED);
	SetButtonVisibility(SPEED_DIVIDER_INDEX, false);

	UpdateControl(visibleItems[MainToolBarItem::RowsPerBeat], m_StaticRowsPerBeat, RPBTEXT_INDEX, IDC_TEXT_RPB);
	UpdateControl(visibleItems[MainToolBarItem::RowsPerBeat], m_SpinRowsPerBeat, EDITRPB_INDEX, IDC_EDIT_RPB);
	SetButtonVisibility(RPB_DIVIDER_INDEX, false);

	UpdateControl(visibleItems[MainToolBarItem::GlobalVolume], m_StaticGlobalVolume, GLOBALVOLTEXT_INDEX, IDC_TEXT_GLOBALVOL);
	UpdateControl(visibleItems[MainToolBarItem::GlobalVolume], m_SpinGlobalVolume, EDITGLOBALVOL_INDEX, IDC_EDIT_GLOBALVOL);
	SetButtonVisibility(GLOBALVOL_DIVIDER_INDEX, visibleItems.test_any_except(MainToolBarItem::VUMeter));

	m_VuMeter.SetOrientation(true);
	m_VuMeter.SetWindowPos(0, 0, VUMETER_WIDTH, VUMETER_HEIGHT, false, true);
	UpdateControl(visibleItems[MainToolBarItem::VUMeter], m_VuMeter, VUMETER_INDEX, IDC_VUMETER, VUMETER_HEIGHT);
	redraw();
}


bool CMainToolBar::ToggleVisibility(MainToolBarItem item)
{
	FlagSet<MainToolBarItem> visibleItems = TrackerSettings::Instance().mainToolBarVisibleItems.Get();
	visibleItems.flip(item);
	// At least one icon group must be visible, otherwise the toolbar collapses
	if(!visibleItems[MainToolBarItem::AllIcons])
		visibleItems.flip(item);
	TrackerSettings::Instance().mainToolBarVisibleItems = visibleItems.value().as_enum();
	RefreshToolbar();
	return visibleItems[item];
}


uint32 CMainToolBar::GetBaseOctave() const
{
	if(m_currentOctave >= MIN_BASEOCTAVE) return (uint32)m_currentOctave;
	return 4;
}


void CMainToolBar::SetBaseOctave(uint32 octave)
{
	if(octave == static_cast<uint32>(m_currentOctave) || octave < MIN_BASEOCTAVE || octave > MAX_BASEOCTAVE)
		return;

	m_currentOctave = octave;
	m_SpinOctave.SetPos(octave);
}


static void EnableEdit(Spinner &spinner, bool enable)
{
	if(!enable)
		spinner.SetWindowText(UL_("---"));
	spinner.EnableWindow(enable);
	spinner.SetReadOnly(!enable);
	spinner.Invalidate(false);
}


void CMainToolBar::SetCurrentSong(CTrackerSoundFile *pSndFile)
{
	// Update Info
	m_updating = true;
	const Wnd *focus = GetFocus();
	const FlagSet<MainToolBarItem> visibleItems = TrackerSettings::Instance().mainToolBarVisibleItems.Get();
	if(pSndFile)
	{
		const auto &specs = pSndFile->GetModSpecifications();
		m_SpinSpeed.SetRange32(specs.speedMin, specs.speedMax);
		m_SpinTempo.SetAllowFractions(specs.hasFractionalTempo);
		m_SpinTempo.range(specs.GetTempoMin().ToDouble(), specs.GetTempoMax().ToDouble());
		m_SpinRowsPerBeat.SetRange32(1, std::max(pSndFile->m_PlayState.m_nCurrentRowsPerMeasure, ROWINDEX(1)));
		m_SpinGlobalVolume.SetRange32(0, pSndFile->GlobalVolumeRange());

		// Update play/pause button
		if(m_currentTempo == TEMPO(0, 0))
			SetButtonInfo(PLAY_STARTPAUSE_INDEX, ID_PLAYER_PAUSE, ui::ToolStyleButton, TOOLBAR_IMAGE_PAUSE);
		// Update Speed
		int nSpeed = pSndFile->m_PlayState.m_nMusicSpeed;
		if(nSpeed != m_currentSpeed && focus != &m_SpinSpeed && visibleItems[MainToolBarItem::Speed])
		{
			if(m_currentSpeed < 0)
				EnableEdit(m_SpinSpeed, true);

			m_currentSpeed = nSpeed;
			SetDlgItemInt(IDC_EDIT_CURRENTSPEED, m_currentSpeed, false);
		}
		TEMPO nTempo = pSndFile->m_PlayState.m_nMusicTempo;
		if(nTempo != m_currentTempo && focus != &m_SpinTempo && visibleItems[MainToolBarItem::Tempo])
		{
			if(m_currentTempo <= TEMPO(0, 0))
				EnableEdit(m_SpinTempo, true);

			m_currentTempo = nTempo;
			m_SpinTempo.SetValue(m_currentTempo.ToDouble());
		}
		int nRowsPerBeat = pSndFile->m_PlayState.m_nCurrentRowsPerBeat;
		if(nRowsPerBeat != m_currentRowsPerBeat && focus != &m_SpinRowsPerBeat && visibleItems[MainToolBarItem::RowsPerBeat])
		{
			if(m_currentRowsPerBeat < 0)
				EnableEdit(m_SpinRowsPerBeat, true);

			m_currentRowsPerBeat = nRowsPerBeat;
			SetDlgItemInt(IDC_EDIT_RPB, m_currentRowsPerBeat, false);
		}
		int globalVol = pSndFile->m_PlayState.m_nGlobalVolume;
		if(globalVol != m_currentGlobalVolume && focus != &m_SpinGlobalVolume && visibleItems[MainToolBarItem::GlobalVolume])
		{
			if(m_currentGlobalVolume < 0)
				EnableEdit(m_SpinGlobalVolume, true);

			m_currentGlobalVolume = globalVol;
			uint32 displayVolume = Util::muldivr_unsigned(m_currentGlobalVolume, pSndFile->GlobalVolumeRange(), MAX_GLOBAL_VOLUME);
			SetDlgItemInt(IDC_EDIT_GLOBALVOL, displayVolume, false);
		}
	} else
	{
		if(m_currentTempo > TEMPO(0, 0))
		{
			EnableEdit(m_SpinTempo, false);
			SetButtonInfo(PLAY_STARTPAUSE_INDEX, ID_PLAYER_PLAY, ui::ToolStyleButton, TOOLBAR_IMAGE_PLAY);
		}
		if(m_currentSpeed != -1)
			EnableEdit(m_SpinSpeed, false);
		if(m_currentRowsPerBeat != -1)
			EnableEdit(m_SpinRowsPerBeat, false);
		if(m_currentGlobalVolume != -1)
			EnableEdit(m_SpinGlobalVolume, false);

		m_currentTempo.Set(0);
		m_currentSpeed = -1;
		m_currentRowsPerBeat = -1;
		m_currentGlobalVolume = -1;
	}
	// If focus was on a now-disabled input field, move it somewhere else
	if(focus && !focus->IsWindowEnabled() && focus->GetParent() == this)
		CMainFrame::GetMainFrame()->SetFocus();

	m_updating = false;
}


void CMainToolBar::OnOctaveChanged()
{
	if(m_updating || m_SpinOctave.GetWindowText().empty())
		return;
	SetBaseOctave(static_cast<uint32>(std::clamp(m_SpinOctave.GetPos(), MIN_BASEOCTAVE, MAX_BASEOCTAVE)));
}


void CMainToolBar::OnTempoSpinDelta(NotifyHeader *, LResult *)
{
	const CTrackerSoundFile *sndFile = CMainFrame::GetMainFrame()->GetSoundFilePlaying();
	if(!sndFile || !sndFile->GetModSpecifications().hasFractionalTempo)
		m_SpinTempo.SetIncrement(1.0);
	else
		m_SpinTempo.SetIncrement(CMainFrame::GetMainFrame()->GetInputHandler()->CtrlPressed() ? 0.01 : 0.1);
}


void CMainToolBar::OnSpeedChanged()
{
	if(CMainFrame *mainFrm = CMainFrame::GetMainFrame(); mainFrm && !m_updating)
	{
		bool ok = false;
		uint32 newSpeed = GetDlgItemInt(IDC_EDIT_CURRENTSPEED, &ok, false);
		CTrackerSoundFile *sndFile = mainFrm->GetSoundFilePlaying();
		if(sndFile && ok)
		{
			const auto &specs = sndFile->GetModSpecifications();
			sndFile->m_PlayState.m_nMusicSpeed = Clamp(newSpeed, specs.speedMin, specs.speedMax);
		}
		m_currentSpeed = 0;  // Force display update once focus moves away from this input field
	}
}


void CMainToolBar::OnTempoChanged()
{
	if(CMainFrame *mainFrm = CMainFrame::GetMainFrame(); mainFrm && !m_updating)
	{
		const TEMPO newTempo(m_SpinTempo.GetValue());
		CTrackerSoundFile *sndFile = mainFrm->GetSoundFilePlaying();
		if(sndFile && !m_SpinTempo.GetWindowText().empty())
		{
			const auto &specs = sndFile->GetModSpecifications();
			sndFile->m_PlayState.m_nMusicTempo = Clamp(newTempo, specs.GetTempoMin(), specs.GetTempoMax());
		}
		m_currentTempo.Set(0, 1);  // Force display update once focus moves away from this input field
	}
}


void CMainToolBar::OnRPBChanged()
{
	if(CMainFrame *mainFrm = CMainFrame::GetMainFrame(); mainFrm && !m_updating)
	{
		bool ok = false;
		uint32 newRPB = GetDlgItemInt(IDC_EDIT_RPB, &ok, false);
		CTrackerSoundFile *sndFile = mainFrm->GetSoundFilePlaying();
		if(sndFile && ok && newRPB > 0)
		{
			SetRowsPerBeat(newRPB);
		}
		m_currentRowsPerBeat = -2;  // Force display update once focus moves away from this input field
	}
}


void CMainToolBar::OnGlobalVolChanged()
{
	if(CMainFrame *mainFrm = CMainFrame::GetMainFrame(); mainFrm && !m_updating)
	{
		bool ok = false;
		uint32 newGlobalVol = GetDlgItemInt(IDC_EDIT_GLOBALVOL, &ok, false);
		CTrackerSoundFile *sndFile = mainFrm->GetSoundFilePlaying();
		if(sndFile && ok)
		{
			sndFile->m_PlayState.m_nGlobalVolume = Clamp(Util::muldivr_unsigned(newGlobalVol, MAX_GLOBAL_VOLUME, sndFile->GlobalVolumeRange()), uint32(0), MAX_GLOBAL_VOLUME);
		}
		m_currentGlobalVolume = -2;  // Force display update once focus moves away from this input field
	}
}


void CMainToolBar::OnTbnDropDownToolBar(NotifyHeader *pNMHDR, LResult *pResult)
{
	const ui::ToolbarDropDownInfo *info = static_cast<const ui::ToolbarDropDownInfo *>(pNMHDR->extra);
	Rect buttonRect = info->rect;
	ClientToScreen(buttonRect);

	switch(info->id)
	{
	case ID_FILE_NEW:
		{
			auto *mainFrm = CMainFrame::GetMainFrame();
			auto [newMenu, newPos] = CMainFrame::FindMenuItemByCommand(*mainFrm->GetMenu(), ID_FILE_NEWIT);
			auto [templateMenu, templatePos] = CMainFrame::FindMenuItemByCommand(*mainFrm->GetMenu(), ID_FILE_OPENTEMPLATE);
			if(newMenu)
			{
				Menu popup = *newMenu;
				if(templateMenu)
					popup.AppendMenu(ui::MenuItemPopup, *templateMenu, UL_("&Templates"));
				const uint32 command = popup.TrackPopupMenu(Point(buttonRect.left, buttonRect.bottom), this);
				if(command)
					SendCommand(command);
			}
		}
		break;
	case ID_MIDI_RECORD:
		// Show a list of MIDI devices
		{
			Menu popup;
			popup.CreatePopupMenu();
			const std::vector<mpt::ustring> devices = CMainFrame::GetMidiInputDeviceNames();
			const uint32 numDevs = std::min(static_cast<uint32>(devices.size()), static_cast<uint32>(MAX_MIDI_DEVICES));
			const uint32 current = TrackerSettings::Instance().GetCurrentMIDIDevice();
			for(uint32 i = 0; i < numDevs; i++)
			{
				popup.AppendMenu(ui::MenuItemString | (i == current ? ui::MenuItemChecked : 0), ID_SELECT_MIDI_DEVICE + i, theApp.GetFriendlyMIDIPortName(devices[i], true));
			}
			if(!numDevs)
			{
				popup.AppendMenu(ui::MenuItemString | ui::MenuItemGrayed, 0, UL_("No MIDI input devices found"));
			}
			const uint32 command = popup.TrackPopupMenu(Point(buttonRect.left, buttonRect.bottom), this);
			if(command)
				SendCommand(command);
		}
		break;
	}

	*pResult = 0;
}


void CMainToolBar::OnSelectMIDIDevice(uint32 id)
{
	CMainFrame::GetMainFrame()->midiCloseDevice();
	TrackerSettings::Instance().SetMIDIDevice(id - ID_SELECT_MIDI_DEVICE);
	CMainFrame::GetMainFrame()->midiOpenDevice();
}


void CMainToolBar::SetRowsPerBeat(ROWINDEX newRPB)
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if(pMainFrm == nullptr)
		return;
	CModDoc *pModDoc = pMainFrm->GetModPlaying();
	CTrackerSoundFile *pSndFile = pMainFrm->GetSoundFilePlaying();
	if(pModDoc == nullptr || pSndFile == nullptr)
		return;

	TrackerCriticalSection cs;
	PATTERNINDEX pat = pSndFile->GetCurrentPattern();
	bool modified = false;
	if(pSndFile->Patterns.IsValidPat(pat) && pSndFile->Patterns[pat].GetOverrideSignature())
	{
		CPattern &pattern = pSndFile->Patterns[pat];
		if(newRPB <= pattern.GetRowsPerMeasure())
		{
			pattern.SetSignature(newRPB, pattern.GetRowsPerMeasure());
			TempoSwing swing = pattern.GetTempoSwing();
			if(!swing.empty())
			{
				swing.resize(newRPB);
				pattern.SetTempoSwing(swing);
			}
			modified = true;
		}
	} else
	{
		if(newRPB <= pSndFile->m_nDefaultRowsPerMeasure)
		{
			pSndFile->m_nDefaultRowsPerBeat = newRPB;
			if(!pSndFile->m_tempoSwing.empty())
				pSndFile->m_tempoSwing.resize(newRPB);
			modified = true;
		}
	}

	// Update pattern editor
	if(modified)
	{
		pSndFile->m_PlayState.m_nCurrentRowsPerBeat = newRPB;
		cs.Leave();
		pModDoc->SetModified();
		pModDoc->UpdateAllViews(PatternHint().Data());
	}
}


mpt::ustring CMainToolBar::GetButtonToolTip(uint32 id) const
{
	const mpt::uchar *s = nullptr;
	CommandID cmd = kcNull;
	switch(id)
	{
	case ID_FILE_NEW: s = UL_("New"); cmd = kcFileNew; break;
	case ID_FILE_OPEN: s = UL_("Open"); cmd = kcFileOpen; break;
	case ID_FILE_SAVE: s = UL_("Save"); cmd = kcFileSave; break;
	case ID_EDIT_CUT: s = UL_("Cut"); cmd = kcEditCut; break;
	case ID_EDIT_COPY: s = UL_("Copy"); cmd = kcEditCopy; break;
	case ID_EDIT_PASTE: s = UL_("Paste"); cmd = kcEditPaste; break;
	case ID_MIDI_RECORD: s = UL_("MIDI Record"); cmd = kcMidiRecord; break;
	case ID_PLAYER_STOP: s = UL_("Stop"); cmd = kcStopSong; break;
	case ID_PLAYER_PLAY: s = UL_("Play"); cmd = kcPlayPauseSong; break;
	case ID_PLAYER_PAUSE: s = UL_("Pause"); cmd = kcPlayPauseSong; break;
	case ID_PLAYER_PLAYFROMSTART: s = UL_("Play From Start"); cmd = kcPlaySongFromStart; break;
	case ID_VIEW_OPTIONS: s = UL_("Setup"); cmd = kcViewOptions; break;
	case ID_PANIC: s = UL_("Stop all hanging plugin and sample voices"); cmd = kcPanic; break;
	}

	if(s == nullptr)
		return {};

	mpt::ustring text = s;
	if(cmd != kcNull)
	{
		auto keyText = CMainFrame::GetInputHandler()->m_activeCommandSet->GetKeyTextFromCommand(cmd, 0);
		if(!keyText.empty())
			text += MPT_UFORMAT(" ({})")(keyText);
	}
	return text;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// CModTreeBar

UI_MESSAGE_MAP_BEGIN(CModTreeBar, Wnd)
	UI_NOTIFY(ui::EditChange, IDC_EDIT1,             &CModTreeBar::OnFilterChanged)
	UI_NOTIFY(ui::EditKillFocus, IDC_EDIT1,          &CModTreeBar::OnFilterLostFocus)
	UI_COMMAND(ID_CLOSE_LIBRARY_FILTER, &CModTreeBar::CloseTreeFilter)
UI_MESSAGE_MAP_END()


CModTreeBar::CModTreeBar()
{
	m_nTreeSplitRatio = TrackerSettings::Instance().glTreeSplitRatio;
	m_isFocusable = false;
	m_pModTreeData = new CModTree(nullptr);
	m_pModTreeData->SetDlgCtrlID(IDC_TREEDATA);
	m_pModTree = new CModTree(m_pModTreeData);
	m_pModTree->SetDlgCtrlID(IDC_TREEVIEW);
	add(m_pModTree->GetWidget());
	add(m_pModTreeData->GetWidget());
	m_status.reset();
}


CModTreeBar::~CModTreeBar()
{
	// The trees are children of this window and are deleted with it
	m_pModTree = nullptr;
	m_pModTreeData = nullptr;
}


void CModTreeBar::Init()
{
	m_nTreeSplitRatio = TrackerSettings::Instance().glTreeSplitRatio;
	if(m_pModTree)
	{
		m_pModTreeData->Init();
		m_pModTree->Init();
	}
}


bool CModTreeBar::PreTranslateMessage(int event)
{
	if(m_filterEdit && event == FL_KEYBOARD && Wnd::GetFocus() == m_filterEdit.get() && m_filterSource != nullptr)
	{
		switch(ui::KeyFromEvent())
		{
		case ui::Key_RETURN:
			if(const auto modItem = m_filterSource->GetModItem(m_filterSource->GetSelectedItem()); (modItem.type == CModTree::MODITEM_INSLIB_FOLDER || modItem.type == CModTree::MODITEM_INSLIB_SONG))
			{
				m_filterSource->PostCommand(ID_MODTREE_EXECUTE);
			}
			[[fallthrough]];
		case ui::Key_ESCAPE:
			CloseTreeFilter();
			return true;

		case ui::Key_TAB:
			if(m_filterSource)
			{
				m_filterSource->SetFocus();
				return true;
			}
			break;

		case ui::Key_UP:
		case ui::Key_DOWN:
			if(const auto selectedItem = m_filterSource->GetSelectedItem(); selectedItem != nullptr)
			{
				const auto item = m_filterSource->GetNextItem(selectedItem, (ui::KeyFromEvent() == ui::Key_UP) ? ui::TreePreviousSibling : ui::TreeNextSibling);
				if(item)
					m_filterSource->SelectItem(item);
				return true;
			}
			break;
		}
	}
	return Wnd::PreTranslateMessage(event);
}


void CModTreeBar::RefreshDlsBanks()
{
	if(m_pModTree) m_pModTree->RefreshDlsBanks();
}


void CModTreeBar::RefreshMidiLibrary()
{
	if(m_pModTree) m_pModTree->RefreshMidiLibrary();
}


void CModTreeBar::OnOptionsChanged()
{
	if(m_pModTree) m_pModTree->OnOptionsChanged();
}


int CModTreeBar::Padding() const
{
	return ui::ScalePixels(3, this);
}


int CModTreeBar::GetDesiredWidth() const
{
	return std::max(ui::ScalePixels(TrackerSettings::Instance().glTreeWindowWidth, this), 1) + Padding();
}


void CModTreeBar::RecalcLayout()
{
	if((m_pModTree) && (m_pModTreeData))
	{
		const Rect rect = GetClientRect();
		const int padding = Padding();
		// The splitter to the document area is at the inner edge of the bar
		const int contentLeft = m_isOnLeft ? 0 : padding;
		const int contentWidth = std::max(rect.Width() - padding, 0);
		int cyavail = rect.Height() - padding;
		if(cyavail < 0) cyavail = 0;
		const int cytree = (cyavail * m_nTreeSplitRatio) >> 8;
		const int cydata = cyavail - cytree;
		const int x0 = GetWidget()->x() + contentLeft;
		const int y0 = GetWidget()->y();
		const int editHeight = ui::ScalePixels(20, this);
		if(m_filterSource == m_pModTree && m_filterEdit)
		{
			m_pModTree->MoveWindow(x0, y0, contentWidth, std::max(cytree - editHeight, 0));
			m_pModTreeData->MoveWindow(x0, y0 + cytree + padding, contentWidth, cydata);
			m_filterEdit->MoveWindow(x0, y0 + cytree - editHeight, contentWidth, editHeight);
		} else if(m_filterSource == m_pModTreeData && m_filterEdit)
		{
			m_pModTree->MoveWindow(x0, y0, contentWidth, cytree);
			m_pModTreeData->MoveWindow(x0, y0 + cytree + padding, contentWidth, std::max(cydata - editHeight, 0));
			m_filterEdit->MoveWindow(x0, y0 + cytree + padding + cydata - editHeight, contentWidth, editHeight);
		} else
		{
			m_pModTree->MoveWindow(x0, y0, contentWidth, cytree);
			m_pModTreeData->MoveWindow(x0, y0 + cytree + padding, contentWidth, cydata);
		}
		// Redrawing only the moved trees would clip the splitter gap out of the damage region
		redraw();
	}
}


void CModTreeBar::draw()
{
	ui::Painter painter(Point(x(), y()));
	painter.FillSolidRect(GetClientRect(), ui::GetSystemColor(ui::SysColor::ButtonFace));
	draw_children();
}


namespace
{
int FindTreeSplitterTop(const Fl_Widget &bar, const Fl_Widget &treeData, int padding)
{
	return treeData.y() - bar.y() - padding;
}
}


// Fl_Group consumes FL_MOVE and forwards clicks to the trees, so the splitters are tracked before the children see the events
int CModTreeBar::handle(int event)
{
	const Point point(Fl::event_x() - x(), Fl::event_y() - y());
	switch(event)
	{
	case FL_ENTER:
	case FL_MOVE:
		DoMouseMove(point);
		break;
	case FL_PUSH:
		if(Fl::event_button() == FL_LEFT_MOUSE && m_status[MTB_CAPTURE])
		{
			DoLButtonDown(point);
			return 1;
		}
		break;
	case FL_DRAG:
		if(m_status[MTB_DRAGGING])
		{
			DoMouseMove(point);
			return 1;
		}
		break;
	case FL_RELEASE:
		if(m_status[MTB_DRAGGING])
		{
			DoLButtonUp();
			return 1;
		}
		break;
	case FL_LEAVE:
		if(!m_status[MTB_DRAGGING])
			CancelTracking();
		break;
	default:
		break;
	}
	return Panel::handle(event);
}


void CModTreeBar::DoMouseMove(Point pt)
{
	const Rect rect = GetClientRect();
	const int padding = Padding();

	if(m_status[MTB_DRAGGING])
	{
		// Drag offsets use window coordinates because the bar moves while it is resized on the right side
		if(m_status[MTB_VERTICAL])
		{
			const int cyavail = std::max(rect.Height() - padding, padding + 1);
			const int treeHeight = std::clamp(m_cyOriginal + Fl::event_y() - ptDragging.y, 0, cyavail);
			m_nTreeSplitRatio = std::clamp(treeHeight * 256 / cyavail, 0, 256);
			TrackerSettings::Instance().glTreeSplitRatio = m_nTreeSplitRatio;
			RecalcLayout();
		} else if(CMainFrame *pMainFrm = CMainFrame::GetMainFrame())
		{
			const int deltaX = Fl::event_x() - ptDragging.x;
			const int maxWidth = std::max(pMainFrm->w() - 2 * padding, 1);
			const int contentWidth = std::clamp(m_cxOriginal - padding + (BarOnLeft() ? deltaX : -deltaX), 1, maxWidth);
			TrackerSettings::Instance().glTreeWindowWidth = ui::ScalePixelsInv(contentWidth, this);
			pMainFrm->RecalcLayout();
		}
		return;
	}

	const int extraPadding = ui::ScalePixels(2, this);
	const Rect widthSplitter = BarOnLeft()
		? Rect(rect.right - padding - extraPadding, rect.top, rect.right, rect.bottom)
		: Rect(rect.left, rect.top, rect.left + padding + extraPadding, rect.bottom);
	const bool isHorizontal = widthSplitter.PtInRect(pt);
	bool isVertical = false;
	if(!isHorizontal && m_pModTreeData)
	{
		const int splitterTop = FindTreeSplitterTop(*this, *m_pModTreeData->GetWidget(), padding);
		isVertical = Rect(0, splitterTop - extraPadding, rect.Width(), splitterTop + padding + extraPadding).PtInRect(pt);
	}
	if(isHorizontal || isVertical)
	{
		m_status.set(MTB_CAPTURE);
		m_status.set(MTB_VERTICAL, isVertical);
		SetCursorShape(isVertical ? FL_CURSOR_NS : FL_CURSOR_WE);
	} else if(m_status[MTB_CAPTURE])
	{
		m_status.reset(MTB_CAPTURE);
		SetCursorShape(FL_CURSOR_DEFAULT);
	}
}


void CModTreeBar::DoLButtonDown(Point)
{
	if(m_status[MTB_CAPTURE] && !m_status[MTB_DRAGGING])
	{
		m_cxOriginal = GetClientRect().Width();
		m_cyOriginal = FindTreeSplitterTop(*this, *m_pModTreeData->GetWidget(), Padding());
		ptDragging = Point(Fl::event_x(), Fl::event_y());
		m_status.set(MTB_DRAGGING);
	}
}


void CModTreeBar::DoLButtonUp()
{
	m_status.reset(MTB_DRAGGING);
	if(m_status[MTB_CAPTURE])
	{
		m_status.reset(MTB_CAPTURE);
		SetCursorShape(FL_CURSOR_DEFAULT);
	}
}


void CModTreeBar::CancelTracking()
{
	DoLButtonUp();
}


void CModTreeBar::OnDocumentCreated(CModDoc *pModDoc)
{
	if(m_pModTree && pModDoc) m_pModTree->AddDocument(*pModDoc);
}


void CModTreeBar::OnDocumentClosed(CModDoc *pModDoc)
{
	if(m_pModTree && pModDoc) m_pModTree->RemoveDocument(*pModDoc);
}


void CModTreeBar::OnUpdate(CModDoc *pModDoc, UpdateHint hint, HintObject *pHint)
{
	if(m_pModTree) m_pModTree->OnUpdate(pModDoc, hint, pHint);
}


void CModTreeBar::UpdatePlayPos(CModDoc *pModDoc, Notification *pNotify)
{
	if(m_pModTree && pModDoc) m_pModTree->UpdatePlayPos(*pModDoc, pNotify);
}


void CModTreeBar::OnSize(uint32 nType, int cx, int cy)
{
	Wnd::OnSize(nType, cx, cy);
	RecalcLayout();
}


bool CModTreeBar::SetTreeSoundfile(FileReader &file)
{
	return m_pModTree->SetSoundFile(file);
}


void CModTreeBar::StartTreeFilter(CModTree &source)
{
	if(!m_filterEdit)
	{
		m_filterEdit = std::make_unique<Edit>(GetWidget()->x(), GetWidget()->y(), GetWidget()->w(), ui::ScalePixels(20, this));
		m_filterEdit->SetDlgCtrlID(IDC_EDIT1);
		add(m_filterEdit->GetWidget());
	} else if(m_filterSource != &source)
	{
		m_filterEdit->SetWindowText(UL_(""));
	}
	m_filterEdit->SetFocus();
	m_filterSource = &source;
	RecalcLayout();
}


void CModTreeBar::OnFilterChanged()
{
	if(!m_filterSource || !m_filterEdit)
		return;

	const mpt::ustring filter = m_filterEdit->GetWindowText();
	const std::size_t length = filter.size();
	if(length < 1 || length > 2)
	{
		CancelTimer();
		m_filterSource->SetInstrumentLibraryFilter(filter);
	} else
	{
		if(!m_filterTimer)
			m_filterTimer = SetTimer(1, static_cast<uint32>(360 - length * 120));
	}
}


void CModTreeBar::OnTimer(uintptr_t id)
{
	if(id != m_filterTimer)
		return;

	if(m_filterSource && m_filterEdit)
	{
		m_filterSource->SetInstrumentLibraryFilter(m_filterEdit->GetWindowText());
	}
	CancelTimer();
}


void CModTreeBar::OnFilterLostFocus()
{
	if(m_filterEdit && m_filterEdit->GetWindowText().empty())
		CloseTreeFilter();
}


void CModTreeBar::CloseTreeFilter()
{
	CancelTimer();
	if(m_filterSource)
	{
		m_filterSource->SetInstrumentLibraryFilter({});
		if(Wnd::GetFocus() == m_filterEdit.get())
			m_filterSource->SetFocus();
		m_filterSource = nullptr;
	}
	if(m_filterEdit)
	{
		remove(m_filterEdit->GetWidget());
		m_filterEdit.reset();
		RecalcLayout();
	}
}


void CModTreeBar::CancelTimer()
{
	if(m_filterTimer)
	{
		KillTimer(m_filterTimer);
		m_filterTimer = 0;
	}
}


////////////////////////////////////////////////////////////////////////////////
//
// Stereo VU Meter for toolbar
//

void CStereoVU::draw()
{
	fl_push_clip(x(), y(), w(), h());
	ui::Painter painter(Point(x(), y()));
	DrawVuMeters(painter);
	fl_pop_clip();
}


void CStereoVU::SetVuMeter(uint8 validChannels, const uint32 channels[4], bool force)
{
	bool changed = false;
	if(validChannels == 0)
	{
		// reset
		validChannels = numChannels;
	} else if(validChannels != numChannels)
	{
		changed = true;
		force = true;
		numChannels = validChannels;
		allowRightToLeft = (numChannels > 2);
	}
	for(uint8 c = 0; c < validChannels; ++c)
	{
		if(vuMeter[c] != channels[c])
		{
			changed = true;
		}
	}
	if(changed)
	{
		const uint64 curTime = Util::GetTickCount64();
		if(curTime - lastVuUpdateTime >= TrackerSettings::Instance().VuMeterUpdateInterval || force)
		{
			for(uint8 c = 0; c < validChannels; ++c)
			{
				vuMeter[c] = channels[c];
			}
			redraw();
			lastVuUpdateTime = curTime;
		}
	}
}


// Draw stereo VU
void CStereoVU::DrawVuMeters(ui::Painter &dc)
{
	const Rect rect = GetClientRect();
	dc.FillSolidRect(rect.left, rect.top, rect.Width(), rect.Height(), RGB(0,0,0));

	for(uint8 channel = 0; channel < numChannels; ++channel)
	{
		Rect chanrect = rect;
		if(horizontal)
		{
			if(allowRightToLeft)
			{
				const int col = channel % 2;
				const int row = channel / 2;

				float width = (rect.Width() - 2.0f) / 2.0f;
				float height = rect.Height() / float(numChannels/2);

				chanrect.top = mpt::saturate_round<int32>(rect.top + height * row);
				chanrect.bottom = mpt::saturate_round<int32>(chanrect.top + height) - 1;
				
				chanrect.left = mpt::saturate_round<int32>(rect.left + width * col) + ((col == 1) ? 2 : 0);
				chanrect.right = mpt::saturate_round<int32>(chanrect.left + width) - 1;

			} else
			{
				float height = rect.Height() / float(numChannels);
				chanrect.top = mpt::saturate_round<int32>(rect.top + height * channel);
				chanrect.bottom = mpt::saturate_round<int32>(chanrect.top + height) - 1;
			}
		} else
		{
			float width = rect.Width() / float(numChannels);
			chanrect.left = mpt::saturate_round<int32>(rect.left + width * channel);
			chanrect.right = mpt::saturate_round<int32>(chanrect.left + width) - 1;
		}
		DrawVuMeter(dc, chanrect, channel);
	}

}


// Draw a single VU Meter
void CStereoVU::DrawVuMeter(ui::Painter &dc, const Rect &rect, int index)
{
	uint32 vu = vuMeter[index];

	if(CMainFrame::GetMainFrame()->GetSoundFilePlaying() == nullptr)
	{
		vu = 0;
	}

	const bool clip = (vu & Notification::ClipVU) != 0;
	vu = (vu & (~Notification::ClipVU)) >> 8;

	if(horizontal)
	{
		const bool rtl = allowRightToLeft && ((index % 2) == 0);

		const int cx = std::max(1, rect.Width());
		int v = (vu * cx) >> 8;

		for(int x = 0; x <= cx; x += 2)
		{
			int pen = Clamp((x * NUM_VUMETER_PENS) / cx, 0, NUM_VUMETER_PENS - 1);
			const bool last = (x == (cx & ~0x1));

			// Darken everything above volume, unless it's the clip indicator
			if(v <= x && (!last || !clip))
				pen += NUM_VUMETER_PENS;

			dc.FillSolidRect(
				((!rtl) ? (rect.left + x) : (rect.right - x)),
				rect.top, 1, rect.Height(), CMainFrame::gcolrefVuMeter[pen]);
		}
	} else
	{
		const int cy = std::max(1, rect.Height());
		int v = (vu * cy) >> 8;

		for(int ry = rect.bottom - 1; ry >= rect.top; ry -= 2)
		{
			const int y0 = rect.bottom - ry;
			int pen = Clamp((y0 * NUM_VUMETER_PENS) / cy, 0, NUM_VUMETER_PENS - 1);
			const bool last = (ry <= rect.top + 1);

			// Darken everything above volume, unless it's the clip indicator
			if(v <= y0 && (!last || !clip))
				pen += NUM_VUMETER_PENS;

			dc.FillSolidRect(rect.left, ry, rect.Width(), 1, CMainFrame::gcolrefVuMeter[pen]);
		}
	}
}


void CStereoVU::OnLButtonDown(uint32, Point)
{
	// Reset clip indicator.
	CMainFrame::GetMainFrame()->m_VUMeterInput.ResetClipped();
	CMainFrame::GetMainFrame()->m_VUMeterOutput.ResetClipped();
}


OPENMPT_NAMESPACE_END
