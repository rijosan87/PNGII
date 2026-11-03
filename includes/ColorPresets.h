#pragma once

#include "Settings.h"

namespace ColorPresets
{
	inline constexpr const char* kColorKeys[] = { "sNewItem", "sBestInClass", "sQuestItem", "sFavorite", "sHarmfulEffect" };

	struct Value
	{
		const char* key;
		const char* hex;
	};

	struct Preset
	{
		const char* name;
		std::array<Value, std::size(kColorKeys)> values{};
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
