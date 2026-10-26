#include "TCCItemStatus.h"

namespace TCCItemStatus
{
	namespace
	{
		constexpr auto kPlugin = "DBM_RelicNotifications.esp";
		RE::BGSListForm* g_new = nullptr;
		RE::BGSListForm* g_found = nullptr;
		RE::BGSListForm* g_displayed = nullptr;
		RE::TESGlobal* g_setupStart = nullptr;
		RE::TESGlobal* g_setupFinish = nullptr;
	}

	void Init()
	{
		auto* data = RE::TESDataHandler::GetSingleton();
		if (!data || (!data->LookupLoadedModByName(kPlugin) && !data->LookupLoadedLightModByName(kPlugin))) {
			logger::info("TCC item status unavailable: {} is not loaded", kPlugin);
			return;
		}
		// Reads TCC's lists directly (the ones it registers with moreHUD IE), so status icons do not depend on moreHUD. IDs checked against TCC v7.0.10.
		g_new = data->LookupForm<RE::BGSListForm>(0x558285, kPlugin);
		g_found = data->LookupForm<RE::BGSListForm>(0x558286, kPlugin);
		g_displayed = data->LookupForm<RE::BGSListForm>(0x558287, kPlugin);
		g_setupStart = data->LookupForm<RE::TESGlobal>(0x1DCE24, kPlugin);
		g_setupFinish = data->LookupForm<RE::TESGlobal>(0x1DCE25, kPlugin);
		if (!g_new || !g_found || !g_displayed || !g_setupStart || !g_setupFinish) {
			g_new = g_found = g_displayed = nullptr;
			g_setupStart = g_setupFinish = nullptr;
			logger::warn("TCC item status unavailable: status lists or setup globals are missing");
			return;
		}
		logger::info("TCC item status lists found");
	}

	bool Loaded()
	{
		return g_new != nullptr;
	}

	Status Get(RE::FormID a_itemFormID)
	{
		// TCC sets Finish only after it fills the lists; a loaded plugin alone does not mean its Papyrus state is ready.
		if (!a_itemFormID || !g_new || g_setupStart->value != 0.0f || g_setupFinish->value == 0.0f) {
			return Status::None;
		}
		// TCC moves forms between lists; prefer the furthest state if two briefly overlap. HasForm also reads scriptAddedTempForms.
		if (g_displayed->HasForm(a_itemFormID)) {
			return Status::Displayed;
		}
		if (g_found->HasForm(a_itemFormID)) {
			return Status::Found;
		}
		if (g_new->HasForm(a_itemFormID)) {
			return Status::New;
		}
		return Status::None;
	}

	const char* Name(Status a_status)
	{
		switch (a_status) {
		case Status::New:
			return "New";
		case Status::Found:
			return "Found";
		case Status::Displayed:
			return "Displayed";
		default:
			return "None";
		}
	}
}
