/*
 * ModDocTemplate.cpp
 * ------------------
 * Purpose: CDocTemplate and CModDocManager specialization for CModDoc.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "ui/Ui.h"
#include "ModDocTemplate.h"
#include "FolderScanner.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "SelectPluginDialog.h"
#include "TrackerSettings.h"
#include "../soundlib/plugins/PluginManager.h"

OPENMPT_NAMESPACE_BEGIN


Document *CModDocTemplate::OpenDocumentFile(const mpt::PathString &filename, bool addToMru, bool makeVisible)
{
	// First, remove document from MRU list.
	if(addToMru)
	{
		theApp.RemoveMruItem(filename);
	}

	Document *pDoc = DocTemplate::OpenDocumentFile(filename, addToMru, makeVisible);
	if(pDoc)
	{
		CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
		if (pMainFrm) pMainFrm->OnDocumentCreated(static_cast<CModDoc *>(pDoc));
	} else if(!filename.empty() && CMainFrame::GetMainFrame() && addToMru)
	{
		// Opening the document failed
		CMainFrame::GetMainFrame()->UpdateMRUList();
	}
	return pDoc;
}


Document *CModDocTemplate::OpenTemplateFile(const mpt::PathString &filename, bool isExampleTune)
{
	Document *doc = OpenDocumentFile(filename, isExampleTune ? true : false, true);
	if(doc)
	{
		CModDoc *modDoc = static_cast<CModDoc *>(doc);
		// Clear path so that saving will not take place in templates/examples folder.
		modDoc->ClearFilePath();
		if(!isExampleTune)
		{
			SetDefaultTitle(modDoc);
			m_nUntitledCount++;
			// Name has changed...
			CMainFrame::GetMainFrame()->UpdateTree(modDoc, GeneralHint().General());

			// Reset edit history for template files
			CSoundFile &sndFile = modDoc->GetSoundFile();
			sndFile.GetFileHistory().clear();
			sndFile.m_dwCreatedWithVersion = Version::Current();
			sndFile.m_dwLastSavedWithVersion = Version();
			sndFile.m_modFormat = ModFormatDetails();
			sndFile.m_songArtist = TrackerSettings::Instance().defaultArtist;
			if(sndFile.GetType() != MOD_TYPE_MPT)
			{
				// Always enforce most compatible playback for legacy module types
				sndFile.m_playBehaviour = sndFile.GetDefaultPlaybackBehaviour(sndFile.GetType());
			}
			doc->UpdateAllViews(nullptr, UpdateHint().ModType().AsLPARAM());
		} else
		{
			// Remove extension from title, so that saving the file will not suggest a filename like e.g. "example.it.it".
			const mpt::ustring title = modDoc->GetTitle();
			const std::size_t dotPos = title.rfind(UL_('.'));
			if(dotPos != mpt::ustring::npos)
			{
				modDoc->SetTitle(title.substr(0, dotPos));
			}
		}
	}
	return doc;
}


void CModDocTemplate::AddDocument(Document *doc)
{
	DocTemplate::AddDocument(doc);
	m_documents.insert(static_cast<CModDoc *>(doc));
	// At this point we don't know yet if opening the document is going to be successful, so don't call UpdateDocumentCount()
}


void CModDocTemplate::RemoveDocument(Document *doc)
{
	DocTemplate::RemoveDocument(doc);
	m_documents.erase(static_cast<CModDoc *>(doc));
	CMainFrame::GetMainFrame()->UpdateDocumentCount();
}


bool CModDocTemplate::DocumentExists(const CModDoc *doc) const
{
	return m_documents.count(const_cast<CModDoc *>(doc)) != 0;
}


OPENMPT_NAMESPACE_END
