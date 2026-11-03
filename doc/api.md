# PNG Icon Injector API

Get the icon the player's rules pick for a form, from a Flash widget or an SKSE plugin.

## ActionScript

```actionscript
var url = _global.skse.plugins.PNGIconInjector.GetIcon(formId, iconLabel, size, soulStatus);
```

| Argument | Meaning |
|---|---|
| `formId` | FormID of the item, spell or effect. `0` if you only have a label. |
| `iconLabel` | Optional. Leave `undefined` unless you use your own label. |
| `size` | Drawn height. Default `64`. |
| `soulStatus` | Optional, soul gems: `0` empty, `1` partial, `2` full. |

Returns a URL to load, or `undefined`: keep your own icon.

```actionscript
var SIZE = 32;
var url = _global.skse.plugins.PNGIconInjector.GetIcon(entry.formId, undefined, SIZE);
if (url != undefined) {
    var holder = this.createEmptyMovieClip("pngiiIcon", this.getNextHighestDepth());
    holder._xscale = holder._yscale = SIZE / 64 * 100;  // image loads at 64 x 64
    new MovieClipLoader().loadClip(url, holder);
}
```

### Getting the FormID

Use the FormID the game gives you. Never hard-code one.

SkyUI item list:

```actionscript
var pngii = _global.skse.plugins.PNGIconInjector;
// In your entry renderer's setEntry(a_entryObject, a_state):
var url = pngii.GetIcon(a_entryObject.formId, a_entryObject.iconLabel, 32);
// Selected row:
var entry = itemList.selectedEntry;
var url = pngii.GetIcon(entry.formId, entry.iconLabel, 32);
```

Papyrus:

```papyrus
UI.InvokeInt("HUD Menu", "_root.MyWidget.showSpell", akSpell.GetFormID())
```

```actionscript
function showSpell(formId:Number):Void {
    var url = _global.skse.plugins.PNGIconInjector.GetIcon(formId, undefined, 32);
    if (url != undefined) new MovieClipLoader().loadClip(url, spellIcon);
}
```

SKSE plugin sending it to your widget:

```cpp
RE::GFxValue arg(static_cast<double>(spell->GetFormID()));
movie->Invoke("_root.MyWidget.showSpell", nullptr, &arg, 1);
```

ESL FormIDs may arrive negative. Pass them as they are.

### Notes

- Call on refresh, not every frame.
- Returns `undefined` before a save loads; call again after loading.
- `GetAPIVersion()` returns `2`.

## C++

Copy `PNGII_API.h` into your plugin.

```cpp
#include "PNGII_API.h"

const PNGII::API::InterfaceV2* g_pngii = nullptr;

// At kPostLoad or later:
if (auto dll = GetModuleHandleW(L"PNGIconInjector.dll")) {
    auto request = reinterpret_cast<PNGII::API::RequestAPIFunction>(
        GetProcAddress(dll, PNGII::API::kExportName));
    if (request) {
        g_pngii = static_cast<const PNGII::API::InterfaceV2*>(request(PNGII::API::kVersion2));
    }
}

// On the UI thread, after a save loads:
char path[260];
std::uint32_t needed = 0;
if (g_pngii && g_pngii->resolveIcon(form->GetFormID(), nullptr, -1, path, sizeof(path), &needed) ==
                   PNGII::API::Result::kSuccess) {
    // path is relative to Data, e.g. "Interface/PNGIconInjector/base/weapon_sword.svg"
}
```

| Function | Returns |
|---|---|
| `resolveIcon(formID, iconLabel, soulStatus, path, capacity, &needed)` | Icon path. `iconLabel` `nullptr` unless you use your own label. |
| `getIconLabel(formID, soulStatus, label, capacity, &needed)` | The SkyUI label for the form, e.g. `weapon_sword`. |

`soulStatus`: `-1` none, `0` empty, `1` partial, `2` full. `kBufferTooSmall` sets `needed`; retry with a bigger buffer.

## Custom labels

For things that aren't forms, make up a label and ship a rule file, `SKSE/Plugins/PNGIconInjector/MyWidget.json`.
Art goes under `Interface/PNGIconInjector/MyWidget/`.

```json
{
  "version": 1,
  "rules": [
    { "match": { "iconLabel": "mywidget_fire" }, "icon": "MyWidget/fire.png" }
  ]
}
```

```actionscript
var url = _global.skse.plugins.PNGIconInjector.GetIcon(0, "mywidget_fire", 24);
```

```cpp
g_pngii->resolveIcon(0, "mywidget_fire", -1, path, sizeof(path), &needed);
```
