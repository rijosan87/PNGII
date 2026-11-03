#pragma once

// Draws PNG status icons after the item name in Inventory, Container, Barter and Gift rows, from statusIcon rules.
// INI: [StatusIcons].
namespace StatusIcons
{
	void Install();
	void SetTiming(bool a_enabled);
	// Clamped so a typo cannot hide icons or spill them over neighbouring rows.
	inline constexpr double kMinSize = 0.3;
	inline constexpr double kMaxSize = 0.9;
	void SetSize(double a_fraction);
	// Safe before Install ran or without TCC: no paths, nothing queued.
	void PrewarmIcons();
	// Multiply tint on a loaded clip, 0xRRGGBB, 0 = identity. Keeps the clip's _alpha.
	void SetTint(RE::GFxMovie& a_movie, RE::GFxValue& a_child, std::uint32_t a_rgb);
}
