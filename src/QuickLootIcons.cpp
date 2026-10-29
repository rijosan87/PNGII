#include "QuickLootIcons.h"
#include "IconRules.h"
#include "TextureManager.h"

namespace QuickLootIcons
{
	namespace
	{
		constexpr auto kMenu = "LootMenu"sv;
		constexpr auto kProto = "_global.QuickLoot.ListItemRenderer.prototype";
		constexpr auto kItemList = "_root.lootMenu.itemList";
		constexpr auto kLootMenu = "_root.lootMenu";
		constexpr auto kOriginalSetData = "pngii_qlOrigSetData";
		constexpr auto kOriginalOnLoadInit = "pngii_qlOrigOnLoadInit";
		constexpr auto kInstalledMember = "pngii_qlInstalled";
		// Inside the row clip; well above QuickLoot's own children.
		constexpr std::int32_t kHolderDepth = 5000;
		// ListItemRenderer.ICON_SIZE; QuickLoot's onLoadInit forces itemIcon to 24 x 24 row units.
		constexpr double kRowIconSize = 24.0;
		// BSScaleformImageLoader::LoadImage reports every registered image as 64x64, so the loaded clip is always 64 units square.
		constexpr double kLoadedImageSize = 64.0;
		constexpr std::uint32_t kLoggedCalls = 8;
		// StageToScreenPx assumes InventoryMenu's 720-unit stage.
		constexpr double kInventoryStageHeight = 720.0;

		bool g_enabled = true;
		// LootMenuIE.swf stage is 1920 x 1080. Set from the movie def at install.
		double g_stageHeight = kInventoryStageHeight;

		double Num(const RE::GFxValue& a_obj, const char* a_member, double a_default)
		{
			RE::GFxValue v;
			return a_obj.GetMember(a_member, &v) && v.IsNumber() ? v.GetNumber() : a_default;
		}

		bool Bool(const RE::GFxValue& a_obj, const char* a_member, bool a_default)
		{
			RE::GFxValue v;
			return a_obj.GetMember(a_member, &v) && v.IsBool() ? v.GetBool() : a_default;
		}

		// SkyUI InventoryDataSetter.processSoulGemStatus. QuickLoot's labels already carry the fill state,
		// but `status` rules need the number.
		std::int32_t SoulStatus(RE::FormID a_form, const RE::GFxValue& a_data)
		{
			if (!IconRules::HasStatusRules()) {
				return -1;
			}
			const auto* gem = RE::TESForm::LookupByID<RE::TESSoulGem>(a_form);
			if (!gem) {
				return -1;
			}
			const auto soul = Num(a_data, "soulLVL", 0.0);
			if (soul <= 0.0) {
				return 0;
			}
			const auto capacity = static_cast<double>(static_cast<std::uint32_t>(gem->GetMaximumCapacity()));
			return soul >= capacity ? 2 : 1;
		}

		// Rows are pooled CLIK renderers, so a row that drew a PNG earlier must get QuickLoot's icon back
		// when the next item has no rule.
		void Restore(RE::GFxValue& a_row, RE::GFxValue& a_icon, RE::GFxValue& a_holder)
		{
			a_row.SetMember("pngii_qlShow", RE::GFxValue(false));
			if (a_holder.IsDisplayObject()) {
				a_holder.SetMember("_visible", RE::GFxValue(false));
			}
			if (Bool(a_row, "pngii_qlHid", false)) {
				a_icon.SetMember("_visible", RE::GFxValue(true));
				a_row.SetMember("pngii_qlHid", RE::GFxValue(false));
			}
		}

		// A finished loadClip shows the holder again. A row recycled onto an item without a PNG while the
		// load was in flight would show our old PNG over QuickLoot's icon.
		class HolderLoadInitHandler final : public RE::GFxFunctionHandler
		{
		public:
			void Call(Params& a_params) override
			{
				// onLoadInit(target); target is the holder, its _parent the row.
				RE::GFxValue row;
				if (a_params.argCount < 1 || !a_params.args[0].IsDisplayObject() ||
					!a_params.args[0].GetMember("_parent", &row) || !row.IsDisplayObject() ||
					Bool(row, "pngii_qlShow", true)) {
					return;
				}
				a_params.args[0].SetMember("_visible", RE::GFxValue(false));

				static std::uint32_t logged = 0;
				if (logged < kLoggedCalls) {
					++logged;
					logger::debug("loot row holder onLoadInit re-hid stale PNG");
				}
			}
		};

		void Apply(RE::GFxMovie& a_movie, RE::GFxValue& a_row, const RE::GFxValue& a_data, bool a_log)
		{
			RE::GFxValue icon;
			RE::GFxValue holder;
			RE::GFxValue lootMenu;
			if (!a_row.GetMember("itemIcon", &icon) || !icon.IsDisplayObject()) {
				return;
			}
			a_row.GetMember("pngii_qlHolder", &holder);
			a_movie.GetVariable(&lootMenu, kLootMenu);
			if (!a_data.IsObject() || (lootMenu.IsObject() && !Bool(lootMenu, "showItemIcons", true))) {
				Restore(a_row, icon, holder);
				return;
			}

			const auto form = static_cast<RE::FormID>(Num(a_data, "formId", 0.0));
			RE::GFxValue labelValue;
			const std::string_view label = a_data.GetMember("iconLabel", &labelValue) && labelValue.IsString() ? labelValue.GetString() : ""sv;
			const std::string* path = form || !label.empty() ? IconRules::Find(form, label, SoulStatus(form, a_data)) : nullptr;
			// updateScale() scales the whole lootMenu clip by the user's window scale; the row icon scales with it.
			const double menuScale = lootMenu.IsDisplayObject() ? Num(lootMenu, "_yscale", 100.0) / 100.0 : 1.0;
			const std::string* url = path ? TextureManager::GetURL(*path, TextureManager::StageToScreenPx(kRowIconSize * menuScale * kInventoryStageHeight / g_stageHeight)) : nullptr;
			if (a_log) {
				logger::debug("loot row label {}: form {:08X} -> {}", label, form, url ? path->c_str() : "vanilla");
			}
			if (!url) {
				Restore(a_row, icon, holder);
				return;
			}
			if (!holder.IsDisplayObject() && !a_row.CreateEmptyMovieClip(&holder, "pngii_qlHolder", kHolderDepth)) {
				return;
			}

			const double scale = kRowIconSize / kLoadedImageSize * 100.0;
			holder.SetMember("_x", RE::GFxValue(Num(icon, "_x", 0.0)));
			holder.SetMember("_y", RE::GFxValue(Num(icon, "_y", 0.0)));
			holder.SetMember("_xscale", RE::GFxValue(scale));
			holder.SetMember("_yscale", RE::GFxValue(scale));

			// loadClip replaces the holder's contents and members, so the URL and loader live on the row.
			// Reload only when the pooled row moves to a different PNG.
			RE::GFxValue current;
			if (!a_row.GetMember("pngii_qlUrl", &current) || !current.IsString() || *url != current.GetString()) {
				RE::GFxValue loader;
				a_movie.CreateObject(&loader, "MovieClipLoader");
				// Own listener object: the row's onLoadInit is QuickLoot's itemIcon handler.
				static auto holderLoadHandler = RE::make_gptr<HolderLoadInitHandler>();
				RE::GFxValue listener;
				RE::GFxValue onLoadInit;
				a_movie.CreateObject(&listener);
				a_movie.CreateFunction(&onLoadInit, holderLoadHandler.get());
				listener.SetMember("onLoadInit", onLoadInit);
				std::array<RE::GFxValue, 1> listenerArgs{ listener };
				loader.Invoke("addListener", listenerArgs);
				a_row.SetMember("pngii_qlLoader", loader);
				std::array<RE::GFxValue, 2> args{ RE::GFxValue(url->c_str()), holder };
				loader.Invoke("loadClip", args);
				a_row.SetMember("pngii_qlUrl", RE::GFxValue(url->c_str()));
			}

			a_row.SetMember("pngii_qlShow", RE::GFxValue(true));
			holder.SetMember("_visible", RE::GFxValue(true));
			icon.SetMember("_visible", RE::GFxValue(false));
			a_row.SetMember("pngii_qlHid", RE::GFxValue(true));
		}

		class SetDataWrapper final : public RE::GFxFunctionHandler
		{
		public:
			void Call(Params& a_params) override
			{
				// setData(data); thisPtr is the row renderer.
				if (!a_params.thisPtr) {
					return;
				}
				// Original first: I4 relabels data, setItemIcon starts its load, then itemIcon._visible = true.
				a_params.thisPtr->Invoke(kOriginalSetData, a_params.retVal, a_params.args, a_params.argCount);
				// setData(null) returns early in QuickLoot; reset() already hid the row.
				if (a_params.argCount < 1 || !a_params.args[0].IsObject()) {
					return;
				}

				static std::uint32_t logged = 0;
				const bool log = logged < kLoggedCalls && ++logged;
				Apply(*a_params.movie, *a_params.thisPtr, a_params.args[0], log);
			}
		};

		// QuickLoot's onLoadInit runs after an icon source SWF change; re-hide itemIcon if we hid it.
		class OnLoadInitWrapper final : public RE::GFxFunctionHandler
		{
		public:
			void Call(Params& a_params) override
			{
				if (!a_params.thisPtr) {
					return;
				}
				a_params.thisPtr->Invoke(kOriginalOnLoadInit, a_params.retVal, a_params.args, a_params.argCount);

				if (a_params.argCount < 1 || !Bool(*a_params.thisPtr, "pngii_qlHid", false)) {
					return;
				}
				a_params.args[0].SetMember("_visible", RE::GFxValue(false));

				static std::uint32_t logged = 0;
				if (logged < kLoggedCalls) {
					++logged;
					logger::debug("loot row onLoadInit re-hid icon");
				}
			}
		};

		bool Wrap(RE::GFxMovieView& a_view, RE::GFxValue& a_proto, const char* a_method,
			const char* a_originalMember, RE::GFxFunctionHandler* a_handler)
		{
			RE::GFxValue original;
			if (!a_proto.GetMember(a_method, &original) || !original.IsObject()) {
				logger::warn("{}: QuickLoot.ListItemRenderer.prototype.{} missing", kMenu, a_method);
				return false;
			}
			RE::GFxValue wrapper;
			a_view.CreateFunction(&wrapper, a_handler);
			a_proto.SetMember(a_originalMember, original);
			a_proto.SetMember(a_method, wrapper);
			logger::info("{}: wrapped QuickLoot.ListItemRenderer.prototype.{}", kMenu, a_method);
			return true;
		}

		bool InstallInto(RE::GFxMovieView& a_view)
		{
			RE::GFxValue proto;
			if (!a_view.GetVariable(&proto, kProto) || (!proto.IsObject() && !proto.IsDisplayObject())) {
				return false;
			}

			// QuickLoot may cache the movie across opens (EnableMenuCaching); the marker stops double wrapping.
			if (Bool(proto, kInstalledMember, false)) {
				return true;
			}

			// Before the redraw below, or the first rows size against 720.
			if (const auto* def = a_view.GetMovieDef(); def && def->GetHeight() > 0.0f) {
				g_stageHeight = def->GetHeight();
				static bool stageLogged = false;
				if (!stageLogged) {
					stageLogged = true;
					logger::debug("{}: stage {} x {}", kMenu, def->GetWidth(), def->GetHeight());
				}
			}

			static auto setDataHandler = RE::make_gptr<SetDataWrapper>();
			static auto loadInitHandler = RE::make_gptr<OnLoadInitWrapper>();
			const bool wrapped = Wrap(a_view, proto, "setData", kOriginalSetData, setDataHandler.get());
			Wrap(a_view, proto, "onLoadInit", kOriginalOnLoadInit, loadInitHandler.get());
			// Marked even if a method was missing, so a broken prototype isn't retried 120 times.
			proto.SetMember(kInstalledMember, RE::GFxValue(true));

			// Rows filled by QuickLoot's Show() before this UI task ran redraw through the wrapper.
			RE::GFxValue list;
			if (wrapped && a_view.GetVariable(&list, kItemList) && list.IsObject()) {
				list.Invoke("invalidateData");
			}
			return true;
		}

		void ScheduleInstall(std::uint32_t a_attemptsRemaining)
		{
			SKSE::GetTaskInterface()->AddUITask([a_attemptsRemaining]() {
				auto* ui = RE::UI::GetSingleton();
				auto  view = ui ? ui->GetMovieView(kMenu) : nullptr;
				if (view && InstallInto(*view)) {
					return;
				}
				if (a_attemptsRemaining == 0) {
					logger::warn("{}: QuickLoot.ListItemRenderer prototype never became available", kMenu);
					return;
				}
				ScheduleInstall(a_attemptsRemaining - 1);
			});
		}

		class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
				RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (a_event && a_event->opening && a_event->menuName == kMenu) {
					ScheduleInstall(120);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void SetEnabled(bool a_enabled)
	{
		g_enabled = a_enabled;
	}

	void Install()
	{
		if (!g_enabled) {
			logger::info("QuickLoot icons: off (INI bLootMenuIcons=0)");
			return;
		}
		static MenuSink menuSink;
		RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&menuSink);
		logger::info("QuickLoot icons: sink registered");
	}
}
