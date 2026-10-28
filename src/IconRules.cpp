#include "IconRules.h"
#include "TCCItemStatus.h"
#include "TextureManager.h"

namespace IconRules
{
	namespace
	{
		constexpr auto kRuleFolder = "Data/SKSE/Plugins/PNGIconInjector";
		// PNGs are read as raw bytes via BSResourceNiBinaryStream, so any Data path works; only template.dds must stay under Textures\.
		constexpr auto kIconRoot = "Interface/PNGIconInjector/"sv;
		constexpr std::int32_t kFormTier = 100;
		constexpr std::int32_t kTypeTier = 10;
		constexpr std::int32_t kKeywordTier = 30;
		constexpr std::int32_t kPluginTier = 20;
		constexpr std::int32_t kLabelTier = 15;
		constexpr std::array<std::pair<std::string_view, RE::FormType>, 13> kFormTypes{ {
			{ "ammo"sv, RE::FormType::Ammo }, { "armor"sv, RE::FormType::Armor },
			{ "book"sv, RE::FormType::Book }, { "ingredient"sv, RE::FormType::Ingredient },
			{ "key"sv, RE::FormType::KeyMaster }, { "light"sv, RE::FormType::Light },
			{ "miscitem"sv, RE::FormType::Misc }, { "potion"sv, RE::FormType::AlchemyItem },
			{ "scroll"sv, RE::FormType::Scroll }, { "soulgem"sv, RE::FormType::SoulGem },
			{ "weapon"sv, RE::FormType::Weapon },
			{ "spell"sv, RE::FormType::Spell }, { "shout"sv, RE::FormType::Shout },
		} };

		struct EffectMatch
		{
			RE::FormID effectId = 0;
			std::vector<RE::EffectArchetypes::ArchetypeID> archetypes;
			std::vector<RE::ActorValue> values;
			std::optional<bool> detrimental;
		};

		constexpr std::array<std::string_view, 47> kArchetypeNames{
			"valuemodifier"sv, "script"sv, "dispel"sv, "curedisease"sv, "absorb"sv, "dualvaluemodifier"sv, "calm"sv,
			"demoralize"sv, "frenzy"sv, "disarm"sv, "commandsummoned"sv, "invisibility"sv, "light"sv, "darkness"sv,
			"nighteye"sv, "lock"sv, "open"sv, "boundweapon"sv, "summoncreature"sv, "detectlife"sv, "telekinesis"sv,
			"paralysis"sv, "reanimate"sv, "soultrap"sv, "turnundead"sv, "guide"sv, "werewolffeed"sv, "cureparalysis"sv,
			"cureaddiction"sv, "curepoison"sv, "concussion"sv, "valueandparts"sv, "accumulatemagnitude"sv, "stagger"sv,
			"peakvaluemodifier"sv, "cloak"sv, "werewolf"sv, "slowtime"sv, "rally"sv, "enhanceweapon"sv, "spawnhazard"sv,
			"etherealize"sv, "banish"sv, "spawnscriptedref"sv, "disguise"sv, "grabactor"sv, "vampirelord"sv
		};

		struct Rule
		{
			std::string path;
			std::int32_t priority;
			std::uint32_t order;
			std::string origin;
			std::vector<RE::FormType> types;
			std::vector<std::string> allIds;
			std::vector<std::string> anyIds;
			std::vector<const RE::BGSKeyword*> all;
			std::vector<const RE::BGSKeyword*> any;
			bool disabled = false;
			std::vector<const RE::TESFile*> plugins;
			bool exactForm = false;
			std::uint32_t parts = 0;  // Slot N = bit N-30; all listed slots required.
			std::vector<std::string> models;
			std::vector<std::string> effectAllIds;
			std::vector<std::string> effectAnyIds;
			std::vector<const RE::BGSKeyword*> effectAll;
			std::vector<const RE::BGSKeyword*> effectAny;
			std::uint8_t status = 0;
			std::vector<RE::FormID> forms;
			bool statusIcon = false;
			std::uint8_t museum = 0;  // Bit 0 New, 1 Found, 2 Displayed (TCCItemStatus::Status - 1); per row.
			std::optional<float> magMin;
			std::optional<float> magMax;
			std::optional<double> durMin;
			std::optional<double> durMax;
			bool questItem = false;
			bool newItem = false;
			std::uint8_t enchantment = 0;  // Bit 0 Known, 1 Unknown, 2 NoDisenchant, 3 Ability; per row.
			std::uint8_t knownEffects = 0;  // Bit k = k ingredient effects known (4 = all); per row.
			bool upgrade = false;
			bool bestInClass = false;
			bool favorite = false;
			std::uint8_t replace = 0;      // 1..6 SkyUI status icon in formatName order; 0 appends.
			std::optional<EffectMatch> knownEffect;

			bool HasRange() const { return magMin || magMax || durMin || durMax; }
		};

		struct Counts
		{
			std::size_t rules = 0;
			std::size_t forms = 0;
			std::size_t typeRules = 0;
			std::size_t keywordRules = 0;
			std::size_t pluginRules = 0;
			std::size_t labelRules = 0;
			std::size_t statusRules = 0;
			std::size_t statusIconRules = 0;
			std::size_t skipped = 0;
		};

		std::deque<Rule> g_rules;
		std::unordered_map<RE::FormID, const Rule*> g_byForm;
		std::unordered_map<std::string, const Rule*> g_byLabel;
		std::unordered_set<std::string> g_loggedLabels;
		std::vector<const Rule*> g_statusRules;
		std::unordered_set<std::uint64_t> g_loggedStatus;
		std::vector<const Rule*> g_statusIconRules;
		std::unordered_map<RE::FormID, std::vector<const Rule*>> g_statusIconCache;
		std::unordered_set<std::uint64_t> g_loggedStatusIcons;
		std::size_t g_museumRules = 0;
		std::size_t g_museumIgnored = 0;
		std::unordered_map<std::string, std::string> g_categoryIcons;
		std::size_t g_liveRules = 0;
		std::size_t g_newRules = 0;
		std::size_t g_knownEffectRules = 0;
		std::size_t g_effectRules = 0;
		std::size_t g_upgradeRules = 0;
		std::unordered_map<RE::FormType, std::vector<const Rule*>> g_byType;
		std::vector<const Rule*> g_anyType;
		bool g_keywordsResolved = false;
		std::unordered_map<RE::FormID, const Rule*> g_cache;
		std::uint32_t g_order = 0;
		std::uint64_t g_lookups = 0;
		struct Settings
		{
			bool logMatches = false;
			bool logTiming = false;
		};
		Settings g_debug;
		std::uint32_t g_maxEffectIcons = kDefaultMaxEffectIcons;
		bool g_showProgress = true;
		std::uint32_t g_harmTint = 0;
		Timer g_resolveTimer;

		void ResolveKeywords();
		std::string Lower(std::string_view a_text);

		bool EffectHas(const RE::MagicItem& a_item, const RE::BGSKeyword* a_keyword)
		{
			return std::ranges::any_of(a_item.effects, [&](const RE::Effect* effect) {
				return effect && effect->baseEffect && effect->baseEffect->HasKeyword(a_keyword);
			});
		}

		bool EffectHits(const EffectMatch& a_match, const RE::Effect* a_effect)
		{
			const auto* base = a_effect ? a_effect->baseEffect : nullptr;
			if (!base) {
				return false;
			}
			// Damage Stamina/Magicka Regen are DualValueModifier with a placeholder primary AV; the regen they change is the secondary AV.
			const auto hasValue = [&](RE::ActorValue a_value) { return std::ranges::find(a_match.values, a_value) != a_match.values.end(); };
			const bool dual = base->GetArchetype() == RE::EffectArchetypes::ArchetypeID::kDualValueModifier;
			return (!a_match.effectId || base->GetFormID() == a_match.effectId) &&
			       (a_match.archetypes.empty() || std::ranges::find(a_match.archetypes, base->GetArchetype()) != a_match.archetypes.end()) &&
			       (a_match.values.empty() || hasValue(base->data.primaryAV) || (dual && hasValue(base->data.secondaryAV))) &&
			       (!a_match.detrimental || base->IsDetrimental() == *a_match.detrimental);
		}

		std::uint8_t EffectIndexMask(const EffectMatch& a_match, const RE::IngredientItem& a_ingredient)
		{
			std::uint8_t mask = 0;
			const auto count = (std::min)(a_ingredient.effects.size(), std::uint32_t{ 4 });
			for (std::uint32_t i = 0; i < count; ++i) {
				if (EffectHits(a_match, a_ingredient.effects[i])) {
					mask |= static_cast<std::uint8_t>(1u << i);
				}
			}
			return mask;
		}

		bool Passes(const Rule& a_rule, const RE::TESForm& a_form)
		{
			if (a_rule.disabled) {
				return false;
			}
			if (!a_rule.plugins.empty()) {
				const auto* origin = a_form.GetFile(0);
				if (!origin || std::ranges::find(a_rule.plugins, origin) == a_rule.plugins.end()) {
					return false;
				}
			}
			if (a_rule.parts) {
				const auto* biped = a_form.As<RE::BGSBipedObjectForm>();
				if (!biped || (biped->GetSlotMask().underlying() & a_rule.parts) != a_rule.parts) {
					return false;
				}
			}
			if (!a_rule.models.empty()) {
				// Notes/journals are BookOrTome, told apart only by model. ARMO has no TESModel base: its models are in TESBipedModelForm, one per sex; either may match.
				std::array<const char*, 2> paths{};
				if (const auto* armor = a_form.As<RE::TESObjectARMO>()) {
					paths = { armor->worldModels[RE::TESBipedModelForm::Sexes::kMale].GetModel(), armor->worldModels[RE::TESBipedModelForm::Sexes::kFemale].GetModel() };
				} else if (const auto* model = a_form.As<RE::TESModel>()) {
					paths[0] = model->GetModel();
				}
				const auto matches = [&](const char* a_path) {
					if (!a_path || !*a_path) {
						return false;
					}
					const auto lower = Lower(a_path);
					return std::ranges::any_of(a_rule.models, [&](const std::string& needle) { return lower.contains(needle); });
				};
				if (std::ranges::none_of(paths, matches)) {
					return false;
				}
			}
			if (!a_rule.effectAll.empty() || !a_rule.effectAny.empty()) {
				const auto* magic = a_form.As<RE::MagicItem>();
				if (!magic) {
					return false;
				}
				for (const auto* keyword : a_rule.effectAll) {
					if (!EffectHas(*magic, keyword)) {
						return false;
					}
				}
				if (!a_rule.effectAny.empty() && std::ranges::none_of(a_rule.effectAny,
					[&](const auto* keyword) { return EffectHas(*magic, keyword); })) {
					return false;
				}
			}
			if (a_rule.HasRange()) {
				const auto* magic = a_form.As<RE::MagicItem>();
				if (!magic) {
					return false;
				}
				const bool keyed = !a_rule.effectAll.empty() || !a_rule.effectAny.empty();
				const auto measured = [&](const RE::Effect* effect) {
					if (!effect || !effect->baseEffect) {
						return false;
					}
					const auto has = [&](const RE::BGSKeyword* keyword) { return effect->baseEffect->HasKeyword(keyword); };
					if (keyed && std::ranges::none_of(a_rule.effectAll, has) && std::ranges::none_of(a_rule.effectAny, has)) {
						return false;
					}
					const float magnitude = effect->GetMagnitude();
					const double duration = effect->GetDuration();
					return (!a_rule.magMin || magnitude >= *a_rule.magMin) && (!a_rule.magMax || magnitude <= *a_rule.magMax) &&
					       (!a_rule.durMin || duration >= *a_rule.durMin) && (!a_rule.durMax || duration <= *a_rule.durMax);
				};
				if (std::ranges::none_of(magic->effects, measured)) {
					return false;
				}
			}
			// Static half only: never read knownEffectFlags here, the candidate cache would go stale when an ingredient is eaten.
			if (a_rule.knownEffect) {
				const auto* ingredient = a_form.As<RE::IngredientItem>();
				if (!ingredient || !EffectIndexMask(*a_rule.knownEffect, *ingredient)) {
					return false;
				}
			}
			if (a_rule.all.empty() && a_rule.any.empty()) {
				return true;
			}
			const auto* keywords = a_form.As<RE::BGSKeywordForm>();
			if (!keywords) {
				return false;
			}
			for (const auto* keyword : a_rule.all) {
				if (!keywords->HasKeyword(keyword)) {
					return false;
				}
			}
			return a_rule.any.empty() || std::ranges::any_of(a_rule.any,
				[&](const auto* keyword) { return keywords->HasKeyword(keyword); });
		}

		bool FormScope(const Rule& a_rule, RE::FormID a_formID, const RE::TESForm& a_form)
		{
			return (a_rule.forms.empty() || std::ranges::find(a_rule.forms, a_formID) != a_rule.forms.end()) &&
			       (a_rule.types.empty() || std::ranges::find(a_rule.types, a_form.GetFormType()) != a_rule.types.end()) &&
			       Passes(a_rule, a_form);
		}

		const Rule* FirstPassing(const std::vector<const Rule*>& a_bucket, const RE::TESForm& a_form)
		{
			for (const auto* rule : a_bucket) {
				if (Passes(*rule, a_form)) {
					return rule;
				}
			}
			return nullptr;
		}

		bool Better(const Rule* a_lhs, const Rule* a_rhs)
		{
			return !a_rhs || a_lhs->priority > a_rhs->priority ||
			       (a_lhs->priority == a_rhs->priority && a_lhs->order > a_rhs->order);
		}

		const Rule* Resolve(RE::FormID a_formID)
		{
			++g_lookups;
			if (const auto it = g_cache.find(a_formID); it != g_cache.end()) {
				return it->second;
			}
			const auto start = g_debug.logTiming ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
			if (!g_keywordsResolved) {
				ResolveKeywords();
			}
			const Rule* best = nullptr;
			const auto* form = RE::TESForm::LookupByID(a_formID);
			if (form) {
				if (const auto it = g_byForm.find(a_formID); it != g_byForm.end()) {
					best = it->second;
				}
				if (const auto it = g_byType.find(form->GetFormType()); it != g_byType.end()) {
					const auto* candidate = FirstPassing(it->second, *form);
					if (candidate && Better(candidate, best)) {
						best = candidate;
					}
				}
				const auto* candidate = FirstPassing(g_anyType, *form);
				if (candidate && Better(candidate, best)) {
					best = candidate;
				}
			}
			g_cache.emplace(a_formID, best);
			if (g_debug.logTiming) {
				g_resolveTimer.Add(std::chrono::duration_cast<std::chrono::nanoseconds>(
					std::chrono::steady_clock::now() - start).count());
			}
			if (g_debug.logMatches) {
				const auto* name = form ? form->GetName() : nullptr;
				const auto type = form ? static_cast<std::uint32_t>(form->GetFormType()) : 0;
				if (best) {
					logger::info("match {:08X} '{}' {} -> {} {}", a_formID, name ? name : "", type, best->origin, best->path);
				} else {
					logger::info("match {:08X} '{}' {} -> none", a_formID, name ? name : "", type);
				}
			}
			return best;
		}

		std::string Lower(std::string_view a_text)
		{
			std::string text{ a_text };
			std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return text;
		}

		const Rule* ResolveLabel(std::string_view a_label)
		{
			if (g_byLabel.empty() || a_label.empty()) {
				return nullptr;
			}
			const auto key = Lower(a_label);
			const auto it = g_byLabel.find(key);
			const Rule* rule = it != g_byLabel.end() ? it->second : nullptr;
			if (g_debug.logMatches && g_loggedLabels.insert(key).second) {
				if (rule) {
					logger::info("label {} -> {} {}", key, rule->origin, rule->path);
				} else {
					logger::info("label {} -> none", key);
				}
			}
			return rule;
		}

		const Rule* ResolveStatus(RE::FormID a_formID, std::int32_t a_status)
		{
			if (a_status < 0 || a_status > 2 || g_statusRules.empty() || !a_formID) {
				return nullptr;
			}
			if (!g_keywordsResolved) {
				ResolveKeywords();
			}
			const auto* form = RE::TESForm::LookupByID(a_formID);
			const Rule* found = nullptr;
			if (form) {
				const auto bit = static_cast<std::uint8_t>(1u << a_status);
				for (const auto* rule : g_statusRules) {
					if ((rule->status & bit) && FormScope(*rule, a_formID, *form)) {
						found = rule;
						break;
					}
				}
			}
			if (g_debug.logMatches && g_loggedStatus.insert((std::uint64_t{ a_formID } << 2) | a_status).second) {
				constexpr std::array<std::string_view, 3> kNames{ "Empty", "Partial", "Full" };
				if (found) {
					logger::info("status {:08X} {} -> {} {}", a_formID, kNames[a_status], found->origin, found->path);
				} else {
					logger::info("status {:08X} {} -> none", a_formID, kNames[a_status]);
				}
			}
			return found;
		}

		const std::vector<const Rule*>& StatusCandidates(RE::FormID a_formID)
		{
			if (const auto it = g_statusIconCache.find(a_formID); it != g_statusIconCache.end()) {
				return it->second;
			}
			if (!g_keywordsResolved) {
				ResolveKeywords();
			}
			std::vector<const Rule*> found;
			if (const auto* form = RE::TESForm::LookupByID(a_formID)) {
				for (const auto* rule : g_statusIconRules) {
					if (FormScope(*rule, a_formID, *form)) {
						found.push_back(rule);
					}
				}
			}
			return g_statusIconCache.emplace(a_formID, std::move(found)).first->second;
		}

		const Rule* Best(RE::FormID a_formID, std::string_view a_label, std::int32_t a_status)
		{
			const auto* best = a_formID ? Resolve(a_formID) : nullptr;
			if (const auto* label = ResolveLabel(a_label); label && Better(label, best)) {
				best = label;
			}
			if (const auto* status = ResolveStatus(a_formID, a_status); status && Better(status, best)) {
				best = status;
			}
			return best;
		}

		RE::FormID ResolveForm(std::string_view a_text, const std::string& a_origin)
		{
			const auto bar = a_text.find('|');
			if (bar == std::string_view::npos || bar == 0) {
				logger::warn("{}: bad form string '{}' (expected Plugin|hexID)", a_origin, a_text);
				return 0;
			}
			const auto plugin = a_text.substr(0, bar);
			auto id = a_text.substr(bar + 1);
			if (id.starts_with("0x") || id.starts_with("0X")) {
				id.remove_prefix(2);
			}
			RE::FormID local = 0;
			const auto [ptr, ec] = std::from_chars(id.data(), id.data() + id.size(), local, 16);
			if (ec != std::errc{} || ptr != id.data() + id.size()) {
				logger::warn("{}: bad form ID '{}'", a_origin, a_text);
				return 0;
			}

			auto* handler = RE::TESDataHandler::GetSingleton();
			const auto* file = handler ? handler->LookupModByName(plugin) : nullptr;
			if (!file || file->compileIndex == 0xFF) {
				logger::warn("{}: plugin not loaded '{}'", a_origin, plugin);
				return 0;
			}
			// xEdit IDs often include the load-order prefix; LookupFormID adds it unmasked.
			local &= file->IsLight() ? 0xFFF : 0xFFFFFF;
			const auto formID = handler->LookupFormID(local, plugin);
			if (!formID || !RE::TESForm::LookupByID(formID)) {
				logger::warn("{}: form not found '{}'", a_origin, a_text);
				return 0;
			}
			return formID;
		}

		const RE::BGSKeyword* FindKeyword(const std::string& a_id, const std::string& a_origin)
		{
			if (a_id.find('|') != std::string::npos) {
				const auto formID = ResolveForm(a_id, a_origin);
				if (!formID) {
					return nullptr;
				}
				const auto* form = RE::TESForm::LookupByID(formID);
				if (form->GetFormType() != RE::FormType::Keyword) {
					logger::warn("{}: keyword identifier '{}' is not a Keyword", a_origin, a_id);
					return nullptr;
				}
				return form->As<RE::BGSKeyword>();
			}
			if (const auto* keyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>(a_id)) {
				return keyword;
			}
			// KID-created keywords may be absent from the engine's EditorID map.
			if (auto* handler = RE::TESDataHandler::GetSingleton()) {
				const auto& keywords = handler->GetFormArray<RE::BGSKeyword>();
				for (auto index = keywords.size(); index > 0; --index) {
					const auto* keyword = keywords[index - 1];
					if (keyword && std::string_view(keyword->formEditorID.c_str()) == a_id) {
						return keyword;
					}
				}
			}
			logger::warn("{}: unknown keyword '{}'", a_origin, a_id);
			return nullptr;
		}

		void ResolveKeywords()
		{
			const auto start = g_debug.logTiming ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
			g_keywordsResolved = true;
			std::size_t resolved = 0;
			std::size_t unknown = 0;
			std::size_t disabled = 0;
			for (auto& rule : g_rules) {
				for (const auto& id : rule.allIds) {
					if (const auto* keyword = FindKeyword(id, rule.origin)) {
						rule.all.push_back(keyword);
						++resolved;
					} else {
						rule.disabled = true;
						++unknown;
					}
				}
				for (const auto& id : rule.anyIds) {
					if (const auto* keyword = FindKeyword(id, rule.origin)) {
						rule.any.push_back(keyword);
						++resolved;
					} else {
						++unknown;
					}
				}
				if (!rule.anyIds.empty() && rule.any.empty()) {
					rule.disabled = true;
				}
				for (const auto& id : rule.effectAllIds) {
					if (const auto* keyword = FindKeyword(id, rule.origin)) {
						rule.effectAll.push_back(keyword);
						++resolved;
					} else {
						rule.disabled = true;
						++unknown;
					}
				}
				for (const auto& id : rule.effectAnyIds) {
					if (const auto* keyword = FindKeyword(id, rule.origin)) {
						rule.effectAny.push_back(keyword);
						++resolved;
					} else {
						++unknown;
					}
				}
				if (!rule.effectAnyIds.empty() && rule.effectAny.empty()) {
					rule.disabled = true;
				}
				if (rule.disabled) {
					++disabled;
				}
			}
			logger::info("IconRules: resolved {} keywords, {} unknown, {} rules disabled", resolved, unknown, disabled);
			if (g_debug.logTiming) {
				logger::info("keyword resolve {} us", std::chrono::duration_cast<std::chrono::microseconds>(
					std::chrono::steady_clock::now() - start).count());
			}
		}

		bool ValidIcon(std::string& a_icon)
		{
			std::ranges::replace(a_icon, '\\', '/');
			return !a_icon.empty() && !a_icon.starts_with('/') && a_icon.find(':') == std::string::npos &&
			       a_icon.find("..") == std::string::npos && (Lower(a_icon).ends_with(".png") || Lower(a_icon).ends_with(".svg"));
		}

		void LoadRule(const nlohmann::json& a_rule, const std::string& a_origin, Counts& a_counts)
		{
			const auto skip = [&](std::string_view reason) {
				logger::warn("{}: {}", a_origin, reason);
				++a_counts.skipped;
			};
			if (!a_rule.is_object() || !a_rule.contains("match") || !a_rule["match"].is_object()) {
				skip("rule requires a match object");
				return;
			}
			const auto& match = a_rule["match"];
			for (const auto& [key, value] : match.items()) {
				if (key != "form" && key != "formType" && key != "keywordsAll" && key != "keywordsAny" &&
				    key != "plugin" && key != "iconLabel" && key != "status" && key != "parts" &&
				    key != "effectKeywordsAll" && key != "effectKeywordsAny" && key != "modelContains" &&
				    key != "museum" && key != "effectMagnitude" && key != "effectDuration" && key != "questItem" &&
				    key != "enchantment" && key != "newItem" && key != "knownEffects" &&
				    key != "knownEffect" && key != "upgrade" && key != "bestInClass" &&
				    key != "favorite") {
					skip(std::format("match key '{}' not supported yet", key));
					return;
				}
			}
			const bool hasForm = match.contains("form");
			const bool hasType = match.contains("formType");
			const bool hasKeywords = match.contains("keywordsAll") || match.contains("keywordsAny");
			const bool hasPlugin = match.contains("plugin");
			const bool hasLabel = match.contains("iconLabel");
			const bool hasStatus = match.contains("status");
			const bool hasParts = match.contains("parts");
			const bool hasEffect = match.contains("effectKeywordsAll") || match.contains("effectKeywordsAny");
			const bool hasModel = match.contains("modelContains");
			const bool hasRange = match.contains("effectMagnitude") || match.contains("effectDuration");
			const bool hasMuseum = match.contains("museum");
			const bool hasQuest = match.contains("questItem");
			const bool hasEnchant = match.contains("enchantment");
			const bool hasNew = match.contains("newItem");
			const bool hasKnown = match.contains("knownEffects");
			const bool hasKnownEffect = match.contains("knownEffect");
			const bool hasUpgrade = match.contains("upgrade");
			const bool hasBest = match.contains("bestInClass");
			const bool hasFavorite = match.contains("favorite");
			const bool hasReplace = a_rule.contains("replace");
			const bool hasRowKey = hasMuseum || hasQuest || hasEnchant || hasNew || hasKnown || hasKnownEffect || hasUpgrade || hasBest ||
				hasFavorite;
			const bool isStatusIcon = a_rule.contains("statusIcon");
			if (isStatusIcon && a_rule.contains("icon")) {
				skip("rule has both icon and statusIcon");
				return;
			}
			if (!isStatusIcon && !a_rule.contains("icon")) {
				skip("rule requires icon or statusIcon");
				return;
			}
			if (hasMuseum && !isStatusIcon) {
				skip("museum is only supported on statusIcon rules");
				return;
			}
			for (const auto& [present, key] : { std::pair{ hasQuest, "questItem" }, std::pair{ hasEnchant, "enchantment" },
				     std::pair{ hasNew, "newItem" }, std::pair{ hasKnown, "knownEffects" },
				     std::pair{ hasKnownEffect, "knownEffect" }, std::pair{ hasUpgrade, "upgrade" },
				     std::pair{ hasBest, "bestInClass" }, std::pair{ hasFavorite, "favorite" },
				     std::pair{ hasReplace, "replace" } }) {
				if (present && !isStatusIcon) {
					skip(std::format("{} is only supported on statusIcon rules", key));
					return;
				}
			}
			if (hasKnownEffect && hasReplace) {
				skip("knownEffect cannot combine with replace");
				return;
			}
			if (hasLabel && isStatusIcon) {
				skip("iconLabel is not supported on statusIcon rules yet");
				return;
			}
			if (hasLabel && match.size() > 1) {
				skip("iconLabel cannot combine with other match keys yet");
				return;
			}
			if (hasStatus && !hasForm && !hasType && !hasKeywords && !hasPlugin && !hasRowKey) {
				skip(isStatusIcon ? "status requires form, formType, keywords, plugin, museum, questItem, enchantment, newItem, knownEffects, knownEffect or upgrade" :
				                    "status requires form, formType, keywords or plugin");
				return;
			}
			if (!hasForm && !hasType && !hasKeywords && !hasPlugin && !hasLabel && !hasParts && !hasEffect && !hasModel &&
			    !hasRange && !hasRowKey) {
				skip("match requires form, formType, keywords, plugin, iconLabel, parts, effect keywords, modelContains, effect ranges, museum, questItem, enchantment, newItem, knownEffects, knownEffect or upgrade");
				return;
			}
			if (hasForm && hasPlugin) {
				skip("form already names its plugin");
				return;
			}
			if (hasForm && hasKeywords) {
				skip("form rules cannot combine with keywords yet");
				return;
			}
			if (hasForm && (hasParts || hasEffect || hasModel || hasRange)) {
				skip("form rules cannot combine with parts/effect keywords/modelContains/effect ranges yet");
				return;
			}
			if (hasForm && !match["form"].is_string() && !match["form"].is_array()) {
				skip("form must be a string or array of strings");
				return;
			}
			if (hasForm && match["form"].is_array() && match["form"].empty()) {
				skip("form array is empty");
				return;
			}
			std::vector<RE::FormType> types;
			if (hasType) {
				const auto& values = match["formType"];
				if ((!values.is_string() && !values.is_array()) || (values.is_array() && values.empty())) {
					skip("formType must be a string or non-empty array of strings");
					return;
				}
				const auto addType = [&](const nlohmann::json& value) {
					if (!value.is_string()) {
						skip("formType entry must be a string");
						return false;
					}
					const auto name = value.get<std::string>();
					const auto lower = Lower(name);
					const auto it = std::ranges::find(kFormTypes, lower, &std::pair<std::string_view, RE::FormType>::first);
					if (it == kFormTypes.end()) {
						skip(std::format("unknown formType '{}'", name));
						return false;
					}
					if (std::ranges::find(types, it->second) == types.end()) {
						types.push_back(it->second);
					}
					return true;
				};
				if (values.is_string()) {
					if (!addType(values)) {
						return;
					}
				} else {
					for (const auto& value : values) {
						if (!addType(value)) {
							return;
						}
					}
				}
			}
			std::vector<std::string> allIds;
			std::vector<std::string> anyIds;
			const auto readStrings = [&](const char* key, std::vector<std::string>& ids) {
				if (!match.contains(key)) {
					return true;
				}
				const auto& values = match[key];
				if ((!values.is_string() && !values.is_array()) || (values.is_array() && values.empty())) {
					skip(std::format("{} must be a string or non-empty array of strings", key));
					return false;
				}
				const auto add = [&](const nlohmann::json& value) {
					if (!value.is_string()) {
						skip(std::format("{} entry must be a string", key));
						return false;
					}
					ids.push_back(value.get<std::string>());
					return true;
				};
				if (values.is_string()) {
					return add(values);
				}
				for (const auto& value : values) {
					if (!add(value)) {
						return false;
					}
				}
				return true;
			};
			if (!readStrings("keywordsAll", allIds) || !readStrings("keywordsAny", anyIds)) {
				return;
			}
			std::vector<std::string> models;
			if (!readStrings("modelContains", models)) {
				return;
			}
			for (auto& needle : models) {
				if (needle.empty()) {
					skip("modelContains entry must be a non-empty string");
					return;
				}
				std::ranges::replace(needle, '/', '\\');
				needle = Lower(needle);
			}
			std::vector<std::string> effectAllIds;
			std::vector<std::string> effectAnyIds;
			if (!readStrings("effectKeywordsAll", effectAllIds) || !readStrings("effectKeywordsAny", effectAnyIds)) {
				return;
			}
			std::vector<std::string> statuses;
			if (!readStrings("status", statuses)) {
				return;
			}
			std::uint8_t status = 0;
			for (const auto& name : statuses) {
				const auto lower = Lower(name);
				if (lower == "empty") {
					status |= 1u;
				} else if (lower == "partial") {
					status |= 2u;
				} else if (lower == "full") {
					status |= 4u;
				} else {
					skip(std::format("unknown status '{}' (expected Empty, Partial or Full)", name));
					return;
				}
			}
			std::optional<float> magMin;
			std::optional<float> magMax;
			std::optional<double> durMin;
			std::optional<double> durMax;
			const auto readRange = [&](const char* key, auto& lo, auto& hi) {
				if (!match.contains(key)) {
					return true;
				}
				const auto bad = [&] {
					skip(std::format("{} must be an object with numeric min and/or max", key));
					return false;
				};
				const auto& value = match[key];
				if (!value.is_object() || value.empty()) {
					return bad();
				}
				for (const auto& [name, number] : value.items()) {
					if ((name != "min" && name != "max") || !number.is_number()) {
						return bad();
					}
					// float for magnitude, double for duration; read as the target type so MSVC doesn't warn C4244.
					(name == "min" ? lo : hi) = number.get<typename std::remove_reference_t<decltype(lo)>::value_type>();
				}
				return !(lo && hi && *lo > *hi) || bad();
			};
			if (!readRange("effectMagnitude", magMin, magMax) || !readRange("effectDuration", durMin, durMax)) {
				return;
			}
			std::vector<std::string> museumNames;
			if (!readStrings("museum", museumNames)) {
				return;
			}
			std::uint8_t museum = 0;
			for (const auto& name : museumNames) {
				const auto lower = Lower(name);
				if (lower == "new") {
					museum |= 1u;
				} else if (lower == "found") {
					museum |= 2u;
				} else if (lower == "displayed") {
					museum |= 4u;
				} else {
					skip(std::format("unknown museum state '{}' (expected New, Found or Displayed)", name));
					return;
				}
			}
			if (hasQuest && !(match["questItem"].is_boolean() && match["questItem"].get<bool>())) {
				skip("questItem must be true");
				return;
			}
			if (hasNew && !(match["newItem"].is_boolean() && match["newItem"].get<bool>())) {
				skip("newItem must be true");
				return;
			}
			if (hasUpgrade && !(match["upgrade"].is_boolean() && match["upgrade"].get<bool>())) {
				skip("upgrade must be true");
				return;
			}
			if (hasBest && !(match["bestInClass"].is_boolean() && match["bestInClass"].get<bool>())) {
				skip("bestInClass must be true");
				return;
			}
			if (hasFavorite && !(match["favorite"].is_boolean() && match["favorite"].get<bool>())) {
				skip("favorite must be true");
				return;
			}
			std::uint8_t knownEffects = 0;
			if (hasKnown) {
				const auto& values = match["knownEffects"];
				const auto addKnown = [&](const nlohmann::json& value) {
					if (!value.is_number_integer() || value.get<std::int64_t>() < 0 || value.get<std::int64_t>() > 4) {
						return false;
					}
					knownEffects |= static_cast<std::uint8_t>(1u << value.get<std::uint32_t>());
					return true;
				};
				bool valid = values.is_number_integer() ? addKnown(values) : values.is_array() && !values.empty();
				if (values.is_array()) {
					for (const auto& value : values) {
						valid = valid && addKnown(value);
					}
				}
				if (!valid) {
					skip("knownEffects must be 0-4 or a non-empty array of 0-4");
					return;
				}
			}
			std::optional<EffectMatch> knownEffect;
			if (hasKnownEffect) {
				const auto& object = match["knownEffect"];
				if (!object.is_object() || object.empty()) {
					skip("knownEffect must be a non-empty object");
					return;
				}
				EffectMatch effect;
				const auto readNames = [&](const char* key, std::vector<std::string>& names) {
					const auto& values = object[key];
					if ((!values.is_string() && !values.is_array()) || (values.is_array() && values.empty())) {
						skip(std::format("knownEffect {} must be a string or non-empty array of strings", key));
						return false;
					}
					const auto add = [&](const nlohmann::json& value) {
						if (!value.is_string()) {
							skip(std::format("knownEffect {} entry must be a string", key));
							return false;
						}
						names.push_back(value.get<std::string>());
						return true;
					};
					if (values.is_string()) {
						return add(values);
					}
					return std::ranges::all_of(values, add);
				};
				for (const auto& [key, value] : object.items()) {
					if (key != "effectId" && key != "archetype" && key != "primaryValue" && key != "detrimental") {
						skip(std::format("knownEffect key '{}' not supported", key));
						return;
					}
				}
				if (object.contains("effectId")) {
					const auto& id = object["effectId"];
					if (!id.is_string()) {
						skip("knownEffect effectId must be a form string");
						return;
					}
					effect.effectId = ResolveForm(id.get<std::string>(), a_origin);
					const auto* base = effect.effectId ? RE::TESForm::LookupByID<RE::EffectSetting>(effect.effectId) : nullptr;
					if (!base) {
						skip("knownEffect effectId must name a magic effect");
						return;
					}
				}
				if (object.contains("archetype")) {
					std::vector<std::string> names;
					if (!readNames("archetype", names)) {
						return;
					}
					for (const auto& name : names) {
						const auto lower = Lower(name);
						const auto it = std::ranges::find(kArchetypeNames, std::string_view{ lower });
						if (it == kArchetypeNames.end()) {
							skip(std::format("unknown archetype '{}'", name));
							return;
						}
						effect.archetypes.push_back(static_cast<RE::EffectArchetypes::ArchetypeID>(it - kArchetypeNames.begin()));
					}
				}
				if (object.contains("primaryValue")) {
					std::vector<std::string> names;
					if (!readNames("primaryValue", names)) {
						return;
					}
					for (const auto& name : names) {
						// The lookup returns kTotal, not kNone, for unknown names. Range check only: GetActorValueName returns display names, so comparing names rejects valid values.
						const auto value = RE::ActorValueList::LookupActorValueByName(name.c_str());
						const auto index = std::to_underlying(value);
						if (index < 0 || index >= std::to_underlying(RE::ActorValue::kTotal)) {
							skip(std::format("unknown actor value '{}' (engine returned {})", name, index));
							return;
						}
						effect.values.push_back(value);
					}
				}
				if (object.contains("detrimental")) {
					if (!object["detrimental"].is_boolean()) {
						skip("knownEffect detrimental must be true or false");
						return;
					}
					effect.detrimental = object["detrimental"].get<bool>();
				}
				knownEffect = std::move(effect);
			}
			std::vector<std::string> enchantNames;
			if (!readStrings("enchantment", enchantNames)) {
				return;
			}
			std::uint8_t enchantment = 0;
			for (const auto& name : enchantNames) {
				const auto lower = Lower(name);
				if (lower == "known") {
					enchantment |= 1u;
				} else if (lower == "unknown") {
					enchantment |= 2u;
				} else if (lower == "nodisenchant") {
					enchantment |= 4u;
				} else if (lower == "ability") {
					enchantment |= 8u;
				} else {
					skip(std::format("unknown enchantment state '{}' (expected Known, Unknown, NoDisenchant or Ability)", name));
					return;
				}
			}
			std::uint8_t replace = 0;
			if (hasReplace) {
				constexpr std::array<std::string_view, 6> kReplaceNames{
					"bestinclass"sv, "favorite"sv, "poisoned"sv, "stolen"sv, "enchanted"sv, "read"sv
				};
				const auto& value = a_rule["replace"];
				const auto lower = value.is_string() ? Lower(value.get<std::string>()) : std::string{};
				const auto it = std::ranges::find(kReplaceNames, std::string_view{ lower });
				if (it == kReplaceNames.end()) {
					skip("replace must be one of bestInClass, favorite, poisoned, stolen, enchanted, read");
					return;
				}
				replace = static_cast<std::uint8_t>(it - kReplaceNames.begin() + 1);
			}
			if (hasMuseum && !TCCItemStatus::Loaded()) {
				++g_museumIgnored;
				return;
			}
			std::uint32_t parts = 0;
			if (hasParts) {
				const auto& values = match["parts"];
				if ((!values.is_number_integer() && !values.is_array()) || (values.is_array() && values.empty())) {
					skip("parts must be a slot number or non-empty array of slot numbers");
					return;
				}
				const auto addPart = [&](const nlohmann::json& value) {
					if (!value.is_number_integer() ||
						(value.is_number_unsigned() && (value.get<std::uint64_t>() < 30 || value.get<std::uint64_t>() > 61)) ||
						(!value.is_number_unsigned() && (value.get<std::int64_t>() < 30 || value.get<std::int64_t>() > 61))) {
						skip("parts entry must be an integer slot number from 30 to 61");
						return false;
					}
					parts |= std::uint32_t{ 1 } << (value.get<std::uint32_t>() - 30);
					return true;
				};
				if (values.is_number_integer()) {
					if (!addPart(values)) {
						return;
					}
				} else {
					for (const auto& value : values) {
						if (!addPart(value)) {
							return;
						}
					}
				}
			}
			std::vector<std::string> labels;
			if (!readStrings("iconLabel", labels)) {
				return;
			}
			std::vector<std::string> pluginNames;
			std::vector<const RE::TESFile*> plugins;
			if (!readStrings("plugin", pluginNames)) {
				return;
			}
			if (hasPlugin) {
				auto* handler = RE::TESDataHandler::GetSingleton();
				for (const auto& name : pluginNames) {
					const auto* file = handler ? handler->LookupModByName(name) : nullptr;
					if (!file || file->compileIndex == 0xFF) {
						logger::warn("{}: plugin not loaded '{}'", a_origin, name);
						continue;
					}
					plugins.push_back(file);
				}
				if (plugins.empty()) {
					skip("no listed plugin is loaded");
					return;
				}
			}
			const auto* outKey = isStatusIcon ? "statusIcon" : "icon";
			if (!a_rule[outKey].is_string()) {
				skip(std::format("{} must be a string", outKey));
				return;
			}
			auto icon = a_rule[outKey].get<std::string>();
			if (!ValidIcon(icon)) {
				skip(std::format("bad {} path '{}'", outKey, icon));
				return;
			}
			const auto fullPath = std::string(kIconRoot) + icon;
			if (!TextureManager::Exists(fullPath)) {
				skip(std::format("icon not found '{}' (rule skipped, lower rules apply)", fullPath));
				return;
			}
			std::int32_t priority = hasForm ? kFormTier : (hasKeywords || hasParts || hasEffect || hasModel || hasRange) ? kKeywordTier : hasPlugin ? kPluginTier : hasLabel ? kLabelTier : kTypeTier;
			if (a_rule.contains("priority")) {
				const auto& number = a_rule["priority"];
				if (!number.is_number_integer() ||
				    (number.is_number_unsigned() && number.get<std::uint64_t>() > INT32_MAX) ||
				    (!number.is_number_unsigned() && (number.get<std::int64_t>() < INT32_MIN || number.get<std::int64_t>() > INT32_MAX))) {
					skip("priority must be a 32-bit integer");
					return;
				}
				priority = number.get<std::int32_t>();
			}

			if (hasLabel) {
				g_rules.push_back(Rule{ fullPath, priority, ++g_order, a_origin });
				const auto* rule = &g_rules.back();
				std::size_t added = 0;
				for (const auto& label : labels) {
					if (label.empty()) {
						logger::warn("{}: empty iconLabel entry skipped", a_origin);
						continue;
					}
					const auto key = Lower(label);
					const auto it = g_byLabel.find(key);
					if (it == g_byLabel.end() || Better(rule, it->second)) {
						g_byLabel.insert_or_assign(key, rule);
					}
					++added;
				}
				if (added) {
					++a_counts.rules;
					++a_counts.labelRules;
				}
				return;
			}

			const auto pushStatusIcon = [&](std::vector<RE::FormID> a_forms) {
				g_rules.push_back(Rule{ fullPath, priority, ++g_order, a_origin,
					types, std::move(allIds), std::move(anyIds), {}, {}, false, std::move(plugins) });
				auto& rule = g_rules.back();
				rule.parts = parts;
				rule.models = std::move(models);
				rule.effectAllIds = std::move(effectAllIds);
				rule.effectAnyIds = std::move(effectAnyIds);
				rule.status = status;
				rule.forms = std::move(a_forms);
				rule.statusIcon = true;
				rule.museum = museum;
				rule.magMin = magMin;
				rule.magMax = magMax;
				rule.durMin = durMin;
				rule.durMax = durMax;
				rule.questItem = hasQuest;
				rule.newItem = hasNew;
				rule.knownEffects = knownEffects;
				rule.knownEffect = std::move(knownEffect);
				rule.upgrade = hasUpgrade;
				rule.bestInClass = hasBest;
				rule.favorite = hasFavorite;
				rule.enchantment = enchantment;
				rule.replace = replace;
				g_statusIconRules.push_back(&rule);
				g_museumRules += museum != 0;
				g_liveRules += hasQuest || hasEnchant || hasUpgrade;
				g_newRules += hasNew;
				g_knownEffectRules += hasKnown || hasKnownEffect;
				g_effectRules += hasKnownEffect;
				g_upgradeRules += hasUpgrade;
				a_counts.forms += rule.forms.size();
				++a_counts.rules;
				++a_counts.statusIconRules;
			};

			if (!hasForm) {
				if (isStatusIcon) {
					pushStatusIcon({});
					return;
				}
				g_rules.push_back(Rule{ fullPath, priority, ++g_order, a_origin,
					types, std::move(allIds), std::move(anyIds), {}, {}, false, std::move(plugins) });
				auto& rule = g_rules.back();
				rule.parts = parts;
				rule.models = std::move(models);
				rule.effectAllIds = std::move(effectAllIds);
				rule.effectAnyIds = std::move(effectAnyIds);
				rule.status = status;
				rule.magMin = magMin;
				rule.magMax = magMax;
				rule.durMin = durMin;
				rule.durMax = durMax;
				if (hasStatus) {
					// Filled instances share a form ID; keep these out of the per-form buckets.
					g_statusRules.push_back(&rule);
					++a_counts.rules;
					++a_counts.statusRules;
					return;
				}
				for (const auto type : types) {
					g_byType[type].push_back(&g_rules.back());
				}
				if (types.empty()) {
					g_anyType.push_back(&g_rules.back());
				}
				++a_counts.rules;
				if (hasType) {
					++a_counts.typeRules;
				}
				if (hasKeywords || hasParts || hasEffect || hasModel || hasRange) {
					++a_counts.keywordRules;
				}
				if (hasPlugin) {
					++a_counts.pluginRules;
				}
				return;
			}

			std::vector<RE::FormID> resolved;
			const auto add = [&](const nlohmann::json& value) {
				if (!value.is_string()) {
					skip("form entry must be a string");
					return;
				}
				const auto formID = ResolveForm(value.get<std::string>(), a_origin);
				if (!formID) {
					++a_counts.skipped;
					return;
				}
				if (hasType) {
					const auto type = RE::TESForm::LookupByID(formID)->GetFormType();
					if (std::ranges::find(types, type) == types.end()) {
						const auto it = std::ranges::find(kFormTypes, type, &std::pair<std::string_view, RE::FormType>::second);
						skip(std::format("form '{}' is {}, not in formType", value.get<std::string>(),
							it != kFormTypes.end() ? std::string(it->first) : std::to_string(static_cast<std::uint32_t>(type))));
						return;
					}
				}
				resolved.push_back(formID);
			};
			const auto& forms = match["form"];
			if (forms.is_string()) {
				add(forms);
			} else {
				for (const auto& form : forms) {
					add(form);
				}
			}
			if (!resolved.empty()) {
				if (isStatusIcon) {
					pushStatusIcon(std::move(resolved));
					return;
				}
				if (hasStatus) {
					g_rules.push_back(Rule{ fullPath, priority, ++g_order, a_origin, types });
					auto& rule = g_rules.back();
					rule.status = status;
					rule.forms = std::move(resolved);
					g_statusRules.push_back(&rule);
					a_counts.forms += rule.forms.size();
					++a_counts.rules;
					++a_counts.statusRules;
					return;
				}
				g_rules.push_back(Rule{ fullPath, priority, ++g_order, a_origin,
					types, {}, {}, {}, {}, false, {}, true });
				const auto* rule = &g_rules.back();
				for (const auto formID : resolved) {
					++a_counts.forms;
					const auto it = g_byForm.find(formID);
					if (it == g_byForm.end() || Better(rule, it->second)) {
						if (it != g_byForm.end()) {
							logger::debug("{}: form {:08X} replaces {} (priority {} -> {})", a_origin, formID,
								it->second->origin, it->second->priority, priority);
						}
						g_byForm.insert_or_assign(formID, rule);
					}
				}
				++a_counts.rules;
			}
		}

		void LoadCategoryIcons(const nlohmann::json& a_map, const std::string& a_file)
		{
			if (!a_map.is_object()) {
				logger::warn("{}: categoryIcons must be an object of label -> PNG path", a_file);
				return;
			}
			for (const auto& [label, value] : a_map.items()) {
				std::string icon = value.is_string() ? value.get<std::string>() : std::string{};
				if (label.empty() || !ValidIcon(icon)) {
					logger::warn("{}: categoryIcons '{}' skipped: bad PNG path", a_file, label);
					continue;
				}
				auto fullPath = std::string(kIconRoot) + icon;
				if (!TextureManager::Exists(fullPath)) {
					logger::warn("{}: categoryIcons '{}' skipped: icon not found '{}'", a_file, label, fullPath);
					continue;
				}
				g_categoryIcons.insert_or_assign(Lower(label), std::move(fullPath));
			}
		}

		void LoadFile(const std::filesystem::path& a_path, Counts& a_total)
		{
			const auto name = a_path.filename().string();
			std::ifstream stream(a_path);
			if (!stream) {
				logger::warn("{}: cannot open rule file", name);
				++a_total.skipped;
				return;
			}
			const auto json = nlohmann::json::parse(stream, nullptr, false, true);
			const bool hasCategories = json.is_object() && json.contains("categoryIcons");
			if (json.is_discarded() || !json.is_object() || !json.contains("version") ||
			    !json["version"].is_number_integer() || json["version"] != 1 ||
			    (json.contains("rules") ? !json["rules"].is_array() : !hasCategories)) {
				logger::warn("{}: invalid JSON or schema (requires integer version 1 and rules array)", name);
				++a_total.skipped;
				return;
			}
			if (hasCategories) {
				LoadCategoryIcons(json["categoryIcons"], name);
			}
			Counts counts;
			std::size_t index = 0;
			for (const auto& rule : json.value("rules", nlohmann::json::array())) {
				LoadRule(rule, std::format("{}#{}", name, ++index), counts);
			}
			logger::info("{}: {} rules, {} forms, {} skipped", name, counts.rules, counts.forms, counts.skipped);
			a_total.rules += counts.rules;
			a_total.forms += counts.forms;
			a_total.typeRules += counts.typeRules;
			a_total.keywordRules += counts.keywordRules;
			a_total.pluginRules += counts.pluginRules;
			a_total.labelRules += counts.labelRules;
			a_total.statusRules += counts.statusRules;
			a_total.statusIconRules += counts.statusIconRules;
			a_total.skipped += counts.skipped;
		}
	}

	void Load()
	{
		static bool loaded = false;
		if (loaded) {
			return;
		}
		loaded = true;
		const auto start = g_debug.logTiming ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
		std::error_code ec;
		std::vector<std::filesystem::path> files;
		for (std::filesystem::directory_iterator it{ kRuleFolder, ec }, end; !ec && it != end; it.increment(ec)) {
			std::error_code fileError;
			if (it->is_regular_file(fileError) && Lower(it->path().extension().string()) == ".json") {
				files.push_back(it->path());
			}
			if (fileError) {
				logger::warn("{}: cannot inspect rule file: {}", it->path().string(), fileError.message());
			}
		}
		if (ec) {
			logger::warn("{}: cannot enumerate rules: {}", kRuleFolder, ec.message());
		}
		std::ranges::sort(files, [](const auto& a, const auto& b) {
			const auto aName = a.filename().string();
			const auto bName = b.filename().string();
			const auto aLower = Lower(aName);
			const auto bLower = Lower(bName);
			return aLower == bLower ? aName < bName : aLower < bLower;
		});
		Counts total;
		for (const auto& file : files) {
			LoadFile(file, total);
		}
		for (auto& [type, rules] : g_byType) {
			std::ranges::sort(rules, [](const Rule* a, const Rule* b) { return Better(a, b); });
		}
		std::ranges::sort(g_anyType, [](const Rule* a, const Rule* b) { return Better(a, b); });
		std::ranges::sort(g_statusRules, [](const Rule* a, const Rule* b) { return Better(a, b); });
		std::ranges::sort(g_statusIconRules, [](const Rule* a, const Rule* b) { return Better(a, b); });
		logger::info("IconRules: {} files, {} rules, {} resolved forms, {} unique forms, {} type rules, {} keyword rules, {} plugin rules, {} label rules, {} status rules, {} status icon rules, {} live status rules, {} new-item status rules, {} known-effect status rules, {} effect status rules, {} skipped",
			files.size(), total.rules, total.forms, g_byForm.size(), total.typeRules, total.keywordRules, total.pluginRules, total.labelRules, total.statusRules,
			total.statusIconRules, g_liveRules, g_newRules, g_knownEffectRules, g_effectRules, total.skipped);
		logger::info("IconRules: {} category icons", g_categoryIcons.size());
		if (g_museumIgnored) {
			logger::info("IconRules: {} museum rules ignored (TCC unavailable)", g_museumIgnored);
		}
		if (g_debug.logTiming) {
			logger::info("IconRules load {:.2f} ms", std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - start).count());
		}
	}

	std::vector<std::string> IconPaths()
	{
		std::vector<const Rule*> broad;
		for (const auto& rule : g_rules) {
			if (!rule.exactForm && !rule.statusIcon) {
				broad.push_back(&rule);
			}
		}
		std::ranges::sort(broad, [](const Rule* a, const Rule* b) { return Better(a, b); });
		std::vector<std::string> paths;
		std::unordered_set<std::string> seen;
		const auto add = [&](const Rule& rule) {
			if (seen.insert(rule.path).second) {
				paths.push_back(rule.path);
			}
		};
		for (const auto* rule : broad) {
			add(*rule);
		}
		for (const auto& rule : g_rules) {
			if (rule.exactForm && !rule.statusIcon) {
				add(rule);
			}
		}
		return paths;
	}

	std::vector<std::string> StatusIconPaths()
	{
		std::vector<std::string> paths;
		std::unordered_set<std::string> seen;
		for (const auto* rule : g_statusIconRules) {
			if (seen.insert(rule->path).second) {
				paths.push_back(rule->path);
			}
		}
		return paths;
	}

	void PrepareKeywords()
	{
		if (!g_keywordsResolved) {
			ResolveKeywords();
		}
	}

	void SetDebug(bool a_matches, bool a_timing)
	{
		g_debug = Settings{ a_matches, a_timing };
	}

	void SetIngredientIcons(bool a_progress, std::uint32_t a_maxEffects, std::uint32_t a_harmTint)
	{
		g_showProgress = a_progress;
		g_maxEffectIcons = (std::min)(a_maxEffects, kMaxEffectIcons);
		g_harmTint = a_harmTint;
	}

	Timer TakeResolveTiming()
	{
		const auto timer = g_resolveTimer;
		g_resolveTimer = {};
		return timer;
	}

	bool HasLabelRules()
	{
		return !g_byLabel.empty();
	}

	bool HasStatusRules()
	{
		return !g_statusRules.empty();
	}

	bool HasStatusIconRules()
	{
		return !g_statusIconRules.empty();
	}

	bool HasMuseumRules()
	{
		return g_museumRules != 0;
	}

	bool HasLiveRules()
	{
		return g_liveRules != 0;
	}

	bool HasNewRules()
	{
		return g_newRules != 0;
	}

	bool HasKnownEffectRules()
	{
		return g_knownEffectRules != 0 && (g_showProgress || g_maxEffectIcons != 0);
	}

	bool HasUpgradeRules()
	{
		return g_upgradeRules != 0;
	}

	void FindStatusIcons(RE::FormID a_formID, std::int32_t a_soulStatus, std::int32_t a_museum, std::uint32_t a_live,
		std::vector<StatusIconMatch>& a_out)
	{
		a_out.clear();
		if (g_statusIconRules.empty() || !a_formID) {
			return;
		}
		const auto soulBit = a_soulStatus >= 0 && a_soulStatus <= 2 ? static_cast<std::uint8_t>(1u << a_soulStatus) : std::uint8_t{ 0 };
		const auto museumBit = a_museum >= 1 && a_museum <= 3 ? static_cast<std::uint8_t>(1u << (a_museum - 1)) : std::uint8_t{ 0 };
		const auto enchant = (a_live >> kLiveEnchShift) & kLiveEnchMask;
		// Ability (bit 3) is its own live flag: the state field is full and an ability item has no enchantment state.
		const auto enchantBit = enchant ? static_cast<std::uint8_t>(1u << (enchant - 1)) :
		                        (a_live & kLiveAbility) ? std::uint8_t{ 8 } : std::uint8_t{ 0 };
		const auto known = (a_live >> kLiveKnownShift) & kLiveKnownMask;
		const auto knownBit = known ? static_cast<std::uint8_t>(1u << (known - 1)) : std::uint8_t{ 0 };
		const auto rowKeysFail = [&](const Rule& a_rule) {
			return (a_rule.status && !(a_rule.status & soulBit)) || (a_rule.museum && !(a_rule.museum & museumBit)) ||
			       (a_rule.questItem && !(a_live & kLiveQuest)) || (a_rule.enchantment && !(a_rule.enchantment & enchantBit)) ||
			       (a_rule.newItem && !(a_live & kLiveNew)) || (a_rule.knownEffects && !(a_rule.knownEffects & knownBit)) ||
			       (a_rule.upgrade && !(a_live & kLiveUpgrade)) || (a_rule.bestInClass && !(a_live & kLiveBest)) ||
			       (a_rule.favorite && !(a_live & kLiveFavorite));
		};
		std::uint8_t taken = 0;
		bool capped = false;
		const auto& candidates = StatusCandidates(a_formID);
		for (const auto* rule : candidates) {
			if (rule->knownEffect || (rule->knownEffects && !g_showProgress) || rowKeysFail(*rule)) {
				continue;
			}
			if (rule->replace && (taken & (1u << (rule->replace - 1)))) {
				continue;
			}
			if (std::ranges::any_of(a_out, [&](const StatusIconMatch& a_match) { return *a_match.path == rule->path; })) {
				continue;
			}
			if (a_out.size() == kMaxStatusIcons) {
				capped = true;
				break;
			}
			if (rule->replace) {
				taken |= static_cast<std::uint8_t>(1u << (rule->replace - 1));
			}
			a_out.push_back({ &rule->path, rule->replace, 0 });
		}
		const auto effectMask = static_cast<std::uint8_t>((a_live >> kLiveEffectShift) & kLiveEffectMask);
		if (effectMask && g_maxEffectIcons && !capped) {
			const auto* ingredient = RE::TESForm::LookupByID<RE::IngredientItem>(a_formID);
			std::uint32_t effects = 0;
			for (std::uint32_t i = 0; ingredient && i < 4 && effects < g_maxEffectIcons && !capped; ++i) {
				if (!(effectMask & (1u << i))) {
					continue;
				}
				for (const auto* rule : candidates) {
					if (!rule->knownEffect || rowKeysFail(*rule) ||
						!(EffectIndexMask(*rule->knownEffect, *ingredient) & (1u << i))) {
						continue;
					}
						const auto* base = ingredient->effects[i] ? ingredient->effects[i]->baseEffect : nullptr;
						const auto tint = base && base->IsDetrimental() && !rule->knownEffect->detrimental ? g_harmTint : 0u;
						if (std::ranges::none_of(a_out, [&](const StatusIconMatch& a_match) {
								return *a_match.path == rule->path && a_match.tint == tint;
							})) {
						if (a_out.size() == kMaxStatusIcons) {
							capped = true;
							break;
						}
						a_out.push_back({ &rule->path, 0, tint });
						++effects;
					}
					break;
				}
			}
		}
		const auto soulKey = static_cast<std::uint64_t>(std::clamp(a_soulStatus, -1, 2) + 1);
		const auto museumKey = static_cast<std::uint64_t>(std::clamp(a_museum, 0, 3));
		const auto liveKey = std::uint64_t{ a_live & 0x7FFFu };
		if (g_debug.logMatches &&
			g_loggedStatusIcons.insert((std::uint64_t{ a_formID } << 32) | (liveKey << 4) | (museumKey << 2) | soulKey).second) {
			const auto live = a_live ? std::format(" (live {})", a_live) : std::string{};
			if (a_out.empty()) {
				logger::info("statusIcon {:08X}{} -> none", a_formID, live);
			} else {
				constexpr std::array<std::string_view, 6> kReplaceTags{
					"bestInClass"sv, "favorite"sv, "poisoned"sv, "stolen"sv, "enchanted"sv, "read"sv
				};
				std::string list;
				for (const auto& match : a_out) {
					if (!list.empty()) {
						list += ", ";
					}
					list += *match.path;
					if (match.replace) {
						list += std::format("[replace {}]", kReplaceTags[match.replace - 1]);
					}
				}
				logger::info("statusIcon {:08X}{} -> {}", a_formID, live, list);
			}
			if (capped) {
				logger::info("statusIcon {:08X} capped at {}", a_formID, kMaxStatusIcons);
			}
		}
	}

	const std::string* Find(RE::FormID a_formID, std::string_view a_label, std::int32_t a_status)
	{
		const auto* rule = Best(a_formID, a_label, a_status);
		return rule ? &rule->path : nullptr;
	}

	const std::string* FindOrigin(RE::FormID a_formID, std::string_view a_label, std::int32_t a_status)
	{
		const auto* rule = Best(a_formID, a_label, a_status);
		return rule ? &rule->origin : nullptr;
	}

	const std::string* FindCategoryIcon(std::string_view a_label)
	{
		const auto it = g_categoryIcons.find(Lower(a_label));
		return it != g_categoryIcons.end() ? &it->second : nullptr;
	}

	void ClearCache()
	{
		g_cache.clear();
		g_loggedLabels.clear();
		g_loggedStatus.clear();
		g_statusIconCache.clear();
		g_loggedStatusIcons.clear();
	}

	void LogStats()
	{
		const auto matched = std::ranges::count_if(g_cache, [](const auto& entry) { return entry.second != nullptr; });
		logger::info("IconRules cache: {} forms, {} matched, {} lookups", g_cache.size(), matched, g_lookups);
	}
}
