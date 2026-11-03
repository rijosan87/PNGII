# Map marker icons

PNG Icon Injector can draw your PNG or SVG over markers on the world map and the local map. Markers without a rule keep their own icon. CoMAP and Atlas icons are left alone unless a rule targets them.

## Add a rule file

Ship a rule file, `SKSE/Plugins/PNGIconInjector/MyMap.json`. Art goes under `Interface/PNGIconInjector/MyMap/`.

```json
{
  "version": 1,
  "mapMarkers": [
    { "match": { "markerType": "Mine" }, "icon": "MyMap/mine.png" },
    { "match": { "form": "Skyrim.esm|0x162A4" }, "icon": "MyMap/riverwood.png" },
    { "match": { "comapIcon": "TavernMarker" }, "icon": "MyMap/tavern.png" },
    { "match": { "map": "local", "markerType": "Door", "locTypeAny": "LocTypeInn" }, "icon": "MyMap/inn_door.png" },
    { "match": { "markerType": "Cave" }, "icon": "MyMap/cave.png", "iconUndiscovered": "MyMap/cave_undiscovered.png" }
  ]
}
```

Keep the icons square. Each one is drawn at the size of the icon it replaces.

## Match keys

All keys in one `match` must be true. Any key takes one value or a list.

| Key | Matches | Example |
| --- | --- | --- |
| `form` | One marker, or one door on the local map | `"Skyrim.esm\|0x162A4"` |
| `location` | Markers of a location; doors leading to it | `"Skyrim.esm\|0x18A56"` (Whiterun) |
| `comapIcon` | Markers CoMAP or Atlas gave an icon | `"GraveyardMarker"` |
| `locTypeAny` | Keyword on the location | `"LocTypeInn"` |
| `plugin` | Markers placed by a plugin | `"MyLocations.esp"` |
| `markerType` | Vanilla marker type, or `Door` for local map doors | `"City"`, `"Mine"`, `"Door"` |
| `map` | Only on `world` or `local` | `"local"` |

`map` cannot be used alone. Markers CoMAP re-iconed do not match `markerType`; use `comapIcon` for them.

## Rule fields

| Field | Meaning |
| --- | --- |
| `icon` | Required. Path under `Interface/PNGIconInjector/`. |
| `iconUndiscovered` | Optional. Icon for undiscovered markers. Without it, undiscovered markers keep the map's own icon. |
| `priority` | Optional. Higher wins. Defaults: `form` 100, `location` 50, `comapIcon` 40, `locTypeAny` 30, `plugin` 20, `markerType` 10. |

## Turn it off

`PNGIconInjector.ini`: `[MapMarkers] bMapMarkerIcons = 0`.

Quest markers, the player arrow and the compass are never changed.
