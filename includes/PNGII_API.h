#pragma once

#include <cstdint>

// Public plugin interface. Include this header in the calling plugin and obtain
// PNGII_RequestAPI from PNGIconInjector.dll with GetProcAddress.
namespace PNGII::API
{
	inline constexpr std::uint32_t kVersion1 = 1;
	inline constexpr std::uint32_t kVersion2 = 2;
	inline constexpr char kExportName[] = "PNGII_RequestAPI";

	enum class Result : std::uint32_t
	{
		kSuccess = 0,
		kNoMatch = 1,
		kNotReady = 2,
		kBufferTooSmall = 3,
		kInvalidArgument = 4,
		kError = 5
	};

	// Call on Skyrim's UI thread after a save or new game has loaded.
	// formID: runtime FormID; 0 permits a label-only lookup.
	// iconLabel: NUL-terminated SkyUI/I4 label; nullptr means no label.
	// soulStatus: -1 unavailable, 0 empty, 1 partial, 2 full.
	// path: caller-owned output, relative to Data, including Interface/PNGIconInjector/.
	// capacity and requiredSize count bytes including the terminating NUL.
	// requiredSize is mandatory. Use path=nullptr and capacity=0 to query its size.
	// kSuccess copies the complete path. kBufferTooSmall supplies requiredSize.
	// Other results set requiredSize to 0. A writable path starts empty on failure.
	using ResolveIconFunction = Result (*)(std::uint32_t formID, const char* iconLabel,
		std::int32_t soulStatus, char* path, std::uint32_t capacity,
		std::uint32_t* requiredSize) noexcept;

	struct InterfaceV1
	{
		std::uint32_t version;
		std::uint32_t structSize;
		ResolveIconFunction resolveIcon;
	};

	// Version 2 only. SkyUI label PNGII derives from the form (the label an inventory row would carry with I4 off).
	// Call on Skyrim's UI thread after a save or new game has loaded. formID must be non-zero.
	// Same buffer contract and result codes as ResolveIconFunction.
	// kNoMatch: no SkyUI label for this form type, or unknown FormID.
	using GetIconLabelFunction = Result (*)(std::uint32_t formID, std::int32_t soulStatus,
		char* label, std::uint32_t capacity, std::uint32_t* requiredSize) noexcept;

	// Request kVersion2 and cast to const InterfaceV2*. Fields are only ever appended:
	// check structSize covers a field before calling it.
	struct InterfaceV2
	{
		std::uint32_t version;
		std::uint32_t structSize;
		// As V1, except a nullptr or empty iconLabel with a non-zero formID matches with the derived label.
		ResolveIconFunction resolveIcon;
		GetIconLabelFunction getIconLabel;
	};

	// The returned table remains valid while PNGIconInjector.dll is loaded.
	// Request kVersion1 or kVersion2 and cast the result to const InterfaceV1* or const InterfaceV2*.
	// An unsupported version returns nullptr. Interface discovery is thread-safe.
	using RequestAPIFunction = const void* (*)(std::uint32_t version) noexcept;
}
