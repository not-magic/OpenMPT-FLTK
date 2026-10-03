/*
 * View_tre.cpp
 * ------------
 * Purpose: Tree view for managing open songs, sound files, file browser, ...
 * Notes  : There are two instances of this class, one for the upper half and one for the lower half of the tree view.
 *          The lower half is referred to the sample browser in the code, i.e. IsSampleBrowser() returns true for code
 *          running in the lower half.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "ui/Ui.h"
#include "View_tre.h"
#include "dlg_misc.h"
#include "Dlsbank.h"
#include "ExternalSamples.h"
#include "FileDialog.h"
#include "FolderScanner.h"
#include "Globals.h"
#include "ImageLists.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "MPTrackUtil.h"
#include "Reporting.h"
#include "View_ins.h"
#include "View_smp.h"
#include "resource.h"
#include "TrackerSettings.h"
#include "WindowMessages.h"
#include "../common/FileReader.h"
#include "../common/mptFileIO.h"
#include "../soundlib/MIDIEvents.h"
#include "../soundlib/mod_specifications.h"
#include "../soundlib/plugins/PlugInterface.h"
#include "mpt/fs/fs.hpp"
#include "mpt/io_file/inputfile.hpp"
#include "mpt/io_file_read/inputfile_filecursor.hpp"
#include "mpt/parse/parse.hpp"



OPENMPT_NAMESPACE_BEGIN


CSoundFile *CModTree::m_SongFile = nullptr;
CModTree::LibrarySortOrder CModTree::m_librarySort = LibrarySortOrder::Name;

ModTreeDocInfo::ModTreeDocInfo(CModDoc &modDoc)
    : modDoc(modDoc)
{
	const CSoundFile &sndFile = modDoc.GetSoundFile();
	tiPatterns.resize(sndFile.Patterns.Size(), nullptr);
	tiOrders.resize(sndFile.Order.GetNumSequences());
	tiSequences.resize(sndFile.Order.GetNumSequences(), nullptr);
}


/////////////////////////////////////////////////////////////////////////////
// CModTree

UI_MESSAGE_MAP_BEGIN(CModTree, TreeCtrl)
	UI_NOTIFY_REFLECT(ui::TreeDblClick,          &CModTree::OnItemDblClk)
	UI_NOTIFY_REFLECT(ui::TreeReturn,          &CModTree::OnItemReturn)
	UI_NOTIFY_REFLECT(ui::TreeRClick,          &CModTree::OnTreeRightClick)
	UI_NOTIFY_REFLECT(ui::TreeClick,           &CModTree::OnItemLeftClick)
	UI_NOTIFY_REFLECT(ui::TreeItemExpanded,   &CModTree::OnTreeItemExpanded)
	UI_NOTIFY_REFLECT(ui::TreeBeginDrag,      &CModTree::OnBeginLDrag)
	UI_NOTIFY_REFLECT(ui::TreeBeginRDrag,     &CModTree::OnBeginRDrag)
	UI_NOTIFY_REFLECT(ui::TreeBeginLabelEdit, &CModTree::OnBeginLabelEdit)
	UI_NOTIFY_REFLECT(ui::TreeEndLabelEdit,   &CModTree::OnEndLabelEdit)
	UI_NOTIFY_REFLECT(ui::TreeSelChanged,     &CModTree::OnSelChanged)

	UI_COMMAND(ID_MODTREE_REFRESH,           &CModTree::OnRefreshTree)
	UI_COMMAND(ID_MODTREE_EXECUTE,           &CModTree::OnExecuteItem)
	UI_COMMAND(ID_MODTREE_REMOVE,            &CModTree::OnDeleteTreeItem)
	UI_COMMAND(ID_MODTREE_PLAY,              &CModTree::OnPlayTreeItem)
	UI_COMMAND(ID_MODTREE_REFRESHINSTRLIB,   &CModTree::OnRefreshInstrLib)
	UI_COMMAND(ID_MODTREE_OPENITEM,          &CModTree::OnOpenTreeItem)
	UI_COMMAND(ID_MODTREE_MUTE,              &CModTree::OnMuteTreeItem)
	UI_COMMAND(ID_MODTREE_MUTE_ONLY_EFFECTS, &CModTree::OnMuteOnlyEffects)
	UI_COMMAND(ID_MODTREE_SOLO,              &CModTree::OnSoloTreeItem)
	UI_COMMAND(ID_MODTREE_UNMUTEALL,         &CModTree::OnUnmuteAllTreeItem)
	UI_COMMAND(ID_MODTREE_DUPLICATE,         &CModTree::OnDuplicateTreeItem)
	UI_COMMAND(ID_MODTREE_INSERT,            &CModTree::OnInsertTreeItem)
	UI_COMMAND(ID_MODTREE_SWITCHTO,          &CModTree::OnSwitchToTreeItem)
	UI_COMMAND(ID_MODTREE_CLOSE,             &CModTree::OnCloseItem)
	UI_COMMAND(ID_MODTREE_SETPATH,           &CModTree::OnSetItemPath)
	UI_COMMAND(ID_MODTREE_SAVEITEM,          &CModTree::OnSaveItem)
	UI_COMMAND(ID_MODTREE_SAVEALL,           &CModTree::OnSaveAll)
	UI_COMMAND(ID_MODTREE_RELOADITEM,        &CModTree::OnReloadItem)
	UI_COMMAND(ID_MODTREE_RELOADALL,         &CModTree::OnReloadAll)
	UI_COMMAND(ID_MODTREE_FINDMISSING,       &CModTree::OnFindMissing)
	UI_COMMAND(ID_MODTREE_RENAME,            &CModTree::OnRenameItem)
	UI_COMMAND(ID_ADD_SOUNDBANK,             &CModTree::OnAddDlsBank)
	UI_COMMAND(ID_IMPORT_MIDILIB,            &CModTree::OnImportMidiLib)
	UI_COMMAND(ID_EXPORT_MIDILIB,            &CModTree::OnExportMidiLib)
	UI_COMMAND(ID_SOUNDBANK_PROPERTIES,      &CModTree::OnSoundBankProperties)
	UI_COMMAND(ID_MODTREE_SHOWDIRS,          &CModTree::OnShowDirectories)
	UI_COMMAND(ID_MODTREE_SHOWALLFILES,      &CModTree::OnShowAllFiles)
	UI_COMMAND(ID_MODTREE_SOUNDFILESONLY,    &CModTree::OnShowSoundFiles)
	UI_COMMAND(ID_MODTREE_GOTO_INSDIR,       &CModTree::OnGotoInstrumentDir)
	UI_COMMAND(ID_MODTREE_GOTO_SMPDIR,       &CModTree::OnGotoSampleDir)
	UI_COMMAND(ID_OPEN_LIBRARY_FILTER,       &CModTree::OnOpenInstrumentLibraryFilter)
	UI_COMMAND(ID_MODTREE_SORT_BY_NAME,      &CModTree::OnSortByName)
	UI_COMMAND(ID_MODTREE_SORT_BY_DATE,      &CModTree::OnSortByDate)
	UI_COMMAND(ID_MODTREE_SORT_BY_SIZE,      &CModTree::OnSortBySize)
	
	UI_MESSAGE(MSG_MOD_KEYCOMMAND, &CModTree::OnCustomKeyMsg)
	UI_MESSAGE(MSG_MOD_MIDIMSG,    &CModTree::OnMidiMsg)
UI_MESSAGE_MAP_END()


/////////////////////////////////////////////////////////////////////////////
// CViewModTree construction/destruction

CModTree::CModTree(CModTree *pDataTree)
	: m_pDataTree(pDataTree)
{
	if(m_pDataTree != nullptr)
	{
		// Set up instrument library monitoring thread
		m_WatchDirThread = std::thread([this]() { MonitorInstrumentLibrary(); });
	}
	MemsetZero(m_tiMidi);
	MemsetZero(m_tiPerc);
}


CModTree::~CModTree()
{
	delete m_SongFile;
	m_SongFile = nullptr;

	if(m_pDataTree != nullptr)
	{
		{
			const std::lock_guard<std::mutex> lock(m_WatchDirMutex);
			m_isWatchDirKillRequested = true;
		}
		m_watchDirSignal.notify_all();
		m_WatchDirThread.join();
	}
}


void CModTree::Init()
{
	m_modExtensions = CSoundFile::GetSupportedExtensions(false);
	m_MediaFoundationExtensions = FileType(CSoundFile::GetMediaFoundationFileTypes()).GetExtensions();

	uint32 dwRemove = ui::TreeStyleSingleExpand;
	uint32 dwAdd = ui::TreeStyleEditLabels | ui::TreeStyleHasLines | ui::TreeStyleLinesAtRoot | ui::TreeStyleHasButtons | ui::TreeStyleShowSelectionAlways;

	if(IsSampleBrowser())
	{
		dwRemove |= (ui::TreeStyleHasLines | ui::TreeStyleLinesAtRoot | ui::TreeStyleHasButtons);
		dwAdd &= ~(ui::TreeStyleHasLines | ui::TreeStyleLinesAtRoot | ui::TreeStyleHasButtons);
	}
	if(TrackerSettings::Instance().patternSetup & PatternSetup::SingleClickToExpand)
	{
		dwRemove &= ~ui::TreeStyleSingleExpand;
		dwAdd |= ui::TreeStyleSingleExpand;
		m_dwStatus |= TREESTATUS_SINGLEEXPAND;
	}
	ModifyStyle(dwRemove, dwAdd);

	if(!IsSampleBrowser())
	{
		std::error_code currentPathError;
		const mpt::PathString curDir = mpt::PathString::FromUTF8(std::filesystem::current_path(currentPathError).string());
		mpt::PathString dirs[] =
		{
			TrackerSettings::Instance().PathSamples.GetDefaultDir(),
			TrackerSettings::Instance().PathInstruments.GetDefaultDir(),
			TrackerSettings::Instance().PathSongs.GetDefaultDir(),
			curDir
		};
		for(auto &path : dirs)
		{
			m_InstrLibPath = std::move(path);
			if(!m_InstrLibPath.empty())
				break;
		}
		m_InstrLibPath = m_InstrLibPath.WithTrailingSlash();
		m_pDataTree->InsLibSetFullPath(m_InstrLibPath, mpt::PathString());
	}

	SetImageList(&CMainFrame::GetMainFrame()->m_MiscIcons);
	if(!IsSampleBrowser())
	{
		// Create Midi Library
		m_hMidiLib = InsertItem(UL_("MIDI Library"), IMAGE_FOLDER, IMAGE_FOLDER, ui::TreeRoot, ui::TreeLast);
		for(uint32 iMidGrp = 0; iMidGrp < 17; iMidGrp++)
		{
			InsertItem(ui::TreeItemText | ui::TreeItemImage | ui::TreeItemSelectedImage | ui::TreeItemParam, mpt::ToUnicode(mpt::Charset::ASCII, szMidiGroupNames[iMidGrp]), IMAGE_FOLDER, IMAGE_FOLDER, 0, 0, (MODITEM_HDR_MIDIGROUP << MIDILIB_SHIFT) | iMidGrp, m_hMidiLib, ui::TreeLast);
		}
	}
	m_hInsLib = InsertItem(UL_("Instrument Library"), IMAGE_FOLDER, IMAGE_FOLDER, ui::TreeRoot, ui::TreeLast);
	RefreshMidiLibrary();
	RefreshDlsBanks();
	RefreshInstrumentLibrary();
}


void CModTree::OnDestroy()
{
	if(CMainFrame::GetMainFrame()->GetMidiRecordWnd() == this)
	{
		CMainFrame::GetMainFrame()->SetMidiRecordWnd(nullptr);
	}
}


void CModTree::OnDPIChanged()
{
	SetImageList(&CMainFrame::GetMainFrame()->m_MiscIcons);
	SetIndent(0);  // Set to minimum after going back from high-DPI to low-DPI
}


bool CModTree::PreTranslateMessage(int event)
{
	if(m_doLabelEdit)
	{
		if(event == FL_KEYBOARD && (ui::KeyFromEvent() == ui::Key_RETURN || ui::KeyFromEvent() == ui::Key_ESCAPE))
		{
			EndEditLabel(ui::KeyFromEvent() == ui::Key_ESCAPE);
			return true;
		}
		return TreeCtrl::PreTranslateMessage(event);
	}

	if(event == FL_KEYBOARD)
	{
		if(ui::KeyFromEvent() == ui::Key_ESCAPE)
			GetParent()->PostCommand(ID_CLOSE_LIBRARY_FILTER);

		const ModItem item = GetModItem(GetSelectedItem());
		switch(item.type)
		{
		case MODITEM_SAMPLE:
		case MODITEM_INSTRUMENT:
		case MODITEM_MIDIINSTRUMENT:
		case MODITEM_MIDIPERCUSSION:
		case MODITEM_INSLIB_SAMPLE:
		case MODITEM_INSLIB_INSTRUMENT:
		case MODITEM_DLSBANK_INSTRUMENT:
			// Avoid cycling through tree-view elements on key hold
			if(ui::IsKeyRepeat() && !CMainFrame::GetInputHandler()->IsBypassed())
				return true;
			break;
		default:
			break;
		}
	}

	if(event == FL_KEYBOARD || event == FL_KEYUP)
	{
		CInputHandler *ih = CMainFrame::GetInputHandler();
		const auto keyEvent = ih->Translate(ui::KeyFromEvent(), 1, (event == FL_KEYUP) ? ui::KeyFlagRelease : (ui::IsKeyRepeat() ? ui::KeyFlagRepeat : 0));
		if(ih->KeyEvent(kCtxViewTree, keyEvent, this) != kcNull)
			return true;  // Mapped to a command, no need to pass message on.

		// For context menu shortcut
		if(ih->KeyEvent(kCtxAllContexts, keyEvent, this) != kcNull)
			return true;  // Mapped to a command, no need to pass message on.
	}
	return TreeCtrl::PreTranslateMessage(event);
}


mpt::PathString CModTree::InsLibGetFullPath(TreeItemHandle hItem) const
{
	mpt::PathString fullPath = m_InstrLibPath;
	fullPath = fullPath.WithTrailingSlash();
	return fullPath + mpt::PathString::FromUnicode(GetItemText(hItem));
}


bool CModTree::InsLibSetFullPath(const mpt::PathString &libPath, const mpt::PathString &songName)
{
	if(!songName.empty() && mpt::PathCompareNoCase(m_SongFileName, songName))
	{
		// Load module for previewing its instruments
		mpt::IO::InputFile f(libPath + songName, TrackerSettings::Instance().MiscCacheCompleteFileBeforeLoading);
		if(f.IsValid())
		{
			FileReader file = GetFileReader(f);
			if(file.IsValid())
			{
				if(m_SongFile != nullptr)
				{
					m_SongFile->Destroy();
				} else
				{
					m_SongFile = new(std::nothrow) CSoundFile;
				}
				if(m_SongFile != nullptr)
				{
					try
					{
						if(!m_SongFile->Create(file, CSoundFile::loadNoPatternOrPluginData, nullptr))
						{
							return false;
						}
					} catch(mpt::out_of_memory e)
					{
						mpt::delete_out_of_memory(e);
						return false;
					} catch(const std::exception &)
					{
						return false;
					}
					// Destroy some stuff that we're not going to use anyway.
					m_SongFile->Patterns.DestroyPatterns();
					m_SongFile->m_songMessage.clear();
				}
			}
		} else
		{
			return false;
		}
	}
	m_InstrLibPath = libPath;
	m_SongFileName = songName;
	return true;
}


bool CModTree::SetSoundFile(FileReader &file)
{
	std::unique_ptr<CSoundFile> sndFile;
	try
	{
		sndFile = std::make_unique<CSoundFile>();
		if(!sndFile->Create(file, CSoundFile::loadNoPatternOrPluginData))
		{
			return false;
		}
	} catch(mpt::out_of_memory e)
	{
		mpt::delete_out_of_memory(e);
		return false;
	} catch(const std::exception &)
	{
		return false;
	}

	if(m_SongFile != nullptr)
	{
		m_SongFile->Destroy();
		delete m_SongFile;
	}
	m_SongFile = sndFile.release();
	m_SongFile->Patterns.DestroyPatterns();
	m_SongFile->m_songMessage.clear();
	const mpt::PathString fileName = file.GetOptionalFileName().value_or(P_(""));
	m_InstrLibPath = fileName.GetDirectoryWithDrive();
	m_SongFileName = fileName.GetFilename();
	RefreshInstrumentLibrary();
	return true;
}


CModTree* CModTree::GetOtherView()
{
	if(this == CMainFrame::GetMainFrame()->GetUpperTreeview())
		return CMainFrame::GetMainFrame()->GetLowerTreeview();
	else
		return CMainFrame::GetMainFrame()->GetUpperTreeview();
}

void CModTree::OnOptionsChanged()
{
	uint32 dwRemove = ui::TreeStyleSingleExpand, dwAdd = 0;
	m_dwStatus &= ~TREESTATUS_SINGLEEXPAND;
	if(TrackerSettings::Instance().patternSetup & PatternSetup::SingleClickToExpand)
	{
		dwRemove = 0;
		dwAdd = ui::TreeStyleSingleExpand;
		m_dwStatus |= TREESTATUS_SINGLEEXPAND;
	}
	ModifyStyle(dwRemove, dwAdd);
}


void CModTree::AddDocument(CModDoc &modDoc)
{
	const auto [it, inserted] = m_docInfo.try_emplace(&modDoc, modDoc);
	if(!inserted)
		return;

	auto &info = it->second;
	UpdateView(info, UpdateHint().ModType());
	if(info.hSong)
	{
		Expand(info.hSong, ui::TreeExpand);
		EnsureVisible(info.hSong);
		SelectItem(info.hSong);
	}
}


void CModTree::RemoveDocument(const CModDoc &modDoc)
{
	auto doc = m_docInfo.find(&modDoc);
	if(doc == m_docInfo.end())
		return;

	DeleteItem(doc->second.hSong);
	m_docInfo.erase(doc);
}


// Get CModDoc that is associated with a tree item
ModTreeDocInfo *CModTree::GetDocumentInfoFromItem(TreeItemHandle hItem)
{
	hItem = GetParentRootItem(hItem);
	if(hItem != nullptr)
	{
		// Root item has moddoc pointer
		const auto doc = m_docInfo.find(reinterpret_cast<const CModDoc *>(GetItemData(hItem)));
		if(doc != m_docInfo.end() && hItem == doc->second.hSong)
		{
			return &doc->second;
		}
	}
	return nullptr;
}


// Get modtree doc information for a given CModDoc
ModTreeDocInfo *CModTree::GetDocumentInfoFromModDoc(CModDoc &modDoc)
{
	auto doc = m_docInfo.find(&modDoc);
	if(doc != m_docInfo.end())
		return &doc->second;
	else
		return nullptr;
}


size_t CModTree::GetDLSBankIndexFromItem(TreeItemHandle hItem) const
{
	return static_cast<size_t>(std::distance(m_tiDLS.begin(), std::find(m_tiDLS.begin(), m_tiDLS.end(), GetParentRootItem(hItem))));
}


CDLSBank *CModTree::GetDLSBankFromItem(TreeItemHandle hItem) const
{
	const auto bank = GetDLSBankIndexFromItem(hItem);
	if(bank < CTrackApp::gpDLSBanks.size())
		return CTrackApp::gpDLSBanks[bank].get();
	else
		return nullptr;
}


/////////////////////////////////////////////////////////////////////////////
// CViewModTree drawing

void CModTree::RefreshMidiLibrary()
{
	mpt::ustring s;
	ui::TreeItemInfo tvi;
	const MidiLibrary &midiLib = CTrackApp::GetMidiLibrary();

	if(IsSampleBrowser())
		return;
	// Midi Programs
	TreeItemHandle parent = GetChildItem(m_hMidiLib);
	for(uint32 iMidi = 0; iMidi < 128; iMidi++)
	{
		int image = IMAGE_INSTRMUTE;
		s = mpt::ufmt::val(iMidi) + UL_(": ") + mpt::ToUnicode(mpt::Charset::ASCII, szMidiProgramNames[iMidi]);
		const LParam param = (MODITEM_MIDIINSTRUMENT << MIDILIB_SHIFT) | iMidi;
		if(midiLib[iMidi] && !midiLib[iMidi]->empty())
		{
			s += UL_(": ") + midiLib[iMidi]->GetFilename().ToUnicode();
			image = IMAGE_INSTRUMENTS;
		}
		if(!m_tiMidi[iMidi])
		{
			m_tiMidi[iMidi] = InsertItem(ui::TreeItemText | ui::TreeItemImage | ui::TreeItemSelectedImage | ui::TreeItemParam,
							s, image, image, 0, 0, param, parent, ui::TreeLast);
		} else
		{
			tvi.mask = ui::TreeItemText | ui::TreeItemImage | ui::TreeItemSelectedImage | ui::TreeItemParam;
			tvi.item = m_tiMidi[iMidi];
			tvi.image = tvi.selectedImage = image;
			GetItem(&tvi);
			if(tvi.image != image || s != tvi.text)
			{
				SetItem(m_tiMidi[iMidi], ui::TreeItemText | ui::TreeItemImage | ui::TreeItemSelectedImage | ui::TreeItemParam,
					s, image, image, 0, 0, param);
			}
		}
		if((iMidi % 8u) == 7u)
		{
			parent = GetNextSiblingItem(parent);
		}
	}
	// Midi Percussions
	for(uint32 iPerc = 24; iPerc <= 84; iPerc++)
	{
		int image = IMAGE_NOSAMPLE;
		s = mpt::ToUnicode(CSoundFile::GetNoteName((ModCommand::NOTE)(iPerc + NOTE_MIN), CSoundFile::GetDefaultNoteNames()))
		    + UL_(": ") + mpt::ToUnicode(mpt::Charset::ASCII, szMidiPercussionNames[iPerc - 24]);
		const LParam param = (MODITEM_MIDIPERCUSSION << MIDILIB_SHIFT) | iPerc;
		if(midiLib[iPerc | 0x80] && !midiLib[iPerc | 0x80]->empty())
		{
			s += UL_(": ") + midiLib[iPerc | 0x80]->GetFilename().ToUnicode();
			image = IMAGE_SAMPLES;
		}
		if(!m_tiPerc[iPerc])
		{
			m_tiPerc[iPerc] = InsertItem(ui::TreeItemText | ui::TreeItemImage | ui::TreeItemSelectedImage | ui::TreeItemParam,
							s, image, image, 0, 0, param, parent, ui::TreeLast);
		} else
		{
			tvi.mask = ui::TreeItemText | ui::TreeItemImage | ui::TreeItemSelectedImage | ui::TreeItemParam;
			tvi.item = m_tiPerc[iPerc];
			tvi.image = tvi.selectedImage = image;
			GetItem(&tvi);
			if(tvi.image != image || s != tvi.text)
			{
				SetItem(m_tiPerc[iPerc], ui::TreeItemText | ui::TreeItemImage | ui::TreeItemSelectedImage,
							s, image, image, 0, 0, param);
			}
		}
	}
}


void CModTree::RefreshDlsBanks()
{
	const mpt::Charset charset = mpt::Charset::Locale;

	if(IsSampleBrowser())
		return;

	if(m_tiDLS.size() < CTrackApp::gpDLSBanks.size())
		m_tiDLS.resize(CTrackApp::gpDLSBanks.size(), nullptr);

	LockRedraw();
	TreeItemHandle hInsertAfter = m_hMidiLib;
	for(size_t iDls = 0; iDls < CTrackApp::gpDLSBanks.size(); iDls++)
	{
		if(!CTrackApp::gpDLSBanks[iDls])
		{
			// Was this bank removed?
			if(m_tiDLS[iDls])
			{
				DeleteItem(m_tiDLS[iDls]);
				m_tiDLS[iDls] = nullptr;
			}
			continue;
		}

		if(m_tiDLS[iDls] != nullptr)
		{
			hInsertAfter = m_tiDLS[iDls];
			continue;
		}

		// Add DLS file folder
		const CDLSBank &dlsBank = *CTrackApp::gpDLSBanks[iDls];
		if(m_tiDLS[iDls])
			DeleteChildren(m_tiDLS[iDls]);
		else
			m_tiDLS[iDls] = InsertItem(ui::TreeItemText | ui::TreeItemImage | ui::TreeItemSelectedImage | ui::TreeItemParam,
				dlsBank.GetFileName().GetFilename().ToUnicode(), IMAGE_FOLDER, IMAGE_FOLDER, 0, 0, iDls, ui::TreeRoot, hInsertAfter);

		std::map<uint16, TreeItemHandle> banks;
		TreeItemHandle hDrums = nullptr;

		// Add Instruments (done backwards to improve performance as per https://devblogs.microsoft.com/oldnewthing/20111125-00/?p=9033)
		MPT_ASSERT(dlsBank.GetNumInstruments() <= 0x10000);
		for(auto iIns = dlsBank.GetNumInstruments(); iIns > 0;)
		{
			iIns--;
			const DLSINSTRUMENT *pDlsIns = dlsBank.GetInstrument(iIns);
			if(!pDlsIns)
				continue;

			mpt::uchar szName[256];
			const LParam lParamInstr = DlsItem::ToLPARAM(static_cast<uint16>(iIns), (pDlsIns->ulInstrument & 0x7F), false);
			if(pDlsIns->ulBank & F_INSTRUMENT_DRUMS)
			{
				// Drum Kit
				TreeItemHandle hKit = nullptr;
				MPT_ASSERT(pDlsIns->Regions.size() <= 0x8000);
				for(auto region = static_cast<uint32>(pDlsIns->Regions.size()); region > 0;)
				{
					region--;
					if(pDlsIns->Regions[region].IsDummy())
						continue;

					uint32 keymin = pDlsIns->Regions[region].uKeyMin;
					uint32 keymax = pDlsIns->Regions[region].uKeyMax;

					const char *regionName = dlsBank.GetRegionName(iIns, region);
					if(regionName == nullptr || !regionName[0])
					{
						if(keymin >= 24 && keymin <= 84)
							regionName = szMidiPercussionNames[keymin - 24];
						else
							regionName = "";
					}

					if(!hKit)
					{
						if(!hDrums)
							hDrums = InsertItem(ui::TreeItemText | ui::TreeItemImage | ui::TreeItemSelectedImage | ui::TreeItemParam, UL_("Drum Kits"), IMAGE_FOLDER, IMAGE_FOLDER, 0, 0, DLS_DRUM_FOLDER_LPARAM, m_tiDLS[iDls], ui::TreeLast);

						wsprintf(szName, UL_("%u: %s"), pDlsIns->ulInstrument & 0x7F, mpt::ToUnicode(charset, pDlsIns->szName).c_str());
						hKit = InsertItem(ui::TreeItemText | ui::TreeItemImage | ui::TreeItemSelectedImage | ui::TreeItemParam, szName, IMAGE_FOLDER, IMAGE_FOLDER, 0, 0, lParamInstr, hDrums, ui::TreeFirst);
					}

					const auto regionNameStr = mpt::ToUnicode(charset, regionName);
					if(keymin >= keymax)
					{
						wsprintf(szName, UL_("%s%u: %s"),
							mpt::ToUnicode(CSoundFile::GetDefaultNoteName(keymin % 12)).c_str(),
							keymin / 12,
							regionNameStr.c_str());
					} else
					{
						wsprintf(szName, UL_("%s%u-%s%u: %s"),
							mpt::ToUnicode(CSoundFile::GetDefaultNoteName(keymin % 12)).c_str(),
							keymin / 12,
							mpt::ToUnicode(CSoundFile::GetDefaultNoteName(keymax % 12)).c_str(),
							keymax / 12,
							regionNameStr.c_str());
					}

					LParam lParam = DlsItem::ToLPARAM(static_cast<uint16>(iIns), static_cast<uint16>(region), true);
					InsertItem(ui::TreeItemText | ui::TreeItemImage | ui::TreeItemSelectedImage | ui::TreeItemParam,
							szName, IMAGE_INSTRUMENTS, IMAGE_INSTRUMENTS, 0, 0, lParam, hKit, ui::TreeFirst);
				}
			} else
			{
				// Melodic
				uint16 mbank = (pDlsIns->ulBank & 0x7F7F);
				auto hbank = banks.find(mbank);
				if(hbank == banks.end())
				{
					wsprintf(szName, mbank ? UL_("Melodic Bank %02d.%02d") : UL_("Melodic"), mbank >> 8, mbank & 0x7F);
					hbank = banks.insert(std::make_pair(mbank, nullptr)).first;
					hbank->second = InsertItem(ui::TreeItemText | ui::TreeItemImage | ui::TreeItemSelectedImage,
						szName, IMAGE_FOLDER, IMAGE_FOLDER, 0, 0, 0,
						m_tiDLS[iDls], ui::TreeFirst);
				}

				const auto instrName = mpt::ToUnicode(charset, pDlsIns->szName);
				wsprintf(szName, UL_("%u: %s"), pDlsIns->ulInstrument & 0x7F, instrName.c_str());
				InsertItem(ui::TreeItemText | ui::TreeItemImage | ui::TreeItemSelectedImage | ui::TreeItemParam,
					szName, IMAGE_INSTRUMENTS, IMAGE_INSTRUMENTS, 0, 0, lParamInstr, hbank->second, ui::TreeFirst);
			}
		}
		hInsertAfter = m_tiDLS[iDls];
	}
	UnlockRedraw();
}


void CModTree::RefreshInstrumentLibrary()
{
	LockRedraw();
	// Check if the currently selected item should be selected after refreshing
	mpt::ustring selectedName;
	if((IsSampleBrowser() || GetParentRootItem(GetSelectedItem()) == m_hInsLib)
	   && GetItemText(GetSampleBrowser()->m_hInsLib) == (m_SongFileName.empty() ? m_InstrLibPath : m_SongFileName).ToUnicode())
	{
		selectedName = GetItemText(GetSelectedItem());
	}
	if(!m_InstrLibHighlightPath.empty())
	{
		selectedName = m_InstrLibHighlightPath.ToUnicode();
	}
	m_InstrLibHighlightPath = {};
	FillInstrumentLibrary(selectedName);
	auto selectedItem = GetSelectedItem();
	if(selectedItem)
		EnsureVisible(selectedItem);
	UnlockRedraw();
	if(!IsSampleBrowser())
	{
		m_pDataTree->InsLibSetFullPath(m_InstrLibPath, m_SongFileName);
		m_pDataTree->RefreshInstrumentLibrary();
	}
}


void CModTree::UpdateView(ModTreeDocInfo &info, UpdateHint hint)
{
	ui::TreeItemInfo tvi;
	const FlagSet<HintType> hintType = hint.GetType();
	if(IsSampleBrowser() || hintType == HINT_NONE)
		return;

	const CModDoc &modDoc = info.modDoc;
	const CSoundFile &sndFile = modDoc.GetSoundFile();

	// Create headers
	const GeneralHint generalHint = hint.ToType<GeneralHint>();
	if(generalHint.GetType()[HINT_MODTYPE | HINT_MODGENERAL] || (!info.hSong))
	{
		// Module folder + sub folders
		mpt::ustring name = modDoc.GetPathNameMpt().GetFilename().ToUnicode();
		if(name.empty())
			name = mpt::SanitizePathComponent(modDoc.GetTitle());

		if(!info.hSong)
		{
			info.hSong = InsertItem(ui::TreeItemText | ui::TreeItemImage | ui::TreeItemSelectedImage | ui::TreeItemParam, name, IMAGE_FOLDER, IMAGE_FOLDER, 0, 0, reinterpret_cast<LParam>(&info.modDoc), ui::TreeRoot, ui::TreeFirst);
			info.hOrders = InsertItem(UL_("Sequence"), IMAGE_FOLDER, IMAGE_FOLDER, info.hSong, ui::TreeLast);
			info.hPatterns = InsertItem(UL_("Patterns"), IMAGE_FOLDER, IMAGE_FOLDER, info.hSong, ui::TreeLast);
			info.hSamples = InsertItem(UL_("Samples"), IMAGE_FOLDER, IMAGE_FOLDER, info.hSong, ui::TreeLast);
		} else if(generalHint.GetType()[HINT_MODGENERAL | HINT_MODTYPE])
		{
			if(name != GetItemText(info.hSong))
			{
				SetItemText(info.hSong, name);
			}
		}
	}

	if(sndFile.GetModSpecifications().instrumentsMax > 0)
	{
		if(!info.hInstruments)
			info.hInstruments = InsertItem(UL_("Instruments"), IMAGE_FOLDER, IMAGE_FOLDER, info.hSong, info.hSamples);
	} else
	{
		if(info.hInstruments)
		{
			DeleteItem(info.hInstruments);
			info.hInstruments = NULL;
		}
	}
	if(!info.hComments)
		info.hComments = InsertItem(UL_("Comments"), IMAGE_COMMENTS, IMAGE_COMMENTS, info.hSong, ui::TreeLast);
	// Add effects
	const PluginHint pluginHint = hint.ToType<PluginHint>();
	if(pluginHint.GetType()[HINT_MODTYPE | HINT_PLUGINNAMES])
	{
		TreeItemHandle hItem = info.hEffects ? GetChildItem(info.hEffects) : nullptr;
		PLUGINDEX firstPlug = 0, lastPlug = MAX_MIXPLUGINS - 1;
		if(pluginHint.GetPlugin() && hItem)
		{
			// Only update one specific plugin name
			firstPlug = lastPlug = pluginHint.GetPlugin() - 1;
			while(hItem && GetItemData(hItem) != firstPlug)
			{
				hItem = GetNextSiblingItem(hItem);
			}
		}
		for(PLUGINDEX i = firstPlug; i <= lastPlug; i++)
		{
			const SNDMIXPLUGIN &plugin = sndFile.m_MixPlugins[i];
			if(plugin.IsValidPlugin())
			{
				// Now we can be sure that we want to create this folder.
				if(!info.hEffects)
				{
					info.hEffects = InsertItem(UL_("Plugins"), IMAGE_FOLDER, IMAGE_FOLDER, info.hSong, info.hInstruments ? info.hInstruments : info.hSamples);
				}

				mpt::ustring s = MPT_UFORMAT("FX{}: {}")(i + 1, mpt::ToUnicode(plugin.GetName()));
				int nImage = IMAGE_NOPLUGIN;
				if(plugin.pMixPlugin != nullptr)
					nImage = (plugin.pMixPlugin->IsInstrument()) ? IMAGE_PLUGININSTRUMENT : IMAGE_EFFECTPLUGIN;

				if(hItem)
				{
					// Replace existing item
					tvi.mask = ui::TreeItemText | TreeItemHandleMask | ui::TreeItemImage | ui::TreeItemSelectedImage | ui::TreeItemParam;
					tvi.item = hItem;
					GetItem(&tvi);
					if(tvi.image != nImage || tvi.param != i || s != tvi.text)
					{
						SetItem(hItem, ui::TreeItemText | TreeItemHandleMask | ui::TreeItemImage | ui::TreeItemSelectedImage | ui::TreeItemParam, s, nImage, nImage, 0, 0, i);
					}
					hItem = GetNextSiblingItem(hItem);
				} else
				{
					InsertItem(ui::TreeItemText | TreeItemHandleMask | ui::TreeItemImage | ui::TreeItemSelectedImage | ui::TreeItemParam, s, nImage, nImage, 0, 0, i, info.hEffects, ui::TreeLast);
				}
			}
		}
		if(!sndFile.m_loadedPlugins && info.hEffects)
		{
			DeleteItem(info.hEffects);
			info.hEffects = nullptr;
		} else if(!pluginHint.GetPlugin())
		{
			// Delete superfluous tree items
			while(hItem)
			{
				TreeItemHandle nextItem = GetNextSiblingItem(hItem);
				DeleteItem(hItem);
				hItem = nextItem;
			}
		}
	}
	// Add Orders
	const PatternHint patternHint = hint.ToType<PatternHint>();
	const SequenceHint seqHint = hint.ToType<SequenceHint>();
	if(info.hOrders && (seqHint.GetType()[HINT_MODTYPE | HINT_MODSEQUENCE | HINT_SEQNAMES] || patternHint.GetType()[HINT_PATNAMES]))
	{
		const PATTERNINDEX nPat = patternHint.GetPattern();
		bool adjustParentNode = false;  // adjust sequence name of "Sequence" node?

		// (only one seq remaining || previously only one sequence): update parent item
		if((info.tiSequences.size() > 1 && sndFile.Order.GetNumSequences() == 1) || (info.tiSequences.size() == 1 && sndFile.Order.GetNumSequences() > 1))
		{
			for(auto &seq : info.tiOrders)
			{
				for(auto &ord : seq)
				{
					if(ord)
						DeleteItem(ord);
					ord = nullptr;
				}
			}
			for(auto &seq : info.tiSequences)
			{
				if(seq)
					DeleteItem(seq);
				seq = nullptr;
			}
			info.tiOrders.resize(sndFile.Order.GetNumSequences());
			info.tiSequences.resize(sndFile.Order.GetNumSequences(), nullptr);
			adjustParentNode = true;
		}

		// If there are too many sequences, delete them.
		for(size_t seq = sndFile.Order.GetNumSequences(); seq < info.tiSequences.size(); seq++) if (info.tiSequences[seq])
		{
			for(auto &ord : info.tiOrders[seq]) if (ord)
			{
				DeleteItem(ord); ord = nullptr;
			}
			DeleteItem(info.tiSequences[seq]); info.tiSequences[seq] = nullptr;
		}
		if (info.tiSequences.size() < sndFile.Order.GetNumSequences()) // Resize tiSequences if needed.
		{
			info.tiSequences.resize(sndFile.Order.GetNumSequences(), nullptr);
			info.tiOrders.resize(sndFile.Order.GetNumSequences());
		}

		TreeItemHandle hAncestorNode = info.hOrders;

		SEQUENCEINDEX nSeqMin = 0, nSeqMax = sndFile.Order.GetNumSequences() - 1;
		SEQUENCEINDEX nHintParam = seqHint.GetSequence();
		if(seqHint.GetType()[HINT_SEQNAMES] && (nHintParam <= nSeqMax))
			nSeqMin = nSeqMax = nHintParam;

		// Adjust caption of the "Sequence" node (if only one sequence exists, it should be labeled with the sequence name)
		if((seqHint.GetType()[HINT_SEQNAMES] && sndFile.Order.GetNumSequences() == 1) || adjustParentNode)
		{
			mpt::ustring seqName = mpt::ToUnicode(sndFile.Order(0).GetName());
			if(seqName.empty() || sndFile.Order.GetNumSequences() > 1)
				seqName = UL_("Sequence");
			else
				seqName = UL_("Sequence: ") + seqName;
			SetItem(info.hOrders, ui::TreeItemText, seqName, 0, 0, 0, 0, 0);
		}

		// go through all sequences
		mpt::ustring seqName;
		for(SEQUENCEINDEX seq = nSeqMin; seq <= nSeqMax; seq++)
		{
			if(sndFile.Order.GetNumSequences() > 1)
			{
				// more than one sequence -> add folder
				if(sndFile.Order(seq).GetName().empty())
				{
					seqName = MPT_UFORMAT("Sequence {}")(seq + 1);
				} else
				{
					seqName = MPT_UFORMAT("{}: ")(seq + 1);
					seqName += mpt::ToUnicode(sndFile.Order(seq).GetName());
				}

				uint32 state = (seq == sndFile.Order.GetCurrentSequenceIndex()) ? ui::TreeStateBold : 0;

				if(info.tiSequences[seq] == NULL)
				{
					info.tiSequences[seq] = InsertItem(seqName, IMAGE_FOLDER, IMAGE_FOLDER, info.hOrders, ui::TreeLast);
				}
				// Update bold item
				tvi.mask = ui::TreeItemText | TreeItemHandleMask | ui::TreeItemState | ui::TreeItemParam;
				tvi.state = 0;
				tvi.stateMask = ui::TreeStateBold;
				tvi.item = info.tiSequences[seq];
				LParam param = (seq << SEQU_SHIFT) | ORDERINDEX_INVALID;
				GetItem(&tvi);
				if(tvi.state != state || tvi.text != seqName || tvi.param != param)
					SetItem(info.tiSequences[seq], ui::TreeItemText | ui::TreeItemState | ui::TreeItemParam, seqName, 0, 0, state, ui::TreeStateBold, param);

				hAncestorNode = info.tiSequences[seq];
			}

			const ORDERINDEX ordLength = sndFile.Order(seq).GetLengthTailTrimmed();
			// If there are items past the new sequence length, delete them.
			for(size_t nOrd = ordLength; nOrd < info.tiOrders[seq].size(); nOrd++) if (info.tiOrders[seq][nOrd])
			{
				DeleteItem(info.tiOrders[seq][nOrd]); info.tiOrders[seq][nOrd] = NULL;
			}
			if (info.tiOrders[seq].size() < ordLength) // Resize tiOrders if needed.
				info.tiOrders[seq].resize(ordLength, nullptr);
			const bool patNamesOnly = patternHint.GetType()[HINT_PATNAMES];

			//if (hintFlagPart == HINT_PATNAMES) && (dwHintParam < sndFile.Order().length())) imin = imax = dwHintParam;
			mpt::ustring patName;
			for(ORDERINDEX iOrd = 0; iOrd < ordLength; iOrd++)
			{
				mpt::uchar s[256];
				s[0] = 0;
				if(patNamesOnly && sndFile.Order(seq)[iOrd] != nPat)
					continue;
				uint32 state = (iOrd == info.ordSel && seq == info.seqSel) ? ui::TreeStateBold : 0;
				if(sndFile.Order(seq)[iOrd] < sndFile.Patterns.Size())
				{
					patName = mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.Patterns[sndFile.Order(seq)[iOrd]].GetName());
					const bool hexOrders = (TrackerSettings::Instance().patternSetup & PatternSetup::RowAndOrderNumbersHex);
					if(!patName.empty())
					{
						wsprintf(s, hexOrders ? UL_("[%02Xh] %u: ") : UL_("[%02u] %u: "),
							iOrd, sndFile.Order(seq)[iOrd]);
						_tcscat(s, patName.c_str());
					} else
					{
						wsprintf(s, hexOrders ? UL_("[%02Xh] Pattern %u") : UL_("[%02u] Pattern %u"),
							iOrd, sndFile.Order(seq)[iOrd]);
					}
				} else
				{
					if(sndFile.Order(seq)[iOrd] == PATTERNINDEX_SKIP)
					{
						// +++ Item
						wsprintf(s, UL_("[%02u] Skip"), iOrd);
					} else
					{
						// --- Item
						wsprintf(s, UL_("[%02u] Stop"), iOrd);
					}
				}

				LParam param = (seq << SEQU_SHIFT) | iOrd;
				if(info.tiOrders[seq][iOrd])
				{
					tvi.mask = ui::TreeItemText | TreeItemHandleMask | ui::TreeItemState;
					tvi.state = 0;
					tvi.stateMask = ui::TreeStateBold;
					tvi.item = info.tiOrders[seq][iOrd];
					GetItem(&tvi);
					if(tvi.state != state || s != tvi.text)
						SetItem(info.tiOrders[seq][iOrd], ui::TreeItemText | ui::TreeItemState | ui::TreeItemParam, s, 0, 0, state, ui::TreeStateBold, param);
				} else
				{
					info.tiOrders[seq][iOrd] = InsertItem(TreeItemHandleMask | ui::TreeItemText | ui::TreeItemImage | ui::TreeItemSelectedImage | ui::TreeItemParam, s, IMAGE_PARTITION, IMAGE_PARTITION, 0, 0, param, hAncestorNode, ui::TreeLast);
				}
			}
		}
	}
	// Add Patterns
	if(info.hPatterns && patternHint.GetType()[HINT_MODTYPE | HINT_PATNAMES])
	{
		const PATTERNINDEX nPat = patternHint.GetPattern();
		PATTERNINDEX minPat = 0, maxPat = sndFile.Patterns.Size();
		if(patternHint.GetType()[HINT_PATNAMES] && nPat < sndFile.Patterns.Size())
		{
			minPat = nPat;
			maxPat = nPat + 1;
		}

		for(size_t pat = sndFile.Patterns.Size(); pat < info.tiPatterns.size(); pat++)
		{
			if(info.tiPatterns[pat])
				DeleteItem(info.tiPatterns[pat]);
		}
		info.tiPatterns.resize(sndFile.Patterns.Size(), nullptr);

		mpt::ustring patName;
		for(PATTERNINDEX pat = minPat; pat < maxPat; pat++)
		{
			mpt::uchar s[256];
			s[0] = 0;
			if(sndFile.Patterns.IsValidPat(pat))
			{
				patName = mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.Patterns[pat].GetName());
				wsprintf(s, UL_("%u"), pat);
				if(!patName.empty())
				{
					_tcscat(s, UL_(": "));
					_tcscat(s, patName.c_str());
				}
				if(info.tiPatterns[pat])
				{
					tvi.mask = ui::TreeItemText | TreeItemHandleMask;
					tvi.item = info.tiPatterns[pat];
					GetItem(&tvi);
					if(s != tvi.text)
						SetItem(info.tiPatterns[pat], ui::TreeItemText, s, 0, 0, 0, 0, 0);
				} else
				{
					info.tiPatterns[pat] = InsertItem(s, IMAGE_PATTERNS, IMAGE_PATTERNS, info.hPatterns, ui::TreeLast);
				}
				SetItemData(info.tiPatterns[pat], pat);
			} else if(pat < info.tiPatterns.size() && info.tiPatterns[pat])
			{
				DeleteItem(info.tiPatterns[pat]);
				info.tiPatterns[pat] = nullptr;
			}
		}
	}
	// Add Samples
	const SampleHint sampleHint = hint.ToType<SampleHint>();
	if(info.hSamples && sampleHint.GetType()[HINT_MODTYPE | HINT_SMPNAMES | HINT_SAMPLEINFO | HINT_SAMPLEDATA])
	{
		const SAMPLEINDEX hintSmp = sampleHint.GetSample();
		SAMPLEINDEX smin = 1, smax = MAX_SAMPLES - 1;
		if(sampleHint.GetType()[HINT_SMPNAMES | HINT_SAMPLEINFO | HINT_SAMPLEDATA] && hintSmp > 0 && hintSmp < MAX_SAMPLES)
		{
			smin = smax = hintSmp;
		}
		TreeItemHandle hChild = GetNthChildItem(info.hSamples, smin - 1);
		for(SAMPLEINDEX nSmp = smin; nSmp <= smax; nSmp++)
		{
			mpt::uchar s[256];
			s[0] = 0;
			TreeItemHandle hNextChild = GetNextSiblingItem(hChild);
			if(nSmp <= sndFile.GetNumSamples())
			{
				const ModSample &sample = sndFile.GetSample(nSmp);
				const bool sampleExists = (sample.HasSampleData());

				static constexpr int Images[] =
				{
					IMAGE_NOSAMPLE,  IMAGE_NOSAMPLE,        IMAGE_NOSAMPLE,
					IMAGE_SAMPLES,   IMAGE_SAMPLEACTIVE,    IMAGE_SAMPLEMUTE,
					IMAGE_EXTSAMPLE, IMAGE_EXTSAMPLEACTIVE, IMAGE_EXTSAMPLEMUTE,
					IMAGE_OPLINSTR,  IMAGE_OPLINSTRACTIVE,  IMAGE_OPLINSTRMUTE,
				};

				int image = 0;
				if(sampleExists)
					image = 3;
				if(sample.uFlags[SMP_KEEPONDISK])
					image = 6;
				if(sample.uFlags[CHN_ADLIB])
					image = 9;

				if(info.modDoc.IsSampleMuted(nSmp))
					image += 2;
				else if(info.samplesPlaying[nSmp])
					image++;

				if(sample.uFlags[SMP_KEEPONDISK] && !sampleExists)
					image = IMAGE_EXTSAMPLEMISSING;
				else
					image = Images[image];

				if(sndFile.GetType() == MOD_TYPE_MPT)
				{
					const mpt::uchar *status = UL_("");
					if(sample.uFlags[SMP_KEEPONDISK])
					{
						status = sampleExists ? UL_(" [external]") : UL_(" [MISSING]");
					}
					wsprintf(s, UL_("%3d: %s%s%s"), nSmp, sample.uFlags.test_all(SMP_MODIFIED | SMP_KEEPONDISK) ? UL_("* ") : UL_(""), mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.m_szNames[nSmp]).c_str(), status);
				} else
				{
					wsprintf(s, UL_("%3d: %s"), nSmp, mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.m_szNames[nSmp]).c_str());
				}

				if(!hChild)
				{
					hChild = InsertItem(ui::TreeItemText | ui::TreeItemImage | ui::TreeItemSelectedImage | ui::TreeItemParam, s, image, image, 0, 0, nSmp, info.hSamples, ui::TreeLast);
				} else
				{
					tvi.mask = ui::TreeItemText | TreeItemHandleMask | ui::TreeItemImage;
					tvi.item = hChild;
					tvi.image = tvi.selectedImage = image;
					GetItem(&tvi);
					if(tvi.image != image || s != tvi.text || GetItemData(hChild) != nSmp)
					{
						SetItem(hChild, ui::TreeItemText | ui::TreeItemImage | ui::TreeItemSelectedImage | ui::TreeItemParam, s, image, image, 0, 0, nSmp);
					}
				}
			} else if(hChild != nullptr)
			{
				DeleteItem(hChild);
			} else
			{
				break;
			}
			hChild = hNextChild;
		}
	}
	// Add Instruments
	const InstrumentHint instrHint = hint.ToType<InstrumentHint>();
	if(info.hInstruments && instrHint.GetType()[HINT_MODTYPE | HINT_INSNAMES | HINT_INSTRUMENT])
	{
		INSTRUMENTINDEX smin = 1, smax = MAX_INSTRUMENTS - 1;
		const INSTRUMENTINDEX hintIns = instrHint.GetInstrument();
		if(instrHint.GetType()[HINT_INSNAMES | HINT_INSTRUMENT] && hintIns > 0 && hintIns < MAX_INSTRUMENTS)
		{
			smin = smax = hintIns;
		}
		TreeItemHandle hChild = GetNthChildItem(info.hInstruments, smin - 1);
		for(INSTRUMENTINDEX nIns = smin; nIns <= smax; nIns++)
		{
			mpt::uchar s[256];
			s[0] = 0;
			TreeItemHandle hNextChild = GetNextSiblingItem(hChild);
			if(nIns <= sndFile.GetNumInstruments())
			{
				wsprintf(s, UL_("%3u: %s"), nIns, mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.GetInstrumentName(nIns)).c_str());

				int nImage = IMAGE_INSTRUMENTS;
				if(info.instrumentsPlaying[nIns])
					nImage = IMAGE_INSTRACTIVE;
				if(!sndFile.Instruments[nIns] || info.modDoc.IsInstrumentMuted(nIns))
					nImage = IMAGE_INSTRMUTE;

				if(!hChild)
				{
					hChild = InsertItem(ui::TreeItemText | ui::TreeItemImage | ui::TreeItemSelectedImage | ui::TreeItemParam, s, nImage, nImage, 0, 0, nIns, info.hInstruments, ui::TreeLast);
				} else
				{
					tvi.mask = ui::TreeItemText | TreeItemHandleMask | ui::TreeItemImage;
					tvi.item = hChild;
					tvi.image = tvi.selectedImage = nImage;
					GetItem(&tvi);
					if(tvi.image != nImage || s != tvi.text || GetItemData(hChild) != nIns)
					{
						SetItem(hChild, ui::TreeItemText | ui::TreeItemImage | ui::TreeItemSelectedImage | ui::TreeItemParam, s, nImage, nImage, 0, 0, nIns);
					}
				}
			} else if(hChild != nullptr)
			{
				DeleteItem(hChild);
			} else
			{
				break;
			}
			hChild = hNextChild;
		}
	}
}


CModTree::ModItem CModTree::GetModItem(TreeItemHandle hItem)
{
	if(!hItem)
		return ModItem(MODITEM_NULL);
	// First, test root items
	if(hItem == m_hInsLib)
		return ModItem(MODITEM_HDR_INSTRUMENTLIB);
	if(hItem == m_hMidiLib)
		return ModItem(MODITEM_HDR_MIDILIB);

	// The immediate parent of the item (NULL if this item is on the root level of the tree)
	TreeItemHandle hItemParent = GetParentItem(hItem);
	// Parent of the parent.
	TreeItemHandle hItemParentParent = GetParentItem(hItemParent);
	// Get the root parent of the selected item, which can be the item itself.
	TreeItemHandle hRootParent = hItem;
	if(!IsSampleBrowser())
	{
		hRootParent = GetParentRootItem(hItem);
	}

	uint32 itemData = static_cast<uint32>(GetItemData(hItem));
	uint32 rootItemData = static_cast<uint32>(GetItemData(hRootParent));

	// Midi Library
	if(hRootParent == m_hMidiLib && hRootParent != hItem && !IsSampleBrowser())
	{
		return ModItem(static_cast<ModItemType>(itemData >> MIDILIB_SHIFT), itemData & MIDILIB_MASK);
	}
	// Instrument Library
	if(hRootParent == m_hInsLib || (IsSampleBrowser() && hItem != m_hInsLib))
	{
		ui::TreeItemInfo tvi;
		tvi.mask = ui::TreeItemImage | TreeItemHandleMask;
		tvi.item = hItem;
		tvi.image = 0;
		if(GetItem(&tvi))
		{
			switch(tvi.image)
			{
			case IMAGE_SAMPLES:
			case IMAGE_OPLINSTR:
				// Sample
				return ModItem(MODITEM_INSLIB_SAMPLE);
			case IMAGE_INSTRUMENTS:
				// Instrument
				return ModItem(MODITEM_INSLIB_INSTRUMENT);
			case IMAGE_FOLDERSONG:
				// Song
				return ModItem(MODITEM_INSLIB_SONG);
			default:
				return ModItem(MODITEM_INSLIB_FOLDER);
			}
		}
		return ModItem(MODITEM_NULL);
	}
	if(IsSampleBrowser())
		return ModItem(MODITEM_NULL);
	// Songs
	if(auto info = GetDocumentInfoFromItem(hRootParent); info != nullptr)
	{
		m_selectedDoc = &info->modDoc;

		if(hItem == info->hSong)
			return ModItem(MODITEM_HDR_SONG);
		if(hRootParent == info->hSong)
		{
			if(hItem == info->hPatterns)
				return ModItem(MODITEM_HDR_PATTERNS);
			if(hItem == info->hOrders)
				return ModItem(MODITEM_HDR_ORDERS);
			if(hItem == info->hSamples)
				return ModItem(MODITEM_HDR_SAMPLES);
			if(hItem == info->hInstruments)
				return ModItem(MODITEM_HDR_INSTRUMENTS);
			if(hItem == info->hEffects)
				return ModItem(MODITEM_HDR_EFFECTS);
			if(hItem == info->hComments)
				return ModItem(MODITEM_COMMENTS);
			// Order List or Sequence item?
			if((hItemParent == info->hOrders) || (hItemParentParent == info->hOrders))
			{
				const auto ord = static_cast<ORDERINDEX>(itemData & SEQU_MASK);
				const auto seq = static_cast<SEQUENCEINDEX>(itemData >> SEQU_SHIFT);
				if(ord == ORDERINDEX_INVALID)
					return ModItem(MODITEM_SEQUENCE, seq);
				else
					return ModItem(MODITEM_ORDER, ord, seq);
			}

			ModItem modItem(MODITEM_NULL, itemData);
			if(hItemParent == info->hPatterns)
			{
				// Pattern
				modItem.type = MODITEM_PATTERN;
			} else if(hItemParent == info->hSamples)
			{
				// Sample
				modItem.type = MODITEM_SAMPLE;
			} else if(hItemParent == info->hInstruments)
			{
				// Instrument
				modItem.type = MODITEM_INSTRUMENT;
			} else if(hItemParent == info->hEffects)
			{
				// Effect
				modItem.type = MODITEM_EFFECT;
			}
			return modItem;
		}
	}

	// DLS banks
	if(itemData < m_tiDLS.size() && hItem == m_tiDLS[itemData])
		return ModItem(MODITEM_DLSBANK_FOLDER, itemData);

	// DLS Instruments
	if(hRootParent != nullptr)
	{
		if(rootItemData < m_tiDLS.size() && m_tiDLS[rootItemData] == hRootParent)
		{
			int image = 0, selImage = 0;
			const bool isFolder = GetItemImage(hItem, image, selImage) && (image == IMAGE_FOLDER || image == IMAGE_OPENFOLDER);
			if(!isFolder || GetItemData(hItemParent) == DLS_DRUM_FOLDER_LPARAM)
				return DlsItem::FromLPARAM(itemData);
		}
	}
	return ModItem(MODITEM_NULL);
}


bool CModTree::ExecuteItem(TreeItemHandle hItem)
{
	if(hItem)
	{
		const ModItem modItem = GetModItem(hItem);
		uint32 modItemID = modItem.val1;
		CModDoc *modDoc = m_docInfo.count(m_selectedDoc) ? m_selectedDoc : nullptr;

		switch(modItem.type)
		{
		case MODITEM_COMMENTS:
			if(modDoc)
				modDoc->ActivateView(IDD_CONTROL_COMMENTS, 0);
			return true;

		case MODITEM_SEQUENCE:
			if(modDoc
				&& modItemID < modDoc->GetSoundFile().Order.GetNumSequences()
				&& !modDoc->GetSoundFile().Order(static_cast<SEQUENCEINDEX>(modItemID)).GetLengthTailTrimmed())
				modDoc->ActivateView(IDD_CONTROL_PATTERNS, (modItemID << SEQU_SHIFT) | SEQU_INDICATOR);
			return true;

		case MODITEM_ORDER:
			if(modDoc)
				modDoc->ActivateView(IDD_CONTROL_PATTERNS, modItemID | (uint32(modItem.val2) << SEQU_SHIFT) | SEQU_INDICATOR);
			return true;

		case MODITEM_PATTERN:
			if(modDoc)
				modDoc->ActivateView(IDD_CONTROL_PATTERNS, modItemID);
			return true;

		case MODITEM_SAMPLE:
			if(modDoc)
				modDoc->ActivateView(IDD_CONTROL_SAMPLES, modItemID);
			return true;

		case MODITEM_INSTRUMENT:
			if(modDoc)
				modDoc->ActivateView(IDD_CONTROL_INSTRUMENTS, modItemID);
			return true;

		case MODITEM_MIDIPERCUSSION:
			modItemID |= 0x80;
			[[fallthrough]];
		case MODITEM_MIDIINSTRUMENT:
			OpenMidiInstrument(modItemID);
			return true;

		case MODITEM_EFFECT:
		case MODITEM_INSLIB_SAMPLE:
		case MODITEM_INSLIB_INSTRUMENT:
			PlayItem(hItem, NOTE_MIDDLEC);
			return true;

		case MODITEM_INSLIB_SONG:
		case MODITEM_INSLIB_FOLDER:
			InstrumentLibraryChDir(mpt::PathString::FromUnicode(GetItemText(hItem)), modItem.type == MODITEM_INSLIB_SONG);
			return true;

		case MODITEM_HDR_SONG:
			if(modDoc)
				modDoc->ActivateWindow();
			return true;

		case MODITEM_DLSBANK_INSTRUMENT:
			if(GetItemData(GetParentItem(hItem)) != DLS_DRUM_FOLDER_LPARAM)
				PlayItem(hItem, NOTE_MIDDLEC);
			return true;

		case MODITEM_HDR_INSTRUMENTLIB:
			if(IsSampleBrowser())
			{
				BrowseForFolder dlg(m_InstrLibPath, UL_("Select a new instrument library folder..."));
				if(dlg.Show())
				{
					SetFullInstrumentLibraryPath(dlg.GetDirectory());
				}
				return true;
			}
			break;

		default:
			break;
		}
	}
	return false;
}


void CModTree::PlayDLSItem(const CDLSBank &dlsBank, const DlsItem &item, ModCommand::NOTE note)
{
	uint32 rgn, instr = item.GetInstr();
	if(item.IsPercussion())
		rgn = item.GetRegion();
	else
		rgn = dlsBank.GetRegionFromKey(instr, note - NOTE_MIN);
	CMainFrame::GetMainFrame()->PlayDLSInstrument(dlsBank, instr, rgn, note);
}


bool CModTree::PlayItem(TreeItemHandle hItem, ModCommand::NOTE note, int volume)
{
	if(hItem)
	{
		const ModItem modItem = GetModItem(hItem);
		uint32 modItemID = modItem.val1;
		CModDoc *modDoc = m_docInfo.count(m_selectedDoc) ? m_selectedDoc : nullptr;

		switch(modItem.type)
		{
		case MODITEM_SAMPLE:
			if(modDoc)
			{
				if(note == NOTE_NOTECUT)
				{
					modDoc->NoteOff(0, true);  // cut previous playing samples
				} else if(note & 0x80)
				{
					modDoc->NoteOff(note & 0x7F, true);
				} else
				{
					modDoc->NoteOff(0, true);  // cut previous playing samples
					modDoc->PlayNote(PlayNoteParam(note & 0x7F).Sample(static_cast<SAMPLEINDEX>(modItemID)).Volume(volume));
				}
			}
			return true;

		case MODITEM_INSTRUMENT:
			if(modDoc)
			{
				if(note == NOTE_NOTECUT)
				{
					modDoc->NoteOff(0, true);
				} else if(note & 0x80)
				{
					modDoc->NoteOff(note & 0x7F, true);
				} else
				{
					modDoc->NoteOff(0, true);
					modDoc->PlayNote(PlayNoteParam(note & 0x7F).Instrument(static_cast<INSTRUMENTINDEX>(modItemID)).Volume(volume));
				}
			}
			return true;

		case MODITEM_EFFECT:
			if((modDoc) && (modItemID < MAX_MIXPLUGINS))
			{
				modDoc->TogglePluginEditor(modItemID);
			}
			return true;

		case MODITEM_INSLIB_SAMPLE:
		case MODITEM_INSLIB_INSTRUMENT:
			if(note != NOTE_NOTECUT)
			{
				CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
				if(!m_SongFileName.empty())
				{
					// Preview sample / instrument in module
					const size_t n = mpt::parse<size_t>(GetItemText(hItem));
					if(pMainFrm && m_SongFile)
					{
						if(modItem.type == MODITEM_INSLIB_INSTRUMENT)
						{
							pMainFrm->PlaySoundFile(*m_SongFile, static_cast<INSTRUMENTINDEX>(n), SAMPLEINDEX_INVALID, note, volume);
						} else
						{
							pMainFrm->PlaySoundFile(*m_SongFile, INSTRUMENTINDEX_INVALID, static_cast<SAMPLEINDEX>(n), note, volume);
						}
					}
				} else
				{
					// Preview sample / instrument file
					const auto file = InsLibGetFullPath(hItem);

					if(pMainFrm)
						pMainFrm->PlaySoundFile(file, note, volume);
				}
			} else
			{
				CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
				if(pMainFrm)
					pMainFrm->StopPreview();
			}

			return true;

		case MODITEM_MIDIPERCUSSION:
			modItemID |= 0x80;
			[[fallthrough]];
		case MODITEM_MIDIINSTRUMENT:
			{
				const MidiLibrary &midiLib = CTrackApp::GetMidiLibrary();
				if(modItemID < midiLib.size() && midiLib[modItemID] && !midiLib[modItemID]->empty())
				{
					CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
					CDLSBank *dlsBank = nullptr;
					if(!mpt::PathCompareNoCase(m_cachedBankName, *midiLib[modItemID]))
					{
						dlsBank = m_cachedBank.get();
					}
					if(dlsBank == nullptr && CDLSBank::IsDLSBank(*midiLib[modItemID]))
					{
						m_cachedBank = std::make_unique<CDLSBank>();
						if(m_cachedBank->Open(*midiLib[modItemID]))
						{
							m_cachedBankName = *midiLib[modItemID];
							dlsBank = m_cachedBank.get();
						}
					}
					if(dlsBank != nullptr)
					{
						uint32 item = 0;
						if(modItemID < 0x80)
						{
							if(dlsBank->FindInstrument(false, 0xFFFF, modItemID, note - NOTE_MIN, &item))
								PlayDLSItem(*dlsBank, DlsItem(static_cast<uint16>(item)), note);
						} else
						{
							if(dlsBank->FindInstrument(true, 0xFFFF, 0xFF, modItemID & 0x7F, &item))
								PlayDLSItem(*dlsBank, DlsItem(static_cast<uint16>(item), static_cast<uint16>(dlsBank->GetRegionFromKey(item, modItemID & 0x7F))), note);
						}
					} else
					{
						pMainFrm->PlaySoundFile(*midiLib[modItemID], note, volume);
					}
				}
			}
			return true;

		case MODITEM_DLSBANK_INSTRUMENT:
			{
				const DlsItem &item = static_cast<const DlsItem &>(modItem);
				CDLSBank *dlsBank = GetDLSBankFromItem(hItem);
				if(dlsBank != nullptr)
				{
					PlayDLSItem(*dlsBank, item, note);
					return true;
				}
			}
			break;

		default:
			break;
		}
	}
	return false;
}


bool CModTree::SetMidiInstrument(uint32 nIns, const mpt::PathString &fileName)
{
	MidiLibrary &midiLib = CTrackApp::GetMidiLibrary();
	if(nIns < 128)
	{
		midiLib[nIns] = fileName;
		RefreshMidiLibrary();
		return true;
	}
	return false;
}


bool CModTree::SetMidiPercussion(uint32 nPerc, const mpt::PathString &fileName)
{
	MidiLibrary &midiLib = CTrackApp::GetMidiLibrary();
	if(nPerc < 128)
	{
		uint32 nIns = nPerc | 0x80;
		midiLib[nIns] = fileName;
		RefreshMidiLibrary();
		return true;
	}
	return false;
}


static mpt::ustring TreeDeletionString(const mpt::uchar *type, uint32 id, const mpt::ustring &name)
{
	mpt::ustring s = MPT_UFORMAT("Remove {} {}")(mpt::ustring(type), id);
	if(!name.empty())
		s += UL_(": ") + name;
	s.append(1, UC_('?'));
	return s;
}


void CModTree::DeleteTreeItem(TreeItemHandle hItem, const bool permanently)
{
	const ModItem modItem = GetModItem(hItem);
	uint32 modItemID = modItem.val1;

	CModDoc *modDoc = m_docInfo.count(m_selectedDoc) ? m_selectedDoc : nullptr;
	CSoundFile *sndFile = modDoc ? &modDoc->GetSoundFile() : nullptr;
	if(modItem.IsSongItem() && modDoc == nullptr)
	{
		return;
	}

	switch(modItem.type)
	{
	case MODITEM_SEQUENCE:
		if(sndFile)
		{
			const SEQUENCEINDEX seq = static_cast<SEQUENCEINDEX>(modItemID);
			if(Reporting::Confirm(TreeDeletionString(UL_("sequence"), seq + 1, sndFile->Order(seq).GetName()), false, true) == cnfNo) break;
			sndFile->Order.RemoveSequence(seq);
			modDoc->UpdateAllViews(nullptr, SequenceHint().Data());
		}
		break;

	case MODITEM_ORDER:
		// might be slightly annoying to ask for confirmation here, and it's rather easy to restore the orderlist anyway.
		if(modDoc && modDoc->RemoveOrder(static_cast<SEQUENCEINDEX>(modItem.val2), static_cast<ORDERINDEX>(modItem.val1)))
		{
			modDoc->UpdateAllViews(nullptr, SequenceHint().Data());
		}
		break;

	case MODITEM_PATTERN:
		if(modDoc && sndFile)
		{
			const PATTERNINDEX pat = static_cast<PATTERNINDEX>(modItemID);
			bool isUsed = false;
			// First, find all used patterns in all sequences.
			for(const auto &sequence : sndFile->Order)
			{
				if(sequence.FindOrder(pat) != ORDERINDEX_INVALID)
				{
					isUsed = true;
					break;
				}
			}
			mpt::ustring s = TreeDeletionString(UL_("pattern"), modItemID, mpt::ToUnicode(sndFile->GetCharsetInternal(), sndFile->Patterns[pat].GetName()));
			s += MPT_UFORMAT("\nThis pattern is currently {}used.")(isUsed ? UL_("") : UL_("un"));
			if(Reporting::Confirm(s, false, isUsed) == cnfYes && modDoc->RemovePattern(pat))
			{
				modDoc->UpdateAllViews(nullptr, PatternHint(pat).Data().Names());
				if(isUsed)
					modDoc->UpdateAllViews(nullptr, SequenceHint().Data());  // Pattern color will change in sequence
			}
		}
		break;

	case MODITEM_SAMPLE:
		if(modDoc && sndFile)
		{
			if(!sndFile->GetSample(static_cast<SAMPLEINDEX>(modItemID)).HasSampleData()
				 || Reporting::Confirm(TreeDeletionString(UL_("sample"), modItemID, mpt::ToUnicode(sndFile->GetCharsetInternal(), sndFile->m_szNames[modItemID])), false, true) == cnfYes)
			{
				const SAMPLEINDEX smp = static_cast<SAMPLEINDEX>(modItemID);
				modDoc->GetSampleUndo().PrepareUndo(smp, sundo_replace, "Delete");
				const SAMPLEINDEX oldNumSamples = modDoc->GetNumSamples();
				if(modDoc->RemoveSample(smp))
				{
					modDoc->UpdateAllViews(nullptr, SampleHint(modDoc->GetNumSamples() != oldNumSamples ? 0 : smp).Info().Data().Names());
				}
			}
		}
		break;

	case MODITEM_INSTRUMENT:
		if(modDoc && sndFile)
		{
			if(sndFile->Instruments[modItemID] == nullptr
				 || Reporting::Confirm(TreeDeletionString(UL_("instrument"), modItemID, mpt::ToUnicode(sndFile->GetCharsetInternal(), sndFile->Instruments[modItemID]->name)), false, true) == cnfYes)
			{
				const INSTRUMENTINDEX ins = static_cast<INSTRUMENTINDEX>(modItemID);
				modDoc->GetInstrumentUndo().PrepareUndo(ins, "Delete");
				const INSTRUMENTINDEX oldNumInstrs = modDoc->GetNumInstruments();
				if(modDoc->RemoveInstrument(ins))
				{
					modDoc->UpdateAllViews(nullptr, InstrumentHint(modDoc->GetNumInstruments() != oldNumInstrs ? 0 : ins).Info().Envelope().ModType());
				}
			}
		}
		break;

	case MODITEM_EFFECT:
		if(modDoc && Reporting::Confirm(TreeDeletionString(UL_("plugin FX"), modItemID + 1, sndFile->m_MixPlugins[modItemID].GetName()), false, true) == cnfYes)
		{
			modDoc->RemovePlugin(static_cast<PLUGINDEX>(modItemID));
		}
		break;

	case MODITEM_MIDIINSTRUMENT:
		SetMidiInstrument(modItemID, P_(""));
		RefreshMidiLibrary();
		break;
	case MODITEM_MIDIPERCUSSION:
		SetMidiPercussion(modItemID, P_(""));
		RefreshMidiLibrary();
		break;

	case MODITEM_DLSBANK_FOLDER:
		CTrackApp::RemoveDLSBank(modItemID);
		RefreshDlsBanks();
		break;

	case MODITEM_INSLIB_SONG:
	case MODITEM_INSLIB_SAMPLE:
	case MODITEM_INSLIB_INSTRUMENT:
		{
			const mpt::PathString fullPath = InsLibGetFullPath(hItem);
			if(permanently ? Util::DeleteFile(fullPath) : Util::MoveToTrash(fullPath))
			{
				TreeItemHandle newSel = GetNextSiblingItem(hItem);
				if(!newSel) newSel = GetPrevSiblingItem(hItem);
				SelectItem(newSel);
				RefreshInstrumentLibrary();
				SetFocus();
			}
		}
		break;

	default:
		break;
	}
}


bool CModTree::OpenTreeItem(TreeItemHandle hItem)
{
	const ModItem modItem = GetModItem(hItem);

	switch(modItem.type)
	{
	case MODITEM_HDR_SONG:
		if(const auto pathName = GetDocumentFromItem(hItem)->GetPathNameMpt(); !pathName.empty())
			CTrackApp::OpenDirectory(pathName);
		break;
	case MODITEM_INSLIB_SONG:
		theApp.OpenDocumentFile(InsLibGetFullPath(hItem));
		break;
	case MODITEM_HDR_INSTRUMENTLIB:
		CTrackApp::OpenDirectory(m_InstrLibPath + m_SongFileName);
		break;
	case MODITEM_INSLIB_FOLDER:
	case MODITEM_INSLIB_INSTRUMENT:
	case MODITEM_INSLIB_SAMPLE:
		// Open path in Explorer
		CTrackApp::OpenDirectory(InsLibGetFullPath(hItem));
		break;
	default:
		break;
	}
	return true;
}


bool CModTree::OpenMidiInstrument(uint32 dwItem)
{
	std::vector<FileType> mediaFoundationTypes = CSoundFile::GetMediaFoundationFileTypes();
	FileDialog dlg = OpenFileDialog()
		.EnableAudioPreview()
		.ExtensionFilter(
			"All Instruments and Banks (*.xi,*.pat,*.iti,*.sfz,*.dls,*.sf2,...)|*.xi;*.pat;*.iti;*.sfz;*.wav;*.w64;*.caf;*.aif;*.aiff;*.sbk;*.sf2;*.sf3;*.sf4;*.dls;*.mss;*.flac;*.opus;*.ogg;*.oga;*.mp1;*.mp2;*.mp3" + ToFilterOnlyString(mediaFoundationTypes, true).ToLocale() + "|"
			"FastTracker II Instruments (*.xi)|*.xi|"
			"GF1 Patches (*.pat)|*.pat|"
			"Wave Files (*.wav)|*.wav|"
			"Wave64 Files (*.w64)|*.w64|"
			"CAF Files (*.caf)|*.caf|"
	#ifdef MPT_WITH_FLAC
			"FLAC Files (*.flac,*.oga)|*.flac;*.oga|"
	#endif // MPT_WITH_FLAC
	#if defined(MPT_WITH_OPUSFILE)
			"Opus Files (*.opus,*.oga)|*.opus;*.oga|"
	#endif // MPT_WITH_OPUSFILE
	#if defined(MPT_WITH_VORBISFILE) || defined(MPT_WITH_STBVORBIS)
			"Ogg Vorbis Files (*.ogg,*.oga)|*.ogg;*.oga|"
	#endif // VORBIS
	#if defined(MPT_ENABLE_MP3_SAMPLES)
			"MPEG Files (*.mp1,*.mp2,*.mp3)|*.mp1;*.mp2;*.mp3|"
	#endif // MPT_ENABLE_MP3_SAMPLES
			"Impulse Tracker Instruments (*.iti)|*.iti;*.its|"
			"SFZ Instruments (*.sfz)|*.sfz|"
			"SoundFont 2.0 Banks (*.sf2)|*.sbk;*.sf2;*.sf3;*.sf4|"
			"DLS Sound Banks (*.dls;*.mss)|*.dls;*.mss|"
			"All Files (*.*)|*.*||");
	if(!dlg.Show()) return false;

	if(dwItem & 0x80)
		return SetMidiPercussion(dwItem & 0x7F, dlg.GetFirstFile());
	else
		return SetMidiInstrument(dwItem, dlg.GetFirstFile());
}


// Refresh Instrument Library
void CModTree::FillInstrumentLibrary(const mpt::ustring &selectedItem)
{
	if(!m_hInsLib && !IsSampleBrowser())
		return;

	LockRedraw();

	if(!IsSampleBrowser())
	{
		DeleteChildren(m_hInsLib);
	} else
	{
		DeleteItem(ui::TreeRoot);
		m_hInsLib = nullptr;
	}

	m_fileBrowserEntries.clear();

	if(!m_SongFileName.empty() && IsSampleBrowser() && m_SongFile)
	{
		// Fill browser with samples / instruments of module file
		for(INSTRUMENTINDEX ins = 1; ins <= m_SongFile->GetNumInstruments(); ins++)
		{
			ModInstrument *pIns = m_SongFile->Instruments[ins];
			if(pIns)
			{
				m_fileBrowserEntries.push_back({ui::Format(UL_("%3d: "), ins) + mpt::ToUnicode(m_SongFile->GetCharsetInternal(), pIns->name), 0, 0, IMAGE_INSTRUMENTS, false});
			}
		}
		for(SAMPLEINDEX smp = 1; smp <= m_SongFile->GetNumSamples(); smp++)
		{
			const ModSample &sample = m_SongFile->GetSample(smp);
			if(sample.HasSampleData() || sample.uFlags[CHN_ADLIB])
			{
				m_fileBrowserEntries.push_back({ui::Format(UL_("%3d: "), smp) + mpt::ToUnicode(m_SongFile->GetCharsetInternal(), m_SongFile->m_szNames[smp]), sample.GetSampleSizeInBytes(), 0, static_cast<uint32>(sample.uFlags[CHN_ADLIB] ? IMAGE_OPLINSTR : IMAGE_SAMPLES), false});
			}
		}
	} else if(!m_InstrLibPath.empty())
	{
		if(!IsSampleBrowser())
		{
			SetItemText(m_hInsLib, UL_("Instrument Library (") + m_InstrLibPath.ToUnicode() + UL_(")"));
		}

		// Shortcuts to the file system root, the home directory and mounted media
		if(!IsSampleBrowser())
		{
			const auto addRoot = [this](const std::filesystem::path &path)
			{
				std::error_code ec;
				if(!std::filesystem::is_directory(path, ec))
					return;
				m_fileBrowserEntries.push_back({mpt::PathString::FromUTF8(path.string()).WithTrailingSlash().ToUnicode(), 0, 0, static_cast<uint32>(IMAGE_FOLDER), false});
			};
			addRoot("/");
			if(const char *home = std::getenv("HOME"); home && *home)
				addRoot(home);
			const char *user = std::getenv("USER");
			for(const char *mediaRoot : {"/media", "/run/media", "/mnt"})
			{
				std::error_code ec;
				const std::filesystem::path mediaDir = (user && std::string_view(mediaRoot) != "/mnt") ? std::filesystem::path(mediaRoot) / user : std::filesystem::path(mediaRoot);
				for(std::filesystem::directory_iterator it(mediaDir, ec), end; !ec && it != end; it.increment(ec))
					addRoot(it->path());
			}
		}

		// Enumerating Directories and samples/instruments
		const bool showDirs = !IsSampleBrowser() || TrackerSettings::Instance().showDirsInSampleBrowser;
		const bool showInstrs = IsSampleBrowser();

		enum
		{
			FILTER_FIRST_VALID = 0,
			FILTER_REJECT_FILE = -1,
		};

		const auto FilterFile = [this, showInstrs, showDirs](const mpt::PathString &fileName) -> int
		{

			static constexpr auto instrExts = {"xi", "iti", "sfz", "sf2", "sf3", "sf4", "sbk", "dls", "mss", "pat"};
			static constexpr auto sampleExts = {"wav", "flac", "ogg", "opus", "mp1", "mp2", "mp3", "smp", "raw", "s3i", "its", "aif", "aiff", "au", "snd", "svx", "voc", "8sv", "8svx", "16sv", "16svx", "w64", "caf", "sb0", "sb2", "sbi", "brr"};
			// These are hidden even when "show all files" is enabled, as it would be extremely unlikely that they contain any sample data
			static constexpr auto hideFileExts = {"txt", "diz", "nfo", "doc", "ini", "pdf", "zip", "rar", "lha", "exe", "dll", "lnk", "url"};

			// Get lower-case file extension without dot.
			mpt::PathString extPS = fileName.GetFilenameExtension();
			std::string ext = extPS.ToUTF8();
			if(!ext.empty())
			{
				ext.erase(0, 1);
				ext = mpt::ToLowerCaseAscii(ext);
				extPS = mpt::PathString::FromUTF8(ext);
			}

			if(mpt::contains(instrExts, ext))
			{
				if(showInstrs)
					return IMAGE_INSTRUMENTS;
			} else if(mpt::contains(sampleExts, ext))
			{
				if(showInstrs)
					return IMAGE_SAMPLES;
			} else if(mpt::contains(m_modExtensions, ext))
			{
				if(showDirs || m_showAllFiles)
					return IMAGE_FOLDERSONG;
			} else if(!extPS.empty() && mpt::contains(m_MediaFoundationExtensions, extPS))
			{
				if(showInstrs)
					return IMAGE_SAMPLES;
			} else
			{
				if(showDirs)
				{
					// Amiga-style prefix (i.e. mod.songname)
					std::string prefixExt = fileName.ToUTF8();
					const auto dotPos = prefixExt.find('.');
					if(dotPos != std::string::npos && mpt::contains(m_modExtensions, prefixExt.erase(dotPos)))
						return IMAGE_FOLDERSONG;
				}
			}

			if(m_showAllFiles && !mpt::contains(hideFileExts, ext))
				return IMAGE_SAMPLES;

			return FILTER_REJECT_FILE;
		};

		const std::filesystem::path libraryPath(mpt::transcode<std::string>(mpt::common_encoding::utf8, m_InstrLibPath.ToUnicode()));
		std::error_code ec;
		if(showDirs && libraryPath.has_relative_path() && libraryPath.parent_path() != libraryPath)
			m_fileBrowserEntries.push_back({UL_(".."), 0, 0, static_cast<uint32>(IMAGE_FOLDERPARENT), false});

		for(std::filesystem::directory_iterator it(libraryPath, ec), end; !ec && it != end; it.increment(ec))
		{
			const std::filesystem::directory_entry &dirEntry = *it;
			const std::string fileNameUtf8 = dirEntry.path().filename().string();
			// Dot files are hidden
			if(fileNameUtf8.empty() || fileNameUtf8[0] == '.')
				continue;
			const mpt::ustring fileName = mpt::transcode<mpt::ustring>(mpt::common_encoding::utf8, fileNameUtf8);

			std::error_code statusError;
			int type = FILTER_REJECT_FILE;
			uint64 size = 0, modTime = 0;
			if(dirEntry.is_directory(statusError))
			{
				if(showDirs)
					type = IMAGE_FOLDER;
			} else if(dirEntry.is_regular_file(statusError))
			{
				size = static_cast<uint64>(dirEntry.file_size(statusError));
				if(size >= 9)
					type = FilterFile(mpt::PathString::FromUnicode(fileName));
			}
			if(type >= FILTER_FIRST_VALID)
			{
				modTime = static_cast<uint64>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::time_point_cast<std::chrono::system_clock::duration>(dirEntry.last_write_time(statusError) - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()).time_since_epoch()).count());
				m_fileBrowserEntries.push_back({fileName, size, modTime, static_cast<uint32>(type), false});
			}
		}
	}

	SortInstrumentLibrary();
	FilterInstrumentLibrary(m_filterString, selectedItem);

	UnlockRedraw();

	{
		const std::lock_guard<std::mutex> lock(m_WatchDirMutex);
		if(m_InstrLibPath != m_WatchDir)
		{
			m_WatchDir = m_InstrLibPath;
			m_isWatchDirSwitchPending = true;
			m_watchDirSignal.notify_all();
		}
	}
}


void CModTree::SortInstrumentLibrary()
{
	std::sort(m_fileBrowserEntries.begin(), m_fileBrowserEntries.end(), [this](const FileBrowserEntry &left, const FileBrowserEntry &right)
	{
		const int sortL = ImageToSortOrder(left.image), sortR = ImageToSortOrder(right.image);
		if (sortL != sortR)
			return sortL < sortR;

		if (m_librarySort == LibrarySortOrder::Date && left.modtime != right.modtime)
			return left.modtime > right.modtime;
		else if (m_librarySort == LibrarySortOrder::Size && left.size != right.size)
			return left.size > right.size;

		return Util::IsNaturalLess(left.name, right.name);
	});
}


void CModTree::FilterInstrumentLibrary(mpt::ustring filter, const mpt::ustring &selectedItem)
{
	LockRedraw();

	if(!filter.empty())
		filter = UL_("*") + filter + UL_("*");

	TreeItemHandle item = GetNextItem(m_hInsLib, IsSampleBrowser() ? ui::TreeNextSibling : ui::TreeChild);
	while(item != nullptr)
	{
		TreeItemHandle nextItem = GetNextSiblingItem(item);
		if(nextItem == nullptr)
			break;
		item = nextItem;
	}

	// TODO: Maybe first delete front-to-back, then insert back-to-front?

	// Insert items in reverse, as insertion via ui::TreeFirst is faster than ui::TreeLast as per https://devblogs.microsoft.com/oldnewthing/20111125-00/?p=9033
	uintptr_t entryID = m_fileBrowserEntries.size();
	TreeItemHandle selectedTreeItem = nullptr;
	for(auto entry = m_fileBrowserEntries.crbegin(); entry != m_fileBrowserEntries.crend(); entry++, entryID--)
	{
		const bool add = filter.empty() || Util::IsWildcardMatch(filter, entry->name);

		if(add)
		{
			while(item && GetItemData(item) > entryID)
			{
				item = GetPrevSiblingItem(item);
			}

			if(!item || GetItemData(item) != entryID)
			{
				int state = entry->hidden ? ui::TreeStateCut : 0;
				item = InsertItem(ui::TreeItemImage | ui::TreeItemSelectedImage | ui::TreeItemText | ui::TreeItemState | ui::TreeItemParam, entry->name, entry->image, entry->image, state, state, entryID, IsSampleBrowser() ? ui::TreeRoot : m_hInsLib, item ? item : ui::TreeFirst);
			}

			if(!selectedItem.empty() && entry->name == selectedItem)
			{
				selectedTreeItem = item;
			}

			item = GetPrevSiblingItem(item);
		} else
		{
			while(item && GetItemData(item) >= entryID)
			{
				TreeItemHandle prevItem = GetPrevSiblingItem(item);
				DeleteItem(item);
				if(item == m_hInsLib)
					m_hInsLib = nullptr;
				item = prevItem;
			}
		}
	}

	if(IsSampleBrowser())
	{
		if(m_hInsLib)
			DeleteItem(m_hInsLib);
		if(!m_SongFileName.empty() && m_SongFile)
			m_hInsLib = InsertItem(m_SongFileName.ToUnicode(), IMAGE_FOLDERSONG, IMAGE_FOLDERSONG, ui::TreeRoot, ui::TreeFirst);
		else
			m_hInsLib = InsertItem(m_InstrLibPath.ToUnicode(), IMAGE_FOLDER, IMAGE_FOLDER, ui::TreeRoot, ui::TreeFirst);
	}

	if(!filter.empty() && !selectedTreeItem)
	{
		if(IsSampleBrowser())
		{
			selectedTreeItem = GetFirstVisibleItem();
			if(selectedTreeItem == m_hInsLib)
				selectedTreeItem = GetNextItem(m_hInsLib, ui::TreeNextSibling);
		} else
		{
			selectedTreeItem = GetChildItem(m_hInsLib);
		}
	}

	UnlockRedraw();

	if(selectedTreeItem)
	{
		SelectItem(selectedTreeItem);
		EnsureVisible(selectedTreeItem);
	}
}


void CModTree::SetInstrumentLibraryFilter(const mpt::ustring &filter)
{
	if(m_filterString == filter)
		return;

	m_filterString = filter;
	FilterInstrumentLibrary(m_filterString, GetItemText(GetSelectedItem()));
}


void CModTree::SetInstrumentLibraryFilterSortOrder(LibrarySortOrder sortType)
{
	if(m_librarySort == sortType)
		return;

	m_librarySort = sortType;
	SortInstrumentLibrary();
	FilterInstrumentLibrary(m_filterString, GetItemText(GetSelectedItem()));
	GetOtherView()->SortInstrumentLibrary();
	GetOtherView()->FilterInstrumentLibrary(GetOtherView()->m_filterString, GetItemText(GetSelectedItem()));
}


int CModTree::ImageToSortOrder(int image) const
{
	// Item image indicates sort order
	switch(image)
	{
		case IMAGE_FOLDERPARENT:
			return 1;
		case IMAGE_FOLDER:
			return 2;
		case IMAGE_FOLDERSONG:
			return 3;
		case IMAGE_SAMPLES:
		case IMAGE_OPLINSTR:
			// Only group instruments and samples separately if we're browsing inside a module file
			if(!m_SongFileName.empty())
				return 5;
			[[fallthrough]];
		case IMAGE_INSTRUMENTS:
		default:
			return 4;
	}
}


namespace
{
std::vector<std::filesystem::path> ListDirectory(const mpt::PathString &dir)
{
	std::vector<std::filesystem::path> entries;
	std::error_code ec;
	for(std::filesystem::directory_iterator it(dir.AsNative(), ec), end; !ec && it != end; it.increment(ec))
		entries.push_back(it->path());
	std::sort(entries.begin(), entries.end());
	return entries;
}
}


void CModTree::MonitorInstrumentLibrary()
{
	mpt::log::Trace::SetThreadId(mpt::log::Trace::ThreadKindWatchdir, mpt::log::Trace::GetCurrentThreadId());
	mpt::PathString lastWatchDir;
	std::vector<std::filesystem::path> lastEntries;
	uint64 lastRefresh = Util::GetTickCount64();
	const uint32 interval = TrackerSettings::Instance().FSUpdateInterval;
	constexpr std::chrono::milliseconds pollInterval{500};
	std::unique_lock<std::mutex> lock(m_WatchDirMutex);
	while(!m_isWatchDirKillRequested)
	{
		if(m_WatchDir != lastWatchDir || m_isWatchDirSwitchPending)
		{
			lastWatchDir = m_WatchDir;
			lastEntries = lastWatchDir.empty() ? std::vector<std::filesystem::path>{} : ListDirectory(lastWatchDir);
			m_isWatchDirSwitchPending = false;
		}
		m_watchDirSignal.wait_for(lock, pollInterval);
		if(m_isWatchDirKillRequested)
			break;
		if(lastWatchDir.empty() || m_WatchDir != lastWatchDir)
			continue;
		const uint64 now = Util::GetTickCount64();
		if(now - lastRefresh < interval)
			continue;
		auto entries = ListDirectory(lastWatchDir);
		if(entries != lastEntries)
		{
			lastEntries = std::move(entries);
			lastRefresh = now;
			PostCommand(ID_MODTREE_REFRESHINSTRLIB);
		}
	}
}


void CModTree::SetFullInstrumentLibraryPath(mpt::PathString path)
{
	if(mpt::native_fs{}.is_directory(path))
	{
		path = path.WithTrailingSlash();
		InstrumentLibraryChDir(path, false);
	} else if(mpt::native_fs{}.is_file(path))
	{
		// Browse module contents
		CModTree *dirBrowser = CMainFrame::GetMainFrame()->GetUpperTreeview();
		dirBrowser->m_InstrLibPath = path.GetDirectoryWithDrive();
		dirBrowser->RefreshInstrumentLibrary();
		dirBrowser->InstrumentLibraryChDir(path.GetFilename(), true);
	}
}


void CModTree::InstrumentLibraryChDir(mpt::PathString dir, bool isSong)
{
	if(dir.empty())
		return;

	if(IsSampleBrowser())
	{
		CMainFrame::GetMainFrame()->GetUpperTreeview()->InstrumentLibraryChDir(dir, isSong);
		return;
	}

	GetParent()->PostCommand(ID_CLOSE_LIBRARY_FILTER);
	m_filterString.clear();
	m_pDataTree->m_filterString.clear();

	BeginWaitCursor();

	bool ok = false;
	const bool goUp = (dir == P_(".."));
	m_previousPath = {};
	if(isSong && !goUp)
	{
		ok = m_pDataTree->InsLibSetFullPath(m_InstrLibPath, dir);
		if(ok)
		{
			m_pDataTree->RefreshInstrumentLibrary();
			m_InstrLibHighlightPath = dir;
		}
	} else
	{
		if(goUp)
		{
			if(isSong)
			{
				// Leave song
				m_InstrLibHighlightPath = std::move(m_pDataTree->m_SongFileName);
				dir = m_InstrLibPath;
			} else
			{
				// Go one dir up.
				mpt::ustring prevDir = m_InstrLibPath.GetDirectoryWithDrive().ToUnicode();
				mpt::ustring::size_type pos = prevDir.find_last_of(UL_("\\/"), prevDir.length() - 2);
				if(pos != mpt::ustring::npos)
				{
					m_InstrLibHighlightPath = mpt::PathString::FromUnicode(prevDir.substr(pos + 1, prevDir.length() - pos - 2));  // Highlight previously accessed directory
					prevDir = prevDir.substr(0, pos + 1);
				}
				m_previousPath = m_InstrLibHighlightPath;
				dir = mpt::PathString::FromUnicode(prevDir);
			}
		} else
		{
			// Drives are formatted like "E:\", folders are just folder name without slash.
			do
			{
				if(!dir.HasTrailingSlash())
				{
					dir = m_InstrLibPath + dir;
					dir = dir.WithTrailingSlash();
				}
				m_InstrLibHighlightPath = P_("..");  // Highlight first entry

				FolderScanner scan(dir, FolderScanner::kFilesAndDirectories);
				mpt::PathString name;
				if(scan.Next(name) && !scan.Next(name) && mpt::native_fs{}.is_directory(name))
				{
					// There is only one directory and nothing else in the path,
					// so skip this directory and automatically descend further down into the tree.
					dir = name;
					dir = dir.WithTrailingSlash();
					continue;
				}
			} while(false);
		}

		if(mpt::native_fs{}.is_directory(dir))
		{
			m_SongFileName = P_("");
			delete m_SongFile;
			m_SongFile = nullptr;
			m_InstrLibPath = dir;
			GetSampleBrowser()->m_InstrLibHighlightPath = m_InstrLibHighlightPath;
			PostCommand(ID_MODTREE_REFRESHINSTRLIB);
			ok = true;
		}
	}

	EndWaitCursor();

	if(ok)
	{
		const std::lock_guard<std::mutex> lock(m_WatchDirMutex);
		m_WatchDir = mpt::PathString();
	} else
	{
		Reporting::Error(MPT_UFORMAT("Unable to browse to \"{}\"")(dir), UL_("Instrument Library"));
	}
}


bool CModTree::GetDropInfo(DRAGONDROP &dropInfo, mpt::PathString &fullPath)
{
	const auto dragDoc = m_docInfo.find(m_dragDoc);
	dropInfo.sndFile = dragDoc != m_docInfo.end() ? &dragDoc->second.modDoc.GetSoundFile() : nullptr;
	dropInfo.dropType = DRAGONDROP_NOTHING;
	dropInfo.dropItem = m_itemDrag.val1;
	dropInfo.dropParam = 0;
	switch(m_itemDrag.type)
	{
	case MODITEM_ORDER:
		dropInfo.dropType = DRAGONDROP_ORDER;
		break;

	case MODITEM_PATTERN:
		dropInfo.dropType = DRAGONDROP_PATTERN;
		break;

	case MODITEM_SAMPLE:
		dropInfo.dropType = DRAGONDROP_SAMPLE;
		break;

	case MODITEM_INSTRUMENT:
		dropInfo.dropType = DRAGONDROP_INSTRUMENT;
		break;

	case MODITEM_SEQUENCE:
	case MODITEM_HDR_ORDERS:
		dropInfo.dropType = DRAGONDROP_SEQUENCE;
		break;

	case MODITEM_INSLIB_SAMPLE:
	case MODITEM_INSLIB_INSTRUMENT:
		if(!m_SongFileName.empty())
		{
			const uint32 n = mpt::parse<uint32>(GetItemText(m_hItemDrag));
			dropInfo.dropType = (m_itemDrag.type == MODITEM_INSLIB_SAMPLE) ? DRAGONDROP_SAMPLE : DRAGONDROP_INSTRUMENT;
			dropInfo.dropItem = n;
			dropInfo.sndFile = m_SongFile;
			dropInfo.dropParam = 0;
		} else
		{
			fullPath = InsLibGetFullPath(m_hItemDrag);
			dropInfo.dropType = DRAGONDROP_SOUNDFILE;
			dropInfo.dropParam = reinterpret_cast<uintptr_t>(&fullPath);
		}
		break;

	case MODITEM_MIDIPERCUSSION:
		dropInfo.dropItem |= 0x80;
		[[fallthrough]];
	case MODITEM_MIDIINSTRUMENT:
		if(dropInfo.dropItem < CTrackApp::GetMidiLibrary().size())
		{
			const auto &libItem = CTrackApp::GetMidiLibrary()[dropInfo.dropItem];
			if(libItem && !libItem->empty())
			{
				fullPath = *libItem;
				dropInfo.dropType = DRAGONDROP_MIDIINSTR;
				dropInfo.dropParam = reinterpret_cast<uintptr_t>(&fullPath);
			}
		}
		break;

	case MODITEM_INSLIB_SONG:
		fullPath = InsLibGetFullPath(m_hItemDrag);
		dropInfo.sndFile = nullptr;
		dropInfo.dropType = DRAGONDROP_SONG;
		dropInfo.dropItem = 0;
		dropInfo.dropParam = reinterpret_cast<uintptr_t>(&fullPath);
		break;

	case MODITEM_DLSBANK_INSTRUMENT:
		{
			dropInfo.dropType = DRAGONDROP_DLS;
			dropInfo.dropItem = static_cast<uint32>(GetDLSBankIndexFromItem(m_hItemDrag));  // bank #
			// Melodic: (Instrument)
			// Drums:   (0x80000000) | (Region << 16) | (Instrument)
			dropInfo.dropParam = m_itemDrag.val1;
		}
		break;

	default:
		break;
	}
	return (dropInfo.dropType != DRAGONDROP_NOTHING);
}


bool CModTree::CanDrop(TreeItemHandle hItem, bool doDrop)
{
	const ModItem modItemDrop = GetModItem(hItem);
	const uint32 modItemDropID = modItemDrop.val1;
	const uint32 modItemDragID = m_itemDrag.val1;

	const auto dragIter = m_docInfo.find(m_dragDoc);
	const auto selIter = m_docInfo.find(m_selectedDoc);
	const ModTreeDocInfo *infoDrag = dragIter != m_docInfo.end() ? &dragIter->second : nullptr;
	const ModTreeDocInfo *infoDrop = selIter != m_docInfo.end() ? &selIter->second : nullptr;
	CModDoc *modDoc = infoDrop ? &infoDrop->modDoc : nullptr;
	CSoundFile *sndFile = modDoc ? &modDoc->GetSoundFile() : nullptr;
	const bool sameModDoc = infoDrag && (modDoc == &infoDrag->modDoc);
	const bool sameItem = modItemDrop == m_itemDrag && sameModDoc;

	switch(modItemDrop.type)
	{
	case MODITEM_ORDER:
	case MODITEM_SEQUENCE:
		if(m_itemDrag.type == MODITEM_ORDER && modDoc && sameModDoc)
		{
			// Drop an order somewhere
			if(doDrop)
			{
				SEQUENCEINDEX seqFrom = static_cast<SEQUENCEINDEX>(m_itemDrag.val2), seqTo = static_cast<SEQUENCEINDEX>(modItemDrop.val2);
				ORDERINDEX ordFrom = static_cast<ORDERINDEX>(m_itemDrag.val1), ordTo = static_cast<ORDERINDEX>(modItemDrop.val1);
				if(modItemDrop.type == MODITEM_SEQUENCE)
				{
					// Drop on sequence -> attach
					seqTo = static_cast<SEQUENCEINDEX>(modItemDrop.val1);
					ordTo = sndFile->Order(seqTo).GetLengthTailTrimmed();
				}

				if(seqFrom != seqTo || ordFrom != ordTo)
				{
					if(modDoc->MoveOrder(ordFrom, ordTo, true, false, seqFrom, seqTo) == true)
					{
						modDoc->SetModified();
					}
				}
			}
			return true;
		} else if(modItemDrop.type == MODITEM_SEQUENCE && m_itemDrag.type == MODITEM_SEQUENCE && modDoc && sameModDoc)
		{
			if(doDrop && !sameItem)
			{
				// Rearrange sequences
				const SEQUENCEINDEX from = static_cast<SEQUENCEINDEX>(modItemDragID), to = static_cast<SEQUENCEINDEX>(modItemDropID);

				std::vector<SEQUENCEINDEX> newOrder(sndFile->Order.GetNumSequences());
				std::iota(newOrder.begin(), newOrder.end(), SEQUENCEINDEX(0));
				newOrder.erase(newOrder.begin() + from);
				newOrder.insert(newOrder.begin() + to, from);

				modDoc->ReArrangeSequences(newOrder);

				auto curSeq = sndFile->Order.GetCurrentSequenceIndex();
				if(curSeq == from)
					curSeq = to;
				else if(from > curSeq && to <= curSeq)
					curSeq++;
				else if(from < curSeq && to >= curSeq)
					curSeq--;
				sndFile->Order.SetSequence(curSeq);

				modDoc->UpdateAllViews(nullptr, SequenceHint(SEQUENCEINDEX_INVALID).Names().Data());
				modDoc->SetModified();

				SelectItem(hItem);
			}
			return true;
		}
		break;

	case MODITEM_HDR_ORDERS:
		// Drop your sequences here.
		// At the moment, only dropping sequences into another module is possible and it doesn't copy the patterns themselves.
		if((m_itemDrag.type == MODITEM_SEQUENCE || m_itemDrag.type == MODITEM_HDR_ORDERS) && modDoc && sndFile && infoDrag && !sameModDoc)
		{
			if(doDrop && infoDrag != nullptr)
			{
				// copy mod sequence over.
				CSoundFile &dragSndFile = infoDrag->modDoc.GetSoundFile();
				const SEQUENCEINDEX origSeqId = static_cast<SEQUENCEINDEX>(modItemDragID);
				const ModSequence &origSeq = dragSndFile.Order(origSeqId);
				SEQUENCEINDEX sequenceHint = SEQUENCEINDEX_INVALID;

				if(sndFile->GetModSpecifications().sequencesMax > 1)
				{
					sequenceHint = sndFile->Order.AddSequence();
				} else
				{
					if(Reporting::Confirm(UL_("Replace the current orderlist?"), UL_("Sequence import")) == cnfNo)
						return false;
					sequenceHint = 0;
				}
				sndFile->Order().resize(std::min(sndFile->GetModSpecifications().ordersMax, origSeq.GetLength()), PATTERNINDEX_INVALID);
				for(ORDERINDEX nOrd = 0; nOrd < std::min(sndFile->GetModSpecifications().ordersMax, origSeq.GetLengthTailTrimmed()); nOrd++)
				{
					PATTERNINDEX pat = dragSndFile.Order(origSeqId)[nOrd];
					// translate pattern index
					if(pat == PATTERNINDEX_SKIP && sndFile->GetModSpecifications().hasIgnoreIndex)
						pat = PATTERNINDEX_SKIP;
					else if(pat >= sndFile->GetModSpecifications().patternsMax)
						pat = PATTERNINDEX_INVALID;
					
					sndFile->Order()[nOrd] = pat;
				}
				modDoc->UpdateAllViews(nullptr, SequenceHint(sequenceHint).Data());
				modDoc->SetModified();
			}
			return true;
		}
		break;

	case MODITEM_SAMPLE:
		if(m_itemDrag.type == MODITEM_SAMPLE && modDoc && infoDrag != nullptr)
		{
			if(doDrop)
			{
				if(sameModDoc)
				{
					// Reorder samples in a module
					if(sameItem)
						return true;
					const SAMPLEINDEX from = static_cast<SAMPLEINDEX>(modItemDragID - 1), to = static_cast<SAMPLEINDEX>(modItemDropID - 1);

					std::vector<SAMPLEINDEX> newOrder(modDoc->GetNumSamples());
					std::iota(newOrder.begin(), newOrder.end(), SAMPLEINDEX(1));
					newOrder.erase(newOrder.begin() + from);
					newOrder.insert(newOrder.begin() + to, from + 1);

					modDoc->ReArrangeSamples(newOrder);
				} else
				{
					// Load sample into other module
					sndFile->ReadSampleFromSong(static_cast<SAMPLEINDEX>(modItemDropID), infoDrag->modDoc.GetSoundFile(), static_cast<SAMPLEINDEX>(modItemDragID));
				}
				modDoc->UpdateAllViews(nullptr, SampleHint().Info().Data().Names());
				modDoc->UpdateAllViews(nullptr, PatternHint().Data());
				modDoc->UpdateAllViews(nullptr, InstrumentHint().Info());
				modDoc->SetModified();
				SelectItem(hItem);
			}
			return true;
		}
		break;

	case MODITEM_INSTRUMENT:
		if(m_itemDrag.type == MODITEM_INSTRUMENT && modDoc && infoDrag != nullptr)
		{
			if(doDrop)
			{
				if(sameModDoc)
				{
					// Reorder instruments in a module
					if(sameItem)
						return true;
					const INSTRUMENTINDEX from = static_cast<INSTRUMENTINDEX>(modItemDragID - 1), to = static_cast<INSTRUMENTINDEX>(modItemDropID - 1);

					std::vector<INSTRUMENTINDEX> newOrder(modDoc->GetNumInstruments());
					std::iota(newOrder.begin(), newOrder.end(), INSTRUMENTINDEX(1));
					newOrder.erase(newOrder.begin() + from);
					newOrder.insert(newOrder.begin() + to, from + 1);

					modDoc->ReArrangeInstruments(newOrder);
				} else
				{
					// Load instrument into other module
					if(sndFile->ReadInstrumentFromSong(static_cast<INSTRUMENTINDEX>(modItemDropID), infoDrag->modDoc.GetSoundFile(), static_cast<INSTRUMENTINDEX>(modItemDragID)))
					{
						if(sndFile->Instruments[modItemDropID] && sndFile->Instruments[modItemDropID]->pTuning)
							modDoc->UpdateAllViews(nullptr, GeneralHint().Tunings());
					}
				}
				modDoc->UpdateAllViews(nullptr, InstrumentHint().Info().Envelope().Names());
				modDoc->UpdateAllViews(nullptr, PatternHint().Data());
				modDoc->SetModified();
				SelectItem(hItem);
			}
			return true;
		}
		break;

	case MODITEM_MIDIINSTRUMENT:
	case MODITEM_MIDIPERCUSSION:
		if((m_itemDrag.type == MODITEM_INSLIB_SAMPLE) || (m_itemDrag.type == MODITEM_INSLIB_INSTRUMENT))
		{
			if(doDrop)
			{
				mpt::PathString fullPath = InsLibGetFullPath(m_hItemDrag);
				if(modItemDrop.type == MODITEM_MIDIINSTRUMENT)
					SetMidiInstrument(modItemDropID, fullPath);
				else
					SetMidiPercussion(modItemDropID, fullPath);
			}
			return true;
		}
		break;

	default:
		break;
	}
	return false;
}


void CModTree::UpdatePlayPos(CModDoc &modDoc, Notification *pNotify)
{
	ModTreeDocInfo *info = GetDocumentInfoFromModDoc(modDoc);
	if(info == nullptr)
		return;

	const CSoundFile &sndFile = modDoc.GetSoundFile();
	ORDERINDEX nNewOrd = (pNotify) ? pNotify->order : ORDERINDEX_INVALID;
	SEQUENCEINDEX nNewSeq = sndFile.Order.GetCurrentSequenceIndex();
	if(nNewOrd != info->ordSel || nNewSeq != info->seqSel)
	{
		// Remove bold state from old item
		if(info->seqSel < info->tiOrders.size() && info->ordSel < info->tiOrders[info->seqSel].size())
			SetItemState(info->tiOrders[info->seqSel][info->ordSel], 0, ui::TreeStateBold);

		info->ordSel = nNewOrd;
		info->seqSel = nNewSeq;
		if(info->seqSel < info->tiOrders.size() && info->ordSel < info->tiOrders[info->seqSel].size())
			SetItemState(info->tiOrders[info->seqSel][info->ordSel], ui::TreeStateBold, ui::TreeStateBold);
		else
			UpdateView(*info, SequenceHint().Data());
	}

	// Update sample / instrument playing status icons (will only detect instruments with samples, though)

	if(!(TrackerSettings::Instance().patternSetup & PatternSetup::LiveUpdateTreeView))
		return;
	// TODO: Is there a way to find out if the treeview is actually visible?
	/*static int nUpdateCount = 0;
	nUpdateCount++;
	if(nUpdateCount < 5) return; // don't update too often
	nUpdateCount = 0;*/

	// check whether the lists are actually visible (don't waste resources)
	const bool updateSamples = IsItemExpanded(info->hSamples), updateInstruments = IsItemExpanded(info->hInstruments);

	info->samplesPlaying.reset();
	info->instrumentsPlaying.reset();

	if(!updateSamples && !updateInstruments)
		return;

	for(const auto &chn : sndFile.m_PlayState.Chn)
	{
		if(chn.pCurrentSample != nullptr && chn.nLength != 0 && chn.IsSamplePlaying())
		{
			if(updateSamples)
			{
				for(SAMPLEINDEX nSmp = sndFile.GetNumSamples(); nSmp >= 1; nSmp--)
				{
					if(chn.pModSample == &sndFile.GetSample(nSmp))
					{
						info->samplesPlaying.set(nSmp);
						break;
					}
				}
			}
			if(updateInstruments)
			{
				for(INSTRUMENTINDEX nIns = sndFile.GetNumInstruments(); nIns >= 1; nIns--)
				{
					if(chn.pModInstrument == sndFile.Instruments[nIns])
					{
						info->instrumentsPlaying.set(nIns);
						break;
					}
				}
			}
		}
	}
	// what should be updated?
	if(updateSamples)
		UpdateView(*info, SampleHint().Info());
	if(updateInstruments)
		UpdateView(*info, InstrumentHint().Info());
}



/////////////////////////////////////////////////////////////////////////////
// CViewModTree message handlers


void CModTree::OnUpdate(CModDoc *pModDoc, UpdateHint hint, HintObject *pHint)
{
	if(pHint == this)
		return;

	for(auto &[doc, docInfo] : m_docInfo)
	{
		if(doc == pModDoc || !pModDoc)
		{
			UpdateView(docInfo, hint);
			if(pModDoc)
				break;
		}
	}
}

void CModTree::OnTreeItemExpanded(NotifyHeader *pnmhdr, LResult *pResult)
{
	const TreeItemHandle item = static_cast<const ui::TreeNotification *>(pnmhdr->extra)->item;
	int image = 0, selectedImage = 0;
	if(GetItemImage(item, image, selectedImage) && (image == IMAGE_FOLDER || image == IMAGE_OPENFOLDER))
	{
		const int newImage = GetItemState(item, ui::TreeStateExpanded) ? IMAGE_OPENFOLDER : IMAGE_FOLDER;
		SetItemImage(item, newImage, newImage);
	}
	if(pResult)
		*pResult = true;
}


void CModTree::OnBeginDrag(TreeItemHandle hItem, bool bLeft, LResult *pResult)
{
	if(!(m_dwStatus & TREESTATUS_DRAGGING))
	{
		bool bDrag = false;

		m_hDropWnd = NULL;
		m_hItemDrag = hItem;
		if(m_hItemDrag != NULL)
		{
			if(!ItemHasChildren(m_hItemDrag))
				SelectItem(m_hItemDrag);
		}
		m_itemDrag = GetModItem(m_hItemDrag);
		m_dragDoc = m_selectedDoc;
		switch(m_itemDrag.type)
		{
		case MODITEM_ORDER:
		case MODITEM_PATTERN:
		case MODITEM_SAMPLE:
		case MODITEM_INSTRUMENT:
		case MODITEM_SEQUENCE:
		case MODITEM_MIDIINSTRUMENT:
		case MODITEM_MIDIPERCUSSION:
		case MODITEM_INSLIB_SAMPLE:
		case MODITEM_INSLIB_INSTRUMENT:
		case MODITEM_INSLIB_SONG:
			bDrag = true;
			break;
		case MODITEM_HDR_ORDERS:
			// can we drag an order header? (only in MPTM format and if there's only one sequence)
			{
				const CModDoc *pModDoc = m_docInfo.count(m_dragDoc) ? m_dragDoc : nullptr;
				if(pModDoc && pModDoc->GetSoundFile().Order.GetNumSequences() == 1)
					bDrag = true;
			}
			break;
		default:
			if(m_itemDrag.type == MODITEM_DLSBANK_INSTRUMENT)
				bDrag = true;
		}
		if(bDrag)
		{
			m_dwStatus |= (bLeft) ? TREESTATUS_LDRAG : TREESTATUS_RDRAG;
			m_hItemDrop = NULL;
			SetCapture();
		}
	}
	if(pResult)
		*pResult = true;
}


void CModTree::OnBeginRDrag(NotifyHeader * pnmhdr, LResult *pResult)
{
	if(pnmhdr)
	{
		OnBeginDrag(static_cast<const ui::TreeNotification *>(pnmhdr->extra)->item, false, pResult);
	}
}


void CModTree::OnBeginLDrag(NotifyHeader * pnmhdr, LResult *pResult)
{
	if(pnmhdr)
	{
		OnBeginDrag(static_cast<const ui::TreeNotification *>(pnmhdr->extra)->item, true, pResult);
	}
}


void CModTree::OnItemDblClk(NotifyHeader *, LResult *pResult)
{
	POINT pt;
	GetCursorPos(&pt);
	ScreenToClient(&pt);
	TreeItemHandle hItem = GetSelectedItem();
	if((hItem) && (hItem == HitTest(pt)))
	{
		ExecuteItem(hItem);
	}
	if(pResult)
		*pResult = 0;
}


void CModTree::OnItemReturn(NotifyHeader *, LResult *pResult)
{
	TreeItemHandle hItem = GetSelectedItem();
	if(hItem)
		ExecuteItem(hItem);
	if(pResult)
		*pResult = 0;
}


void CModTree::OnTreeRightClick(NotifyHeader *, LResult *pResult)
{
	Point pt, ptClient;
	uint32 flags = 0;

	GetCursorPos(&pt);
	ptClient = pt;
	ScreenToClient(&ptClient);
	OnItemRightClick(HitTest(ptClient, &flags), pt);
	if(pResult)
		*pResult = 0;
}


void CModTree::OnItemRightClick(TreeItemHandle hItem, Point pt)
{
	if(m_dwStatus & TREESTATUS_LDRAG)
	{
		if(ItemHasChildren(hItem))
		{
			Expand(hItem, ui::TreeToggle);
		} else
		{
			m_hItemDrop = NULL;
			m_hDropWnd = NULL;
			OnEndDrag(TREESTATUS_DRAGGING);
		}
	} else
	{
		if(m_dwStatus & TREESTATUS_DRAGGING)
		{
			m_hItemDrop = NULL;
			m_hDropWnd = NULL;
			OnEndDrag(TREESTATUS_DRAGGING);
		}
		HMENU hMenu = ::CreatePopupMenu(), hSubMenu = nullptr;
		if(!hMenu)
			return;

		const CModDoc *modDoc = GetDocumentFromItem(hItem);
		const CSoundFile *sndFile = modDoc != nullptr ? &modDoc->GetSoundFile() : nullptr;
		const CInputHandler *ih = CMainFrame::GetInputHandler();

		uint32 defaultID = 0;
		bool addSeparator = false;

		const ModItem modItem = GetModItem(hItem);
		const uint32 modItemID = modItem.val1;

		SelectItem(hItem);
		switch(modItem.type)
		{
		case MODITEM_HDR_SONG:
			defaultID = ID_MODTREE_EXECUTE;
			AppendMenu(hMenu, ui::MenuItemString, defaultID, ih->GetKeyTextFromCommand(kcTreeViewOpen, UL_("&View")));
			AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_CLOSE, UL_("&Close"));
			AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_RENAME, ih->GetKeyTextFromCommand(kcTreeViewRename, UL_("Re&name")));
			AppendMenu(hMenu, ui::MenuItemString | ((!modDoc || modDoc->GetPathNameMpt().empty()) ? ui::MenuItemGrayed : 0), ID_MODTREE_OPENITEM, UL_("&Open in Explorer"));
			break;

		case MODITEM_COMMENTS:
			defaultID = ID_MODTREE_EXECUTE;
			AppendMenu(hMenu, ui::MenuItemString, defaultID, ih->GetKeyTextFromCommand(kcTreeViewOpen, UL_("&View Comments")));
			break;

		case MODITEM_ORDER:
		case MODITEM_PATTERN:
			defaultID = ID_MODTREE_EXECUTE;
			AppendMenu(hMenu, ui::MenuItemString, defaultID, ih->GetKeyTextFromCommand(kcTreeViewOpen, UL_("&Edit Pattern")));
			AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_REMOVE,
							ih->GetKeyTextFromCommand(kcTreeViewDelete, (modItem.type == MODITEM_ORDER) ? UL_("&Delete from list") : UL_("&Delete Pattern")));
			if(modItem.type == MODITEM_PATTERN && sndFile && sndFile->GetModSpecifications().hasPatternNames)
				AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_RENAME, ih->GetKeyTextFromCommand(kcTreeViewRename, UL_("Re&name Pattern")));
			else if(modItem.type == MODITEM_ORDER)
				AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_RENAME, ih->GetKeyTextFromCommand(kcTreeViewRename, UL_("&Set Pattern")));
			break;

		case MODITEM_SEQUENCE:
			if(sndFile)
			{
				bool isCurSeq = false;
				if(sndFile->GetModSpecifications().sequencesMax > 1)
				{
					if(sndFile->Order((SEQUENCEINDEX)modItemID).GetLength() == 0)
					{
						defaultID = ID_MODTREE_SWITCHTO;
					}
					isCurSeq = (sndFile->Order.GetCurrentSequenceIndex() == (SEQUENCEINDEX)modItemID);
				}

				if(!isCurSeq)
				{
					AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_SWITCHTO, UL_("&Switch to Seqeuence"));
				}
				AppendMenu(hMenu, ui::MenuItemString | (sndFile->Order.GetNumSequences() < MAX_SEQUENCES ? 0 : ui::MenuItemGrayed), ID_MODTREE_INSERT, ih->GetKeyTextFromCommand(kcTreeViewInsert, UL_("&Insert Sequence")));
				AppendMenu(hMenu, ui::MenuItemString | (sndFile->Order.GetNumSequences() < MAX_SEQUENCES ? 0 : ui::MenuItemGrayed), ID_MODTREE_DUPLICATE , ih->GetKeyTextFromCommand(kcTreeViewDuplicate,  UL_("D&uplicate Sequence")));
				AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_REMOVE, ih->GetKeyTextFromCommand(kcTreeViewDelete, UL_("&Delete Sequence")));
				AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_RENAME, ih->GetKeyTextFromCommand(kcTreeViewRename, UL_("Re&name Sequence")));
			}
			break;


		case MODITEM_HDR_ORDERS:
			if(sndFile && sndFile->GetModSpecifications().sequencesMax > 1)
			{
				AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_INSERT, ih->GetKeyTextFromCommand(kcTreeViewInsert, UL_("&Insert Sequence")));
				if(sndFile->Order.GetNumSequences() == 1)
				{
					// This is a sequence
					AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_DUPLICATE, ih->GetKeyTextFromCommand(kcTreeViewDuplicate, UL_("D&uplicate Sequence")));
					AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_RENAME, ih->GetKeyTextFromCommand(kcTreeViewRename, UL_("Re&name Sequence")));
				}
			}
			break;

		case MODITEM_SAMPLE:
			defaultID = ID_MODTREE_EXECUTE;
			AppendMenu(hMenu, ui::MenuItemString, defaultID, ih->GetKeyTextFromCommand(kcTreeViewOpen, UL_("&View Sample")));
			AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_PLAY, ih->GetKeyTextFromCommand(kcTreeViewPlay, UL_("&Play Sample")));
			AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_INSERT, ih->GetKeyTextFromCommand(kcTreeViewInsert, UL_("&Insert Sample")));
			AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_DUPLICATE, ih->GetKeyTextFromCommand(kcTreeViewDuplicate, UL_("D&uplicate Sample")));
			AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_REMOVE, ih->GetKeyTextFromCommand(kcTreeViewDelete, UL_("&Delete Sample")));
			AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_RENAME, ih->GetKeyTextFromCommand(kcTreeViewRename, UL_("Re&name Sample")));
			if(modDoc && !modDoc->GetNumInstruments())
			{
				AppendMenu(hMenu, ui::MenuItemSeparator, 0, UL_(""));
				AppendMenu(hMenu, (modDoc->IsSampleMuted((SAMPLEINDEX)modItemID) ? ui::MenuItemChecked : 0) | ui::MenuItemString, ID_MODTREE_MUTE, UL_("&Mute Sample"));
				AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_SOLO, UL_("S&olo Sample"));
				AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_UNMUTEALL, UL_("&Unmute all"));
			}
			if(sndFile != nullptr)
			{
				SAMPLEINDEX smpID = static_cast<SAMPLEINDEX>(modItem.val1);
				const ModSample &sample = sndFile->GetSample(smpID);
				const bool hasPath = sndFile->SampleHasPath(smpID);
				const bool menuForThisSample = (sample.HasSampleData() && sndFile->GetType() == MOD_TYPE_MPT) || hasPath;

				bool anyPath = false, anyModified = false, anyMissing = false;
				for(SAMPLEINDEX smp = 1; smp <= sndFile->GetNumSamples(); smp++)
				{
					if(sndFile->SampleHasPath(smp) && smp != smpID)
					{
						anyPath = true;
						if(sndFile->GetSample(smp).HasSampleData() && sndFile->GetSample(smp).uFlags[SMP_MODIFIED])
						{
							anyModified = true;
						}
					}
					if(sndFile->IsExternalSampleMissing(smp))
					{
						anyMissing = true;
					}
					if(anyPath && anyModified && anyMissing) break;
				}

				if(menuForThisSample || anyPath || anyModified)
				{
					AppendMenu(hMenu, ui::MenuItemSeparator, 0, UL_(""));
					if(menuForThisSample) AppendMenu(hMenu, ui::MenuItemString | ((sndFile->GetType() == MOD_TYPE_MPT || hasPath) ? 0 : ui::MenuItemGrayed), ID_MODTREE_SETPATH, UL_("Set P&ath"));
					if(menuForThisSample) AppendMenu(hMenu, ui::MenuItemString | ((hasPath && sample.HasSampleData() && sample.uFlags[SMP_MODIFIED]) ? 0 : ui::MenuItemGrayed), ID_MODTREE_SAVEITEM, UL_("&Save"));
					if(anyModified) AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_SAVEALL, UL_("&Save All"));
					if(menuForThisSample) AppendMenu(hMenu, ui::MenuItemString | (hasPath ? 0 : ui::MenuItemGrayed), ID_MODTREE_RELOADITEM, UL_("&Reload"));
					if(anyPath) AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_RELOADALL, UL_("&Reload All"));
					if(anyMissing) AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_FINDMISSING, UL_("&Find Missing Samples"));
				}
			}
			break;

		case MODITEM_INSTRUMENT:
			defaultID = ID_MODTREE_EXECUTE;
			AppendMenu(hMenu, ui::MenuItemString, defaultID, ih->GetKeyTextFromCommand(kcTreeViewOpen, UL_("&View Instrument")));
			AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_PLAY, ih->GetKeyTextFromCommand(kcTreeViewPlay, UL_("&Play Instrument")));
			AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_INSERT, ih->GetKeyTextFromCommand(kcTreeViewInsert, UL_("&Insert Instrument")));
			AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_DUPLICATE, ih->GetKeyTextFromCommand(kcTreeViewDuplicate, UL_("D&uplicate Instrument")));
			AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_REMOVE, ih->GetKeyTextFromCommand(kcTreeViewDelete, UL_("&Delete Instrument")));
			AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_RENAME, ih->GetKeyTextFromCommand(kcTreeViewRename, UL_("Re&name Instrument")));
			if(modDoc)
			{
				AppendMenu(hMenu, ui::MenuItemSeparator, 0, UL_(""));
				AppendMenu(hMenu, (modDoc->IsInstrumentMuted((INSTRUMENTINDEX)modItemID) ? ui::MenuItemChecked : 0) | ui::MenuItemString, ID_MODTREE_MUTE, UL_("&Mute Instrument"));
				AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_SOLO, UL_("S&olo Instrument"));
				AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_UNMUTEALL, UL_("&Unmute all"));
			}
			break;

		case MODITEM_HDR_EFFECTS:
			if(sndFile && sndFile->m_loadedPlugins)
			{
				AppendMenu(hMenu, ui::MenuItemString | (AllPluginsBypassed(*sndFile, false) ? ui::MenuItemChecked : 0), ID_MODTREE_MUTE, UL_("B&ypass All Plugins"));
				if(HasEffectPlugins(*sndFile))
					AppendMenu(hMenu, ui::MenuItemString | (AllPluginsBypassed(*sndFile, true) ? ui::MenuItemChecked : 0), ID_MODTREE_MUTE_ONLY_EFFECTS, UL_("Bypass All &Effects"));
			}
			break;

		case MODITEM_EFFECT:
			defaultID = ID_MODTREE_EXECUTE;
			AppendMenu(hMenu, ui::MenuItemString, defaultID, ih->GetKeyTextFromCommand(kcTreeViewOpen, UL_("&Edit")));

			if(modDoc != nullptr)
			{
				AppendMenu(hMenu, (modDoc->GetSoundFile().m_MixPlugins[modItemID].IsBypassed() ? ui::MenuItemChecked : 0) | ui::MenuItemString, ID_MODTREE_MUTE, UL_("&Bypass"));
				AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_REMOVE, ih->GetKeyTextFromCommand(kcTreeViewDelete, UL_("&Delete Plugin")));
			}
			break;

		case MODITEM_MIDIINSTRUMENT:
		case MODITEM_MIDIPERCUSSION:
			defaultID = ID_MODTREE_EXECUTE;
			AppendMenu(hMenu, ui::MenuItemString, defaultID, ih->GetKeyTextFromCommand(kcTreeViewOpen, UL_("&Map Instrument")));
			AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_PLAY, ih->GetKeyTextFromCommand(kcTreeViewPlay, UL_("&Play Instrument")));
			AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_REMOVE, UL_("&Unmap Instrument"));
			AppendMenu(hMenu, ui::MenuItemSeparator, 0, UL_(""));
			[[fallthrough]];
		case MODITEM_HDR_MIDILIB:
		case MODITEM_HDR_MIDIGROUP:
			AppendMenu(hMenu, ui::MenuItemString, ID_IMPORT_MIDILIB, UL_("&Import MIDI Library"));
			AppendMenu(hMenu, ui::MenuItemString, ID_EXPORT_MIDILIB, UL_("E&xport MIDI Library"));
			addSeparator = true;
			break;

		case MODITEM_HDR_INSTRUMENTLIB:
			if(!IsSampleBrowser())
			{
				hSubMenu = AddLibraryFindAndSortMenus(hMenu);
				break;
			}
			if(!m_SongFileName.empty())
				AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_CLOSE, UL_("&Close Song"));
			[[fallthrough]];
		case MODITEM_INSLIB_FOLDER:
			defaultID = ID_MODTREE_EXECUTE;
			AppendMenu(hMenu, ui::MenuItemString, defaultID, ih->GetKeyTextFromCommand(kcTreeViewOpen, UL_("&Browse...")));
			AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_RENAME, ih->GetKeyTextFromCommand(kcTreeViewRename, UL_("Set &Path")));
			AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_OPENITEM, UL_("&Open in Explorer"));
			hSubMenu = AddLibraryFindAndSortMenus(hMenu);

			{
				auto insDir = TrackerSettings::Instance().PathInstruments.GetDefaultDir();
				auto smpDir = TrackerSettings::Instance().PathSamples.GetDefaultDir();
				if(!insDir.empty() && insDir != m_InstrLibPath)
					AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_GOTO_INSDIR, UL_("Go to &Instrument directory"));
				if(!smpDir.empty() && smpDir != insDir && smpDir != m_InstrLibPath)
					AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_GOTO_SMPDIR, UL_("Go to Sa&mple directory"));
			}
			break;

		case MODITEM_INSLIB_SONG:
			defaultID = ID_MODTREE_EXECUTE;
			AppendMenu(hMenu, ui::MenuItemString, defaultID, ih->GetKeyTextFromCommand(kcTreeViewOpen, UL_("&Browse Song...")));
			AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_OPENITEM, UL_("&Edit Song"));
			hSubMenu = AddLibraryFindAndSortMenus(hMenu);
			AppendMenu(hMenu, ui::MenuItemSeparator, 0, UL_(""));
			AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_REMOVE, ih->GetKeyTextFromCommand(kcTreeViewDelete, UL_("&Delete")));
			break;

		case MODITEM_INSLIB_SAMPLE:
		case MODITEM_INSLIB_INSTRUMENT:
			defaultID = ID_MODTREE_PLAY;
			if(!m_SongFileName.empty())
			{
				AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_PLAY, ih->GetKeyTextFromCommand(kcTreeViewPlay, UL_("&Play")));
			} else
			{
				AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_PLAY, ih->GetKeyTextFromCommand(kcTreeViewPlay, UL_("&Play File")));
				AppendMenu(hMenu, ui::MenuItemSeparator, 0, UL_(""));
				AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_OPENITEM, UL_("&Open in Explorer"));
				AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_REMOVE, ih->GetKeyTextFromCommand(kcTreeViewDelete, UL_("&Delete")));
			}
			hSubMenu = AddLibraryFindAndSortMenus(hMenu);
			break;

		case MODITEM_DLSBANK_FOLDER:
			defaultID = ID_SOUNDBANK_PROPERTIES;
			AppendMenu(hMenu, ui::MenuItemString, defaultID, UL_("&Properties"));
			AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_REMOVE, UL_("Re&move this bank"));
			[[fallthrough]];
		case MODITEM_NULL:
			AppendMenu(hMenu, ui::MenuItemString, ID_ADD_SOUNDBANK, UL_("Add Sound &Bank..."));
			addSeparator = true;
			break;

		case MODITEM_DLSBANK_INSTRUMENT:
			defaultID = ID_MODTREE_PLAY;
			AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_PLAY, ih->GetKeyTextFromCommand(kcTreeViewPlay, UL_("&Play Instrument")));
			break;

		default:
			break;
		}

		if(defaultID)
			SetMenuDefaultItem(hMenu, defaultID, false);
		
		if((modItem.type == MODITEM_INSLIB_FOLDER)
			|| (modItem.type == MODITEM_INSLIB_SONG)
			|| (modItem.type == MODITEM_HDR_INSTRUMENTLIB))
		{
			if(addSeparator || defaultID)
				AppendMenu(hMenu, ui::MenuItemSeparator, 0, UL_(""));
			AppendMenu(hMenu, TrackerSettings::Instance().showDirsInSampleBrowser ? (ui::MenuItemString|ui::MenuItemChecked) : ui::MenuItemString, ID_MODTREE_SHOWDIRS, UL_("Show &Directories in Sample Browser"));
			AppendMenu(hMenu, (m_showAllFiles) ? (ui::MenuItemString|ui::MenuItemChecked) : ui::MenuItemString, ID_MODTREE_SHOWALLFILES, UL_("Show &All Files"));
			AppendMenu(hMenu, (m_showAllFiles) ? ui::MenuItemString : (ui::MenuItemString|ui::MenuItemChecked), ID_MODTREE_SOUNDFILESONLY, UL_("Show &Sound Files"));
			addSeparator = true;
		}

		if(addSeparator || defaultID)
			AppendMenu(hMenu, ui::MenuItemSeparator, 0, UL_(""));
		AppendMenu(hMenu, ui::MenuItemString, ID_MODTREE_REFRESH, UL_("&Refresh"));

		TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x + 4, pt.y, 0, this, NULL);
		DestroyMenu(hMenu);
		if(hSubMenu)
			DestroyMenu(hSubMenu);
	}
}


HMENU CModTree::AddLibraryFindAndSortMenus(HMENU hMenu) const
{
	const CInputHandler *ih = CMainFrame::GetInputHandler();
	AppendMenu(hMenu, ui::MenuItemString, ID_OPEN_LIBRARY_FILTER, ih->GetKeyTextFromCommand(kcTreeViewFind, UL_("&Find...")));

	HMENU hSubMenu = ::CreatePopupMenu();
	AppendMenu(hSubMenu, ui::MenuItemString | (m_librarySort == LibrarySortOrder::Name ? ui::MenuItemChecked : 0), ID_MODTREE_SORT_BY_NAME, ih->GetKeyTextFromCommand(kcTreeViewSortByName, UL_("&Name")));
	AppendMenu(hSubMenu, ui::MenuItemString | (m_librarySort == LibrarySortOrder::Date ? ui::MenuItemChecked : 0), ID_MODTREE_SORT_BY_DATE, ih->GetKeyTextFromCommand(kcTreeViewSortByDate, UL_("&Date")));
	AppendMenu(hSubMenu, ui::MenuItemString | (m_librarySort == LibrarySortOrder::Size ? ui::MenuItemChecked : 0), ID_MODTREE_SORT_BY_SIZE, ih->GetKeyTextFromCommand(kcTreeViewSortBySize, UL_("&Size")));
	AppendMenu(hMenu, ui::MenuItemPopup, reinterpret_cast<uintptr_t>(hSubMenu), UL_("Sort B&y"));
	return hSubMenu;
}


void CModTree::OnItemLeftClick(NotifyHeader *, LResult *pResult)
{
	if(!(m_dwStatus & TREESTATUS_RDRAG))
	{
		POINT pt;
		uint32 flags = 0;
		GetCursorPos(&pt);
		ScreenToClient(&pt);
		TreeItemHandle hItem = HitTest(pt, &flags);
		if(hItem != NULL)
		{
			const ModItem modItem = GetModItem(hItem);
			const uint32 modItemID = modItem.val1;

			switch(modItem.type)
			{
			case MODITEM_INSLIB_FOLDER:
			case MODITEM_INSLIB_SONG:
				if(m_dwStatus & TREESTATUS_SINGLEEXPAND)
					ExecuteItem(hItem);
				break;

			case MODITEM_SAMPLE:
			case MODITEM_INSTRUMENT:
				{
					CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
					ui::ChildFrameBase *pFrame = pMainFrm->MDIGetActive();
					if (pFrame)
					{
						pFrame->SendMessage(MSG_MOD_INSTRSELECTED,
							(modItem.type == MODITEM_INSTRUMENT) ? true : false,
							(LParam)modItemID);
					}
				}
				break;

			case MODITEM_HDR_SONG:
				ExecuteItem(hItem);
				break;

			default:
				break;
			}
		}
	}
	if(pResult)
		*pResult = 0;
}


void CModTree::OnSelChanged(NotifyHeader *, LResult *)
{
	if(m_redrawLockCount)
		return;
	TreeItemHandle hItem = GetSelectedItem();
	const auto type = GetModItem(hItem).type;
	switch(type)
	{
	case MODITEM_INSLIB_SONG:
	case MODITEM_INSLIB_SAMPLE:
	case MODITEM_INSLIB_INSTRUMENT:
		if(uint32 itemData = static_cast<uint32>(GetItemData(hItem)); itemData > 0 && itemData <= m_fileBrowserEntries.size())
		{
			const auto &entry = m_fileBrowserEntries[itemData - 1];
			m_HelpText = MPT_UFORMAT("Size: {}")(FormatFileSize(entry.size));
			if(!m_SongFile)
			{
				m_HelpText += MPT_UFORMAT(", last modified: {}")(Util::FormatLocalTime(static_cast<int64>(entry.modtime), "%d %b %Y, %H:%M:%S"));
			} else if(type != MODITEM_INSLIB_SAMPLE)
			{
				m_HelpText.clear();
			}
			CMainFrame::GetMainFrame()->SetHelpText(m_HelpText);
		}
		break;

	default:
		if(CMainFrame::GetMainFrame()->GetHelpText() == m_HelpText)
		{
			CMainFrame::GetMainFrame()->SetHelpText(UL_(""));
			m_HelpText.clear();
		}
		break;
	}
}


void CModTree::OnEndDrag(uint32 dwMask)
{
	if(m_dwStatus & dwMask)
	{
		m_dwStatus &= ~dwMask;
		if(!(m_dwStatus & TREESTATUS_DRAGGING))
		{
			ReleaseCapture();
			SetCursor(CMainFrame::curArrow);
			SelectDropTarget(nullptr);
			if(m_hItemDrop != nullptr)
			{
				CanDrop(m_hItemDrop, true);
			} else if(m_hDropWnd)
			{
				DRAGONDROP dropinfo;
				mpt::PathString fullPath;
				if(GetDropInfo(dropinfo, fullPath))
				{
					if(dropinfo.dropType == DRAGONDROP_SONG)
					{
						theApp.OpenDocumentFile(fullPath);
					} else
					{
						m_hDropWnd->SendMessage(MSG_MOD_DRAGONDROPPING, true, (LParam)&dropinfo);
					}
				}
			}
		}
	}
}


void CModTree::OnLButtonUp(uint32 nFlags, Point point)
{
	OnEndDrag(TREESTATUS_LDRAG);
	TreeCtrl::OnLButtonUp(nFlags, point);
}


void CModTree::OnRButtonUp(uint32 nFlags, Point point)
{
	OnEndDrag(TREESTATUS_RDRAG);
	TreeCtrl::OnRButtonUp(nFlags, point);
}


void CModTree::OnXButtonUp(uint32 nFlags, uint32 nButton, Point point)
{
	bool isSampleBrowser = IsSampleBrowser();
	if(!isSampleBrowser)
	{
		// In the upper panel, only do folder navigation if the mouse cursor is somewhere below the "Instrument Library" item
		Rect rect;
		GetItemRect(m_hInsLib, rect, false);
		if(point.y > rect.top)
			isSampleBrowser = true;
	}
	if(isSampleBrowser)
	{
		if(nButton == XBUTTON1)
		{
			InstrumentLibraryChDir(P_(".."), !m_SongFileName.empty());
		} else if(nButton == XBUTTON2)
		{
			const auto &previousPath = CMainFrame::GetMainFrame()->GetUpperTreeview()->m_previousPath;
			InstrumentLibraryChDir(previousPath, mpt::native_fs{}.is_file(m_InstrLibPath + previousPath));
		}
	}
	TreeCtrl::OnXButtonUp(nFlags, nButton, point);
}


void CModTree::OnMouseMove(uint32 nFlags, Point point)
{
	if(m_dwStatus & TREESTATUS_DRAGGING)
	{
		TreeItemHandle hItem;
		uint32 flags = 0;

		// Bug?
		if(!(nFlags & (ui::MouseLeft | ui::MouseRight)))
		{
			m_itemDrag = ModItem(MODITEM_NULL);
			m_hItemDrag = NULL;
			OnEndDrag(TREESTATUS_DRAGGING);
			return;
		}
		CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
		if(pMainFrm)
		{
			Rect rect;
			GetClientRect(&rect);
			if(rect.PtInRect(point))
			{
				m_hDropWnd = this;
				bool bCanDrop = CanDrop(HitTest(point, &flags), false);
				SetCursor((bCanDrop) ? CMainFrame::curDragging : CMainFrame::curNoDrop2);
			} else
			{
				Point screenPt = point;
				ClientToScreen(&screenPt);
				WindowHandle hwnd = ui::WindowFromPoint(screenPt);
				if(hwnd != m_hDropWnd)
				{
					bool canDrop = false;
					m_hDropWnd = hwnd;
					if(hwnd == this)
					{
						canDrop = true;
					} else if(hwnd != NULL)
					{
						DRAGONDROP dropinfo;
						mpt::PathString fullPath;
						if(GetDropInfo(dropinfo, fullPath))
						{
							if(dropinfo.dropType == DRAGONDROP_SONG)
							{
								canDrop = true;
							} else if(hwnd->SendMessage(MSG_MOD_DRAGONDROPPING, false, (LParam)&dropinfo))
							{
								canDrop = true;
							}
						}
					}
					SetCursor(canDrop ? CMainFrame::curDragging : CMainFrame::curNoDrop);
					if(canDrop)
					{
						if(GetDropHighlightItem() != m_hItemDrag)
						{
							SelectDropTarget(m_hItemDrag);
						}
						m_hItemDrop = NULL;
						return;
					}
				}
			}

			if((point.x >= -1) && (point.x <= rect.right + ui::ScrollBarSize))
			{
				if(point.y <= 0)
				{
					TreeItemHandle hfirst = GetFirstVisibleItem();
					if(hfirst != NULL)
					{
						TreeItemHandle hprev = GetNextItem(hfirst, ui::TreePreviousVisible);
						if(hprev != NULL)
							SetFirstVisibleItem(hprev);
					}
				} else if(point.y >= rect.bottom - 1)
				{
					hItem = HitTest(point, &flags);
					TreeItemHandle hNext = GetNextItem(hItem, ui::TreeNextVisible);
					if(hNext != NULL)
					{
						EnsureVisible(hNext);
					}
				}
			}
			if((hItem = HitTest(point, &flags)) != NULL)
			{
				SelectDropTarget(hItem);
				m_hItemDrop = hItem;
			}
		}
	}
	TreeCtrl::OnMouseMove(nFlags, point);
}


void CModTree::OnRefreshTree()
{
	BeginWaitCursor();
	for(auto &doc : m_docInfo)
	{
		UpdateView(doc.second, UpdateHint().ModType());
	}
	RefreshMidiLibrary();
	RefreshDlsBanks();
	RefreshInstrumentLibrary();
	EndWaitCursor();
}


void CModTree::OnExecuteItem()
{
	ExecuteItem(GetSelectedItem());
}


void CModTree::OnDeleteTreeItem()
{
	DeleteTreeItem(GetSelectedItem(), CInputHandler::ShiftPressed());
}


void CModTree::OnPlayTreeItem()
{
	PlayItem(GetSelectedItem(), NOTE_MIDDLEC);
}


void CModTree::OnOpenTreeItem()
{
	OpenTreeItem(GetSelectedItem());
}


void CModTree::OnMuteTreeItem()
{
	TreeItemHandle hItem = GetSelectedItem();

	const ModItem modItem = GetModItem(hItem);
	const uint32 modItemID = modItem.val1;

	ModTreeDocInfo *info = GetDocumentInfoFromItem(hItem);
	if(info)
	{
		CModDoc &modDoc = info->modDoc;
		if((modItem.type == MODITEM_SAMPLE) && !modDoc.GetNumInstruments())
		{
			modDoc.MuteSample((SAMPLEINDEX)modItemID, !modDoc.IsSampleMuted((SAMPLEINDEX)modItemID));
			UpdateView(*info, SampleHint((SAMPLEINDEX)modItemID).Info().Names());
		} else if((modItem.type == MODITEM_INSTRUMENT) && modDoc.GetNumInstruments())
		{
			modDoc.MuteInstrument((INSTRUMENTINDEX)modItemID, !modDoc.IsInstrumentMuted((INSTRUMENTINDEX)modItemID));
			UpdateView(*info, InstrumentHint((INSTRUMENTINDEX)modItemID).Info().Names());
		} else if(modItem.type == MODITEM_EFFECT)
		{
			IMixPlugin *pPlugin = modDoc.GetSoundFile().m_MixPlugins[modItemID].pMixPlugin;
			if(pPlugin == nullptr)
				return;
			pPlugin->ToggleBypass();
			if(modDoc.GetSoundFile().GetModSpecifications().supportsPlugins)
				modDoc.SetModified();
			//UpdateView(*info, PluginHint(static_cast<PLUGINDEX>(modItemID + 1)));
		} else if(modItem.type == MODITEM_HDR_EFFECTS)
		{
			auto &sndFile = modDoc.GetSoundFile();
			BypassAllPlugins(sndFile, !AllPluginsBypassed(sndFile, false), false);
		}
	}
}


void CModTree::OnMuteOnlyEffects()
{
	TreeItemHandle hItem = GetSelectedItem();

	const ModItem modItem = GetModItem(hItem);

	ModTreeDocInfo *info = GetDocumentInfoFromItem(hItem);
	if(info)
	{
		CModDoc &modDoc = info->modDoc;
		if(modItem.type == MODITEM_HDR_EFFECTS)
		{
			auto &sndFile = modDoc.GetSoundFile();
			BypassAllPlugins(sndFile, !AllPluginsBypassed(sndFile, true), true);
		}
	}
}


void CModTree::OnSoloTreeItem()
{
	TreeItemHandle hItem = GetSelectedItem();

	const ModItem modItem = GetModItem(hItem);
	const uint32 modItemID = modItem.val1;

	ModTreeDocInfo *info = GetDocumentInfoFromItem(hItem);
	if(info)
	{
		CModDoc &modDoc = info->modDoc;
		INSTRUMENTINDEX nInstruments = modDoc.GetNumInstruments();
		if((modItem.type == MODITEM_SAMPLE) && (!nInstruments))
		{
			for(SAMPLEINDEX nSmp = 1; nSmp <= modDoc.GetNumSamples(); nSmp++)
			{
				modDoc.MuteSample(nSmp, nSmp != modItemID);
			}
			UpdateView(*info, SampleHint().Info().Names());
		} else if((modItem.type == MODITEM_INSTRUMENT) && (nInstruments))
		{
			for(INSTRUMENTINDEX nIns = 1; nIns <= nInstruments; nIns++)
			{
				modDoc.MuteInstrument(nIns, nIns != modItemID);
			}
			UpdateView(*info, InstrumentHint().Info().Names());
		}
	}
}


void CModTree::OnUnmuteAllTreeItem()
{
	TreeItemHandle hItem = GetSelectedItem();

	const ModItem modItem = GetModItem(hItem);

	ModTreeDocInfo *info = GetDocumentInfoFromItem(hItem);
	if(info)
	{
		CModDoc &modDoc = info->modDoc;
		if((modItem.type == MODITEM_SAMPLE) || (modItem.type == MODITEM_INSTRUMENT))
		{
			for(SAMPLEINDEX nSmp = 1; nSmp <= modDoc.GetNumSamples(); nSmp++)
			{
				modDoc.MuteSample(nSmp, false);
			}
			UpdateView(*info, SampleHint().Info().Names());
			for(INSTRUMENTINDEX nIns = 1; nIns <= modDoc.GetNumInstruments(); nIns++)
			{
				modDoc.MuteInstrument(nIns, false);
			}
			UpdateView(*info, InstrumentHint().Info().Names());
		}
	}
}


bool CModTree::HasEffectPlugins(const CSoundFile &sndFile)
{
	for(const auto &plugin : sndFile.m_MixPlugins)
	{
		if(!plugin.pMixPlugin)
			continue;
		if(!plugin.pMixPlugin->IsInstrument())
			return true;
	}
	return false;

}


bool CModTree::AllPluginsBypassed(const CSoundFile &sndFile, bool onlyEffects)
{
	for(const auto &plugin : sndFile.m_MixPlugins)
	{
		if(!plugin.pMixPlugin)
			continue;
		if(onlyEffects && plugin.pMixPlugin->IsInstrument())
			continue;
		if(!plugin.IsBypassed())
			return false;
	}
	return true;
}


void CModTree::BypassAllPlugins(CSoundFile &sndFile, bool bypass, bool onlyEffects)
{
	bool modified = false;
	for(auto &plugin : sndFile.m_MixPlugins)
	{
		if(!plugin.pMixPlugin)
			continue;
		if(onlyEffects && plugin.pMixPlugin->IsInstrument())
			continue;
		if(plugin.IsBypassed() != bypass)
		{
			plugin.pMixPlugin->Bypass(bypass);
			modified = true;
		}
	}
	if(modified && sndFile.GetModSpecifications().supportsPlugins && sndFile.GetpModDoc())
		sndFile.GetpModDoc()->SetModified();
}


// Helper function for generating an insert vector for samples/instruments/sequences
template <typename T>
static std::vector<T> GenerateInsertVector(size_t howMany, size_t insertPos, T insertId, T startId)
{
	std::vector<T> newOrder(howMany);
	std::iota(newOrder.begin(), newOrder.end(), startId);
	newOrder.insert(newOrder.begin() + insertPos, insertId);
	return newOrder;
}


void CModTree::InsertOrDupItem(bool insert)
{
	TreeItemHandle hItem = GetSelectedItem();

	const ModItem modItem = GetModItem(hItem);
	const uint32 modItemID = modItem.val1;

	ModTreeDocInfo *info = GetDocumentInfoFromItem(hItem);
	if(info)
	{
		CModDoc &modDoc = info->modDoc;
		CSoundFile &sndFile = modDoc.GetSoundFile();
		if(modItem.type == MODITEM_SEQUENCE || modItem.type == MODITEM_HDR_ORDERS)
		{
			// Duplicate / insert sequence
			const SEQUENCEINDEX newIndex = (modItem.type == MODITEM_HDR_ORDERS) ? sndFile.Order.GetNumSequences() : static_cast<SEQUENCEINDEX>(modItemID + 1);
			std::vector<SEQUENCEINDEX> newOrder = GenerateInsertVector<SEQUENCEINDEX>(sndFile.Order.GetNumSequences(), newIndex, static_cast<SEQUENCEINDEX>(insert ? SEQUENCEINDEX_INVALID : modItemID), 0);
			if(modDoc.ReArrangeSequences(newOrder) != SEQUENCEINDEX_INVALID)
			{
				sndFile.Order.SetSequence(newIndex);
				if(const auto name = sndFile.Order().GetName(); !insert && !name.empty())
					sndFile.Order().SetName(name + UL_(" (Copy)"));
				modDoc.UpdateAllViews(nullptr, SequenceHint(SEQUENCEINDEX_INVALID).Names().Data());
				modDoc.SetModified();
			} else
			{
				Reporting::Error("Maximum number of sequences reached.");
			}
		} else if(modItem.type == MODITEM_SAMPLE)
		{
			// Duplicate / insert sample
			std::vector<SAMPLEINDEX> newOrder = GenerateInsertVector<SAMPLEINDEX>(sndFile.GetNumSamples(), modItemID, static_cast<SAMPLEINDEX>(insert ? 0 : modItemID), 1);
			if(modDoc.ReArrangeSamples(newOrder) != SAMPLEINDEX_INVALID)
			{
				modDoc.SetModified();
				modDoc.UpdateAllViews(nullptr, SampleHint().Info().Data().Names());
				modDoc.UpdateAllViews(nullptr, PatternHint().Data());
			} else
			{
				Reporting::Error("Maximum number of samples reached.");
			}
		} else if(modItem.type == MODITEM_INSTRUMENT)
		{
			// Duplicate / insert instrument
			std::vector<INSTRUMENTINDEX> newOrder = GenerateInsertVector<INSTRUMENTINDEX>(sndFile.GetNumInstruments(), modItemID, static_cast<INSTRUMENTINDEX>(insert ? 0 : modItemID), 1);
			if(modDoc.ReArrangeInstruments(newOrder) != INSTRUMENTINDEX_INVALID)
			{
				modDoc.UpdateAllViews(nullptr, InstrumentHint().Info().Envelope().Names());
				modDoc.UpdateAllViews(nullptr, PatternHint().Data());
				modDoc.SetModified();
			} else
			{
				Reporting::Error("Maximum number of instruments reached.");
			}
		}
	}
}


template<typename T>
static std::pair<std::vector<T>, T> PrepareMoveVector(T item, T firstItem, T lastItem, bool moveUp)
{
	std::pair<std::vector<T>, T> result{};
	T target = item;
	if(moveUp && item > firstItem)
		target--;
	else if(!moveUp && item < lastItem)
		target++;
	else
		return result;

	auto &newOrder = result.first;
	newOrder.resize(lastItem - firstItem + 1);
	std::iota(newOrder.begin(), newOrder.end(), firstItem);
	std::swap(newOrder[item - firstItem], newOrder[target - firstItem]);
	result.second = target;
	return result;
}


void CModTree::MoveTreeItem(TreeItemHandle hItem, bool moveUp)
{
	ModTreeDocInfo *info = GetDocumentInfoFromItem(hItem);
	if(!info)
		return;

	const ModItem modItem = GetModItem(hItem);
	const uint32 modItemID = modItem.val1;

	CModDoc &modDoc = info->modDoc;
	CSoundFile &sndFile = modDoc.GetSoundFile();
	TreeItemHandle newSelection = nullptr;

	switch(modItem.type)
	{
	case MODITEM_ORDER:
		{
			const SEQUENCEINDEX seq = static_cast<SEQUENCEINDEX>(modItem.val2);
			const ORDERINDEX ord = static_cast<ORDERINDEX>(modItemID);
			ModSequence &sequence = sndFile.Order(seq);

			ORDERINDEX target = ord;
			if(moveUp && ord > 0)
				target--;
			else if(!moveUp && ord < sequence.GetLengthTailTrimmed() - 1)
				target++;
			else
				return;

			if(sequence[ord] != sequence[target])
			{
				std::swap(sequence[ord], sequence[target]);
				modDoc.SetModified();
				modDoc.UpdateAllViews(nullptr, SequenceHint(seq).Data());
			}

			if(seq < info->tiOrders.size() && target < info->tiOrders[seq].size())
				newSelection = info->tiOrders[seq][target];
		}
		break;

	case MODITEM_SEQUENCE:
		if(const auto [newOrder, target] = PrepareMoveVector(static_cast<SEQUENCEINDEX>(modItemID), SEQUENCEINDEX(0), static_cast<SEQUENCEINDEX>(sndFile.Order.GetNumSequences() - 1), moveUp); !newOrder.empty())
		{
			modDoc.ReArrangeSequences(newOrder);

			auto curSeq = sndFile.Order.GetCurrentSequenceIndex();
			if(curSeq == modItemID)
				curSeq = target;
			else if(modItemID > curSeq && target <= curSeq)
				curSeq++;
			else if(modItemID < curSeq && target >= curSeq)
				curSeq--;
			sndFile.Order.SetSequence(curSeq);

			modDoc.SetModified();
			modDoc.UpdateAllViews(nullptr, SequenceHint(SEQUENCEINDEX_INVALID).Names().Data());

			if(info->tiSequences.size() > target)
				newSelection = info->tiSequences[target];
		}
		break;

	case MODITEM_SAMPLE:
		if(const auto [newOrder, target] = PrepareMoveVector(static_cast<SAMPLEINDEX>(modItemID), SAMPLEINDEX(1), sndFile.GetNumSamples(), moveUp); !newOrder.empty())
		{
			modDoc.ReArrangeSamples(newOrder);
			modDoc.SetModified();
			modDoc.UpdateAllViews(nullptr, SampleHint().Info().Data().Names());
			modDoc.UpdateAllViews(nullptr, PatternHint().Data());
			modDoc.UpdateAllViews(nullptr, InstrumentHint().Info());
			newSelection = GetNthChildItem(info->hSamples, target - 1);
		}
		break;

	case MODITEM_INSTRUMENT:
		if(const auto [newOrder, target] = PrepareMoveVector(static_cast<INSTRUMENTINDEX>(modItemID), INSTRUMENTINDEX(1), sndFile.GetNumInstruments(), moveUp); !newOrder.empty())
		{
			modDoc.ReArrangeInstruments(newOrder);
			modDoc.SetModified();
			modDoc.UpdateAllViews(nullptr, InstrumentHint().Info().Envelope().Names());
			modDoc.UpdateAllViews(nullptr, PatternHint().Data());
			newSelection = GetNthChildItem(info->hInstruments, target - 1);
		}
		break;

	default:
		break;
	}
	if(newSelection)
		SelectItem(newSelection);
}


void CModTree::OnSwitchToTreeItem()
{
	TreeItemHandle hItem = GetSelectedItem();

	const ModItem modItem = GetModItem(hItem);

	CModDoc *pModDoc = GetDocumentFromItem(hItem);
	if(pModDoc && (modItem.type == MODITEM_SEQUENCE))
	{
		pModDoc->ActivateView(IDD_CONTROL_PATTERNS, uint32(modItem.val1 << SEQU_SHIFT) | SEQU_INDICATOR);
	}
}


void CModTree::OnSetItemPath()
{
	TreeItemHandle hItem = GetSelectedItem();

	const ModItem modItem = GetModItem(hItem);
	CModDoc *pModDoc = GetDocumentFromItem(hItem);

	if(pModDoc && modItem.val1)
	{
		SAMPLEINDEX smpID = static_cast<SAMPLEINDEX>(modItem.val1);
		const mpt::PathString path = pModDoc->GetSoundFile().GetSamplePath(smpID);
		FileDialog dlg = OpenFileDialog()
			.ExtensionFilter(UL_("All Samples|*.wav;*.flac|All files(*.*)|*.*||"));	// Only show samples that we actually can save as well.
		if(path.empty())
			dlg.WorkingDirectory(TrackerSettings::Instance().PathSamples.GetWorkingDir());
		else
			dlg.DefaultFilename(path);
		if(!dlg.Show())
			return;
		TrackerSettings::Instance().PathSamples.SetWorkingDir(dlg.GetWorkingDirectory());

		if(dlg.GetFirstFile() != pModDoc->GetSoundFile().GetSamplePath(smpID))
		{
			pModDoc->GetSoundFile().SetSamplePath(smpID, dlg.GetFirstFile());
			pModDoc->SetModified();
		}
		OnReloadItem();
	}
}


void CModTree::OnSaveItem()
{
	TreeItemHandle hItem = GetSelectedItem();

	const ModItem modItem = GetModItem(hItem);
	CModDoc *pModDoc = GetDocumentFromItem(hItem);

	if(pModDoc && modItem.val1)
	{
		SAMPLEINDEX smpID = static_cast<SAMPLEINDEX>(modItem.val1);
		pModDoc->SaveSample(smpID);
		if(pModDoc)
			pModDoc->UpdateAllViews(nullptr, SampleHint(smpID).Info());
		OnRefreshTree();
	}
}


void CModTree::OnSaveAll()
{
	CModDoc *pModDoc = GetDocumentFromItem(GetSelectedItem());
	if(pModDoc != nullptr)
	{
		pModDoc->SaveAllSamples(false);
		if(pModDoc)
			pModDoc->UpdateAllViews(nullptr, SampleHint().Info());
		OnRefreshTree();
	}
}


void CModTree::OnReloadItem()
{
	TreeItemHandle hItem = GetSelectedItem();

	const ModItem modItem = GetModItem(hItem);
	CModDoc *pModDoc = GetDocumentFromItem(hItem);

	if(pModDoc && modItem.val1)
	{
		SAMPLEINDEX smpID = static_cast<SAMPLEINDEX>(modItem.val1);
		CSoundFile &sndFile = pModDoc->GetSoundFile();
		pModDoc->GetSampleUndo().PrepareUndo(smpID, sundo_replace, "Replace");
		if(!sndFile.LoadExternalSample(smpID, sndFile.GetSamplePath(smpID)))
		{
			pModDoc->GetSampleUndo().RemoveLastUndoStep(smpID);
			Reporting::Error(UL_("Unable to load sample:\n") + sndFile.GetSamplePath(smpID).ToUnicode());
		} else
		{
			if(!sndFile.GetSample(smpID).uFlags[SMP_KEEPONDISK])
			{
				pModDoc->SetModified();
			}
			pModDoc->UpdateAllViews(nullptr, SampleHint(smpID).Info().Data().Names());
		}

		OnRefreshTree();
	}
}


void CModTree::OnReloadAll()
{
	CModDoc *pModDoc = GetDocumentFromItem(GetSelectedItem());
	if(pModDoc != nullptr)
	{
		CSoundFile &sndFile = pModDoc->GetSoundFile();
		bool anyMissing = false;
		for(SAMPLEINDEX smp = 1; smp <= sndFile.GetNumSamples(); smp++)
		{
			const mpt::PathString &path = sndFile.GetSamplePath(smp);
			if(path.empty())
				continue;

			pModDoc->GetSampleUndo().PrepareUndo(smp, sundo_replace, "Replace");
			if(!sndFile.LoadExternalSample(smp, path))
			{
				pModDoc->GetSampleUndo().RemoveLastUndoStep(smp);
				anyMissing = true;
			} else
			{
				if(!sndFile.GetSample(smp).uFlags[SMP_KEEPONDISK])
				{
					pModDoc->SetModified();
				}
			}
		}
		pModDoc->UpdateAllViews(nullptr, SampleHint().Info().Data().Names());
		OnRefreshTree();
		if(anyMissing)
		{
			OnFindMissing();
		}
	}
}


// Find missing external samples
void CModTree::OnFindMissing()
{
	CModDoc *pModDoc = GetDocumentFromItem(GetSelectedItem());
	if(pModDoc == nullptr)
	{
		return;
	}
	MissingExternalSamplesDlg dlg(*pModDoc, CMainFrame::GetMainFrame());
	dlg.DoModal();
}


void CModTree::OnAddDlsBank()
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if(pMainFrm)
		pMainFrm->OnAddDlsBank();
}


void CModTree::OnImportMidiLib()
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if(pMainFrm)
		pMainFrm->OnImportMidiLib();
}


void CModTree::OnExportMidiLib()
{
	FileDialog dlg = SaveFileDialog()
		.DefaultExtension(UL_("ini"))
		.DefaultFilename(P_("mptrack.ini"))
		.ExtensionFilter(UL_("Text and INI files (*.txt,*.ini)|*.txt;*.ini|All Files (*.*)|*.*||"));
	if(!dlg.Show()) return;

	CTrackApp::ExportMidiConfig(dlg.GetFirstFile());
}


void CModTree::OnRefreshInstrLib()
{
	BeginWaitCursor();
	RefreshInstrumentLibrary();
	EndWaitCursor();
}


void CModTree::OnOpenInstrumentLibraryFilter()
{
	static_cast<CModTreeBar*>(GetParent())->StartTreeFilter(*this);
}


void CModTree::OnShowDirectories()
{
	TrackerSettings::Instance().showDirsInSampleBrowser = !TrackerSettings::Instance().showDirsInSampleBrowser;
	OnRefreshInstrLib();
}


void CModTree::OnShowAllFiles()
{
	if(!m_showAllFiles)
	{
		m_showAllFiles = true;
		OnRefreshInstrLib();
	}
}


void CModTree::OnShowSoundFiles()
{
	if(m_showAllFiles)
	{
		m_showAllFiles = false;
		OnRefreshInstrLib();
	}
}


void CModTree::OnGotoInstrumentDir()
{
	SetFullInstrumentLibraryPath(TrackerSettings::Instance().PathInstruments.GetDefaultDir());
}


void CModTree::OnGotoSampleDir()
{
	SetFullInstrumentLibraryPath(TrackerSettings::Instance().PathSamples.GetDefaultDir());
}


void CModTree::OnSoundBankProperties()
{
	const ModItem modItem = GetModItem(GetSelectedItem());
	if(modItem.type == MODITEM_DLSBANK_FOLDER
	   && modItem.val1 < CTrackApp::gpDLSBanks.size() && CTrackApp::gpDLSBanks[modItem.val1])
	{
		CSoundBankProperties dlg(*CTrackApp::gpDLSBanks[modItem.val1], this);
		dlg.DoModal();
	}
}


LResult CModTree::OnCustomKeyMsg(WParam wParam, LParam /*lParam*/)
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();

	ModCommand::NOTE note = NOTE_NONE;
	const bool start = wParam >= kcTreeViewStartNotes && wParam <= kcTreeViewEndNotes;
	const bool stop = wParam >= kcTreeViewStartNoteStops && wParam <= kcTreeViewEndNoteStops && !IsSampleBrowser();

	switch(wParam)
	{
	case kcContextMenu:
		if(TreeItemHandle item = GetSelectedItem())
		{
			Rect rect;
			GetItemRect(item, rect, false);
			ClientToScreen(&rect);
			OnItemRightClick(item, rect.TopLeft() + Point{ rect.Height() / 2, rect.Height() / 2 });
			return wParam;
		}
		break;

	case kcTreeViewStopPreview:
		note = NOTE_NOTECUT;
		break;

	case kcTreeViewSwitchViews:
		// Tab: Switch between folder and file view.
		GetOtherView()->SetFocus();
		return wParam;

	case kcTreeViewOpen:
		if(TreeItemHandle hItem = GetSelectedItem(); hItem)
		{
			if(!ExecuteItem(hItem))
			{
				if(ItemHasChildren(hItem))
				{
					Expand(hItem, ui::TreeToggle);
				}
			}
		}
		return wParam;

	case kcTreeViewPlay:
		OnPlayTreeItem();
		return wParam;

	case kcTreeViewInsert:
	case kcTreeViewDuplicate:
		InsertOrDupItem(wParam == kcTreeViewInsert);
		return wParam;

	case kcTreeViewDelete:
	case kcTreeViewDeletePermanently:
		DeleteTreeItem(GetSelectedItem(), wParam == kcTreeViewDeletePermanently);
		return wParam;

	case kcTreeViewFolderUp:
		// Backspace: Go up one directory
		if(GetParentRootItem(GetSelectedItem()) == m_hInsLib || IsSampleBrowser())
		{
			InstrumentLibraryChDir(P_(".."), !m_SongFileName.empty());
			return wParam;
		}
		return kcNull;

	case kcTreeViewFind:
		OnOpenInstrumentLibraryFilter();
		return wParam;

	case kcTreeViewRename:
	case kcTreeViewSendToEditorInsertNew:
		if(TreeItemHandle hItem = GetSelectedItem(); hItem)
		{
			const ModItem modItem = GetModItem(hItem);
			static constexpr ModItemType instrumentTypes[] = {MODITEM_INSLIB_SAMPLE, MODITEM_INSLIB_INSTRUMENT, MODITEM_MIDIINSTRUMENT, MODITEM_MIDIPERCUSSION, MODITEM_DLSBANK_INSTRUMENT};
			if(mpt::contains(instrumentTypes, modItem.type))
			{
				// Load sample into currently selected (or new) sample or instrument slot
				CModScrollView *view = static_cast<CModScrollView *>(CMainFrame::GetMainFrame()->GetActiveView());
				if(view)
				{
					const bool isSampleView = dynamic_cast<CViewSample *>(view) != nullptr;
					const bool isInstrumentView = dynamic_cast<CViewInstrument *>(view) != nullptr;
					mpt::PathString fullPath = InsLibGetFullPath(hItem);
					DRAGONDROP dropInfo;
					m_hItemDrag = hItem;
					m_itemDrag = modItem;
					if((isSampleView || isInstrumentView) && GetDropInfo(dropInfo, fullPath))
					{
						dropInfo.insertType = (wParam == kcTreeViewSendToEditorInsertNew) ? DRAGONDROP::InsertType::InsertNew : DRAGONDROP::InsertType::Replace;
						view->SendMessage(MSG_MOD_DRAGONDROPPING, true, reinterpret_cast<LParam>(&dropInfo));
						// In case a message box like "create instrument for sample?" showed up
						SetFocus();
					}
				}
			} else if(!IsSampleBrowser() && wParam != kcTreeViewSendToEditorInsertNew)
			{
				EditLabel(hItem);
			}
		}
		return wParam;

	case kcTreeViewSortByName:
		OnSortByName();
		return wParam;
	case kcTreeViewSortByDate:
		OnSortByDate();
		return wParam;
	case kcTreeViewSortBySize:
		OnSortBySize();
		return wParam;

	case kcTreeViewMoveUp:
	case kcTreeViewMoveDown:
		MoveTreeItem(GetSelectedItem(), wParam == kcTreeViewMoveUp);
		return wParam;

	default:
		if(start || stop)
		{
			const ModItem modItem = GetModItem(GetSelectedItem());
			CModDoc *modDoc = m_docInfo.count(m_selectedDoc) ? m_selectedDoc : nullptr;
			const int noteOffset = static_cast<int>(wParam - (start ? kcTreeViewStartNotes : kcTreeViewStartNoteStops));
			note = static_cast<ModCommand::NOTE>(Clamp(NOTE_MIN + pMainFrm->GetBaseOctave() * 12 + noteOffset, NOTE_MIN, NOTE_MAX));
			if(modDoc && modItem.type == MODITEM_INSTRUMENT)
				note = modDoc->GetNoteWithBaseOctave(noteOffset, static_cast<INSTRUMENTINDEX>(modItem.val1));
		}
		break;
	}

	if(note != NOTE_NONE)
	{
		if(stop)
			note |= 0x80;

		if(PlayItem(GetSelectedItem(), note))
			return wParam;
		else
			return kcNull;
	}

	return kcNull;
}


LResult CModTree::OnMidiMsg(WParam midiData_, LParam)
{
	uint32 midiData = static_cast<uint32>(midiData_);
	// Handle MIDI messages assigned to shortcuts
	CInputHandler *ih = CMainFrame::GetInputHandler();
	if(ih->HandleMIDIMessage(kCtxViewTree, midiData) == kcNull)
	    ih->HandleMIDIMessage(kCtxAllContexts, midiData);

	uint8 midiByte1 = MIDIEvents::GetDataByte1FromEvent(midiData);
	int volume;
	switch(MIDIEvents::GetTypeFromEvent(midiData))
	{
	case MIDIEvents::evNoteOn:
		volume = MIDIEvents::GetDataByte2FromEvent(midiData);
		if(volume > 0)
		{
			PlayItem(GetSelectedItem(), midiByte1 + NOTE_MIN, Util::muldivr(volume, 256, 127));
			return 1;
		}
		[[fallthrough]];
	case MIDIEvents::evNoteOff:
		PlayItem(GetSelectedItem(), NOTE_NOTECUT);
		return 1;
	default:
		return 0;
	}
}


void CModTree::OnKillFocus(Wnd *pNewWnd)
{
	if(CMainFrame::GetMainFrame()->GetHelpText() == m_HelpText)
	{
		CMainFrame::GetMainFrame()->SetHelpText(UL_(""));
		m_HelpText.clear();
	}
	TreeCtrl::OnKillFocus(pNewWnd);
	CMainFrame::GetMainFrame()->m_bModTreeHasFocus = false;
	// Required to immediately redirect MIDI input focus after drag&drop from tree view to editor
	if(pNewWnd != nullptr)
		CMainFrame::GetMainFrame()->SetMidiRecordWnd(pNewWnd);
}


void CModTree::OnSetFocus(Wnd *pOldWnd)
{
	TreeCtrl::OnSetFocus(pOldWnd);
	CMainFrame::GetMainFrame()->m_bModTreeHasFocus = true;
	CMainFrame::GetMainFrame()->SetMidiRecordWnd(this);
}


bool CModTree::IsItemExpanded(TreeItemHandle hItem) const
{
	// checks if a treeview item is expanded.
	if(hItem == nullptr)
		return false;
	return GetItemState(hItem, ui::TreeStateExpanded) != 0;
}


void CModTree::OnCloseItem()
{
	TreeItemHandle hItem = GetSelectedItem();
	if(hItem == m_hInsLib && !m_SongFileName.empty())
	{
		InstrumentLibraryChDir(P_(".."), true);
		return;
	}
	CModDoc *pModDoc = GetDocumentFromItem(hItem);
	if(pModDoc == nullptr)
		return;
	// Spam our message to the first available view
	const auto &views = pModDoc->GetViews();
	if(!views.empty())
		views.front()->PostCommand(ID_FILE_CLOSE);
}


// Delete all children of a tree item
void CModTree::DeleteChildren(TreeItemHandle hItem)
{
	if(hItem != nullptr)
	{
		TreeItemHandle hChildItem;
		while((hChildItem = GetChildItem(hItem)) != nullptr)
		{
			DeleteItem(hChildItem);
		}
	}
}


// Get the n-th child of a tree node
TreeItemHandle CModTree::GetNthChildItem(TreeItemHandle hItem, int index) const
{
	TreeItemHandle hChildItem = nullptr;
	if(hItem != nullptr && ItemHasChildren(hItem))
	{
		hChildItem = GetChildItem(hItem);
		while(index-- > 0)
		{
			hChildItem = GetNextSiblingItem(hChildItem);
		}
	}
	return hChildItem;
}


// Gets the root parent of an item, i.e. if C is a child of B and B is a child of A, GetParentRootItem(C) returns A.
// A root item is considered to be its own parent, i.e. the returned value is only ever NULL if the input value was NULL.
TreeItemHandle CModTree::GetParentRootItem(TreeItemHandle hItem) const
{
	while(hItem != nullptr)
	{
		const TreeItemHandle h = GetParentItem(hItem);
		if(h == nullptr || h == hItem)
			break;
		hItem = h;
	}
	return hItem;
}


void CModTree::OnRenameItem()
{
	EditLabel(GetSelectedItem());
}


// Editing sample, instrument, order, pattern, etc. labels
void CModTree::OnBeginLabelEdit(NotifyHeader *nmhdr, LResult *result)
{
	const auto *notification = static_cast<const ui::TreeNotification *>(nmhdr->extra);
	Edit *editCtrl = GetEditControl();
	if(editCtrl == nullptr)
		return;

	const ModItem modItem = GetModItem(notification->item);
	const CModDoc *modDoc = modItem.IsSongItem() ? GetDocumentFromItem(notification->item) : nullptr;

	mpt::ustring text;
	m_doLabelEdit = false;

	if(modDoc != nullptr)
	{
		const CSoundFile &sndFile = modDoc->GetSoundFile();
		const CModSpecifications &modSpecs = sndFile.GetModSpecifications();

		switch(modItem.type)
		{
		case MODITEM_HDR_SONG:
			text = mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.m_songName);
			editCtrl->SetLimitText(modSpecs.modNameLengthMax);
			m_doLabelEdit = true;
			break;

		case MODITEM_ORDER:
			{
				PATTERNINDEX pat = sndFile.Order(static_cast<SEQUENCEINDEX>(modItem.val2)).at(static_cast<ORDERINDEX>(modItem.val1));
				if(pat == PATTERNINDEX_INVALID)
					text = UL_("---");
				else if(pat == PATTERNINDEX_SKIP)
					text = UL_("+++");
				else
					text = mpt::ufmt::val(pat);
				m_doLabelEdit = true;
			}
			break;

		case MODITEM_HDR_ORDERS:
			if(sndFile.Order.GetNumSequences() != 1 || sndFile.GetModSpecifications().sequencesMax <= 1)
			{
				break;
			}
			[[fallthrough]];
		case MODITEM_SEQUENCE:
			if(modItem.val1 < sndFile.Order.GetNumSequences())
			{
				text = sndFile.Order(static_cast<SEQUENCEINDEX>(modItem.val1)).GetName();
				m_doLabelEdit = true;
			}
			break;

		case MODITEM_PATTERN:
			if(modItem.val1 < sndFile.Patterns.GetNumPatterns() && modSpecs.hasPatternNames)
			{
				text = mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.Patterns[modItem.val1].GetName());
				editCtrl->SetLimitText(MAX_PATTERNNAME - 1);
				m_doLabelEdit = true;
			}
			break;

		case MODITEM_SAMPLE:
			if(modItem.val1 <= sndFile.GetNumSamples())
			{
				text = mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.m_szNames[modItem.val1]);
				editCtrl->SetLimitText(modSpecs.sampleNameLengthMax);
				m_doLabelEdit = true;
			}
			break;

		case MODITEM_INSTRUMENT:
			if(modItem.val1 <= sndFile.GetNumInstruments() && sndFile.Instruments[modItem.val1] != nullptr)
			{
				text = mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.Instruments[modItem.val1]->name);
				editCtrl->SetLimitText(modSpecs.instrNameLengthMax);
				m_doLabelEdit = true;
			}
			break;

		default:
			break;
		}
	} else if(modItem.type == MODITEM_HDR_INSTRUMENTLIB)
	{
		text = (m_InstrLibPath + m_SongFileName).ToUnicode();
		m_doLabelEdit = true;
	}

	if(m_doLabelEdit)
	{
		CMainFrame::GetInputHandler()->Bypass(true);
		editCtrl->SetWindowText(mpt::ToUnicode(text));
	}
	*result = m_doLabelEdit ? false : true;
}


// End editing sample, instrument, order, pattern, etc. labels
void CModTree::OnEndLabelEdit(NotifyHeader *nmhdr, LResult *result)
{
	CMainFrame::GetInputHandler()->Bypass(false);
	m_doLabelEdit = false;

	const auto *notification = static_cast<const ui::TreeNotification *>(nmhdr->extra);
	const ModItem modItem = GetModItem(notification->item);
	CModDoc *modDoc = modItem.IsSongItem() ? GetDocumentFromItem(notification->item) : nullptr;

	*result = false;
	if(notification->text == nullptr)
		return;

	if(modDoc != nullptr)
	{
		CSoundFile &sndFile = modDoc->GetSoundFile();
		const CModSpecifications &modSpecs = sndFile.GetModSpecifications();

		const mpt::ustring itemText = *notification->text;
		switch(modItem.type)
		{
		case MODITEM_HDR_SONG:
			if(mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.m_songName) != itemText)
			{
				sndFile.m_songName = mpt::truncate(mpt::ToCharset(sndFile.GetCharsetInternal(), itemText), modSpecs.modNameLengthMax);
				modDoc->SetModified();
				modDoc->UpdateAllViews(nullptr, GeneralHint().General());
			}
			break;

		case MODITEM_ORDER:
			if(!itemText.empty())
			{
				PATTERNINDEX pat = mpt::parse<PATTERNINDEX>(itemText);
				bool valid = true;
				if(itemText[0] == UC_('-'))
				{
					pat = PATTERNINDEX_INVALID;
				} else if(itemText[0] == UC_('+'))
				{
					if(modSpecs.hasIgnoreIndex)
						pat = PATTERNINDEX_SKIP;
					else
						valid = false;
				} else
				{
					valid = (pat < sndFile.Patterns.GetNumPatterns());
				}
				PATTERNINDEX &target = sndFile.Order(static_cast<SEQUENCEINDEX>(modItem.val2))[static_cast<ORDERINDEX>(modItem.val1)];
				if(valid && pat != target)
				{
					target = pat;
					modDoc->SetModified();
					modDoc->UpdateAllViews(nullptr, SequenceHint().Data());
				}
			} else
			{
				ui::Beep();
			}
			break;

		case MODITEM_HDR_ORDERS:
		case MODITEM_SEQUENCE:
			if(modItem.val1 < sndFile.Order.GetNumSequences() && sndFile.Order(static_cast<SEQUENCEINDEX>(modItem.val1)).GetName() != itemText)
			{
				sndFile.Order(static_cast<SEQUENCEINDEX>(modItem.val1)).SetName(itemText);
				modDoc->SetModified();
				modDoc->UpdateAllViews(nullptr, SequenceHint(static_cast<SEQUENCEINDEX>(modItem.val1)).Names());
			}
			break;

		case MODITEM_PATTERN:
			if(modItem.val1 < sndFile.Patterns.GetNumPatterns() && modSpecs.hasPatternNames && mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.Patterns[modItem.val1].GetName()) != itemText)
			{
				sndFile.Patterns[modItem.val1].SetName(mpt::ToCharset(sndFile.GetCharsetInternal(), itemText));
				modDoc->SetModified();
				modDoc->UpdateAllViews(nullptr, PatternHint(static_cast<PATTERNINDEX>(modItem.val1)).Data().Names());
			}
			break;

		case MODITEM_SAMPLE:
			if(modItem.val1 <= sndFile.GetNumSamples() && mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.m_szNames[modItem.val1]) != itemText)
			{
				sndFile.m_szNames[modItem.val1] = mpt::truncate(mpt::ToCharset(sndFile.GetCharsetInternal(), itemText), modSpecs.sampleNameLengthMax);
				modDoc->SetModified();
				modDoc->UpdateAllViews(nullptr, SampleHint(static_cast<SAMPLEINDEX>(modItem.val1)).Info().Names());
			}
			break;

		case MODITEM_INSTRUMENT:
			if(modItem.val1 <= sndFile.GetNumInstruments() && sndFile.Instruments[modItem.val1] != nullptr && mpt::ToUnicode(sndFile.GetCharsetInternal(), sndFile.Instruments[modItem.val1]->name) != itemText)
			{
				sndFile.Instruments[modItem.val1]->name = mpt::truncate(mpt::ToCharset(sndFile.GetCharsetInternal(), itemText), modSpecs.instrNameLengthMax);
				modDoc->SetModified();
				modDoc->UpdateAllViews(nullptr, InstrumentHint(static_cast<INSTRUMENTINDEX>(modItem.val1)).Info().Names());
			}
			break;

		default:
			break;
		}
	} else if(modItem.type == MODITEM_HDR_INSTRUMENTLIB)
	{
		const auto newPath = mpt::PathString::FromUnicode(*notification->text);
		if(mpt::PathCompareNoCase(newPath, m_InstrLibPath + m_SongFileName))
			SetFullInstrumentLibraryPath(newPath);
	}
}


void CModTree::OnDropFiles(const std::vector<mpt::PathString> &files)
{
	const Point point(Fl::event_x() - x(), Fl::event_y() - y());
	const TreeItemHandle hItem = HitTest(point);
	const ModItem modItem = GetModItem(hItem);
	if((modItem.type == MODITEM_MIDIINSTRUMENT || modItem.type == MODITEM_MIDIPERCUSSION) && !files.empty())
	{
		if(modItem.type == MODITEM_MIDIINSTRUMENT)
			SetMidiInstrument(modItem.val1, files.front());
		else
			SetMidiPercussion(modItem.val1, files.front());
		SelectDropTarget(nullptr);
		return;
	}

	bool refreshDLS = false;
	CMainFrame::GetMainFrame()->SetForegroundWindow();
	for(const mpt::PathString &file : files)
	{
		if(IsSampleBrowser())
		{
			SetFullInstrumentLibraryPath(file);
			break;
		} else if(CTrackApp::AddDLSBank(file))
		{
			refreshDLS = true;
		} else
		{
			theApp.OpenDocumentFile(file);
		}
	}
	if(refreshDLS)
	{
		RefreshDlsBanks();
	}
}


OPENMPT_NAMESPACE_END
