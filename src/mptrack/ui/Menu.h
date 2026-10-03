/*
 * Menu.h
 * ------
 * Purpose: Menus and popup menus, built from menu templates or by code.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../Geometry.h"
#include "ResourceTables.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Menu_Item.H>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{

class Wnd;

// Receives the command ID of every menu item that gets selected
extern std::function<void(uint32)> menuCommandHandler;


enum MenuItemFlags : uint32
{
	MenuItemString = 0,
	MenuItemGrayed = 1,
	MenuItemChecked = 2,
	MenuItemSeparator = 4,
	MenuItemPopup = 8,
};


// A tree of menu items. Items are identified by command ID.
class Menu
{
public:
	struct Item
	{
		mpt::ustring text;
		uint32 id = 0;
		uint32 flags = 0;
		std::shared_ptr<Menu> subMenu;
	};

	Menu() = default;

	bool LoadMenu(uint32 resourceId);
	void CreatePopupMenu() { m_items.clear(); }
	bool IsValid() const noexcept { return !m_items.empty(); }

	// Appends an item or, with MenuItemPopup, a sub menu with the items of another menu
	bool AppendMenu(uint32 flags, uint32 id = 0, const mpt::ustring &text = {});
	bool AppendMenu(uint32 flags, const Menu &subMenu, const mpt::ustring &text);
	bool InsertMenu(uint32 position, uint32 flags, uint32 id, const mpt::ustring &text);
	bool InsertMenu(uint32 position, uint32 flags, const Menu &subMenu, const mpt::ustring &text);
	bool RemoveMenu(uint32 position, bool isByPosition = true);
	bool DeleteMenu(uint32 position, bool isByPosition = true) { return RemoveMenu(position, isByPosition); }
	void RemoveAll() { m_items.clear(); }

	uint32 GetMenuItemCount() const { return static_cast<uint32>(m_items.size()); }
	uint32 GetMenuItemID(uint32 position) const;
	bool GetMenuString(uint32 id, mpt::ustring &text, bool isByPosition = false) const;
	bool ModifyMenu(uint32 id, uint32 flags, uint32 newId, const mpt::ustring &text);
	uint32 EnableMenuItem(uint32 id, bool isEnabled);
	uint32 CheckMenuItem(uint32 id, bool isChecked);
	bool SetMenuItemText(uint32 id, const mpt::ustring &text);
	Menu *GetSubMenu(uint32 position);
	const Menu *GetSubMenu(uint32 position) const;
	// Searches this menu and all sub menus
	Item *FindItem(uint32 id);
	const Item *FindItem(uint32 id) const;

	const std::vector<Item> &GetItems() const noexcept { return m_items; }
	std::vector<Item> &GetItems() noexcept { return m_items; }

	// Shows the menu at a screen position and returns the selected command ID, or 0 if nothing was selected
	uint32 TrackPopupMenu(Point screenPosition, Wnd *owner = nullptr);
	// Shows the menu and either returns the selected ID (TrackReturnCommand) or sends it as a command to the owner
	uint32 TrackPopupMenu(uint32 flags, int x, int y, Wnd *owner);
	void DestroyMenu() { m_items.clear(); }

	// Translates the tree into FLTK menu items. The label storage must outlive the result.
	void BuildFltkItems(std::vector<Fl_Menu_Item> &items, std::vector<std::unique_ptr<std::string>> &labels) const;

private:
	std::vector<Item> m_items;
};


// Flags of TrackPopupMenu
enum TrackPopupFlags : uint32
{
	TrackLeftAlign = 0,
	TrackRightButton = 0x0002,
	TrackReturnCommand = 0x0100,
};
constexpr uint32 TPM_LEFTALIGN = TrackLeftAlign;
constexpr uint32 TPM_RIGHTBUTTON = TrackRightButton;
constexpr uint32 TPM_RETURNCMD = TrackReturnCommand;


// Functions that keep the shape of the Windows menu functions; menus are owned by whoever created them
using HMENU = Menu *;
inline HMENU CreatePopupMenu() { return new Menu; }
inline bool DestroyMenu(HMENU menu) { delete menu; return true; }
// With MenuItemPopup, idOrSubMenu is the HMENU of a menu that is moved into the new item
bool AppendMenu(HMENU menu, uint32 flags, uintptr_t idOrSubMenu = 0, const mpt::ustring &text = {});
// Menus have no default item
inline void SetMenuDefaultItem(HMENU, uint32, bool) { }
inline HMENU GetSubMenu(HMENU menu, int position) { return menu->GetSubMenu(static_cast<uint32>(position)); }
inline uint32 EnableMenuItem(HMENU menu, uint32 id, uint32 flags) { return menu->EnableMenuItem(id, (flags & MenuItemGrayed) == 0); }
inline uint32 CheckMenuItem(HMENU menu, uint32 id, uint32 flags) { return menu->CheckMenuItem(id, (flags & MenuItemChecked) != 0); }
uint32 TrackPopupMenu(HMENU menu, uint32 flags, int x, int y, int reserved, Wnd *owner, const void *rect);


// Updates the enabled/checked state of all items by asking the target through its command-state handlers
void UpdateMenuItems(Menu &menu, Wnd &target);


// A menu bar showing a Menu; selected commands are routed to the target window
class MenuBar : public Fl_Menu_Bar
{
public:
	MenuBar(int x, int y, int width, int height, Wnd &target);

	void SetMenu(Menu *menu) { m_menu = menu; Rebuild(); }
	Menu *GetMenu() const noexcept { return m_menu; }
	void Rebuild();
	// Called before a menu is opened so that dynamic items can be refreshed
	std::function<void()> onBeforeOpen;

	int handle(int event) override;

private:
	Wnd &m_target;
	Menu *m_menu = nullptr;
	std::vector<Fl_Menu_Item> m_items;
	std::vector<std::unique_ptr<std::string>> m_labels;
};


}  // namespace ui


OPENMPT_NAMESPACE_END
