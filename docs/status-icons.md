# Status icons

Status icon shows after the item name in Inventory, Container, Barter and Gift menus.

## SkyUI Default status icons

The SkyUI comes with its own status icons. There are six of them. `bestInClass`, `favorite`, `poisoned`, `stolen`, `enchanted`, `read`.

So rules for these six keys must add a replace key. Otherwise both skyui and PNG icon injector will show same icons. Check `08_Status-Items.json` and [How to](howto.md) for examples.

## Match keys

| Key | Values | Matches |
| --- | --- | --- |
| `questItem` | `true` | Quest items |
| `newItem` | `true` | Items added since you last looked |
| `bestInClass` | `true` | Items with SkyUI's best-in-class diamond |
| `favorite` | `true` | Favorited items |
| `upgrade` | `true` | Items better than what you have equipped |
| `enchantment` | `Known`, `Unknown`, `NoDisenchant`, `Ability` | Learned or player made, can be learned, cannot be disenchanted, built-in power |
| `knownEffects` | `0` to `4`, or a list like `[1, 2]` | Ingredients by how many effects you know |
| `knownEffect` | An effect, see below | One icon per discovered ingredient effect |
| `museum` | `New`, `Found`, `Displayed` | Legacy of the Dragonborn museum state (needs The Curator's Companion) |

You may also combine these status icon keys with [Matching keys and values](matching.md) (`formType`, `keywordsAny`, `plugin` etc), except `iconLabel`. 

Example of an icon assigned for quest books only.

```json
{ "match": { "questItem": true, "formType": "Book" }, "statusIcon": "status/quest_book.png" }
```

## Ingredient effect icons

Ingredients shows an icon for each effect you have discovered. 

`knownEffect` need one of these effects - primaryValue, archtype, effectid(same as form id).

Match by primaryValue:

```json
{ "match": { "knownEffect": { "primaryValue": "FireResist" } }, "statusIcon": "effects/resist_fire.png" }
```

Match by archetype:

```json
{ "match": { "knownEffect": { "archetype": "Paralysis" } }, "statusIcon": "effects/paralysis.png" }
```

Single effect with effectID(same as form) (example Feather):

```json
{ "match": { "knownEffect": { "effectId": "Skyrim.esm|3EB01" } }, "statusIcon": "effects/feather.png", "priority": 20 }
```

Combined with a key from [Matching keys and values](matching.md), here only Dragonborn ingredients:

```json
{ "match": { "plugin": "Dragonborn.esm", "knownEffect": { "primaryValue": "Health" } }, "statusIcon": "effects/health_solstheim.png" }
```

Only the harmful version of an effect (Damage Health, not Restore Health), with your own colored art:

```json
{ "match": { "knownEffect": { "primaryValue": "Health", "detrimental": true } }, "statusIcon": "effects/damage_health.png", "priority": 20 }
```

- Harmful effects are tinted with red color in INI `sHarmfulEffectColor`. You may set `sHarmfulEffectColor = FFFFFF` to turn the tint off.
- You may also use `detrimental` rule to pick your own icons for harmful effects, this will disable the tint as well.
- Max icons per row: `iMaxEffectIcons`. Default 2.

### Archetypes (`archetype`)

| Family | Archetypes |
| --- | --- |
| Change a value | ValueModifier, PeakValueModifier, DualValueModifier, Absorb, AccumulateMagnitude, ValueAndParts |
| Cure | CureDisease, CureParalysis, CurePoison, CureAddiction, Dispel |
| Mind | Calm, Demoralize, Frenzy, Rally, CommandSummoned, TurnUndead, Banish |
| Senses | Invisibility, Light, Darkness, NightEye, DetectLife, Guide |
| Control | Paralysis, Stagger, Concussion, Disarm, Telekinesis, GrabActor, SlowTime, Etherealize |
| Summon and bound | SummonCreature, Reanimate, BoundWeapon, SoulTrap, SpawnHazard, SpawnScriptedRef |
| Weapon and armor | EnhanceWeapon, Cloak |
| Locks | Lock, Open |
| Transformation | Werewolf, WerewolfFeed, VampireLord, Disguise |
| Other | Script |

### Actor values (`primaryValue`)

| Family | Actor values |
| --- | --- |
| Attributes | Health, Magicka, Stamina, HealRate, MagickaRate, StaminaRate, HealRateMult, MagickaRateMult, StaminaRateMult |
| Combat skills | OneHanded, TwoHanded, Marksman, Block, HeavyArmor, LightArmor, Smithing |
| Magic skills | Alteration, Conjuration, Destruction, Illusion, Restoration, Enchanting |
| Stealth skills | Sneak, Lockpicking, Pickpocket, Speechcraft, Alchemy |
| Fortify skill | OneHandedMod, TwoHandedMod, MarksmanMod, BlockMod, HeavyArmorMod, LightArmorMod, SmithingMod, AlterationMod, ConjurationMod, DestructionMod, IllusionMod, RestorationMod, EnchantingMod, SneakMod, LockpickingMod, PickPocketMod, SpeechcraftMod, AlchemyMod |
| Fortify skill strength | OneHandedPowerMod, TwoHandedPowerMod, MarksmanPowerMod, BlockPowerMod, HeavyArmorPowerMod, LightArmorPowerMod, SmithingPowerMod, AlterationPowerMod, ConjurationPowerMod, DestructionPowerMod, IllusionPowerMod, RestorationPowerMod, EnchantingPowerMod, SneakPowerMod, LockpickingPowerMod, PickPocketPowerMod, SpeechcraftPowerMod, AlchemyPowerMod |
| Resistances | DamageResist, FireResist, FrostResist, ElectricResist, MagicResist, PoisonResist, DiseaseResist, AbsorbChance |
| Combat | AttackDamageMult, MeleeDamage, UnarmedDamage, WeaponSpeedMult, LeftWeaponSpeedMult, BowSpeedBonus, WardPower |
| Movement and carrying | SpeedMult, CarryWeight, MovementNoiseMult |
| Special states | Invisibility, NightEye, Paralysis, WaterBreathing, WaterWalking, ShoutRecoveryMult |

```json
{ "match": { "knownEffect": { "primaryValue": ["Sneak", "SneakMod", "SneakPowerMod"] } }, "statusIcon": "effects/sneak.png" }
```

## Shipped files

- Rules: `SKSE/Plugins/PNGIconInjector/07_Status-TCC.json` to `10_Status-Effects.json`
- PNGs: `Interface/PNGIconInjector/status/`
