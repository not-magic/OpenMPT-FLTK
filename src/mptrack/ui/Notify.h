// MFC replacement on FLTK. Notification codes sent by controls to their parents.

#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../Geometry.h"


OPENMPT_NAMESPACE_BEGIN


namespace ui
{

// Button clicks are delivered as plain commands (code 0)
enum NotifyCode : uint32
{
	ComboSelChange = 1,
	ComboEditChange,
	ComboEditUpdate,
	ComboSetFocus,
	ComboKillFocus,
	ComboDropDown,
	ComboSelEndOk,
	ComboSelEndCancel,
	EditChange,
	EditUpdate,
	EditSetFocus,
	EditKillFocus,
	ListSelChange,
	ListDblClk,
	CheckListChange,
	ListItemChanged,
	ListClick,
	ListDblClick,
	ListRClick,
	ListBeginLabelEdit,
	ListEndLabelEdit,
	TreeSelChanged,
	TreeBeginDrag,
	TreeBeginRDrag,
	TreeItemExpanded,
	TreeBeginLabelEdit,
	TreeEndLabelEdit,
	TreeDeleteItem,
	TreeClick,
	TreeDblClick,
	TreeRClick,
	TreeReturn,
	TabSelChange,
	SpinDeltaPos,
	ToolbarDropDown,
	TooltipNeedText,
	TooltipShow,
	TooltipPop,
	TooltipLinkClick,
	SliderChange,
	ScrollChange,
};

// Extra data of the label editing notifications of lists; text is null if the edit was cancelled
struct ListLabelEdit
{
	int item = -1;
	int column = 0;
	const mpt::ustring *text = nullptr;
};

// Extra data of the click notifications of lists
struct ListClickInfo
{
	int item = -1;
	int column = -1;
	// In client coordinates of the list
	Point point;
};

// Extra data of CheckListChange: a check box of a list item was toggled by the user
struct ListCheckInfo
{
	int item = -1;
	bool isChecked = false;
};

// Extra data of SpinDeltaPos: the spin button is about to change from position by delta
struct SpinDelta
{
	int position = 0;
	int delta = 0;
};

}  // namespace ui


OPENMPT_NAMESPACE_END
