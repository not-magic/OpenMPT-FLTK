// FLTK port of openmpt/mptrack/view_com.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "view_com.h"
#include "Childfrm.h"
#include "Clipboard.h"
#include "Ctrl_com.h"
#include "Globals.h"
#include "ImageLists.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "WindowMessages.h"
#include "../common/mptStringBuffer.h"
#include "../soundlib/mod_specifications.h"

OPENMPT_NAMESPACE_BEGIN


#define DETAILS_TOOLBAR_CY ui::ScalePixels(28, this)

enum
{
	SMPLIST_SAMPLENAME = 0,
	SMPLIST_SAMPLENO,
	SMPLIST_SIZE,
	SMPLIST_TYPE,
	SMPLIST_MIDDLEC,
	SMPLIST_INSTR,
	SMPLIST_FILENAME,
	SMPLIST_PATH,
	SMPLIST_COLUMNS
};


enum
{
	INSLIST_INSTRUMENTNAME = 0,
	INSLIST_INSTRUMENTNO,
	INSLIST_SAMPLES,
	INSLIST_ENVELOPES,
	INSLIST_FILENAME,
	INSLIST_PLUGIN,
	INSLIST_COLUMNS
};


static constexpr CListCtrlEx::Header SampleHeaders[SMPLIST_COLUMNS] =
{
	{ UL_("Sample Name"), 212, ui::ListColumnLeft },
	{ UL_("Num"),         45,  ui::ListColumnRight },
	{ UL_("Size"),        72,  ui::ListColumnRight },
	{ UL_("Type"),        96,  ui::ListColumnRight },
	{ UL_("C-5 Freq"),    80,  ui::ListColumnRight },
	{ UL_("Instr"),       64,  ui::ListColumnRight },
	{ UL_("File Name"),   160, ui::ListColumnRight },
	{ UL_("Path"),        256, ui::ListColumnLeft },
};

static constexpr CListCtrlEx::Header InstrumentHeaders[INSLIST_COLUMNS] =
{
	{ UL_("Instrument Name"), 212, ui::ListColumnLeft },
	{ UL_("Num"),             45,  ui::ListColumnRight },
	{ UL_("Samples"),         64,  ui::ListColumnRight },
	{ UL_("Envelopes"),       128, ui::ListColumnRight },
	{ UL_("File Name"),       160, ui::ListColumnRight },
	{ UL_("Plugin"),          128, ui::ListColumnRight },
};


UI_MESSAGE_MAP_BEGIN(CViewComments, CModScrollView)
	UI_MESSAGE(MSG_MOD_KEYCOMMAND,       &CViewComments::OnCustomKeyMsg)
	UI_MESSAGE(MSG_MOD_MIDIMSG,          &CViewComments::OnMidiMsg)
	UI_COMMAND(IDC_LIST_SAMPLES,        &CViewComments::OnShowSamples)
	UI_COMMAND(IDC_LIST_INSTRUMENTS,    &CViewComments::OnShowInstruments)
	UI_COMMAND(IDC_LIST_PATTERNS,       &CViewComments::OnShowPatterns)
	UI_COMMAND(ID_EDIT_COPY,            &CViewComments::OnCopyNames)
	UI_COMMAND(ID_EDIT_PASTE,           &CViewComments::OnPasteNames)
	UI_NOTIFY(ui::ListEndLabelEdit, IDC_LIST_DETAILS, &CViewComments::OnEndLabelEdit)
	UI_NOTIFY(ui::ListBeginLabelEdit, IDC_LIST_DETAILS, &CViewComments::OnBeginLabelEdit)
	UI_NOTIFY(ui::ListDblClick, IDC_LIST_DETAILS, &CViewComments::OnDblClickListItem)
	UI_NOTIFY(ui::ListRClick, IDC_LIST_DETAILS, &CViewComments::OnRClickListItem)
UI_MESSAGE_MAP_END()


void CViewComments::OnInitialUpdate()
{
	CModScrollView::OnInitialUpdate();
	if(m_nListId == 0)
	{
		m_nListId = IDC_LIST_SAMPLES;

		// For XM, set the instrument list as the default list
		const CModDoc *pModDoc = GetDocument();
		if(pModDoc && pModDoc->GetSoundFile().GetMessageHeuristic() == ModMessageHeuristicOrder::InstrumentsSamples && pModDoc->GetNumInstruments() > 0)
		{
			m_nListId = IDC_LIST_INSTRUMENTS;
		}
	}

	CChildFrame *pFrame = (CChildFrame *)GetParentFrame();
	Rect rect;

	if (pFrame)
	{
		CommentsViewState &commentState = pFrame->GetCommentViewState();
		if (commentState.initialized)
		{
			m_nListId = commentState.nId;
		}
	}
	GetClientRect(&rect);
	m_ToolBar.CreateChild(*this, rect, IDC_TOOLBAR_DETAILS);
	m_ToolBar.Init(CMainFrame::GetMainFrame()->m_MiscIcons, CMainFrame::GetMainFrame()->m_MiscIconsDisabled);
	m_ItemList.CreateChild(*this, rect, IDC_LIST_DETAILS);
	m_ItemList.onQueryCellFont = [this](int, int column, ui::Font &font)
	{
		const bool useFont = column == 0 || (m_nListId == IDC_LIST_SAMPLES && column == SMPLIST_FILENAME) || (m_nListId == IDC_LIST_INSTRUMENTS && column == INSLIST_FILENAME);
		if(useFont)
			font = m_fixedFont;
		return useFont;
	};
	m_ItemList.SetExtendedStyle(m_ItemList.GetExtendedStyle() | ui::ListStyleFullRowSelect);

	// Add ToolBar Buttons
	m_ToolBar.AddButton(IDC_LIST_SAMPLES, IMAGE_SAMPLES);
	m_ToolBar.AddButton(IDC_LIST_INSTRUMENTS, IMAGE_INSTRUMENTS);
	//m_ToolBar.AddButton(IDC_LIST_PATTERNS, TIMAGE_TAB_PATTERNS);
	UpdateButtonState();
	OnDPIChanged();
}


void CViewComments::OnDPIChanged()
{
	UpdateView(GeneralHint().MPTOptions().ModType());
	m_ToolBar.SetIndent(ui::ScalePixels(4, this));
	const int imgSize = ui::ScalePixels(16, this), btnSizeX = ui::ScalePixels(26, this), btnSizeY = ui::ScalePixels(24, this);
	m_ToolBar.SetButtonSize(Size(btnSizeX, btnSizeY));
	m_ToolBar.SetBitmapSize(Size(imgSize, imgSize));
	RecalcLayout();
	CModScrollView::OnDPIChanged();
}


// cppcheck-suppress duplInheritedMember
void CViewComments::OnDestroy()
{
	if(m_lastNote != NOTE_NONE)
		GetDocument()->NoteOff(m_lastNote, true, m_noteInstr, m_noteChannel);

	CChildFrame *pFrame = (CChildFrame *)GetParentFrame();
	if (pFrame)
	{
		CommentsViewState &commentState = pFrame->GetCommentViewState();
		commentState.initialized = true;
		commentState.nId = m_nListId;
	}
	CModScrollView::OnDestroy();
}


LResult CViewComments::OnModViewMsg(WParam wParam, LParam lParam)
{
	switch(wParam)
	{
		case VIEWMSG_SETFOCUS:
		case VIEWMSG_SETACTIVE:
			GetParentFrame()->SetActiveView(this);
			m_ItemList.SetFocus();
			return 0;
		default:
			return CModScrollView::OnModViewMsg(wParam, lParam);
	}
}


LResult CViewComments::OnMidiMsg(WParam midiData_, LParam)
{
	uint32 midiData = static_cast<uint32>(midiData_);
	INSTRUMENTINDEX ins = 0;
	SAMPLEINDEX smp = 0;

	const int item = m_ItemList.GetSelectionMark() + 1;
	if(item > 0 && m_nListId == IDC_LIST_SAMPLES)
		smp = static_cast<SAMPLEINDEX>(item);
	else if(item > 0 && m_nListId == IDC_LIST_INSTRUMENTS)
		ins = static_cast<INSTRUMENTINDEX>(item);

	GetDocument()->ProcessMIDI(midiData, smp, ins, GetDocument()->GetSoundFile().GetInstrumentPlugin(ins), kCtxViewComments);
	return 1;
}


LResult CViewComments::OnCustomKeyMsg(WParam wParam, LParam)
{
	const int item = m_ItemList.GetSelectionMark() + 1;
	if(item == 0)
		return kcNull;

	auto modDoc = GetDocument();
	if(wParam >= kcCommentsStartNotes && wParam <= kcCommentsEndNotes)
	{
		const auto lastInstr = m_noteInstr;
		m_noteInstr = (m_nListId == IDC_LIST_SAMPLES) ? INSTRUMENTINDEX_INVALID : static_cast<INSTRUMENTINDEX>(item);
		const auto note = modDoc->GetNoteWithBaseOctave(static_cast<int>(wParam - kcCommentsStartNotes), m_noteInstr);
		PlayNoteParam params(note);
		if(m_nListId == IDC_LIST_SAMPLES)
			params.Sample(static_cast<SAMPLEINDEX>(item));
		else if(m_nListId == IDC_LIST_INSTRUMENTS)
			params.Instrument(m_noteInstr);
		else
			return kcNull;
		if(m_lastNote != NOTE_NONE)
			modDoc->NoteOff(m_lastNote, true, lastInstr, m_noteChannel);
		m_noteChannel = modDoc->PlayNote(params);
		m_lastNote = note;
		return wParam;
	} else if(wParam >= kcCommentsStartNoteStops && wParam <= kcCommentsEndNoteStops)
	{
		const auto note = modDoc->GetNoteWithBaseOctave(static_cast<int>(wParam - kcCommentsStartNoteStops), m_noteInstr);
		modDoc->NoteOff(note, false, m_noteInstr, m_noteChannel);
		return wParam;
	} else if(wParam == kcToggleSmpInsList)
	{
		bool ok = SwitchToList(m_nListId == IDC_LIST_SAMPLES ? IDC_LIST_INSTRUMENTS : IDC_LIST_SAMPLES);
		if(ok)
		{
			int newItem = 0;
			switch(m_nListId)
			{
			case IDC_LIST_SAMPLES:
				// Switch to a sample belonging to previously selected instrument
				if(SAMPLEINDEX smp = modDoc->FindInstrumentChild(static_cast<INSTRUMENTINDEX>(item)); smp != 0 && smp != SAMPLEINDEX_INVALID)
					newItem = smp - 1;
				break;

			case IDC_LIST_INSTRUMENTS:
				// Switch to parent instrument of previously selected sample
				if(INSTRUMENTINDEX ins = modDoc->FindSampleParent(static_cast<SAMPLEINDEX>(item)); ins != 0 && ins != INSTRUMENTINDEX_INVALID)
					newItem = ins - 1;
				break;
			}
			m_ItemList.SetItemState(newItem, ui::ListItemSelected | ui::ListItemFocused, ui::ListItemSelected | ui::ListItemFocused);
			m_ItemList.SetSelectionMark(newItem);
			m_ItemList.EnsureVisible(newItem, false);
			m_ItemList.SetFocus();
		}
		return wParam;
	} else if(wParam == kcExecuteSmpInsListItem)
	{
		OnDblClickListItem(nullptr, nullptr);
		return wParam;
	} else if(wParam == kcRenameSmpInsListItem)
	{
		m_ItemList.EditLabel(item - 1);
		return wParam;
	} else if(wParam == kcEditCopy)
	{
		OnCopyNames();
		return wParam;
	} else if(wParam == kcEditPaste)
	{
		OnPasteNames();
		return wParam;
	}
	return kcNull;
}


bool CViewComments::PreTranslateMessage(int event)
{
	if(m_editLabel && event == FL_KEYBOARD && ui::KeyFromEvent() == ui::Key_ESCAPE)
	{
		m_editLabel = false;
		m_ItemList.CancelLabelEdit();
		m_ItemList.SetFocus();
		return true;
	}

	if(event == FL_KEYBOARD || event == FL_KEYUP)
	{
		CInputHandler *ih = CMainFrame::GetInputHandler();
		if(!ih->IsBypassed() && ih->KeyEvent(kCtxViewComments, ih->Translate(ui::KeyFromEvent(), 1, (event == FL_KEYUP) ? ui::KeyFlagRelease : (ui::IsKeyRepeat() ? ui::KeyFlagRepeat : 0))) != kcNull)
		{
			return true;  // Mapped to a command, no need to pass message on.
		}
	}

	return CModScrollView::PreTranslateMessage(event);
}


///////////////////////////////////////////////////////////////
// CViewComments drawing

void CViewComments::UpdateView(UpdateHint hint, HintObject *)
{
	const CModDoc *pModDoc = GetDocument();

	if ((!pModDoc) || (!((&m_ItemList)))) return;
	const FlagSet<HintType> hintType = hint.GetType();
	if (hintType[HINT_MPTOPTIONS])
	{
		m_ToolBar.UpdateStyle();
		
		// Font for sample / instrument names
		m_fixedFont = ui::CreateFont(mpt::ToUnicode(TrackerSettings::Instance().commentsFont.Get().name), m_ItemList.GetFont().size, false, false, true);

		m_ItemList.Invalidate(false);
	}
	const SampleHint sampleHint = hint.ToType<SampleHint>();
	const InstrumentHint instrHint = hint.ToType<InstrumentHint>();
	const bool updateSamples = sampleHint.GetType()[HINT_SMPNAMES | HINT_SAMPLEINFO];
	const bool updateInstr = instrHint.GetType()[HINT_INSNAMES|HINT_INSTRUMENT];
	bool updateAll = hintType[HINT_MODTYPE];

	if(!updateSamples && !updateInstr && !updateAll) return;

	const CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();

	m_ToolBar.SetButtonImage(IDC_LIST_INSTRUMENTS, sndFile.GetNumInstruments() ? IMAGE_INSTRUMENTS : IMAGE_INSTRMUTE);

	mpt::ustring s;

	m_ItemList.SetRedraw(false);
	// Add sample headers
	if (m_nListId != m_nCurrentListId || updateAll)
	{
		m_ItemList.DeleteAllItems();
		m_ItemList.DeleteAllColumns();
		m_nCurrentListId = m_nListId;
		if (m_nCurrentListId == IDC_LIST_SAMPLES)
		{
			// Add Sample Headers
			m_ItemList.SetHeaders(SampleHeaders);
		} else if (m_nCurrentListId == IDC_LIST_INSTRUMENTS)
		{
			// Add Instrument Headers
			m_ItemList.SetHeaders(InstrumentHeaders);
		} else
		updateAll = true;
	}
	// Add Items
	uint32 nCount = m_ItemList.GetItemCount();
	// Add Samples
	if (m_nCurrentListId == IDC_LIST_SAMPLES && (updateAll || updateSamples))
	{
		SAMPLEINDEX nMax = static_cast<SAMPLEINDEX>(nCount);
		if (nMax < sndFile.GetNumSamples()) nMax = sndFile.GetNumSamples();
		for (SAMPLEINDEX iSmp = 0; iSmp < nMax; iSmp++)
		{
			if (iSmp < sndFile.GetNumSamples())
			{
				uint32 nCol = 0;
				for (uint32 iCol=0; iCol<SMPLIST_COLUMNS; iCol++)
				{
					const ModSample &sample = sndFile.GetSample(iSmp + 1);
					s.clear();
					switch(iCol)
					{
					case SMPLIST_SAMPLENAME:
						s = mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.m_szNames[iSmp + 1]);
						break;
					case SMPLIST_SAMPLENO:
						s = mpt::ufmt::dec0<2>(iSmp + 1);
						break;
					case SMPLIST_SIZE:
						if(sample.nLength && !sample.uFlags[CHN_ADLIB])
							s = FormatFileSize(sample.GetSampleSizeInBytes());
						break;
					case SMPLIST_TYPE:
						if(sample.uFlags[CHN_ADLIB])
							s = UL_("OPL");
						else if(sample.HasSampleData())
							s = MPT_UFORMAT("{}-bit {}")(sample.GetElementarySampleSize() * 8, (sample.GetNumChannels() == 2) ? mpt::ustring(UL_("stereo")) : mpt::ustring(UL_("mono")));
						break;
					case SMPLIST_INSTR:
						if (sndFile.GetNumInstruments())
						{
							bool first = true;
							for (INSTRUMENTINDEX i = 1; i <= sndFile.GetNumInstruments(); i++)
							{
								if (sndFile.IsSampleReferencedByInstrument(iSmp + 1, i))
								{
									if (!first) s.push_back(UL_(','));
									first = false;

									s += ui::Format(UL_("%u"), i);
								}
							}
						}
						break;
					case SMPLIST_MIDDLEC:
						if (sample.nLength)
						{
							s = ui::Format(UL_("%u Hz"), sample.GetSampleRate(sndFile.GetType()));
						}
						break;
					case SMPLIST_FILENAME:
						s = mpt::ToUnicode(sndFile.GetCharsetInternal(), sample.filename);
						break;
					case SMPLIST_PATH:
						s = sndFile.GetSamplePath(iSmp + 1).ToUnicode();
						break;
					}
					if ((iCol) || (iSmp < nCount))
					{
						bool update = true;
						if (iSmp < nCount)
						{
							mpt::ustring stmp = m_ItemList.GetItemText(iSmp, nCol);
							if (s == stmp) update = false;
						}
						if (update) m_ItemList.SetItemText(iSmp, nCol, s);
					} else
					{
						m_ItemList.InsertItem(iSmp, s);
					}
					nCol++;
				}
			} else
			{
				m_ItemList.DeleteItem(iSmp);
			}
		}
	} else
	// Add Instruments
	if ((m_nCurrentListId == IDC_LIST_INSTRUMENTS) && (updateAll || updateInstr))
	{
		INSTRUMENTINDEX nMax = static_cast<INSTRUMENTINDEX>(nCount);
		if (nMax < sndFile.GetNumInstruments()) nMax = sndFile.GetNumInstruments();
		for (INSTRUMENTINDEX iIns = 0; iIns < nMax; iIns++)
		{
			if (iIns < sndFile.GetNumInstruments())
			{
				uint32 nCol = 0;
				for (uint32 iCol=0; iCol<INSLIST_COLUMNS; iCol++)
				{
					ModInstrument *pIns = sndFile.Instruments[iIns+1];
					s.clear();
					switch(iCol)
					{
					case INSLIST_INSTRUMENTNAME:
						if (pIns) s = mpt::ToUnicode(sndFile.GetCharsetInternal(), pIns->name);
						break;
					case INSLIST_INSTRUMENTNO:
						s = mpt::ufmt::dec0<2>(iIns + 1);
						break;
					case INSLIST_SAMPLES:
						if (pIns)
						{
							bool first = true;
							for(auto sample : pIns->GetSamples())
							{
								if(!first) s.push_back(UL_(','));
								first = false;
								s += ui::Format(UL_("%u"), sample);
							}
						}
						break;
					case INSLIST_ENVELOPES:
						if (pIns)
						{
							if (pIns->VolEnv.dwFlags[ENV_ENABLED]) s += UL_("Vol");
							if (pIns->PanEnv.dwFlags[ENV_ENABLED]) { if (!s.empty()) s += UL_(", "); s += UL_("Pan"); }
							if (pIns->PitchEnv.dwFlags[ENV_ENABLED]) { if (!s.empty()) s += UL_(", "); s += (pIns->PitchEnv.dwFlags[ENV_FILTER] ? UL_("Filter") : UL_("Pitch")); }
						}
						break;
					case INSLIST_FILENAME:
						if (pIns)
						{
							s = mpt::ToUnicode(sndFile.GetCharsetInternal(), pIns->filename);
						}
						break;
					case INSLIST_PLUGIN:
						if (pIns != nullptr && pIns->nMixPlug > 0 && sndFile.m_MixPlugins[pIns->nMixPlug - 1].IsValidPlugin())
						{
							s = ui::Format(UL_("FX%02u: "), pIns->nMixPlug);
							s += mpt::ToUnicode(sndFile.m_MixPlugins[pIns->nMixPlug - 1].GetLibraryName());
						}
						break;
					}
					if ((iCol) || (iIns < nCount))
					{
						bool update = true;
						if (iIns < nCount)
						{
							mpt::ustring stmp = m_ItemList.GetItemText(iIns, nCol);
							if (s == stmp) update = false;
						}
						if (update) m_ItemList.SetItemText(iIns, nCol, s);
					} else
					{
						m_ItemList.InsertItem(iIns, s);
					}
					nCol++;
				}
			} else
			{
				m_ItemList.DeleteItem(iIns);
			}
		}
	} else
	// Add Patterns
	//if ((m_nCurrentListId == IDC_LIST_PATTERNS) && (hintType & (HINT_MODTYPE|HINT_PATNAMES|HINT_PATTERNROW)))
	{
	}
	m_ItemList.SetRedraw(true);
}


void CViewComments::RecalcLayout()
{
	Rect rect;

	if(!IsWindow()) return;
	GetClientRect(&rect);
	MoveChild(m_ToolBar, Rect(0, 0, rect.Width(), DETAILS_TOOLBAR_CY));
	MoveChild(m_ItemList, Rect(-1, DETAILS_TOOLBAR_CY, rect.Width() + 1, rect.Height() + 1));
}


void CViewComments::UpdateButtonState()
{
	const CModDoc *pModDoc = GetDocument();
	if (pModDoc)
	{
		m_ToolBar.SetButtonState(IDC_LIST_SAMPLES, ((m_nListId == IDC_LIST_SAMPLES) ? ui::ToolStateChecked : 0)|ui::ToolStateEnabled);
		m_ToolBar.SetButtonState(IDC_LIST_INSTRUMENTS, ((m_nListId == IDC_LIST_INSTRUMENTS) ? ui::ToolStateChecked : 0)|ui::ToolStateEnabled);
		m_ToolBar.SetButtonState(IDC_LIST_PATTERNS, ((m_nListId == IDC_LIST_PATTERNS) ? ui::ToolStateChecked : 0)|ui::ToolStateEnabled);
		m_ToolBar.EnableButton(IDC_LIST_INSTRUMENTS, (pModDoc->GetNumInstruments()) ? true : false);
	}
}


void CViewComments::OnBeginLabelEdit(NotifyHeader *, LResult *)
{
	Edit *editCtrl = m_ItemList.GetEditControl();
	if(editCtrl)
	{
		m_editLabel = true;
		const CModSpecifications &specs = GetDocument()->GetSoundFile().GetModSpecifications();
		const auto maxStrLen = (m_nListId == IDC_LIST_SAMPLES) ? specs.sampleNameLengthMax : specs.instrNameLengthMax;
		editCtrl->LimitText(maxStrLen);
		CMainFrame::GetInputHandler()->Bypass(true);
	}
}


void CViewComments::OnEndLabelEdit(NotifyHeader * pnmhdr, LResult *result)
{
	CMainFrame::GetInputHandler()->Bypass(false);
	if(!m_editLabel)
	{
		*result = false;
		return;
	}
	m_editLabel = false;

	const auto *labelEdit = static_cast<const ui::ListLabelEdit *>(pnmhdr->extra);
	CModDoc *pModDoc = GetDocument();

	if(labelEdit->text != nullptr && !labelEdit->column && pModDoc)
	{
		const uint32 iItem = labelEdit->item;
		CTrackerSoundFile &sndFile = pModDoc->GetSoundFile();

		if(m_nListId == IDC_LIST_SAMPLES)
		{
			if(iItem < sndFile.GetNumSamples())
			{
				sndFile.m_szNames[iItem + 1] = mpt::ToCharset(sndFile.GetCharsetInternal(), *labelEdit->text);
				pModDoc->UpdateAllViews(this, SampleHint(static_cast<SAMPLEINDEX>(iItem + 1)).Info().Names(), this);
				pModDoc->SetModified();
			}
		} else if(m_nListId == IDC_LIST_INSTRUMENTS)
		{
			if((iItem < sndFile.GetNumInstruments()) && (sndFile.Instruments[iItem + 1]))
			{
				ModInstrument *pIns = sndFile.Instruments[iItem + 1];
				pIns->name = mpt::ToCharset(sndFile.GetCharsetInternal(), *labelEdit->text);
				pModDoc->UpdateAllViews(this, InstrumentHint(static_cast<INSTRUMENTINDEX>(iItem + 1)).Info().Names(), this);
				pModDoc->SetModified();
			}
		} else
		{
			return;
		}
		m_ItemList.SetItemText(iItem, labelEdit->column, *labelEdit->text);
	}
}


///////////////////////////////////////////////////////////////
// CViewComments messages


void CViewComments::OnSize(uint32 nType, int cx, int cy)
{
	CModScrollView::OnSize(nType, cx, cy);
	if (((nType == SIZE_RESTORED) || (nType == SIZE_MAXIMIZED)) && (cx > 0) && (cy > 0))
	{
		RecalcLayout();
	}
}


void CViewComments::OnShowSamples() { SwitchToList(IDC_LIST_SAMPLES); }
void CViewComments::OnShowInstruments() { SwitchToList(IDC_LIST_INSTRUMENTS); }
void CViewComments::OnShowPatterns() { SwitchToList(IDC_LIST_PATTERNS); }


bool CViewComments::SwitchToList(int list)
{
	if(list == m_nListId)
		return false;

	if(list == IDC_LIST_SAMPLES)
	{
		m_nListId = IDC_LIST_SAMPLES;
		UpdateButtonState();
		UpdateView(UpdateHint().ModType());
	} else if(list == IDC_LIST_INSTRUMENTS)
	{
		const CModDoc *modDoc = GetDocument();
		if(!modDoc || !modDoc->GetNumInstruments())
			return false;

		m_nListId = IDC_LIST_INSTRUMENTS;
		UpdateButtonState();
		UpdateView(UpdateHint().ModType());
	/*} else if(list == IDC_LIST_PATTERNS)
	{
		m_nListId = IDC_LIST_PATTERNS;
		UpdateButtonState();
		UpdateView(UpdateHint().ModType());*/
	} else
	{
		return false;
	}
	return true;
}


void CViewComments::OnDblClickListItem(NotifyHeader *, LResult *)
{
	// Double click -> switch to instrument or sample tab
	int nItem = m_ItemList.GetSelectionMark();
	if(nItem == -1) return;
	CModDoc *pModDoc = GetDocument();
	if(!pModDoc) return;
	nItem++;

	switch(m_nListId)
	{
	case IDC_LIST_SAMPLES:
		pModDoc->ViewSample(nItem);
		break;
	case IDC_LIST_INSTRUMENTS:
		pModDoc->ViewInstrument(nItem);
		break;
	case IDC_LIST_PATTERNS:
		pModDoc->ViewPattern(nItem, 0);
		break;
	}
}


void CViewComments::OnRClickListItem(NotifyHeader *, LResult *)
{
	const auto ih = CMainFrame::GetMainFrame()->GetInputHandler();
	Menu menu;
	menu.AppendMenu(ui::MenuItemString, ID_EDIT_COPY, ih->GetKeyTextFromCommand(kcEditCopy, UL_("&Copy Names")));
	menu.AppendMenu(ui::MenuItemString | (ui::IsClipboardFormatAvailable(ui::ClipboardUnicodeText) ? 0 : ui::MenuItemGrayed), ID_EDIT_PASTE, ih->GetKeyTextFromCommand(kcEditPaste, UL_("&Paste Names")));
	menu.TrackPopupMenu(ui::GetCursorPosition(), this);
}


void CViewComments::OnCopyNames()
{
	mpt::ustring names;
	const CTrackerSoundFile &sndFile = GetDocument()->GetSoundFile();
	if(m_nListId == IDC_LIST_SAMPLES)
	{
		for(SAMPLEINDEX i = 1; i <= sndFile.GetNumSamples(); i++)
			names += mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.GetSampleName(i)) + UL_("\r\n");
	} else if(m_nListId == IDC_LIST_INSTRUMENTS)
	{
		for(INSTRUMENTINDEX i = 1; i <= sndFile.GetNumInstruments(); i++)
			names += mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.GetInstrumentName(i)) + UL_("\r\n");
	}
	const std::u16string wideNames = mpt::transcode<std::u16string>(names);
	const size_t sizeBytes = (wideNames.length() + 1) * sizeof(char16_t);
	Clipboard clipboard(ui::ClipboardUnicodeText, sizeBytes);
	if(auto dst = clipboard.Get(); dst.data())
	{
		std::memcpy(dst.data(), wideNames.c_str(), sizeBytes);
	}
}


void CViewComments::OnPasteNames()
{
	Clipboard clipboard(ui::ClipboardUnicodeText);
	if(!clipboard.IsValid())
		return;

	if(Reporting::Confirm(MPT_UFORMAT("Replace all {} names?")(m_nListId == IDC_LIST_INSTRUMENTS ? UL_("instrument") : UL_("sample"))) != cnfYes)
		return;

	auto whitespace = mpt::default_whitespace<mpt::ustring>();
	whitespace.push_back(UC_('\0'));
	const auto names = mpt::split(mpt::trim_right(mpt::transcode<mpt::ustring>(std::u16string{clipboard.GetWideString()}), whitespace), mpt::ustring{UL_("\n")});

	CTrackerSoundFile &sndFile = GetDocument()->GetSoundFile();
	const auto FormatName = [&](size_t index, size_t maxLength)
	{
		if(index >= names.size())
			return std::string{};
		return mpt::replace(mpt::ToCharset(sndFile.GetCharsetInternal(), names[index]), std::string{"\t"}, std::string{" "}).substr(0, maxLength);
	};

	TrackerCriticalSection cs;
	if(m_nListId == IDC_LIST_SAMPLES)
	{
		if(sndFile.GetNumSamples() < names.size())
			sndFile.m_nSamples = std::min(sndFile.GetModSpecifications().samplesMax, mpt::saturate_cast<SAMPLEINDEX>(names.size()));

		for(SAMPLEINDEX i = 1; i <= sndFile.GetNumSamples(); i++)
		{
			sndFile.m_szNames[i] = FormatName(i - 1, sndFile.GetModSpecifications().sampleNameLengthMax);
		}
		cs.Leave();
		GetDocument()->UpdateAllViews(SampleHint().Names());
	} else if(m_nListId == IDC_LIST_INSTRUMENTS)
	{
		if(sndFile.GetNumInstruments() < names.size())
			sndFile.m_nInstruments = std::min(sndFile.GetModSpecifications().instrumentsMax, mpt::saturate_cast<INSTRUMENTINDEX>(names.size()));

		for(INSTRUMENTINDEX i = 1; i <= sndFile.GetNumInstruments(); i++)
		{
			if(sndFile.Instruments[i] || sndFile.AllocateInstrument(i))
				sndFile.Instruments[i]->name = FormatName(i - 1, sndFile.GetModSpecifications().instrNameLengthMax);
		}
		cs.Leave();
		GetDocument()->UpdateAllViews(InstrumentHint().Names());
	}

	GetDocument()->SetModified();
}


OPENMPT_NAMESPACE_END
