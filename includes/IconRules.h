#pragma once

// Loads the JSON rule files in Data/SKSE/Plugins/PNGIconInjector and resolves icons for forms, labels and statuses.
// Higher priority wins, then the later file/rule. Match keys and defaults are documented in docs/matching.md.
namespace IconRules
{
	// Room for the ingredient progress icon, up to kMaxEffectIcons effect icons and one more (new, upgrade). Children are created on demand.
	inline constexpr std::size_t kMaxStatusIcons = 6;
	inline constexpr std::uint32_t kMaxEffectIcons = 4;  // An ingredient has four effects.
	// entry.pngii_live, written by StatusIcons' SKSE inventory callback per stack.
	inline constexpr std::uint32_t kLiveQuest = 1;
	inline constexpr std::uint32_t kLiveEnchShift = 1;
	inline constexpr std::uint32_t kLiveEnchMask = 3;  // 0 none, 1 Known, 2 Unknown, 3 NoDisenchant
	// Set by StatusIcons::Apply from NewItems, never by the inventory callback.
	inline constexpr std::uint32_t kLiveNew = 8;
	// Set by StatusIcons::Apply from the ingredient's knownEffectFlags: 0 no state, 1..5 = 0..4 effects known.
	inline constexpr std::uint32_t kLiveKnownShift = 4;
	inline constexpr std::uint32_t kLiveKnownMask = 7;
	// Set by StatusIcons::Apply: knownEffectFlags bits 0-3 (effect index i known); used by knownEffect icons.
	inline constexpr std::uint32_t kLiveEffectShift = 8;
	inline constexpr std::uint32_t kLiveEffectMask = 0xF;
	// Effect icons per row (INI [StatusIcons] iMaxEffectIcons); more than 2 crowds the row.
	inline constexpr std::uint32_t kDefaultMaxEffectIcons = 2;
	// Set by StatusIcons::Apply from the row value the inventory callback stored: armor/weapon better than the equipped baseline.
	inline constexpr std::uint32_t kLiveUpgrade = 128;
	// Set by StatusIcons::Apply from SkyUI's own entry.bestInClass flag (bits 8-11 belong to kLiveEffectShift).
	inline constexpr std::uint32_t kLiveBest = 4096;
	// Set by StatusIcons::Apply: SkyUI shows effect text (entry.isEnchanted) but the stack has no enchantment (Dawnguard
	// vampire amulets and similar built-in powers). Matched by enchantment "Ability".
	inline constexpr std::uint32_t kLiveAbility = 8192;
	// Set by StatusIcons::Apply from SkyUI's own entry.favorite flag (arrives as 1/0, read with Flag()).
	inline constexpr std::uint32_t kLiveFavorite = 16384;

	struct StatusIconMatch
	{
		const std::string* path;
		std::uint8_t replace;  // 0 append; 1..6 SkyUI bestInClass, favorite, poisoned, stolen, enchanted, read
		std::uint32_t tint;    // 0xRRGGBB multiply tint for the drawn clip, 0 = untinted (harmful effects)
	};

	struct Timer
	{
		std::uint64_t count = 0;
		std::uint64_t totalNs = 0;
		std::uint64_t maxNs = 0;

		void Add(std::uint64_t a_ns)
		{
			++count;
			totalNs += a_ns;
			maxNs = (std::max)(maxNs, a_ns);
		}
	};

	void SetDebug(bool a_matches, bool a_timing);
	// INI [StatusIcons]. a_progress off skips knownEffects rules; a_maxEffects is clamped to kMaxEffectIcons and 0
	// turns knownEffect icons off. a_harmTint (0xRRGGBB, 0 = none) tints effect icons of detrimental effects.
	void SetIngredientIcons(bool a_progress, std::uint32_t a_maxEffects, std::uint32_t a_harmTint);
	Timer TakeResolveTiming();
	void Load();
	// Resolves keyword identifiers before menu use. Safe from kPostLoadGame on.
	void PrepareKeywords();
	// Distinct icon paths of loaded rules: broad rules first (Better order), then form rules.
	std::vector<std::string> IconPaths();
	bool HasLabelRules();
	bool HasStatusRules();
	bool HasStatusIconRules();
	bool HasMuseumRules();
	// Rules with questItem or enchantment: StatusIcons registers the SKSE inventory callback only then.
	bool HasLiveRules();
	// Rules with newItem: NewItems tracks container changes and writes its co-save record only then.
	bool HasNewRules();
	// Rules with knownEffects or knownEffect: StatusIcons reads ingredient knowledge per drawn row only then.
	bool HasKnownEffectRules();
	// Rules with upgrade: StatusIcons compares armor/damage against the equipped baseline only then.
	bool HasUpgradeRules();
	// Distinct statusIcon paths in draw order (StatusIcons prewarm).
	std::vector<std::string> StatusIconPaths();
	// a_soulStatus: SkyUI entry.status, -1 none. a_museum: TCCItemStatus::Status as int, 0 None.
	// a_live: entry.pngii_live bits (kLiveQuest, enchantment state at kLiveEnchShift) plus kLiveNew, the
	// known-effects state at kLiveKnownShift, the known effect index mask at kLiveEffectShift, kLiveUpgrade and kLiveBest.
	// Paths point into the rule store and stay valid for the session.
	void FindStatusIcons(RE::FormID a_formID, std::int32_t a_soulStatus, std::int32_t a_museum, std::uint32_t a_live,
		std::vector<StatusIconMatch>& a_out);
	// Full path under Interface/PNGIconInjector/, or nullptr (keep vanilla).
	const std::string* Find(RE::FormID a_formID, std::string_view a_label, std::int32_t a_status);
	const std::string* FindOrigin(RE::FormID a_formID, std::string_view a_label, std::int32_t a_status);
	// categoryIcons: full path for a category label (case-insensitive), or nullptr. Built once at load.
	const std::string* FindCategoryIcon(std::string_view a_label);
	void LogStats();
	void ClearCache();
}
