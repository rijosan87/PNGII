#include "SettingsMenu.h"
#include "ColorPresets.h"
#include "IconRules.h"
#include "ItemTints.h"
#include "Settings.h"
#include "TCCItemStatus.h"

#include <SimpleIni.h>
#include <numeric>

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
		std::string g_message;

		void Load()
		{
			g_ini.Reset();
			g_ini.SetUnicode();
			g_message = g_ini.LoadFile(kIni) < SI_OK ? "PNGIconInjector.ini could not be read." : "";
			g_loaded = true;
			g_pending = false;
		}

		void Save()
		{
			g_pending = false;
			// No BOM: GetPrivateProfileStringA would read it into the first section name.
			if (const SI_Error rc = g_ini.SaveFile(kIni, false); rc < SI_OK) {
				g_message = "Saving failed. Is PNGIconInjector.ini read-only?";
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

		void Toggle(const char* a_label, const char* a_section, const char* a_key, const char* a_default = "1")
		{
			bool value = std::string_view(g_ini.GetValue(a_section, a_key, a_default)) != "0";
			if (ImGuiMCP::Checkbox(a_label, &value)) {
				Set(a_section, a_key, value ? "1" : "0");
			}
		}

		void MenusTab()
		{
			ImGuiMCP::SeparatorText("Item icons");
			Toggle("Inventory", "Menus", "bInventory");
			Toggle("Container", "Menus", "bContainer");
			Toggle("Barter", "Menus", "bBarter");
			Toggle("Gift", "Menus", "bGift");
			Toggle("Magic (spells, powers, shouts)", "Menus", "bMagic");
			Toggle("Favorites", "Menus", "bFavorites");
			Toggle("Crafting (forge, smelter, tanning, enchanting, alchemy)", "Menus", "bCrafting");
			ImGuiMCP::SeparatorText("Category icons");
			Toggle("Item menu category bar", "Categories", "bItemMenuTabs");
			Toggle("Crafting categories", "Categories", "bCraftingTabs");
		}

		void StatusTab()
		{
			Toggle("Enable status icons", "StatusIcons", "bEnabled");
			ImGuiMCP::BeginDisabled(std::string_view(g_ini.GetValue("StatusIcons", "bEnabled", "1")) == "0");
			ImGuiMCP::SeparatorText("Sources");
			Toggle("Museum (Legacy of the Dragonborn)", "StatusIcons", "bMuseum");
			Toggle("Quest item", "StatusIcons", "bQuestItem");
			Toggle("Enchantment (known, unknown, cannot disenchant)", "StatusIcons", "bEnchantment");
			Toggle("New item", "StatusIcons", "bNewItem");
			ImGuiMCP::SeparatorText("Ingredients");
			Toggle("Learned-effects progress", "StatusIcons", "bShowIngredientProgress");
			Toggle("Discovered-effect icons", "StatusIcons", "bShowEffectIcons");

			int maxEffects = 2;
			const std::string_view maxText = g_ini.GetValue("StatusIcons", "iMaxEffectIcons", "2");
			std::from_chars(maxText.data(), maxText.data() + maxText.size(), maxEffects);
			if (ImGuiMCP::SliderInt("Effect icons per ingredient", &maxEffects, 1, 4)) {
				Set("StatusIcons", "iMaxEffectIcons", std::to_string(maxEffects));
			}

			ImGuiMCP::SeparatorText("Size");
			float size = 0.7f;
			const std::string_view sizeText = g_ini.GetValue("StatusIcons", "fStatusIconSize", "0.65");
			std::from_chars(sizeText.data(), sizeText.data() + sizeText.size(), size);
			if (ImGuiMCP::SliderFloat("Status icon size (fraction of row)", &size, 0.3f, 0.9f, "%.2f")) {
				Set("StatusIcons", "fStatusIconSize", std::format("{:.2f}", size));
			}
			ImGuiMCP::EndDisabled();
		}

		void Color(const char* a_label, const char* a_key)
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
			if (ImGuiMCP::ColorEdit3(a_label, color, ImGuiMCP::ImGuiColorEditFlags_NoInputs)) {
				const auto channel = [](float a_value) { return static_cast<unsigned>(std::clamp(a_value, 0.0f, 1.0f) * 255.0f + 0.5f); };
				Set("Colors", a_key, std::format("{:02X}{:02X}{:02X}", channel(color[0]), channel(color[1]), channel(color[2])));
			}
		}

		void ColorsPage()
		{
			Toggle("Use custom colors", "Colors", "bUseCustomColors");
			ImGuiMCP::BeginDisabled(std::string_view(g_ini.GetValue("Colors", "bUseCustomColors", "1")) == "0");

			static int preset = 0;
			ImGuiMCP::SetNextItemWidth(200.0f);
			if (ImGuiMCP::BeginCombo("##pngii_color_preset", ColorPresets::kPresets[preset].name)) {
				for (int i = 0; i < static_cast<int>(std::size(ColorPresets::kPresets)); ++i) {
					if (ImGuiMCP::Selectable(ColorPresets::kPresets[i].name, i == preset)) {
						preset = i;
					}
				}
				ImGuiMCP::EndCombo();
			}
			ImGuiMCP::SameLine();
			if (ImGuiMCP::Button("Apply preset")) {
				for (const auto* key : ColorPresets::AllKeys()) {
					Set("Colors", key, ColorPresets::ValueFor(ColorPresets::kPresets[preset], key));
				}
			}

			if (ImGuiMCP::BeginTabBar("##pngii_color_tabs")) {
				if (ImGuiMCP::BeginTabItem("Status icons")) {
					Color("New item", "sNewItem");
					Color("Best in class", "sBestInClass");
					Color("Quest item", "sQuestItem");
					Color("Favorite", "sFavorite");
					Color("Harmful ingredient effect", "sHarmfulEffect");
					ImGuiMCP::EndTabItem();
				}
				if (ImGuiMCP::BeginTabItem("Item icons")) {
					ImGuiMCP::TextDisabled("Color a whole category, or a subcategory on its own. White = no tint.");
					// Alphabetical for display only; ItemTints indices stay in table order.
					static const auto order = [] {
						std::vector<std::size_t> categories(std::size(ItemTints::kCategories));
						std::iota(categories.begin(), categories.end(), std::size_t{ 0 });
						std::ranges::sort(categories, {}, [](std::size_t a_index) { return std::string_view(ItemTints::kCategories[a_index].name); });
						std::vector<std::size_t> subs(std::size(ItemTints::kSubs));
						std::iota(subs.begin(), subs.end(), std::size_t{ 0 });
						std::ranges::sort(subs, {}, [](std::size_t a_index) { return std::string_view(ItemTints::kSubs[a_index].name); });
						return std::pair{ categories, subs };
					}();
					for (const auto c : order.first) {
						const auto& category = ItemTints::kCategories[c];
						const auto count = std::ranges::count_if(ItemTints::kSubs, [&](const auto& a_sub) { return a_sub.category == c; });
						// Swatch on the header line so the category color is editable while collapsed.
						Color(std::format("##{}", category.key).c_str(), category.key);
						ImGuiMCP::SameLine();
						// The count tells an empty group apart before it is opened; ##key keeps the id stable.
						if (ImGuiMCP::CollapsingHeader(std::format("{} ({})##{}", category.name, count, category.key).c_str())) {
							ImGuiMCP::Indent();
							for (const auto i : order.second) {
								const auto& sub = ItemTints::kSubs[i];
								if (sub.category == c) {
									Color(std::format("{}##{}", sub.name, sub.key).c_str(), sub.key);
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
			ImGuiMCP::SeparatorText("Other mods");
			Toggle("TrueHUD Recent Loot", "TrueHUD", "bRecentLootIcons");
			Toggle("QuickLoot loot window", "QuickLoot", "bLootMenuIcons");
			Toggle("World and local map markers", "MapMarkers", "bMapMarkerIcons");
			ImGuiMCP::SeparatorText("Rendering");
			Toggle("Sharp SVG icons (applies after restart)", "Icons", "bSharpSVG");
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
			ImGuiMCP::Text("%zu rule files, %zu rules (%zu status icon rules), %zu skipped", files.size(), rules, status, skipped);
			ImGuiMCP::Text("%zu map marker rules, %zu category icons", IconRules::MapMarkerRuleCount(), IconRules::CategoryIconCount());
			ImGuiMCP::Text("Legacy of the Dragonborn museum status: %s", TCCItemStatus::Loaded() ? "found" : "not found");
			ImGuiMCP::Spacing();

			using namespace ImGuiMCP;
			if (BeginTable("pngii_files", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
				TableSetupColumn("File", ImGuiTableColumnFlags_WidthStretch);
				TableSetupColumn("Rules");
				TableSetupColumn("Status");
				TableSetupColumn("Skipped");
				TableHeadersRow();
				for (const auto& file : files) {
					TableNextRow();
					TableNextColumn();
					Text("%s", file.name.c_str());
					TableNextColumn();
					if (file.invalid) {
						Text("invalid");
					} else {
						Text("%zu", file.rules);
					}
					TableNextColumn();
					Text("%zu", file.statusIconRules);
					TableNextColumn();
					Text("%zu", file.skipped);
				}
				EndTable();
			}
			if (skipped) {
				TextWrapped("Skipped rules are listed with their reason in PNGIconInjector.log.");
			}
		}

		void DebugTab()
		{
			constexpr const char* kLevels[] = { "debug", "info", "off" };
			const std::string_view level = g_ini.GetValue("Debug", "sLogLevel", "info");
			int index = level == "debug" ? 0 : level == "off" ? 2 : 1;
			if (ImGuiMCP::Combo("Log level", &index, kLevels, 3)) {
				Set("Debug", "sLogLevel", kLevels[index]);
			}
			Toggle("Log rule matches (for rule authors)", "Debug", "bLogMatches", "0");
			Toggle("Log menu and rule timing", "Debug", "bLogTiming", "0");
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
				ImGuiMCP::Text("%s", g_message.c_str());
			} else {
				ImGuiMCP::TextDisabled("Changes save at once and apply the next time a menu opens.");
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
				const std::pair<const char*, void (*)()> tabs[] = {
					{ "Menus", MenusTab }, { "Status icons", StatusTab }, { "Other", OtherTab },
					{ "Stats", StatsTab }, { "Debug", DebugTab },
				};
				for (const auto& [label, draw] : tabs) {
					if (ImGuiMCP::BeginTabItem(label)) {
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
		SKSEMenuFramework::AddSectionItem("Settings", RenderSettings);
		SKSEMenuFramework::AddSectionItem("Colors", RenderColors);
		logger::info("settings menu: registered with SKSE Menu Framework");
	}
}
