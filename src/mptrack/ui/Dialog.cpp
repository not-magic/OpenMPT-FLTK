// MFC replacement on FLTK. Dialog windows (modal and modeless) built from the dialog templates in the resource
// tables.

#include "stdafx.h"
#include "Dialog.h"
#include "Menu.h"
#include "Controls.h"
#include "StandardIds.h"

#include <FL/Fl.H>
#include <FL/Fl_Group.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <unordered_map>

// From FL/platform.H, which can't be included here because X11 names clash with ours
void fl_open_display();
#include <vector>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{


namespace
{

constexpr int kDefaultFontSize = 12;
// MS Shell Dlg at 8pt and 96 DPI is 13 pixels high
constexpr double kWindowsDialogUnitY = 13.0 / 8.0;

enum WindowStyle : uint32
{
	StyleGroup = 0x00020000,
	StyleTabStop = 0x00010000,
	StyleDisabled = 0x08000000,
	StyleInvisible = 0x00000000,
	StyleVisible = 0x10000000,
};

enum SpinStyle : uint32
{
	SpinSetBuddyInt = 0x02,
	SpinAutoBuddy = 0x10,
};

// Keeps spin buttons attached to a buddy control that is replaced by a bound member
void RetargetSpinBuddies(Fl_Group &group, const Wnd *oldBuddy, Wnd *newBuddy)
{
	for(int i = 0; i < group.children(); ++i)
	{
		if(SpinButton *spin = dynamic_cast<SpinButton *>(group.child(i)); spin != nullptr && spin->GetBuddy() == oldBuddy)
			spin->SetBuddy(newBuddy);
	}
}

struct DialogUnits
{
	double x = 0.0;
	double y = 0.0;
};

// Like Windows, derive the dialog unit size from the average character width and height of the dialog font
const DialogUnits &GetDialogUnits()
{
	static const DialogUnits units = []
	{
		fl_open_display();
		fl_font(FL_HELVETICA, kDefaultFontSize);
		const double averageWidth = fl_width("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz") / 52.0;
		return DialogUnits{averageWidth / 4.0, fl_height() / 8.0};
	}();
	return units;
}

std::unordered_map<std::string, ControlFactory> &GetFactories()
{
	static std::unordered_map<std::string, ControlFactory> factories;
	return factories;
}

// Window that keeps its dialog the size of its own
class DialogFrame : public Fl_Double_Window
{
public:
	DialogFrame(int width, int height, const char *title, Dialog &dialog)
	    : Fl_Double_Window(width, height, title)
	    , m_dialog(dialog)
	{
	}

	void resize(int x, int y, int width, int height) override
	{
		const bool isMoved = (x != this->x() || y != this->y());
		Fl_Double_Window::resize(x, y, width, height);
		if(m_dialog.IsResizable())
			m_dialog.MoveWindow(0, 0, width, height);
		if(isMoved)
			m_dialog.OnMove(x, y);
	}

	int handle(int event) override
	{
		if(event == FL_FOCUS)
			m_dialog.OnActivate(true);
		else if(event == FL_UNFOCUS)
			m_dialog.OnActivate(false);
		return Fl_Double_Window::handle(event);
	}

private:
	Dialog &m_dialog;
};

int FindWrappedLabelHeight(const Fl_Widget &widget)
{
	int width = widget.w() - Fl::box_dw(widget.box());
	int height = 0;
	fl_font(widget.labelfont(), widget.labelsize());
	fl_measure(widget.label(), width, height, 0);
	return height + Fl::box_dh(widget.box());
}

// Windows fonts are narrower than FLTK's, so wrapped labels may need more lines than the template allows.
// Grow such labels and push everything below them down.
void FitWrappedLabels(Fl_Group &group)
{
	std::vector<Fl_Widget *> labels;
	for(int i = 0; i < group.children(); ++i)
	{
		Fl_Widget *child = group.child(i);
		if(dynamic_cast<Static *>(child) != nullptr && child->label() != nullptr && (child->align() & FL_ALIGN_WRAP) && child->image() == nullptr)
			labels.push_back(child);
	}
	std::sort(labels.begin(), labels.end(), [](const Fl_Widget *a, const Fl_Widget *b) { return a->y() < b->y(); });
	int growTotal = 0;
	for(Fl_Widget *label : labels)
	{
		const int growHeight = FindWrappedLabelHeight(*label) - label->h();
		if(growHeight <= 0)
			continue;
		const int bottomY = label->y() + label->h();
		for(int i = 0; i < group.children(); ++i)
		{
			Fl_Widget *child = group.child(i);
			if(child == label)
				continue;
			if(child->y() >= bottomY)
				child->position(child->x(), child->y() + growHeight);
			else if(child->y() + child->h() >= bottomY)
				child->size(child->w(), child->h() + growHeight);
		}
		label->size(label->w(), label->h() + growHeight);
		growTotal += growHeight;
	}
	if(growTotal > 0)
	{
		group.Fl_Widget::resize(group.x(), group.y(), group.w(), group.h() + growTotal);
		group.init_sizes();
	}
}

}  // namespace


void RegisterControlClass(const std::string &className, ControlFactory factory)
{
	GetFactories()[className] = std::move(factory);
}


int DialogUnitsToPixelsX(int units)
{
	return static_cast<int>(units * GetDialogUnits().x + 0.5);
}


int DialogUnitsToPixelsY(int units)
{
	return static_cast<int>(units * GetDialogUnits().y + 0.5);
}


int WindowsPixelsToPixelsY(int pixels)
{
	return static_cast<int>(pixels * GetDialogUnits().y / kWindowsDialogUnitY + 0.5);
}


int PixelsToWindowsPixelsY(int pixels)
{
	return static_cast<int>(pixels * kWindowsDialogUnitY / GetDialogUnits().y + 0.5);
}


bool Wnd::CreateChild(Wnd &parent, const Rect &rect, uint32 id)
{
	Fl_Group *group = parent.GetWidget()->as_group();
	if(group == nullptr)
		return false;
	Fl_Widget *widget = GetWidget();
	widget->resize(group->x() + rect.left, group->y() + rect.top, rect.Width(), rect.Height());
	SetDlgCtrlID(id);
	group->add(widget);
	widget->show();
	return true;
}


bool Wnd::SubclassDlgItem(uint32 id, Wnd *parent)
{
	Dialog *dialog = nullptr;
	for(Wnd *wnd = parent; wnd != nullptr && dialog == nullptr; wnd = wnd->GetParent())
		dialog = dynamic_cast<Dialog *>(wnd);
	// A FormView's dialog is a child of the view rather than an ancestor
	if(dialog == nullptr && parent != nullptr)
	{
		if(Wnd *item = parent->GetDlgItem(id))
		{
			for(Wnd *wnd = item->GetParent(); wnd != nullptr && dialog == nullptr; wnd = wnd->GetParent())
				dialog = dynamic_cast<Dialog *>(wnd);
		}
	}
	if(dialog == nullptr || dialog->GetDlgItem(id) == nullptr)
		return false;
	DataExchange dx(*dialog, false);
	dx.BindControl(id, *this);
	return true;
}


void DataExchange::BindControl(uint32 id, Wnd &member)
{
	Wnd *existing = m_dialog.GetDlgItem(id);
	if(existing == nullptr || existing == &member)
		return;
	Fl_Widget *oldWidget = existing->GetWidget();
	Fl_Widget *newWidget = member.GetWidget();
	Fl_Group *group = oldWidget->parent();
	if(group == nullptr)
		return;
	const int index = group->find(oldWidget);
	newWidget->resize(oldWidget->x(), oldWidget->y(), oldWidget->w(), oldWidget->h());
	newWidget->argument(oldWidget->argument());
	if(oldWidget->label() != nullptr)
		newWidget->copy_label(oldWidget->label());
	newWidget->labelfont(oldWidget->labelfont());
	newWidget->labelsize(oldWidget->labelsize());
	newWidget->align(oldWidget->align());
	if(!oldWidget->visible())
		newWidget->hide();
	if(!oldWidget->active())
		newWidget->deactivate();
	const auto it = m_dialog.m_controlTemplates.find(id);
	if(it != m_dialog.m_controlTemplates.end())
		member.ConfigureFromTemplate(*it->second);
	if(Spinner *spinner = dynamic_cast<Spinner *>(&member))
	{
		if(const auto spinIt = m_dialog.m_spinTemplates.find(id); spinIt != m_dialog.m_spinTemplates.end())
			spinner->ConfigureSpinFromTemplate(*spinIt->second);
	}
	if(const SpinButton *oldSpin = dynamic_cast<const SpinButton *>(existing))
	{
		if(SpinButton *newSpin = dynamic_cast<SpinButton *>(&member))
			newSpin->SetBuddy(oldSpin->GetBuddy());
	}
	RetargetSpinBuddies(*group, existing, &member);
	group->remove(oldWidget);
	group->insert(*newWidget, index);
	delete oldWidget;
}


Dialog::Dialog()
{
	m_isFocusable = false;
	m_isCustomPaint = false;
}


Dialog::Dialog(uint32 templateId, Wnd *)
    : m_templateId(templateId)
{
	m_isCustomPaint = false;
}


Dialog::~Dialog()
{
	DestroyFrame();
}


int Dialog::CalcTemplateHeight() const
{
	const DialogTemplate *dialogTemplate = FindDialogTemplate(m_templateId);
	return dialogTemplate ? DialogUnitsToPixelsY(dialogTemplate->height) : 0;
}


void Dialog::CreateControls()
{
	const DialogTemplate *dialogTemplate = FindDialogTemplate(m_templateId);
	if(dialogTemplate == nullptr)
		return;
	const int width = DialogUnitsToPixelsX(dialogTemplate->width);
	const int height = DialogUnitsToPixelsY(dialogTemplate->height);
	resize(x(), y(), width, height);
	begin();
	int radioGroup = 0;
	Wnd *previous = nullptr;
	for(std::size_t i = 0; i < dialogTemplate->controlCount; ++i)
	{
		const DialogControl &control = dialogTemplate->controls[i];
		const int controlX = x() + DialogUnitsToPixelsX(control.x);
		const int controlY = y() + DialogUnitsToPixelsY(control.y);
		const int controlWidth = std::max(DialogUnitsToPixelsX(control.width), 1);
		const int controlHeight = std::max(DialogUnitsToPixelsY(control.height), 1);
		if(control.style & StyleGroup)
			++radioGroup;

		Wnd *created = nullptr;
		const std::string className = (control.kind == ControlKind::Control) ? std::string(control.windowClass) : std::string();
		// An edit field and the up-down control attached to it become one spinner with the edit field's ID
		if(className == "msctls_updown32" && (control.style & (SpinAutoBuddy | SpinSetBuddyInt)) != 0 && dynamic_cast<Edit *>(previous) != nullptr)
		{
			Fl_Widget *edit = previous->GetWidget();
			const uint32 editId = static_cast<uint32>(edit->argument());
			Spinner *spinner = new Spinner(edit->x(), edit->y(), edit->w(), edit->h());
			Fl_Widget *spinnerWidget = spinner->GetWidget();
			spinnerWidget->argument(edit->argument());
			spinnerWidget->labelsize(kDefaultFontSize);
			if(!edit->active())
				spinnerWidget->deactivate();
			if(!edit->visible())
				spinnerWidget->hide();
			spinner->ConfigureFromTemplate(*m_controlTemplates[editId]);
			spinner->ConfigureSpinFromTemplate(control);
			m_spinTemplates[editId] = &control;
			insert(*spinnerWidget, find(edit));
			remove(edit);
			delete edit;
			previous = spinner;
			continue;
		}
		if(!className.empty() && className != "Button" && className != "Static")
		{
			const auto it = GetFactories().find(className);
			if(it != GetFactories().end())
				created = it->second(control, controlX, controlY, controlWidth, controlHeight);
		}
		if(created == nullptr)
		{
			switch(control.kind)
			{
			case ControlKind::Ltext:
			case ControlKind::Ctext:
			case ControlKind::Rtext:
			case ControlKind::Icon:
				created = new Static(controlX, controlY, controlWidth, controlHeight);
				break;
			case ControlKind::Edittext:
				created = new Edit(controlX, controlY, controlWidth, controlHeight);
				break;
			case ControlKind::Combobox:
				created = new ComboBox(controlX, controlY, controlWidth, controlHeight);
				break;
			case ControlKind::Listbox:
				created = new ListBox(controlX, controlY, controlWidth, controlHeight);
				break;
			case ControlKind::Scrollbar:
				created = new ScrollBar(controlX, controlY, controlWidth, controlHeight);
				break;
			case ControlKind::Pushbutton:
			case ControlKind::DefPushButton:
			case ControlKind::Autocheckbox:
			case ControlKind::Autoradiobutton:
			case ControlKind::Checkbox:
			case ControlKind::Radiobutton:
			case ControlKind::State3:
				created = new Button(controlX, controlY, controlWidth, controlHeight);
				break;
			case ControlKind::Groupbox:
				created = new Static(controlX, controlY, controlWidth, controlHeight);
				break;
			case ControlKind::Control:
				if(className == "Button")
					created = new Button(controlX, controlY, controlWidth, controlHeight);
				else
					created = new Static(controlX, controlY, controlWidth, controlHeight);
				break;
			}
		}
		Fl_Widget *widget = created->GetWidget();
		widget->argument(static_cast<long>(control.id));
		// Common controls without a visible caption still carry placeholder text like "Slider1" in templates
		const bool hasCaption = className != "msctls_trackbar32" && className != "msctls_updown32" && className != "msctls_progress32" && className != "ToolbarWindow32";
		if(hasCaption && control.text != nullptr && control.text[0] != '\0')
			widget->copy_label(control.text);
		widget->labelsize(kDefaultFontSize);
		if(control.style & StyleDisabled)
			widget->deactivate();
		if(control.exStyle == 0 && control.kind == ControlKind::Control && (control.style & StyleVisible) == 0 && (control.clearedStyle & StyleVisible))
			widget->hide();
		created->ConfigureFromTemplate(control);
		if(Button *button = dynamic_cast<Button *>(created))
			button->SetRadioGroup(radioGroup);
		if(SpinButton *spin = dynamic_cast<SpinButton *>(created))
		{
			if((control.style & SpinAutoBuddy) != 0 && previous != nullptr)
				spin->SetBuddy(previous);
		}
		m_controlTemplates[static_cast<uint32>(control.id)] = &control;
		previous = created;
	}
	end();
	FitWrappedLabels(*this);
}


bool Dialog::CreateFrame(Wnd *parent, bool isModal)
{
	const DialogTemplate *dialogTemplate = FindDialogTemplate(m_templateId);
	if(dialogTemplate == nullptr)
		return false;
	if(m_frame != nullptr)
		return true;
	// Build the controls first so that the size of the window is known
	resize(0, 0, DialogUnitsToPixelsX(dialogTemplate->width), DialogUnitsToPixelsY(dialogTemplate->height));
	CreateControls();
	m_frame = new DialogFrame(w(), h(), dialogTemplate->caption, *this);
	m_frame->copy_label(dialogTemplate->caption);
	m_frame->user_data(this);
	m_frame->callback(
	    [](Fl_Widget *, void *data)
	    {
		    static_cast<Dialog *>(data)->OnCancel();
	    },
	    this);
	m_frame->add(this);
	this->resize(0, 0, m_frame->w(), m_frame->h());
	m_frame->end();
	if(isModal)
		m_frame->set_modal();
	else
		m_frame->set_non_modal();
	MPT_UNUSED(parent);
	return true;
}


void Dialog::DestroyFrame()
{
	if(m_frame == nullptr)
		return;
	m_frame->remove(this);
	m_frame->hide();
	delete m_frame;
	m_frame = nullptr;
}


bool Dialog::Create(uint32 templateId, Wnd *parent, bool isModal)
{
	m_templateId = templateId;
	m_isModal = false;
	if(!CreateFrame(parent, isModal))
		return false;
	UpdateData(false);
	OnInitDialog();
	CenterWindow(parent);
	m_frame->show();
	return true;
}


bool Dialog::CreateChild(uint32 templateId, Wnd &parent, int x, int y)
{
	m_templateId = templateId;
	CreateControls();
	resize(x, y, w(), h());
	Fl_Group *group = parent.GetWidget()->as_group();
	if(group == nullptr)
		return false;
	group->add(this);
	UpdateData(false);
	OnInitDialog();
	return true;
}


intptr_t Dialog::DoModal()
{
	m_isModal = true;
	m_isDone = false;
	m_result = IDCANCEL;
	if(!CreateFrame(nullptr, true))
		return -1;
	UpdateData(false);
	OnInitDialog();
	CenterWindow(nullptr);
	m_frame->show();
	while(!m_isDone && m_frame->shown())
		Fl::wait();
	DestroyFrame();
	return m_result;
}


void Dialog::Finish(intptr_t result)
{
	m_result = result;
	m_isDone = true;
	if(m_frame != nullptr)
		m_frame->hide();
}


void Dialog::EndDialog(intptr_t result)
{
	Finish(result);
}


bool Dialog::UpdateData(bool isSaveAndValidate)
{
	DataExchange dx(*this, isSaveAndValidate);
	dx.m_bSaveAndValidate = isSaveAndValidate;
	DoDataExchange(&dx);
	return true;
}


void Dialog::resize(int x, int y, int width, int height)
{
	Panel::resize(x, y, width, height);
	if(m_frame != nullptr && !m_isSyncingFrame && (m_frame->w() != width || m_frame->h() != height))
	{
		m_isSyncingFrame = true;
		m_frame->size(width, height);
		m_isSyncingFrame = false;
	}
}


void Dialog::SetMenu(Menu *menu)
{
	constexpr int menuBarHeight = 24;
	m_menu = menu;
	if(m_menuBar == nullptr && menu != nullptr)
	{
		Fl_Group *group = this;
		for(int i = 0; i < group->children(); ++i)
			group->child(i)->position(group->child(i)->x(), group->child(i)->y() + menuBarHeight);
		Fl_Widget::resize(x(), y(), w(), h() + menuBarHeight);
		if(m_frame != nullptr)
			m_frame->size(w(), h());
		begin();
		m_menuBar = new MenuBar(x(), y(), w(), menuBarHeight, *this);
		m_menuBar->onBeforeOpen = [this]() { OnInitMenu(); };
		end();
	}
	if(m_menuBar != nullptr)
		m_menuBar->SetMenu(menu);
}


void Dialog::DrawMenuBar()
{
	if(m_menuBar != nullptr)
		m_menuBar->Rebuild();
}


void Dialog::CenterWindow(Wnd *relativeTo)
{
	if(m_frame == nullptr)
		return;
	int centerX = 0;
	int centerY = 0;
	if(relativeTo != nullptr)
	{
		const Rect rect = relativeTo->GetWindowRect();
		centerX = (rect.left + rect.right) / 2;
		centerY = (rect.top + rect.bottom) / 2;
	} else
	{
		int screenX, screenY, screenW, screenH;
		Fl::screen_xywh(screenX, screenY, screenW, screenH);
		centerX = screenX + screenW / 2;
		centerY = screenY + screenH / 2;
	}
	m_frame->position(centerX - m_frame->w() / 2, centerY - m_frame->h() / 2);
}


void Dialog::GotoDlgCtrl(Wnd *control)
{
	if(control != nullptr)
		control->SetFocus();
}


bool Dialog::OnInitDialog()
{
	m_isInitialized = true;
	return true;
}


void Dialog::OnOK()
{
	if(!UpdateData(true))
		return;
	Finish(IDOK);
}


void Dialog::OnCancel()
{
	Finish(IDCANCEL);
}


void Dialog::DoDataExchange(DataExchange *)
{
}


void Dialog::PostNcDestroy()
{
}


void Dialog::DestroyWindow()
{
	if(m_frame != nullptr)
	{
		OnDestroy();
		DestroyFrame();
	} else
	{
		Wnd::DestroyWindow();
	}
	PostNcDestroy();
}


UI_MESSAGE_MAP_BEGIN(Dialog, Wnd)
	UI_COMMAND(IDOK, &Dialog::OnOK)
	UI_COMMAND(IDCANCEL, &Dialog::OnCancel)
UI_MESSAGE_MAP_END()


}  // namespace ui


OPENMPT_NAMESPACE_END
