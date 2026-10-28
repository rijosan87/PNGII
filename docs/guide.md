# Guide

## Folders

```text
Rules (JSON): Data/SKSE/Plugins/PNGIconInjector/MyIcons.json
PNG icons:    Data/Interface/PNGIconInjector/MyIcons/sword.png
```

- If you are adding new rules and icons, I suggest you keep them in a separate mod folder, named after your own mod.
- PNGs can be loose files or packed in a BSA that is loaded by an ESP/ESL.
- SVG files work anywhere a PNG does: point `icon` or `statusIcon` at a `.svg` path. SVGs are drawn on a square canvas,
  so non-square art is fitted and centered. `bSharpSVG=1` (default, `[Icons]` in `PNGIconInjector.ini`) draws them as crisp
  as a PNG; `0` rasterizes at the exact size, which looks softer.

## PNG size

| Icon | Source size | Drawn at 1080p |
| --- | --- | --- |
| Row icon | 128 px | about 27 px |
| Status icon | 128 px | about 24 px (`fStatusIconSize=0.7` in `PNGIconInjector.ini`) |
| Category tab | 128 px | 48 px |

At 4K the drawn size will be roughly double and it happens automatically, so you don't need a separate PNG.

## Rule file

```json
{
  "version": 1,
  "rules": [
    { "match": { "form": "Skyrim.esm|0x12EB7" }, "icon": "MyIcons/sword.png" }
  ]
}
```

| Part | Meaning |
| --- | --- |
| `"version": 1` | Required, always keep at 1. Otherwise file is skipped |
| `rules` | Your list of match rules, between square `[` and `]` |
| `match` | Item and icon mapping. See [Matching keys and values](matching.md) |
| `icon` | PNG or SVG image on the left side of any item, place them relative to `Interface/PNGIconInjector/` |
| `statusIcon` | PNG or SVG image used instead of `icon` for a status icon right side of item name. See [Status icons](status-icons.md) |

A file with three rules:

```json
{
  "version": 1,
  "rules": [
    // One specific item
    { "match": { "form": "Skyrim.esm|0x1397E" }, "icon": "MyIcons/iron_dagger.png" },
    // Every dagger
    { "match": { "keywordsAny": ["WeapTypeDagger"] }, "icon": "MyIcons/dagger.png" },
    // Every book
    { "match": { "formType": "Book" }, "icon": "MyIcons/book.png" }
  ]
}
```

When several rules match the same item only one of them wins. See [Matching keys and values](matching.md), section "Which rule wins".

## Form IDs

The form value should be plugin name and the form base ID. Check examples.

```text
"Skyrim.esm|0x12EB7"
"Skyrim.esm|12EB7"
"Skyrim.esm|0x00012EB7"
"MyMod.esp|0x05012EB7"
"MyLight.esl|0xFE000801"
```

All of these work: `0x` is optional. Even if you write down the full base ID, mod will ignore load order numbers safely.

You can put several items to assign the same icon using array like list.

```json
{ "match": { "form": ["Skyrim.esm|0x12EB7", "Skyrim.esm|0x1397E"] }, "icon": "MyIcons/iron.png" }
```

## Shipped rule files

| File | Purpose |
| --- | --- |
| `00_SkyUI-Labels.json` | Stock SkyUI labels |
| `01_I4-Labels.json` | Extra labels |
| `02_SkyUI-Extra-Labels.json` |  Extra labels |
| `03_Item-Categories.json` | Category rules |
| `04_Kittytail.json`, `05_Spells.json` | Spell mod icons |
| `06_KID-Groups.json` | Rules for shipped KID ini |
| `07` to `10_Status-*.json` | Status icons, see [Status icons](status-icons.md) |

## Check that it loaded

`PNGIconInjector.log` in the SKSE folder can be quite useful to check if the rules are loaded.

```text
MyIcons.json: 3 rules, 1 forms, 0 skipped
```
