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
}
