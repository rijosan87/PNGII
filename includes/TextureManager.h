#pragma once

// Turns a PNG path (relative to Data) into an img:// URL Scaleform can draw. Every PNG goes through here.
// Back ends, in order: pre-scaled (DirectXTex decode, CPU mip chain, the level nearest above the drawn size uploaded
// as a D3D texture) and LoadPNG as a last resort. A path that fails both is cached as a miss.
namespace TextureManager
{
	// Cached existence check for a path relative to Data. Does not validate PNG decoding.
	bool Exists(const std::string& a_path);

	// a_targetPx: on-screen size the icon is drawn at (0 = full size). One registration per
	// path and uploaded level. nullptr when the PNG can't be loaded; callers keep the vanilla icon.
	const std::string* GetURL(const std::string& a_path, std::uint32_t a_targetPx = 0);

	struct Stats
	{
		std::uint32_t scaled;
		std::uint32_t loadPNG;
		std::uint32_t missed;
		std::size_t   vramBytes;
	};
	Stats GetStats();

	// Stage units to screen pixels. Item menu stages are 720 units tall, scaled to the screen
	// height (1.5 at 1080p). Shared by RowIcons and StatusIcons.
	std::uint32_t StageToScreenPx(double a_stageUnits);
}
