# Plugin icon API

Other SKSE plugins can query the PNG selected by PNG Icon Injector's item and spell rules. Queries use a runtime FormID, a SkyUI or I4 icon label, and an optional soul-gem status. The returned path is relative to `Data`, for example `Interface/PNGIconInjector/MyIcons/sword.png`.

Version 2 (labels worked out from the FormID, `getIconLabel`) and the ActionScript API: see [API](api.md).

## Discover the interface

Include [`PNGII_API.h`](../includes/PNGII_API.h) in the calling plugin. It requires C++17 and the standard library headers supplied by the compiler.

Use `GetModuleHandleW(L"PNGIconInjector.dll")` and `GetProcAddress(module, PNGII::API::kExportName)` to find `PNGII_RequestAPI`. Cast the export to `PNGII::API::RequestAPIFunction`, request `PNGII::API::kVersion1`, and cast the returned pointer to `const PNGII::API::InterfaceV1*`. Check the returned version, structure size and function pointer before using the interface. An unsupported version returns `nullptr`.

The table remains valid for the DLL's lifetime. Interface discovery can run on any thread. Icon queries run on Skyrim's UI thread; SKSE's `AddUITask` can schedule them there.

## Resolve an icon

PNGII enables queries after preparing rules and keywords for a loaded save or new game. Queries return `kNotReady` before that point and while loading another save. Retry after loading completes.

Pass the item's current runtime FormID. For I4 artwork, first obtain the label assigned by I4's `ProcessEntry`, then pass that label to `resolveIcon`. A FormID of `0` supports label-only requests, including category artwork for actions such as an empty hand. A null label supports form-only requests.

Set `soulStatus` to `-1` when unavailable. Soul gems use `0` for empty, `1` for partial and `2` for full. PNGII applies its existing rule priorities and file ordering to every query.

The caller supplies the output buffer and a required-size pointer. All sizes count bytes, including the final NUL. A size query uses a null buffer and zero capacity:

```cpp
#include "PNGII_API.h"
#include <vector>

// Run on the UI thread after discovering and validating api.
void ReadIcon(const PNGII::API::InterfaceV1& api, std::uint32_t formID,
    const char* i4Label)
{
    using PNGII::API::Result;
    std::uint32_t required = 0;
    const auto result = api.resolveIcon(formID, i4Label, -1, nullptr, 0, &required);
    if (result == Result::kBufferTooSmall) {
        std::vector<char> path(required);
        if (api.resolveIcon(formID, i4Label, -1, path.data(),
                static_cast<std::uint32_t>(path.size()), &required) == Result::kSuccess) {
            // Load or copy path.data() while this caller-owned buffer is in scope.
        }
    }
    // Handle kNotReady by retrying after loading; kNoMatch uses the caller's fallback.
}
```

| Result | Meaning |
| --- | --- |
| `kSuccess` | The complete NUL-terminated path was copied to the supplied buffer. |
| `kNoMatch` | The request has no matching item-icon rule. |
| `kNotReady` | Rule queries become available after save/new-game preparation. |
| `kBufferTooSmall` | `requiredSize` contains the necessary buffer size. |
| `kInvalidArgument` | Supply `requiredSize`, use a buffer for nonzero capacity, and use a supported soul status. |
| `kError` | The query failed internally. The caller can use its fallback artwork. |

`requiredSize` is zero for every result except `kSuccess` and `kBufferTooSmall`. When a writable output buffer is supplied, its first byte is set to NUL before lookup. The caller owns the copied path and can retain it for subsequent rendering.

## Load the image

Read the returned game-resource path with `RE::BSResourceNiBinaryStream` to support loose files and loaded BSAs, then decode and render the PNG in the consuming plugin. Keep the caller's fallback available for unmatched rules and image-loading failures. PNGII's result identifies the selected artwork; decoding and GPU resource ownership belong to the consuming renderer.

## Run the API tests

```text
cmake -S tests -B build/api-tests
cmake --build build/api-tests --config Release
ctest --test-dir build/api-tests -C Release --output-on-failure
```

The standalone tests cover the query contract. On Windows, a separate consumer also discovers the exported API from a test DLL and retrieves a path into its own buffer. Runtime verification compares queried paths against PNGII's menu icons for form, keyword, I4-label and soul-gem rules, including another save load.
