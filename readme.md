# PNG Icon Injector

## Description
PNG Icon Injector replaces item and spell icons in the vanilla and SkyUI menus with PNG files
chosen by JSON rules. SVG files work too. No SWF editing. It works in the Inventory, Container, Barter, Gift, Magic,
Favorites and Crafting menus.

## Quick start

```text
Data/
  SKSE/Plugins/PNGIconInjector/MyIcons.json
  Interface/PNGIconInjector/MyPack/book.png
```

Every .json file in the rules folder is loaded and combined, so give your pack its own file name.
Icon paths are relative to Interface/PNGIconInjector/. 
Restart Skyrim after editing rules; there is no hot reload.

```json
{
  "version": 1,
  "rules": [
    { "match": { "formType": "Book" }, "icon": "MyPack/book.png" },
    { "match": { "form": "Skyrim.esm|0x12EB7" }, "icon": "MyPack/iron-sword.png" }
  ]
}
```

## Build

Requirements:
- Visual Studio 2022 with the "Desktop development with C++" workload
- [vcpkg](https://github.com/microsoft/vcpkg), with the `VCPKG_ROOT` environment variable set
- CMake 3.21 or newer (the one bundled with Visual Studio works)
- CommonLibSSE-NG v12.1.0 source, built together with the plugin (SE + AE)

Steps:
1. Clone CommonLibSSE-NG into `external/`:
   ```text
   git clone --depth 1 --branch v12.1.0 https://github.com/alandtse/CommonLibSSE-NG.git external/CommonLibSSE-NG
   ```
2. Open the project folder in Visual Studio 2022 (File > Open > Folder).
3. Select the `release` (or `debug`) configure preset and build. vcpkg installs the dependencies
   listed in `vcpkg.json` on the first configure.
4. The result is `PNGIconInjector.dll`. To copy it to a mod folder after each build, pass
   `-DOUTPUT_FOLDER=<mod folder>` to CMake; the DLL goes to `<mod folder>/SKSE/Plugins`.

## License

GPL-3.0. See [COPYING](COPYING).
