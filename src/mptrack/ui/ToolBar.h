// MFC replacement on FLTK. A bar of icon buttons with check and drop-down buttons, separators, and room for
// embedded controls.

#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "ImageList.h"
#include "Wnd.h"

#include <vector>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{

enum ToolStyle : uint32
{
	ToolStyleButton = 0x00,
	ToolStyleSeparator = 0x01,
	ToolStyleCheck = 0x02,
	ToolStyleDropDown = 0x08,
};

enum ToolState : uint32
{
	ToolStateChecked = 0x01,
	ToolStateEnabled = 0x04,
	ToolStateHidden = 0x08,
};

enum ToolExtendedStyle : uint32
{
	ToolExtendedDrawDropDownArrows = 0x01,
};


// Sent with the ToolbarDropDown notification
struct ToolbarDropDownInfo
{
	uint32 id = 0;
	// Button rectangle in client coordinates of the toolbar
	Rect rect;
};


class ToolBar : public WndT<Fl_Group>
{
public:
	ToolBar(int x = 0, int y = 0, int width = 0, int height = 0, const char *label = nullptr);

	void ConfigureFromTemplate(const DialogControl &control) override;

	void SetImageList(const ImageList *images) { m_images = images; redraw(); }
	void SetDisabledImageList(const ImageList *images) { m_disabledImages = images; redraw(); }
	void SetBitmapSize(Size size) noexcept { m_imageSize = size; }
	void SetButtonSize(Size size) noexcept { m_buttonSize = size; }
	// Space before the first button
	void SetIndent(int indent) noexcept { m_indent = indent; redraw(); }
	void SetFlat(bool isFlat) noexcept { m_isFlat = isFlat; redraw(); }
	uint32 GetExtendedStyle() const noexcept { return m_extendedStyle; }
	void SetExtendedStyle(uint32 style) noexcept { m_extendedStyle = style; }

	bool AddButton(uint32 id, int image = 0, uint32 style = ToolStyleButton, uint32 state = ToolStateEnabled);
	void EnableButton(uint32 id, bool isEnabled = true);
	void CheckButton(uint32 id, bool isChecked = true);
	bool IsButtonChecked(uint32 id) const;
	bool IsButtonEnabled(uint32 id) const;
	void HideButton(uint32 id, bool isHidden = true);
	bool IsButtonHidden(uint32 id) const;
	void SetButtonImage(uint32 id, int image);
	int GetButtonCount() const { return static_cast<int>(m_buttons.size()); }
	int CommandToIndex(uint32 id) const;
	// Rectangle of a button in client coordinates
	bool GetItemRect(int index, Rect &rect) const;
	// Total size needed to show all visible buttons in one row
	Size CalcFixedSize() const;
	// Changes the width of a separator, which can then host a control
	void SetSeparatorWidth(int index, int width);
	void SetButtonVisibility(int index, bool isVisible);
	// Turns a separator into a button or the other way round
	void SetButtonInfo(int index, uint32 id, uint32 style, int imageOrWidth);
	uint32 GetButtonStyle(int index) const;
	void SetButtonStyle(int index, uint32 style);
	uint32 GetButtonState(uint32 id) const;
	void SetButtonState(uint32 id, uint32 state);
	// Shows a control in place of the separator at the index, or hides it
	void UpdateControl(bool isShown, Wnd &control, int index, int id, int height = 0);

	void draw() override;

protected:
	// Text shown while the mouse rests on a button; empty for none
	virtual mpt::ustring GetButtonToolTip(uint32 id) const;

	struct Button
	{
		uint32 id = 0;
		int image = 0;
		uint32 style = ToolStyleButton;
		uint32 state = ToolStateEnabled;
		int width = 0;
	};

	void OnLButtonDown(uint32 flags, Point point) override;
	void OnLButtonUp(uint32 flags, Point point) override;
	void OnMouseMove(uint32 flags, Point point) override;
	void OnMouseLeave() override;

	int FindButtonAt(Point point) const;
	int ButtonWidth(const Button &button) const;

	std::vector<Button> m_buttons;
	const ImageList *m_images = nullptr;
	const ImageList *m_disabledImages = nullptr;
	Size m_imageSize{16, 16};
	Size m_buttonSize{26, 24};
	uint32 m_extendedStyle = 0;
	int m_indent = 0;
	int m_hotIndex = -1;
	int m_pressedIndex = -1;
	bool m_isFlat = false;
};

}  // namespace ui


OPENMPT_NAMESPACE_END
