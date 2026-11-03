#pragma once

namespace Settings
{
	void Init();
	// Re-reads the INI on the UI thread; applies at the next menu open.
	void Reload();
	const char* Default(const char* a_section, const char* a_key);
}
