#include "ASBridge.h"
#include "CategoryIcons.h"
#include "LootIcons.h"
#include "MapMarkerIcons.h"
#include "MMF/Interfaces.h"
#include "QuickLootIcons.h"
#include "RowIcons.h"
#include "Settings.h"
#include "SettingsMenu.h"
#include "IconAPI.h"
#include "IconLabels.h"
#include "IconRules.h"
#include "Localization.h"
#include "NewItems.h"
#include "StatusIcons.h"
#include "TCCItemStatus.h"

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

	void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
	{
		if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
			TCCItemStatus::Init();
			IconRules::Load();
			// Label reads run behind the resolver gate, so KID keywords are in place by then.
			IconLabels::Init();
			IconAPI::SetLabeler(IconLabels::ForFormID);
			// Map rules need no keyword pass.
			IconAPI::SetMapProvider(IconRules::MapMarkerProvider());
			RowIcons::Install();
			CategoryIcons::Install();
			LootIcons::Install();
			QuickLootIcons::Install();
			MapMarkerIcons::Install();
			StatusIcons::Install();
			NewItems::Install();
			// Before Register: page names are translated at registration.
			char language[32]{};
			REX::W32::GetPrivateProfileStringA("Localization", "sLanguage", "en", language, sizeof(language), ".\\Data\\SKSE\\Plugins\\PNGIconInjector.ini");
			Localization::Load(language);
			SettingsMenu::Register();
		} else if (a_msg->type == SKSE::MessagingInterface::kPreLoadGame) {
			IconAPI::SetResolver(nullptr);
		} else if (a_msg->type == SKSE::MessagingInterface::kPostLoadGame ||
		           a_msg->type == SKSE::MessagingInterface::kNewGame) {
			IconAPI::SetResolver(nullptr);
			IconRules::ClearCache();
			// After every kDataLoaded distributor so KID/SkyPatcher keywords exist; keeps the one-time cost off the first inventory frame.
			IconRules::PrepareKeywords();
			IconAPI::SetResolver(IconRules::Find);
			RowIcons::PrewarmIcons();
			StatusIcons::PrewarmIcons();
		}
	}

	// SKSE only delivers to the first null-sender listener per plugin.
	void ForeignMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		// Never call GetVanillaMarkerType: null function pointer.
		MMF::MapMarkerInterface* coMAP = nullptr;
		MMF::QueryMapMarkerInterface(a_msg, coMAP);
		if (coMAP) {
			IconRules::SetCoMAP(coMAP);
			logger::info("CoMAP MapMarkerInterface acquired (version {})", coMAP->Version());
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	InitLog();
	Settings::Init();
	// NG initializes its own logger inside Init unless told not to; it would replace ours.
	SKSE::Init(a_skse, { .log = false });
	// SKSE only accepts the co-save unique ID during plugin load.
	NewItems::RegisterSerialization();
	// SKSE builds skse.plugins per movie at creation; register before the main menu movie.
	ASBridge::Register();

	logger::info("PNG Icon Injector v0.1.0, runtime {}", a_skse->RuntimeVersion().string());

	SKSE::GetMessagingInterface()->RegisterListener(MessageHandler);
	SKSE::GetMessagingInterface()->RegisterListener(nullptr, ForeignMessage);
	return true;
}
