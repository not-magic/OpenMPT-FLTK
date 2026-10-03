// MFC replacement on FLTK. Menus and popup menus, built from menu templates or by code.

#include "stdafx.h"
#include "Menu.h"
#include "Wnd.h"

#include <FL/Fl.H>
#include <FL/Fl_Menu_Item.H>

#include <algorithm>


OPENMPT_NAMESPACE_BEGIN


namespace ui
{


std::function<void(uint32)> menuCommandHandler;


namespace
{

mpt::ustring FromUtf8(const char *text)
{
	return mpt::transcode<mpt::ustring>(mpt::common_encoding::utf8, std::string(text));
}

std::string ToUtf8(const mpt::ustring &text)
{
	return mpt::transcode<std::string>(mpt::common_encoding::utf8, text);
}

void LoadEntries(Menu &menu, const MenuEntry *entries, std::size_t count)
{
	for(std::size_t i = 0; i < count; ++i)
	{
		const MenuEntry &entry = entries[i];
		Menu::Item item;
		if(entry.label == nullptr)
		{
			item.flags = MenuItemSeparator;
		} else if(entry.children != nullptr)
		{
			item.text = FromUtf8(entry.label);
			item.flags = MenuItemPopup;
			item.subMenu = std::make_shared<Menu>();
			LoadEntries(*item.subMenu, entry.children, entry.childCount);
		} else
		{
			item.text = FromUtf8(entry.label);
			item.id = entry.id;
			if(entry.flags & MenuGrayed)
				item.flags |= MenuItemGrayed;
			if(entry.flags & MenuChecked)
				item.flags |= MenuItemChecked;
		}
		menu.GetItems().push_back(std::move(item));
	}
}

// Converts "Ctrl+Shift+F5" into an FLTK shortcut, or 0 if there is none
int ParseShortcut(const std::string &text)
{
	int shortcut = 0;
	std::size_t start = 0;
	while(start < text.size())
	{
		std::size_t end = text.find('+', start);
		if(end == std::string::npos || end + 1 >= text.size())
			end = std::string::npos;
		const std::string part = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
		if(part == "Ctrl")
			shortcut |= FL_CTRL;
		else if(part == "Shift")
			shortcut |= FL_SHIFT;
		else if(part == "Alt")
			shortcut |= FL_ALT;
		else if(part.size() == 1)
			shortcut |= static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(part[0])));
		else if(part.size() >= 2 && part[0] == 'F' && std::isdigit(static_cast<unsigned char>(part[1])))
			shortcut |= FL_F + std::atoi(part.c_str() + 1);
		else if(part == "Esc")
			shortcut |= FL_Escape;
		else if(part == "Del")
			shortcut |= FL_Delete;
		else if(part == "Ins")
			shortcut |= FL_Insert;
		else if(part == "Space")
			shortcut |= ' ';
		else
			return 0;
		if(end == std::string::npos)
			break;
		start = end + 1;
	}
	return shortcut;
}

void MenuItemCallback(Fl_Widget *, void *data)
{
	const uint32 id = static_cast<uint32>(reinterpret_cast<uintptr_t>(data));
	if(menuCommandHandler)
		menuCommandHandler(id);
}

}  // namespace


bool Menu::LoadMenu(uint32 resourceId)
{
	const MenuTemplate *menuTemplate = FindMenuTemplate(resourceId);
	if(menuTemplate == nullptr)
		return false;
	m_items.clear();
	LoadEntries(*this, menuTemplate->entries, menuTemplate->entryCount);
	return true;
}


bool Menu::AppendMenu(uint32 flags, uint32 id, const mpt::ustring &text)
{
	Item item;
	item.text = text;
	item.id = id;
	item.flags = flags;
	m_items.push_back(std::move(item));
	return true;
}


bool Menu::AppendMenu(uint32 flags, const Menu &subMenu, const mpt::ustring &text)
{
	Item item;
	item.text = text;
	item.flags = flags | MenuItemPopup;
	item.subMenu = std::make_shared<Menu>(subMenu);
	m_items.push_back(std::move(item));
	return true;
}


uint32 Menu::TrackPopupMenu(uint32 flags, int x, int y, Wnd *owner)
{
	const uint32 id = TrackPopupMenu(Point(x, y), owner);
	if(!(flags & TrackReturnCommand) && id != 0 && owner != nullptr)
		owner->RouteCommand(id, 0, nullptr);
	return id;
}


bool AppendMenu(HMENU menu, uint32 flags, uintptr_t idOrSubMenu, const mpt::ustring &text)
{
	if(flags & MenuItemPopup)
	{
		HMENU subMenu = reinterpret_cast<HMENU>(idOrSubMenu);
		const bool result = menu->AppendMenu(flags, *subMenu, text);
		delete subMenu;
		return result;
	}
	return menu->AppendMenu(flags, static_cast<uint32>(idOrSubMenu), text);
}


uint32 TrackPopupMenu(HMENU menu, uint32 flags, int x, int y, int, Wnd *owner, const void *)
{
	return menu->TrackPopupMenu(flags, x, y, owner);
}


bool Menu::InsertMenu(uint32 position, uint32 flags, uint32 id, const mpt::ustring &text)
{
	Item item;
	item.text = text;
	item.id = id;
	item.flags = flags;
	if(position >= m_items.size())
		m_items.push_back(std::move(item));
	else
		m_items.insert(m_items.begin() + position, std::move(item));
	return true;
}


bool Menu::InsertMenu(uint32 position, uint32 flags, const Menu &subMenu, const mpt::ustring &text)
{
	Item item;
	item.text = text;
	item.flags = flags | MenuItemPopup;
	item.subMenu = std::make_shared<Menu>(subMenu);
	if(position >= m_items.size())
		m_items.push_back(std::move(item));
	else
		m_items.insert(m_items.begin() + position, std::move(item));
	return true;
}


bool Menu::RemoveMenu(uint32 position, bool isByPosition)
{
	if(isByPosition)
	{
		if(position >= m_items.size())
			return false;
		m_items.erase(m_items.begin() + position);
		return true;
	}
	for(auto it = m_items.begin(); it != m_items.end(); ++it)
	{
		if(it->id == position)
		{
			m_items.erase(it);
			return true;
		}
	}
	return false;
}


uint32 Menu::GetMenuItemID(uint32 position) const
{
	if(position >= m_items.size())
		return 0;
	return m_items[position].id;
}


bool Menu::GetMenuString(uint32 id, mpt::ustring &text, bool isByPosition) const
{
	if(isByPosition)
	{
		if(id >= m_items.size())
			return false;
		text = m_items[id].text;
		return true;
	}
	if(const Item *item = FindItem(id))
	{
		text = item->text;
		return true;
	}
	return false;
}


bool Menu::ModifyMenu(uint32 id, uint32 flags, uint32 newId, const mpt::ustring &text)
{
	if(Item *item = FindItem(id))
	{
		item->flags = (item->flags & MenuItemPopup) | flags;
		item->id = newId;
		item->text = text;
		return true;
	}
	return false;
}


uint32 Menu::EnableMenuItem(uint32 id, bool isEnabled)
{
	if(Item *item = FindItem(id))
	{
		const uint32 previous = item->flags;
		if(isEnabled)
			item->flags &= ~static_cast<uint32>(MenuItemGrayed);
		else
			item->flags |= MenuItemGrayed;
		return previous;
	}
	return static_cast<uint32>(-1);
}


uint32 Menu::CheckMenuItem(uint32 id, bool isChecked)
{
	if(Item *item = FindItem(id))
	{
		const uint32 previous = item->flags;
		if(isChecked)
			item->flags |= MenuItemChecked;
		else
			item->flags &= ~static_cast<uint32>(MenuItemChecked);
		return previous;
	}
	return static_cast<uint32>(-1);
}


bool Menu::SetMenuItemText(uint32 id, const mpt::ustring &text)
{
	if(Item *item = FindItem(id))
	{
		item->text = text;
		return true;
	}
	return false;
}


Menu *Menu::GetSubMenu(uint32 position)
{
	if(position >= m_items.size())
		return nullptr;
	return m_items[position].subMenu.get();
}


const Menu *Menu::GetSubMenu(uint32 position) const
{
	if(position >= m_items.size())
		return nullptr;
	return m_items[position].subMenu.get();
}


Menu::Item *Menu::FindItem(uint32 id)
{
	for(Item &item : m_items)
	{
		if(item.subMenu)
		{
			if(Item *found = item.subMenu->FindItem(id))
				return found;
		} else if(!(item.flags & MenuItemSeparator) && item.id == id)
		{
			return &item;
		}
	}
	return nullptr;
}


const Menu::Item *Menu::FindItem(uint32 id) const
{
	return const_cast<Menu *>(this)->FindItem(id);
}


void Menu::BuildFltkItems(std::vector<Fl_Menu_Item> &items, std::vector<std::unique_ptr<std::string>> &labels) const
{
	for(const Item &item : m_items)
	{
		Fl_Menu_Item entry{};
		if(item.flags & MenuItemSeparator)
		{
			// A separator is attached to the previous entry as a divider
			if(!items.empty())
				items.back().flags |= FL_MENU_DIVIDER;
			continue;
		}
		std::string label = ToUtf8(item.text);
		int shortcut = 0;
		const std::size_t tab = label.find('\t');
		if(tab != std::string::npos)
		{
			shortcut = ParseShortcut(label.substr(tab + 1));
			label.erase(tab);
		}
		labels.push_back(std::make_unique<std::string>(std::move(label)));
		entry.text = labels.back()->c_str();
		entry.shortcut_ = shortcut;
		if(item.subMenu)
		{
			entry.flags = FL_SUBMENU;
			items.push_back(entry);
			item.subMenu->BuildFltkItems(items, labels);
			items.push_back(Fl_Menu_Item{});
		} else
		{
			entry.callback_ = MenuItemCallback;
			entry.user_data_ = reinterpret_cast<void *>(static_cast<uintptr_t>(item.id));
			if(item.flags & MenuItemGrayed)
				entry.flags |= FL_MENU_INACTIVE;
			if(item.flags & MenuItemChecked)
				entry.flags |= FL_MENU_TOGGLE | FL_MENU_VALUE;
			items.push_back(entry);
		}
	}
}


uint32 Menu::TrackPopupMenu(Point screenPosition, Wnd *owner)
{
	std::vector<Fl_Menu_Item> items;
	std::vector<std::unique_ptr<std::string>> labels;
	BuildFltkItems(items, labels);
	if(items.empty())
		return 0;
	items.push_back(Fl_Menu_Item{});
	Fl_Window *window = owner ? owner->GetWidget()->window() : nullptr;
	int x = screenPosition.x;
	int y = screenPosition.y;
	if(window)
	{
		x -= window->x_root();
		y -= window->y_root();
	}
	const Fl_Menu_Item *selected = items.data()->popup(x, y, nullptr, nullptr, nullptr);
	if(selected == nullptr)
		return 0;
	return static_cast<uint32>(reinterpret_cast<uintptr_t>(selected->user_data_));
}


namespace
{

class MenuCmdUI final : public CmdUI
{
public:
	explicit MenuCmdUI(Menu::Item &item) : m_item(item) { id = item.id; }

	void Enable(bool isEnabled) override
	{
		if(isEnabled)
			m_item.flags &= ~static_cast<uint32>(MenuItemGrayed);
		else
			m_item.flags |= MenuItemGrayed;
	}
	void SetCheck(int state) override
	{
		if(state)
			m_item.flags |= MenuItemChecked;
		else
			m_item.flags &= ~static_cast<uint32>(MenuItemChecked);
	}
	void SetRadio(bool isOn) override { SetCheck(isOn ? 1 : 0); }
	void SetText(const mpt::ustring &text) override { m_item.text = text; }

private:
	Menu::Item &m_item;
};

constexpr uint32 updateUiCode = MsgUser + 0x7F10;

}  // namespace


void UpdateMenuItems(Menu &menu, Wnd &target)
{
	for(Menu::Item &item : menu.GetItems())
	{
		if(item.subMenu)
		{
			UpdateMenuItems(*item.subMenu, target);
		} else if(!(item.flags & MenuItemSeparator) && item.id != 0)
		{
			MenuCmdUI cmdUi(item);
			LResult result = 0;
			target.RouteCommand(item.id, updateUiCode, &cmdUi, &result);
		}
	}
}


MenuBar::MenuBar(int x, int y, int width, int height, Wnd &target)
    : Fl_Menu_Bar(x, y, width, height)
    , m_target(target)
{
	callback(
	    [](Fl_Widget *widget, void *)
	    {
		    MenuBar *bar = static_cast<MenuBar *>(widget);
		    if(const Fl_Menu_Item *item = bar->mvalue())
			    bar->m_target.RouteCommand(static_cast<uint32>(reinterpret_cast<uintptr_t>(item->user_data_)), 0, nullptr);
	    });
}


void MenuBar::Rebuild()
{
	m_items.clear();
	m_labels.clear();
	if(m_menu)
	{
		UpdateMenuItems(*m_menu, m_target);
		m_menu->BuildFltkItems(m_items, m_labels);
		for(Fl_Menu_Item &item : m_items)
			item.callback_ = nullptr;
	}
	m_items.push_back(Fl_Menu_Item{});
	copy(m_items.data());
}


int MenuBar::handle(int event)
{
	if(event == FL_PUSH || event == FL_SHORTCUT)
	{
		if(onBeforeOpen)
			onBeforeOpen();
		Rebuild();
	}
	return Fl_Menu_Bar::handle(event);
}


}  // namespace ui


OPENMPT_NAMESPACE_END
