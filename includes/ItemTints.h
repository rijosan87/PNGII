#pragma once

#include <functional>

// Row icon tint per item category with an optional subcategory override, INI [Colors] sItem<Cat>[<Sub>].
// The category comes from the form, not the row iconLabel, so icon pack relabels do not change a color.
namespace ItemTints
{
	struct Category
	{
		const char* key;  // Display name: ItemTints.<key> in the translation file
	};

	struct Sub
	{
		std::uint8_t category;  // Index into kCategories
		const char* key;
	};

	inline constexpr Category kCategories[] = {
		{ "sItemWeapons" },
		{ "sItemArmor" },
		{ "sItemAmmo" },
		{ "sItemPotions" },
		{ "sItemPoisons" },
		{ "sItemFood" },
		{ "sItemIngredients" },
		{ "sItemBooks" },
		{ "sItemScrolls" },
		{ "sItemSoulGems" },
		{ "sItemKeys" },
		{ "sItemMisc" },
	};

	// Label to subcategory mapping lives in ItemTints.cpp; keep both in the same order.
	inline constexpr Sub kSubs[] = {
		{ 0, "sItemWeaponsSwords" },
		{ 0, "sItemWeaponsDaggers" },
		{ 0, "sItemWeaponsAxes" },
		{ 0, "sItemWeaponsBlunt" },
		{ 0, "sItemWeaponsBows" },
		{ 0, "sItemWeaponsCrossbows" },
		{ 0, "sItemWeaponsStaves" },
		{ 0, "sItemWeaponsTools" },
		{ 1, "sItemArmorShields" },
		{ 1, "sItemArmorHeavy" },
		{ 1, "sItemArmorLight" },
		{ 1, "sItemArmorClothing" },
		{ 1, "sItemArmorJewelry" },
		{ 2, "sItemAmmoArrows" },
		{ 2, "sItemAmmoBolts" },
		{ 3, "sItemPotionsHealth" },
		{ 3, "sItemPotionsMagicka" },
		{ 3, "sItemPotionsStamina" },
		{ 3, "sItemPotionsResist" },
		{ 5, "sItemFoodFood" },
		{ 5, "sItemFoodDrinks" },
		{ 7, "sItemBooksBooks" },
		{ 7, "sItemBooksSpellTomes" },
		{ 7, "sItemBooksNotes" },
		{ 9, "sItemSoulGemsEmpty" },
		{ 9, "sItemSoulGemsPartial" },
		{ 9, "sItemSoulGemsFilled" },
		{ 11, "sItemMiscGems" },
		{ 11, "sItemMiscGold" },
		{ 11, "sItemMiscIngots" },
		{ 11, "sItemMiscHides" },
		{ 11, "sItemMiscAnimalParts" },
		{ 11, "sItemMiscArtifacts" },
		{ 11, "sItemMiscClutter" },
		{ 11, "sItemMiscFirewood" },
		{ 11, "sItemMiscLockpicks" },
	};

	// a_read returns the key's tint (0 = none), already 0 when the master switch is off. Main/UI thread like SetStatusColors.
	void Set(const std::function<std::uint32_t(const char*)>& a_read);
	// Non-zero category and subcategory colors, for the settings log line.
	std::size_t ActiveCount();
	// 0 = untinted. a_status: the row's soul status 0..2, else -1.
	std::uint32_t For(RE::FormID a_formID, std::int32_t a_status);
}
