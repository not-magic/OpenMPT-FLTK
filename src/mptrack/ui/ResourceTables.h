// MFC replacement on FLTK. Static dialog, menu, string and binary resources, generated from mptrack.rc by
// src/tools/rc2cpp.py.

#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../Geometry.h"

#include <cstddef>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{

enum class ResourceType : uint8
{
	Png,
	Bitmap,
	Data,
};

struct BinaryResource
{
	uint32 id;
	ResourceType type;
	const uint8 *data;
	std::size_t size;
};

struct StringResource
{
	uint32 id;
	const char *text;
};

enum class ControlKind : uint8
{
	Control,
	Ltext,
	Ctext,
	Rtext,
	Edittext,
	Combobox,
	Listbox,
	Scrollbar,
	Pushbutton,
	DefPushButton,
	Groupbox,
	Icon,
	Autocheckbox,
	Autoradiobutton,
	Checkbox,
	Radiobutton,
	State3,
};

struct DialogControl
{
	ControlKind kind;
	int32 id;
	const char *text;
	const char *windowClass;  // only for ControlKind::Control
	int x, y, width, height;  // dialog units
	uint32 style;
	uint32 clearedStyle;      // styles removed with NOT
	uint32 exStyle;
};

struct DialogTemplate
{
	uint32 id;
	int x, y, width, height;  // dialog units
	const char *caption;
	uint32 style;
	uint32 exStyle;
	int fontSize;
	const DialogControl *controls;
	std::size_t controlCount;
	// Per control in template order: move x, move y, size x, size y in percent of the window growth. Null if the dialog is not resizable.
	const int16 *layout;
	std::size_t layoutCount;
};

enum MenuEntryFlags : uint8
{
	MenuGrayed = 1,
	MenuChecked = 2,
	MenuInactive = 4,
};

struct MenuEntry
{
	const char *label;  // nullptr for separators
	uint32 id;
	uint8 flags;
	const MenuEntry *children;
	std::size_t childCount;
};

struct MenuTemplate
{
	uint32 id;
	const MenuEntry *entries;
	std::size_t entryCount;
};

extern const BinaryResource binaryResources[];
extern const std::size_t binaryResourceCount;
extern const StringResource stringResources[];
extern const std::size_t stringResourceCount;
extern const DialogTemplate dialogTemplates[];
extern const std::size_t dialogTemplateCount;
extern const MenuTemplate menuTemplates[];
extern const std::size_t menuTemplateCount;

// Looks up resources by ID; returns nullptr if there is none
const BinaryResource *FindBinaryResource(uint32 id);
const char *FindStringResource(uint32 id);
const DialogTemplate *FindDialogTemplate(uint32 id);
const MenuTemplate *FindMenuTemplate(uint32 id);

}  // namespace ui


OPENMPT_NAMESPACE_END
