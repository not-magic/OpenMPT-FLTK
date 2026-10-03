// FLTK port of openmpt/mptrack/View_tre.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "../soundlib/modcommand.h"
#include "../soundlib/Snd_defs.h"

#include "Mptrack.h"
#include "Notification.h"
#include "UpdateHints.h"

#include <vector>
#include <bitset>
#include <condition_variable>
#include <mutex>

OPENMPT_NAMESPACE_BEGIN

class CModDoc;
class CModTree;
class CTrackerSoundFile;
class CDLSBank;

struct ModTreeDocInfo
{
	// Tree state variables
	std::vector<std::vector<TreeItemHandle>> tiOrders;
	std::vector<TreeItemHandle> tiSequences, tiPatterns;
	CModDoc &modDoc;
	TreeItemHandle hSong = nullptr, hPatterns = nullptr, hSamples = nullptr, hInstruments = nullptr, hComments = nullptr, hOrders = nullptr, hEffects = nullptr;

	// Module information
	ORDERINDEX ordSel = ORDERINDEX_INVALID;
	SEQUENCEINDEX seqSel = SEQUENCEINDEX_INVALID;

	std::bitset<MAX_SAMPLES> samplesPlaying;
	std::bitset<MAX_INSTRUMENTS> instrumentsPlaying;

	ModTreeDocInfo(CModDoc &modDoc);
};


class CModTree: public TreeCtrl
{
public:
	enum ModItemType : uint8
	{
		MODITEM_NULL = 0,

		MODITEM_BEGIN_SONGITEMS,
		MODITEM_ORDER = MODITEM_BEGIN_SONGITEMS,
		MODITEM_PATTERN,
		MODITEM_SAMPLE,
		MODITEM_INSTRUMENT,
		MODITEM_COMMENTS,
		MODITEM_EFFECT,
		MODITEM_SEQUENCE,
		MODITEM_HDR_SONG,
		MODITEM_HDR_ORDERS,
		MODITEM_HDR_PATTERNS,
		MODITEM_HDR_SAMPLES,
		MODITEM_HDR_INSTRUMENTS,
		MODITEM_HDR_EFFECTS,
		MODITEM_END_SONGITEMS = MODITEM_HDR_EFFECTS,

		MODITEM_HDR_INSTRUMENTLIB,
		MODITEM_HDR_MIDILIB,
		MODITEM_HDR_MIDIGROUP,
		MODITEM_MIDIINSTRUMENT,
		MODITEM_MIDIPERCUSSION,
		MODITEM_INSLIB_FOLDER,
		MODITEM_INSLIB_SAMPLE,
		MODITEM_INSLIB_INSTRUMENT,
		MODITEM_INSLIB_SONG,
		MODITEM_DLSBANK_FOLDER,
		MODITEM_DLSBANK_INSTRUMENT,
	};

protected:
	enum TreeStatus
	{
		TREESTATUS_RDRAG = 0x01,
		TREESTATUS_LDRAG = 0x02,
		TREESTATUS_SINGLEEXPAND = 0x04,
		TREESTATUS_DRAGGING = (TREESTATUS_RDRAG | TREESTATUS_LDRAG)
	};

	// Bit mask magic
	enum : unsigned int
	{
		MIDILIB_SHIFT = 16,
		MIDILIB_MASK  = (1 << MIDILIB_SHIFT) - 1,

		// Must be consistent with CCtrlPatterns::OnActivatePage
		SEQU_SHIFT     = 16,
		SEQU_MASK      = (1 << SEQU_SHIFT) - 1,
		SEQU_INDICATOR = 0x80000000,

		// Soundbank instrument identification (must be consistent with CViewInstrument::OnDragonDrop / CViewSample::OnDragonDrop)
		DLS_TYPEPERC    = 0x80000000,
		DLS_INSTRMASK   = 0x0000FFFF,
		DLS_REGIONMASK  = 0x7FFF0000,  // Drum region
		DLS_REGIONSHIFT = 16,

		DLS_DRUM_FOLDER_LPARAM = 0x12345678,
	};
	static_assert((ORDERINDEX_INVALID & SEQU_MASK) == ORDERINDEX_INVALID, "ORDERINDEX doesn't fit in GetItemData() parameter");
	static_assert((ORDERINDEX_MAX & SEQU_MASK) == ORDERINDEX_MAX, "ORDERINDEX doesn't fit in GetItemData() parameter");
	static_assert((((SEQUENCEINDEX_INVALID << SEQU_SHIFT) & ~SEQU_INDICATOR) >> SEQU_SHIFT) == SEQUENCEINDEX_INVALID, "SEQUENCEINDEX doesn't fit in GetItemData() parameter");

	struct ModItem
	{
		uint32 val1;
		uint16 val2;
		ModItemType type;

		ModItem(ModItemType t = MODITEM_NULL, uint32 v1 = 0, uint16 v2 = 0) : val1(v1), val2(v2), type(t) { }
		bool IsSongItem() const noexcept { return type >= MODITEM_BEGIN_SONGITEMS && type <= MODITEM_END_SONGITEMS; }
		bool operator==(const ModItem &other) const noexcept { return val1 == other.val1 && val2 == other.val2 && type == other.type; }
		bool operator!=(const ModItem &other) const noexcept { return !(*this == other); }
	};

	struct DlsItem : public ModItem
	{
		explicit DlsItem(uint16 instr) : ModItem(MODITEM_DLSBANK_INSTRUMENT, instr, 0) { }
		DlsItem(uint16 instr, uint16 region) : ModItem(MODITEM_DLSBANK_INSTRUMENT, (instr & DLS_INSTRMASK) | ((region << DLS_REGIONSHIFT) & DLS_REGIONMASK) | DLS_TYPEPERC, 0) { }

		uint32 GetRegion() const noexcept { return (val1 & DLS_REGIONMASK) >> DLS_REGIONSHIFT; }
		uint32 GetInstr() const noexcept { return (val1 & DLS_INSTRMASK); }
		bool IsPercussion() const noexcept { return ((val1 & DLS_TYPEPERC) == DLS_TYPEPERC); }
		bool IsMelodic() const noexcept { return !IsPercussion(); }

		static ModItem FromLPARAM(uint32 lparam) noexcept { return ModItem{MODITEM_DLSBANK_INSTRUMENT, lparam, 0}; }
		static LParam ToLPARAM(uint16 instr, uint16 region, bool isPerc) noexcept { return (instr & DLS_INSTRMASK) | ((region << DLS_REGIONSHIFT) & DLS_REGIONMASK) | (isPerc ? DLS_TYPEPERC : 0); }
	};

	static CTrackerSoundFile *m_SongFile;  // For browsing samples and instruments inside modules on disk
	CModTree *m_pDataTree = nullptr;  // Pointer to instrument browser (lower part of tree view) - if it's a nullptr, this object is the instrument browser itself.
	WindowHandle m_hDropWnd = nullptr;
	std::mutex m_WatchDirMutex;
	std::condition_variable m_watchDirSignal;
	bool m_isWatchDirSwitchPending = false;
	mpt::PathString m_WatchDir;
	bool m_isWatchDirKillRequested = false;
	std::thread m_WatchDirThread;
	ModItem m_itemDrag;
	uint32 m_dwStatus = 0;
	CModDoc *m_selectedDoc = nullptr, *m_dragDoc = nullptr;
	TreeItemHandle m_hItemDrag = nullptr, m_hItemDrop = nullptr;
	TreeItemHandle m_hInsLib = nullptr, m_hMidiLib = nullptr;
	TreeItemHandle m_tiMidi[128];
	TreeItemHandle m_tiPerc[128];
	std::vector<TreeItemHandle> m_tiDLS;
	std::map<const CModDoc *, ModTreeDocInfo> m_docInfo;
	mpt::ustring m_HelpText;

	std::unique_ptr<CDLSBank> m_cachedBank;
	mpt::PathString m_cachedBankName;

	// Instrument library
	mpt::PathString m_InstrLibPath;           // Current path to be explored
	mpt::PathString m_InstrLibHighlightPath;  // Folder to highlight in browser after a refresh
	mpt::PathString m_SongFileName;           // Name of open module, without path (== m_InstrLibPath).
	mpt::PathString m_previousPath;           // The folder from which we came from when navigating one folder up
	
	std::vector<const char*> m_modExtensions;                  // Cached in order to avoid querying too often when changing browsed folder

	struct FileBrowserEntry
	{
		mpt::ustring name;
		uint64 size;
		uint64 modtime;
		uint32 image;
		bool hidden;
	};
	std::vector<FileBrowserEntry> m_fileBrowserEntries;
	mpt::ustring m_filterString;

	enum class LibrarySortOrder
	{
		Name,
		Date,
		Size,
	};

	static LibrarySortOrder m_librarySort;

	int m_redrawLockCount = 0;

	bool m_showAllFiles = false;
	bool m_doLabelEdit = false;

public:
	CModTree(CModTree *pDataTree);
	~CModTree();

	void Init();
	bool InsLibSetFullPath(const mpt::PathString &libPath, const mpt::PathString &songFolder);
	mpt::PathString InsLibGetFullPath(TreeItemHandle hItem) const;
	bool SetSoundFile(FileReader &file);
	void RefreshMidiLibrary();
	void RefreshDlsBanks();
	void RefreshInstrumentLibrary();
	void MonitorInstrumentLibrary();
	ModItem GetModItem(TreeItemHandle hItem);
	bool SetMidiInstrument(uint32 nIns, const mpt::PathString &fileName);
	bool SetMidiPercussion(uint32 nPerc, const mpt::PathString &fileName);
	bool ExecuteItem(TreeItemHandle hItem);
	void DeleteTreeItem(TreeItemHandle hItem, const bool permanently);
	static void PlayDLSItem(const CDLSBank &dlsBank, const DlsItem &item, ModCommand::NOTE note);
	bool PlayItem(TreeItemHandle hItem, ModCommand::NOTE nParam, int volume = -1);
	bool OpenTreeItem(TreeItemHandle hItem);
	bool OpenMidiInstrument(uint32 dwItem);
	void SetFullInstrumentLibraryPath(mpt::PathString path);
	void InstrumentLibraryChDir(mpt::PathString dir, bool isSong);
	bool GetDropInfo(DRAGONDROP &dropInfo, mpt::PathString &fullPath);
	void OnOptionsChanged();
	void AddDocument(CModDoc &modDoc);
	void RemoveDocument(const CModDoc &modDoc);
	void UpdateView(ModTreeDocInfo &info, UpdateHint hint);
	void OnUpdate(CModDoc *pModDoc, UpdateHint hint, HintObject *pHint);
	bool CanDrop(TreeItemHandle hItem, bool bDoDrop);
	void UpdatePlayPos(CModDoc &modDoc, Notification *pNotify);
	bool IsItemExpanded(TreeItemHandle hItem) const;
	void DeleteChildren(TreeItemHandle hItem);
	TreeItemHandle GetNthChildItem(TreeItemHandle hItem, int index) const;
	TreeItemHandle GetParentRootItem(TreeItemHandle hItem) const;

	bool IsSampleBrowser() const { return m_pDataTree == nullptr; }
	CModTree *GetSampleBrowser() { return IsSampleBrowser() ? this : m_pDataTree; }
	CModTree *GetOtherView();

	void SetInstrumentLibraryFilter(const mpt::ustring &filter);
	void SetInstrumentLibraryFilterSortOrder(LibrarySortOrder sortType);
	void SortInstrumentLibrary();

// Overrides
public:
	bool PreTranslateMessage(int event) override;

// Drag & Drop operations
public:
	bool CanDropFiles(Point) const override { return true; }
	void OnDropFiles(const std::vector<mpt::PathString> &files) override;

protected:
	int ImageToSortOrder(int image) const;
	ModTreeDocInfo *GetDocumentInfoFromItem(TreeItemHandle hItem);
	CModDoc *GetDocumentFromItem(TreeItemHandle hItem) { ModTreeDocInfo *info = GetDocumentInfoFromItem(hItem); return info ? &info->modDoc : nullptr; }
	ModTreeDocInfo *GetDocumentInfoFromModDoc(CModDoc &modDoc);

	size_t GetDLSBankIndexFromItem(TreeItemHandle hItem) const;
	CDLSBank *GetDLSBankFromItem(TreeItemHandle hItem) const;

	void InsertOrDupItem(bool insert);
	void MoveTreeItem(TreeItemHandle hItem, bool moveUp);
	void OnItemRightClick(TreeItemHandle hItem, Point pt);

	static bool HasEffectPlugins(const CTrackerSoundFile &sndFile);
	static bool AllPluginsBypassed(const CTrackerSoundFile &sndFile, bool onlyEffects);
	static void BypassAllPlugins(CTrackerSoundFile &sndFile, bool bypass, bool onlyEffects);

	void FillInstrumentLibrary(const mpt::ustring &selectedItem = {});
	void FilterInstrumentLibrary(mpt::ustring filter, const mpt::ustring &selectedItem = {});

	HMENU AddLibraryFindAndSortMenus(HMENU hMenu) const;

	void LockRedraw()
	{
		if(!m_redrawLockCount++)
			SetRedraw(false);
	}

	void UnlockRedraw()
	{
		if(!--m_redrawLockCount)
			SetRedraw(true);
	}

protected:
	void OnDPIChanged();
	void OnLButtonUp(uint32 nFlags, Point point);
	void OnRButtonUp(uint32 nFlags, Point point);
	void OnXButtonUp(uint32 nFlags, uint32 nButton, Point point);
	void OnMouseMove(uint32 nFlags, Point point);
	void OnBeginDrag(TreeItemHandle, bool bLeft, LResult *pResult);
	void OnBeginLDrag(NotifyHeader *, LResult *pResult);
	void OnBeginRDrag(NotifyHeader *, LResult *pResult);
	void OnEndDrag(uint32 dwMask);
	void OnItemDblClk(NotifyHeader * phdr, LResult *pResult);
	void OnItemReturn(NotifyHeader *, LResult *pResult);
	void OnItemLeftClick(NotifyHeader * pNMHDR, LResult *pResult);
	void OnTreeRightClick(NotifyHeader *, LResult *pResult);
	void OnTreeItemExpanded(NotifyHeader * pnmhdr, LResult *pResult);
	void OnSelChanged(NotifyHeader * pnmhdr, LResult *pResult);
	void OnRefreshTree();
	void OnExecuteItem();
	void OnPlayTreeItem();
	void OnDeleteTreeItem();
	void OnOpenTreeItem();
	void OnMuteTreeItem();
	void OnMuteOnlyEffects();
	void OnSoloTreeItem();
	void OnUnmuteAllTreeItem();
	void OnDuplicateTreeItem() { InsertOrDupItem(false); }
	void OnInsertTreeItem() { InsertOrDupItem(true); }
	void OnSwitchToTreeItem();	// hack for sequence items to avoid double-click action
	void OnCloseItem();
	void OnRenameItem();
	void OnBeginLabelEdit(NotifyHeader *nmhdr, LResult *result);
	void OnEndLabelEdit(NotifyHeader *nmhdr, LResult *result);
	void OnKillFocus(Wnd *pNewWnd);
	void OnSetFocus(Wnd *pOldWnd);
	void OnDestroy();

	void OnSetItemPath();
	void OnSaveItem();
	void OnSaveAll();
	void OnReloadItem();
	void OnReloadAll();
	void OnFindMissing();

	void OnAddDlsBank();
	void OnImportMidiLib();
	void OnExportMidiLib();
	void OnSoundBankProperties();
	void OnRefreshInstrLib();
	void OnShowDirectories();
	void OnShowAllFiles();
	void OnShowSoundFiles();

	void OnGotoInstrumentDir();
	void OnGotoSampleDir();

	void OnOpenInstrumentLibraryFilter();
	void OnSortByName() { SetInstrumentLibraryFilterSortOrder(LibrarySortOrder::Name); }
	void OnSortByDate() { SetInstrumentLibraryFilterSortOrder(LibrarySortOrder::Date); }
	void OnSortBySize() { SetInstrumentLibraryFilterSortOrder(LibrarySortOrder::Size); }

	LResult OnCustomKeyMsg(WParam, LParam);
	LResult OnMidiMsg(WParam midiData, LParam);
	UI_DECLARE_MESSAGE_MAP()
};


OPENMPT_NAMESPACE_END
