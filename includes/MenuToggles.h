#pragma once

// INI [Menus] and [Categories]: a menu that is off gets no row, tab or status icon install, so it keeps its own art.
namespace MenuToggles
{
	inline std::vector<std::string_view> g_off;
	inline bool g_itemTabs = true;
	inline bool g_craftingTabs = true;

	inline void SetMenu(std::string_view a_menu, bool a_enabled)
	{
		std::erase(g_off, a_menu);
		if (!a_enabled) {
			g_off.push_back(a_menu);
		}
	}

	inline bool Menu(std::string_view a_menu)
	{
		return std::ranges::find(g_off, a_menu) == g_off.end();
	}

	inline bool Tabs(std::string_view a_menu)
	{
		return Menu(a_menu) && (a_menu == RE::CraftingMenu::MENU_NAME ? g_craftingTabs : g_itemTabs);
	}
}
