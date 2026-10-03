// FLTK port of openmpt/mptrack/DefaultVstEditor.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "Mptrack.h"
#include "AbstractVstEditor.h"

OPENMPT_NAMESPACE_BEGIN

enum
{
	PARAM_RESOLUTION = 1000,
	NUM_PLUGINEDITOR_PARAMETERS = 8,	// Parameters on screen
};

struct PluginEditorMeasurements;

class ParamControlSet
{
protected:
	HSlider valueSlider;
	Edit valueEdit;
	Static nameLabel;
	Static valueLabel;
	Static perMilLabel;

public:
	ParamControlSet(Wnd *parent, const Rect &rect, int setID, const PluginEditorMeasurements &m);
	~ParamControlSet();

	void EnableControls(bool enable = true);
	void ResetContent();

	void SetParamName(const mpt::ustring &name);
	void SetParamValue(int value, const mpt::ustring &text);

	int GetParamValueFromSlider() const;
	int GetParamValueFromEdit() const;

	int GetSliderID() const { return valueSlider.GetDlgCtrlID(); };
};


class CDefaultVstEditor : public CAbstractVstEditor
{
protected:

	std::vector<ParamControlSet *> controls;

	ScrollBar paramScroller;
	PlugParamIndex paramOffset;

	int m_nControlLock;

public:

	CDefaultVstEditor(IMixPlugin &plugin);
	~CDefaultVstEditor() override;

	void UpdateParamDisplays() override { CAbstractVstEditor::UpdateParamDisplays(); UpdateControls(false); };

	bool OpenEditor(Wnd *parent) override;

	// Plugins may not request to change the GUI size, since we use our own GUI.
	bool IsResizable() const override { return false; };
	bool SetSize(int, int) override { return false; };

protected:

	void DoDataExchange(DataExchange *pDX) override;
	
	UI_DECLARE_MESSAGE_MAP()

	void OnParamTextboxChanged(uint32 id);
	void OnParamSliderChanged(uint32 id);

	void OnHScroll(uint32 nSBCode, uint32 nPos, Wnd * pScrollBar) override;
	void OnVScroll(uint32 nSBCode, uint32 nPos, Wnd * pScrollBar) override;
	bool OnMouseWheel(uint32 nFlags, short zDelta, Point pt);

protected:

	void CreateControls();
	void UpdateControls(bool updateParamNames);
	void SetParam(PlugParamIndex param, int value);
	void UpdateParamDisplay(PlugParamIndex param);

};

OPENMPT_NAMESPACE_END
