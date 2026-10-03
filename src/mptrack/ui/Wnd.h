/*
 * Wnd.h
 * -----
 * Purpose: Base class of all windows, dialogs, views and controls of the GUI. Adds message routing, timers and per-window helpers on top of FLTK.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../Geometry.h"
#include "../UiTypes.h"
#include "Input.h"
#include "MessageMap.h"
#include "Painter.h"

#include <FL/Fl.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Window.H>

#include <functional>
#include <map>
#include <type_traits>
#include <vector>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{


class ChildFrameBase;


// Anything that can receive commands: windows, documents, the application object
class CommandTarget
{
public:
	virtual ~CommandTarget() = default;

	// Handles a command or notification. Returns true if some handler consumed it.
	virtual bool OnCmdMsg(uint32 id, uint32 code, void *extra, LResult *result = nullptr);

	// Shows the wait cursor in all windows until the matching EndWaitCursor; calls can be nested
	static void BeginWaitCursor();
	static void EndWaitCursor();

	static const MessageMap messageMap;

protected:
	virtual const MessageMap *GetMessageMap() const { return &messageMap; }
};


// Flags of Wnd::SetWindowPos
enum WindowPos : uint32
{
	PosNoSize = 0x0001,
	PosNoMove = 0x0002,
	PosNoZOrder = 0x0004,
	PosNoRedraw = 0x0008,
	PosNoActivate = 0x0010,
	PosFrameChanged = 0x0020,
	PosShowWindow = 0x0040,
	PosHideWindow = 0x0080,
	PosNoCopyBits = 0x0100,
	PosNoOwnerZOrder = 0x0200,
	PosNoReposition = 0x0200,
	PosDrawFrame = 0x0020,
};


// Reasons of a size change
constexpr uint32 SIZE_RESTORED = 0;
constexpr uint32 SIZE_MAXIMIZED = 2;


// Show states of a window
enum ShowCommand : uint32
{
	SW_HIDE = 0,
	SW_SHOWNORMAL = 1,
	SW_SHOWMINIMIZED = 2,
	SW_SHOWMAXIMIZED = 3,
};


// Position and state of a window as stored in settings
struct WindowPlacement
{
	uint32 length = sizeof(uint32) * 5 + sizeof(int32) * 8;
	uint32 flags = 0;
	uint32 showCmd = SW_SHOWNORMAL;
	Point ptMinPosition;
	Point ptMaxPosition;
	Rect rcNormalPosition;
};
using WINDOWPLACEMENT = WindowPlacement;


// Scroll bar and slider notification codes
enum ScrollCode : uint32
{
	ScrollLineUp = 0,
	ScrollLineLeft = 0,
	ScrollLineDown = 1,
	ScrollLineRight = 1,
	ScrollPageUp = 2,
	ScrollPageLeft = 2,
	ScrollPageDown = 3,
	ScrollPageRight = 3,
	ScrollThumbPosition = 4,
	ScrollThumbTrack = 5,
	ScrollTop = 6,
	ScrollLeft = 6,
	ScrollBottom = 7,
	ScrollRight = 7,
	ScrollEndScroll = 8,
};


// Interface of every window-like object. The concrete FLTK widget is reached through GetWidget().
struct DialogControl;


class Wnd : public CommandTarget, public HintObject
{
public:
	Wnd();

	// Gives a derived class the chance to consume an event before the default dispatch
	virtual bool PreTranslateMessage(int event);
	~Wnd() override;

	Wnd(const Wnd &) = delete;
	Wnd &operator=(const Wnd &) = delete;

	virtual Fl_Widget *GetWidget() = 0;

	// Adjusts the control to the style flags of its dialog template entry
	virtual void ConfigureFromTemplate(const DialogControl &control);
	const Fl_Widget *GetWidget() const { return const_cast<Wnd *>(this)->GetWidget(); }

	// Geometry. Window rectangles are in screen coordinates, client rectangles start at (0, 0).
	virtual Rect GetClientRect() const;
	Rect GetWindowRect() const;
	void GetWindowRect(Rect &rect) const { rect = GetWindowRect(); }
	void GetClientRect(Rect &rect) const { rect = GetClientRect(); }
	void GetClientRect(Rect *rect) const { *rect = GetClientRect(); }
	void GetWindowRect(Rect *rect) const { *rect = GetWindowRect(); }
	Rect GetRectInParent() const;
	void ScreenToClient(Point &pt) const;
	void ScreenToClient(Rect &rect) const;
	void ClientToScreen(Point &pt) const;
	void ClientToScreen(Rect &rect) const;
	void ScreenToClient(Point *pt) const { ScreenToClient(*pt); }
	void ScreenToClient(Rect *rect) const { ScreenToClient(*rect); }
	void ClientToScreen(Point *pt) const { ClientToScreen(*pt); }
	void ClientToScreen(Rect *rect) const { ClientToScreen(*rect); }
	// Moves all child windows by an offset, clipping them to this window
	void ScrollChildren(int dx, int dy);
	// The frame that holds this window
	ChildFrameBase *GetParentFrame() const;
	void MoveWindow(int x, int y, int width, int height, bool shouldRedraw = true);
	void MoveWindow(const Rect &rect, bool shouldRedraw = true) { MoveWindow(rect.left, rect.top, rect.Width(), rect.Height(), shouldRedraw); }
	void MoveWindow(const Rect *rect, bool shouldRedraw = true) { MoveWindow(*rect, shouldRedraw); }
	// Position of the window in the coordinates of its parent, or of the screen for top-level windows
	bool GetWindowPlacement(WindowPlacement *placement) const;
	bool SetWindowPlacement(const WindowPlacement *placement);
	// Positions a child window; the rectangle is relative to the top left of this window
	void MoveChild(Wnd &child, const Rect &rect);
	// Positions the window relative to its parent. Both size and position can be left unchanged.
	void SetWindowPos(int x, int y, int width, int height, bool shouldMove = true, bool shouldSize = true);
	// Same with window position flags; the insertion window is ignored
	void SetWindowPos(const Wnd *, int x, int y, int width, int height, uint32 flags);

	// State
	void ShowWindow(bool isVisible);
	bool IsWindowVisible() const { return GetWidget()->visible_r() != 0; }
	void EnableWindow(bool isEnabled = true);
	bool IsWindowEnabled() const { return GetWidget()->active_r() != 0; }
	// Whether the window has been created and not yet destroyed
	virtual bool IsWindow() const;
	void SetFocus();
	// Whether the window is a descendant of this window
	bool IsChild(const Wnd *wnd) const;
	bool HasFocus() const;
	static Wnd *GetFocus();
	void Invalidate(bool shouldErase = true);
	// FLTK has no way to suspend painting; the window is only redrawn when drawing is switched on again
	void SetRedraw(bool isRedraw = true) { if(isRedraw) Invalidate(); }
	// Replaces the control of a dialog template that has the given ID with this object
	bool SubclassDlgItem(uint32 id, Wnd *parent);
	// Places the window inside the parent at a position given in the client coordinates of the parent
	bool CreateChild(Wnd &parent, const Rect &rect, uint32 id);
	void InvalidateRect(const Rect *rect, bool shouldErase = true);
	void UpdateWindow();
	virtual void SetWindowText(const mpt::ustring &text);
	virtual mpt::ustring GetWindowText() const;
	void GetWindowText(mpt::ustring &text) const { text = GetWindowText(); }
	// Copies at most maximumCount - 1 characters and the terminating zero; returns the number of characters copied
	int GetWindowText(mpt::uchar *buffer, int maximumCount) const;
	void SetFont(const Font &font);
	const Font &GetFont() const noexcept { return m_font; }
	void SetCapture();
	void ReleaseCapture();
	static Wnd *GetCapture();
	void SetCursorShape(Fl_Cursor cursor);
	void SetCursor(Fl_Cursor cursor) { SetCursorShape(cursor); }
	void SetForegroundWindow();
	// Fixed tooltip for the whole window
	void SetToolTipText(const mpt::ustring &text);
	// Shows a tooltip for an area of the window (client coordinates) while the mouse stays in it
	void ShowToolTip(const Rect &area, const mpt::ustring &text);

	// Text that assistive technology reads out for the window; stored for such software to query
	void SetAccessibleName(const mpt::ustring &name) { m_accessibleName = name; }
	void SetAccessibleSuffix(const mpt::ustring &suffix) { m_accessibleSuffix = suffix; }
	void SetAccessibleText(const mpt::ustring &text) { m_accessibleText = text; }
	virtual void UpdateAccessibleTitle() { }

	// Hierarchy
	Wnd *GetParent() const;
	Wnd *GetTopLevelParent();
	Wnd *GetDlgItem(uint32 id) const;
	uint32 GetDlgCtrlID() const { return static_cast<uint32>(GetWidget()->argument()); }
	void SetDlgCtrlID(uint32 id) { GetWidget()->argument(static_cast<long>(id)); }
	// Closes and detaches the window; the object itself is not deleted
	virtual void DestroyWindow();

	// Items identified by control ID
	void SetDlgItemText(uint32 id, const mpt::ustring &text);
	mpt::ustring GetDlgItemText(uint32 id) const;
	void GetDlgItemText(uint32 id, mpt::ustring &text) const { text = GetDlgItemText(id); }
	// Window icons are provided by the window manager; nothing is drawn by the toolkit
	void SetIcon(const void *, bool) { }
	// Value that the owner of the window can attach to it
	void SetUserData(intptr_t value) noexcept { m_userData = value; }
	intptr_t GetUserData() const noexcept { return m_userData; }
	void SetDlgItemInt(uint32 id, int32 value, bool isSigned = true);
	int32 GetDlgItemInt(uint32 id, bool *isTranslated = nullptr, bool isSigned = true) const;
	void CheckDlgButton(uint32 id, int state);
	int IsDlgButtonChecked(uint32 id) const;
	// Returns the ID of the checked radio button in the range, or 0
	uint32 GetCheckedRadioButton(uint32 firstId, uint32 lastId) const;
	void CheckRadioButton(uint32 firstId, uint32 lastId, uint32 checkedId);
	void EnableDlgItem(uint32 id, bool isEnabled);
	void ShowDlgItem(uint32 id, bool isVisible);

	// Timers and messages
	uintptr_t SetTimer(uintptr_t timerId, uint32 milliseconds);
	void KillTimer(uintptr_t timerId);
	// Delivers synchronously
	LResult SendMessage(uint32 message, WParam wParam = 0, LParam lParam = 0);
	// Delivers later from the GUI event loop; safe to call from any thread
	void SendNotifyMessage(uint32 message, WParam wParam = 0, LParam lParam = 0) { PostMessage(message, wParam, lParam); }
	// Sends the message to this window and all windows below it
	void SendMessageToDescendants(uint32 message, WParam wParam = 0, LParam lParam = 0);
	void PostMessage(uint32 message, WParam wParam = 0, LParam lParam = 0);
	LResult SendNotifyMessageToChildren(uint32 message, WParam wParam = 0, LParam lParam = 0);

	// Command routing
	void SendCommand(uint32 id, uint32 code = 0);
	// Delivers a command later from the GUI event loop
	void PostCommand(uint32 id);
	bool RouteCommand(uint32 id, uint32 code, void *extra, LResult *result = nullptr);
	// Sends a notification of this control: first to the control's own UI_NOTIFY_REFLECT entries, then to the parents
	bool RouteNotification(NotifyHeader &header, LResult *result = nullptr);

	// Lets controls tell their parent about scrolling
	void CallHScroll(uint32 code, uint32 position, Wnd *scrollBar) { OnHScroll(code, position, scrollBar); }
	void CallVScroll(uint32 code, uint32 position, Wnd *scrollBar) { OnVScroll(code, position, scrollBar); }

	// Entry points called by the FLTK adapter
	void DispatchDraw();
	bool DispatchEvent(int event);
	void DispatchResize(int width, int height);

	static const MessageMap messageMap;

	// Overridable event handlers
protected:
	virtual void OnPaint(Painter &painter);
	virtual bool OnEraseBkgnd(Painter &painter);
	virtual void OnSize(uint32 type, int width, int height);
	virtual void OnTimer(uintptr_t timerId);
	virtual void OnDestroy();
	virtual void OnSetFocus(Wnd *oldWindow);
	virtual void OnKillFocus(Wnd *newWindow);
	virtual void OnLButtonDown(uint32 flags, Point point);
	virtual void OnLButtonUp(uint32 flags, Point point);
	virtual void OnLButtonDblClk(uint32 flags, Point point);
	virtual void OnRButtonDown(uint32 flags, Point point);
	virtual void OnRButtonUp(uint32 flags, Point point);
	virtual void OnMButtonDown(uint32 flags, Point point);
	virtual void OnMButtonUp(uint32 flags, Point point);
	// The extra mouse buttons used for navigation; button is XBUTTON1 or XBUTTON2
	virtual void OnXButtonUp(uint32 flags, uint32 button, Point point);
	virtual void OnMouseMove(uint32 flags, Point point);
	// A strip at the top of the window that is not part of the client area. The window paints it and
	// receives its mouse events with coordinates relative to the whole window.
	void SetNonClientTop(int height) noexcept { m_nonClientTop = height; }
	int GetNonClientTop() const noexcept { return m_nonClientTop; }
	virtual void OnNcPaint(Painter &) { }
	virtual void OnNcMouseMove(Point) { }
	virtual void OnNcLButtonDown(uint32, Point) { }
	virtual void OnNcLButtonUp(uint32, Point) { }
	// Tooltip for the part of the window under the mouse; area is in client coordinates
	virtual bool FindToolTip(Point /*point*/, Rect &/*area*/, mpt::ustring &/*text*/) const { return false; }
	virtual bool OnMouseWheel(uint32 flags, int16 delta, Point point);
	virtual void OnMouseLeave();
	virtual bool OnKeyDown(uint32 key, uint32 repeatCount, uint32 flags);
	virtual bool OnKeyUp(uint32 key, uint32 repeatCount, uint32 flags);
	virtual void OnChar(const mpt::ustring &character, uint32 repeatCount, uint32 flags);
	virtual void OnContextMenu(Wnd *window, Point point);
	// Called by sliders and scroll bars that have this window as their parent
	virtual void OnHScroll(uint32 code, uint32 position, Wnd *scrollBar);
	virtual void OnVScroll(uint32 code, uint32 position, Wnd *scrollBar);
	// Whether files dropped at this position would be accepted
	virtual bool CanDropFiles(Point /*point*/) const { return false; }
	virtual void OnDropFiles(const std::vector<mpt::PathString> &files);
	virtual LResult OnUserMessage(uint32 message, WParam wParam, LParam lParam);

	// Paint with OnPaint() before drawing the child widgets
	bool m_isCustomPaint = true;
	// Accepts keyboard focus even without focusable children
	bool m_isFocusable = false;
	intptr_t m_userData = 0;
	int m_nonClientTop = 0;
	mpt::ustring m_accessibleName;
	mpt::ustring m_accessibleSuffix;
	mpt::ustring m_accessibleText;
	Font m_font;
};


// Binds a concrete FLTK widget class to the Wnd interface
template <typename FlBase>
class WndT : public FlBase, public Wnd
{
	// Group-derived widgets like Fl_Table and Fl_Browser draw and lay out their own contents
	static constexpr bool kIsContainer = std::is_same_v<Fl_Group, FlBase> || std::is_base_of_v<Fl_Window, FlBase>;

public:
	WndT(int x = 0, int y = 0, int width = 0, int height = 0, const char *label = nullptr)
	    : FlBase(x, y, width, height, label)
	{
		if constexpr(kIsContainer)
		{
			this->box(FL_FLAT_BOX);
			this->resizable(nullptr);
		}
		if constexpr(std::is_base_of_v<Fl_Group, FlBase>)
			this->end();
	}

	// Child Wnds add their unbound member controls to this group after themselves, so they must be deleted first
	~WndT() override
	{
		if constexpr(kIsContainer)
		{
			while(this->children() > 0)
			{
				Fl_Widget *child = this->child(0);
				this->remove(0);
				delete child;
			}
		}
	}

	Fl_Widget *GetWidget() override { return this; }

	void draw() override
	{
		if constexpr(kIsContainer)
		{
			if(this->damage() & ~FL_DAMAGE_CHILD)
				this->draw_box();
			DispatchDraw();
			this->draw_children();
		} else if constexpr(std::is_same_v<FlBase, Fl_Widget>)
		{
			DispatchDraw();
		} else
		{
			FlBase::draw();
		}
	}

	int handle(int event) override
	{
		if(PreTranslateMessage(event))
			return 1;
		const int result = FlBase::handle(event);
		if(event == FL_FOCUS || event == FL_UNFOCUS)
		{
			const bool isAccepted = (result != 0) || m_isFocusable;
			if(isAccepted)
			{
				if(event == FL_FOCUS)
				{
					// Like WM_SETFOCUS, OnSetFocus runs once the focus has moved
					if(!this->contains(Fl::focus()))
						Fl::focus(this);
					OnSetFocus(nullptr);
				}
				else
					OnKillFocus(nullptr);
			}
			return isAccepted ? 1 : 0;
		}
		if(result)
			return result;
		return DispatchEvent(event) ? 1 : 0;
	}

	void resize(int x, int y, int width, int height) override
	{
		const bool hasSizeChanged = (width != this->w() || height != this->h());
		FlBase::resize(x, y, width, height);
		if(hasSizeChanged)
			DispatchResize(width, height);
	}
};


// A plain container or custom-drawn window
using Panel = WndT<Fl_Group>;


constexpr uint32 XBUTTON1 = 1;
constexpr uint32 XBUTTON2 = 2;
// Width of vertical and height of horizontal scroll bars
constexpr int ScrollBarSize = 16;

constexpr int GWLP_USERDATA = -21;
inline intptr_t GetWindowLongPtr(const Wnd &wnd, int) { return wnd.GetUserData(); }
inline intptr_t GetWindowLongPtr(const Wnd *wnd, int) { return wnd->GetUserData(); }
inline void SetWindowLongPtr(Wnd &wnd, int, intptr_t value) { wnd.SetUserData(value); }
inline void SetWindowLongPtr(Wnd *wnd, int, intptr_t value) { wnd->SetUserData(value); }

// printf-style formatting of UTF-8 text
mpt::ustring Format(const mpt::uchar *format, ...);
std::string FormatA(const char *format, ...);

inline void BeginWaitCursor() { Wnd::BeginWaitCursor(); }
inline void EndWaitCursor() { Wnd::EndWaitCursor(); }
// Position of the mouse pointer on the screen
inline Point GetCursorPosition() { return Point(Fl::event_x_root(), Fl::event_y_root()); }
inline void GetCursorPos(Point *position) { *position = GetCursorPosition(); }
// Size of the screen that holds the mouse pointer
// The innermost window under a screen position, or null if there is none
Wnd *WindowFromPoint(Point screenPosition);

inline Size GetScreenSize() { return Size(Fl::w(), Fl::h()); }
// Handles pending window system events without blocking
inline void PumpMessages() { Fl::check(); }
// Plays the system alert sound
void Beep();


}  // namespace ui


OPENMPT_NAMESPACE_END
