#include "SettingsMenu.h"
#include "ColorPresets.h"
#include "IconRules.h"
#include "ItemTints.h"
#include "Localization.h"
#include "Settings.h"
#include "TCCItemStatus.h"

#include <SimpleIni.h>
#include <numeric>
#include <tuple>

#ifndef NOMINMAX
#	define NOMINMAX
#endif
#pragma warning(push)
#pragma warning(disable: 4099 5054)
#include "SKSEMenuFramework.h"
#pragma warning(pop)

namespace SettingsMenu
{
	namespace
	{
		constexpr auto kIni = ".\\Data\\SKSE\\Plugins\\PNGIconInjector.ini";

		constexpr auto kBanner = "Data/Interface/PNGIconInjector/banner.png";
		constexpr float kBannerAspect = 5.0f;

		CSimpleIniA g_ini;
		bool g_loaded = false;
		bool g_pending = false;
		std::string g_message;  // Translation key

		using Localization::Get;

		// ### keeps the ImGui id stable across languages.
		std::string Label(std::string_view a_key, std::string_view a_id)
		{
			return std::format("{}###{}", Get(a_key), a_id);
		}

		void Header(std::string_view a_key)
		{
			ImGuiMCP::SeparatorText(Get(a_key).c_str());
		}

		void Load()
		{
			g_ini.Reset();
			g_ini.SetUnicode();
			g_message = g_ini.LoadFile(kIni) < SI_OK ? "Messages.ReadFailed" : "";
			g_loaded = true;
			g_pending = false;
		}

		void Save()
		{
			g_pending = false;
			// No BOM: GetPrivateProfileStringA would read it into the first section name.
			if (const SI_Error rc = g_ini.SaveFile(kIni, false); rc < SI_OK) {
				g_message = "Messages.SaveFailed";
				logger::error("settings menu: saving PNGIconInjector.ini failed (SimpleIni error {})", static_cast<int>(rc));
				return;
			}
			g_message.clear();
			logger::info("settings menu: PNGIconInjector.ini saved");
			Settings::Reload();
		}

		// Saved once no control is held, so slider and color drags write on release.
		void Set(const char* a_section, const char* a_key, const std::string& a_value)
		{
			g_ini.SetValue(a_section, a_key, a_value.c_str());
			g_pending = true;
		}

		void Toggle(std::string_view a_label, const char* a_section, const char* a_key, const char* a_default = "1")
		{
			bool value = std::string_view(g_ini.GetValue(a_section, a_key, a_default)) != "0";
			if (ImGuiMCP::Checkbox(Label(a_label, a_key).c_str(), &value)) {
				Set(a_section, a_key, value ? "1" : "0");
			}
		}

		void MenusTab()
		{
			Header("Menus.ItemIcons");
			Toggle("Menus.Inventory", "Menus", "bInventory");
			Toggle("Menus.Container", "Menus", "bContainer");
			Toggle("Menus.Barter", "Menus", "bBarter");
			Toggle("Menus.Gift", "Menus", "bGift");
			Toggle("Menus.Magic", "Menus", "bMagic");
			Toggle("Menus.Favorites", "Menus", "bFavorites");
			Toggle("Menus.Crafting", "Menus", "bCrafting");
			Header("Menus.CategoryIcons");
			Toggle("Menus.ItemMenuTabs", "Categories", "bItemMenuTabs");
			Toggle("Menus.CraftingTabs", "Categories", "bCraftingTabs");
		}

		void StatusTab()
		{
			Toggle("Status.Enable", "StatusIcons", "bEnabled");
			ImGuiMCP::BeginDisabled(std::string_view(g_ini.GetValue("StatusIcons", "bEnabled", "1")) == "0");
			Header("Status.Sources");
			Toggle("Status.Museum", "StatusIcons", "bMuseum");
			Toggle("Status.QuestItem", "StatusIcons", "bQuestItem");
			Toggle("Status.Enchantment", "StatusIcons", "bEnchantment");
			Toggle("Status.NewItem", "StatusIcons", "bNewItem");
			Header("Status.Ingredients");
			Toggle("Status.IngredientProgress", "StatusIcons", "bShowIngredientProgress");
			Toggle("Status.EffectIcons", "StatusIcons", "bShowEffectIcons");

			int maxEffects = 2;
			const std::string_view maxText = g_ini.GetValue("StatusIcons", "iMaxEffectIcons", "2");
			std::from_chars(maxText.data(), maxText.data() + maxText.size(), maxEffects);
			if (ImGuiMCP::SliderInt(Label("Status.MaxEffectIcons", "iMaxEffectIcons").c_str(), &maxEffects, 1, 4)) {
				Set("StatusIcons", "iMaxEffectIcons", std::to_string(maxEffects));
			}

			Header("Status.Size");
			float size = 0.7f;
			const std::string_view sizeText = g_ini.GetValue("StatusIcons", "fStatusIconSize", "0.65");
			std::from_chars(sizeText.data(), sizeText.data() + sizeText.size(), size);
			if (ImGuiMCP::SliderFloat(Label("Status.IconSize", "fStatusIconSize").c_str(), &size, 0.3f, 0.9f, "%.2f")) {
				Set("StatusIcons", "fStatusIconSize", std::format("{:.2f}", size));
			}
			ImGuiMCP::EndDisabled();
		}

		void Color(std::string_view a_label, const char* a_key)
		{
			std::string_view hex = g_ini.GetValue("Colors", a_key, Settings::Default("Colors", a_key));
			if (hex.starts_with('#')) {
				hex.remove_prefix(1);
			} else if (hex.starts_with("0x") || hex.starts_with("0X")) {
				hex.remove_prefix(2);
			}
			std::uint32_t rgb = 0xFFFFFF;
			std::from_chars(hex.data(), hex.data() + hex.size(), rgb, 16);
			float color[3]{ ((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f };
			if (ImGuiMCP::ColorEdit3((a_label.empty() ? std::format("##{}", a_key) : Label(a_label, a_key)).c_str(), color, ImGuiMCP::ImGuiColorEditFlags_NoInputs)) {
				const auto channel = [](float a_value) { return static_cast<unsigned>(std::clamp(a_value, 0.0f, 1.0f) * 255.0f + 0.5f); };
				Set("Colors", a_key, std::format("{:02X}{:02X}{:02X}", channel(color[0]), channel(color[1]), channel(color[2])));
			}
		}

		void ColorsPage()
		{
			Toggle("Colors.UseCustom", "Colors", "bUseCustomColors");
			ImGuiMCP::BeginDisabled(std::string_view(g_ini.GetValue("Colors", "bUseCustomColors", "1")) == "0");

			static int preset = 0;
			ImGuiMCP::SetNextItemWidth(200.0f);
			const auto presetName = [](int a_index) -> const std::string& { return Get(std::format("Presets.{}", ColorPresets::kPresets[a_index].key)); };
			if (ImGuiMCP::BeginCombo("##pngii_color_preset", presetName(preset).c_str())) {
				for (int i = 0; i < static_cast<int>(std::size(ColorPresets::kPresets)); ++i) {
					if (ImGuiMCP::Selectable(std::format("{}##{}", presetName(i), i).c_str(), i == preset)) {
						preset = i;
					}
				}
				ImGuiMCP::EndCombo();
			}
			ImGuiMCP::SameLine();
			if (ImGuiMCP::Button(Label("Colors.ApplyPreset", "pngii_apply_preset").c_str())) {
				for (const auto* key : ColorPresets::AllKeys()) {
					Set("Colors", key, ColorPresets::ValueFor(ColorPresets::kPresets[preset], key));
				}
			}

			if (ImGuiMCP::BeginTabBar("##pngii_color_tabs")) {
				if (ImGuiMCP::BeginTabItem(Label("Colors.TabStatus", "tab_color_status").c_str())) {
					Color("Colors.NewItem", "sNewItem");
					Color("Colors.BestInClass", "sBestInClass");
					Color("Colors.QuestItem", "sQuestItem");
					Color("Colors.Favorite", "sFavorite");
					Color("Colors.HarmfulEffect", "sHarmfulEffect");
					ImGuiMCP::EndTabItem();
				}
				if (ImGuiMCP::BeginTabItem(Label("Colors.TabItems", "tab_color_items").c_str())) {
					ImGuiMCP::TextDisabled("%s", Get("Colors.ItemsHelp").c_str());
					// Sorted by translated name, rebuilt on a language change.
					const auto name = [](const char* a_key) -> const std::string& { return Get(std::format("ItemTints.{}", a_key)); };
					static std::pair<std::vector<std::size_t>, std::vector<std::size_t>> order;
					static std::uint32_t generation = 0;
					if (generation != Localization::Generation() || order.first.empty()) {
						generation = Localization::Generation();
						order.first.resize(std::size(ItemTints::kCategories));
						std::iota(order.first.begin(), order.first.end(), std::size_t{ 0 });
						std::ranges::sort(order.first, {}, [&](std::size_t a_index) { return name(ItemTints::kCategories[a_index].key); });
						order.second.resize(std::size(ItemTints::kSubs));
						std::iota(order.second.begin(), order.second.end(), std::size_t{ 0 });
						std::ranges::sort(order.second, {}, [&](std::size_t a_index) { return name(ItemTints::kSubs[a_index].key); });
					}
					for (const auto c : order.first) {
						const auto& category = ItemTints::kCategories[c];
						const auto count = std::ranges::count_if(ItemTints::kSubs, [&](const auto& a_sub) { return a_sub.category == c; });
						// Swatch on the header line so the category color is editable while collapsed.
						Color({}, category.key);
						ImGuiMCP::SameLine();
						// The count tells an empty group apart before it is opened; ##key keeps the id stable.
						if (ImGuiMCP::CollapsingHeader(std::format("{} ({})###{}", name(category.key), count, category.key).c_str())) {
							ImGuiMCP::Indent();
							for (const auto i : order.second) {
								const auto& sub = ItemTints::kSubs[i];
								if (sub.category == c) {
									Color(std::format("ItemTints.{}", sub.key), sub.key);
								}
							}
							ImGuiMCP::Unindent();
						}
					}
					ImGuiMCP::EndTabItem();
				}
				ImGuiMCP::EndTabBar();
			}
			ImGuiMCP::EndDisabled();
		}

		void OtherTab()
		{
			Header("Other.Language");
			const auto& languages = Localization::Available();
			const auto current = std::ranges::find(languages, Localization::Code(), &Localization::Language::code);
			ImGuiMCP::SetNextItemWidth(200.0f);
			if (ImGuiMCP::BeginCombo(Label("Other.LanguageCombo", "pngii_language").c_str(), current != languages.end() ? current->name.c_str() : Localization::Code().c_str())) {
				// Load after the loop: it rebuilds the list being walked.
				std::string picked;
				for (const auto& language : languages) {
					if (ImGuiMCP::Selectable(std::format("{}##{}", language.name, language.code).c_str(), language.code == Localization::Code())) {
						picked = language.code;
					}
				}
				ImGuiMCP::EndCombo();
				if (!picked.empty() && picked != Localization::Code()) {
					Set("Localization", "sLanguage", picked);
					Localization::Load(picked);
				}
			}
			Header("Other.OtherMods");
			Toggle("Other.TrueHUD", "TrueHUD", "bRecentLootIcons");
			Toggle("Other.QuickLoot", "QuickLoot", "bLootMenuIcons");
			Toggle("Other.MapMarkers", "MapMarkers", "bMapMarkerIcons");
			Header("Other.Rendering");
			Toggle("Other.SharpSVG", "Icons", "bSharpSVG");
		}

		void StatsTab()
		{
			const auto& files = IconRules::LoadedFiles();
			std::size_t rules = 0;
			std::size_t status = 0;
			std::size_t skipped = 0;
			for (const auto& file : files) {
				rules += file.rules;
				status += file.statusIconRules;
				skipped += file.skipped;
			}
			using namespace ImGuiMCP;
			if (BeginTable("pngii_totals", 2, ImGuiTableFlags_SizingFixedFit)) {
				const std::pair<const char*, std::size_t> totals[] = {
					{ "Stats.RuleFiles", files.size() }, { "Stats.Rules", rules }, { "Stats.StatusIconRules", status }, { "Stats.Skipped", skipped },
					{ "Stats.MapMarkerRules", IconRules::MapMarkerRuleCount() }, { "Stats.CategoryIcons", IconRules::CategoryIconCount() },
				};
				for (const auto& [label, value] : totals) {
					TableNextRow();
					TableNextColumn();
					Text("%s", Get(label).c_str());
					TableNextColumn();
					Text("%zu", value);
				}
				TableNextRow();
				TableNextColumn();
				Text("%s", Get("Stats.TCC").c_str());
				TableNextColumn();
				if (TCCItemStatus::Loaded()) {
					TextColored(ImVec4{ 0.45f, 0.85f, 0.45f, 1.0f }, "%s", Get("Stats.Detected").c_str());
				} else {
					TextDisabled("%s", Get("Stats.NotInstalled").c_str());
				}
				EndTable();
			}
			Spacing();

			if (BeginTable("pngii_files", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
				TableSetupColumn(Get("Stats.ColFile").c_str(), ImGuiTableColumnFlags_WidthStretch);
				TableSetupColumn(Get("Stats.ColRules").c_str());
				TableSetupColumn(Get("Stats.ColSkipped").c_str());
				TableHeadersRow();
				for (const auto& file : files) {
					TableNextRow();
					TableNextColumn();
					Text("%s", file.name.c_str());
					TableNextColumn();
					if (file.invalid) {
						Text("%s", Get("Stats.Invalid").c_str());
					} else {
						Text("%zu", file.rules + file.mapMarkerRules);
					}
					TableNextColumn();
					Text("%zu", file.skipped + file.mapMarkerSkipped);
				}
				EndTable();
			}
		}

		void DebugTab()
		{
			constexpr const char* kLevels[] = { "debug", "info", "off" };
			const std::string_view level = g_ini.GetValue("Debug", "sLogLevel", "info");
			int index = level == "debug" ? 0 : level == "off" ? 2 : 1;
			// The INI keeps the English values.
			const std::string shown[] = { Get("Debug.LevelDebug"), Get("Debug.LevelInfo"), Get("Debug.LevelOff") };
			const char* items[] = { shown[0].c_str(), shown[1].c_str(), shown[2].c_str() };
			if (ImGuiMCP::Combo(Label("Debug.LogLevel", "pngii_log_level").c_str(), &index, items, 3)) {
				Set("Debug", "sLogLevel", kLevels[index]);
			}
			Toggle("Debug.LogMatches", "Debug", "bLogMatches", "0");
			Toggle("Debug.LogTiming", "Debug", "bLogTiming", "0");
		}

		void Banner()
		{
			static ImGuiMCP::ImTextureID texture = nullptr;
			static bool tried = false;
			if (!tried) {
				tried = true;
				std::error_code ec;
				if (std::filesystem::exists(kBanner, ec)) {
					texture = SKSEMenuFramework::LoadTexture(kBanner);
				}
				logger::info("settings menu: banner {}", texture ? "loaded" : "not found");
			}
			if (!texture) {
				return;
			}
			const float width = ImGuiMCP::GetContentRegionAvail().x;
			ImGuiMCP::Image(texture, ImGuiMCP::ImVec2(width, width / kBannerAspect));
		}

		void Page(void (*a_body)())
		{
			if (!g_loaded) {
				Load();
			}
			Banner();
			if (!g_message.empty()) {
				ImGuiMCP::Text("%s", Get(g_message).c_str());
			} else {
				ImGuiMCP::TextDisabled("%s", Get("Page.SavesAtOnce").c_str());
			}
			ImGuiMCP::Separator();
			a_body();
			if (g_pending && !ImGuiMCP::IsAnyItemActive()) {
				Save();
			}
		}

		void SettingsTabs()
		{
			if (ImGuiMCP::BeginTabBar("pngii_settings")) {
				const std::tuple<const char*, const char*, void (*)()> tabs[] = {
					{ "Tabs.Menus", "tab_menus", MenusTab }, { "Tabs.Status", "tab_status", StatusTab }, { "Tabs.Other", "tab_other", OtherTab },
					{ "Tabs.Stats", "tab_stats", StatsTab }, { "Tabs.Debug", "tab_debug", DebugTab },
				};
				for (const auto& [key, id, draw] : tabs) {
					if (ImGuiMCP::BeginTabItem(Label(key, id).c_str())) {
						draw();
						ImGuiMCP::EndTabItem();
					}
				}
				ImGuiMCP::EndTabBar();
			}
		}

		void __stdcall RenderSettings() { Page(SettingsTabs); }
		void __stdcall RenderColors() { Page(ColorsPage); }
	}

	void Register()
	{
		auto* module = GetMenuFrameworkModule();
		if (!module || !GetProcAddress(module, "AddSectionItem")) {
			logger::info("settings menu: SKSE Menu Framework not found, INI only");
			return;
		}
		SKSEMenuFramework::SetSection("SVG and PNG Icon Injector");
		// Page names change after a restart.
		SKSEMenuFramework::AddSectionItem(Get("Pages.Settings"), RenderSettings);
		SKSEMenuFramework::AddSectionItem(Get("Pages.Colors"), RenderColors);
		logger::info("settings menu: registered with SKSE Menu Framework");
	}
}
