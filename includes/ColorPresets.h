#pragma once

#include "ItemTints.h"
#include "Settings.h"

namespace ColorPresets
{
	inline constexpr const char* kColorKeys[] = { "sNewItem", "sBestInClass", "sQuestItem", "sFavorite", "sHarmfulEffect" };
	inline constexpr std::size_t kKeyCount = std::size(kColorKeys) + std::size(ItemTints::kCategories) + std::size(ItemTints::kSubs);

	// Every key Apply preset writes, status tints first.
	inline std::vector<const char*> AllKeys()
	{
		std::vector<const char*> keys(std::begin(kColorKeys), std::end(kColorKeys));
		for (const auto& category : ItemTints::kCategories) {
			keys.push_back(category.key);
		}
		for (const auto& sub : ItemTints::kSubs) {
			keys.push_back(sub.key);
		}
		return keys;
	}

	struct Value
	{
		const char* key;
		const char* hex;
	};

	struct Preset
	{
		const char* key;
		std::array<Value, kKeyCount> values{};
	};

	// Keys a preset leaves out get their INI default, so "Default" is the reset.
	inline constexpr Preset kPresets[] = {
		{ "Default", {} },
	};

	inline const char* ValueFor(const Preset& a_preset, const char* a_key)
	{
		for (const auto& value : a_preset.values) {
			if (value.key && std::string_view(value.key) == a_key) {
				return value.hex;
			}
		}
		return Settings::Default("Colors", a_key);
	}
}
