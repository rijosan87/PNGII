#pragma once

// Replaces the row icon in SkyUI item, magic, favorites and crafting menus with the PNG chosen by IconRules.
// Unmatched rows keep the vanilla icon.
namespace RowIcons
{
	void Install();
	void SetTiming(bool a_enabled);
	void PrewarmIcons();
}
