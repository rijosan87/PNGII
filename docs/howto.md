# How to

## Example - Replace a shipped icon

Always create a new mod folder (eg: MynewMod) to replace the existing images. No JSON edits needed.

```text
MynewMod/Interface/PNGIconInjector/base/weapon_dagger.png
```

## Example - Add Icon for an item

```json
{ "match": { "form": "Skyrim.esm|0x12EB7" }, "icon": "MyIcons/iron_sword.png" }
```

If several items need same icon, use the below format.

```json
{ "match": { "form": ["Skyrim.esm|0x12EB7", "Skyrim.esm|0x1397E"] }, "icon": "MyIcons/blade.png" }
```

## Example - Icon for the whole record type

```json
{ "match": { "formType": "Book" }, "icon": "MyIcons/book.png" }
```

Check the [Matching keys and values](matching.md) article to get a list of all Skyrim formTypes.

## Example - Icon for a SkyUI icon family

```json
{ "match": { "iconLabel": ["weapon_sword", "weapon_greatsword"] }, "icon": "MyIcons/blade.png" }
```

Check main mod - PNG Icon Injector\SKSE\Plugins\PNGIconInjector\00_SkyUI-Labels.json, here you will see all mapping clearly separated. 

## Example - Icon for every item from one mod

This will add the same icon to every form type of MyMod.esp plugin.
```json
{ "match": { "plugin": "MyMod.esp" }, "icon": "MyMod/item.png" }
```

Now you can also make icon to show on specific form type like spell as in the example below.

```json
{ "match": { "formType": "Spell", "plugin": "Arcanum.esp" }, "icon": "spells/arcanum.png" }
```

## Example - Icon for items with a keyword

Any of the keywords:

```json
{ "match": { "keywordsAny": ["WeapTypeWarAxe", "WeapTypeMace"] }, "icon": "MyIcons/blunt.png" }
```

All of the keywords:

```json
{ "match": { "keywordsAll": ["ArmorHeavy", "ArmorHelmet"] }, "icon": "MyIcons/heavy_helm.png" }
```

Keyword names are case-sensitive. Check [Matching keys and values](matching.md) for list of all default vanilla and DLC keywords.

## Example - Icon for a custom keyword (KID)

### KID

Let's say you are using KID files to add keyword to weapon in the format below:

`Data/MyMod_KID.ini` 

```ini
Keyword = MyModCursed|Weapon|*Cursed
```
So, once this INI is used, almost every weapon with a name cursed or its editor ID having the name cursed will get this keyword, right? Now you can use the same KID keyword "MyModCursed" to assign an icon for it using the below format.

```json
{ "match": { "keywordsAny": "MyModCursed" }, "icon": "MyMod/cursed.png" }
```

Here is one our mod ships. KID tags every armor whose editor ID contains "ClothesMGRobesArchmage" to PNGII_RobesArchmage.

`Data/PNGII_Armor_KID.ini` 
```ini
Keyword = PNGII_RobesArchmage|Armor|*ClothesMGRobesArchmage
```

So the rule below gives those arch mage robes their own icon.
```json
{ "match": { "keywordsAll": ["PNGII_RobesArchmage"] }, "icon": "base/clothing_robearchmage.png" }
```

Any tool or plugin that adds keywords works the same way.

## Example - Icon for an armor slot

Armor pieces use one or more body slots. Like backpack on slot 47. Now you can add rule that gives every backpack the same icon:

```json
{ "match": { "formType": "Armor", "parts": [47] }, "icon": "MyIcons/backpack.png" }
```

Outfits with a hood sometimes use slot 31 (hair) and slot 32 (body), so this rule gives every hooded robe one icon and skips plain robes:

```json
{ "match": { "formType": "Armor", "parts": [31, 32] }, "icon": "MyIcons/hooded_robe.png" }
```

Check the section Armor slots for Slot numbers in [Matching keys and values](matching.md).

## Example - Icon for spells and potions by their effect

By the effect's keyword: 

```json
{ "match": { "formType": "Spell", "effectKeywordsAny": ["MagicDamageFire"] }, "icon": "MyIcons/fire.png" }
```
Find the full list of magic effect keywords in [Matching keys and values](matching.md).

By strength:

```json
{ "match": { "formType": "Potion", "effectMagnitude": { "min": 50 } }, "icon": "MyIcons/strong.png" }
```

## Example - Icon for model path

Inject icons for items whose model path contains certain text "eg - ingot". Spells and shouts have no model, so keys don't match.

```json
{ "match": { "formType": "MiscItem", "modelContains": "ingot" }, "icon": "MyIcons/ingot.png" }
```

## Example - Icon for soul gems

Use `status` to show how full the soul gem is: `Empty`, `Partial` or `Full`. Keep `"formType": "SoulGem"` in the rule.

```json
{ "match": { "formType": "SoulGem", "status": "Empty" }, "icon": "MyIcons/soul_empty.png" }
```


## Example - Add High Priority

The higher priority match always wins. If it's a tie, the later rule wins (later in the file, or in a file that loads later).

```json
{ "match": { "keywordsAny": "MyModCursed" }, "icon": "MyMod/cursed.png", "priority": 150 }
```

## Status icons
A rule where all quest related items will show quest icon on the right in your inventory.
```json
{ "match": { "questItem": true }, "statusIcon": "status/quest_item.png" }
```

You can also replace one of SkyUI's own six status icons. Add `replace`, otherwise your icon is drawn next to SkyUI's. The list is in [Status icons](status-icons.md).

```json
{ "match": { "enchantment": "Unknown" }, "statusIcon": "status/enchant_learnable.png", "replace": "enchanted" }
```

## Category bar icon

Replace images in the category folder to change the icon design.

```text
Interface/PNGIconInjector/categories/
```

You may also look at the JSON file to understand the mapping better.

```
PNG Icon Injector\SKSE\Plugins\PNGIconInjector\11_Categories.json
```

Check the article [Category bar icons](category-icons.md).
