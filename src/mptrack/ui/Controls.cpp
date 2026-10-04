// MFC replacement on FLTK. Standard input and display controls: buttons, edit fields, combo boxes, sliders,
// spin buttons, scroll bars, static text and list boxes.

#include "stdafx.h"
#include "Controls.h"
#include "Dialog.h"

#include <FL/Fl.H>
#include <FL/Fl_Multiline_Input.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <cmath>
#include <cstdlib>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{


namespace
{

// Dialog templates give combo boxes the height of their drop-down list; the closed box is one edit line tall
constexpr int kComboBoxHeightDlu = 12;

int FindComboBoxHeight(int height)
{
	return std::min(height, DialogUnitsToPixelsY(kComboBoxHeightDlu));
}

std::string ToUtf8(const mpt::ustring &text)
{
	return mpt::transcode<std::string>(mpt::common_encoding::utf8, text);
}

mpt::ustring FromUtf8(const std::string &text)
{
	return mpt::transcode<mpt::ustring>(mpt::common_encoding::utf8, text);
}

// Ported code builds Win32 "\r\n" line breaks, which Fl_Input would draw as ^M
std::string ToEditText(const mpt::ustring &text)
{
	std::string utf8 = ToUtf8(text);
	utf8.erase(std::remove(utf8.begin(), utf8.end(), '\r'), utf8.end());
	return utf8;
}

}  // namespace


namespace
{

// Style bits of the dialog templates
constexpr uint32 kButtonTypeMask = 0x0F;
constexpr uint32 kButtonCheck = 2;
constexpr uint32 kButtonAutoCheck = 3;
constexpr uint32 kButtonRadio = 4;
constexpr uint32 kButtonAutoRadio = 9;
constexpr uint32 kButtonDefault = 1;
constexpr uint32 kButtonFlat = 0x8000;
constexpr uint32 kButtonLeftText = 0x20;
constexpr uint32 kStaticAlignMask = 0x03;
constexpr uint32 kStaticTypeMask = 0x1F;
constexpr uint32 kStaticEtchedFrame = 0x12;
constexpr uint32 kStaticSunken = 0x1000;
constexpr uint32 kStaticCenterImage = 0x200;
constexpr uint32 kStaticNoPrefix = 0x80;
constexpr uint32 kEditMultiline = 0x4;
constexpr uint32 kEditNumber = 0x2000;
constexpr uint32 kEditReadOnly = 0x800;
constexpr uint32 kEditUpperCase = 0x8;
constexpr uint32 kEditCenter = 1;
constexpr uint32 kEditRight = 2;
constexpr uint32 kComboTypeMask = 0x03;
constexpr uint32 kComboDropDown = 2;
constexpr uint32 kScrollVertical = 1;
constexpr uint32 kSpinWrap = 1;
constexpr uint32 kSpinSetBuddyInt = 2;

constexpr int kKnobLength = 13;
constexpr int kKnobThickness = 26;
constexpr int kSmallKnobLength = 7;
constexpr int kSmallKnobThickness = 13;

}  // namespace


void NotifyParent(Wnd &control, uint32 code)
{
	if(Wnd *parent = control.GetParent())
	{
		NotifyHeader header;
		header.from = &control;
		header.id = control.GetDlgCtrlID();
		header.code = code;
		parent->RouteCommand(header.id, code, &header);
	}
}


Button::Button(int x, int y, int width, int height, const char *label)
    : WndT<Fl_Button>(x, y, width, height, label)
{
	when(FL_WHEN_RELEASE);
	callback(
	    [](Fl_Widget *widget, void *)
	    {
		    if(Button *button = dynamic_cast<Button *>(widget))
			    NotifyParent(*button, 0);
	    });
}


void Button::ConfigureFromTemplate(const DialogControl &control)
{
	Kind kind = Kind::Push;
	switch(control.kind)
	{
	case ControlKind::DefPushButton:
		kind = Kind::DefaultPush;
		break;
	case ControlKind::Autocheckbox:
	case ControlKind::Checkbox:
	case ControlKind::State3:
		kind = Kind::Check;
		break;
	case ControlKind::Autoradiobutton:
	case ControlKind::Radiobutton:
		kind = Kind::Radio;
		break;
	case ControlKind::Groupbox:
		kind = Kind::Push;
		break;
	case ControlKind::Control:
	{
		const uint32 type = control.style & kButtonTypeMask;
		if(type == kButtonCheck || type == kButtonAutoCheck || type == 5 || type == 6)
			kind = Kind::Check;
		else if(type == kButtonRadio || type == kButtonAutoRadio)
			kind = Kind::Radio;
		else if(type == kButtonDefault)
			kind = Kind::DefaultPush;
		if((control.style & 0x1000) != 0)  // push-like toggle
			kind = Kind::Push;
		break;
	}
	default:
		break;
	}
	SetKind(kind);
	m_isFlat = (control.style & kButtonFlat) != 0;
	if((control.style & 0x1000) != 0)
		type(FL_TOGGLE_BUTTON);
}


void Button::SetKind(Kind kind)
{
	m_kind = kind;
	switch(kind)
	{
	case Kind::Push:
	case Kind::DefaultPush:
		type(FL_NORMAL_BUTTON);
		break;
	case Kind::Check:
	case Kind::Radio:
		type(FL_TOGGLE_BUTTON);
		break;
	}
}


void Button::SetCheck(int state)
{
	value(state != 0 ? 1 : 0);
	redraw();
}


void Button::draw()
{
	if(m_kind == Kind::Push || m_kind == Kind::DefaultPush)
	{
		Fl_Button::draw();
		return;
	}
	// Without a box, anti-aliased text would be drawn over the previous copy and get darker with each redraw
	fl_color(parent() ? parent()->color() : color());
	fl_rectf(x(), y(), w(), h());
	const int boxSize = std::min(h(), 13);
	const int boxX = x();
	const int boxY = y() + (h() - boxSize) / 2;
	fl_color(active_r() ? FL_BACKGROUND2_COLOR : FL_BACKGROUND_COLOR);
	if(m_kind == Kind::Check)
	{
		fl_rectf(boxX, boxY, boxSize, boxSize);
		fl_color(FL_DARK3);
		fl_rect(boxX, boxY, boxSize, boxSize);
		if(value())
		{
			fl_color(active_r() ? FL_FOREGROUND_COLOR : FL_INACTIVE_COLOR);
			fl_line_style(FL_SOLID, 2);
			fl_line(boxX + 3, boxY + boxSize / 2, boxX + boxSize / 2 - 1, boxY + boxSize - 4);
			fl_line(boxX + boxSize / 2 - 1, boxY + boxSize - 4, boxX + boxSize - 3, boxY + 3);
			fl_line_style(FL_SOLID, 0);
		}
	} else
	{
		fl_pie(boxX, boxY, boxSize, boxSize, 0.0, 360.0);
		fl_color(FL_DARK3);
		fl_arc(boxX, boxY, boxSize, boxSize, 0.0, 360.0);
		if(value())
		{
			fl_color(active_r() ? FL_FOREGROUND_COLOR : FL_INACTIVE_COLOR);
			fl_pie(boxX + 3, boxY + 3, boxSize - 6, boxSize - 6, 0.0, 360.0);
		}
	}
	fl_font(labelfont(), labelsize());
	fl_color(active_r() ? labelcolor() : fl_inactive(labelcolor()));
	const int textX = boxX + boxSize + 4;
	const char previousDrawShortcut = fl_draw_shortcut;
	fl_draw_shortcut = 1;
	fl_draw(label() ? label() : "", textX, y(), w() - (textX - x()), h(), FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
	fl_draw_shortcut = previousDrawShortcut;
	if(Fl::focus() == this)
		draw_focus(FL_NO_BOX, textX - 2, y(), w() - (textX - x()) + 2, h());
}


int Button::handle(int event)
{
	if(m_kind == Kind::Radio && event == FL_RELEASE && Fl::event_inside(this))
	{
		if(Fl_Group *group = parent())
		{
			for(int i = 0; i < group->children(); ++i)
			{
				if(Button *other = dynamic_cast<Button *>(group->child(i)))
				{
					if(other != this && other->m_kind == Kind::Radio && other->m_radioGroup == m_radioGroup)
						other->value(0);
				}
			}
		}
		value(1);
		redraw();
		do_callback();
		return 1;
	}
	return WndT<Fl_Button>::handle(event);
}


Static::Static(int x, int y, int width, int height, const char *label)
    : WndT<Fl_Box>(x, y, width, height, label)
{
	align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
}


void Static::ConfigureFromTemplate(const DialogControl &control)
{
	if(control.kind == ControlKind::Groupbox || (control.style & kStaticNoPrefix) == 0)
		set_flag(SHORTCUT_LABEL);
	Fl_Align alignment = FL_ALIGN_LEFT;
	switch(control.kind)
	{
	case ControlKind::Ctext:
		alignment = FL_ALIGN_CENTER;
		break;
	case ControlKind::Rtext:
		alignment = FL_ALIGN_RIGHT;
		break;
	case ControlKind::Groupbox:
		m_isGroupBox = true;
		box(FL_NO_BOX);
		return;
	default:
		break;
	}
	if(control.kind == ControlKind::Control)
	{
		const uint32 align = control.style & kStaticAlignMask;
		if(align == 1)
			alignment = FL_ALIGN_CENTER;
		else if(align == 2)
			alignment = FL_ALIGN_RIGHT;
		if((control.style & kStaticTypeMask) == kStaticEtchedFrame)
			box(FL_ENGRAVED_FRAME);
	}
	if(control.exStyle & 0x20000)  // static edge
		box(FL_THIN_DOWN_BOX);
	if(control.style & kStaticSunken)
		box(FL_THIN_DOWN_BOX);
	this->align(alignment | FL_ALIGN_INSIDE | FL_ALIGN_CLIP | ((control.style & kStaticCenterImage) ? 0 : FL_ALIGN_WRAP));
}


void Static::draw()
{
	if(!m_isGroupBox)
	{
		// Unlike Fl_Widget::draw_label(), Windows statics have no horizontal text inset
		draw_box();
		draw_label(x() + Fl::box_dx(box()), y() + Fl::box_dy(box()), w() - Fl::box_dw(box()), h() - Fl::box_dh(box()), align());
		return;
	}
	// Like Windows, the caption sits on the top edge of the frame
	fl_font(labelfont(), labelsize());
	const int textHeight = fl_height();
	const int frameY = y() + textHeight / 2;
	draw_box(FL_ENGRAVED_FRAME, x(), frameY, w(), h() - (frameY - y()), color());
	if(label() == nullptr || label()[0] == '\0')
		return;
	int textWidth = 0, measuredHeight = 0;
	fl_measure(label(), textWidth, measuredHeight, 0);
	constexpr int kCaptionIndentX = 6;
	constexpr int kCaptionPaddingX = 2;
	const int captionWidth = std::min(textWidth + 2 * kCaptionPaddingX, w() - 2 * kCaptionIndentX);
	if(captionWidth <= 0)
		return;
	fl_color(color());
	fl_rectf(x() + kCaptionIndentX, y(), captionWidth, textHeight);
	draw_label(x() + kCaptionIndentX + kCaptionPaddingX, y(), captionWidth - 2 * kCaptionPaddingX, textHeight, FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
}


void Static::SetCentered(bool isCentered)
{
	align((isCentered ? FL_ALIGN_CENTER : FL_ALIGN_LEFT) | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
}


namespace
{
// Returns an image that owns a copy of the pixels
Fl_RGB_Image *CreateImage(const Bitmap *bitmap)
{
	if(bitmap == nullptr || !bitmap->IsValid())
		return nullptr;
	const int width = bitmap->GetWidth();
	const int height = bitmap->GetHeight();
	uchar *rgb = new uchar[static_cast<std::size_t>(width) * height * 3];
	for(int i = 0; i < width * height; ++i)
	{
		const uint32 pixel = bitmap->GetPixels()[i];
		rgb[i * 3 + 0] = static_cast<uchar>(pixel >> 16);
		rgb[i * 3 + 1] = static_cast<uchar>(pixel >> 8);
		rgb[i * 3 + 2] = static_cast<uchar>(pixel);
	}
	Fl_RGB_Image *rgbImage = new Fl_RGB_Image(rgb, width, height, 3);
	rgbImage->alloc_array = 1;
	return rgbImage;
}
}


void Button::SetBitmap(const Bitmap *bitmap)
{
	image(CreateImage(bitmap));
	redraw();
}


void Static::SetBitmap(const Bitmap *bitmap)
{
	if(bitmap == nullptr || !bitmap->IsValid())
	{
		image(nullptr);
		return;
	}
	// FLTK keeps the pixel data pointer, so the image owns a copy
	const int width = bitmap->GetWidth();
	const int height = bitmap->GetHeight();
	uchar *rgb = new uchar[static_cast<std::size_t>(width) * height * 3];
	for(int i = 0; i < width * height; ++i)
	{
		const uint32 pixel = bitmap->GetPixels()[i];
		rgb[i * 3 + 0] = static_cast<uchar>(pixel >> 16);
		rgb[i * 3 + 1] = static_cast<uchar>(pixel >> 8);
		rgb[i * 3 + 2] = static_cast<uchar>(pixel);
	}
	Fl_RGB_Image *rgbImage = new Fl_RGB_Image(rgb, width, height, 3);
	rgbImage->alloc_array = 1;
	image(rgbImage);
	redraw();
}


Edit::Edit(int x, int y, int width, int height, const char *label)
    : WndT<Fl_Input>(x, y, width, height, label)
{
	when(FL_WHEN_CHANGED);
	callback(
	    [](Fl_Widget *widget, void *)
	    {
		    if(Edit *edit = dynamic_cast<Edit *>(widget))
		    {
			    edit->m_isModified = true;
			    NotifyParent(*edit, EditUpdate);
			    NotifyParent(*edit, EditChange);
		    }
	    });
}


void Edit::ConfigureFromTemplate(const DialogControl &control)
{
	SetMultiline((control.style & kEditMultiline) != 0);
	if(control.style & kEditNumber)
		SetNumberOnly(true);
	SetReadOnly((control.style & kEditReadOnly) != 0);
	SetUpperCase((control.style & kEditUpperCase) != 0);
	if(control.exStyle & 0x200)  // client edge
		box(FL_DOWN_BOX);
}


void Edit::SetNumberOnly(bool isNumberOnly)
{
	type(isNumberOnly ? FL_INT_INPUT : FL_NORMAL_INPUT);
}


void Edit::SetMultiline(bool isMultiline)
{
	type(isMultiline ? FL_MULTILINE_INPUT : FL_NORMAL_INPUT);
}


void Edit::SetSel(int start, int end)
{
	const int length = size();
	if(end < 0)
		end = length;
	insert_position(end, start);
}


void Edit::GetSel(int &start, int &end) const
{
	start = mark();
	end = insert_position();
	if(start > end)
		std::swap(start, end);
}


void Edit::ReplaceSel(const mpt::ustring &text)
{
	const std::string utf8 = ToEditText(text);
	replace(std::min(insert_position(), mark()), std::max(insert_position(), mark()), utf8.c_str(), static_cast<int>(utf8.size()));
}


void Edit::SetWindowText(const mpt::ustring &text)
{
	value(ToEditText(text).c_str());
	m_isModified = false;
}


mpt::ustring Edit::GetWindowText() const
{
	return FromUtf8(value() ? value() : "");
}


int Edit::GetLineCount() const
{
	const char *content = value();
	int lines = 1;
	for(const char *c = content; *c != '\0'; ++c)
	{
		if(*c == '\n')
			++lines;
	}
	return lines;
}


int Edit::handle(int event)
{
	if(m_isUpperCase && event == FL_KEYBOARD && Fl::event_length() == 1)
	{
		const char *text = Fl::event_text();
		if(text[0] >= 'a' && text[0] <= 'z')
		{
			const char upper = static_cast<char>(text[0] - 'a' + 'A');
			replace(std::min(insert_position(), mark()), std::max(insert_position(), mark()), &upper, 1);
			do_callback();
			return 1;
		}
	}
	return WndT<Fl_Input>::handle(event);
}


void Edit::OnSetFocus(Wnd *)
{
	NotifyParent(*this, EditSetFocus);
}


void Edit::OnKillFocus(Wnd *)
{
	NotifyParent(*this, EditKillFocus);
}


ComboBox::ComboBox(int x, int y, int width, int height, const char *label)
    : WndT<Fl_Group>(x, y, width, FindComboBoxHeight(height), label)
{
	CreateInner();
}


ComboBox::~ComboBox()
{
}


void ComboBox::CreateInner()
{
	begin();
	if(m_isEditable)
	{
		m_choice = nullptr;
		m_inputChoice = new Fl_Input_Choice(x(), y(), w(), h());
		m_inputChoice->callback(InnerCallback, this);
		m_inputChoice->when(FL_WHEN_CHANGED | FL_WHEN_RELEASE);
	} else
	{
		m_inputChoice = nullptr;
		m_choice = new Fl_Choice(x(), y(), w(), h());
		m_choice->callback(InnerCallback, this);
	}
	end();
	RebuildMenu();
}


void ComboBox::ConfigureFromTemplate(const DialogControl &control)
{
	SetEditable((control.style & kComboTypeMask) == kComboDropDown);
}


void ComboBox::SetEditable(bool isEditable)
{
	if(isEditable == m_isEditable)
		return;
	m_isEditable = isEditable;
	if(m_choice)
	{
		remove(m_choice);
		delete m_choice;
		m_choice = nullptr;
	}
	if(m_inputChoice)
	{
		remove(m_inputChoice);
		delete m_inputChoice;
		m_inputChoice = nullptr;
	}
	CreateInner();
}


void ComboBox::RebuildMenu()
{
	m_menu.clear();
	for(const Item &item : m_items)
	{
		Fl_Menu_Item entry{};
		entry.text = item.text.c_str();
		m_menu.push_back(entry);
	}
	m_menu.push_back(Fl_Menu_Item{});
	if(m_choice)
	{
		m_choice->menu(m_menu.data());
		m_choice->value(m_currentSelection);
	} else if(m_inputChoice)
	{
		m_inputChoice->menubutton()->menu(m_menu.data());
	}
}


int ComboBox::AddString(const mpt::ustring &text)
{
	if(m_isSorted)
	{
		const std::string key = ToUtf8(text);
		const auto isLess = [](const std::string &a, const std::string &b)
		{
			return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](char x, char y) { return std::tolower(static_cast<unsigned char>(x)) < std::tolower(static_cast<unsigned char>(y)); });
		};
		const auto position = std::upper_bound(m_items.begin(), m_items.end(), key, [&](const std::string &value, const Item &item) { return isLess(value, item.text); });
		const int index = static_cast<int>(position - m_items.begin());
		if(index < static_cast<int>(m_items.size()))
			return InsertString(index, text);
	}
	m_items.push_back(Item{ToUtf8(text), 0});
	RebuildMenu();
	return static_cast<int>(m_items.size()) - 1;
}


int ComboBox::InsertString(int index, const mpt::ustring &text)
{
	if(index < 0 || index >= static_cast<int>(m_items.size()))
		return AddString(text);
	m_items.insert(m_items.begin() + index, Item{ToUtf8(text), 0});
	if(m_currentSelection >= index)
		++m_currentSelection;
	RebuildMenu();
	return index;
}


int ComboBox::InsertItem(int index, const mpt::ustring &text, int, uintptr_t data)
{
	const int position = InsertString(index, text);
	if(position >= 0)
		SetItemData(position, data);
	return position;
}


int ComboBox::DeleteString(int index)
{
	if(index < 0 || index >= static_cast<int>(m_items.size()))
		return -1;
	m_items.erase(m_items.begin() + index);
	if(m_currentSelection == index)
		m_currentSelection = -1;
	else if(m_currentSelection > index)
		--m_currentSelection;
	RebuildMenu();
	return static_cast<int>(m_items.size());
}


void ComboBox::ResetContent()
{
	m_items.clear();
	m_currentSelection = -1;
	RebuildMenu();
	if(m_inputChoice)
		m_inputChoice->value("");
}


int ComboBox::GetCurSel() const
{
	if(m_choice)
		return m_choice->value();
	return m_currentSelection;
}


int ComboBox::SetCurSel(int index)
{
	if(index < 0 || index >= static_cast<int>(m_items.size()))
		index = -1;
	m_currentSelection = index;
	if(m_choice)
		m_choice->value(index);
	else if(m_inputChoice && index >= 0)
		m_inputChoice->value(m_items[index].text.c_str());
	redraw();
	return index;
}


void ComboBox::SetItemData(int index, uintptr_t data)
{
	if(index >= 0 && index < static_cast<int>(m_items.size()))
		m_items[index].data = data;
}


uintptr_t ComboBox::GetItemData(int index) const
{
	if(index >= 0 && index < static_cast<int>(m_items.size()))
		return m_items[index].data;
	return 0;
}


mpt::ustring ComboBox::GetLBText(int index) const
{
	if(index >= 0 && index < static_cast<int>(m_items.size()))
		return FromUtf8(m_items[index].text);
	return {};
}


int ComboBox::GetLBTextLen(int index) const
{
	if(index >= 0 && index < static_cast<int>(m_items.size()))
		return static_cast<int>(m_items[index].text.size());
	return -1;
}


int ComboBox::FindStringExact(int startAfter, const mpt::ustring &text) const
{
	const std::string utf8 = ToUtf8(text);
	const int count = static_cast<int>(m_items.size());
	for(int i = 1; i <= count; ++i)
	{
		const int index = (startAfter + i) % count;
		if(m_items[index].text == utf8)
			return index;
	}
	return -1;
}


int ComboBox::SelectString(int startAfter, const mpt::ustring &text)
{
	const int index = FindStringExact(startAfter, text);
	if(index >= 0)
		SetCurSel(index);
	return index;
}


void ComboBox::SetEditSel(int start, int end)
{
	if(m_inputChoice)
		m_inputChoice->input()->insert_position(end < 0 ? m_inputChoice->input()->size() : end, start);
}


void ComboBox::SetEditLimitText(int maximumLength)
{
	if(m_inputChoice)
		m_inputChoice->input()->maximum_size(maximumLength);
}


void ComboBox::SetEditNumberOnly(bool isNumberOnly)
{
	if(m_inputChoice)
		m_inputChoice->input()->type(isNumberOnly ? FL_INT_INPUT : FL_NORMAL_INPUT);
}


void ComboBox::SetWindowText(const mpt::ustring &text)
{
	if(m_inputChoice)
	{
		m_inputChoice->value(ToUtf8(text).c_str());
	} else
	{
		SelectString(-1, text);
	}
}


mpt::ustring ComboBox::GetWindowText() const
{
	if(m_inputChoice)
		return FromUtf8(m_inputChoice->value() ? m_inputChoice->value() : "");
	const int selection = GetCurSel();
	return selection >= 0 ? GetLBText(selection) : mpt::ustring();
}


void ComboBox::resize(int x, int y, int width, int height)
{
	const int comboHeight = FindComboBoxHeight(height);
	WndT<Fl_Group>::resize(x, y, width, comboHeight);
	if(m_choice)
		m_choice->resize(x, y, width, comboHeight);
	if(m_inputChoice)
		m_inputChoice->resize(x, y, width, comboHeight);
}


void ComboBox::OnInnerChanged(uint32 code)
{
	if(m_choice)
		m_currentSelection = m_choice->value();
	else if(m_inputChoice)
		m_currentSelection = FindStringExact(-1, FromUtf8(m_inputChoice->value() ? m_inputChoice->value() : ""));
	NotifyParent(*this, code);
}


void ComboBox::InnerCallback(Fl_Widget *, void *data)
{
	ComboBox *self = static_cast<ComboBox *>(data);
	self->OnInnerChanged(ComboSelChange);
	if(self->m_inputChoice)
		self->OnInnerChanged(ComboEditChange);
}


template <typename FlSlider>
SliderT<FlSlider>::SliderT(int x, int y, int width, int height, const char *label)
    : WndT<FlSlider>(x, y, width, height, label)
{
	this->step(1);
	this->bounds(0, 100);
	this->callback(SliderCallback);
}


template <typename FlSlider>
void SliderT<FlSlider>::SetRange(int minimum, int maximum, bool)
{
	m_minimum = minimum;
	m_maximum = maximum;
	this->bounds(minimum, maximum);
}


template <typename FlSlider>
void SliderT<FlSlider>::SetRangeMin(int minimum)
{
	SetRange(minimum, m_maximum);
}


template <typename FlSlider>
void SliderT<FlSlider>::SetRangeMax(int maximum)
{
	SetRange(m_minimum, maximum);
}


template <typename FlSlider>
void SliderT<FlSlider>::SetPos(int position)
{
	this->value(position);
}


template <typename FlSlider>
int SliderT<FlSlider>::GetPos() const
{
	const double value = this->value();
	return static_cast<int>(value + (value < 0 ? -0.5 : 0.5));
}


template <typename FlSlider>
void SliderT<FlSlider>::draw()
{
	this->draw_box();
	const bool isHorizontal = this->horizontal() != 0;
	const int x = this->x(), y = this->y();
	const int trackLength = isHorizontal ? this->w() : this->h();
	const int crossSize = isHorizontal ? this->h() : this->w();
	const auto drawTrackBox = [&](Fl_Boxtype boxType, int along, int across, int alongSize, int acrossSize, Fl_Color color)
	{
		if(isHorizontal)
			fl_draw_box(boxType, x + along, y + across, alongSize, acrossSize, color);
		else
			fl_draw_box(boxType, x + across, y + along, acrossSize, alongSize, color);
	};

	const bool isActive = this->active_r() != 0;
	drawTrackBox(FL_THIN_DOWN_BOX, 0, crossSize / 2 - 2, trackLength, 4, isActive ? FL_FOREGROUND_COLOR : FL_INACTIVE_COLOR);

	const bool isSmall = crossSize < kKnobThickness;
	const int knobLength = isSmall ? kSmallKnobLength : kKnobLength;
	const int knobThickness = std::min(crossSize, isSmall ? kSmallKnobThickness : kKnobThickness);
	// Fl_Slider::handle tracks a knob of this length, so the drawn knob is centered on it
	const int handleKnobLength = std::min(trackLength, crossSize / 2 + 5);
	const double range = this->maximum() - this->minimum();
	const double position = (range == 0.0) ? 0.5 : std::clamp((this->value() - this->minimum()) / range, 0.0, 1.0);
	const int knobStart = static_cast<int>(position * (trackLength - handleKnobLength) + 0.5) + (handleKnobLength - knobLength) / 2;
	const int knobAcross = (crossSize - knobThickness) / 2;
	const int markInset = std::max((knobLength - 4) / 2, 2);
	drawTrackBox(FL_UP_BOX, knobStart, knobAcross, knobLength, knobThickness, FL_GRAY);
	drawTrackBox(FL_THIN_DOWN_BOX, knobStart + markInset, knobAcross + 2, knobLength - 2 * markInset, knobThickness - 4, isActive ? this->selection_color() : fl_inactive(this->selection_color()));

	if(Fl::focus() == this)
	{
		if(isHorizontal)
			this->draw_focus(FL_UP_BOX, x + knobStart, y + knobAcross, knobLength, knobThickness);
		else
			this->draw_focus(FL_UP_BOX, x + knobAcross, y + knobStart, knobThickness, knobLength);
	}
}


template <typename FlSlider>
void SliderT<FlSlider>::SliderCallback(Fl_Widget *widget, void *)
{
	SliderT *slider = dynamic_cast<SliderT *>(widget);
	if(slider == nullptr)
		return;
	if(Wnd *parent = slider->GetParent())
	{
		// Sliders report as scroll messages
		NotifyParent(*slider, SliderChange);
		const uint32 position = static_cast<uint32>(slider->GetPos());
		if(slider->horizontal())
			parent->CallHScroll(ScrollThumbTrack, position, slider);
		else
			parent->CallVScroll(ScrollThumbTrack, position, slider);
	}
}


template class SliderT<Fl_Hor_Nice_Slider>;
template class SliderT<Fl_Nice_Slider>;


HSlider::HSlider(int x, int y, int width, int height, const char *label)
    : SliderT<Fl_Hor_Nice_Slider>(x, y, width, height, label)
{
}


VSlider::VSlider(int x, int y, int width, int height, const char *label)
    : SliderT<Fl_Nice_Slider>(x, y, width, height, label)
{
}


ScrollBar::ScrollBar(int x, int y, int width, int height, const char *label)
    : WndT<Fl_Scrollbar>(x, y, width, height, label)
{
	type(FL_HORIZONTAL);
	callback(ScrollCallback, this);
}


void ScrollBar::ConfigureFromTemplate(const DialogControl &control)
{
	SetVertical((control.style & kScrollVertical) != 0);
}


void ScrollBar::SetScrollRange(int minimum, int maximum, bool)
{
	m_minimum = minimum;
	m_maximum = maximum;
	scrollvalue(value(), m_pageSize, m_minimum, std::max(m_maximum - m_minimum + m_pageSize, 1));
}


void ScrollBar::SetScrollInfo(int minimum, int maximum, int pageSize, int position)
{
	m_minimum = minimum;
	m_maximum = maximum;
	m_pageSize = std::max(pageSize, 1);
	scrollvalue(position, m_pageSize, m_minimum, std::max(m_maximum - m_minimum + m_pageSize, 1));
}


bool ScrollBar::GetScrollInfo(ScrollInfo *info, uint32) const
{
	info->nMin = m_minimum;
	info->nMax = m_maximum;
	info->nPage = static_cast<uint32>(m_pageSize);
	info->nPos = GetScrollPos();
	info->nTrackPos = GetScrollPos();
	return true;
}


bool ScrollBar::SetScrollInfo(const ScrollInfo *info, bool)
{
	SetScrollInfo(info->nMin, info->nMax, static_cast<int>(info->nPage), info->nPos);
	return true;
}


void ScrollBar::SetScrollPos(int position)
{
	scrollvalue(position, m_pageSize, m_minimum, std::max(m_maximum - m_minimum + m_pageSize, 1));
}


void ScrollBar::SetVertical(bool isVertical)
{
	type(isVertical ? FL_VERTICAL : FL_HORIZONTAL);
}


void ScrollBar::ScrollCallback(Fl_Widget *widget, void *)
{
	if(ScrollBar *scrollBar = dynamic_cast<ScrollBar *>(widget))
	{
		NotifyParent(*scrollBar, ScrollChange);
		if(Wnd *parent = scrollBar->GetParent())
		{
			if(scrollBar->type() == FL_VERTICAL)
				parent->CallVScroll(ScrollThumbTrack, static_cast<uint32>(scrollBar->value()), scrollBar);
			else
				parent->CallHScroll(ScrollThumbTrack, static_cast<uint32>(scrollBar->value()), scrollBar);
		}
	}
}


SpinButton::SpinButton(int x, int y, int width, int height, const char *label)
    : WndT<Fl_Widget>(x, y, width, height, label)
{
}


Spinner::Spinner(int x, int y, int width, int height, const char *label)
    : WndT<Fl_Spinner>(x, y, width, height, label)
{
	range(0, 100);
	wrap(0);
	type(FL_INT_INPUT);
	format(m_format.c_str());
	value(0);
	input_.when(FL_WHEN_CHANGED | FL_WHEN_ENTER_KEY | FL_WHEN_RELEASE_ALWAYS);
	input_.callback(InputCallback, this);
	up_button_.callback(ButtonCallback, this);
	down_button_.callback(ButtonCallback, this);
	// Clicking the arrows keeps the keyboard focus where it was
	up_button_.clear_visible_focus();
	down_button_.clear_visible_focus();
}


void Spinner::ConfigureFromTemplate(const DialogControl &control)
{
	SetReadOnly((control.style & kEditReadOnly) != 0);
}


void Spinner::ConfigureSpinFromTemplate(const DialogControl &control)
{
	SetWrap((control.style & kSpinWrap) != 0);
}


void Spinner::SetRange(int minimum, int maximum)
{
	m_isReversed = minimum > maximum;
	range(std::min(minimum, maximum), std::max(minimum, maximum));
}


void Spinner::GetRange(int &minimum, int &maximum) const
{
	minimum = static_cast<int>(this->minimum());
	maximum = static_cast<int>(this->maximum());
	if(m_isReversed)
		std::swap(minimum, maximum);
}


int Spinner::GetPos() const
{
	return static_cast<int>(std::lround(GetValue()));
}


void Spinner::SetValue(double newValue)
{
	value(std::clamp(newValue, minimum(), maximum()));
}


double Spinner::GetValue() const
{
	const char *text = input_.value();
	char *end = nullptr;
	const double parsed = std::strtod(text, &end);
	return (end != text) ? parsed : value();
}


void Spinner::SetDecimalPlaces(int digitCount)
{
	type(digitCount > 0 ? FL_FLOAT_INPUT : FL_INT_INPUT);
	m_format = "%." + std::to_string(std::max(digitCount, 0)) + "f";
	format(m_format.c_str());
}


void Spinner::SetAllowFractions(bool allowFractions)
{
	type(allowFractions ? FL_FLOAT_INPUT : FL_INT_INPUT);
	SetDisplayFormat(allowFractions ? "%.10g" : "%.0f");
}


void Spinner::SetDisplayFormat(const std::string &printfFormat)
{
	m_format = printfFormat;
	format(m_format.c_str());
}


void Spinner::SetSel(int start, int end)
{
	if(end < 0)
		end = input_.size();
	input_.insert_position(end, start);
}


void Spinner::SetWindowText(const mpt::ustring &text)
{
	input_.value(ToUtf8(text).c_str());
}


mpt::ustring Spinner::GetWindowText() const
{
	return FromUtf8(input_.value() ? input_.value() : "");
}


void Spinner::InputCallback(Fl_Widget *, void *data)
{
	Spinner *self = static_cast<Spinner *>(data);
	const Fl_Callback_Reason reason = Fl::callback_reason();
	if(reason == FL_REASON_CHANGED)
	{
		NotifyParent(*self, EditUpdate);
		NotifyParent(*self, EditChange);
		return;
	}
	self->CommitInput();
	if(reason == FL_REASON_LOST_FOCUS)
		NotifyParent(*self, EditKillFocus);
}


void Spinner::ButtonCallback(Fl_Widget *widget, void *data)
{
	Spinner *self = static_cast<Spinner *>(data);
	const bool isUp = (widget == &self->up_button_);
	self->Step((isUp != self->m_isReversed) ? 1 : -1);
}


void Spinner::Step(int direction)
{
	const double current = std::clamp(GetValue(), minimum(), maximum());
	// The parent may cancel the step, or change the increment before it is applied
	NotifyHeader header;
	header.from = this;
	header.id = GetDlgCtrlID();
	header.code = SpinDeltaPos;
	SpinDelta delta{static_cast<int>(std::lround(current)), direction};
	header.extra = &delta;
	LResult result = 0;
	if(Wnd *parent = GetParent())
		parent->RouteCommand(header.id, SpinDeltaPos, &header, &result);
	if(result != 0)
		return;
	double newValue = current + direction * step();
	if(newValue > maximum())
		newValue = wrap() ? minimum() : maximum();
	else if(newValue < minimum())
		newValue = wrap() ? maximum() : minimum();
	if(newValue == current)
		return;
	value(newValue);
	NotifyParent(*this, EditUpdate);
	NotifyParent(*this, EditChange);
}


void Spinner::CommitInput()
{
	const char *text = input_.value();
	char *end = nullptr;
	const double parsed = std::strtod(text, &end);
	if(end == text)
		return;
	const double clamped = std::clamp(parsed, minimum(), maximum());
	value(clamped);
	if(clamped != parsed)
	{
		NotifyParent(*this, EditUpdate);
		NotifyParent(*this, EditChange);
	}
}


void SpinButton::ConfigureFromTemplate(const DialogControl &control)
{
	m_isWrapping = (control.style & kSpinWrap) != 0;
	m_useBuddyText = (control.style & kSpinSetBuddyInt) != 0;
}


int SpinButton::SetPos(int position)
{
	const int previous = m_position;
	m_position = std::clamp(position, std::min(m_minimum, m_maximum), std::max(m_minimum, m_maximum));
	if(m_buddy && m_useBuddyText)
	{
		m_buddy->SetWindowText(mpt::transcode<mpt::ustring>(mpt::common_encoding::utf8, std::to_string(m_position)));
		NotifyParent(*m_buddy, EditUpdate);
		NotifyParent(*m_buddy, EditChange);
	}
	redraw();
	return previous;
}


void SpinButton::Step(int direction)
{
	int newPosition = m_position + direction * m_increment;
	const int low = std::min(m_minimum, m_maximum);
	const int high = std::max(m_minimum, m_maximum);
	if(newPosition > high)
		newPosition = m_isWrapping ? low : high;
	else if(newPosition < low)
		newPosition = m_isWrapping ? high : low;
	if(newPosition == m_position)
		return;
	// Tell the parent about the pending change; it may veto it by returning a non-zero result
	NotifyHeader header;
	header.from = this;
	header.id = GetDlgCtrlID();
	header.code = SpinDeltaPos;
	SpinDelta delta{m_position, direction * m_increment};
	header.extra = &delta;
	LResult result = 0;
	if(Wnd *parent = GetParent())
		parent->RouteCommand(header.id, SpinDeltaPos, &header, &result);
	if(result != 0)
		return;
	SetPos(newPosition);
	if(Wnd *parent = GetParent())
		parent->CallVScroll(ScrollThumbPosition, static_cast<uint32>(m_position), this);
}


void SpinButton::draw()
{
	const int halfHeight = h() / 2;
	const bool isUpPressed = (m_pressedDirection > 0);
	const bool isDownPressed = (m_pressedDirection < 0);
	draw_box(isUpPressed ? FL_DOWN_BOX : FL_UP_BOX, x(), y(), w(), halfHeight, FL_BACKGROUND_COLOR);
	draw_box(isDownPressed ? FL_DOWN_BOX : FL_UP_BOX, x(), y() + halfHeight, w(), h() - halfHeight, FL_BACKGROUND_COLOR);
	fl_color(active_r() ? FL_FOREGROUND_COLOR : FL_INACTIVE_COLOR);
	const int cx = x() + w() / 2;
	const int arrow = std::max(2, std::min(w() / 4, halfHeight / 2));
	fl_polygon(cx - arrow, y() + halfHeight / 2 + arrow / 2, cx + arrow, y() + halfHeight / 2 + arrow / 2, cx, y() + halfHeight / 2 - arrow / 2);
	const int lowerCenter = y() + halfHeight + (h() - halfHeight) / 2;
	fl_polygon(cx - arrow, lowerCenter - arrow / 2, cx + arrow, lowerCenter - arrow / 2, cx, lowerCenter + arrow / 2);
}


void SpinButton::OnLButtonDown(uint32, Point point)
{
	m_pressedDirection = (point.y < h() / 2) ? 1 : -1;
	Step(m_pressedDirection);
	SetTimer(1, 400);
	redraw();
}


void SpinButton::OnLButtonUp(uint32, Point)
{
	m_pressedDirection = 0;
	KillTimer(1);
	redraw();
}


void SpinButton::OnTimer(uintptr_t timerId)
{
	if(timerId == 1 && m_pressedDirection != 0)
	{
		Step(m_pressedDirection);
		SetTimer(1, 60);
	}
}


ProgressBar::ProgressBar(int x, int y, int width, int height, const char *label)
    : WndT<Fl_Progress>(x, y, width, height, label)
{
	minimum(0.0f);
	maximum(100.0f);
}


void ProgressBar::SetRange(int minimum, int maximum)
{
	this->minimum(static_cast<float>(minimum));
	this->maximum(static_cast<float>(maximum));
}


void ProgressBar::SetPos(int position)
{
	value(static_cast<float>(position));
}


ListBox::ListBox(int x, int y, int width, int height, const char *label)
    : WndT<Fl_Browser>(x, y, width, height, label)
{
	type(FL_HOLD_BROWSER);
	callback(BrowserCallback, this);
}


void ListBox::ConfigureFromTemplate(const DialogControl &control)
{
	type((control.style & 0x808) != 0 ? FL_MULTI_BROWSER : FL_HOLD_BROWSER);
}


int ListBox::AddString(const mpt::ustring &text)
{
	add(ToFl(text));
	return size() - 1;
}


int ListBox::InsertString(int index, const mpt::ustring &text)
{
	if(index < 0 || index >= size())
		return AddString(text);
	insert(index + 1, ToFl(text));
	return index;
}


int ListBox::DeleteString(int index)
{
	if(index < 0 || index >= size())
		return -1;
	remove(index + 1);
	return size();
}


void ListBox::ResetContent()
{
	clear();
}


int ListBox::GetCurSel() const
{
	return value() - 1;
}


int ListBox::SetCurSel(int index)
{
	if(index < 0 || index >= size())
	{
		deselect();
		return -1;
	}
	value(index + 1);
	return index;
}


void ListBox::SetItemData(int index, uintptr_t data)
{
	if(index >= 0 && index < size())
		this->data(index + 1, reinterpret_cast<void *>(data));
}


uintptr_t ListBox::GetItemData(int index) const
{
	if(index >= 0 && index < size())
		return reinterpret_cast<uintptr_t>(this->data(index + 1));
	return 0;
}


mpt::ustring ListBox::GetText(int index) const
{
	if(index >= 0 && index < size())
		return FromUtf8(text(index + 1));
	return {};
}


int ListBox::FindStringExact(int startAfter, const mpt::ustring &textToFind) const
{
	const std::string utf8 = ToUtf8(textToFind);
	const int count = size();
	for(int i = 1; i <= count; ++i)
	{
		const int index = (startAfter + i) % count;
		if(utf8 == text(index + 1))
			return index;
	}
	return -1;
}


void ListBox::SetCheck(int index, int state)
{
	// Items of a checked list carry a marker in front of the text
	if(index < 0 || index >= size())
		return;
	std::string label = text(index + 1);
	const bool isMarked = label.rfind("\xE2\x98\x91 ", 0) == 0 || label.rfind("\xE2\x98\x90 ", 0) == 0;
	if(isMarked)
		label.erase(0, 4);
	label.insert(0, state ? "\xE2\x98\x91 " : "\xE2\x98\x90 ");
	this->text(index + 1, label.c_str());
}


int ListBox::GetCheck(int index) const
{
	if(index < 0 || index >= size())
		return 0;
	return std::string(text(index + 1)).rfind("\xE2\x98\x91 ", 0) == 0 ? 1 : 0;
}


void ListBox::BrowserCallback(Fl_Widget *widget, void *)
{
	if(ListBox *list = dynamic_cast<ListBox *>(widget))
	{
		if(Fl::event_clicks() > 0)
			NotifyParent(*list, ListDblClk);
		else
			NotifyParent(*list, ListSelChange);
	}
}



int ListBox::SetSel(int index, bool isSelected)
{
	if(index < 0)
	{
		for(int i = 1; i <= size(); ++i)
			select(i, isSelected ? 1 : 0);
		return 0;
	}
	if(index >= size())
		return -1;
	select(index + 1, isSelected ? 1 : 0);
	return 0;
}


int ListBox::GetSelCount() const
{
	int count = 0;
	for(int i = 1; i <= size(); ++i)
	{
		if(selected(i))
			++count;
	}
	return count;
}


int ListBox::GetSelItems(int maximumCount, int *items) const
{
	int count = 0;
	for(int i = 1; i <= size() && count < maximumCount; ++i)
	{
		if(selected(i))
			items[count++] = i - 1;
	}
	return count;
}


void ListBox::SelItemRange(bool isSelected, int first, int last)
{
	for(int i = std::max(first, 0); i <= last && i < size(); ++i)
		select(i + 1, isSelected ? 1 : 0);
}


bool ListBox::GetItemRect(int index, Rect &rect) const
{
	if(index < 0 || index >= size())
		return false;
	const int itemHeight = textsize() + 4;
	rect = Rect(0, 0, w(), itemHeight);
	rect.OffsetRect(0, (index - GetTopIndex()) * itemHeight);
	return true;
}


int Edit::LineIndex(int line) const
{
	const std::string text = value();
	int currentLine = 0;
	for(size_t i = 0; i < text.size(); ++i)
	{
		if(currentLine == line)
			return static_cast<int>(i);
		if(text[i] == '\n')
			++currentLine;
	}
	return currentLine == line ? static_cast<int>(text.size()) : -1;
}


int Edit::LineFromChar(int index) const
{
	const std::string text = value();
	if(index < 0)
		index = position();
	int line = 0;
	for(int i = 0; i < index && i < static_cast<int>(text.size()); ++i)
	{
		if(text[i] == '\n')
			++line;
	}
	return line;
}


}  // namespace ui


OPENMPT_NAMESPACE_END
