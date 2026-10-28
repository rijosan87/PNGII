#include "IconAPI.h"
#include "PNGII_API.h"

#include <atomic>
#include <cstring>
#include <limits>

namespace
{
	std::atomic<IconAPI::Resolver> g_resolver{ nullptr };

	PNGII::API::Result ResolveIcon(std::uint32_t a_formID, const char* a_label,
		std::int32_t a_soulStatus, char* a_path, std::uint32_t a_capacity,
		std::uint32_t* a_requiredSize) noexcept
	{
		using PNGII::API::Result;
		if (a_requiredSize) {
			*a_requiredSize = 0;
		}
		if (a_path && a_capacity) {
			a_path[0] = '\0';
		}
		if (!a_requiredSize || (!a_path && a_capacity) || a_soulStatus < -1 || a_soulStatus > 2) {
			return Result::kInvalidArgument;
		}
		const auto resolver = g_resolver.load(std::memory_order_acquire);
		if (!resolver) {
			return Result::kNotReady;
		}
		try {
			const auto* path = resolver(a_formID, a_label ? std::string_view(a_label) : std::string_view{}, a_soulStatus);
			if (!path || path->empty()) {
				return Result::kNoMatch;
			}
			if (path->size() >= (std::numeric_limits<std::uint32_t>::max)()) {
				return Result::kError;
			}
			*a_requiredSize = static_cast<std::uint32_t>(path->size() + 1);
			if (a_capacity < *a_requiredSize) {
				return Result::kBufferTooSmall;
			}
			std::memcpy(a_path, path->c_str(), *a_requiredSize);
			return Result::kSuccess;
		} catch (...) {
			return Result::kError;
		}
	}

	const PNGII::API::InterfaceV1 g_interface{
		PNGII::API::kVersion1, sizeof(PNGII::API::InterfaceV1), ResolveIcon
	};
}

void IconAPI::SetResolver(Resolver a_resolver) noexcept
{
	g_resolver.store(a_resolver, std::memory_order_release);
}

#ifdef _WIN32
extern "C" __declspec(dllexport)
#else
extern "C" __attribute__((visibility("default")))
#endif
const void* PNGII_RequestAPI(std::uint32_t a_version) noexcept
{
	return a_version == PNGII::API::kVersion1 ? &g_interface : nullptr;
}
