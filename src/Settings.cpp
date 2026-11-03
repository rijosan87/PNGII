#include "Settings.h"
#include "IconRules.h"
#include "LootIcons.h"
#include "MapMarkerIcons.h"
#include "MenuToggles.h"
#include "QuickLootIcons.h"
#include "RowIcons.h"
#include "StatusIcons.h"
#include "TextureManager.h"
#include <REX/W32/KERNEL32.h>
#include <SimpleIni.h>

namespace Settings
{
	namespace
	{
		constexpr auto kIni = ".\\Data\\SKSE\\Plugins\\PNGIconInjector.ini";

		struct IniDefault
		{
			const char* section;
			const char* key;
			const char* value;
			const char* comment;
		};

		void EnsureIni(const char* a_path)
		{
			constexpr IniDefault kDefaults[] = {
				{ "Debug", "sLogLevel", "info",
					"; Log level: debug, info or off. Debug adds one line per icon and other diagnostics." },
				{ "Debug", "bLogMatches", "0",
					"; 1 = log which rule each item matched, once per item. For rule authors." },
				{ "Debug", "bLogTiming", "0",
					"; 1 = log menu and rule timing when a menu closes." },
				{ "Menus", "bInventory", "1",
					"; 0 = leave a menu untouched: no item, category tab or status icons there. Rules still apply elsewhere." },
				{ "Menus", "bContainer", "1", nullptr },
				{ "Menus", "bBarter", "1", nullptr },
				{ "Menus", "bGift", "1", nullptr },
				{ "Menus", "bMagic", "1", "; Spells, powers and shouts list." },
				{ "Menus", "bFavorites", "1", nullptr },
				{ "Menus", "bCrafting", "1", "; Forge, smelter, tanning rack, enchanting and alchemy." },
				{ "Categories", "bItemMenuTabs", "1",
					"; 1 = PNG category tab icons in Inventory, Container, Barter, Gift and Magic, 0 = SkyUI's own tab icons." },
				{ "Categories", "bCraftingTabs", "1",
					"; 1 = PNG category icons in crafting menus (tabs and the alchemy column), 0 = SkyUI's own." },
				{ "StatusIcons", "bEnabled", "1",
					"; 0 = no status icons after item names at all." },
				{ "StatusIcons", "bMuseum", "1",
					"; Per source, 0 = ignore those statusIcon rules. Museum = Legacy of the Dragonborn New/Found/Displayed." },
				{ "StatusIcons", "bQuestItem", "1", nullptr },
				{ "StatusIcons", "bEnchantment", "1", "; Known, unknown and cannot-disenchant enchantment icons." },
				{ "StatusIcons", "bNewItem", "1", "; Items picked up since the menu was last opened." },
				{ "StatusIcons", "bUpgrade", "1", "; Armor and weapons better than what is equipped." },
				{ "StatusIcons", "bShowIngredientProgress", "1",
					"; 1 = show the learned-effects progress icon on ingredients, 0 = hide it." },
				{ "StatusIcons", "bShowEffectIcons", "1",
					"; 1 = show discovered-effect icons on ingredients, 0 = hide them." },
				{ "StatusIcons", "iMaxEffectIcons", "2",
					"; Effect icons per ingredient, 1 to 4." },
				{ "StatusIcons", "sHarmfulEffectColor", "CC3333",
					"; Tint for harmful effect icons, hex RRGGBB. FFFFFF = no tint." },
				{ "StatusIcons", "fStatusIconSize", "0.7",
					"; Status icon height as a fraction of the row height, 0.3 to 0.9. 0.5 = DIII size, 0.7 = about 24 px at 1080p." },
				{ "Icons", "bSharpSVG", "1",
					"; 1 = SVG icons drawn crisp like PNGs (2x raster, then downscaled), 0 = rasterized at the exact size, softer." },
				{ "TrueHUD", "bRecentLootIcons", "1",
					"; 1 = show rule icons on TrueHUD Recent Loot cards, 0 = keep TrueHUD's icons." },
				{ "QuickLoot", "bLootMenuIcons", "1",
					"; 1 = show rule icons in QuickLoot's loot window, 0 = keep QuickLoot's icons." },
				{ "MapMarkers", "bMapMarkerIcons", "1",
					"; 1 = show rule icons on world and local map markers, 0 = keep the map's own icons." },
			};

			CSimpleIniA ini;
			ini.SetUnicode();
			const bool existed = std::filesystem::exists(a_path);
			if (existed) {
				if (const SI_Error rc = ini.LoadFile(a_path); rc < SI_OK) {
					logger::error("PNGIconInjector.ini unreadable (SimpleIni error {}); not rewriting it", static_cast<int>(rc));
					return;
				}
			}

			std::size_t added = 0;
			for (const auto& d : kDefaults) {
				if (!ini.GetValue(d.section, d.key)) {
					ini.SetValue(d.section, d.key, d.value, d.comment);
					++added;
				}
			}
			if (existed && added == 0) {
				return;
			}

			std::error_code ec;
			std::filesystem::create_directories(std::filesystem::path(a_path).parent_path(), ec);
			if (ec) {
				logger::error("Failed to create the PNGIconInjector.ini directory: {}", ec.message());
				return;
			}
			// No BOM: GetPrivateProfileStringA would read it into the first section name.
			if (const SI_Error rc = ini.SaveFile(a_path, false); rc < SI_OK) {
				logger::error("Failed to write PNGIconInjector.ini (SimpleIni error {})", static_cast<int>(rc));
			} else {
				logger::info("PNGIconInjector.ini {}: {} default key(s) added", existed ? "updated" : "created", added);
			}
		}

		void Apply(bool a_startup)
		{
			char buf[16]{};
			const auto readOn = [&](const char* a_section, const char* a_key) {
				REX::W32::GetPrivateProfileStringA(a_section, a_key, "1", buf, sizeof(buf), kIni);
				return std::string_view(buf) != "0";
			};

			REX::W32::GetPrivateProfileStringA("Debug", "sLogLevel", "info", buf, sizeof(buf), kIni);
			const std::string_view level(buf);
			spdlog::set_level(level == "debug" ? spdlog::level::debug : level == "off" ? spdlog::level::off : spdlog::level::info);
			REX::W32::GetPrivateProfileStringA("Debug", "bLogMatches", "0", buf, sizeof(buf), kIni);
			const bool logMatches = std::string_view(buf) == "1";
			REX::W32::GetPrivateProfileStringA("Debug", "bLogTiming", "0", buf, sizeof(buf), kIni);
			const bool logTiming = std::string_view(buf) == "1";
			IconRules::SetDebug(logMatches, logTiming);
			RowIcons::SetTiming(logTiming);
			StatusIcons::SetTiming(logTiming);

			const bool showProgress = readOn("StatusIcons", "bShowIngredientProgress");
			const bool showEffects = readOn("StatusIcons", "bShowEffectIcons");
			REX::W32::GetPrivateProfileStringA("StatusIcons", "iMaxEffectIcons", "", buf, sizeof(buf), kIni);
			std::uint32_t maxEffectIcons = IconRules::kDefaultMaxEffectIcons;
			std::from_chars(buf, buf + std::string_view(buf).size(), maxEffectIcons);
			maxEffectIcons = (std::min)(maxEffectIcons, IconRules::kMaxEffectIcons);
			REX::W32::GetPrivateProfileStringA("StatusIcons", "sHarmfulEffectColor", "CC3333", buf, sizeof(buf), kIni);
			std::string_view colorText(buf);
			if (colorText.starts_with('#')) {
				colorText.remove_prefix(1);
			} else if (colorText.starts_with("0x") || colorText.starts_with("0X")) {
				colorText.remove_prefix(2);
			}
			std::uint32_t harmColor = 0xCC3333;
			if (const auto parsed = std::from_chars(colorText.data(), colorText.data() + colorText.size(), harmColor, 16);
				parsed.ec != std::errc{} || parsed.ptr != colorText.data() + colorText.size() || harmColor > 0xFFFFFF) {
				harmColor = 0xCC3333;
			}
			const std::uint32_t harmTint = harmColor == 0xFFFFFF ? 0 : harmColor;
			IconRules::SetIngredientIcons(showProgress, showEffects ? maxEffectIcons : 0, harmTint);
			REX::W32::GetPrivateProfileStringA("StatusIcons", "fStatusIconSize", "0.7", buf, sizeof(buf), kIni);
			double statusSize = 0.7;
			std::from_chars(buf, buf + std::string_view(buf).size(), statusSize);
			StatusIcons::SetSize(statusSize);

			IconRules::StatusSources sources;
			sources.enabled = readOn("StatusIcons", "bEnabled");
			sources.museum = readOn("StatusIcons", "bMuseum");
			sources.questItem = readOn("StatusIcons", "bQuestItem");
			sources.enchantment = readOn("StatusIcons", "bEnchantment");
			sources.newItem = readOn("StatusIcons", "bNewItem");
			sources.upgrade = readOn("StatusIcons", "bUpgrade");
			IconRules::SetStatusSources(sources);

			std::string menusOff;
			for (const auto& [key, menu] : { std::pair{ "bInventory", RE::InventoryMenu::MENU_NAME }, std::pair{ "bContainer", RE::ContainerMenu::MENU_NAME },
				     std::pair{ "bBarter", RE::BarterMenu::MENU_NAME }, std::pair{ "bGift", RE::GiftMenu::MENU_NAME },
				     std::pair{ "bMagic", RE::MagicMenu::MENU_NAME }, std::pair{ "bFavorites", RE::FavoritesMenu::MENU_NAME },
				     std::pair{ "bCrafting", RE::CraftingMenu::MENU_NAME } }) {
				const bool on = readOn("Menus", key);
				MenuToggles::SetMenu(menu, on);
				if (!on) {
					menusOff += menusOff.empty() ? "" : ",";
					menusOff += menu;
				}
			}
			MenuToggles::g_itemTabs = readOn("Categories", "bItemMenuTabs");
			MenuToggles::g_craftingTabs = readOn("Categories", "bCraftingTabs");

			const bool lootIcons = readOn("TrueHUD", "bRecentLootIcons");
			const bool quickLootIcons = readOn("QuickLoot", "bLootMenuIcons");
			const bool mapMarkerIcons = readOn("MapMarkers", "bMapMarkerIcons");
			LootIcons::SetEnabled(lootIcons);
			QuickLootIcons::SetEnabled(quickLootIcons);
			MapMarkerIcons::SetEnabled(mapMarkerIcons);

			// Registered textures keep their raster.
			if (a_startup) {
				TextureManager::SetSharpSVG(readOn("Icons", "bSharpSVG"));
			}

			logger::info("settings{}: menus off=[{}] itemTabs={} craftingTabs={} status={} museum={} quest={} enchantment={} new={} upgrade={}",
				a_startup ? "" : " (reloaded)", menusOff, MenuToggles::g_itemTabs, MenuToggles::g_craftingTabs, sources.enabled,
				sources.museum, sources.questItem, sources.enchantment, sources.newItem, sources.upgrade);
			logger::info("debug: logMatches={} logTiming={}, ingredient progress={} effects={} iMaxEffectIcons={} harmTint={:06X} statusIconSize={} recentLootIcons={} quickLootIcons={} mapMarkerIcons={}",
				logMatches, logTiming, showProgress, showEffects, maxEffectIcons, harmTint,
				std::clamp(statusSize, StatusIcons::kMinSize, StatusIcons::kMaxSize), lootIcons, quickLootIcons, mapMarkerIcons);
		}
	}

	void Init()
	{
		EnsureIni(kIni);
		Apply(true);
	}

	void Reload()
	{
		SKSE::GetTaskInterface()->AddUITask([] { Apply(false); });
	}
}
