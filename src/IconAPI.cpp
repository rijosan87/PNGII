#include "IconAPI.h"
#include "PNGII_API.h"

#include <atomic>
#include <cstring>
#include <limits>

namespace
{
	using PNGII::API::Result;

	std::atomic<IconAPI::Resolver> g_resolver{ nullptr };
	std::atomic<IconAPI::Labeler> g_labeler{ nullptr };

	Result CopyOut(std::string_view a_text, char* a_out, std::uint32_t a_capacity, std::uint32_t* a_requiredSize)
	{
		if (a_text.size() >= (std::numeric_limits<std::uint32_t>::max)()) {
			return Result::kError;
		}
		*a_requiredSize = static_cast<std::uint32_t>(a_text.size() + 1);
		if (a_capacity < *a_requiredSize) {
			return Result::kBufferTooSmall;
		}
		std::memcpy(a_out, a_text.data(), a_text.size());
		a_out[a_text.size()] = '\0';
		return Result::kSuccess;
	}

	// Shared by both tables; a_derive is false for V1, whose behaviour is frozen.
	Result Query(std::uint32_t a_formID, const char* a_label, std::int32_t a_soulStatus, char* a_path,
		std::uint32_t a_capacity, std::uint32_t* a_requiredSize, bool a_derive) noexcept
	{
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
			std::string_view label = a_label ? std::string_view(a_label) : std::string_view{};
			if (a_derive && label.empty() && a_formID) {
				if (const auto labeler = g_labeler.load(std::memory_order_acquire)) {
					if (const char* derived = labeler(a_formID, a_soulStatus)) {
						label = derived;
					}
				}
			}
			const auto* path = resolver(a_formID, label, a_soulStatus);
			if (!path || path->empty()) {
				return Result::kNoMatch;
			}
			const auto result = CopyOut(*path, a_path, a_capacity, a_requiredSize);
			if (result == Result::kError) {
				*a_requiredSize = 0;
			}
			return result;
		} catch (...) {
			*a_requiredSize = 0;
			return Result::kError;
		}
	}

	Result ResolveIcon(std::uint32_t a_formID, const char* a_label, std::int32_t a_soulStatus, char* a_path,
		std::uint32_t a_capacity, std::uint32_t* a_requiredSize) noexcept
	{
		return Query(a_formID, a_label, a_soulStatus, a_path, a_capacity, a_requiredSize, false);
	}

	Result ResolveIconV2(std::uint32_t a_formID, const char* a_label, std::int32_t a_soulStatus, char* a_path,
		std::uint32_t a_capacity, std::uint32_t* a_requiredSize) noexcept
	{
		return Query(a_formID, a_label, a_soulStatus, a_path, a_capacity, a_requiredSize, true);
	}

	Result GetIconLabel(std::uint32_t a_formID, std::int32_t a_soulStatus, char* a_label, std::uint32_t a_capacity,
		std::uint32_t* a_requiredSize) noexcept
	{
		if (a_requiredSize) {
			*a_requiredSize = 0;
		}
		if (a_label && a_capacity) {
			a_label[0] = '\0';
		}
		if (!a_requiredSize || (!a_label && a_capacity) || !a_formID || a_soulStatus < -1 || a_soulStatus > 2) {
			return Result::kInvalidArgument;
		}
		// Same gate as resolveIcon: keyword checks need KID/SkyPatcher additions, present only after a load.
		if (!g_resolver.load(std::memory_order_acquire)) {
			return Result::kNotReady;
		}
		try {
			const auto labeler = g_labeler.load(std::memory_order_acquire);
			const char* label = labeler ? labeler(a_formID, a_soulStatus) : nullptr;
			if (!label || !*label) {
				return Result::kNoMatch;
			}
			const auto result = CopyOut(label, a_label, a_capacity, a_requiredSize);
			if (result == Result::kError) {
				*a_requiredSize = 0;
			}
			return result;
		} catch (...) {
			*a_requiredSize = 0;
			return Result::kError;
		}
	}

	const PNGII::API::InterfaceV1 g_interface{
		PNGII::API::kVersion1, sizeof(PNGII::API::InterfaceV1), ResolveIcon
	};

	const PNGII::API::InterfaceV2 g_interfaceV2{
		PNGII::API::kVersion2, sizeof(PNGII::API::InterfaceV2), ResolveIconV2, GetIconLabel
	};
}

void IconAPI::SetResolver(Resolver a_resolver) noexcept
{
	g_resolver.store(a_resolver, std::memory_order_release);
}

IconAPI::Resolver IconAPI::GetResolver() noexcept
{
	return g_resolver.load(std::memory_order_acquire);
}

void IconAPI::SetLabeler(Labeler a_labeler) noexcept
{
	g_labeler.store(a_labeler, std::memory_order_release);
}

#ifdef _WIN32
extern "C" __declspec(dllexport)
#else
extern "C" __attribute__((visibility("default")))
#endif
const void* PNGII_RequestAPI(std::uint32_t a_version) noexcept
{
	switch (a_version) {
	case PNGII::API::kVersion1:
		return &g_interface;
	case PNGII::API::kVersion2:
		return &g_interfaceV2;
	default:
		return nullptr;
	}
}
