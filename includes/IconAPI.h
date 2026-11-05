#pragma once

#include "PNGII_API.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace IconAPI
{
	using Resolver = const std::string* (*)(std::uint32_t formID, std::string_view label,
		std::int32_t soulStatus);

	// Publish the matcher after keyword preparation; nullptr suspends queries.
	void SetResolver(Resolver a_resolver) noexcept;

	// Current matcher, nullptr until a save or new game has loaded.
	Resolver GetResolver() noexcept;

	// Derived SkyUI label: string literal or nullptr. V2 and the AS bridge only, never V1.
	using Labeler = const char* (*)(std::uint32_t formID, std::int32_t soulStatus);
	void SetLabeler(Labeler a_labeler) noexcept;

	// nullptr = kNotReady.
	struct MapProvider
	{
		// nullptr = no match.
		const std::string* (*resolve)(const PNGII::API::MapMarkerQuery& a_query);
		std::uint32_t (*count)();
		// a_index < count(); fills every field except structSize.
		void (*get)(std::uint32_t a_index, PNGII::API::MapMarkerIconInfo& a_info);
	};
	void SetMapProvider(const MapProvider* a_provider) noexcept;

	using FileReader = bool (*)(const char* a_path, std::vector<std::uint8_t>& a_out);
	void SetFileReader(FileReader a_reader) noexcept;
}
