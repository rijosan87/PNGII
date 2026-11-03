#pragma once

#include <functional>

// Row icon tint per item category with an optional subcategory override, INI [Colors] sItem<Cat>[<Sub>].
// The category comes from the form, not the row iconLabel, so icon pack relabels do not change a color.
namespace ItemTints
{
	struct Category
	{
		const char* key;
		const char* name;
	};

	struct Sub
	{
		std::uint8_t category;  // Index into kCategories
		const char* key;
		const char* name;
	};

	inline constexpr Category kCategories[] = {
		{ "sItemWeapons", "Weapons" },
		{ "sItemArmor", "Armor" },
		{ "sItemAmmo", "Ammo" },
		{ "sItemPotions", "Potions" },
		{ "sItemPoisons", "Poisons" },
		{ "sItemFood", "Food and drink" },
		{ "sItemIngredients", "Ingredients" },
		{ "sItemBooks", "Books" },
		{ "sItemScrolls", "Scrolls" },
		{ "sItemSoulGems", "Soul gems" },
		{ "sItemKeys", "Keys" },
		{ "sItemMisc", "Misc" },
	};

	// Label to subcategory mapping lives in ItemTints.cpp; keep both in the same order.
	inline constexpr Sub kSubs[] = {
		{ 0, "sItemWeaponsSwords", "Swords" },
		{ 0, "sItemWeaponsDaggers", "Daggers" },
		{ 0, "sItemWeaponsAxes", "Axes" },
		{ 0, "sItemWeaponsBlunt", "Blunt" },
		{ 0, "sItemWeaponsBows", "Bows" },
		{ 0, "sItemWeaponsCrossbows", "Crossbows" },
		{ 0, "sItemWeaponsStaves", "Staves" },
		{ 0, "sItemWeaponsTools", "Tools" },
		{ 1, "sItemArmorShields", "Shields" },
		{ 1, "sItemArmorHeavy", "Heavy" },
		{ 1, "sItemArmorLight", "Light" },
		{ 1, "sItemArmorClothing", "Clothing" },
		{ 1, "sItemArmorJewelry", "Jewelry" },
		{ 2, "sItemAmmoArrows", "Arrows" },
		{ 2, "sItemAmmoBolts", "Bolts" },
		{ 3, "sItemPotionsHealth", "Health" },
		{ 3, "sItemPotionsMagicka", "Magicka" },
		{ 3, "sItemPotionsStamina", "Stamina" },
		{ 3, "sItemPotionsResist", "Resist" },
		{ 5, "sItemFoodFood", "Food" },
		{ 5, "sItemFoodDrinks", "Drinks" },
		{ 7, "sItemBooksBooks", "Books" },
		{ 7, "sItemBooksSpellTomes", "Spell tomes" },
		{ 7, "sItemBooksNotes", "Notes" },
		{ 9, "sItemSoulGemsEmpty", "Empty" },
		{ 9, "sItemSoulGemsPartial", "Partial" },
		{ 9, "sItemSoulGemsFilled", "Filled" },
		{ 11, "sItemMiscGems", "Gems" },
		{ 11, "sItemMiscGold", "Gold" },
		{ 11, "sItemMiscIngots", "Ingots" },
		{ 11, "sItemMiscHides", "Hides" },
		{ 11, "sItemMiscAnimalParts", "Animal parts" },
		{ 11, "sItemMiscArtifacts", "Artifacts" },
		{ 11, "sItemMiscClutter", "Clutter" },
		{ 11, "sItemMiscFirewood", "Firewood" },
		{ 11, "sItemMiscLockpicks", "Lockpicks" },
	};

	// a_read returns the key's tint (0 = none), already 0 when the master switch is off. Main/UI thread like SetStatusColors.
	void Set(const std::function<std::uint32_t(const char*)>& a_read);
	// Non-zero category and subcategory colors, for the settings log line.
	std::size_t ActiveCount();
	// 0 = untinted. a_status: the row's soul status 0..2, else -1.
	std::uint32_t For(RE::FormID a_formID, std::int32_t a_status);
}
