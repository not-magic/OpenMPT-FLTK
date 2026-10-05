/*
 * ProgressDialog.h
 * ----------------
 * Purpose: An abortable, progress-indicating dialog, e.g. for showing conversion progress.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/ProgressDialog.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"
#include "DialogBase.h"

OPENMPT_NAMESPACE_BEGIN

class CProgressDialog : public DialogBase
{
private:
	uint64 m_min = 0, m_max = 0, m_shift = 0;
	const bool m_customDialog;

public:
	bool m_abort = false;

	CProgressDialog(Wnd *parent = nullptr, uint32 resourceID = 0);
	~CProgressDialog();

	// Set the window title
	void SetTitle(const mpt::ustring &title);
	// Set the text on abort button
	void SetAbortText(const mpt::ustring &abort);
	// Set the text to be displayed along the progress bar.
	void SetText(const mpt::ustring &text);
	// Set the minimum and maximum value of the progress bar.
	void SetRange(uint64 min, uint64 max);
	// Set the current progress.
	void SetProgress(uint64 progress);
	// Show the progress in the task bar as well (not supported on all platforms)
	void EnableTaskbarProgress() { }
	// Process all queued events of the user interface
	void ProcessMessages();
	// Run method for this dialog that must implement the action to be carried out - unless a custom resourceID was provided,
	// in which case the user is responsible for running the dialog and the implementation of this function may be a no-op.
	virtual void Run() = 0;

protected:
	bool OnInitDialog() override;
	void OnCancel() override { m_abort = true; }

	UI_DECLARE_MESSAGE_MAP()
};

OPENMPT_NAMESPACE_END
