/*
 * ControlFactories.cpp
 * --------------------
 * Purpose: Registers the controls that dialog templates refer to by window class name.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "Controls.h"
#include "Dialog.h"
#include "ListCtrl.h"
#include "TabCtrl.h"
#include "ToolBar.h"
#include "TreeCtrl.h"


OPENMPT_NAMESPACE_BEGIN


namespace ui
{


namespace
{

constexpr uint32 kSliderVertical = 2;

template <typename T>
ControlFactory MakeFactory()
{
	return [](const DialogControl &, int x, int y, int width, int height) -> Wnd *
	{
		return new T(x, y, width, height);
	};
}

Wnd *CreateSlider(const DialogControl &control, int x, int y, int width, int height)
{
	if(control.style & kSliderVertical)
		return new VSlider(x, y, width, height);
	return new HSlider(x, y, width, height);
}

const bool areControlsRegistered = []()
{
	RegisterControlClass("msctls_progress32", MakeFactory<ProgressBar>());
	RegisterControlClass("msctls_updown32", MakeFactory<SpinButton>());
	RegisterControlClass("msctls_trackbar32", CreateSlider);
	RegisterControlClass("SysListView32", MakeFactory<ListCtrl>());
	RegisterControlClass("ToolbarWindow32", MakeFactory<ToolBar>());
	RegisterControlClass("SysTreeView32", MakeFactory<TreeCtrl>());
	RegisterControlClass("SysTabControl32", MakeFactory<TabCtrl>());
	RegisterControlClass("ComboBoxEx32", MakeFactory<ComboBox>());
	RegisterControlClass("SysLink", MakeFactory<Static>());
	return true;
}();

}  // namespace


}  // namespace ui


OPENMPT_NAMESPACE_END
