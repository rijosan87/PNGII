#pragma once

namespace IconLabels
{
	// kDataLoaded: keyword pointers by FormID.
	void Init();
	// String literal or nullptr. a_soulStatus -1 derives the soul gem fill from the base record.
	const char* ForFormID(std::uint32_t a_formID, std::int32_t a_soulStatus);
}
