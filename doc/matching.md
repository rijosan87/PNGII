# Matching

`match` keys like form, formType decide what rule applies to each item. For examples, see [How to](howto.md).

## Keys

| Key | Matches | Example | Priority |
| --- | --- | --- | ---: |
| `form` | Exact item or spell | <code>"form": "Skyrim.esm&#124;0x12EB7"</code> | 100 |
| `keywordsAny` | Any listed keyword | `"keywordsAny": ["WeapTypeWarAxe", "WeapTypeMace"]` | 30 |
| `keywordsAll` | All listed keywords | `"keywordsAll": ["ArmorHeavy", "ArmorHelmet"]` | 30 |
| `parts` | Armor body slots | `"parts": [47]` | 30 |
| `modelContains` | Text in model path | `"modelContains": "ingot"` | 30 |
| `effectKeywordsAny` | Any effect keyword | `"effectKeywordsAny": ["MagicDamageFire"]` | 30 |
| `effectKeywordsAll` | All effect keywords | `"effectKeywordsAll": ["MagicDamageFire", "MagicDamageFrost"]` | 30 |
| `effectMagnitude` | Effect strength | `"effectMagnitude": { "min": 50 }` | 30 |
| `effectDuration` | Effect duration in seconds | `"effectDuration": { "min": 1, "max": 60 }` | 30 |
| `plugin` | Plugin name | `"plugin": "Dawnguard.esm"` | 20 |
| `iconLabel` | SkyUI or I4 label. Use alone, `icon` rules only | `"iconLabel": "weapon_dagger"` | 15 |
| `formType` | Record type | `"formType": "Book"` | 10 |
| `status` | Soul gem fill | `"formType": "SoulGem", "status": "Full"` | none |

Most keys take one value or an array list [ ].

## Combining keys

`match` keys can be combined together for specific cases and all of them must hold true for the rule to assign icon.

```json
{ "match": { "formType": "Spell", "plugin": "Arcanum.esp" }, "icon": "spells/arcanum.png" }
```

## Which rule wins

When several rules match, the highest priority rule wins. If multiple rules have the same priority then the last one based on alphabetical order wins. Simply put, if a.json has the same rule and priority as z.json, the later z.json will win.

```json
{ "match": { "keywordsAny": "WeapTypeDagger" }, "icon": "MyIcons/dagger.png", "priority": 150 }
```

## Form types (`formType`)

| `formType` | Record | Includes |
| --- | --- | --- |
| `ammo` | AMMO | Arrows, bolts |
| `armor` | ARMO | Armor, clothing, jewelry, shields |
| `book` | BOOK | Books, notes, spell tomes |
| `ingredient` | INGR | Ingredients |
| `key` | KEYM | Keys |
| `light` | LIGH | Torches |
| `miscitem` | MISC | Misc items, gems, ingots, clutter |
| `potion` | ALCH | Potions, poisons, food, drinks |
| `scroll` | SCRL | Scrolls |
| `soulgem` | SLGM | Soul gems |
| `weapon` | WEAP | Weapons, staffs |
| `spell` | SPEL | Spells, powers, abilities |
| `shout` | SHOU | Shouts |

```json
{ "match": { "formType": "Book" }, "icon": "MyIcons/book.png" },
{ "match": { "formType": ["Armor", "Weapon"] }, "icon": "MyIcons/gear.png" }
```

To distinguish between food from potions, or poisons from potions you need keywords. Check below.

## Keywords (`keywordsAny`, `keywordsAll`)

Keywords from Skyrim and its DLC.

| Family | Keywords |
| --- | --- |
| Weapon type | WeapTypeBattleaxe, WeapTypeBoundArrow, WeapTypeBow, WeapTypeDagger, WeapTypeGreatsword, WeapTypeMace, WeapTypeStaff, WeapTypeSword, WeapTypeWarAxe, WeapTypeWarhammer |
| Weapon material | WeapMaterialDaedric, WeapMaterialDraugr, WeapMaterialDraugrHoned, WeapMaterialDwarven, WeapMaterialEbony, WeapMaterialElven, WeapMaterialFalmer, WeapMaterialFalmerHoned, WeapMaterialGlass, WeapMaterialImperial, WeapMaterialIron, WeapMaterialOrcish, WeapMaterialSilver, WeapMaterialSteel, WeapMaterialWood |
| Weapon material (DLC) | DLC1WeapMaterialDragonbone, DLC2WeaponMaterialNordic, DLC2WeaponMaterialStalhrim |
| Armor type | ArmorBoots, ArmorCuirass, ArmorGauntlets, ArmorHelmet, ArmorShield, ArmorJewelry, ArmorClothing, ArmorHeavy, ArmorLight |
| Armor material | ArmorMaterialDaedric, ArmorMaterialDragonplate, ArmorMaterialDragonscale, ArmorMaterialDwarven, ArmorMaterialEbony, ArmorMaterialElven, ArmorMaterialElvenGilded, ArmorMaterialGlass, ArmorMaterialHide, ArmorMaterialImperialHeavy, ArmorMaterialImperialLight, ArmorMaterialImperialStudded, ArmorMaterialIron, ArmorMaterialIronBanded, ArmorMaterialLeather, ArmorMaterialOrcish, ArmorMaterialScaled, ArmorMaterialSteel, ArmorMaterialSteelPlate, ArmorMaterialStormcloak, ArmorMaterialStudded |
| Armor material (Update.esm) | ArmorMaterialBearStormcloak, ArmorMaterialBlades, ArmorMaterialFalmer, ArmorMaterialForsworn, ArmorMaterialMS02Forsworn, ArmorMaterialPenitus, ArmorMaterialThievesGuild, ArmorMaterialThievesGuildLeader |
| Armor material (DLC) | DLC1ArmorMaterialDawnguard, DLC1ArmorMaterialFalmerHardened, DLC1ArmorMaterialHunter, DLC1ArmorMaterialVampire, DLC2ArmorMaterialBonemoldHeavy, DLC2ArmorMaterialBonemoldLight, DLC2ArmorMaterialChitinHeavy, DLC2ArmorMaterialChitinLight, DLC2ArmorMaterialMoragTong, DLC2ArmorMaterialNordicHeavy, DLC2ArmorMaterialNordicLight, DLC2ArmorMaterialStalhrimHeavy, DLC2ArmorMaterialStalhrimLight |
| Armor set | ArmorDarkBrotherhood, ArmorNightingale |
| Item class | VendorItemAnimalHide, VendorItemAnimalPart, VendorItemArmor, VendorItemArrow, VendorItemBook, VendorItemClothing, VendorItemClutter, VendorItemDaedricArtifact, VendorItemFireword, VendorItemFood, VendorItemFoodRaw, VendorItemGem, VendorItemIngredient, VendorItemJewelry, VendorItemKey, VendorItemOreIngot, VendorItemPoison, VendorItemPotion, VendorItemRecipe, VendorItemScroll, VendorItemSoulGem, VendorItemSpellTome, VendorItemStaff, VendorItemTool, VendorItemWeapon |

```json
{ "match": { "keywordsAny": ["WeapTypeDagger"] }, "icon": "MyIcons/dagger.png" },
{ "match": { "keywordsAll": ["ArmorHeavy", "ArmorCuirass"] }, "icon": "MyIcons/heavy_cuirass.png" }
```

### Magic effect keywords (`effectKeywordsAny`, `effectKeywordsAll`)

These keywords belong to the magic effect and will work for potion effects, scrolls and spells.

| Family | Keywords |
| --- | --- |
| Damage | MagicDamageFire, MagicDamageFrost, MagicDamageShock, MagicDamageResist |
| Potion: restore | MagicAlchRestoreHealth, MagicAlchRestoreMagicka, MagicAlchRestoreStamina |
| Potion: fortify | MagicAlchFortifyAlchemy, MagicAlchFortifyAlteration, MagicAlchFortifyBlock, MagicAlchFortifyCarryWeight, MagicAlchFortifyConjuration, MagicAlchFortifyDestruction, MagicAlchFortifyEnchanting, MagicAlchFortifyHealRate, MagicAlchFortifyHealth, MagicAlchFortifyHeavyArmor, MagicAlchFortifyIllusion, MagicAlchFortifyLightArmor, MagicAlchFortifyLockpicking, MagicAlchFortifyMagicka, MagicAlchFortifyMagickaRate, MagicAlchFortifyMarksman, MagicAlchFortifyMass, MagicAlchFortifyOneHanded, MagicAlchFortifyPickPocket, MagicAlchFortifyRestoration, MagicAlchFortifySmithing, MagicAlchFortifySneak, MagicAlchFortifySpeechcraft, MagicAlchFortifyStamina, MagicAlchFortifyStaminaRate, MagicAlchFortifyTwoHanded |
| Potion: resist | MagicAlchResistFire, MagicAlchResistFrost, MagicAlchResistMagic, MagicAlchResistPoison, MagicAlchResistShock |
| Poison: damage and weakness | MagicAlchDamageHealth, MagicAlchDamageMagicka, MagicAlchDamageStamina, MagicAlchWeaknessFire, MagicAlchWeaknessFrost, MagicAlchWeaknessMagic, MagicAlchWeaknessShock |
| Potion: kind | MagicAlchBeneficial, MagicAlchHarmful, MagicAlchDurationBased |
| Spell school | MagicSchoolAlteration, MagicSchoolConjuration, MagicSchoolDestruction, MagicSchoolIllusion, MagicSchoolRestoration |
| Spell: protection | MagicArmorSpell, MagicArmor30Perk, MagicArmor50Perk, MagicArmor70Perk, MagicWard, MagicCloak, MagicFlameCloak |
| Spell: influence | MagicInfluence, MagicInfluenceCharm, MagicInfluenceFear, MagicInfluenceFrenzy, MagicTurnUndead |
| Spell: summon | MagicSummonFamiliar, MagicSummonFire, MagicSummonFrost, MagicSummonShock, MagicSummonUndead, MagicSprigganCallAnimals, MagicThrallHealthRegen |
| Spell: other | MagicRestoreHealth, MagicInvisibility, MagicNightEye, MagicParalysis, MagicRune, MagicSlow, MagicTelekinesis, MagicVampireDrain, MagicWeaponSpeed, MagicFortifyOneHanded, MagicFortifyTwoHanded, MagicIgnite |
| Blessing and rested | MagicBlessing, MagicRestedHealth, MagicRestedMagicka, MagicRestedStamina |

```json
{ "match": { "formType": "Spell", "effectKeywordsAny": ["MagicDamageFire"] }, "icon": "MyIcons/fire.png" },
{ "match": { "formType": "Potion", "effectKeywordsAny": ["MagicAlchRestoreHealth"] }, "icon": "MyIcons/potion_health.png" }
```

## Armor slots (`parts`)

| Slot | Name | Slot | Name | Slot | Name |
| ---: | --- | ---: | --- | ---: | --- |
| 30 | Head | 42 | Circlet | 54 | Left leg (mod) |
| 31 | Hair | 43 | Ears | 55 | Face jewelry (mod) |
| 32 | Body | 44 | Mouth (mod) | 56 | Chest secondary (mod) |
| 33 | Hands | 45 | Neck (mod) | 57 | Shoulder (mod) |
| 34 | Forearms | 46 | Chest primary (mod), often cloaks | 58 | Left arm (mod) |
| 35 | Amulet | 47 | Back (mod), often backpacks | 59 | Right arm (mod) |
| 36 | Ring | 48 | Misc 1 (mod) | 60 | Misc 2 (mod) |
| 37 | Feet | 49 | Pelvis primary (mod) | 61 | FX01 |
| 38 | Calves | 50 | Decapitate head | | |
| 39 | Shield | 51 | Decapitate | | |
| 40 | Tail | 52 | Pelvis secondary (mod) | | |
| 41 | Long hair | 53 | Right leg (mod) | | |

Example below shows icons mapped to backpacks (slot 47), for cloaks (slot 46).

```json
{ "match": { "formType": "Armor", "parts": [47] }, "icon": "MyIcons/backpack.png" },
{ "match": { "formType": "Armor", "parts": [46] }, "icon": "MyIcons/cloak.png" }
```
If multiple armor slots are used [32, 37], only the armor with all of those assigned slots will show the icon.

## Icon labels (`iconLabel`)

Each label has a PNG with the same name in `Interface/PNGIconInjector/base/`. Replace that PNG to change the icon for the label. Label names are listed in the shipped rule files:

- `00_SkyUI-Labels.json`: the 89 labels SkyUI assigns
- `01_I4-Labels.json`: labels I4 assigns, only with I4 installed
- `02_SkyUI-Extra-Labels.json`: labels SkyUI has art for but never assigns

## Rules that get skipped

Some keys cannot be combined with others. A rule that breaks these limits is skipped with log warning.

| Key | Limit |
| --- | --- |
| `iconLabel` | Must be the only key in `match` |
| `status` | Needs another key to pick the item, as in the soul gem example above |
