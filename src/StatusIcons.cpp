#include "StatusIcons.h"
#include "IconRules.h"
#include "NewItems.h"
#include "TCCItemStatus.h"
#include "TextureManager.h"

namespace StatusIcons
{
	namespace
	{
		constexpr auto kProto = "_global.InventoryListEntry.prototype";
		constexpr auto kItemList = "_root.Menu_mc.inventoryLists.itemList";
		constexpr std::array<std::string_view, 4> kMenus{
			RE::InventoryMenu::MENU_NAME, RE::ContainerMenu::MENU_NAME,
			RE::BarterMenu::MENU_NAME, RE::GiftMenu::MENU_NAME
		};
		constexpr auto kOriginal = "pngii_origFormatName";
		constexpr auto kInstalled = "pngii_statusInstalled";
		constexpr std::int32_t kStatusDepth = 5001;
		constexpr double kFallbackEntryHeight = 24.0;
		constexpr double kLoadedImageSize = 64.0;
		constexpr std::uint32_t kLoggedCalls = 8;
		struct ChildKeys
		{
			std::string name;
			std::string url;
			std::string loader;
			std::string show;
			std::string tint;
		};
		const std::array<ChildKeys, IconRules::kMaxStatusIcons> kChildren = [] {
			std::array<ChildKeys, IconRules::kMaxStatusIcons> keys;
			for (std::size_t i = 0; i < keys.size(); ++i) {
				const auto name = std::format("icon{}", i);
				keys[i] = { name, "url_" + name, "loader_" + name, "show_" + name, "tint_" + name };
			}
			return keys;
		}();
		std::vector<std::string> g_prewarmPaths;
		bool g_logTiming = false;
		IconRules::Timer g_timer;
		using Clock = std::chrono::steady_clock;
		std::uint32_t g_lastPx = 0;
		double g_sizeFraction = 0.5;
		std::string_view g_openMenu;

		void RunPrewarm(std::uint32_t a_px, std::size_t a_next, std::uint32_t a_added, std::uint64_t a_workNs)
		{
			SKSE::GetTaskInterface()->AddUITask([a_px, a_next, a_added, a_workNs]() mutable {
				const auto start = Clock::now();
				if (a_next < g_prewarmPaths.size()) {
					const auto before = TextureManager::GetStats();
					TextureManager::GetURL(g_prewarmPaths[a_next++], a_px);
					const auto after = TextureManager::GetStats();
					a_added += (after.scaled + after.loadPNG) - (before.scaled + before.loadPNG);
					a_workNs += std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
					RunPrewarm(a_px, a_next, a_added, a_workNs);
					return;
				}
				logger::info("prewarm status: {} px, {} new, {:.1f} ms work", a_px, a_added, a_workNs / 1e6);
			});
		}

		double Num(const RE::GFxValue& a_obj, const char* a_member, double a_default)
		{
			RE::GFxValue v;
			return a_obj.GetMember(a_member, &v) && v.IsNumber() ? v.GetNumber() : a_default;
		}

		// SkyUI tests `x == true`, which AS2 also passes for the number 1; a bool-only check drew icons over the favorite icon.
		bool Flag(const RE::GFxValue& a_obj, const char* a_member)
		{
			RE::GFxValue v;
			if (!a_obj.GetMember(a_member, &v)) {
				return false;
			}
			if (v.IsBool()) {
				return v.GetBool();
			}
			if (v.IsNumber()) {
				return v.GetNumber() == 1.0;
			}
			return false;
		}

		double EntryHeight(const RE::GFxValue& a_state)
		{
			RE::GFxValue list;
			RE::GFxValue layout;
			double height = 0.0;
			if (a_state.IsObject() && a_state.GetMember("list", &list) && list.IsObject() &&
				list.GetMember("layout", &layout) && layout.IsObject()) {
				height = Num(layout, "entryHeight", 0.0);
			}
			static bool logged = false;
			if (!logged) {
				logged = true;
				logger::debug("status entryHeight {:.1f}{}", height, height > 0.0 ? "" : " (missing, using fallback)");
			}
			return height > 0.0 ? height : kFallbackEntryHeight;
		}

		constexpr std::array<const char*, 6> kSkyUIIcons{
			"bestIcon", "favoriteIcon", "poisonIcon", "stolenIcon", "enchIcon", "readIcon"
		};

		bool SkyUIShown(std::size_t a_index, const RE::GFxValue& a_entry, const RE::GFxValue& a_state)
		{
			switch (a_index) {
			case 0:
				return Flag(a_entry, "bestInClass");
			case 1:
				return Flag(a_entry, "favorite");
			case 2:
				return Flag(a_entry, "isPoisoned");
			case 3:
				return (Flag(a_entry, "isStolen") || Flag(a_entry, "isStealing")) && Flag(a_state, "showStolenIcon");
			case 4:
				return Flag(a_entry, "isEnchanted");
			case 5:
				return Flag(a_entry, "isRead");
			default:
				return false;
			}
		}

		std::uint32_t ShownStatusIcons(const RE::GFxValue& a_entry, const RE::GFxValue& a_state)
		{
			std::uint32_t n = 0;
			for (std::size_t i = 0; i < kSkyUIIcons.size(); ++i) {
				n += SkyUIShown(i, a_entry, a_state);
			}
			return n;
		}

		const RE::BGSKeyword* g_disallowEnchanting = nullptr;

		std::uint32_t EnchantState(const RE::EnchantmentItem* a_enchantment)
		{
			if (!a_enchantment) {
				return 0;
			}
			for (const auto* enchantment : std::array<const RE::EnchantmentItem*, 2>{ a_enchantment, a_enchantment->data.baseEnchantment }) {
				if (!enchantment) {
					continue;
				}
				if (g_disallowEnchanting && enchantment->HasKeyword(g_disallowEnchanting)) {
					return 3;
				}
				if (enchantment->GetFormFlags() & RE::TESForm::RecordFlags::kKnown) {
					return 1;
				}
				// Player-made enchantments are built from known enchantments but carry no kKnown flag or known base.
				if (enchantment->IsDynamicForm()) {
					return 1;
				}
			}
			return 2;
		}

		bool g_upgradeRules = false;

		enum class WeaponGroup
		{
			kNone,
			kMelee,
			kRanged
		};

		WeaponGroup GroupOf(const RE::TESObjectWEAP* a_weapon)
		{
			if (!a_weapon) {
				return WeaponGroup::kNone;
			}
			switch (a_weapon->GetWeaponType()) {
			case RE::WEAPON_TYPE::kBow:
			case RE::WEAPON_TYPE::kCrossbow:
				return WeaponGroup::kRanged;
			case RE::WEAPON_TYPE::kStaff:
			case RE::WEAPON_TYPE::kHandToHandMelee:
				return WeaponGroup::kNone;
			default:
				return WeaponGroup::kMelee;
			}
		}

		struct Baseline
		{
			std::vector<std::pair<std::uint32_t, float>> worn;
			float melee = -1.0f;
			float ranged = -1.0f;
		};
		Baseline g_baseline;
		Clock::time_point g_lastEntry{};
		constexpr auto kListGap = std::chrono::milliseconds(2);

		void RebuildBaseline(RE::PlayerCharacter* a_player)
		{
			g_baseline = {};
			// Actor::GetWornArmor builds a full GetInventory() map per call, so walk the entry list once.
			auto* changes = a_player->GetInventoryChanges();
			if (changes && changes->entryList) {
				for (auto* entry : *changes->entryList) {
					const auto* armor = entry && entry->object ? entry->object->As<RE::TESObjectARMO>() : nullptr;
					if (armor && entry->IsWorn()) {
						g_baseline.worn.emplace_back(armor->GetSlotMask().underlying(), std::round(a_player->GetArmorValue(entry)));
					}
				}
			}
			for (const bool left : { false, true }) {
				auto* entry = a_player->GetEquippedEntryData(left);
				const auto* weapon = entry && entry->object ? entry->object->As<RE::TESObjectWEAP>() : nullptr;
				const auto group = GroupOf(weapon);
				if (group == WeaponGroup::kNone) {
					continue;
				}
				auto& best = group == WeaponGroup::kMelee ? g_baseline.melee : g_baseline.ranged;
				best = (std::max)(best, std::round(a_player->GetDamage(entry)));
			}
		}

		void StoreUpgradeValue(RE::GFxValue* a_object, RE::InventoryEntryData* a_item)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return;
			}
			const auto now = Clock::now();
			if (now - g_lastEntry > kListGap) {
				RebuildBaseline(player);
			}
			g_lastEntry = now;
			if (a_item->IsWorn()) {
				return;
			}
			if (const auto* armor = a_item->object->As<RE::TESObjectARMO>()) {
				a_object->SetMember("pngii_upgMask", RE::GFxValue(static_cast<double>(armor->GetSlotMask().underlying())));
				a_object->SetMember("pngii_upgValue", RE::GFxValue(static_cast<double>(std::round(player->GetArmorValue(a_item)))));
				return;
			}
			const auto group = GroupOf(a_item->object->As<RE::TESObjectWEAP>());
			if (group != WeaponGroup::kNone) {
				a_object->SetMember("pngii_upgGroup", RE::GFxValue(static_cast<double>(group)));
				a_object->SetMember("pngii_upgValue", RE::GFxValue(static_cast<double>(std::round(player->GetDamage(a_item)))));
			}
		}

		bool IsUpgrade(const RE::GFxValue& a_entry)
		{
			const auto value = static_cast<float>(Num(a_entry, "pngii_upgValue", -1.0));
			if (value < 0.0f) {
				return false;
			}
			if (const auto mask = static_cast<std::uint32_t>(Num(a_entry, "pngii_upgMask", 0.0))) {
				float replaced = 0.0f;
				for (const auto& [wornMask, wornValue] : g_baseline.worn) {
					if (wornMask & mask) {
						replaced += wornValue;
					}
				}
				return value > replaced;
			}
			const auto group = static_cast<WeaponGroup>(static_cast<int>(Num(a_entry, "pngii_upgGroup", 0.0)));
			const auto best = group == WeaponGroup::kMelee ? g_baseline.melee : group == WeaponGroup::kRanged ? g_baseline.ranged : -1.0f;
			return best >= 0.0f && value > best;
		}

		IconRules::Timer g_callbackTimer;
		std::uint64_t g_buildNs = 0;
		std::uint64_t g_worstBuildNs = 0;
		std::uint64_t g_builds = 0;
		Clock::time_point g_lastCallback{};

		void OnInventoryEntryImpl(RE::GFxValue* a_object, RE::InventoryEntryData* a_item);

		void OnInventoryEntry(RE::GFxMovieView*, RE::GFxValue* a_object, RE::InventoryEntryData* a_item)
		{
			if (!g_logTiming) {
				OnInventoryEntryImpl(a_object, a_item);
				return;
			}
			const auto start = Clock::now();
			if (start - g_lastCallback > kListGap) {
				g_buildNs = 0;
				++g_builds;
			}
			OnInventoryEntryImpl(a_object, a_item);
			const auto end = Clock::now();
			g_lastCallback = end;
			const auto ns = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
			g_callbackTimer.Add(ns);
			g_buildNs += ns;
			g_worstBuildNs = (std::max)(g_worstBuildNs, g_buildNs);
		}

		void OnInventoryEntryImpl(RE::GFxValue* a_object, RE::InventoryEntryData* a_item)
		{
			// Member, not GetObject(): windows.h renames that call to GetObjectW in this TU.
			if (!a_object || !a_item || !a_item->object) {
				return;
			}
			std::uint32_t live = a_item->IsQuestObject() ? IconRules::kLiveQuest : 0;
			auto enchant = EnchantState(a_item->GetEnchantment());
			if (enchant && g_disallowEnchanting) {
				const auto* keywords = a_item->object->As<RE::BGSKeywordForm>();
				if (keywords && keywords->HasKeyword(g_disallowEnchanting)) {
					enchant = 3;
				}
			}
			// The Arcane Enchanter never lists staffs, and staff ENCH records carry no flag for it, so staffs are always NoDisenchant.
			if (enchant) {
				const auto* weapon = a_item->object->As<RE::TESObjectWEAP>();
				if (weapon && weapon->IsStaff()) {
					enchant = 3;
				}
			}
			live |= enchant << IconRules::kLiveEnchShift;
			if (g_upgradeRules) {
				StoreUpgradeValue(a_object, a_item);
			}
			if (live) {
				a_object->SetMember("pngii_live", RE::GFxValue(static_cast<double>(live)));
			}
		}

		std::uint32_t KnownEffectsLive(RE::FormID a_formID)
		{
			const auto* ingredient = RE::TESForm::LookupByID<RE::IngredientItem>(a_formID);
			if (!ingredient) {
				return 0;
			}
			const auto flags = ingredient->gamedata.knownEffectFlags;
			const std::uint32_t effectBits = (flags & IconRules::kLiveEffectMask) << IconRules::kLiveEffectShift;
			const auto count = (std::min)(ingredient->effects.size(), std::uint32_t{ 4 });
			std::uint32_t total = 0;
			std::uint32_t known = 0;
			for (std::uint32_t i = 0; i < count; ++i) {
				if (ingredient->effects[i]) {
					++total;
					known += (flags >> i) & 1u;
				}
			}
			if (total == 0) {
				return 0;
			}
			return ((1 + (known == total ? 4 : known)) << IconRules::kLiveKnownShift) | effectBits;
		}

		bool PlayerSide(RE::GFxMovie& a_movie)
		{
			if (g_openMenu != RE::ContainerMenu::MENU_NAME && g_openMenu != RE::BarterMenu::MENU_NAME) {
				return true;
			}
			RE::GFxValue segment;
			if (a_movie.GetVariable(&segment, "_root.Menu_mc.inventoryLists.categoryList._activeSegment") &&
				segment.IsNumber()) {
				return segment.GetNumber() == 1.0;
			}
			static bool logged = false;
			if (!logged) {
				logged = true;
				logger::warn("new items: categoryList._activeSegment missing, treating rows as player side");
			}
			return true;
		}

		// Multiply tint via the AS2 Color class: setTransform takes percent multipliers, so white art takes the color and dark detail stays dark. a_rgb 0 restores identity.
		void SetTint(RE::GFxMovie& a_movie, RE::GFxValue& a_child, std::uint32_t a_rgb)
		{
			static std::uint32_t logged = 0;
			RE::GFxValue transform;
			a_movie.CreateObject(&transform);
			const auto percent = [&](std::uint32_t a_shift) { return a_rgb ? ((a_rgb >> a_shift) & 0xFFu) * 100.0 / 255.0 : 100.0; };
			transform.SetMember("ra", RE::GFxValue(percent(16)));
			transform.SetMember("ga", RE::GFxValue(percent(8)));
			transform.SetMember("ba", RE::GFxValue(percent(0)));
			transform.SetMember("aa", RE::GFxValue(100.0));
			for (const auto* offset : { "rb", "gb", "bb", "ab" }) {
				transform.SetMember(offset, RE::GFxValue(0.0));
			}
			RE::GFxValue color;
			std::array<RE::GFxValue, 1> ctor{ a_child };
			a_movie.CreateObject(&color, "Color", ctor.data(), 1);
			if (!color.IsObject()) {
				if (logged < kLoggedCalls) {
					++logged;
					logger::warn("status tint: Color class unavailable, icon left untinted");
				}
				return;
			}
			std::array<RE::GFxValue, 1> args{ transform };
			color.Invoke("setTransform", args);
			if (logged < kLoggedCalls) {
				++logged;
				logger::debug("status tint: {:06X}", a_rgb);
			}
		}

		void HideChild(RE::GFxValue& a_container, std::size_t a_index)
		{
			a_container.SetMember(kChildren[a_index].show.c_str(), RE::GFxValue(false));
			RE::GFxValue child;
			if (a_container.GetMember(kChildren[a_index].name.c_str(), &child) && child.IsDisplayObject()) {
				child.SetMember("_visible", RE::GFxValue(false));
			}
		}

		void Hide(RE::GFxValue& a_row)
		{
			RE::GFxValue container;
			if (!a_row.GetMember("pngii_status", &container) || !container.IsDisplayObject()) {
				return;
			}
			for (std::size_t i = 0; i < kChildren.size(); ++i) {
				HideChild(container, i);
			}
			container.SetMember("_visible", RE::GFxValue(false));
		}

		class OnLoadInitHandler final : public RE::GFxFunctionHandler
		{
		public:
			void Call(Params& a_params) override
			{
				RE::GFxValue container;
				RE::GFxValue name;
				RE::GFxValue show;
				RE::GFxValue tint;
				if (a_params.argCount < 1 || !a_params.args[0].IsDisplayObject() ||
					!a_params.args[0].GetMember("_parent", &container) || !container.IsDisplayObject() ||
					!a_params.args[0].GetMember("_name", &name) || !name.IsString()) {
					return;
				}
				if (container.GetMember(std::format("tint_{}", name.GetString()).c_str(), &tint) && tint.IsNumber() &&
					tint.GetNumber() != 0.0) {
					SetTint(*a_params.movie, a_params.args[0], static_cast<std::uint32_t>(tint.GetNumber()));
				}
				if (!container.GetMember(std::format("show_{}", name.GetString()).c_str(), &show) ||
					!show.IsBool() || show.GetBool()) {
					return;
				}
				a_params.args[0].SetMember("_visible", RE::GFxValue(false));

				static std::uint32_t logged = 0;
				if (logged < kLoggedCalls) {
					++logged;
					logger::debug("status onLoadInit re-hid holder");
				}
			}
		};

		void Apply(RE::GFxMovie& a_movie, RE::GFxValue& a_row, const RE::GFxValue& a_nameField,
			const RE::GFxValue& a_entry, const RE::GFxValue& a_state)
		{
			static std::vector<IconRules::StatusIconMatch> matches;
			const auto formID = static_cast<RE::FormID>(Num(a_entry, "formId", 0.0));
			const auto museum = IconRules::HasMuseumRules() ? static_cast<std::int32_t>(TCCItemStatus::Get(formID)) : 0;
			const auto soul = static_cast<std::int32_t>(Num(a_entry, "status", -1.0));
			auto live = static_cast<std::uint32_t>(Num(a_entry, "pngii_live", 0.0));
			if (formID && IconRules::HasNewRules() && NewItems::IsNew(formID) && PlayerSide(a_movie)) {
				live |= IconRules::kLiveNew;
				NewItems::MarkSeen(formID);
			}
			if (formID && IconRules::HasKnownEffectRules()) {
				live |= KnownEffectsLive(formID);
			}
			if (g_upgradeRules && IsUpgrade(a_entry)) {
				live |= IconRules::kLiveUpgrade;
			}
			if (Flag(a_entry, "bestInClass")) {
				live |= IconRules::kLiveBest;
			}
			if (Flag(a_entry, "favorite")) {
				live |= IconRules::kLiveFavorite;
			}
			if (!((live >> IconRules::kLiveEnchShift) & IconRules::kLiveEnchMask) && Flag(a_entry, "isEnchanted")) {
				live |= IconRules::kLiveAbility;
			}
			IconRules::FindStatusIcons(formID, soul, museum, live, matches);
			if (matches.empty()) {
				Hide(a_row);
				return;
			}

			const double entryHeight = EntryHeight(a_state);
			const double size = entryHeight * g_sizeFraction;
			g_lastPx = TextureManager::StageToScreenPx(size);
			std::array<const std::string*, IconRules::kMaxStatusIcons> urls{};
			std::array<std::uint8_t, IconRules::kMaxStatusIcons> replaces{};
			std::array<std::uint32_t, IconRules::kMaxStatusIcons> tints{};
			std::size_t count = 0;
			for (const auto& match : matches) {
				if (const auto* url = TextureManager::GetURL(*match.path, g_lastPx)) {
					replaces[count] = match.replace;
					tints[count] = match.tint;
					urls[count++] = url;
				}
			}
			if (count == 0) {
				Hide(a_row);
				return;
			}

			RE::GFxValue best;
			a_row.GetMember("bestIcon", &best);
			const double bestWidth = best.IsDisplayObject() ? Num(best, "_width", 0.0) : 0.0;
			const double step = (std::max)(bestWidth, size) * 1.25;
			const double start = Num(a_nameField, "_x", 0.0) + Num(a_nameField, "_width", 0.0) + 5.0;
			if (bestWidth > 0.0 && size > bestWidth) {
				std::uint32_t slot = 0;
				for (std::size_t i = 0; i < kSkyUIIcons.size(); ++i) {
					RE::GFxValue icon;
					if (SkyUIShown(i, a_entry, a_state) && a_row.GetMember(kSkyUIIcons[i], &icon) && icon.IsDisplayObject()) {
						icon.SetMember("_x", RE::GFxValue(start + step * slot++ + (size - Num(icon, "_width", bestWidth)) * 0.5));
					}
				}
			}
			const double x = start + step * ShownStatusIcons(a_entry, a_state);

			RE::GFxValue container;
			a_row.GetMember("pngii_status", &container);
			if (!container.IsDisplayObject() && !a_row.CreateEmptyMovieClip(&container, "pngii_status", kStatusDepth)) {
				return;
			}
			container.SetMember("_x", RE::GFxValue(x));
			container.SetMember("_y", RE::GFxValue((entryHeight - size) * 0.5));
			const double scale = size / kLoadedImageSize * 100.0;

			std::size_t shown = 0;
			std::size_t appended = 0;
			for (std::size_t i = 0; i < kChildren.size(); ++i) {
				const auto& keys = kChildren[i];
				if (i >= count) {
					HideChild(container, i);
					continue;
				}
				RE::GFxValue child;
				container.GetMember(keys.name.c_str(), &child);
				if (!child.IsDisplayObject() &&
					!container.CreateEmptyMovieClip(&child, keys.name.c_str(), static_cast<std::int32_t>(i))) {
					continue;
				}
				// Replace: SkyUI sets the target's _x only when it shows it, so read _x only then; else append.
				RE::GFxValue target;
				const auto replace = replaces[i];
				if (replace && SkyUIShown(replace - 1u, a_entry, a_state) &&
					a_row.GetMember(kSkyUIIcons[replace - 1u], &target) && target.IsDisplayObject()) {
					std::array<RE::GFxValue, 1> frame{ RE::GFxValue("hide") };
					target.Invoke("gotoAndStop", frame);
					child.SetMember("_x", RE::GFxValue(Num(target, "_x", 0.0) - x));
				} else {
					child.SetMember("_x", RE::GFxValue(static_cast<double>(appended++) * size * 1.25));
				}
				child.SetMember("_xscale", RE::GFxValue(scale));
				child.SetMember("_yscale", RE::GFxValue(scale));

				if (static_cast<std::uint32_t>(Num(container, keys.tint.c_str(), 0.0)) != tints[i]) {
					SetTint(a_movie, child, tints[i]);
					container.SetMember(keys.tint.c_str(), RE::GFxValue(static_cast<double>(tints[i])));
				}
				const auto& url = *urls[i];
				RE::GFxValue current;
				if (!container.GetMember(keys.url.c_str(), &current) || !current.IsString() || url != current.GetString()) {
					RE::GFxValue loader;
					a_movie.CreateObject(&loader, "MovieClipLoader");
					static auto loadInitHandler = RE::make_gptr<OnLoadInitHandler>();
					RE::GFxValue listener;
					RE::GFxValue onLoadInit;
					a_movie.CreateObject(&listener);
					a_movie.CreateFunction(&onLoadInit, loadInitHandler.get());
					listener.SetMember("onLoadInit", onLoadInit);
					std::array<RE::GFxValue, 1> listenerArgs{ listener };
					loader.Invoke("addListener", listenerArgs);
					container.SetMember(keys.loader.c_str(), loader);
					std::array<RE::GFxValue, 2> args{ RE::GFxValue(url.c_str()), child };
					loader.Invoke("loadClip", args);
					container.SetMember(keys.url.c_str(), RE::GFxValue(url.c_str()));
				}
				container.SetMember(keys.show.c_str(), RE::GFxValue(true));
				child.SetMember("_visible", RE::GFxValue(true));
				++shown;
			}
			container.SetMember("_visible", RE::GFxValue(shown > 0));
		}

		class FormatNameWrapper final : public RE::GFxFunctionHandler
		{
		public:
			void Call(Params& a_params) override
			{
				if (!a_params.thisPtr) {
					return;
				}
				a_params.thisPtr->Invoke(kOriginal, a_params.retVal, a_params.args, a_params.argCount);
				if (a_params.argCount < 3 || !a_params.args[1].IsObject()) {
					return;
				}
				const auto start = g_logTiming ? Clock::now() : Clock::time_point{};
				Apply(*a_params.movie, *a_params.thisPtr, a_params.args[0], a_params.args[1], a_params.args[2]);
				if (g_logTiming) {
					g_timer.Add(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count());
				}
			}
		};

		std::optional<std::string_view> FindMenu(const RE::BSFixedString& a_name)
		{
			for (const auto menu : kMenus) {
				if (a_name == menu) {
					return menu;
				}
			}
			return std::nullopt;
		}

		bool InstallInto(RE::GFxMovieView& a_view, std::string_view a_menu)
		{
			RE::GFxValue proto;
			if (!a_view.GetVariable(&proto, kProto) || (!proto.IsObject() && !proto.IsDisplayObject())) {
				return false;
			}
			RE::GFxValue installed;
			if (proto.GetMember(kInstalled, &installed) && installed.IsBool() && installed.GetBool()) {
				return true;
			}
			proto.SetMember(kInstalled, RE::GFxValue(true));
			RE::GFxValue original;
			if (!proto.GetMember("formatName", &original) || !original.IsObject()) {
				logger::warn("{}: InventoryListEntry.prototype.formatName missing", a_menu);
				return true;
			}
			static auto handler = RE::make_gptr<FormatNameWrapper>();
			RE::GFxValue wrapper;
			a_view.CreateFunction(&wrapper, handler.get());
			proto.SetMember(kOriginal, original);
			proto.SetMember("formatName", wrapper);
			logger::info("{}: wrapped InventoryListEntry.prototype.formatName", a_menu);

			RE::GFxValue list;
			if (a_view.GetVariable(&list, kItemList) && list.IsDisplayObject()) {
				list.Invoke("UpdateList");
			}
			return true;
		}

		void ScheduleInstall(std::string_view a_menu, std::uint32_t a_attemptsRemaining)
		{
			SKSE::GetTaskInterface()->AddUITask([a_menu, a_attemptsRemaining]() {
				auto* ui = RE::UI::GetSingleton();
				auto  view = ui ? ui->GetMovieView(a_menu) : nullptr;
				if (view && InstallInto(*view, a_menu)) {
					return;
				}
				if (a_attemptsRemaining == 0) {
					logger::warn("{}: InventoryListEntry prototype never became available for status icons", a_menu);
					return;
				}
				ScheduleInstall(a_menu, a_attemptsRemaining - 1);
			});
		}

		class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
				RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				const auto menu = a_event ? FindMenu(a_event->menuName) : std::nullopt;
				if (!menu) {
					return RE::BSEventNotifyControl::kContinue;
				}
				if (a_event->opening) {
					g_openMenu = *menu;
					ScheduleInstall(*menu, 120);
					return RE::BSEventNotifyControl::kContinue;
				}
				NewItems::CommitSeen();
				g_openMenu = {};
				if (g_logTiming) {
					const double averageUs = g_timer.count ? static_cast<double>(g_timer.totalNs) / g_timer.count / 1000.0 : 0.0;
					logger::info("status {} n={} avg={:.1f}us max={:.1f}us", *menu, g_timer.count, averageUs,
						g_timer.maxNs / 1000.0);
					g_timer = {};
					if (g_callbackTimer.count) {
						const double callbackAvgUs = static_cast<double>(g_callbackTimer.totalNs) / g_callbackTimer.count / 1000.0;
						logger::info("status callback {} builds={} worstBuild={:.2f}ms entries={} avg={:.1f}us max={:.1f}us", *menu,
							g_builds, g_worstBuildNs / 1e6, g_callbackTimer.count, callbackAvgUs, g_callbackTimer.maxNs / 1000.0);
					}
					g_callbackTimer = {};
					g_buildNs = 0;
					g_worstBuildNs = 0;
					g_builds = 0;
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Install()
	{
		if (!IconRules::HasStatusIconRules()) {
			logger::info("status icons: no statusIcon rules, hook not installed");
			return;
		}
		g_prewarmPaths = IconRules::StatusIconPaths();
		static MenuSink sink;
		RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&sink);
		logger::info("status icons: menu sink registered");
		if (IconRules::HasLiveRules()) {
			g_upgradeRules = IconRules::HasUpgradeRules();
			g_disallowEnchanting = RE::TESForm::LookupByID<RE::BGSKeyword>(0x000C27BD);
			if (!g_disallowEnchanting) {
				logger::warn("status icons: MagicDisallowEnchanting not found, NoDisenchant disabled");
			}
			SKSE::GetScaleformInterface()->Register(OnInventoryEntry);
			logger::info("status icons: inventory callback registered");
		}
	}

	void PrewarmIcons()
	{
		if (g_prewarmPaths.empty()) {
			return;
		}
		RunPrewarm(g_lastPx ? g_lastPx : TextureManager::StageToScreenPx(kFallbackEntryHeight * g_sizeFraction), 0, 0, 0);
	}

	void SetTiming(bool a_enabled)
	{
		g_logTiming = a_enabled;
	}

	void SetSize(double a_fraction)
	{
		g_sizeFraction = std::clamp(a_fraction, kMinSize, kMaxSize);
	}
}
