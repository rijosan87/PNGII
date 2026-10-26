#pragma once

// Optional Curator's Companion museum status (New/Found/Displayed) for status icon rules. None when TCC is missing.
namespace TCCItemStatus
{
	enum class Status { None, New, Found, Displayed };

	void Init();
	// True when the lists and setup globals resolved at kDataLoaded.
	bool Loaded();
	Status Get(RE::FormID a_itemFormID);
	const char* Name(Status a_status);
}
