// MFC replacement on FLTK. Base class of all windows, dialogs, views and controls of the GUI. Adds message
// routing, timers and per-window helpers on top of FLTK.

#include "stdafx.h"
#include "Wnd.h"
#include "ResourceTables.h"

#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <cstdarg>
#include <cstdio>
#include <FL/Fl_Tooltip.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>
#include <FL/names.h>

#include <algorithm>
#include <mutex>
#include <set>
#include <string>
#include <vector>


OPENMPT_NAMESPACE_BEGIN

namespace
{
std::vector<mpt::PathString> ParseDroppedFiles(const char *text, int length)
{
	std::vector<mpt::PathString> files;
	if(!text)
		return files;
	const std::string_view data(text, static_cast<size_t>(length));
	size_t position = 0;
	while(position < data.size())
	{
		size_t lineEnd = data.find_first_of("\r\n", position);
		if(lineEnd == std::string_view::npos)
			lineEnd = data.size();
		std::string_view line = data.substr(position, lineEnd - position);
		position = lineEnd + 1;
		if(!line.starts_with("file://"))
			continue;
		line.remove_prefix(7);
		std::string path;
		for(size_t i = 0; i < line.size(); ++i)
		{
			if(line[i] == '%' && i + 2 < line.size() + 0 && std::isxdigit(static_cast<unsigned char>(line[i + 1])) && std::isxdigit(static_cast<unsigned char>(line[i + 2])))
			{
				path.push_back(static_cast<char>(std::stoi(std::string(line.substr(i + 1, 2)), nullptr, 16)));
				i += 2;
			} else
			{
				path.push_back(line[i]);
			}
		}
		if(!path.empty())
			files.push_back(mpt::PathString::FromUTF8(path));
	}
	return files;
}
}




namespace ui
{


namespace
{

std::mutex &GetLiveWindowsMutex()
{
	static std::mutex mutex;
	return mutex;
}

std::set<const Wnd *> &GetLiveWindows()
{
	static std::set<const Wnd *> windows;
	return windows;
}

struct PostedMessage
{
	Wnd *target = nullptr;
	uint32 message = 0;
	WParam wParam = 0;
	LParam lParam = 0;
};

struct TimerData
{
	Wnd *target = nullptr;
	uintptr_t timerId = 0;
	double seconds = 0.0;
};

std::map<std::pair<const Wnd *, uintptr_t>, TimerData *> timers;

// Keys that are currently held, to tell new presses from auto-repeat
std::set<uint32> pressedKeys;

Wnd *capturedWindow = nullptr;

bool IsAlive(const Wnd *wnd)
{
	std::lock_guard<std::mutex> lock(GetLiveWindowsMutex());
	return GetLiveWindows().count(wnd) != 0;
}

std::function<bool(int)> idleHandler;
int idleCount = 0;
bool isIdleScheduled = false;

void IdleCallback(void *)
{
	if(idleHandler && idleHandler(idleCount++))
		return;
	Fl::remove_idle(IdleCallback);
	isIdleScheduled = false;
}

int OnSystemEvent(void *, void *)
{
	RequestIdleProcessing();
	return 0;
}

void DeliverPostedMessage(void *data)
{
	RequestIdleProcessing();
	std::unique_ptr<PostedMessage> msg(static_cast<PostedMessage *>(data));
	if(IsAlive(msg->target))
		msg->target->SendMessage(msg->message, msg->wParam, msg->lParam);
}

void TimerCallback(void *data)
{
	TimerData *timer = static_cast<TimerData *>(data);
	if(!IsAlive(timer->target))
		return;
	Wnd *target = timer->target;
	const uintptr_t timerId = timer->timerId;
	const double seconds = timer->seconds;
	Fl::repeat_timeout(seconds, TimerCallback, data);
	RequestIdleProcessing();
	target->SendMessage(MsgUser + 0x7F00, timerId, 0);
}

uint32 MouseFlagsFromEvent()
{
	uint32 flags = 0;
	const int state = Fl::event_state();
	if(state & FL_BUTTON1)
		flags |= MouseLeft;
	if(state & FL_BUTTON2)
		flags |= MouseMiddle;
	if(state & FL_BUTTON3)
		flags |= MouseRight;
	if(state & FL_SHIFT)
		flags |= MouseShift;
	if(state & FL_CTRL)
		flags |= MouseControl;
	if(state & FL_ALT)
		flags |= MouseAlt;
	return flags;
}

uint32 ModifierFlagsFromEvent()
{
	uint32 flags = 0;
	const int state = Fl::event_state();
	if(state & FL_SHIFT)
		flags |= KeyModShift;
	if(state & FL_CTRL)
		flags |= KeyModControl;
	if(state & FL_ALT)
		flags |= KeyModAlt;
	if(state & FL_META)
		flags |= KeyModExtended;
	return flags;
}

uint32 VirtualKeyFromEvent()
{
	const int key = Fl::event_key();
	if(key >= 'a' && key <= 'z')
		return static_cast<uint32>(key - 'a' + 'A');
	if(key >= '0' && key <= '9')
		return static_cast<uint32>(key);
	if(key >= FL_F + 1 && key <= FL_F + 24)
		return static_cast<uint32>(Key_F1 + (key - (FL_F + 1)));
	if(key >= FL_KP + '0' && key <= FL_KP + '9')
		return static_cast<uint32>(Key_NUMPAD0 + (key - (FL_KP + '0')));
	switch(key)
	{
	case FL_BackSpace: return Key_BACK;
	case FL_Tab: return Key_TAB;
	case FL_Enter: return Key_RETURN;
	case FL_KP_Enter: return Key_RETURN;
	case FL_Escape: return Key_ESCAPE;
	case FL_Pause: return Key_PAUSE;
	case FL_Scroll_Lock: return Key_SCROLL;
	case FL_Num_Lock: return Key_NUMLOCK;
	case FL_Caps_Lock: return Key_CAPITAL;
	case FL_Home: return Key_HOME;
	case FL_End: return Key_END;
	case FL_Left: return Key_LEFT;
	case FL_Up: return Key_UP;
	case FL_Right: return Key_RIGHT;
	case FL_Down: return Key_DOWN;
	case FL_Page_Up: return Key_PRIOR;
	case FL_Page_Down: return Key_NEXT;
	case FL_Insert: return Key_INSERT;
	case FL_Delete: return Key_DELETE;
	case FL_Print: return Key_SNAPSHOT;
	case FL_Menu: return Key_APPS;
	case FL_Shift_L: return Key_LSHIFT;
	case FL_Shift_R: return Key_RSHIFT;
	case FL_Control_L: return Key_LCONTROL;
	case FL_Control_R: return Key_RCONTROL;
	case FL_Alt_L: return Key_LMENU;
	case FL_Alt_R: return Key_RMENU;
	case FL_Meta_L: return Key_LWIN;
	case FL_Meta_R: return Key_RWIN;
	case FL_KP + '*': return Key_MULTIPLY;
	case FL_KP + '+': return Key_ADD;
	case FL_KP + '-': return Key_SUBTRACT;
	case FL_KP + '.': return Key_DECIMAL;
	case FL_KP + '/': return Key_DIVIDE;
	case ' ': return Key_SPACE;
	case ';': return Key_OEM_1;
	case '=': return Key_OEM_PLUS;
	case ',': return Key_OEM_COMMA;
	case '-': return Key_OEM_MINUS;
	case '.': return Key_OEM_PERIOD;
	case '/': return Key_OEM_2;
	case '`': return Key_OEM_3;
	case '[': return Key_OEM_4;
	case '\\': return Key_OEM_5;
	case ']': return Key_OEM_6;
	case '\'': return Key_OEM_7;
	default: break;
	}
	return KeyNone;
}

constexpr uint32 MsgTimer = MsgUser + 0x7F00;
constexpr uint32 MsgPostedCommand = MsgUser + 0x7F01;
constexpr uint32 MsgUpdateUiCode = MsgUser + 0x7F10;
constexpr uint32 MsgMessageCode = MsgUser + 0x7F11;

}  // namespace


const MessageMap CommandTarget::messageMap{nullptr};
const MessageMap Wnd::messageMap{&CommandTarget::messageMap};


bool CommandTarget::OnCmdMsg(uint32 id, uint32 code, void *extra, LResult *result)
{
	for(const MessageMap *map = GetMessageMap(); map != nullptr; map = map->GetBase())
	{
		for(const MapEntry &entry : map->GetEntries())
		{
			bool isMatch = false;
			switch(entry.kind)
			{
			case MapKind::Command:
				isMatch = (code == 0 && id == entry.id);
				break;
			case MapKind::CommandRange:
				isMatch = (code == 0 && id >= entry.id && id <= entry.idLast);
				break;
			case MapKind::Notify:
				isMatch = (code == entry.code && id == entry.id);
				break;
			case MapKind::NotifyRange:
				isMatch = (code == entry.code && id >= entry.id && id <= entry.idLast);
				break;
			case MapKind::UpdateUi:
				isMatch = (code == MsgUpdateUiCode && id == entry.id);
				break;
			case MapKind::Message:
				isMatch = (code == MsgMessageCode && id == entry.id);
				break;
			}
			if(isMatch && entry.call(this, id, extra, result))
				return true;
		}
	}
	return false;
}


Wnd::Wnd()
{
	std::lock_guard<std::mutex> lock(GetLiveWindowsMutex());
	GetLiveWindows().insert(this);
}


Wnd::~Wnd()
{
	for(auto it = timers.begin(); it != timers.end();)
	{
		if(it->first.first == this)
		{
			Fl::remove_timeout(TimerCallback, it->second);
			delete it->second;
			it = timers.erase(it);
		} else
		{
			++it;
		}
	}
	if(capturedWindow == this)
		capturedWindow = nullptr;
	std::lock_guard<std::mutex> lock(GetLiveWindowsMutex());
	GetLiveWindows().erase(this);
}


Rect Wnd::GetClientRect() const
{
	const Fl_Widget *widget = GetWidget();
	return Rect(0, 0, widget->w(), std::max(widget->h() - m_nonClientTop, 0));
}


Rect Wnd::GetWindowRect() const
{
	const Fl_Widget *widget = GetWidget();
	int originX = 0;
	int originY = 0;
	if(const Fl_Window *self = widget->as_window())
	{
		originX = self->x_root();
		originY = self->y_root();
	} else if(const Fl_Window *enclosing = widget->window())
	{
		originX = enclosing->x_root() + widget->x();
		originY = enclosing->y_root() + widget->y();
	}
	return Rect(originX, originY, originX + widget->w(), originY + widget->h());
}


Rect Wnd::GetRectInParent() const
{
	const Fl_Widget *widget = GetWidget();
	return Rect(widget->x(), widget->y(), widget->x() + widget->w(), widget->y() + widget->h());
}


void Wnd::ScreenToClient(Point &pt) const
{
	const Rect rect = GetWindowRect();
	pt.x -= rect.left;
	pt.y -= rect.top;
}


void Wnd::ScreenToClient(Rect &rect) const
{
	const Rect origin = GetWindowRect();
	rect.OffsetRect(-origin.left, -origin.top);
}


void Wnd::ClientToScreen(Point &pt) const
{
	const Rect rect = GetWindowRect();
	pt.x += rect.left;
	pt.y += rect.top;
}


void Wnd::ClientToScreen(Rect &rect) const
{
	const Rect origin = GetWindowRect();
	rect.OffsetRect(origin.left, origin.top);
}


void Wnd::MoveWindow(int x, int y, int width, int height, bool shouldRedraw)
{
	GetWidget()->resize(x, y, width, height);
	if(shouldRedraw)
		GetWidget()->redraw();
}


void Wnd::MoveChild(Wnd &child, const Rect &rect)
{
	const Fl_Widget *widget = GetWidget();
	child.MoveWindow(widget->x() + rect.left, widget->y() + rect.top, rect.Width(), rect.Height());
}


void Wnd::SetWindowPos(int x, int y, int width, int height, bool shouldMove, bool shouldSize)
{
	Fl_Widget *widget = GetWidget();
	widget->resize(shouldMove ? x : widget->x(), shouldMove ? y : widget->y(), shouldSize ? width : widget->w(), shouldSize ? height : widget->h());
}


void Wnd::SetWindowPos(const Wnd *, int x, int y, int width, int height, uint32 flags)
{
	SetWindowPos(x, y, width, height, (flags & PosNoMove) == 0, (flags & PosNoSize) == 0);
	if(flags & PosShowWindow)
		ShowWindow(true);
	if(flags & PosHideWindow)
		ShowWindow(false);
}


void Wnd::ShowWindow(bool isVisible)
{
	if(isVisible)
		GetWidget()->show();
	else
		GetWidget()->hide();
}


void Wnd::EnableWindow(bool isEnabled)
{
	if(isEnabled)
		GetWidget()->activate();
	else
		GetWidget()->deactivate();
}


void Wnd::SetFocus()
{
	if(Fl::focus() == GetWidget())
		return;
	GetWidget()->take_focus();
}


bool Wnd::HasFocus() const
{
	return Fl::focus() == GetWidget();
}


Wnd *Wnd::GetFocus()
{
	Fl_Widget *widget = Fl::focus();
	while(widget != nullptr)
	{
		if(Wnd *wnd = dynamic_cast<Wnd *>(widget))
			return wnd;
		widget = widget->parent();
	}
	return nullptr;
}


void Wnd::Invalidate(bool)
{
	GetWidget()->redraw();
}


void Wnd::InvalidateRect(const Rect *rect, bool)
{
	Fl_Widget *widget = GetWidget();
	if(rect == nullptr)
		widget->redraw();
	else
		widget->damage(FL_DAMAGE_ALL, widget->x() + rect->left, widget->y() + rect->top, rect->Width(), rect->Height());
}


void Wnd::UpdateWindow()
{
	if(GetWidget()->damage())
		Fl::flush();
}


void Wnd::SetWindowText(const mpt::ustring &text)
{
	Fl_Widget *widget = GetWidget();
	widget->copy_label(ToFl(text));
	if(Fl_Window *win = widget->as_window())
		win->copy_label(ToFl(text));
	widget->redraw_label();
}


int Wnd::GetWindowText(mpt::uchar *buffer, int maximumCount) const
{
	if(maximumCount <= 0)
		return 0;
	const mpt::ustring text = GetWindowText();
	const int count = std::min(static_cast<int>(text.length()), maximumCount - 1);
	std::copy_n(text.begin(), count, buffer);
	buffer[count] = 0;
	return count;
}


mpt::ustring Wnd::GetWindowText() const
{
	const char *text = GetWidget()->label();
	return text ? mpt::transcode<mpt::ustring>(mpt::common_encoding::utf8, std::string(text)) : mpt::ustring();
}


void Wnd::SetFont(const Font &font)
{
	m_font = font;
	Fl_Widget *widget = GetWidget();
	widget->labelfont(font.face);
	widget->labelsize(font.size);
	if(Fl_Input_ *input = dynamic_cast<Fl_Input_ *>(widget))
	{
		input->textfont(font.face);
		input->textsize(font.size);
	}
}


void Wnd::SetCapture()
{
	capturedWindow = this;
	Fl::grab(GetWidget()->window());
}


void Wnd::ReleaseCapture()
{
	if(capturedWindow == this)
		capturedWindow = nullptr;
	Fl::grab(nullptr);
}


Wnd *Wnd::GetCapture()
{
	return capturedWindow;
}


void Wnd::SetCursorShape(Fl_Cursor cursor)
{
	if(Fl_Window *win = GetWidget()->window())
		win->cursor(cursor);
}


namespace
{
int waitCursorDepth = 0;
}


void CommandTarget::BeginWaitCursor()
{
	if(waitCursorDepth++ == 0)
	{
		for(Fl_Window *window = Fl::first_window(); window != nullptr; window = Fl::next_window(window))
			window->cursor(FL_CURSOR_WAIT);
		Fl::flush();
	}
}


void CommandTarget::EndWaitCursor()
{
	if(waitCursorDepth > 0 && --waitCursorDepth == 0)
	{
		for(Fl_Window *window = Fl::first_window(); window != nullptr; window = Fl::next_window(window))
			window->cursor(FL_CURSOR_DEFAULT);
	}
}


void Wnd::SetForegroundWindow()
{
	if(Fl_Window *window = GetWidget()->window())
		window->show();
}


bool Wnd::GetWindowPlacement(WindowPlacement *placement) const
{
	const Fl_Widget *widget = GetWidget();
	const Fl_Window *window = widget->as_window();
	if(window != nullptr && window->parent() == nullptr)
	{
		placement->rcNormalPosition = Rect(window->x(), window->y(), window->x() + window->w(), window->y() + window->h());
	} else
	{
		const Wnd *parent = GetParent();
		const int offsetX = parent ? parent->GetWidget()->x() : 0;
		const int offsetY = parent ? parent->GetWidget()->y() : 0;
		placement->rcNormalPosition = Rect(widget->x() - offsetX, widget->y() - offsetY, widget->x() - offsetX + widget->w(), widget->y() - offsetY + widget->h());
	}
	placement->showCmd = widget->visible() ? SW_SHOWNORMAL : SW_HIDE;
	return true;
}


bool Wnd::SetWindowPlacement(const WindowPlacement *placement)
{
	Fl_Widget *widget = GetWidget();
	const Rect &rect = placement->rcNormalPosition;
	if(Fl_Window *window = widget->as_window(); window != nullptr && window->parent() == nullptr)
		window->resize(rect.left, rect.top, rect.Width(), rect.Height());
	else
		MoveWindow(rect, true);
	return true;
}


void Wnd::SetToolTipText(const mpt::ustring &text)
{
	GetWidget()->copy_tooltip(mpt::transcode<std::string>(mpt::common_encoding::utf8, text).c_str());
}


void Wnd::ShowToolTip(const Rect &area, const mpt::ustring &text)
{
	Fl_Widget *widget = GetWidget();
	// FLTK keeps the pointer while the tooltip is shown and ignores a repeated widget/pointer pair,
	// so a changed text needs a different buffer
	static std::string tips[2];
	static int tip_index = 0;
	std::string newTip = mpt::transcode<std::string>(mpt::common_encoding::utf8, text);
	if(tips[tip_index] != newTip)
	{
		tip_index ^= 1;
		tips[tip_index] = std::move(newTip);
	}
	Fl_Tooltip::enter_area(widget, area.left, area.top, area.Width(), area.Height(), tips[tip_index].c_str());
}


bool Wnd::IsWindow() const
{
	const Fl_Widget *widget = GetWidget();
	if(widget->parent() != nullptr)
		return true;
	Fl_Window *window = const_cast<Fl_Widget *>(widget)->as_window();
	return window != nullptr && window->shown();
}


bool Wnd::IsChild(const Wnd *wnd) const
{
	if(wnd == nullptr)
		return false;
	const Fl_Widget *self = GetWidget();
	for(const Fl_Widget *widget = wnd->GetWidget()->parent(); widget != nullptr; widget = widget->parent())
	{
		if(widget == self)
			return true;
	}
	return false;
}


void Wnd::ScrollChildren(int dx, int dy)
{
	Fl_Group *group = GetWidget()->as_group();
	if(group == nullptr)
		return;
	group->clip_children(1);
	for(int i = 0; i < group->children(); ++i)
	{
		Fl_Widget *child = group->child(i);
		child->position(child->x() + dx, child->y() + dy);
	}
	group->redraw();
}


void Wnd::SendMessageToDescendants(uint32 message, WParam wParam, LParam lParam)
{
	Fl_Group *group = GetWidget()->as_group();
	if(group == nullptr)
		return;
	for(int i = 0; i < group->children(); ++i)
	{
		if(Wnd *child = dynamic_cast<Wnd *>(group->child(i)))
		{
			child->SendMessage(message, wParam, lParam);
			child->SendMessageToDescendants(message, wParam, lParam);
		}
	}
}


Wnd *Wnd::GetParent() const
{
	return dynamic_cast<Wnd *>(GetWidget()->parent());
}


Wnd *Wnd::GetTopLevelParent()
{
	Wnd *top = this;
	while(Wnd *next = top->GetParent())
		top = next;
	return top;
}


Wnd *Wnd::GetDlgItem(uint32 id) const
{
	const Fl_Group *group = GetWidget()->as_group();
	if(group == nullptr)
		return nullptr;
	for(int i = 0; i < group->children(); ++i)
	{
		Fl_Widget *child = group->child(i);
		if(static_cast<uint32>(child->argument()) == id)
		{
			if(Wnd *wnd = dynamic_cast<Wnd *>(child))
				return wnd;
		}
		if(Wnd *wnd = dynamic_cast<Wnd *>(child))
		{
			if(Wnd *found = wnd->GetDlgItem(id))
				return found;
		}
	}
	return nullptr;
}


void Wnd::DestroyWindow()
{
	OnDestroy();
	{
		// Like a destroyed HWND, drop messages still queued for this window
		std::lock_guard<std::mutex> lock(GetLiveWindowsMutex());
		GetLiveWindows().erase(this);
	}
	Fl_Widget *widget = GetWidget();
	widget->hide();
	if(widget->parent() != nullptr)
		widget->parent()->remove(widget);
}


void Wnd::SetDlgItemText(uint32 id, const mpt::ustring &text)
{
	if(Wnd *item = GetDlgItem(id))
		item->SetWindowText(text);
}


mpt::ustring Wnd::GetDlgItemText(uint32 id) const
{
	if(Wnd *item = GetDlgItem(id))
		return item->GetWindowText();
	return {};
}


void Wnd::SetDlgItemInt(uint32 id, int32 value, bool isSigned)
{
	const std::string text = isSigned ? std::to_string(value) : std::to_string(static_cast<uint32>(value));
	SetDlgItemText(id, mpt::transcode<mpt::ustring>(mpt::common_encoding::utf8, text));
}


int32 Wnd::GetDlgItemInt(uint32 id, bool *isTranslated, bool isSigned) const
{
	const std::string text = mpt::transcode<std::string>(mpt::common_encoding::utf8, GetDlgItemText(id));
	char *end = nullptr;
	const long long value = std::strtoll(text.c_str(), &end, 10);
	const bool isValid = !text.empty() && end != nullptr && *end == '\0' && (isSigned || value >= 0);
	if(isTranslated)
		*isTranslated = isValid;
	return isValid ? static_cast<int32>(value) : 0;
}


void Wnd::CheckDlgButton(uint32 id, int state)
{
	if(Wnd *item = GetDlgItem(id))
	{
		if(Fl_Button *button = dynamic_cast<Fl_Button *>(item->GetWidget()))
			button->value(state != 0 ? 1 : 0);
	}
}


int Wnd::IsDlgButtonChecked(uint32 id) const
{
	if(Wnd *item = GetDlgItem(id))
	{
		if(Fl_Button *button = dynamic_cast<Fl_Button *>(item->GetWidget()))
			return button->value() ? 1 : 0;
	}
	return 0;
}


void Wnd::CheckRadioButton(uint32 firstId, uint32 lastId, uint32 checkedId)
{
	for(uint32 id = firstId; id <= lastId; ++id)
		CheckDlgButton(id, id == checkedId ? 1 : 0);
}


void Wnd::EnableDlgItem(uint32 id, bool isEnabled)
{
	if(Wnd *item = GetDlgItem(id))
		item->EnableWindow(isEnabled);
}


void Wnd::ShowDlgItem(uint32 id, bool isVisible)
{
	if(Wnd *item = GetDlgItem(id))
		item->ShowWindow(isVisible);
}


uintptr_t Wnd::SetTimer(uintptr_t timerId, uint32 milliseconds)
{
	KillTimer(timerId);
	TimerData *timer = new TimerData{this, timerId, milliseconds / 1000.0};
	timers[{this, timerId}] = timer;
	Fl::add_timeout(timer->seconds, TimerCallback, timer);
	return timerId;
}


void Wnd::KillTimer(uintptr_t timerId)
{
	const auto it = timers.find({this, timerId});
	if(it == timers.end())
		return;
	Fl::remove_timeout(TimerCallback, it->second);
	delete it->second;
	timers.erase(it);
}


LResult Wnd::SendMessage(uint32 message, WParam wParam, LParam lParam)
{
	if(message == MsgTimer)
	{
		OnTimer(wParam);
		return 0;
	}
	if(message == MsgPostedCommand)
	{
		RouteCommand(static_cast<uint32>(wParam), 0, nullptr);
		return 0;
	}
	MessageArgs args{wParam, lParam};
	LResult result = 0;
	if(OnCmdMsg(message, MsgMessageCode, &args, &result))
		return result;
	return OnUserMessage(message, wParam, lParam);
}


void SetIdleHandler(std::function<bool(int)> handler)
{
	if(!idleHandler)
		Fl::add_system_handler(OnSystemEvent, nullptr);
	idleHandler = std::move(handler);
	RequestIdleProcessing();
}


void RequestIdleProcessing()
{
	idleCount = 0;
	if(isIdleScheduled)
		return;
	isIdleScheduled = true;
	Fl::add_idle(IdleCallback);
}


void Wnd::PostMessage(uint32 message, WParam wParam, LParam lParam)
{
	Fl::awake(DeliverPostedMessage, new PostedMessage{this, message, wParam, lParam});
}


LResult Wnd::SendNotifyMessageToChildren(uint32 message, WParam wParam, LParam lParam)
{
	LResult result = 0;
	if(Fl_Group *group = GetWidget()->as_group())
	{
		for(int i = 0; i < group->children(); ++i)
		{
			if(Wnd *wnd = dynamic_cast<Wnd *>(group->child(i)))
				result = wnd->SendMessage(message, wParam, lParam);
		}
	}
	return result;
}


void Wnd::PostCommand(uint32 id)
{
	PostMessage(MsgPostedCommand, id, 0);
}


void Wnd::SendCommand(uint32 id, uint32 code)
{
	RouteCommand(id, code, nullptr);
}


bool Wnd::RouteCommand(uint32 id, uint32 code, void *extra, LResult *result)
{
	for(Wnd *wnd = this; wnd != nullptr; wnd = wnd->GetParent())
	{
		if(wnd->OnCmdMsg(id, code, extra, result))
			return true;
	}
	return false;
}


bool Wnd::RouteNotification(NotifyHeader &header, LResult *result)
{
	OnCmdMsg(NotifyReflectId, header.code, &header, result);
	if(Wnd *parent = GetParent())
		return parent->RouteCommand(header.id, header.code, &header, result);
	return false;
}


Wnd *WindowFromPoint(Point screenPosition)
{
	for(Fl_Window *window = Fl::first_window(); window != nullptr; window = Fl::next_window(window))
	{
		if(!window->visible() || screenPosition.x < window->x_root() || screenPosition.y < window->y_root()
		   || screenPosition.x >= window->x_root() + window->w() || screenPosition.y >= window->y_root() + window->h())
			continue;
		Fl_Widget *widget = window;
		while(Fl_Group *group = widget->as_group())
		{
			Fl_Widget *inner = nullptr;
			for(int i = group->children() - 1; i >= 0; --i)
			{
				Fl_Widget *child = group->child(i);
				const int childX = child->as_window() ? child->as_window()->x_root() : child->x() + window->x_root();
				const int childY = child->as_window() ? child->as_window()->y_root() : child->y() + window->y_root();
				if(child->visible() && screenPosition.x >= childX && screenPosition.y >= childY
				   && screenPosition.x < childX + child->w() && screenPosition.y < childY + child->h())
				{
					inner = child;
					break;
				}
			}
			if(inner == nullptr)
				break;
			widget = inner;
		}
		for(; widget != nullptr; widget = widget->parent())
		{
			if(Wnd *wnd = dynamic_cast<Wnd *>(widget))
				return wnd;
		}
	}
	return nullptr;
}


void Wnd::DispatchDraw()
{
	if(!m_isCustomPaint)
		return;
	Fl_Widget *widget = GetWidget();
	fl_push_clip(widget->x(), widget->y(), widget->w(), widget->h());
	if(m_nonClientTop > 0)
	{
		Painter nonClientPainter(Point(widget->x(), widget->y()));
		OnNcPaint(nonClientPainter);
		fl_push_clip(widget->x(), widget->y() + m_nonClientTop, widget->w(), std::max(widget->h() - m_nonClientTop, 0));
	}
	Painter painter(Point(widget->x(), widget->y() + m_nonClientTop));
	OnEraseBkgnd(painter);
	OnPaint(painter);
	if(m_nonClientTop > 0)
		fl_pop_clip();
	fl_pop_clip();
}


void Wnd::DispatchResize(int width, int height)
{
	OnSize(0, width, height);
}


bool Wnd::DispatchEvent(int event)
{
	const Fl_Widget *widget = GetWidget();
	const Point windowPoint(Fl::event_x() - widget->x(), Fl::event_y() - widget->y());
	if(m_nonClientTop > 0 && windowPoint.y < m_nonClientTop)
	{
		switch(event)
		{
		case FL_PUSH:
			if(Fl::event_button() == FL_LEFT_MOUSE)
			{
				OnNcLButtonDown(MouseFlagsFromEvent(), windowPoint);
				return true;
			}
			break;
		case FL_RELEASE:
			if(Fl::event_button() == FL_LEFT_MOUSE)
			{
				OnNcLButtonUp(MouseFlagsFromEvent(), windowPoint);
				return true;
			}
			break;
		case FL_MOVE:
		case FL_DRAG:
			OnNcMouseMove(windowPoint);
			return true;
		default:
			break;
		}
	} else if(event == FL_LEAVE && m_nonClientTop > 0)
	{
		OnNcMouseMove(Point(-1, -1));
	}
	const Point point(windowPoint.x, windowPoint.y - m_nonClientTop);
	switch(event)
	{
	case FL_PUSH:
		switch(Fl::event_button())
		{
		case FL_LEFT_MOUSE:
			if(Fl::event_clicks() > 0)
				OnLButtonDblClk(MouseFlagsFromEvent(), point);
			else
				OnLButtonDown(MouseFlagsFromEvent(), point);
			return true;
		case FL_MIDDLE_MOUSE:
			OnMButtonDown(MouseFlagsFromEvent(), point);
			return true;
		case FL_RIGHT_MOUSE:
			OnRButtonDown(MouseFlagsFromEvent(), point);
			return true;
		}
		return false;
	case FL_RELEASE:
		switch(Fl::event_button())
		{
		case FL_LEFT_MOUSE:
			OnLButtonUp(MouseFlagsFromEvent(), point);
			return true;
		case FL_MIDDLE_MOUSE:
			OnMButtonUp(MouseFlagsFromEvent(), point);
			return true;
		case FL_RIGHT_MOUSE:
			OnRButtonUp(MouseFlagsFromEvent(), point);
			return true;
		case 4:
			OnXButtonUp(MouseFlagsFromEvent(), XBUTTON1, point);
			return true;
		case 5:
			OnXButtonUp(MouseFlagsFromEvent(), XBUTTON2, point);
			return true;
		}
		return false;
	case FL_DRAG:
	case FL_MOVE:
		OnMouseMove(MouseFlagsFromEvent(), point);
		{
			Rect toolArea;
			mpt::ustring toolText;
			if(FindToolTip(point, toolArea, toolText))
				ShowToolTip(toolArea, toolText);
		}
		return true;
	case FL_LEAVE:
		OnMouseLeave();
		return false;
	case FL_ENTER:
		return true;
	case FL_MOUSEWHEEL:
		return OnMouseWheel(MouseFlagsFromEvent(), static_cast<int16>(-Fl::event_dy() * 120), point);
	case FL_KEYBOARD:
	{
		const uint32 key = VirtualKeyFromEvent();
		uint32 flags = 0;
		if(!pressedKeys.insert(key).second)
			flags |= KeyFlagRepeat;
		return OnKeyDown(key, 1, flags);
	}
	case FL_KEYUP:
	{
		const uint32 key = VirtualKeyFromEvent();
		pressedKeys.erase(key);
		return OnKeyUp(key, 1, KeyFlagRelease);
	}
	case FL_DND_ENTER:
	case FL_DND_DRAG:
		return CanDropFiles(point);
	case FL_DND_LEAVE:
		return true;
	case FL_DND_RELEASE:
		return CanDropFiles(point);
	case FL_PASTE:
	{
		const std::vector<mpt::PathString> files = ParseDroppedFiles(Fl::event_text(), Fl::event_length());
		if(files.empty() || !CanDropFiles(point))
			return false;
		OnDropFiles(files);
		return true;
	}
	default:
		break;
	}
	return false;
}


void Wnd::ConfigureFromTemplate(const DialogControl &)
{
}

void Wnd::OnPaint(Painter &)
{
}

bool Wnd::OnEraseBkgnd(Painter &)
{
	return false;
}

void Wnd::OnSize(uint32, int, int)
{
}

void Wnd::OnTimer(uintptr_t)
{
}

void Wnd::OnDestroy()
{
}

void Wnd::OnSetFocus(Wnd *)
{
}

void Wnd::OnKillFocus(Wnd *)
{
}

void Wnd::OnLButtonDown(uint32, Point)
{
}

void Wnd::OnLButtonUp(uint32, Point)
{
}

void Wnd::OnLButtonDblClk(uint32 flags, Point point)
{
	OnLButtonDown(flags, point);
}

void Wnd::OnRButtonDown(uint32, Point)
{
}

void Wnd::OnRButtonUp(uint32 flags, Point point)
{
	OnContextMenu(this, point);
	MPT_UNUSED(flags);
}

void Wnd::OnMButtonDown(uint32, Point)
{
}

void Wnd::OnXButtonUp(uint32, uint32, Point)
{
}


void Wnd::OnMButtonUp(uint32, Point)
{
}

void Wnd::OnMouseMove(uint32, Point)
{
}

bool Wnd::OnMouseWheel(uint32, int16, Point)
{
	return false;
}

void Wnd::OnMouseLeave()
{
}

bool Wnd::OnKeyDown(uint32, uint32, uint32)
{
	return false;
}

bool Wnd::OnKeyUp(uint32, uint32, uint32)
{
	return false;
}

void Wnd::OnChar(const mpt::ustring &, uint32, uint32)
{
}

void Wnd::OnContextMenu(Wnd *, Point)
{
}

void Wnd::OnHScroll(uint32, uint32, Wnd *)
{
}

void Wnd::OnVScroll(uint32, uint32, Wnd *)
{
}

void Wnd::OnDropFiles(const std::vector<mpt::PathString> &)
{
}

LResult Wnd::OnUserMessage(uint32, WParam, LParam)
{
	return 0;
}

bool Wnd::PreTranslateMessage(int)
{
	return false;
}


uint32 KeyFromEvent()
{
	return VirtualKeyFromEvent();
}


namespace
{
struct ScanCodeEntry
{
	uint32 scanCode;
	uint32 virtualKey;
};

constexpr ScanCodeEntry scanCodeTable[] =
{
	{0x02, '1'}, {0x03, '2'}, {0x04, '3'}, {0x05, '4'}, {0x06, '5'}, {0x07, '6'}, {0x08, '7'}, {0x09, '8'}, {0x0A, '9'}, {0x0B, '0'},
	{0x0C, Key_OEM_MINUS}, {0x0D, Key_OEM_PLUS},
	{0x10, 'Q'}, {0x11, 'W'}, {0x12, 'E'}, {0x13, 'R'}, {0x14, 'T'}, {0x15, 'Y'}, {0x16, 'U'}, {0x17, 'I'}, {0x18, 'O'}, {0x19, 'P'},
	{0x1A, Key_OEM_4}, {0x1B, Key_OEM_6},
	{0x1E, 'A'}, {0x1F, 'S'}, {0x20, 'D'}, {0x21, 'F'}, {0x22, 'G'}, {0x23, 'H'}, {0x24, 'J'}, {0x25, 'K'}, {0x26, 'L'},
	{0x27, Key_OEM_1}, {0x28, Key_OEM_7}, {0x29, Key_OEM_3}, {0x2B, Key_OEM_5},
	{0x2C, 'Z'}, {0x2D, 'X'}, {0x2E, 'C'}, {0x2F, 'V'}, {0x30, 'B'}, {0x31, 'N'}, {0x32, 'M'},
	{0x33, Key_OEM_COMMA}, {0x34, Key_OEM_PERIOD}, {0x35, Key_OEM_2},
	{0x39, Key_SPACE}, {0x56, Key_OEM_102},
};
}


uint32 MapScanCodeToVirtualKey(uint32 scanCode)
{
	for(const auto &entry : scanCodeTable)
	{
		if(entry.scanCode == scanCode)
			return entry.virtualKey;
	}
	return 0;
}


uint32 MapVirtualKeyToScanCode(uint32 virtualKey)
{
	for(const auto &entry : scanCodeTable)
	{
		if(entry.virtualKey == virtualKey)
			return entry.scanCode;
	}
	return 0;
}


mpt::ustring GetKeyName(uint32 key, bool)
{
	if((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9'))
		return mpt::ustring(1, static_cast<mpt::uchar>(key));
	if(key >= Key_F1 && key <= Key_F1 + 23)
		return UL_("F") + mpt::ufmt::dec(key - Key_F1 + 1);
	if(key >= Key_NUMPAD0 && key <= Key_NUMPAD0 + 9)
		return UL_("Num ") + mpt::ufmt::dec(key - Key_NUMPAD0);
	switch(key)
	{
	case Key_BACK: return UL_("Backspace");
	case Key_TAB: return UL_("Tab");
	case Key_RETURN: return UL_("Enter");
	case Key_SHIFT: return UL_("SHIFT");
	case Key_CONTROL: return UL_("CTRL");
	case Key_MENU: return UL_("ALT");
	case Key_LSHIFT: return UL_("Shift");
	case Key_RSHIFT: return UL_("RShift");
	case Key_LCONTROL: return UL_("Ctrl");
	case Key_RCONTROL: return UL_("RCtrl");
	case Key_LMENU: return UL_("Alt");
	case Key_RMENU: return UL_("RAlt");
	case Key_PAUSE: return UL_("Pause");
	case Key_CAPITAL: return UL_("Caps Lock");
	case Key_ESCAPE: return UL_("Esc");
	case Key_SPACE: return UL_("Space");
	case Key_PRIOR: return UL_("Page Up");
	case Key_NEXT: return UL_("Page Down");
	case Key_END: return UL_("End");
	case Key_HOME: return UL_("Home");
	case Key_LEFT: return UL_("Left");
	case Key_UP: return UL_("Up");
	case Key_RIGHT: return UL_("Right");
	case Key_DOWN: return UL_("Down");
	case Key_SNAPSHOT: return UL_("Print Screen");
	case Key_INSERT: return UL_("Insert");
	case Key_DELETE: return UL_("Delete");
	case Key_LWIN: return UL_("Left Windows");
	case Key_RWIN: return UL_("Right Windows");
	case Key_APPS: return UL_("Application");
	case Key_MULTIPLY: return UL_("Num *");
	case Key_ADD: return UL_("Num +");
	case Key_SUBTRACT: return UL_("Num -");
	case Key_DECIMAL: return UL_("Num .");
	case Key_DIVIDE: return UL_("Num /");
	case Key_NUMLOCK: return UL_("Num Lock");
	case Key_SCROLL: return UL_("Scroll Lock");
	case Key_OEM_1: return UL_(";");
	case Key_OEM_PLUS: return UL_("=");
	case Key_OEM_COMMA: return UL_(",");
	case Key_OEM_MINUS: return UL_("-");
	case Key_OEM_PERIOD: return UL_(".");
	case Key_OEM_2: return UL_("/");
	case Key_OEM_3: return UL_("`");
	case Key_OEM_4: return UL_("[");
	case Key_OEM_5: return UL_("\\");
	case Key_OEM_6: return UL_("]");
	case Key_OEM_7: return UL_("'");
	default: break;
	}
	return UL_("Key ") + mpt::ufmt::dec(key);
}


uint32 FindVirtualKeyForCharacter(char character)
{
	if(character >= 'a' && character <= 'z')
		return static_cast<uint32>(character - 'a' + 'A');
	if((character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9'))
		return static_cast<uint32>(character);
	return 0;
}


bool IsKeyRepeat()
{
	return pressedKeys.count(VirtualKeyFromEvent()) != 0;
}


uint32 CharacterFromEvent()
{
	const char *text = Fl::event_text();
	if(Fl::event_length() <= 0 || text == nullptr)
		return 0;
	const unsigned char first = static_cast<unsigned char>(text[0]);
	return first;
}


bool IsKeyDown(uint32 key)
{
	int flKey = 0;
	if(key >= 'A' && key <= 'Z')
		flKey = static_cast<int>(key - 'A' + 'a');
	else if(key >= '0' && key <= '9')
		flKey = static_cast<int>(key);
	else if(key >= Key_F1 && key <= Key_F24)
		flKey = FL_F + 1 + static_cast<int>(key - Key_F1);
	else if(key >= Key_NUMPAD0 && key <= Key_NUMPAD9)
		flKey = FL_KP + '0' + static_cast<int>(key - Key_NUMPAD0);
	else
	{
		switch(key)
		{
		case Key_SHIFT: return Fl::get_key(FL_Shift_L) || Fl::get_key(FL_Shift_R);
		case Key_CONTROL: return Fl::get_key(FL_Control_L) || Fl::get_key(FL_Control_R);
		case Key_MENU: return Fl::get_key(FL_Alt_L) || Fl::get_key(FL_Alt_R);
		case Key_LSHIFT: flKey = FL_Shift_L; break;
		case Key_RSHIFT: flKey = FL_Shift_R; break;
		case Key_LCONTROL: flKey = FL_Control_L; break;
		case Key_RCONTROL: flKey = FL_Control_R; break;
		case Key_LMENU: flKey = FL_Alt_L; break;
		case Key_RMENU: flKey = FL_Alt_R; break;
		case Key_LWIN: flKey = FL_Meta_L; break;
		case Key_RWIN: flKey = FL_Meta_R; break;
		case Key_BACK: flKey = FL_BackSpace; break;
		case Key_TAB: flKey = FL_Tab; break;
		case Key_RETURN: flKey = FL_Enter; break;
		case Key_ESCAPE: flKey = FL_Escape; break;
		case Key_SPACE: flKey = ' '; break;
		case Key_LEFT: flKey = FL_Left; break;
		case Key_RIGHT: flKey = FL_Right; break;
		case Key_UP: flKey = FL_Up; break;
		case Key_DOWN: flKey = FL_Down; break;
		case Key_HOME: flKey = FL_Home; break;
		case Key_END: flKey = FL_End; break;
		case Key_PRIOR: flKey = FL_Page_Up; break;
		case Key_NEXT: flKey = FL_Page_Down; break;
		case Key_INSERT: flKey = FL_Insert; break;
		case Key_DELETE: flKey = FL_Delete; break;
		case Key_NUMLOCK: flKey = FL_Num_Lock; break;
		case Key_CAPITAL: flKey = FL_Caps_Lock; break;
		case Key_SCROLL: flKey = FL_Scroll_Lock; break;
		default: return false;
		}
	}
	return Fl::get_key(flKey) != 0;
}


mpt::ustring Format(const mpt::uchar *format, ...)
{
	va_list args;
	va_start(args, format);
	va_list argsCopy;
	va_copy(argsCopy, args);
	const int length = std::vsnprintf(nullptr, 0, reinterpret_cast<const char *>(format), argsCopy);
	va_end(argsCopy);
	std::string result(length > 0 ? static_cast<size_t>(length) : 0, '\0');
	if(length > 0)
		std::vsnprintf(result.data(), result.size() + 1, reinterpret_cast<const char *>(format), args);
	va_end(args);
	return mpt::transcode<mpt::ustring>(mpt::common_encoding::utf8, result);
}


std::string FormatA(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	va_list argsCopy;
	va_copy(argsCopy, args);
	const int length = std::vsnprintf(nullptr, 0, format, argsCopy);
	va_end(argsCopy);
	std::string result(length > 0 ? static_cast<size_t>(length) : 0, '\0');
	if(length > 0)
		std::vsnprintf(result.data(), result.size() + 1, format, args);
	va_end(args);
	return result;
}


void Beep()
{
	fl_beep();
}


uint32 Wnd::GetCheckedRadioButton(uint32 firstId, uint32 lastId) const
{
	for(uint32 id = firstId; id <= lastId; ++id)
	{
		if(IsDlgButtonChecked(id))
			return id;
	}
	return 0;
}


}  // namespace ui


int wsprintf(mpt::uchar *buffer, const mpt::uchar *format, ...)
{
	va_list args;
	va_start(args, format);
	const int length = std::vsnprintf(reinterpret_cast<char *>(buffer), 1024, reinterpret_cast<const char *>(format), args);
	va_end(args);
	return length;
}


OPENMPT_NAMESPACE_END
