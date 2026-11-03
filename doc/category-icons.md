# Category icons

PNG Icon Injector changes Category tab icons in SkyUI to the PNGs in `Interface/PNGIconInjector/categories/`. To change one, put a PNG with the same name in your own mod.

## Point a category at a different PNG

Rule format:

```json
{
  "version": 1,
  "categoryIcons": {
    "weapon": "MyIcons/gear.png",
    "armor": "MyIcons/gear.png"
  }
}
```

## Category names

`11_Categories.json` is the shipped example.

| Menu | Names |
| --- | --- |
| Inventory | `cat_favorites`, `inv_all`, `inv_weapons`, `inv_armor`, `inv_potions`, `inv_scrolls`, `inv_food`, `inv_ingredients`, `inv_books`, `inv_keys`, `inv_misc` |
| Container, Barter, Gift | Inventory names without `cat_favorites` |
| Magic | `cat_favorites`, `mag_all`, `mag_alteration`, `mag_illusion`, `mag_destruction`, `mag_conjuration`, `mag_restoration`, `mag_shouts`, `mag_powers`, `mag_activeeffects` |
| Forge, smelter, tanning rack | `construct_all`, `weapon`, `ammo`, `armor`, `jewelry`, `food`, `misc` |
| Arcane enchanter | `ench_disentchant`, `ench_item`, `ench_effect`, `ench_soul` |
| Alchemy lab | `ingredients`, `beneficial`, `harmful`, `other` |
| Forge with C.O.C.K.S | `all`, `weapon_waraxe`, `armor_head`, `clothing_body`, `armor_ring`, `default_food`, `misc_ingot` |
