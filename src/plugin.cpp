#include "CategoryIcons.h"
#include "LootIcons.h"
#include "RowIcons.h"
#include "IconRules.h"
#include "NewItems.h"
#include "StatusIcons.h"
#include "TCCItemStatus.h"
#include "TextureManager.h"
#include <REX/W32/KERNEL32.h>
#include <SimpleIni.h>

namespace
{
	void InitLog()
	{
		auto path = SKSE::log::log_directory();
		if (!path) {
			return;
		}
		*path /= "PNGIconInjector.log";

		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
		auto log = std::make_shared<spdlog::logger>("global log", std::move(sink));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::debug);
		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%H:%M:%S] [%l] %v");
	}

	struct IniDefault
	{
		const char* section;
		const char* key;
		const char* value;
		const char* comment;
	};

	void EnsureIni(const char* a_path)
	{
		constexpr IniDefault kDefaults[] = {
			{ "Debug", "sLogLevel", "info",
				"; Log level: debug, info or off. Debug adds one line per icon and other diagnostics." },
			{ "Debug", "bLogMatches", "0",
				"; 1 = log which rule each item matched, once per item. For rule authors." },
			{ "Debug", "bLogTiming", "0",
				"; 1 = log menu and rule timing when a menu closes." },
			{ "StatusIcons", "bShowIngredientProgress", "1",
				"; 1 = show the learned-effects progress icon on ingredients, 0 = hide it." },
			{ "StatusIcons", "bShowEffectIcons", "1",
				"; 1 = show discovered-effect icons on ingredients, 0 = hide them." },
			{ "StatusIcons", "iMaxEffectIcons", "2",
				"; Effect icons per ingredient, 1 to 4." },
			{ "StatusIcons", "sHarmfulEffectColor", "CC3333",
				"; Tint for harmful effect icons, hex RRGGBB. FFFFFF = no tint." },
			{ "StatusIcons", "fStatusIconSize", "0.7",
				"; Status icon height as a fraction of the row height, 0.3 to 0.9. 0.5 = DIII size, 0.7 = about 24 px at 1080p." },
			{ "Icons", "bSharpSVG", "1",
				"; 1 = SVG icons drawn crisp like PNGs (2x raster, then downscaled), 0 = rasterized at the exact size, softer." },
			{ "TrueHUD", "bRecentLootIcons", "1",
				"; 1 = show rule icons on TrueHUD Recent Loot cards, 0 = keep TrueHUD's icons." },
		};

		CSimpleIniA ini;
		ini.SetUnicode();
		const bool existed = std::filesystem::exists(a_path);
		if (existed) {
			if (const SI_Error rc = ini.LoadFile(a_path); rc < SI_OK) {
				logger::error("PNGIconInjector.ini unreadable (SimpleIni error {}); not rewriting it", static_cast<int>(rc));
				return;
			}
		}

		std::size_t added = 0;
		for (const auto& d : kDefaults) {
			if (!ini.GetValue(d.section, d.key)) {
				ini.SetValue(d.section, d.key, d.value, d.comment);
				++added;
			}
		}
		if (existed && added == 0) {
			return;
		}

		std::error_code ec;
		std::filesystem::create_directories(std::filesystem::path(a_path).parent_path(), ec);
		if (ec) {
			logger::error("Failed to create the PNGIconInjector.ini directory: {}", ec.message());
			return;
		}
		if (const SI_Error rc = ini.SaveFile(a_path); rc < SI_OK) {
			logger::error("Failed to write PNGIconInjector.ini (SimpleIni error {})", static_cast<int>(rc));
		} else {
			logger::info("PNGIconInjector.ini {}: {} default key(s) added", existed ? "updated" : "created", added);
		}
	}

	void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
	{
		if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
			TCCItemStatus::Init();
			IconRules::Load();
			RowIcons::Install();
			CategoryIcons::Install();
			LootIcons::Install();
			StatusIcons::Install();
			NewItems::Install();
		} else if (a_msg->type == SKSE::MessagingInterface::kPostLoadGame ||
		           a_msg->type == SKSE::MessagingInterface::kNewGame) {
			IconRules::ClearCache();
			// After every kDataLoaded distributor so KID/SkyPatcher keywords exist; keeps the one-time cost off the first inventory frame.
			IconRules::PrepareKeywords();
			RowIcons::PrewarmIcons();
			StatusIcons::PrewarmIcons();
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	InitLog();
	char buf[16]{};
	constexpr auto kIni = ".\\Data\\SKSE\\Plugins\\PNGIconInjector.ini";
	EnsureIni(kIni);
	char levelBuf[16]{};
	REX::W32::GetPrivateProfileStringA("Debug", "sLogLevel", "info", levelBuf, sizeof(levelBuf), kIni);
	if (std::string_view(levelBuf) == "debug") {
		spdlog::set_level(spdlog::level::debug);
	} else if (std::string_view(levelBuf) == "off") {
		spdlog::set_level(spdlog::level::off);
	}
	REX::W32::GetPrivateProfileStringA("Debug", "bLogMatches", "0", buf, sizeof(buf), kIni);
	const bool logMatches = std::string_view(buf) == "1";
	REX::W32::GetPrivateProfileStringA("Debug", "bLogTiming", "0", buf, sizeof(buf), kIni);
	const bool logTiming = std::string_view(buf) == "1";
	IconRules::SetDebug(logMatches, logTiming);
	REX::W32::GetPrivateProfileStringA("StatusIcons", "bShowIngredientProgress", "1", buf, sizeof(buf), kIni);
	const bool showProgress = std::string_view(buf) != "0";
	REX::W32::GetPrivateProfileStringA("StatusIcons", "bShowEffectIcons", "1", buf, sizeof(buf), kIni);
	const bool showEffects = std::string_view(buf) != "0";
	REX::W32::GetPrivateProfileStringA("StatusIcons", "iMaxEffectIcons", "", buf, sizeof(buf), kIni);
	std::uint32_t maxEffectIcons = IconRules::kDefaultMaxEffectIcons;
	std::from_chars(buf, buf + std::string_view(buf).size(), maxEffectIcons);
	maxEffectIcons = (std::min)(maxEffectIcons, IconRules::kMaxEffectIcons);
	REX::W32::GetPrivateProfileStringA("StatusIcons", "sHarmfulEffectColor", "CC3333", buf, sizeof(buf), kIni);
	std::string_view colorText(buf);
	if (colorText.starts_with('#')) {
		colorText.remove_prefix(1);
	} else if (colorText.starts_with("0x") || colorText.starts_with("0X")) {
		colorText.remove_prefix(2);
	}
	std::uint32_t harmColor = 0xCC3333;
	if (const auto parsed = std::from_chars(colorText.data(), colorText.data() + colorText.size(), harmColor, 16);
		parsed.ec != std::errc{} || parsed.ptr != colorText.data() + colorText.size() || harmColor > 0xFFFFFF) {
		harmColor = 0xCC3333;
	}
	const std::uint32_t harmTint = harmColor == 0xFFFFFF ? 0 : harmColor;
	IconRules::SetIngredientIcons(showProgress, showEffects ? maxEffectIcons : 0, harmTint);
	REX::W32::GetPrivateProfileStringA("StatusIcons", "fStatusIconSize", "0.7", buf, sizeof(buf), kIni);
	double statusSize = 0.7;
	std::from_chars(buf, buf + std::string_view(buf).size(), statusSize);
	StatusIcons::SetSize(statusSize);
	REX::W32::GetPrivateProfileStringA("Icons", "bSharpSVG", "1", buf, sizeof(buf), kIni);
	const bool sharpSVG = std::string_view(buf) != "0";
	TextureManager::SetSharpSVG(sharpSVG);
	REX::W32::GetPrivateProfileStringA("TrueHUD", "bRecentLootIcons", "1", buf, sizeof(buf), kIni);
	const bool lootIcons = std::string_view(buf) != "0";
	LootIcons::SetEnabled(lootIcons);
	RowIcons::SetTiming(logTiming);
	StatusIcons::SetTiming(logTiming);
	logger::info("debug: logMatches={} logTiming={}, ingredient progress={} effects={} iMaxEffectIcons={} harmTint={:06X} statusIconSize={} recentLootIcons={} sharpSVG={}", logMatches, logTiming,
		showProgress, showEffects, maxEffectIcons, harmTint, std::clamp(statusSize, StatusIcons::kMinSize, StatusIcons::kMaxSize), lootIcons, sharpSVG);
	// NG initializes its own logger inside Init unless told not to; it would replace ours.
	SKSE::Init(a_skse, { .log = false });
	// SKSE only accepts the co-save unique ID during plugin load.
	NewItems::RegisterSerialization();

	logger::info("PNG Icon Injector v0.1.0, runtime {}", a_skse->RuntimeVersion().string());

	SKSE::GetMessagingInterface()->RegisterListener(MessageHandler);
	return true;
}
