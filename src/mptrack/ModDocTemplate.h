/*
 * ModDocTemplate.h
 * ----------------
 * Purpose: CDocTemplate and CModDocManager specializations for CModDoc.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/ModDocTemplate.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "mpt/base/namespace.hpp"

#include "../common/mptPathString.h"

#include <unordered_set>

OPENMPT_NAMESPACE_BEGIN

class CModDoc;

class CModDocTemplate: public DocTemplate
{
	std::unordered_set<CModDoc *> m_documents;	// Allow faster lookup of open documents than a linear search allows for

public:
	CModDocTemplate(uint32 resourceId, DocumentFactory documentFactory, FrameFactory frameFactory):
		DocTemplate(resourceId, std::move(documentFactory), std::move(frameFactory)) {}

	Document* OpenTemplateFile(const mpt::PathString &filename, bool isExampleTune = false);

	Document* OpenDocumentFile(const mpt::PathString &path, bool addToMru = true, bool makeVisible = true) override;

	void AddDocument(Document *doc) override;
	void RemoveDocument(Document *doc) override;
	bool DocumentExists(const CModDoc *doc) const;

	size_t size() const { return m_documents.size(); }
	bool empty() const { return m_documents.empty(); }
	auto begin() { return m_documents.begin(); }
	auto begin() const { return m_documents.begin(); }
	auto cbegin() const { return m_documents.cbegin(); }
	auto end() { return m_documents.end(); }
	auto end() const { return m_documents.end(); }
	auto cend() const { return m_documents.cend(); }
};

OPENMPT_NAMESPACE_END
