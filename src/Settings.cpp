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
		};

		constexpr IniDefault kDefaults[] = {
				{ "Debug", "sLogLevel", "info" },
				{ "Debug", "bLogMatches", "0" },
				{ "Debug", "bLogTiming", "0" },
				{ "Menus", "bInventory", "1" },
				{ "Menus", "bContainer", "1" },
				{ "Menus", "bBarter", "1" },
				{ "Menus", "bGift", "1" },
				{ "Menus", "bMagic", "1" },
				{ "Menus", "bFavorites", "1" },
				{ "Menus", "bCrafting", "1" },
				{ "Categories", "bItemMenuTabs", "1" },
				{ "Categories", "bCraftingTabs", "1" },
				{ "StatusIcons", "bEnabled", "1" },
				{ "StatusIcons", "bMuseum", "1" },
				{ "StatusIcons", "bQuestItem", "1" },
				{ "StatusIcons", "bEnchantment", "1" },
				{ "StatusIcons", "bNewItem", "1" },
				{ "StatusIcons", "bUpgrade", "1" },
				{ "StatusIcons", "bShowIngredientProgress", "1" },
				{ "StatusIcons", "bShowEffectIcons", "1" },
				{ "StatusIcons", "iMaxEffectIcons", "2" },
				{ "StatusIcons", "fStatusIconSize", "0.7" },
				{ "Icons", "bSharpSVG", "1" },
				{ "TrueHUD", "bRecentLootIcons", "1" },
				{ "QuickLoot", "bLootMenuIcons", "1" },
				{ "MapMarkers", "bMapMarkerIcons", "1" },
				{ "Colors", "bUseCustomColors", "1" },
				{ "Colors", "sNewItem", "FFFFFF" },
				{ "Colors", "sBestInClass", "FFFFFF" },
				{ "Colors", "sQuestItem", "FFFFFF" },
				{ "Colors", "sFavorite", "FFFFFF" },
				{ "Colors", "sHarmfulEffect", "CC3333" },
		};

		void EnsureIni(const char* a_path)
		{
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
			// 0.1.0.5 key.
			if (const char* old = ini.GetValue("StatusIcons", "sHarmfulEffectColor")) {
				if (!ini.GetValue("Colors", "sHarmfulEffect")) {
					ini.SetValue("Colors", "sHarmfulEffect", old);
				}
				ini.Delete("StatusIcons", "sHarmfulEffectColor");
				++added;
			}
			for (const auto& d : kDefaults) {
				if (!ini.GetValue(d.section, d.key)) {
					ini.SetValue(d.section, d.key, d.value);
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

		std::uint32_t ReadTint(const char* a_key)
		{
			const auto parse = [](std::string_view a_text, std::uint32_t& a_out) {
				if (a_text.starts_with('#')) {
					a_text.remove_prefix(1);
				} else if (a_text.starts_with("0x") || a_text.starts_with("0X")) {
					a_text.remove_prefix(2);
				}
				const auto parsed = std::from_chars(a_text.data(), a_text.data() + a_text.size(), a_out, 16);
				return parsed.ec == std::errc{} && parsed.ptr == a_text.data() + a_text.size() && a_out <= 0xFFFFFF;
			};
			std::uint32_t color = 0xFFFFFF;
			parse(Default("Colors", a_key), color);
			char buf[16]{};
			REX::W32::GetPrivateProfileStringA("Colors", a_key, "", buf, sizeof(buf), kIni);
			if (std::uint32_t value = 0; parse(buf, value)) {
				color = value;
			}
			return color == 0xFFFFFF ? 0 : color == 0 ? 0x010101 : color;
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
			const bool customColors = readOn("Colors", "bUseCustomColors");
			const auto tint = [&](const char* a_key) { return customColors ? ReadTint(a_key) : 0u; };
			const std::uint32_t harmTint = tint("sHarmfulEffect");
			IconRules::StatusColors colors;
			colors.newItem = tint("sNewItem");
			colors.bestInClass = tint("sBestInClass");
			colors.questItem = tint("sQuestItem");
			colors.favorite = tint("sFavorite");
			IconRules::SetStatusColors(colors);
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
			logger::info("colors: custom={} new={:06X} best={:06X} quest={:06X} favorite={:06X} (0 = art colors)", customColors,
				colors.newItem, colors.bestInClass, colors.questItem, colors.favorite);
			logger::info("debug: logMatches={} logTiming={}, ingredient progress={} effects={} iMaxEffectIcons={} harmTint={:06X} statusIconSize={} recentLootIcons={} quickLootIcons={} mapMarkerIcons={}",
				logMatches, logTiming, showProgress, showEffects, maxEffectIcons, harmTint,
				std::clamp(statusSize, StatusIcons::kMinSize, StatusIcons::kMaxSize), lootIcons, quickLootIcons, mapMarkerIcons);
		}
	}

	const char* Default(const char* a_section, const char* a_key)
	{
		for (const auto& d : kDefaults) {
			if (std::string_view(d.section) == a_section && std::string_view(d.key) == a_key) {
				return d.value;
			}
		}
		return "";
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
