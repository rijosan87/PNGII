#include "IconAPI.h"
#include "PNGII_API.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <vector>

extern "C" const void* PNGII_RequestAPI(std::uint32_t) noexcept;

namespace
{
	using PNGII::API::Result;
	std::string path = "Interface/PNGIconInjector/test/weapon.png";
	std::uint32_t calls = 0, lastForm = 0;
	std::string lastLabel;
	std::int32_t lastStatus = -1;
	bool match = true, fail = false;

	void Check(bool a_ok, const char* a_message)
	{
		if (!a_ok) {
			throw std::runtime_error(a_message);
		}
	}

	const std::string* Find(std::uint32_t a_form, std::string_view a_label, std::int32_t a_status)
	{
		++calls;
		lastForm = a_form;
		lastLabel = a_label;
		lastStatus = a_status;
		if (fail) {
			throw 42;
		}
		return match ? &path : nullptr;
	}
}

int main() try
{
	static_assert(std::is_standard_layout_v<PNGII::API::InterfaceV1>);
	static_assert(sizeof(Result) == sizeof(std::uint32_t));
	Check(!PNGII_RequestAPI(0) && !PNGII_RequestAPI(2), "Unsupported versions must return nullptr");
	const auto* api = static_cast<const PNGII::API::InterfaceV1*>(PNGII_RequestAPI(PNGII::API::kVersion1));
	Check(api && api->version == 1 && api->structSize == sizeof(*api) && api->resolveIcon, "Version 1 interface");
	Check(api == PNGII_RequestAPI(1), "Interface address must remain stable");

	std::array<char, 256> output{};
	std::uint32_t required = 99;
	output[0] = 'x';
	Check(api->resolveIcon(1, "weapon_sword", -1, output.data(), output.size(), &required) == Result::kNotReady,
		"Queries wait for the matcher");
	Check(required == 0 && output[0] == '\0' && calls == 0, "Not-ready output and matcher isolation");

	IconAPI::SetResolver(Find);
	Check(api->resolveIcon(0xFE012ABC, "custom_I4_label", -1, nullptr, 0, &required) == Result::kBufferTooSmall,
		"Size query");
	Check(required == path.size() + 1, "Size includes NUL");
	Check(lastForm == 0xFE012ABC && lastLabel == "custom_I4_label" && lastStatus == -1,
		"Runtime FormID and I4 label reach the matcher unchanged");

	std::vector<char> exact(required);
	Check(api->resolveIcon(0xFE012ABC, "custom_I4_label", -1, exact.data(), exact.size(), &required) == Result::kSuccess,
		"Exact-size buffer succeeds");
	Check(std::strcmp(exact.data(), path.c_str()) == 0 && exact.back() == '\0', "Complete NUL-terminated path");
	const auto copied = std::string(exact.data());
	path = "Interface/PNGIconInjector/test/changed.png";
	Check(std::string(exact.data()) == copied, "Caller owns the copied path");

	std::array<char, 8> guard;
	guard.fill('x');
	Check(api->resolveIcon(1, nullptr, -1, guard.data() + 1, 3, &required) == Result::kBufferTooSmall,
		"Short buffer reports size");
	Check(guard[0] == 'x' && guard[1] == '\0' &&
		std::all_of(guard.begin() + 2, guard.end(), [](char c) { return c == 'x'; }),
		"Short buffer stays empty and preserves surrounding bytes");
	Check(required == path.size() + 1, "Short buffer receives required size");
	guard.fill('x');
	Check(api->resolveIcon(1, nullptr, -1, guard.data(), 0, &required) == Result::kBufferTooSmall && guard[0] == 'x',
		"Zero-capacity buffer is untouched");

	Check(api->resolveIcon(0, "armor_hands", -1, output.data(), output.size(), &required) == Result::kSuccess &&
		lastForm == 0 && lastLabel == "armor_hands", "Label-only queries reach the matcher");
	for (const auto status : { -1, 0, 1, 2 }) {
		Check(api->resolveIcon(1, nullptr, status, output.data(), output.size(), &required) == Result::kSuccess &&
			lastLabel.empty() && lastStatus == status, "Form-only queries and soul status");
	}

	const auto beforeInvalid = calls;
	Check(api->resolveIcon(1, "label", -1, output.data(), output.size(), nullptr) == Result::kInvalidArgument,
		"Required-size pointer is mandatory");
	Check(api->resolveIcon(1, "label", -1, nullptr, 1, &required) == Result::kInvalidArgument && required == 0,
		"Null output requires zero capacity");
	for (const auto status : { -2, 3 }) {
		output[0] = 'x';
		required = 99;
		Check(api->resolveIcon(1, "label", status, output.data(), output.size(), &required) == Result::kInvalidArgument &&
			required == 0 && output[0] == '\0', "Invalid soul status clears outputs");
	}
	Check(calls == beforeInvalid, "Invalid arguments never reach the matcher");

	match = false;
	output[0] = 'x';
	required = 99;
	Check(api->resolveIcon(1, "label", -1, output.data(), output.size(), &required) == Result::kNoMatch &&
		required == 0 && output[0] == '\0', "Unmatched request clears stale output");
	match = true;
	path.clear();
	Check(api->resolveIcon(1, "label", -1, output.data(), output.size(), &required) == Result::kNoMatch,
		"Empty paths are unmatched");
	fail = true;
	output[0] = 'x';
	required = 99;
	Check(api->resolveIcon(1, "label", -1, output.data(), output.size(), &required) == Result::kError &&
		required == 0 && output[0] == '\0', "Matcher exceptions are contained at the DLL boundary");
	fail = false;

	IconAPI::SetResolver(nullptr);
	const auto beforeSuspend = calls;
	Check(api->resolveIcon(1, "label", -1, nullptr, 0, &required) == Result::kNotReady && calls == beforeSuspend,
		"Suspending the matcher stops lookups");
	IconAPI::SetResolver(Find);
	path = "Interface/PNGIconInjector/test/reloaded.png";
	Check(api->resolveIcon(1, "label", -1, output.data(), output.size(), &required) == Result::kSuccess &&
		std::string(output.data()) == path, "The same interface resumes after loading");
	std::cout << "PASS: version negotiation, readiness, query forwarding, buffer ownership, validation and errors\n";
} catch (const std::exception& error) {
	std::cerr << "FAIL: " << error.what() << '\n';
	return 1;
}
