#pragma once

// Settings page strings from Data/SKSE/Plugins/Translations/PNGIconInjector_<code>.txt; missing keys fall back to _en, then the key.
namespace Localization
{
	struct Language
	{
		std::string code;
		std::string name;  // [Meta] Name, else the code
	};

	void Load(std::string_view a_code);
	const std::string& Code();
	// The reference stays valid until the next Load.
	const std::string& Get(std::string_view a_key);
	// Bumped by every Load.
	std::uint32_t Generation();
	const std::vector<Language>& Available();
}
