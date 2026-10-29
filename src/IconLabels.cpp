#include "IconLabels.h"

namespace IconLabels
{
	namespace
	{
		enum Kw : std::size_t
		{
			kClothing,
			kJewelry,
			kRecipe,
			kSpellTome,
			kWarhammer,
			kArtifact,
			kGem,
			kHide,
			kTool,
			kAnimalPart,
			kOreIngot,
			kClutter,
			kFirewood,
			kChildClothes,
			kToy,
			kHouse0,
			kHouseLast = kHouse0 + 6,
			kCount
		};
		std::array<const RE::BGSKeyword*, kCount> g_kw{};

		// SkyUI PARTMASK_PRECEDENCE (Armor.as:43); its loop stops at the first hit, so Body beats Head.
		constexpr std::uint32_t kHead = 1u << 0, kHair = 1u << 1, kBody = 1u << 2, kHands = 1u << 3, kForearms = 1u << 4,
								kAmulet = 1u << 5, kRing = 1u << 6, kFeet = 1u << 7, kCalves = 1u << 8, kShield = 1u << 9,
								kTail = 1u << 10, kLongHair = 1u << 11, kCirclet = 1u << 12, kEars = 1u << 13;
		constexpr std::array<std::uint32_t, 14> kPrecedence{ kBody, kHair, kHands, kForearms, kFeet, kCalves, kShield,
			kAmulet, kRing, kLongHair, kEars, kHead, kCirclet, kTail };

		bool Has(const RE::TESForm& a_form, std::size_t a_kw)
		{
			const auto* keywords = a_form.As<RE::BGSKeywordForm>();
			return g_kw[a_kw] && keywords && keywords->HasKeyword(g_kw[a_kw]);
		}

		bool HasHouse(const RE::TESForm& a_form)
		{
			for (std::size_t i = kHouse0; i <= kHouseLast; ++i) {
				if (Has(a_form, i)) {
					return true;
				}
			}
			return false;
		}

		// SkyUI matches baseId = formId & 0xFFFFFF, so the plugin index is ignored
		bool Base(std::uint32_t a_formID, std::initializer_list<std::uint32_t> a_ids)
		{
			return std::ranges::find(a_ids, a_formID & 0x00FFFFFF) != a_ids.end();
		}

		const char* WeaponLabel(const RE::TESObjectWEAP& a_weap, std::uint32_t a_formID)
		{
			if (Base(a_formID, { 0x0E3C16, 0x06A707, 0x1019D4 })) {
				return "weapon_pickaxe";
			}
			if (Base(a_formID, { 0x02F2F4, 0x0AE086 })) {
				return "weapon_woodaxe";
			}
			switch (a_weap.GetWeaponType()) {
			case RE::WEAPON_TYPE::kOneHandSword:  return "weapon_sword";
			case RE::WEAPON_TYPE::kOneHandDagger: return "weapon_dagger";
			case RE::WEAPON_TYPE::kOneHandAxe:    return "weapon_waraxe";
			case RE::WEAPON_TYPE::kOneHandMace:   return "weapon_mace";
			case RE::WEAPON_TYPE::kTwoHandSword:  return "weapon_greatsword";
			case RE::WEAPON_TYPE::kTwoHandAxe:    return Has(a_weap, kWarhammer) ? "weapon_hammer" : "weapon_battleaxe";
			case RE::WEAPON_TYPE::kBow:           return "weapon_bow";
			case RE::WEAPON_TYPE::kStaff:         return "weapon_staff";
			case RE::WEAPON_TYPE::kCrossbow:      return "weapon_crossbow";
			default:                              return "default_weapon";
			}
		}

		const char* BookLabel(const RE::TESObjectBOOK& a_book)
		{
			if (Has(a_book, kRecipe)) {
				return "book_note";
			}
			if (Has(a_book, kSpellTome)) {
				return "book_tome";
			}
			return a_book.IsNoteScroll() ? "book_note" : "default_book";
		}

		const char* MiscLabel(const RE::TESObjectMISC& a_misc, std::uint32_t a_formID)
		{
			if (Base(a_formID, { 0x06851E })) {
				return "misc_gem";
			}
			if (Base(a_formID, { 0x04B56C, 0x0AB7BB, 0x07C260, 0x05AF48, 0x0ED417, 0x0AB375, 0x08CDFA, 0x0B634C, 0x0999E7, 0x0663D7, 0x039647 })) {
				return "misc_dragonclaw";
			}
			if (Base(a_formID, { 0x00000A })) {
				return "misc_lockpick";
			}
			if (Base(a_formID, { 0x00000F })) {
				return "misc_gold";
			}
			if (Base(a_formID, { 0x0DB5D2 })) {
				return "misc_leather";
			}
			if (Base(a_formID, { 0x0800E4 })) {
				return "misc_strips";
			}
			// SkyUI's else-if order: toy, house part and tool stop the chain with the plain misc label.
			if (Has(a_misc, kChildClothes)) {
				return "clothing_body";
			}
			if (Has(a_misc, kToy) || HasHouse(a_misc)) {
				return "default_misc";
			}
			if (Has(a_misc, kArtifact)) {
				return "misc_artifact";
			}
			if (Has(a_misc, kGem)) {
				return "misc_gem";
			}
			if (Has(a_misc, kHide)) {
				return "misc_hide";
			}
			if (Has(a_misc, kTool)) {
				return "default_misc";
			}
			if (Has(a_misc, kAnimalPart)) {
				return "misc_remains";
			}
			if (Has(a_misc, kOreIngot)) {
				return "misc_ingot";
			}
			if (Has(a_misc, kClutter)) {
				return "misc_clutter";
			}
			if (Has(a_misc, kFirewood)) {
				return "misc_wood";
			}
			return "default_misc";
		}

		const char* PotionLabel(const RE::AlchemyItem& a_item)
		{
			using Flag = RE::AlchemyItem::AlchemyFlag;
			// Raw flags, food first as SkyUI does; AlchemyItem::IsFood() would exclude food+poison records.
			if (a_item.data.flags.any(Flag::kFoodItem)) {
				const auto* sound = a_item.data.consumptionSound;
				return sound && sound->GetFormID() == 0x000B6435 ? "food_wine" : "default_food";  // ITMPotionUse
			}
			if (a_item.data.flags.any(Flag::kPoison)) {
				return "potion_poison";
			}
			const auto* effect = a_item.GetCostliestEffectItem();
			const auto* base = effect ? effect->baseEffect : nullptr;
			if (!base) {
				return "default_potion";
			}
			switch (base->data.primaryAV) {
			case RE::ActorValue::kHealth:
			case RE::ActorValue::kHealRate:
			case RE::ActorValue::kHealRateMult:
				return "potion_health";
			case RE::ActorValue::kMagicka:
			case RE::ActorValue::kMagickaRate:
			case RE::ActorValue::kMagickaRateMult:
				return "potion_magic";
			case RE::ActorValue::kStamina:
			case RE::ActorValue::kStaminaRate:
			case RE::ActorValue::kStaminaRateMult:
				return "potion_stam";
			case RE::ActorValue::kResistFire:  return "potion_fire";
			case RE::ActorValue::kResistShock: return "potion_shock";
			case RE::ActorValue::kResistFrost: return "potion_frost";
			default:                           return "default_potion";
			}
		}

		const char* SoulGemLabel(const RE::TESSoulGem& a_gem, std::uint32_t a_formID, std::int32_t a_status)
		{
			if (Base(a_formID, { 0x063B27, 0x063B29 })) {
				return "soulgem_azura";
			}
			const auto capacity = a_gem.GetMaximumCapacity();
			if (capacity == RE::SOUL_LEVEL::kNone) {
				return "misc_soulgem";
			}
			// Without row soul data SkyUI reads the base record, so SoulGemGrandFilled is full. Label only; `status` rules need the caller's value.
			std::int32_t status = a_status;
			if (status < 0 || status > 2) {
				const auto soul = a_gem.GetContainedSoul();
				status = soul == RE::SOUL_LEVEL::kNone ? 0 : soul >= capacity ? 2 : 1;
			}
			const bool grand = capacity == RE::SOUL_LEVEL::kGreater || capacity == RE::SOUL_LEVEL::kGrand;
			constexpr std::array<const char*, 3> kSmall{ "soulgem_empty", "soulgem_partial", "soulgem_full" };
			constexpr std::array<const char*, 3> kGrand{ "soulgem_grandempty", "soulgem_grandpartial", "soulgem_grandfull" };
			return grand ? kGrand[status] : kSmall[status];
		}

		const char* SpellLabel(const RE::SpellItem& a_spell)
		{
			const auto* effect = a_spell.GetCostliestEffectItem();
			const auto* base = effect ? effect->baseEffect : nullptr;
			if (!base) {
				return "default_power";
			}
			switch (base->GetMagickSkill()) {
			case RE::ActorValue::kAlteration:  return "default_alteration";
			case RE::ActorValue::kConjuration: return "default_conjuration";
			case RE::ActorValue::kIllusion:    return "default_illusion";
			case RE::ActorValue::kRestoration: return "default_restoration";
			case RE::ActorValue::kDestruction:
				switch (base->data.resistVariable) {
				case RE::ActorValue::kResistFire:  return "magic_fire";
				case RE::ActorValue::kResistShock: return "magic_shock";
				case RE::ActorValue::kResistFrost: return "magic_frost";
				default:                           return "default_destruction";
				}
			default:
				return "default_power";
			}
		}

		enum class Weight
		{
			kNone,
			kLight,
			kHeavy,
			kClothing,
			kJewelry
		};

		const char* ArmorLabel(const RE::TESObjectARMO& a_armor, std::uint32_t a_formID)
		{
			using ArmorType = RE::BIPED_MODEL::ArmorType;
			const auto type = a_armor.GetArmorType();
			Weight weight = type == ArmorType::kLightArmor ? Weight::kLight : type == ArmorType::kHeavyArmor ? Weight::kHeavy : Weight::kNone;
			if (weight == Weight::kNone) {
				weight = Has(a_armor, kClothing) ? Weight::kClothing : Has(a_armor, kJewelry) ? Weight::kJewelry : Weight::kNone;
			}
			const auto mask = a_armor.GetSlotMask().underlying();
			std::uint32_t main = 0;
			for (const auto part : kPrecedence) {
				if (mask & part) {
					main = part;
					break;
				}
			}
			if (weight == Weight::kNone && main) {
				weight = (main & (kAmulet | kRing | kCirclet | kEars)) ? Weight::kJewelry : Weight::kClothing;
			}
			if (Base(a_formID, { 0x08895A })) {  // wedding wreath
				weight = Weight::kJewelry;
			}
			if (Base(a_formID, { 0x011A84 })) {  // Vampire Lord Armor
				main = kBody;
			}

			const auto jewelry = [&]() -> const char* {
				switch (main) {
				case kAmulet:  return "armor_amulet";
				case kRing:    return "armor_ring";
				case kCirclet: return "armor_circlet";
				default:       return "default_armor";
				}
			};
			// prefix tables: head, body, hands, forearms, feet, calves, shield
			const auto part = [&](const std::array<const char*, 7>& a_labels) -> const char* {
				switch (main) {
				case kHead:
				case kHair:
				case kLongHair: return a_labels[0];
				case kBody:
				case kTail:     return a_labels[1];
				case kHands:    return a_labels[2];
				case kForearms: return a_labels[3];
				case kFeet:     return a_labels[4];
				case kCalves:   return a_labels[5];
				case kShield:   return a_labels[6];
				default:        return nullptr;
				}
			};
			constexpr std::array<const char*, 7> kLight{ "lightarmor_head", "lightarmor_body", "lightarmor_hands",
				"lightarmor_forearms", "lightarmor_feet", "lightarmor_calves", "lightarmor_shield" };
			constexpr std::array<const char*, 7> kHeavy{ "armor_head", "armor_body", "armor_hands", "armor_forearms",
				"armor_feet", "armor_calves", "armor_shield" };
			constexpr std::array<const char*, 7> kCloth{ "clothing_head", "clothing_body", "clothing_hands",
				"clothing_forearms", "clothing_feet", "clothing_calves", "clothing_shield" };

			const char* label = nullptr;
			switch (weight) {
			case Weight::kLight:
			case Weight::kHeavy:
				label = part(weight == Weight::kLight ? kLight : kHeavy);
				if (!label && (main & (kAmulet | kRing | kCirclet | kEars))) {
					label = jewelry();
				}
				break;
			case Weight::kJewelry:
				label = jewelry();
				break;
			default:
				label = part(kCloth);
				break;
			}
			return label ? label : "default_armor";
		}
	}

	void Init()
	{
		const auto skyrim = [](RE::FormID a_id) { return RE::TESForm::LookupByID<RE::BGSKeyword>(a_id); };
		g_kw[kClothing] = skyrim(0x0008F95B);
		g_kw[kJewelry] = skyrim(0x0008F95A);
		g_kw[kRecipe] = skyrim(0x000F5CB0);
		g_kw[kSpellTome] = skyrim(0x000937A5);
		g_kw[kWarhammer] = skyrim(0x0006D930);
		g_kw[kArtifact] = skyrim(0x000917E8);
		g_kw[kGem] = skyrim(0x000914ED);
		g_kw[kHide] = skyrim(0x000914EA);
		g_kw[kTool] = skyrim(0x000914EE);
		g_kw[kAnimalPart] = skyrim(0x000914EB);
		g_kw[kOreIngot] = skyrim(0x000914EC);
		g_kw[kClutter] = skyrim(0x000914E9);
		g_kw[kFirewood] = skyrim(0x000BECD7);  // EditorID VendorItemFireword (sic)
		if (auto* data = RE::TESDataHandler::GetSingleton()) {
			constexpr auto kHearthFires = "HearthFires.esm"sv;
			g_kw[kChildClothes] = data->LookupForm<RE::BGSKeyword>(0x00D823, kHearthFires);
			g_kw[kToy] = data->LookupForm<RE::BGSKeyword>(0x00D824, kHearthFires);
			constexpr std::array<RE::FormID, 7> kHouse{ 0x008AD4, 0x008AC3, 0x008A9B, 0x008AC0, 0x008AC1, 0x000807, 0x008AC2 };
			for (std::size_t i = 0; i < kHouse.size(); ++i) {
				g_kw[kHouse0 + i] = data->LookupForm<RE::BGSKeyword>(kHouse[i], kHearthFires);
			}
		}
		logger::debug("IconLabels: {} of {} SkyUI keywords resolved", std::ranges::count_if(g_kw, [](const auto* a_kw) { return a_kw != nullptr; }), g_kw.size());
	}

	const char* ForFormID(std::uint32_t a_formID, std::int32_t a_soulStatus)
	{
		auto* form = a_formID ? RE::TESForm::LookupByID(a_formID) : nullptr;
		if (!form) {
			return nullptr;
		}
		// Each As<T>() matches its form type; null-checked anyway so a modded record with a mismatched type cannot crash.
		switch (form->GetFormType()) {
		case RE::FormType::Scroll:      return "default_scroll";
		case RE::FormType::Ingredient:  return "default_ingredient";
		case RE::FormType::Light:       return "misc_torch";
		case RE::FormType::KeyMaster:   return "default_key";
		case RE::FormType::Shout:       return "default_shout";
		case RE::FormType::MagicEffect: return "default_effect";
		case RE::FormType::Weapon:
			if (const auto* weap = form->As<RE::TESObjectWEAP>()) {
				return WeaponLabel(*weap, a_formID);
			}
			return nullptr;
		case RE::FormType::Ammo:
			// IsBolt() is non-const and hides the SE/AE/VR runtime data layout.
			if (auto* ammo = form->As<RE::TESAmmo>()) {
				return ammo->IsBolt() ? "weapon_bolt" : "weapon_arrow";
			}
			return nullptr;
		case RE::FormType::Book:
			if (const auto* book = form->As<RE::TESObjectBOOK>()) {
				return BookLabel(*book);
			}
			return nullptr;
		case RE::FormType::Misc:
			if (const auto* misc = form->As<RE::TESObjectMISC>()) {
				return MiscLabel(*misc, a_formID);
			}
			return nullptr;
		case RE::FormType::AlchemyItem:
			if (const auto* item = form->As<RE::AlchemyItem>()) {
				return PotionLabel(*item);
			}
			return nullptr;
		case RE::FormType::SoulGem:
			if (const auto* gem = form->As<RE::TESSoulGem>()) {
				return SoulGemLabel(*gem, a_formID, a_soulStatus);
			}
			return nullptr;
		case RE::FormType::Spell:
			if (const auto* spell = form->As<RE::SpellItem>()) {
				return SpellLabel(*spell);
			}
			return nullptr;
		case RE::FormType::Armor:
			if (const auto* armor = form->As<RE::TESObjectARMO>()) {
				return ArmorLabel(*armor, a_formID);
			}
			return nullptr;
		default:
			return nullptr;
		}
	}
}
