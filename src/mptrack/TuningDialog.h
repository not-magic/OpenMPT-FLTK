/*
 * TuningDialog.h
 * --------------
 * Purpose: Alternative sample tuning configuration dialog.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/TuningDialog.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "CDecimalSupport.h"
#include "ResizableDialog.h"
#include "tuningcollection.h"
#include "tuningRatioMapWnd.h"

OPENMPT_NAMESPACE_BEGIN


// Tunings exist even outside of CSoundFile objects. We thus cannot use the
// GetCharsetInternal() encoding consistently. For now, just always treat
// tuning strings as Charset::Locale. As of OpenMPT 1.27, this distinction does
// not yet matter, because GetCharsetInteral() is always mpt::Charset::Locale if
// MODPLUG_TRACKER anyway.
extern const mpt::Charset TuningCharsetFallback;

template<class T1, class T2>
class CBijectiveMap
{
public:
	CBijectiveMap(const T1& a, const T2& b)
		:	m_NotFoundT1(a),
			m_NotFoundT2(b)
	{}

	void AddPair(const T1& a, const T2& b)
	{
		m_T1.push_back(a);
		m_T2.push_back(b);
	}

	void ClearMapping()
	{
		m_T1.clear();
		m_T2.clear();
	}

	size_t Size() const
	{
		MPT_ASSERT(m_T1.size() == m_T2.size());
		return m_T1.size();
	}

	void RemoveValue_1(const T1& a)
	{
		auto iter = find(m_T1.begin(), m_T1.end(), a);
		if(iter != m_T1.end())
		{
			m_T2.erase(m_T2.begin() + (iter-m_T1.begin()));
			m_T1.erase(iter);
		}
	}

	void RemoveValue_2(const T2& b)
	{
		auto iter = find(m_T2.begin(), m_T2.end(), b);
		if(iter != m_T2.end())
		{
			m_T1.erase(m_T1.begin() + (iter-m_T2.begin()));
			m_T2.erase(iter);
		}
	}

	T2 GetMapping_12(const T1& a) const
	{
		auto iter = find(m_T1.begin(), m_T1.end(), a);
		if(iter != m_T1.end())
		{
			return m_T2[iter-m_T1.begin()];
		}
		else
			return m_NotFoundT2;
	}

	T1 GetMapping_21(const T2& b) const
	{
		auto iter = find(m_T2.begin(), m_T2.end(), b);
		if(iter != m_T2.end())
		{
			return m_T1[iter-m_T2.begin()];
		}
		else
			return m_NotFoundT1;
	}

private:
	//Elements are collected to two arrays so that elements with the
	//same index are mapped to each other.
	std::vector<T1> m_T1;
	std::vector<T2> m_T2;

	T1 m_NotFoundT1;
	T2 m_NotFoundT2;
};

class CTuningDialog;

class CTuningTreeCtrl : public TreeCtrl
{
private:
	CTuningDialog &m_rParentDialog;
	bool m_Dragging = false;

public:
	CTuningTreeCtrl(CTuningDialog* parent)
		: m_rParentDialog(*parent)
	{}
	//Note: Parent address may be given in its initializer list.

	void SetDragging(bool state = true) {m_Dragging = state;}
	bool IsDragging() {return m_Dragging;}

	void OnMouseMove(uint32 nFlags, Point point);
	void OnLButtonUp(uint32 nFlags, Point point);
	UI_DECLARE_MESSAGE_MAP()
};

class CTuningTreeItem
{
private:
	CTuning *m_pTuning = nullptr;
	CTuningCollection *m_pTuningCollection = nullptr;

public:
	CTuningTreeItem() = default;

	CTuningTreeItem(CTuning* pT) :
					m_pTuning(pT),
					m_pTuningCollection(nullptr)
	{}

	CTuningTreeItem(CTuningCollection* pTC) :
					m_pTuning(nullptr),
					m_pTuningCollection(pTC)
	{}

	bool operator==(const CTuningTreeItem& ti) const
	{
		if(m_pTuning == ti.m_pTuning &&
			m_pTuningCollection == ti.m_pTuningCollection)
			return true;
		else
			return false;
	}

	void Reset() {m_pTuning = nullptr; m_pTuningCollection = nullptr;}


	void Set(CTuning* pT)
	{
		m_pTuning = pT;
		m_pTuningCollection = nullptr;
	}

	void Set(CTuningCollection* pTC)
	{
		m_pTuning = nullptr;
		m_pTuningCollection = pTC;
	}

	operator bool () const
	{
		return m_pTuning || m_pTuningCollection;
	}
	bool operator ! () const
	{
		return !operator bool();
	}

	CTuningCollection* GetTC() {return m_pTuningCollection;}

	CTuning* GetT() {return m_pTuning;}
};

// CTuningDialog dialog

class CTuningDialog : public ResizableDialog
{
	friend class CTuningTreeCtrl;

	enum EnSclImport
	{
		enSclImportOk,
		enSclImportFailTooLargeNumDenomIntegers,
		enSclImportFailZeroDenominator,
		enSclImportFailNegativeRatio,
		enSclImportFailUnableToOpenFile,
		enSclImportLineCountMismatch,
		enSclImportTuningCreationFailure,
		enSclImportAddTuningFailure,
		enSclImportFailTooManyNotes
	};

public:
	using TUNINGVECTOR = std::vector<CTuningCollection*>;

public:
	CTuningDialog(Wnd *pParent, INSTRUMENTINDEX inst, CTrackerSoundFile &csf);
	~CTuningDialog() override;

	bool OnInitDialog() override;

	void UpdateRatioMapEdits(const Tuning::NOTEINDEXTYPE&);

	bool GetModifiedStatus(const CTuningCollection* const pTc) const;

protected:
	void DoDataExchange(DataExchange* pDX) override;    // DDX/DDV support

private:

	bool CanEdit(CTuningCollection * pTC) const;
	bool CanEdit(CTuning * pT, CTuningCollection * pTC) const;

	void UpdateView(const int UpdateMask = 0);
	void UpdateTuningType();

	TreeItemHandle AddTreeItem(CTuningCollection* pTC, TreeItemHandle parent, TreeItemHandle insertAfter);
	TreeItemHandle AddTreeItem(CTuning* pT, TreeItemHandle parent, TreeItemHandle insertAfter);

	void DeleteTreeItem(CTuning* pT);
	void DeleteTreeItem(CTuningCollection* pTC);

	// Check if item can be dropped here. If yes, the target collection is returned, otherwise nullptr.
	CTuningCollection *CanDrop(TreeItemHandle dragDestItem);
	void OnEndDrag(TreeItemHandle dragDestItem);

	//Returns pointer to the tuning collection where tuning given as argument
	//belongs to.
	CTuningCollection* GetpTuningCollection(const CTuning* const) const;

	//Returns the address of corresponding tuningcollection; if it points
	//to tuning-entry, returning the owning tuningcollection
	CTuningCollection* GetpTuningCollection(TreeItemHandle ti) const;

	//Checks whether tuning collection can be deleted.
	bool IsDeletable(const CTuningCollection* const pTC) const;

	// Scl-file import.
	EnSclImport ImportScl(const mpt::PathString &filename, const mpt::ustring &name, std::unique_ptr<CTuning> & result);
	EnSclImport ImportScl(std::istream& iStrm, const mpt::ustring &name, std::unique_ptr<CTuning> & result);


private:

	CTrackerSoundFile & m_sndFile;

	CTuningRatioMapWnd m_RatioMapWnd;
	TUNINGVECTOR m_TuningCollections;
	std::vector<CTuningCollection*> m_DeletableTuningCollections;

	std::map<const CTuningCollection*, mpt::ustring> m_TuningCollectionsNames;
	std::map<const CTuningCollection*, mpt::PathString> m_TuningCollectionsFilenames;

	CTuning *m_pActiveTuning = nullptr;
	CTuningCollection *m_pActiveTuningCollection = nullptr;

	ComboBox m_CombobTuningType;

	//Tuning Edits-->
	Edit m_EditSteps;
	CNumberEdit m_EditRatioPeriod;
	CNumberEdit m_EditRatio;
	Edit m_EditNotename;
	Edit m_EditMiscActions;
	Edit m_EditFineTuneSteps;
	Edit m_EditName;
	//<--Tuning Edits

	Button m_ButtonSet;
	Button m_ButtonNew;
	Button m_ButtonExport;
	Button m_ButtonImport;
	Button m_ButtonRemove;

	CTuningTreeCtrl m_TreeCtrlTuning;

private:
	using TUNINGTREEITEM = CTuningTreeItem;
	using TREETUNING_MAP = CBijectiveMap<TreeItemHandle, TUNINGTREEITEM>;
	TREETUNING_MAP m_TreeItemTuningItemMap;

	TUNINGTREEITEM m_DragItem;
	TUNINGTREEITEM m_CommandItemSrc;
	TUNINGTREEITEM m_CommandItemDest;
	//Commanditem is used when receiving context menu-commands,
	//m_CommandItemDest is used when the command really need only
	//one argument.

	using MODIFIED_MAP = std::map<const CTuningCollection* const, bool>;
	MODIFIED_MAP m_ModifiedTCs;
	//If tuning collection seems to have been modified, its address
	//is added to this map.

	enum
	{
		TT_TUNINGCOLLECTION = 1,
		TT_TUNING
	};

	static mpt::ustring GetSclImportFailureMsg(EnSclImport);
	static constexpr size_t s_nSclImportMaxNoteCount = 256;

	//To indicate whether to apply changes made to
	//those edit boxes(they are modified by certain activities
	//in case which the modifications should not be applied to
	//tuning data.
	bool m_NoteEditApply = true;
	bool m_RatioEditApply = true;

	enum
	{
		UM_TUNINGDATA = 1, //UM <-> Update Mask
		UM_TUNINGCOLLECTION = 2,
	};

	static const TUNINGTREEITEM s_notFoundItemTuning;
	static const TreeItemHandle s_notFoundItemTree;

	bool AddTuning(CTuningCollection*, CTuning* pT);
	bool AddTuning(CTuningCollection*, Tuning::Type type);

	//Flag to prevent multiple exit error-messages.
	bool m_DoErrorExit = false;

	void DoErrorExit();

	void OnOK() override;

//Treectrl context menu functions.
public:
	void OnRemoveTuning();
	void OnAddTuningGeneral();
	void OnAddTuningGroupGeometric();
	void OnAddTuningGeometric();
	void OnCopyTuning();
	void OnRemoveTuningCollection();

//Event-functions
public:
	void OnEnChangeEditSteps();
	void OnEnChangeEditRatioperiod();
	void OnEnChangeEditNotename();
	void OnBnClickedButtonSetvalues();
	void OnEnChangeEditRatiovalue();
	void OnBnClickedButtonNew();
	void OnBnClickedButtonExport();
	void OnBnClickedButtonImport();
	void OnBnClickedButtonRemove();
	void OnEnChangeEditFinetunesteps();
	void OnEnKillfocusEditFinetunesteps();
	void OnEnKillfocusEditName();
	void OnEnKillfocusEditSteps();
	void OnEnKillfocusEditRatioperiod();
	void OnEnKillfocusEditRatiovalue();
	void OnEnKillfocusEditNotename();

	//Treeview events
	void OnTvnSelchangedTreeTuning(NotifyHeader *pNMHDR, LResult *pResult);
	void OnTvnDeleteitemTreeTuning(NotifyHeader *pNMHDR, LResult *pResult);
	void OnNMRclickTreeTuning(NotifyHeader *pNMHDR, LResult *pResult);
	void OnTvnBegindragTreeTuning(NotifyHeader *pNMHDR, LResult *pResult);

	UI_DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
