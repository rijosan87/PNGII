#include "ASBridge.h"
#include "CategoryIcons.h"
#include "LootIcons.h"
#include "MapMarkerIcons.h"
#include "MenuToggles.h"
#include "MMF/Interfaces.h"
#include "QuickLootIcons.h"
#include "RowIcons.h"
#include "IconAPI.h"
#include "IconLabels.h"
#include "IconRules.h"
#include "NewItems.h"
#include "StatusIcons.h"
#include "TCCItemStatus.h"
#include "TextureManager.h"
#include <REX/W32/KERNEL32.h>
#include <SimpleIni.h>

#include <ranges>

namespace
{
	void InitLog()
	{
		auto path = SKSE::log::log_directory();
		if (!path) {
			return;
		}
		*path /= "PNGIconInjector.log";

		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
		auto log = std::make_shared<spdlog::logger>("global log", std::move(sink));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::debug);
		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%H:%M:%S] [%l] %v");
	}

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
				"; 0 = no status icons after item names at all (every statusIcon rule is ignored)." },
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
			{ "RuleFiles", "sDisabled", "",
				"; Rule files to skip, comma-separated, from SKSE/Plugins/PNGIconInjector/. Example: 05_Spells.json, 13_Diseases-Vanilla" },
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
		// No UTF-8 BOM: GetPrivateProfileStringA would read it as part of the first section name.
		if (const SI_Error rc = ini.SaveFile(a_path, false); rc < SI_OK) {
			logger::error("Failed to write PNGIconInjector.ini (SimpleIni error {})", static_cast<int>(rc));
		} else {
			logger::info("PNGIconInjector.ini {}: {} default key(s) added", existed ? "updated" : "created", added);
		}
	}

	void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
	{
		if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
			TCCItemStatus::Init();
			IconRules::Load();
			// Label reads run behind the resolver gate, so KID keywords are in place by then.
			IconLabels::Init();
			IconAPI::SetLabeler(IconLabels::ForFormID);
			RowIcons::Install();
			CategoryIcons::Install();
			LootIcons::Install();
			QuickLootIcons::Install();
			MapMarkerIcons::Install();
			StatusIcons::Install();
			NewItems::Install();
		} else if (a_msg->type == SKSE::MessagingInterface::kPreLoadGame) {
			IconAPI::SetResolver(nullptr);
		} else if (a_msg->type == SKSE::MessagingInterface::kPostLoadGame ||
		           a_msg->type == SKSE::MessagingInterface::kNewGame) {
			IconAPI::SetResolver(nullptr);
			IconRules::ClearCache();
			// After every kDataLoaded distributor so KID/SkyPatcher keywords exist; keeps the one-time cost off the first inventory frame.
			IconRules::PrepareKeywords();
			IconAPI::SetResolver(IconRules::Find);
			RowIcons::PrewarmIcons();
			StatusIcons::PrewarmIcons();
		}
	}

	// SKSE only delivers to the first null-sender listener per plugin.
	void ForeignMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		// Never call GetVanillaMarkerType: null function pointer.
		MMF::MapMarkerInterface* coMAP = nullptr;
		MMF::QueryMapMarkerInterface(a_msg, coMAP);
		if (coMAP) {
			IconRules::SetCoMAP(coMAP);
			logger::info("CoMAP MapMarkerInterface acquired (version {})", coMAP->Version());
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	InitLog();
	char buf[16]{};
	constexpr auto kIni = ".\\Data\\SKSE\\Plugins\\PNGIconInjector.ini";
	EnsureIni(kIni);
	char levelBuf[16]{};
	REX::W32::GetPrivateProfileStringA("Debug", "sLogLevel", "info", levelBuf, sizeof(levelBuf), kIni);
	if (std::string_view(levelBuf) == "debug") {
		spdlog::set_level(spdlog::level::debug);
	} else if (std::string_view(levelBuf) == "off") {
		spdlog::set_level(spdlog::level::off);
	}
	REX::W32::GetPrivateProfileStringA("Debug", "bLogMatches", "0", buf, sizeof(buf), kIni);
	const bool logMatches = std::string_view(buf) == "1";
	REX::W32::GetPrivateProfileStringA("Debug", "bLogTiming", "0", buf, sizeof(buf), kIni);
	const bool logTiming = std::string_view(buf) == "1";
	IconRules::SetDebug(logMatches, logTiming);
	REX::W32::GetPrivateProfileStringA("StatusIcons", "bShowIngredientProgress", "1", buf, sizeof(buf), kIni);
	const bool showProgress = std::string_view(buf) != "0";
	REX::W32::GetPrivateProfileStringA("StatusIcons", "bShowEffectIcons", "1", buf, sizeof(buf), kIni);
	const bool showEffects = std::string_view(buf) != "0";
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
	REX::W32::GetPrivateProfileStringA("Icons", "bSharpSVG", "1", buf, sizeof(buf), kIni);
	const bool sharpSVG = std::string_view(buf) != "0";
	TextureManager::SetSharpSVG(sharpSVG);
	REX::W32::GetPrivateProfileStringA("TrueHUD", "bRecentLootIcons", "1", buf, sizeof(buf), kIni);
	const bool lootIcons = std::string_view(buf) != "0";
	LootIcons::SetEnabled(lootIcons);
	REX::W32::GetPrivateProfileStringA("QuickLoot", "bLootMenuIcons", "1", buf, sizeof(buf), kIni);
	const bool quickLootIcons = std::string_view(buf) != "0";
	QuickLootIcons::SetEnabled(quickLootIcons);
	REX::W32::GetPrivateProfileStringA("MapMarkers", "bMapMarkerIcons", "1", buf, sizeof(buf), kIni);
	const bool mapMarkerIcons = std::string_view(buf) != "0";
	MapMarkerIcons::SetEnabled(mapMarkerIcons);
	const auto readOn = [&](const char* a_section, const char* a_key) {
		REX::W32::GetPrivateProfileStringA(a_section, a_key, "1", buf, sizeof(buf), kIni);
		return std::string_view(buf) != "0";
	};
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
	IconRules::LoadFilter filter;
	filter.statusIcons = readOn("StatusIcons", "bEnabled");
	filter.museum = readOn("StatusIcons", "bMuseum");
	filter.questItem = readOn("StatusIcons", "bQuestItem");
	filter.enchantment = readOn("StatusIcons", "bEnchantment");
	filter.newItem = readOn("StatusIcons", "bNewItem");
	filter.upgrade = readOn("StatusIcons", "bUpgrade");
	// Names are compared lowercase with .json; "05_Spells" and "05_spells.JSON" both match 05_Spells.json.
	char filesBuf[1024]{};
	REX::W32::GetPrivateProfileStringA("RuleFiles", "sDisabled", "", filesBuf, sizeof(filesBuf), kIni);
	for (const auto part : std::string_view(filesBuf) | std::views::split(',')) {
		std::string name(std::string_view(part.begin(), part.end()));
		std::erase_if(name, [](unsigned char c) { return std::isspace(c); });
		std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (name.empty()) {
			continue;
		}
		if (!name.ends_with(".json")) {
			name += ".json";
		}
		filter.offFiles.push_back(std::move(name));
	}
	logger::info("settings: menus off=[{}] itemTabs={} craftingTabs={} status={} museum={} quest={} enchantment={} new={} upgrade={} rule files off={}",
		menusOff, MenuToggles::g_itemTabs, MenuToggles::g_craftingTabs, filter.statusIcons, filter.museum, filter.questItem,
		filter.enchantment, filter.newItem, filter.upgrade, filter.offFiles.size());
	IconRules::SetLoadFilter(std::move(filter));
	RowIcons::SetTiming(logTiming);
	StatusIcons::SetTiming(logTiming);
	logger::info("debug: logMatches={} logTiming={}, ingredient progress={} effects={} iMaxEffectIcons={} harmTint={:06X} statusIconSize={} recentLootIcons={} quickLootIcons={} mapMarkerIcons={} sharpSVG={}", logMatches, logTiming,
		showProgress, showEffects, maxEffectIcons, harmTint, std::clamp(statusSize, StatusIcons::kMinSize, StatusIcons::kMaxSize), lootIcons, quickLootIcons, mapMarkerIcons, sharpSVG);
	// NG initializes its own logger inside Init unless told not to; it would replace ours.
	SKSE::Init(a_skse, { .log = false });
	// SKSE only accepts the co-save unique ID during plugin load.
	NewItems::RegisterSerialization();
	// SKSE builds skse.plugins per movie at creation; register before the main menu movie.
	ASBridge::Register();

	logger::info("PNG Icon Injector v0.1.0, runtime {}", a_skse->RuntimeVersion().string());

	SKSE::GetMessagingInterface()->RegisterListener(MessageHandler);
	SKSE::GetMessagingInterface()->RegisterListener(nullptr, ForeignMessage);
	return true;
}
