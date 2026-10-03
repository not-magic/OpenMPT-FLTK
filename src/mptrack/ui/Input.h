// MFC replacement on FLTK. Keyboard and mouse input codes shared by the GUI code.

#pragma once

#include "openmpt/all/BuildSettings.hpp"


OPENMPT_NAMESPACE_BEGIN


namespace ui
{

// Physical key identifiers; the digits and letters use their ASCII upper-case value
enum VirtualKey : uint32
{
	KeyNone = 0,
	Key_CANCEL = 3,
	Key_BACK = 8,
	Key_TAB = 9,
	Key_CLEAR = 12,
	Key_RETURN = 13,
	Key_SHIFT = 16,
	Key_CONTROL = 17,
	Key_MENU = 18,
	Key_PAUSE = 19,
	Key_CAPITAL = 20,
	Key_ESCAPE = 27,
	Key_SPACE = 32,
	Key_PRIOR = 33,
	Key_NEXT = 34,
	Key_END = 35,
	Key_HOME = 36,
	Key_LEFT = 37,
	Key_UP = 38,
	Key_RIGHT = 39,
	Key_DOWN = 40,
	Key_SNAPSHOT = 44,
	Key_INSERT = 45,
	Key_DELETE = 46,
	Key_LWIN = 91,
	Key_RWIN = 92,
	Key_APPS = 93,
	Key_NUMPAD0 = 96,
	Key_NUMPAD1 = 97,
	Key_NUMPAD2 = 98,
	Key_NUMPAD3 = 99,
	Key_NUMPAD4 = 100,
	Key_NUMPAD5 = 101,
	Key_NUMPAD6 = 102,
	Key_NUMPAD7 = 103,
	Key_NUMPAD8 = 104,
	Key_NUMPAD9 = 105,
	Key_MULTIPLY = 106,
	Key_ADD = 107,
	Key_SEPARATOR = 108,
	Key_SUBTRACT = 109,
	Key_DECIMAL = 110,
	Key_DIVIDE = 111,
	Key_F1 = 112,
	Key_F2 = 113,
	Key_F3 = 114,
	Key_F4 = 115,
	Key_F5 = 116,
	Key_F6 = 117,
	Key_F7 = 118,
	Key_F8 = 119,
	Key_F9 = 120,
	Key_F10 = 121,
	Key_F11 = 122,
	Key_F12 = 123,
	Key_F13 = 124,
	Key_F14 = 125,
	Key_F15 = 126,
	Key_F16 = 127,
	Key_F17 = 128,
	Key_F18 = 129,
	Key_F19 = 130,
	Key_F20 = 131,
	Key_F21 = 132,
	Key_F22 = 133,
	Key_F23 = 134,
	Key_F24 = 135,
	Key_NUMLOCK = 144,
	Key_SCROLL = 145,
	Key_LSHIFT = 160,
	Key_RSHIFT = 161,
	Key_LCONTROL = 162,
	Key_RCONTROL = 163,
	Key_LMENU = 164,
	Key_RMENU = 165,
	Key_BROWSER_BACK = 166,
	Key_BROWSER_FORWARD = 167,
	Key_OEM_1 = 186,
	Key_OEM_PLUS = 187,
	Key_OEM_COMMA = 188,
	Key_OEM_MINUS = 189,
	Key_OEM_PERIOD = 190,
	Key_OEM_2 = 191,
	Key_OEM_3 = 192,
	Key_OEM_4 = 219,
	Key_OEM_5 = 220,
	Key_OEM_6 = 221,
	Key_OEM_7 = 222,
	Key_OEM_8 = 223,
	Key_OEM_102 = 226,
	Key_PACKET = 231,
};

// Flags that accompany key events
enum KeyFlags : uint32
{
	KeyFlagRepeat = 0x4000,   // key was already down
	KeyFlagRelease = 0x8000,  // key was released
};

// Key code of the current FLTK key event, or KeyNone
uint32 KeyFromEvent();

// Whether the key of the current key event is already held down (valid before the event has been dispatched)
bool IsKeyRepeat();
// The first character typed by the current key event, or 0
uint32 CharacterFromEvent();

// Returns whether a key is held down right now
bool IsKeyDown(uint32 key);

// Hardware key positions are assumed to be those of a US keyboard.
// Returns 0 if the key is not known.
uint32 MapScanCodeToVirtualKey(uint32 scanCode);
uint32 MapVirtualKeyToScanCode(uint32 virtualKey);
// Virtual key that types a letter or digit
uint32 FindVirtualKeyForCharacter(char character);
// Name of a key as shown in key bindings, e.g. "Enter" or "Num 5"
mpt::ustring GetKeyName(uint32 virtualKey, bool isExtended = false);

// Mouse and keyboard state flags that accompany mouse events
enum MouseFlags : uint32
{
	MouseLeft = 0x0001,
	MouseShift = 0x0004,
	MouseControl = 0x0008,
	MouseMiddle = 0x0010,
	MouseRight = 0x0002,
	MouseAlt = 0x0020,
};

// Modifier flags used for key bindings
enum KeyModifier : uint32
{
	KeyModShift = 0x01,
	KeyModControl = 0x02,
	KeyModAlt = 0x04,
	KeyModExtended = 0x08,
	KeyModMidi = 0x10,
	KeyModRightShift = 0x20,
	KeyModRightControl = 0x40,
	KeyModRightAlt = 0x80,
};

}  // namespace ui


OPENMPT_NAMESPACE_END
