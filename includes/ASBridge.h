#pragma once

namespace ASBridge
{
	inline constexpr std::uint32_t kAPIVersion = 2;

	// SKSEPluginLoad, after SKSE::Init. SKSE adds the object to each movie created after this.
	void Register();
}
