// FLTK port of openmpt/mptrack/DialogBase.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"
#include "Snd_defs.h"

OPENMPT_NAMESPACE_BEGIN

class DialogBase : public Dialog
{
public:
	enum class InputDevice : uint8
	{
		Unknown,
		Mouse,
		Keyboard,
	};

	using Dialog::Dialog;

	bool OnInitDialog() override;
	bool PreTranslateMessage(int event) override;

	// Gives the key binding system the chance to handle a key press. Returns true if the key was mapped to a command.
	static bool HandleGlobalKeyMessage(uint32 key, uint32 flags);

	int GetDPI() const { return ui::kLogicalDpi; }

	void SetFocusToFirstControl();

protected:
	virtual void OnDPIChanged() {}

	virtual mpt::ustring GetToolTipText(uint32 /*id*/, Wnd * /*control*/) const { return {}; }

	// Attaches the texts of GetToolTipText() to the controls
	void UpdateToolTips();

	UI_DECLARE_MESSAGE_MAP()

private:
	std::vector<std::unique_ptr<std::string>> m_tooltipStorage;

protected:
	InputDevice m_lastInputDevice = InputDevice::Unknown;
};

OPENMPT_NAMESPACE_END
