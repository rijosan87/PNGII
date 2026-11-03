#include "ItemTints.h"
#include "IconLabels.h"

namespace ItemTints
{
	namespace
	{
		constexpr std::int8_t kNoSub = -1;

		struct LabelTint
		{
			std::string_view label;
			std::uint8_t category;
			std::int8_t sub;  // Index into kSubs, kNoSub = category color only
		};

		// Every item label IconLabels::ForFormID returns. Spell, shout and effect labels are left out: those icons are colored.
		constexpr LabelTint kLabels[] = {
			{ "weapon_sword", 0, 0 }, { "weapon_greatsword", 0, 0 },
			{ "weapon_dagger", 0, 1 },
			{ "weapon_waraxe", 0, 2 }, { "weapon_battleaxe", 0, 2 },
			{ "weapon_mace", 0, 3 }, { "weapon_hammer", 0, 3 },
			{ "weapon_bow", 0, 4 },
			{ "weapon_crossbow", 0, 5 },
			{ "weapon_staff", 0, 6 },
			{ "weapon_pickaxe", 0, 7 }, { "weapon_woodaxe", 0, 7 },
			{ "default_weapon", 0, kNoSub },

			{ "armor_shield", 1, 8 }, { "lightarmor_shield", 1, 8 }, { "clothing_shield", 1, 8 },
			{ "armor_head", 1, 9 }, { "armor_body", 1, 9 }, { "armor_hands", 1, 9 },
			{ "armor_forearms", 1, 9 }, { "armor_feet", 1, 9 }, { "armor_calves", 1, 9 },
			{ "lightarmor_head", 1, 10 }, { "lightarmor_body", 1, 10 }, { "lightarmor_hands", 1, 10 },
			{ "lightarmor_forearms", 1, 10 }, { "lightarmor_feet", 1, 10 }, { "lightarmor_calves", 1, 10 },
			{ "clothing_head", 1, 11 }, { "clothing_body", 1, 11 }, { "clothing_hands", 1, 11 },
			{ "clothing_forearms", 1, 11 }, { "clothing_feet", 1, 11 }, { "clothing_calves", 1, 11 },
			{ "armor_amulet", 1, 12 }, { "armor_ring", 1, 12 }, { "armor_circlet", 1, 12 },
			{ "default_armor", 1, kNoSub },

			{ "weapon_arrow", 2, 13 },
			{ "weapon_bolt", 2, 14 },

			{ "potion_health", 3, 15 },
			{ "potion_magic", 3, 16 },
			{ "potion_stam", 3, 17 },
			{ "potion_fire", 3, 18 }, { "potion_frost", 3, 18 }, { "potion_shock", 3, 18 },
			{ "default_potion", 3, kNoSub },

			{ "potion_poison", 4, kNoSub },

			{ "default_food", 5, 19 },
			{ "food_wine", 5, 20 },

			{ "default_ingredient", 6, kNoSub },

			{ "default_book", 7, 21 },
			{ "book_tome", 7, 22 },
			{ "book_note", 7, 23 },

			{ "default_scroll", 8, kNoSub },

			{ "soulgem_empty", 9, 24 }, { "soulgem_grandempty", 9, 24 }, { "misc_soulgem", 9, 24 },
			{ "soulgem_partial", 9, 25 }, { "soulgem_grandpartial", 9, 25 },
			{ "soulgem_full", 9, 26 }, { "soulgem_grandfull", 9, 26 }, { "soulgem_azura", 9, 26 },

			{ "default_key", 10, kNoSub },

			{ "misc_gem", 11, 27 },
			{ "misc_gold", 11, 28 },
			{ "misc_ingot", 11, 29 },
			{ "misc_hide", 11, 30 }, { "misc_leather", 11, 30 }, { "misc_strips", 11, 30 },
			{ "misc_remains", 11, 31 },
			{ "misc_artifact", 11, 32 }, { "misc_dragonclaw", 11, 32 },
			{ "misc_clutter", 11, 33 },
			{ "misc_wood", 11, 34 },
			{ "misc_lockpick", 11, 35 }, { "misc_torch", 11, 35 },
			{ "default_misc", 11, kNoSub },
		};

		std::array<std::uint32_t, std::size(kCategories)> g_category{};
		std::array<std::uint32_t, std::size(kSubs)> g_sub{};
		bool g_any = false;
	}

	void Set(const std::function<std::uint32_t(const char*)>& a_read)
	{
		for (std::size_t i = 0; i < g_category.size(); ++i) {
			g_category[i] = a_read(kCategories[i].key);
		}
		for (std::size_t i = 0; i < g_sub.size(); ++i) {
			g_sub[i] = a_read(kSubs[i].key);
		}
		g_any = ActiveCount() != 0;
	}

	std::size_t ActiveCount()
	{
		const auto set = [](std::uint32_t a_tint) { return a_tint != 0; };
		return static_cast<std::size_t>(std::ranges::count_if(g_category, set) + std::ranges::count_if(g_sub, set));
	}

	std::uint32_t For(RE::FormID a_formID, std::int32_t a_status)
	{
		// Without any color the label lookup is skipped entirely, so rows cost nothing extra by default.
		if (!g_any || !a_formID) {
			return 0;
		}
		const char* label = IconLabels::ForFormID(a_formID, a_status);
		if (!label) {
			return 0;
		}
		const std::string_view name(label);
		for (const auto& entry : kLabels) {
			if (entry.label == name) {
				const auto sub = entry.sub == kNoSub ? 0u : g_sub[static_cast<std::size_t>(entry.sub)];
				return sub ? sub : g_category[entry.category];
			}
		}
		return 0;
	}
}
