#pragma once

// Replaces world and local map marker icons with the PNG chosen by mapMarkers rules.
namespace MapMarkerIcons
{
	void SetEnabled(bool a_enabled);
	// After IconRules::Load.
	void Install();
}
