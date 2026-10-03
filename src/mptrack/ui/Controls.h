/*
 * Controls.h
 * ----------
 * Purpose: Standard input and display controls: buttons, edit fields, combo boxes, sliders, spin buttons, scroll bars, static text and list boxes.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "Notify.h"
#include "ResourceTables.h"
#include "Wnd.h"

#include <FL/Fl_Box.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Input_Choice.H>
#include <FL/Fl_Progress.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Hor_Nice_Slider.H>
#include <FL/Fl_Nice_Slider.H>
#include <FL/Fl_Spinner.H>

#include <functional>
#include <string>
#include <vector>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{

// States of check boxes and radio buttons
enum CheckState : int
{
	CheckOff = 0,
	CheckOn = 1,
	CheckMixed = 2,
};

// Tells the parent window of a control about something that happened to it
void NotifyParent(Wnd &control, uint32 code);


class ImageList;


class Button : public WndT<Fl_Button>
{
public:
	enum class Kind : uint8
	{
		Push,
		DefaultPush,
		Check,
		Radio,
	};

	Button(int x = 0, int y = 0, int width = 0, int height = 0, const char *label = nullptr);

	void ConfigureFromTemplate(const DialogControl &control) override;

	void SetKind(Kind kind);
	Kind GetKind() const noexcept { return m_kind; }
	void SetRadioGroup(int group) noexcept { m_radioGroup = group; }
	void SetFlat(bool isFlat) noexcept { m_isFlat = isFlat; }
	void SetBitmap(const Bitmap *bitmap);
	void SetCheck(int state);
	int GetCheck() const { return value() ? 1 : 0; }

	void draw() override;
	int handle(int event) override;

private:
	Kind m_kind = Kind::Push;
	int m_radioGroup = 0;
	bool m_isFlat = false;
};


class Static : public WndT<Fl_Box>
{
public:
	Static(int x = 0, int y = 0, int width = 0, int height = 0, const char *label = nullptr);

	void ConfigureFromTemplate(const DialogControl &control) override;

	void SetAlignment(Fl_Align alignment) { align(alignment | FL_ALIGN_INSIDE); }
	void SetCentered(bool isCentered);
	void SetFrame(Fl_Boxtype frame) { box(frame); }
	void SetBitmap(const Bitmap *bitmap);
	void SetIconImage(Fl_Image *image) { this->image(image); }

	void draw() override;

private:
	bool m_isGroupBox = false;
};


class Edit : public WndT<Fl_Input>
{
public:
	Edit(int x = 0, int y = 0, int width = 0, int height = 0, const char *label = nullptr);

	void ConfigureFromTemplate(const DialogControl &control) override;

	void SetNumberOnly(bool isNumberOnly);
	void SetMultiline(bool isMultiline);
	bool IsMultiline() const { return type() == FL_MULTILINE_INPUT; }
	void SetReadOnly(bool isReadOnly) { readonly(isReadOnly ? 1 : 0); }
	void SetLimitText(int maximumLength) { maximum_size(maximumLength); }
	void LimitText(int maximumLength) { SetLimitText(maximumLength); }
	void SetSel(int start, int end);
	void SelectAll() { SetSel(0, -1); }
	void GetSel(int &start, int &end) const;
	void ReplaceSel(const mpt::ustring &text);
	void SetUpperCase(bool isUpperCase) noexcept { m_isUpperCase = isUpperCase; }
	int GetLineCount() const;
	void SetModify(bool isModified = true) noexcept { m_isModified = isModified; }
	bool GetModify() const noexcept { return m_isModified; }
	int GetWindowTextLength() const { return size(); }
	// Deletes the selected text
	void Clear() { ReplaceSel({}); }
	void SetMargins(int, int) { }
	int LineIndex(int line) const;
	int LineFromChar(int index) const;
	void SetWindowText(const mpt::ustring &text) override;
	mpt::ustring GetWindowText() const override;
	using Wnd::GetWindowText;

	int handle(int event) override;

protected:
	void OnSetFocus(Wnd *oldWindow) override;
	void OnKillFocus(Wnd *newWindow) override;

private:
	bool m_isUpperCase = false;
	bool m_isModified = false;
};


class ComboBox : public WndT<Fl_Group>
{
public:
	ComboBox(int x = 0, int y = 0, int width = 0, int height = 0, const char *label = nullptr);

	void ConfigureFromTemplate(const DialogControl &control) override;
	~ComboBox() override;

	// If editable, the user may type text that is not in the list
	void SetEditable(bool isEditable);
	bool IsEditable() const noexcept { return m_isEditable; }

	// New strings are inserted in case-insensitive order
	void SetSorted(bool isSorted) noexcept { m_isSorted = isSorted; }
	int AddString(const mpt::ustring &text);
	int InsertString(int index, const mpt::ustring &text);
	int DeleteString(int index);
	void ResetContent();
	int GetCount() const { return static_cast<int>(m_items.size()); }
	int GetCurSel() const;
	int SetCurSel(int index);
	void SetItemData(int index, uintptr_t data);
	uintptr_t GetItemData(int index) const;
	void SetItemDataPtr(int index, void *data) { SetItemData(index, reinterpret_cast<uintptr_t>(data)); }
	void *GetItemDataPtr(int index) const { return reinterpret_cast<void *>(GetItemData(index)); }
	void InitStorage(int, int) { }
	mpt::ustring GetLBText(int index) const;
	void GetLBText(int index, mpt::ustring &text) const { text = GetLBText(index); }
	int GetLBTextLen(int index) const;
	int FindStringExact(int startAfter, const mpt::ustring &text) const;
	int FindString(int startAfter, const mpt::ustring &text) const { return FindStringExact(startAfter, text); }
	int SelectString(int startAfter, const mpt::ustring &text);
	// Images are kept for the owner; the drop-down list shows only the texts
	int InsertItem(int index, const mpt::ustring &text, int image, uintptr_t data);
	void SetImageList(ImageList *imageList) noexcept { m_imageList = imageList; }
	void SetItemHeight(int, int) { }
	void SetDroppedWidth(int) { }
	void ShowDropDown(bool = true) { }
	void SetEditSel(int start, int end);
	void SetEditLimitText(int maximumLength);
	void SetEditNumberOnly(bool isNumberOnly);
	void SetCueBanner(const mpt::ustring &) { }
	void SetWindowText(const mpt::ustring &text) override;
	mpt::ustring GetWindowText() const override;
	using Wnd::GetWindowText;

	void resize(int x, int y, int width, int height) override;

private:
	struct Item
	{
		std::string text;
		uintptr_t data = 0;
	};

	void CreateInner();
	void RebuildMenu();
	void OnInnerChanged(uint32 code);

	static void InnerCallback(Fl_Widget *widget, void *data);

	std::vector<Item> m_items;
	ImageList *m_imageList = nullptr;
	std::vector<Fl_Menu_Item> m_menu;
	Fl_Choice *m_choice = nullptr;
	Fl_Input_Choice *m_inputChoice = nullptr;
	int m_currentSelection = -1;
	bool m_isEditable = false;
	bool m_isSorted = false;
};


template <typename FlSlider>
class SliderT : public WndT<FlSlider>
{
public:
	SliderT(int x, int y, int width, int height, const char *label);

	void SetRange(int minimum, int maximum, bool = false);
	void SetRangeMin(int minimum);
	void SetRangeMax(int maximum);
	int GetRangeMin() const { return m_minimum; }
	int GetRangeMax() const { return m_maximum; }
	void SetPos(int position);
	int GetPos() const;
	void SetTicFreq(int) { }
	void SetLineSize(int size) { m_lineSize = size; }
	void SetPageSize(int size) { m_pageSize = size; }

	void draw() override;

private:
	static void SliderCallback(Fl_Widget *widget, void *data);

	int m_minimum = 0;
	int m_maximum = 100;
	int m_lineSize = 1;
	int m_pageSize = 10;
};


class HSlider : public SliderT<Fl_Hor_Nice_Slider>
{
public:
	HSlider(int x = 0, int y = 0, int width = 0, int height = 0, const char *label = nullptr);
};


class VSlider : public SliderT<Fl_Nice_Slider>
{
public:
	VSlider(int x = 0, int y = 0, int width = 0, int height = 0, const char *label = nullptr);
};


// Parameters of a scroll bar
enum ScrollInfoMask : uint32
{
	SIF_RANGE = 0x01,
	SIF_PAGE = 0x02,
	SIF_POS = 0x04,
	SIF_DISABLENOSCROLL = 0x08,
	SIF_TRACKPOS = 0x10,
	SIF_ALL = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS,
};

struct ScrollInfo
{
	uint32 cbSize = sizeof(ScrollInfo);
	uint32 fMask = SIF_ALL;
	int nMin = 0;
	int nMax = 0;
	uint32 nPage = 0;
	int nPos = 0;
	int nTrackPos = 0;
};
using SCROLLINFO = ScrollInfo;


class ScrollBar : public WndT<Fl_Scrollbar>
{
public:
	ScrollBar(int x = 0, int y = 0, int width = 0, int height = 0, const char *label = nullptr);

	void ConfigureFromTemplate(const DialogControl &control) override;

	void SetScrollRange(int minimum, int maximum, bool = true);
	void SetScrollInfo(int minimum, int maximum, int pageSize, int position);
	void SetScrollPos(int position);
	int GetScrollPos() const { return value(); }
	void GetScrollRange(int *minimum, int *maximum) const { *minimum = m_minimum; *maximum = m_maximum; }
	bool GetScrollInfo(ScrollInfo *info, uint32 = SIF_ALL) const;
	bool SetScrollInfo(const ScrollInfo *info, bool = true);
	int GetScrollLimit() const { return m_maximum - m_pageSize; }
	void SetVertical(bool isVertical);

private:
	static void ScrollCallback(Fl_Widget *widget, void *data);

	int m_minimum = 0;
	int m_maximum = 100;
	int m_pageSize = 1;
};


// Acceleration of a spin button: after nSec seconds the value changes by nInc
struct UDACCEL
{
	uint32 nSec = 0;
	uint32 nInc = 1;
};


// Numeric edit field with up/down buttons. Notifies its parent like an edit field (EditChange etc.);
// button and arrow key steps first send SpinDeltaPos, and a non-zero result cancels the step.
class Spinner : public WndT<Fl_Spinner>
{
public:
	Spinner(int x = 0, int y = 0, int width = 0, int height = 0, const char *label = nullptr);

	void ConfigureFromTemplate(const DialogControl &control) override;
	// Applies the style of an up-down control template (wrapping)
	void ConfigureSpinFromTemplate(const DialogControl &control);

	// A range with minimum > maximum makes the up button decrease the value
	void SetRange(int minimum, int maximum);
	void SetRange32(int minimum, int maximum) { SetRange(minimum, maximum); }
	void GetRange(int &minimum, int &maximum) const;
	void GetRange32(int &minimum, int &maximum) const { GetRange(minimum, maximum); }
	void SetPos(int position) { SetValue(position); }
	void SetPos32(int position) { SetValue(position); }
	int GetPos() const;
	int GetPos32() const { return GetPos(); }
	void SetValue(double newValue);
	// The value currently shown, which may still be being edited
	double GetValue() const;
	void SetDecimalPlaces(int digitCount);
	// Fractional values are shown with as many digits as needed
	void SetAllowFractions(bool allowFractions);
	// printf format of the shown value, which may add text around the number
	void SetDisplayFormat(const std::string &printfFormat);
	void SetIncrement(double increment) { step(increment); }
	void SetWrap(bool isWrapping) { wrap(isWrapping ? 1 : 0); }

	void SetReadOnly(bool isReadOnly) { input_.readonly(isReadOnly ? 1 : 0); }
	void SetLimitText(int maximumLength) { input_.maximum_size(maximumLength); }
	void LimitText(int maximumLength) { SetLimitText(maximumLength); }
	void SetSel(int start, int end);
	void SelectAll() { SetSel(0, -1); }
	void SetWindowText(const mpt::ustring &text) override;
	mpt::ustring GetWindowText() const override;
	using Wnd::GetWindowText;

private:
	static void InputCallback(Fl_Widget *widget, void *data);
	static void ButtonCallback(Fl_Widget *widget, void *data);
	void Step(int direction);
	void CommitInput();

	std::string m_format = "%g";
	bool m_isReversed = false;
};


// Up/down arrows without an edit field, for stepping through items of another control
class SpinButton : public WndT<Fl_Widget>
{
public:
	SpinButton(int x = 0, int y = 0, int width = 0, int height = 0, const char *label = nullptr);

	void ConfigureFromTemplate(const DialogControl &control) override;

	void SetRange(int minimum, int maximum) { m_minimum = minimum; m_maximum = maximum; }
	void SetRange32(int minimum, int maximum) { SetRange(minimum, maximum); }
	void GetRange(int &minimum, int &maximum) const { minimum = m_minimum; maximum = m_maximum; }
	int SetPos(int position);
	int GetPos() const { return m_position; }
	int SetPos32(int position) { return SetPos(position); }
	int GetPos32() const { return m_position; }
	void GetRange32(int &minimum, int &maximum) const { GetRange(minimum, maximum); }
	void SetAccel(int, const UDACCEL *) { }
	void SetBuddy(Wnd *buddy) { m_buddy = buddy; }
	Wnd *GetBuddy() const noexcept { return m_buddy; }
	// Like UDS_SETBUDDYINT: write the position into the buddy's text
	void SetUseBuddyText(bool useBuddyText) noexcept { m_useBuddyText = useBuddyText; }
	void SetWrap(bool isWrapping) noexcept { m_isWrapping = isWrapping; }
	void SetIncrement(int increment) noexcept { m_increment = increment; }

	void draw() override;

protected:
	void OnLButtonDown(uint32 flags, Point point) override;
	void OnLButtonUp(uint32 flags, Point point) override;
	void OnTimer(uintptr_t timerId) override;

private:
	void Step(int direction);

	Wnd *m_buddy = nullptr;
	int m_minimum = 0;
	int m_maximum = 100;
	int m_position = 0;
	int m_increment = 1;
	int m_pressedDirection = 0;
	bool m_isWrapping = false;
	bool m_useBuddyText = true;
};


// A panel whose content is drawn by a callback of its owner
class OwnerDrawPanel : public Panel
{
public:
	// Draws into the client area; the rectangle is the client rectangle
	std::function<void(Painter &, const Rect &)> onDraw;

protected:
	void OnPaint(Painter &painter) override
	{
		if(onDraw)
			onDraw(painter, GetClientRect());
	}
};


class ProgressBar : public WndT<Fl_Progress>
{
public:
	ProgressBar(int x = 0, int y = 0, int width = 0, int height = 0, const char *label = nullptr);

	void SetRange(int minimum, int maximum);
	void SetPos(int position);
	int GetPos() const { return static_cast<int>(value()); }
};


class ListBox : public WndT<Fl_Browser>
{
public:
	ListBox(int x = 0, int y = 0, int width = 0, int height = 0, const char *label = nullptr);

	void ConfigureFromTemplate(const DialogControl &control) override;

	int AddString(const mpt::ustring &text);
	int InsertString(int index, const mpt::ustring &text);
	int DeleteString(int index);
	void ResetContent();
	int GetCount() const { return size(); }
	int GetCurSel() const;
	int SetCurSel(int index);
	void SetItemData(int index, uintptr_t data);
	uintptr_t GetItemData(int index) const;
	void SetItemDataPtr(int index, void *data) { SetItemData(index, reinterpret_cast<uintptr_t>(data)); }
	void *GetItemDataPtr(int index) const { return reinterpret_cast<void *>(GetItemData(index)); }
	// Multiple selection
	int SetSel(int index, bool isSelected = true);
	int GetSel(int index) const { return selected(index + 1) ? 1 : 0; }
	int GetSelCount() const;
	int GetSelItems(int maximumCount, int *items) const;
	void SelItemRange(bool isSelected, int first, int last);
	void SetItemHeight(int, int) { }
	bool GetItemRect(int index, Rect &rect) const;
	mpt::ustring GetText(int index) const;
	int FindStringExact(int startAfter, const mpt::ustring &text) const;
	void SetCheck(int index, int state);
	int GetCheck(int index) const;
	void SetTopIndex(int index) { topline(index + 1); }
	int GetTopIndex() const { return topline() - 1; }

private:
	static void BrowserCallback(Fl_Widget *widget, void *data);
};


}  // namespace ui


OPENMPT_NAMESPACE_END
