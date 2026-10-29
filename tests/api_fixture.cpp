#include "IconAPI.h"

namespace
{
	const std::string* Find(std::uint32_t a_formID, std::string_view a_label, std::int32_t a_status)
	{
		static const std::string path = "Interface/PNGIconInjector/test/exported.png";
		return a_formID == 0x1234 && a_label == "weapon_sword" && a_status == -1 ? &path : nullptr;
	}

	const char* Label(std::uint32_t a_formID, std::int32_t)
	{
		return a_formID == 0x1234 ? "weapon_sword" : nullptr;
	}
}

extern "C" __declspec(dllexport) void PNGII_TestReady()
{
	IconAPI::SetLabeler(Label);
	IconAPI::SetResolver(Find);
}
