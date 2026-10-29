#include "PNGII_API.h"
#include <Windows.h>

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
	void Check(bool a_ok, const char* a_message)
	{
		if (!a_ok) {
			throw std::runtime_error(a_message);
		}
	}
}

int main(int argc, char** argv) try
{
	using PNGII::API::Result;
	Check(argc == 2, "Fixture DLL path required");
	const auto module = LoadLibraryA(argv[1]);
	Check(module != nullptr, "Load fixture DLL");
	const auto request = reinterpret_cast<PNGII::API::RequestAPIFunction>(GetProcAddress(module, PNGII::API::kExportName));
	Check(request && !request(0) && !request(3), "Undecorated export and version negotiation");
	const auto* api = static_cast<const PNGII::API::InterfaceV1*>(request(PNGII::API::kVersion1));
	Check(api && api->version == 1 && api->structSize == sizeof(*api) && api->resolveIcon, "Interface layout across DLL boundary");
	std::uint32_t required = 0;
	Check(api->resolveIcon(0x1234, "weapon_sword", -1, nullptr, 0, &required) == Result::kNotReady, "Initial readiness");
	const auto ready = reinterpret_cast<void (*)()>(GetProcAddress(module, "PNGII_TestReady"));
	Check(ready != nullptr, "Fixture setup export");
	ready();
	Check(api->resolveIcon(0x1234, "weapon_sword", -1, nullptr, 0, &required) == Result::kBufferTooSmall, "Size query across DLL boundary");
	std::vector<char> path(required);
	Check(api->resolveIcon(0x1234, "weapon_sword", -1, path.data(), path.size(), &required) == Result::kSuccess,
		"Copy to caller's allocation across DLL boundary");
	Check(std::string(path.data()) == "Interface/PNGIconInjector/test/exported.png", "Returned PNG path");
	const auto* v2 = static_cast<const PNGII::API::InterfaceV2*>(request(PNGII::API::kVersion2));
	Check(v2 && v2->version == 2 && v2->structSize == sizeof(*v2) && v2->resolveIcon && v2->getIconLabel,
		"Version 2 layout across DLL boundary");
	std::vector<char> derived(64);
	Check(v2->resolveIcon(0x1234, nullptr, -1, derived.data(), derived.size(), &required) == Result::kSuccess &&
		std::string(derived.data()) == "Interface/PNGIconInjector/test/exported.png", "V2 derives the label");
	Check(api->resolveIcon(0x1234, nullptr, -1, derived.data(), derived.size(), &required) == Result::kNoMatch,
		"V1 never derives the label");
	Check(v2->getIconLabel(0x1234, -1, derived.data(), derived.size(), &required) == Result::kSuccess &&
		std::string(derived.data()) == "weapon_sword", "V2 label copy");
	FreeLibrary(module);
	Check(std::string(path.data()) == "Interface/PNGIconInjector/test/exported.png", "Copied path survives provider unload");
	std::cout << "PASS: Windows DLL discovery, version negotiation and caller-owned path\n";
} catch (const std::exception& error) {
	std::cerr << "FAIL: " << error.what() << '\n';
	return 1;
}
