#pragma once

// Replaces world and local map marker icons with the PNG chosen by mapMarkers rules.
namespace MapMarkerIcons
{
	void SetMapsEnabled(bool a_world, bool a_local);
	// 0xRRGGBB multiply tint for world and local map marker PNGs, 0 = untinted.
	void SetTints(std::uint32_t a_world, std::uint32_t a_local);
	// After IconRules::Load.
	void Install();
}
