/*
 * ColorConfigDlg.cpp
 * ------------------
 * Purpose: Implementation of the display setup dialog.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "ui/Ui.h"
#include "ColorConfigDlg.h"
#include "ColorSchemes.h"
#include "FileDialog.h"
#include "Mainfrm.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "resource.h"
#include "Settings.h"
#include "SettingsIni.h"
#include "WindowMessages.h"
#include "../common/mptStringBuffer.h"


OPENMPT_NAMESPACE_BEGIN


static constexpr struct ColorDescriptions
{
	const mpt::uchar *name;
	int previewImage;
	std::array<ModColor, 3> colorIndex;
	std::array<const mpt::uchar *, 3> descText;
} colorDefs[] =
{
	{ UL_("Pattern Editor"),      0, { MODCOLOR_BACKNORMAL, MODCOLOR_TEXTNORMAL, MODCOLOR_BACKHILIGHT }, { UL_("Background:"), UL_("Foreground:"), UL_("Highlighted:") } },
	{ UL_("Active Row"),          0, { MODCOLOR_BACKCURROW, MODCOLOR_TEXTCURROW, MODCOLOR_BACKRECORDROW }, { UL_("Background:"), UL_("Foreground:"), UL_("Background (Record):")}},
	{ UL_("Pattern Selection"),   0, { MODCOLOR_BACKSELECTED, MODCOLOR_TEXTSELECTED, {} }, { UL_("Background:"), UL_("Foreground:"), nullptr } },
	{ UL_("Play Cursor"),         0, { MODCOLOR_BACKPLAYCURSOR, MODCOLOR_TEXTPLAYCURSOR, {} }, { UL_("Background:"), UL_("Foreground:"), nullptr } },
	{ UL_("Note Highlight"),      0, { MODCOLOR_NOTE, MODCOLOR_INSTRUMENT, MODCOLOR_VOLUME }, { UL_("Note:"), UL_("Instrument:"), UL_("Volume:") } },
	{ UL_("Effect Highlight"),    0, { MODCOLOR_PANNING, MODCOLOR_PITCH, MODCOLOR_GLOBALS }, { UL_("Panning Effects:"), UL_("Pitch Effects:"), UL_("Global Effects:") } },
	{ UL_("Invalid Commands"),    0, { MODCOLOR_DODGY_COMMANDS, {}, {} }, { UL_("Invalid Note:"), nullptr, nullptr } },
	{ UL_("Channel Separator"),   0, { MODCOLOR_SEPHILITE, MODCOLOR_SEPFACE, MODCOLOR_SEPSHADOW }, { UL_("Highlight:"), UL_("Face:"), UL_("Shadow:") } },
	{ UL_("Next/Prev Pattern"),   0, { MODCOLOR_BLENDCOLOR, {}, {} }, { UL_("Blend Colour:"), nullptr, nullptr } },
	{ UL_("Sample Waveform"),     1, { MODCOLOR_SAMPLE, MODCOLOR_BACKSAMPLE, MODCOLOR_SAMPLESELECTED }, { UL_("Sample Data:"), UL_("Background:"), UL_("Selection:") } },
	{ UL_("Sample Markers"),      1, { MODCOLOR_SAMPLE_LOOPMARKER, MODCOLOR_SAMPLE_SUSTAINMARKER, MODCOLOR_SAMPLE_CUEPOINT}, { UL_("Loop Marker:"), UL_("Sustain Marker:"), UL_("Cue Point:") } },
	{ UL_("Instrument Editor"),   2, { MODCOLOR_ENVELOPES, MODCOLOR_ENVELOPE_RELEASE, MODCOLOR_BACKENV }, { UL_("Envelopes:"), UL_("Release Envelope:"), UL_("Background:") } },
	{ UL_("VU-Meters"),           0, { MODCOLOR_VUMETER_HI, MODCOLOR_VUMETER_MED, MODCOLOR_VUMETER_LO }, { UL_("Hi:"), UL_("Med:"), UL_("Lo:") } },
	{ UL_("VU-Meters (Plugins)"), 0, { MODCOLOR_VUMETER_HI_VST, MODCOLOR_VUMETER_MED_VST, MODCOLOR_VUMETER_LO_VST }, { UL_("Hi:"), UL_("Med:"), UL_("Lo:") } }
};

#define PREVIEWBMP_WIDTH	88
#define PREVIEWBMP_HEIGHT	39


UI_MESSAGE_MAP_BEGIN(COptionsColors, PropertyPage)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO1,      &COptionsColors::OnColorSelChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO2,      &COptionsColors::OnSettingsChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO3,      &COptionsColors::OnPresetChange)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO4,      &COptionsColors::OnSettingsChanged)
	UI_NOTIFY(ui::EditChange, IDC_PRIMARYHILITE,   &COptionsColors::OnSettingsChanged)
	UI_NOTIFY(ui::EditChange, IDC_SECONDARYHILITE, &COptionsColors::OnSettingsChanged)
	UI_COMMAND(IDC_BUTTON1,           &COptionsColors::OnSelectColor1)
	UI_COMMAND(IDC_BUTTON2,           &COptionsColors::OnSelectColor2)
	UI_COMMAND(IDC_BUTTON3,           &COptionsColors::OnSelectColor3)
	UI_COMMAND(IDC_BUTTON9,           &COptionsColors::OnChoosePatternFont)
	UI_COMMAND(IDC_BUTTON10,          &COptionsColors::OnChooseCommentFont)
	UI_COMMAND(IDC_BUTTON11,          &COptionsColors::OnClearWindowCache)
	UI_COMMAND(IDC_LOAD_COLORSCHEME,  &COptionsColors::OnLoadColorScheme)
	UI_COMMAND(IDC_SAVE_COLORSCHEME,  &COptionsColors::OnSaveColorScheme)
	UI_COMMAND(IDC_CHECK1,            &COptionsColors::OnHighlightsChanged)
	UI_COMMAND(IDC_CHECK2,            &COptionsColors::OnPreviewChanged)
	UI_COMMAND(IDC_CHECK3,            &COptionsColors::OnSettingsChanged)
	UI_COMMAND(IDC_CHECK4,            &COptionsColors::OnHighlightsChanged)
	UI_COMMAND(IDC_CHECK5,            &COptionsColors::OnSettingsChanged)
	UI_COMMAND(IDC_RADIO1,            &COptionsColors::OnSettingsChanged)
	UI_COMMAND(IDC_RADIO2,            &COptionsColors::OnSettingsChanged)
	UI_COMMAND(IDC_RADIO3,            &COptionsColors::OnSettingsChanged)
	UI_COMMAND(IDC_RADIO4,            &COptionsColors::OnSettingsChanged)
	UI_COMMAND(IDC_RADIO5,            &COptionsColors::OnSettingsChanged)
UI_MESSAGE_MAP_END()


void COptionsColors::DoDataExchange(DataExchange* pDX)
{
	PropertyPage::DoDataExchange(pDX);
	pDX->BindControl(IDC_COMBO1, m_ComboItem);
	pDX->BindControl(IDC_COMBO2, m_ComboFont);
	pDX->BindControl(IDC_COMBO3, m_ComboPreset);
	pDX->BindControl(IDC_COMBO4, m_ComboDPIAwareness);
	pDX->BindControl(IDC_BUTTON1, m_BtnColor[0]);
	pDX->BindControl(IDC_BUTTON2, m_BtnColor[1]);
	pDX->BindControl(IDC_BUTTON3, m_BtnColor[2]);
	pDX->BindControl(IDC_BUTTON4, m_BtnPreview);
	pDX->BindControl(IDC_TEXT1, m_TxtColor[0]);
	pDX->BindControl(IDC_TEXT2, m_TxtColor[1]);
	pDX->BindControl(IDC_TEXT3, m_TxtColor[2]);
	pDX->BindControl(IDC_SPIN1, m_ColorSpin);
	pDX->BindControl(IDC_PRIMARYHILITE, m_spinRowsPerMeasure);
	pDX->BindControl(IDC_SECONDARYHILITE, m_spinRowsPerBeat);
}


COptionsColors::COptionsColors()
    : PropertyPage{IDD_OPTIONS_COLORS}
    , CustomColors{TrackerSettings::Instance().rgbCustomColors}
{
	m_spinRowsPerBeat.SetAccessibleSuffix(UL_("rows per beat"));
	m_spinRowsPerMeasure.SetAccessibleSuffix(UL_("rows per measure"));
}


COptionsColors::~COptionsColors()
{
}


static mpt::ustring FormatFontName(const FontSetting &font)
{
	return mpt::ToUnicode(font.name + UL_(", ") + mpt::ufmt::val(font.size / 10));
}


bool COptionsColors::OnInitDialog()
{
	PropertyPage::OnInitDialog();
	
	m_pPreviewDib = LoadDib(IDB_COLORSETUP);
	m_BtnPreview.onDraw = [this](ui::Painter &dc, const Rect &rect) { DrawPreview(dc, rect); };
	
	m_ComboDPIAwareness.SetRedraw(false);
	static constexpr std::pair<const mpt::uchar *, DPIAwarenessMode> DPIModes[] =
	{
		{UL_("Not DPI-Aware"),                DPIAwarenessMode::NoDPIAwareness           },
		{UL_("Not DPI-Aware; GDI upscaling"), DPIAwarenessMode::NoDPIAwarenessGDIUpscaled},
		{UL_("System DPI-Aware"),             DPIAwarenessMode::SystemDPIAware           },
		{UL_("Per-Monitor DPI-Aware"),        DPIAwarenessMode::PerMonitorDPIAware       },
	};
	const DPIAwarenessMode currentDPIMode = TrackerSettings::Instance().dpiAwareness;
	for(const auto dpiMode : DPIModes)
	{
		int item = m_ComboDPIAwareness.AddString(dpiMode.first);
		m_ComboDPIAwareness.SetItemData(item, static_cast<uintptr_t>(dpiMode.second));
		if(currentDPIMode == dpiMode.second)
			m_ComboDPIAwareness.SetCurSel(item);
	}
	m_ComboDPIAwareness.SetRedraw(true);
	
	m_ComboItem.SetRedraw(false);
	for (size_t i = 0; i < std::size(colorDefs); i++)
	{
		m_ComboItem.SetItemData(m_ComboItem.AddString(colorDefs[i].name), i);
	}
	m_ComboItem.SetRedraw(true);
	m_ComboItem.SetCurSel(0);

	m_BtnPreview.SetWindowPos(nullptr,
		0, 0,
		ui::ScalePixels(PREVIEWBMP_WIDTH * 2, this) + 2, ui::ScalePixels(PREVIEWBMP_HEIGHT * 2, this) + 2,
		ui::PosNoMove | ui::PosNoZOrder | ui::PosNoActivate);
	const FlagSet<PatternSetup> patternSetup = TrackerSettings::Instance().patternSetup;
	if(patternSetup[PatternSetup::HighlightMeasures]) CheckDlgButton(IDC_CHECK1, ui::CheckOn);
	if(patternSetup[PatternSetup::EffectHighlight]) CheckDlgButton(IDC_CHECK2, ui::CheckOn);
	if(patternSetup[PatternSetup::HighlightBeats]) CheckDlgButton(IDC_CHECK4, ui::CheckOn);
	CheckDlgButton(IDC_CHECK3, TrackerSettings::Instance().patternIgnoreSongTimeSignature ? ui::CheckOn : ui::CheckOff);
	CheckDlgButton(IDC_CHECK5, TrackerSettings::Instance().rememberSongWindows ? ui::CheckOn : ui::CheckOff);
	CheckRadioButton(IDC_RADIO1, IDC_RADIO2, TrackerSettings::Instance().accidentalFlats ? IDC_RADIO2 : IDC_RADIO1);
	CheckRadioButton(IDC_RADIO3, IDC_RADIO5, IDC_RADIO3 + static_cast<int>(TrackerSettings::Instance().defaultRainbowChannelColors.Get()));
	GetDlgItem(IDC_CHECK3)->EnableWindow((IsDlgButtonChecked(IDC_CHECK1) || IsDlgButtonChecked(IDC_CHECK4)) ? true : false);

	SetDlgItemInt(IDC_PRIMARYHILITE, TrackerSettings::Instance().m_nRowHighlightMeasures);
	SetDlgItemInt(IDC_SECONDARYHILITE, TrackerSettings::Instance().m_nRowHighlightBeats);
	m_spinRowsPerMeasure.SetRange32(0, MAX_ROWS_PER_MEASURE);
	m_spinRowsPerBeat.SetRange32(0, MAX_ROWS_PER_BEAT);

	patternFont = TrackerSettings::Instance().patternFont;
	m_ComboFont.SetRedraw(false);
	m_ComboFont.AddString(UL_("Built-in (small)"));
	m_ComboFont.AddString(UL_("Built-in (large)"));
	m_ComboFont.AddString(UL_("Built-in (small, x2)"));
	m_ComboFont.AddString(UL_("Built-in (large, x2)"));
	m_ComboFont.AddString(UL_("Built-in (small, x3)"));
	m_ComboFont.AddString(UL_("Built-in (large, x3)"));
	int sel = 0;
	if(patternFont.name == PATTERNFONT_SMALL)
	{
		sel = patternFont.size * 2;
	} else if(patternFont.name == PATTERNFONT_LARGE)
	{
		sel = patternFont.size * 2 + 1;
	} else
	{
		m_ComboFont.AddString(FormatFontName(patternFont));
		sel = 6;
	}
	m_ComboFont.SetRedraw(true);
	m_ComboFont.SetCurSel(sel);

	commentFont = TrackerSettings::Instance().commentsFont;
	SetDlgItemText(IDC_BUTTON10, FormatFontName(commentFont));

	m_ComboPreset.SetRedraw(false);
	const size_t numItems = 2 + std::size(ColorSchemes);
	m_ComboPreset.InitStorage(static_cast<int>(numItems), static_cast<uint32>(numItems * 20 * sizeof(mpt::uchar)));
	m_ComboPreset.AddString(UL_("Choose a Colour Scheme..."));
	m_ComboPreset.AddString(UL_("OpenMPT (Default)"));
	for(const auto &preset : ColorSchemes)
	{
		m_ComboPreset.SetItemDataPtr(m_ComboPreset.AddString(preset.name), const_cast<ColorScheme *>(&preset));
	}
	m_ComboPreset.SetCurSel(0);
	m_ComboPreset.SetRedraw(true);

	m_ColorSpin.SetRange32(-1, 1);
	
	OnColorSelChanged();
	return true;
}


bool COptionsColors::OnKillActive()
{
	int highlightMeasures = GetDlgItemInt(IDC_PRIMARYHILITE);
	int highlightBeats = GetDlgItemInt(IDC_SECONDARYHILITE);

	if(highlightBeats > highlightMeasures)
	{
		Reporting::Warning("Error: Primary highlight must be greater than or equal secondary highlight.");
		GotoDlgCtrl(GetDlgItem(IDC_PRIMARYHILITE));
		return 0;
	}

	return PropertyPage::OnKillActive();
}


void COptionsColors::OnOK()
{
	if(m_ComboDPIAwareness.GetCurSel() >= 0)
	{
		const DPIAwarenessMode currentDPIMode = TrackerSettings::Instance().dpiAwareness;
		const DPIAwarenessMode newDPIMode = static_cast<DPIAwarenessMode>(m_ComboDPIAwareness.GetItemData(m_ComboDPIAwareness.GetCurSel()));
		if(newDPIMode != currentDPIMode)
		{
			TrackerSettings::Instance().dpiAwareness = newDPIMode;
			Reporting::Information(UL_("You need to restart OpenMPT for the new DPI-awareness setting to take effect."));
		}
	}

	FlagSet<PatternSetup> patternSetup = TrackerSettings::Instance().patternSetup;
	patternSetup.set(PatternSetup::HighlightMeasures, IsDlgButtonChecked(IDC_CHECK1) != ui::CheckOff);
	patternSetup.set(PatternSetup::EffectHighlight, IsDlgButtonChecked(IDC_CHECK2) != ui::CheckOff);
	patternSetup.set(PatternSetup::HighlightBeats, IsDlgButtonChecked(IDC_CHECK4) != ui::CheckOff);
	TrackerSettings::Instance().patternSetup = patternSetup;
	TrackerSettings::Instance().patternIgnoreSongTimeSignature = IsDlgButtonChecked(IDC_CHECK3) != ui::CheckOff;
	TrackerSettings::Instance().rememberSongWindows = IsDlgButtonChecked(IDC_CHECK5) != ui::CheckOff;
	TrackerSettings::Instance().accidentalFlats = IsDlgButtonChecked(IDC_RADIO2) != ui::CheckOff;

	int channelColors = GetCheckedRadioButton(IDC_RADIO3, IDC_RADIO5);
	if(channelColors == IDC_RADIO3)
		TrackerSettings::Instance().defaultRainbowChannelColors = DefaultChannelColors::NoColors;
	else if(channelColors == IDC_RADIO4)
		TrackerSettings::Instance().defaultRainbowChannelColors = DefaultChannelColors::Rainbow;
	else
		TrackerSettings::Instance().defaultRainbowChannelColors = DefaultChannelColors::Random;

	FontSetting newPatternFont = patternFont;
	const int fontSel = m_ComboFont.GetCurSel();
	switch(fontSel)
	{
	case 0:
	case 2:
	case 4:
	default:
		newPatternFont.name = PATTERNFONT_SMALL;
		newPatternFont.size = fontSel / 2;
		break;
	case 1:
	case 3:
	case 5:
		newPatternFont.name = PATTERNFONT_LARGE;
		newPatternFont.size = fontSel / 2;
		break;
	case 6:
		break;
	}
	TrackerSettings::Instance().patternFont = newPatternFont;
	TrackerSettings::Instance().commentsFont = commentFont;

	TrackerSettings::Instance().m_nRowHighlightMeasures = GetDlgItemInt(IDC_PRIMARYHILITE);
	TrackerSettings::Instance().m_nRowHighlightBeats = GetDlgItemInt(IDC_SECONDARYHILITE);

	TrackerSettings::Instance().rgbCustomColors = CustomColors;
	CMainFrame::UpdateColors();
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if (pMainFrm) pMainFrm->PostMessage(MSG_MOD_INVALIDATEPATTERNS, HINT_MPTOPTIONS);
	CTrackerSoundFile::SetDefaultNoteNames();
	PropertyPage::OnOK();
}


bool COptionsColors::OnSetActive()
{
	CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_COLORS;
	return PropertyPage::OnSetActive();
}


void COptionsColors::OnChoosePatternFont()
{
	const int32 size = patternFont.size < 10 ? 120 : patternFont.size;
	ui::FontDialog dlg(patternFont.name, size, patternFont.flags[FontSetting::Bold], patternFont.flags[FontSetting::Italic]);
	if(dlg.DoModal() == IDOK)
	{
		while(m_ComboFont.GetCount() > 6)
		{
			m_ComboFont.DeleteString(6);
		}
		patternFont.name = dlg.GetFaceName();
		patternFont.size = dlg.GetSize();
		patternFont.flags = FontSetting::None;
		if(dlg.IsBold()) patternFont.flags |= FontSetting::Bold;
		if(dlg.IsItalic()) patternFont.flags |= FontSetting::Italic;
		m_ComboFont.AddString(FormatFontName(patternFont));
		m_ComboFont.SetCurSel(6);
		OnSettingsChanged();
	}
}


void COptionsColors::OnChooseCommentFont()
{
	ui::FontDialog dlg(commentFont.name, commentFont.size, commentFont.flags[FontSetting::Bold], commentFont.flags[FontSetting::Italic]);
	if(dlg.DoModal() == IDOK)
	{
		commentFont.name = dlg.GetFaceName();
		commentFont.size = dlg.GetSize();
		commentFont.flags = FontSetting::None;
		if(dlg.IsBold()) commentFont.flags |= FontSetting::Bold;
		if(dlg.IsItalic()) commentFont.flags |= FontSetting::Italic;
		SetDlgItemText(IDC_BUTTON10, FormatFontName(commentFont));
		OnSettingsChanged();
	}
}


void COptionsColors::DrawPreview(ui::Painter &dc, const Rect &outerRect)
{
	if(!m_pPreviewDib)
		return;

	const int img = colorDefs[m_nColorItem].previewImage;
	ColorRef p[16];
	std::copy(std::begin(m_pPreviewDib->palette), std::end(m_pPreviewDib->palette), std::begin(p));
	if (IsDlgButtonChecked(IDC_CHECK2))
	{
		p[1] = CustomColors[MODCOLOR_GLOBALS];
		p[3] = CustomColors[MODCOLOR_PITCH];
		p[5] = CustomColors[MODCOLOR_INSTRUMENT];
		p[6] = CustomColors[MODCOLOR_VOLUME];
		p[12] = CustomColors[MODCOLOR_NOTE];
		p[14] = CustomColors[MODCOLOR_PANNING];
	} else
	{
		p[1] = CustomColors[MODCOLOR_TEXTNORMAL];
		p[3] = CustomColors[MODCOLOR_TEXTNORMAL];
		p[5] = CustomColors[MODCOLOR_TEXTNORMAL];
		p[6] = CustomColors[MODCOLOR_TEXTNORMAL];
		p[12] = CustomColors[MODCOLOR_TEXTNORMAL];
		p[14] = CustomColors[MODCOLOR_TEXTNORMAL];
	}
	p[4] = CustomColors[MODCOLOR_TEXTNORMAL];
	p[8] = CustomColors[MODCOLOR_TEXTNORMAL];
	p[9] = CustomColors[MODCOLOR_SAMPLE];
	p[10] = CustomColors[MODCOLOR_BACKNORMAL];
	p[11] = CustomColors[MODCOLOR_BACKHILIGHT];
	p[13] = CustomColors[MODCOLOR_ENVELOPES];
	p[15] = img ? RGB(255, 255, 255) : CustomColors[MODCOLOR_BACKNORMAL];
	// Special cases: same bitmap, different palette
	switch(m_nColorItem)
	{
	// Current Row
	case 1:
		p[8] = CustomColors[MODCOLOR_TEXTCURROW];
		p[11] = CustomColors[MODCOLOR_BACKCURROW];
		break;
	// Selection
	case 2:
		p[5] = CustomColors[MODCOLOR_TEXTSELECTED];
		p[6] = CustomColors[MODCOLOR_TEXTSELECTED];
		p[8] = CustomColors[MODCOLOR_TEXTSELECTED];
		p[11] = CustomColors[MODCOLOR_BACKSELECTED];
		p[12] = CustomColors[MODCOLOR_TEXTSELECTED];
		break;
	// Play Cursor
	case 3:
		p[8] = CustomColors[MODCOLOR_TEXTPLAYCURSOR];
		p[11] = CustomColors[MODCOLOR_BACKPLAYCURSOR];
		break;
	// Sample Editor
	case 9:
	case 10:
		p[0] = CustomColors[MODCOLOR_BACKSAMPLE];
		p[7] = ui::GetSystemColor(ui::SysColor::ButtonFace);
		p[8] = ui::GetSystemColor(ui::SysColor::ButtonShadow);
		p[10] = CustomColors[MODCOLOR_SAMPLE_LOOPMARKER];
		p[11] = CustomColors[MODCOLOR_SAMPLE_CUEPOINT];
		p[14] = CustomColors[MODCOLOR_SAMPLE_SUSTAINMARKER];
		p[15] = CustomColors[MODCOLOR_SAMPLESELECTED];
		break;
	// Envelope Editor
	case 11:
		p[0] = CustomColors[MODCOLOR_BACKENV];
		p[2] = CustomColors[MODCOLOR_ENVELOPE_RELEASE];
		break;
	}

	Rect rect = outerRect;
	dc.Draw3dRect(rect, ui::GetSystemColor(ui::SysColor::ButtonShadow), ui::GetSystemColor(ui::SysColor::ButtonHighlight));
	rect.DeflateRect(1, 1);
	ui::Bitmap bitmap(m_pPreviewDib->width, PREVIEWBMP_HEIGHT);
	for(int row = 0; row < PREVIEWBMP_HEIGHT; ++row)
	{
		for(int column = 0; column < m_pPreviewDib->width; ++column)
		{
			const int sourceRow = img * PREVIEWBMP_HEIGHT + row;
			if(sourceRow >= m_pPreviewDib->height)
				continue;
			bitmap.SetPixel(column, row, p[m_pPreviewDib->pixels[static_cast<size_t>(sourceRow) * m_pPreviewDib->width + column] & 0x0F]);
		}
	}
	dc.StretchBitmap(bitmap, rect);
}


static ColorRef rgbCustomColors[16] =
{
	0x808080,	0x0000FF,	0x00FF00,	0x00FFFF,
	0xFF0000,	0xFF00FF,	0xFFFF00,	0xFFFFFF,
	0xC0C0C0,	0x80FFFF,	0xE0E8E0,	0x606060,
	0x505050,	0x404040,	0x004000,	0x000000,
};


void COptionsColors::SelectColor(int colorIndex)
{
	auto &color = CustomColors[colorDefs[m_nColorItem].colorIndex[colorIndex]];
	ColorRef chosenColor = color;
	if(ui::ChooseColor(chosenColor))
	{
		color = chosenColor;
		m_BtnColor[colorIndex].SetColor(color);
		OnSettingsChanged();
	}
}


void COptionsColors::OnVScroll(uint32 nSBCode, uint32 nPos, Wnd *pScrollBar)
{
	PropertyPage::OnVScroll(nSBCode, nPos, pScrollBar);
	int newSel = m_ComboItem.GetCurSel() - m_ColorSpin.GetPos32();
	if(newSel >= 0)
	{
		m_ComboItem.SetCurSel(newSel);
		OnColorSelChanged();
	}
	m_ColorSpin.SetPos(0);
}


void COptionsColors::OnColorSelChanged()
{
	int sel = m_ComboItem.GetCurSel();
	if (sel >= 0)
	{
		m_nColorItem = static_cast<uint32>(m_ComboItem.GetItemData(sel));
		OnUpdateDialog();
	}
}


void COptionsColors::OnHighlightsChanged()
{
	GetDlgItem(IDC_CHECK3)->EnableWindow((IsDlgButtonChecked(IDC_CHECK1) || IsDlgButtonChecked(IDC_CHECK4)) ? true : false);
	OnSettingsChanged();
}


void COptionsColors::OnSettingsChanged()
{
	SetModified(true);
}

void COptionsColors::OnUpdateDialog()
{
	const ColorDescriptions &cd = colorDefs[m_nColorItem];
	for(int i = 0; i < 3; i++)
	{
		if(cd.descText[i])
		{
			m_TxtColor[i].SetWindowText(cd.descText[i]);
			m_BtnColor[i].SetColor(CustomColors[cd.colorIndex[i]]);
		}
		m_TxtColor[i].ShowWindow(cd.descText[i] ? true : false);
		m_BtnColor[i].ShowWindow(cd.descText[i] ? true : false);
	}

	m_BtnPreview.Invalidate(false);
}


void COptionsColors::OnPreviewChanged()
{
	OnSettingsChanged();
	m_BtnPreview.Invalidate(false);
	for(int i = 0; i < 3; i++)
	{
		m_BtnColor[i].SetColor(CustomColors[colorDefs[m_nColorItem].colorIndex[i]]);
	}
}


void COptionsColors::OnPresetChange()
{
	auto curSel = m_ComboPreset.GetCurSel();
	if(curSel == 0)
		return;
	TrackerSettings::GetDefaultColourScheme(CustomColors);
	auto scheme = static_cast<const ColorScheme *>(m_ComboPreset.GetItemDataPtr(curSel));
	if(scheme != nullptr)
	{
		for(const auto &c : scheme->colors)
		{
			CustomColors[c.id] = c.color;
		}
	}
	OnPreviewChanged();
}


void COptionsColors::OnLoadColorScheme()
{
	FileDialog dlg = OpenFileDialog()
		.DefaultExtension(UL_("mptcolor"))
		.ExtensionFilter(UL_("OpenMPT Color Schemes|*.mptcolor||"))
		.WorkingDirectory(theApp.GetConfigPath());
	if(!dlg.Show(this)) return;

	// Ensure that all colours are reset (for outdated colour schemes)
	TrackerSettings::GetDefaultColourScheme(CustomColors);
	{
		IniFileSettingsContainer file(dlg.GetFirstFile());
		for(uint32 i = 0; i < MAX_MODCOLORS; i++)
		{
			CustomColors[i] = file.Read<int32>(UL_("Colors"), MPT_UFORMAT("Color{}")(mpt::ufmt::dec0<2>(i)), CustomColors[i]);
			// For old color schemes that don't have this color yet
			if(i == MODCOLOR_BACKCURROW)
				CustomColors[MODCOLOR_BACKRECORDROW] = CustomColors[MODCOLOR_BACKCURROW];
		}
	}
	OnPreviewChanged();
}

void COptionsColors::OnSaveColorScheme()
{
	FileDialog dlg = SaveFileDialog()
		.DefaultExtension(UL_("mptcolor"))
		.ExtensionFilter(UL_("OpenMPT Color Schemes|*.mptcolor||"))
		.WorkingDirectory(theApp.GetConfigPath());
	if(!dlg.Show(this)) return;

	{
		IniFileSettingsContainer file(dlg.GetFirstFile());
		for(uint32 i = 0; i < MAX_MODCOLORS; i++)
		{
			file.Write<int32>(UL_("Colors"), MPT_UFORMAT("Color{}")(mpt::ufmt::dec0<2>(i)), CustomColors[i]);
		}
	}
}


void COptionsColors::OnClearWindowCache()
{
	SettingsContainer &settings = theApp.GetSongSettings();
	// First, forget all settings...
	settings.InvalidateCache();
	// Then make sure they are gone for good.
	Util::DeleteFile(theApp.GetSongSettingsFilename());
}


OPENMPT_NAMESPACE_END
