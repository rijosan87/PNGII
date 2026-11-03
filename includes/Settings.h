#pragma once

namespace Settings
{
	void Init();
	// Re-reads the INI on the UI thread; applies at the next menu open.
	void Reload();
}
