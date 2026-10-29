#pragma once

#include <cstdint>
#include <string>
#include <string_view>

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
}
