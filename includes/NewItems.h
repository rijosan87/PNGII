#pragma once

// Tracks base forms added to the player since last shown in an item menu, for newItem status rules. Saved in the co-save.
namespace NewItems
{
	// SKSEPluginLoad only: SKSE takes the unique ID during plugin load, before rules are loaded.
	void RegisterSerialization();
	// kDataLoaded after IconRules::Load.
	void Install();
	bool IsNew(RE::FormID a_formID);
	void MarkSeen(RE::FormID a_formID);
	// Item menu close: rows drawn this open stop being new.
	void CommitSeen();
}
