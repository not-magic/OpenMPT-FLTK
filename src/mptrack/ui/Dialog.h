// MFC replacement on FLTK. Dialog windows (modal and modeless) built from the dialog templates in the resource
// tables.

#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "ResourceTables.h"
#include "Wnd.h"

#include <FL/Fl_Double_Window.H>

#include <functional>
#include <map>
#include <memory>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{

class Dialog;
class Menu;
class MenuBar;


// Passed to Dialog::DoDataExchange(). Binds members to controls, and tells whether controls are read or written.
class DataExchange
{
public:
	DataExchange(Dialog &dialog, bool isSaveAndValidate) : m_dialog(dialog), m_isSaveAndValidate(isSaveAndValidate) { }

	// Replaces the control that was created from the template with the member object
	void BindControl(uint32 id, Wnd &member);

	bool IsSaveAndValidate() const noexcept { return m_isSaveAndValidate; }
	Dialog &GetDialog() noexcept { return m_dialog; }

	bool m_bSaveAndValidate = false;

private:
	Dialog &m_dialog;
	bool m_isSaveAndValidate;
};


// Creates controls for a class name found in dialog templates
using ControlFactory = std::function<Wnd *(const DialogControl &control, int x, int y, int width, int height)>;
void RegisterControlClass(const std::string &className, ControlFactory factory);

// Conversion of dialog units to pixels
int DialogUnitsToPixelsX(int units);
int DialogUnitsToPixelsY(int units);
// Conversion of vertical sizes laid out for the Windows dialog font (stored settings, hard-coded defaults)
int WindowsPixelsToPixelsY(int pixels);
int PixelsToWindowsPixelsY(int pixels);


class Dialog : public Panel
{
public:
	Dialog();
	explicit Dialog(uint32 templateId, Wnd *parent = nullptr);
	~Dialog() override;

	void SetTemplate(uint32 templateId) noexcept { m_templateId = templateId; }
	int CalcTemplateHeight() const;

	// Modeless creation. The dialog stays alive until DestroyWindow() is called.
	bool Create(uint32 templateId, Wnd *parent = nullptr);
	// Builds the child controls inside an existing parent, without opening a window of its own (property pages and nested dialogs)
	bool CreateChild(uint32 templateId, Wnd &parent, int x = 0, int y = 0);
	intptr_t DoModal();
	void EndDialog(intptr_t result);
	bool IsModal() const noexcept { return m_isModal; }

	// Reads the controls (isSaveAndValidate) or writes them
	bool UpdateData(bool isSaveAndValidate = true);

	void CenterWindow(Wnd *relativeTo = nullptr);
	// Lets the user change the size of the window; the dialog is resized with it
	void SetResizable(bool isResizable) noexcept { m_isResizable = isResizable; }
	bool IsResizable() const noexcept { return m_isResizable; }
	Fl_Window *GetFrameWindow() const noexcept { return m_frame; }

	virtual bool OnInitDialog();
	// The window of a modeless dialog was activated or deactivated by the user
	virtual void OnActivate(bool /*isActive*/) {}
	// The window of the dialog was moved on the screen
	virtual void OnMove(int /*x*/, int /*y*/) {}
	// The menu bar is about to open a menu
	virtual void OnInitMenu() {}
	virtual void OnOK();
	virtual void OnCancel();
	virtual void DoDataExchange(DataExchange *dx);
	// Called after a modeless dialog has been closed
	virtual void PostNcDestroy();
	// Closes a modeless dialog and calls PostNcDestroy()
	void DestroyWindow() override;
	bool IsWindow() const override { return m_frame != nullptr || static_cast<const Wnd *>(this)->GetWidget()->parent() != nullptr; }

	// Shows a menu bar at the top of the dialog; the dialog is made taller to make room for it
	void SetMenu(Menu *menu);
	Menu *GetMenu() const noexcept { return m_menu; }
	void DrawMenuBar();

	void resize(int x, int y, int width, int height) override;

	void NextDlgCtrl() { }
	void GotoDlgCtrl(Wnd *control);

	UI_DECLARE_MESSAGE_MAP()

protected:
	void CreateControls();
	bool CreateFrame(Wnd *parent, bool isModal);
	void DestroyFrame();
	void Finish(intptr_t result);

	uint32 m_templateId = 0;
	intptr_t m_result = 0;
	bool m_isModal = false;
	bool m_isDone = false;
	bool m_isInitialized = false;
	bool m_isResizable = false;
	bool m_isSyncingFrame = false;
	Fl_Double_Window *m_frame = nullptr;
	std::map<uint32, const DialogControl *> m_controlTemplates;
	// Up-down controls merged into spinners, by the spinner's ID
	std::map<uint32, const DialogControl *> m_spinTemplates;
	Menu *m_menu = nullptr;
	MenuBar *m_menuBar = nullptr;

private:
	friend class DataExchange;
};


}  // namespace ui


OPENMPT_NAMESPACE_END
