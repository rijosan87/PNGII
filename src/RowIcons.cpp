#include "RowIcons.h"
#include "IconRules.h"
#include "TextureManager.h"

namespace RowIcons
{
	namespace
	{
		enum class RowKind { kFormatItemIcon, kSetEntry };

		struct MenuHook
		{
			std::string_view menu;
			const char* proto;
			const char* className;
			const char* method;
			const char* listPath;
			RowKind kind;
		};

		constexpr auto kItemProto = "_global.InventoryListEntry.prototype";
		constexpr auto kItemList = "_root.Menu_mc.inventoryLists.itemList";
		constexpr std::array<MenuHook, 7> kMenus{ {
			{ RE::InventoryMenu::MENU_NAME, kItemProto, "InventoryListEntry", "formatItemIcon", kItemList, RowKind::kFormatItemIcon },
			{ RE::ContainerMenu::MENU_NAME, kItemProto, "InventoryListEntry", "formatItemIcon", kItemList, RowKind::kFormatItemIcon },
			{ RE::BarterMenu::MENU_NAME, kItemProto, "InventoryListEntry", "formatItemIcon", kItemList, RowKind::kFormatItemIcon },
			{ RE::GiftMenu::MENU_NAME, kItemProto, "InventoryListEntry", "formatItemIcon", kItemList, RowKind::kFormatItemIcon },
			{ RE::MagicMenu::MENU_NAME, kItemProto, "InventoryListEntry", "formatItemIcon", kItemList, RowKind::kFormatItemIcon },
			{ RE::FavoritesMenu::MENU_NAME, "_global.FavoritesListEntry.prototype", "FavoritesListEntry", "setEntry",
				"_root.MenuHolder.Menu_mc.itemList", RowKind::kSetEntry },
			{ RE::CraftingMenu::MENU_NAME, "_global.CraftingListEntry.prototype", "CraftingListEntry", "formatItemIcon",
				"_root.Menu.InventoryLists.panelContainer.itemList", RowKind::kFormatItemIcon },
		} };
		constexpr auto kOriginalMember = "pngii_origFormatItemIcon";
		constexpr auto kOriginalSetEntry = "pngii_origSetEntry";
		constexpr auto kOriginalOnLoadInit = "pngii_origOnLoadInit";
		constexpr auto kInstalledMember = "pngii_installed";
		constexpr std::int32_t kHolderDepth = 5000;
		constexpr double kFallbackIconSize = 18.0;
		constexpr double kFavoritesIconBoxSize = 128.0;
		// Plain SkyUI 6.11 box: 128 units at 16.942%, 33 px at 1080p. Reskins with another box correct the prewarm size after their first Favorites open.
		constexpr double kFavoritesFallbackBox = 21.7;
		constexpr double kLoadedImageSize = 64.0;
		constexpr std::uint32_t kLoggedCalls = 8;
		bool g_logTiming = false;
		IconRules::Timer g_rowTimer;
		using Clock = std::chrono::steady_clock;
		constexpr auto kBurstGap = std::chrono::milliseconds(4);
		struct Burst
		{
			std::uint64_t oursNs = 0;
			Clock::time_point start{};
			Clock::time_point end{};
			std::uint32_t rows = 0;
		};
		Burst g_burst;
		Burst g_worstBurst;
		std::uint32_t g_bursts = 0;
		RE::FormID g_slowestRowForm = 0;
		std::uint32_t g_lastDrawnPx = 0;
		std::uint32_t g_lastFavoritesPx = 0;
		std::uint32_t g_prewarmPx = 0;
		bool g_loggedPrewarmSize = false;
		constexpr auto kPrewarmBudget = std::chrono::milliseconds(4);
		struct Prewarm
		{
			std::vector<std::string> paths;
			std::size_t next = 0;
			std::uint32_t px = 0;
			std::uint32_t favoritesPx = 0;
			bool favoritesPass = false;
			std::uint32_t added = 0;
			std::uint32_t tasks = 0;
			std::uint64_t workNs = 0;
			TextureManager::Stats first{};
		};

		void RunPrewarm(std::shared_ptr<Prewarm> a_state)
		{
			SKSE::GetTaskInterface()->AddUITask([a_state]() {
				const auto start = Clock::now();
				if (a_state->tasks == 0) {
					a_state->first = TextureManager::GetStats();
				}
				++a_state->tasks;
				while (a_state->next < a_state->paths.size() && Clock::now() - start < kPrewarmBudget) {
					const auto before = TextureManager::GetStats();
					TextureManager::GetURL(a_state->paths[a_state->next++], a_state->px);
					const auto after = TextureManager::GetStats();
					a_state->added += (after.scaled + after.loadPNG) - (before.scaled + before.loadPNG);
				}
				a_state->workNs += std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
				if (a_state->next < a_state->paths.size()) {
					RunPrewarm(a_state);
					return;
				}
				logger::info("prewarm{}: {} icons at {} px, {} new, {:.1f} ms work over {} tasks",
					a_state->favoritesPass ? " Favorites" : "", a_state->paths.size(), a_state->px,
					a_state->added, a_state->workNs / 1e6, a_state->tasks);
				const auto end = TextureManager::GetStats();
				const auto svgN = end.svgDecoded - a_state->first.svgDecoded;
				const auto pngN = end.pngDecoded - a_state->first.pngDecoded;
				const auto svgMs = (end.svgDecodeNs - a_state->first.svgDecodeNs) / 1e6;
				const auto pngMs = (end.pngDecodeNs - a_state->first.pngDecodeNs) / 1e6;
				logger::info("prewarm decode: {} SVG {:.1f} ms (avg {:.2f}), {} PNG {:.1f} ms (avg {:.2f})",
					svgN, svgMs, svgN ? svgMs / svgN : 0.0, pngN, pngMs, pngN ? pngMs / pngN : 0.0);
				if (a_state->favoritesPx) {
					a_state->px = std::exchange(a_state->favoritesPx, 0);
					a_state->favoritesPass = true;
					a_state->next = 0;
					a_state->added = 0;
					a_state->tasks = 0;
					a_state->workNs = 0;
					RunPrewarm(a_state);
				}
			});
		}

		void CloseBurst()
		{
			if (g_burst.rows == 0) {
				return;
			}
			if (g_bursts == 0 || g_burst.oursNs > g_worstBurst.oursNs) {
				g_worstBurst = g_burst;
			}
			++g_bursts;
			g_burst = {};
		}

		double Num(const RE::GFxValue& a_obj, const char* a_member, double a_default)
		{
			RE::GFxValue v;
			return a_obj.GetMember(a_member, &v) && v.IsNumber() ? v.GetNumber() : a_default;
		}

		std::string_view Label(const RE::GFxValue& a_entry, RE::GFxValue& a_value)
		{
			if (!IconRules::HasLabelRules()) {
				return {};
			}
			return a_entry.GetMember("iconLabel", &a_value) && a_value.IsString() ? a_value.GetString() : "default_misc"sv;
		}

		std::int32_t Status(const RE::GFxValue& a_entry)
		{
			// SkyUI sets status only for soul gems; invalid values must not match Empty after conversion.
			if (!IconRules::HasStatusRules()) {
				return -1;
			}
			const auto status = Num(a_entry, "status", -1.0);
			return status == 0.0 || status == 1.0 || status == 2.0 ? static_cast<std::int32_t>(status) : -1;
		}

		const std::string* ResolveIcon(const RE::GFxValue& a_entry, double a_drawnSize, bool a_favorites)
		{
			const auto formID = static_cast<RE::FormID>(Num(a_entry, "formId", 0.0));
			RE::GFxValue labelValue;
			const auto label = Label(a_entry, labelValue);
			const auto* path = formID || !label.empty() ? IconRules::Find(formID, label, Status(a_entry)) : nullptr;
			const auto px = TextureManager::StageToScreenPx(a_drawnSize);
			if (a_favorites) {
				g_lastFavoritesPx = px;
			} else {
				g_lastDrawnPx = px;
				if (g_prewarmPx && px != g_prewarmPx && !g_loggedPrewarmSize) {
					g_loggedPrewarmSize = true;
					logger::debug("prewarm size {} px, rows draw {} px", g_prewarmPx, px);
				}
			}
			return path ? TextureManager::GetURL(*path, px) : nullptr;
		}

		// A finished loadClip shows the holder again, so a row recycled onto a no-PNG item meanwhile would show the old PNG; re-hide it.
		class HolderLoadInitHandler final : public RE::GFxFunctionHandler
		{
		public:
			void Call(Params& a_params) override
			{
				RE::GFxValue row;
				RE::GFxValue show;
				if (a_params.argCount < 1 || !a_params.args[0].IsDisplayObject() ||
					!a_params.args[0].GetMember("_parent", &row) || !row.IsDisplayObject() ||
					!row.GetMember("pngii_holderShow", &show) || !show.IsBool() || show.GetBool()) {
					return;
				}
				a_params.args[0].SetMember("_visible", RE::GFxValue(false));

				static std::uint32_t logged = 0;
				if (logged < kLoggedCalls) {
					++logged;
					logger::debug("holder onLoadInit re-hid stale PNG");
				}
			}
		};

		void Apply(RE::GFxMovie& a_movie, RE::GFxValue& a_row, RE::GFxValue& a_iconField, const RE::GFxValue& a_entry, bool a_favorites)
		{
			RE::GFxValue holder;
			a_row.GetMember("pngii_holder", &holder);

			// Item rows: only itemIcon._width is set by the column layout; _height and _yscale follow the frame content and pulse on selection redraws.
			const double drawnHeight = a_favorites ? kFavoritesIconBoxSize * Num(a_iconField, "_yscale", 0.0) / 100.0 : Num(a_iconField, "_width", 0.0);
			const double size = drawnHeight > 0.0 ? drawnHeight : kFallbackIconSize;

			const std::string* url = a_entry.IsObject() ? ResolveIcon(a_entry, size, a_favorites) : nullptr;
			if (!url) {
				a_row.SetMember("pngii_holderShow", RE::GFxValue(false));
				if (holder.IsDisplayObject()) {
					holder.SetMember("_visible", RE::GFxValue(false));
				}
				RE::GFxValue hid;
				if (a_row.GetMember("pngii_hidIcon", &hid) && hid.IsBool() && hid.GetBool()) {
					a_iconField.SetMember("_visible", RE::GFxValue(true));
					a_row.SetMember("pngii_hidIcon", RE::GFxValue(false));
				}
				return;
			}

			if (!holder.IsDisplayObject() && !a_row.CreateEmptyMovieClip(&holder, "pngii_holder", kHolderDepth)) {
				return;
			}

			const double scale = size / kLoadedImageSize * 100.0;
			double x = Num(a_iconField, "_x", 0.0);
			double y = Num(a_iconField, "_y", 0.0);
			// SkyUI Favorites icons are centre-registered in a 128-unit box; I4 moves itemIcon to the top-left and sets _iconPosFixed.
			if (a_favorites && !a_row.HasMember("_iconPosFixed")) {
				x -= size / 2.0;
				y -= size / 2.0;
			}
			holder.SetMember("_x", RE::GFxValue(x));
			holder.SetMember("_y", RE::GFxValue(y));
			holder.SetMember("_xscale", RE::GFxValue(scale));
			holder.SetMember("_yscale", RE::GFxValue(scale));
			if (a_favorites) {
				holder.SetMember("_alpha", RE::GFxValue(Num(a_iconField, "_alpha", 100.0)));
			}

			RE::GFxValue current;
			if (!a_row.GetMember("pngii_url", &current) || !current.IsString() || *url != current.GetString()) {
				RE::GFxValue loader;
				a_movie.CreateObject(&loader, "MovieClipLoader");
				static auto holderLoadHandler = RE::make_gptr<HolderLoadInitHandler>();
				RE::GFxValue listener;
				RE::GFxValue onLoadInit;
				a_movie.CreateObject(&listener);
				a_movie.CreateFunction(&onLoadInit, holderLoadHandler.get());
				listener.SetMember("onLoadInit", onLoadInit);
				std::array<RE::GFxValue, 1> listenerArgs{ listener };
				loader.Invoke("addListener", listenerArgs);
				a_row.SetMember("pngii_loader", loader);
				std::array<RE::GFxValue, 2> args{ RE::GFxValue(url->c_str()), holder };
				loader.Invoke("loadClip", args);
				a_row.SetMember("pngii_url", RE::GFxValue(url->c_str()));
			}

			a_row.SetMember("pngii_holderShow", RE::GFxValue(true));
			holder.SetMember("_visible", RE::GFxValue(true));
			a_iconField.SetMember("_visible", RE::GFxValue(false));
			a_row.SetMember("pngii_hidIcon", RE::GFxValue(true));
		}

		void UpdateRow(RE::GFxMovie& a_movie, RE::GFxValue& a_row, RE::GFxValue& a_iconField,
			const RE::GFxValue& a_entry, const char* a_tag, bool a_log, bool a_favorites, Clock::time_point a_callStart)
		{
			if (a_log) {
				RE::GFxValue text;
				a_entry.GetMember("text", &text);
				const auto formID = static_cast<RE::FormID>(Num(a_entry, "formId", 0.0));
				RE::GFxValue labelValue;
				const auto label = Label(a_entry, labelValue);
				const auto* origin = IconRules::FindOrigin(formID, label, Status(a_entry));
				logger::info("{} formId={:X} text=\"{}\" label=\"{}\" rule={}", a_tag, formID,
					text.IsString() ? text.GetString() : "", label, origin ? origin->c_str() : "none");
			}

			const auto start = g_logTiming ? Clock::now() : Clock::time_point{};
			Apply(a_movie, a_row, a_iconField, a_entry, a_favorites);
			if (g_logTiming) {
				const auto end = Clock::now();
				const auto ours = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
				if (static_cast<std::uint64_t>(ours) > g_rowTimer.maxNs) {
					g_slowestRowForm = static_cast<RE::FormID>(Num(a_entry, "formId", 0.0));
				}
				g_rowTimer.Add(ours);
				if (g_burst.rows == 0 || a_callStart - g_burst.end > kBurstGap) {
					CloseBurst();
					g_burst = { 0, a_callStart, end, 0 };
				}
				g_burst.oursNs += ours;
				g_burst.end = end;
				++g_burst.rows;
			}
		}

		class FormatItemIconWrapper final : public RE::GFxFunctionHandler
		{
		public:
			void Call(Params& a_params) override
			{
				if (!a_params.thisPtr || a_params.argCount < 2) {
					return;
				}
				const auto callStart = g_logTiming ? Clock::now() : Clock::time_point{};
				a_params.thisPtr->Invoke(kOriginalMember, a_params.retVal, a_params.args, a_params.argCount);

				static std::uint32_t logged = 0;
				const bool log = logged < kLoggedCalls && ++logged;
				UpdateRow(*a_params.movie, *a_params.thisPtr, a_params.args[0], a_params.args[1],
					"formatItemIcon", log, false, callStart);
			}
		};

		class SetEntryWrapper final : public RE::GFxFunctionHandler
		{
		public:
			void Call(Params& a_params) override
			{
				if (!a_params.thisPtr || a_params.argCount < 1) {
					return;
				}
				const auto callStart = g_logTiming ? Clock::now() : Clock::time_point{};
				a_params.thisPtr->Invoke(kOriginalSetEntry, a_params.retVal, a_params.args, a_params.argCount);

				RE::GFxValue icon;
				if (!a_params.thisPtr->GetMember("itemIcon", &icon) || !icon.IsDisplayObject()) {
					return;
				}
				static std::uint32_t logged = 0;
				const bool log = logged < kLoggedCalls && ++logged;
				UpdateRow(*a_params.movie, *a_params.thisPtr, icon, a_params.args[0], "setEntry", log, true, callStart);
			}
		};

		// I4's async icon SWF load re-shows itemIcon after our hide, so hide again in onLoadInit (a hide applied after loading sticks).
		class OnLoadInitWrapper final : public RE::GFxFunctionHandler
		{
		public:
			void Call(Params& a_params) override
			{
				if (!a_params.thisPtr) {
					return;
				}
				a_params.thisPtr->Invoke(kOriginalOnLoadInit, a_params.retVal, a_params.args, a_params.argCount);

				RE::GFxValue hid;
				if (a_params.argCount < 1 || !a_params.thisPtr->GetMember("pngii_hidIcon", &hid) || !hid.IsBool() || !hid.GetBool()) {
					return;
				}
				a_params.args[0].SetMember("_visible", RE::GFxValue(false));

				static std::uint32_t logged = 0;
				if (logged < kLoggedCalls) {
					++logged;
					logger::debug("onLoadInit re-hid itemIcon");
				}
			}
		};

		const MenuHook* FindMenu(const RE::BSFixedString& a_name)
		{
			for (const auto& menu : kMenus) {
				if (a_name == menu.menu) {
					return &menu;
				}
			}
			return nullptr;
		}

		void LogStageOnce(RE::GFxMovieView& a_view, std::string_view a_menu)
		{
			static std::unordered_set<std::string_view> logged;
			if (!logged.insert(a_menu).second) {
				return;
			}
			const auto visible = a_view.GetVisibleFrameRect();
			const auto* def = a_view.GetMovieDef();
			logger::debug("{} stage: visible height {:.1f}, movie height {:.1f}", a_menu,
				visible.bottom - visible.top, def ? def->GetHeight() : 0.0f);
		}

		void Wrap(RE::GFxMovieView& a_view, RE::GFxValue& a_proto, const MenuHook& a_hook, const char* a_method, const char* a_originalMember,
			RE::GFxFunctionHandler* a_handler)
		{
			RE::GFxValue original;
			if (!a_proto.GetMember(a_method, &original) || !original.IsObject()) {
				if (a_hook.kind == RowKind::kSetEntry && std::string_view(a_method) == "onLoadInit") {
					logger::info("{}: {}.prototype.{} absent without I4", a_hook.menu, a_hook.className, a_method);
				} else {
					logger::warn("{}: {}.prototype.{} missing", a_hook.menu, a_hook.className, a_method);
				}
				return;
			}
			RE::GFxValue wrapper;
			a_view.CreateFunction(&wrapper, a_handler);
			a_proto.SetMember(a_originalMember, original);
			a_proto.SetMember(a_method, wrapper);
			logger::info("{}: wrapped {}.prototype.{}", a_hook.menu, a_hook.className, a_method);
		}

		bool InstallInto(RE::GFxMovieView& a_view, const MenuHook& a_hook)
		{
			RE::GFxValue proto;
			if (!a_view.GetVariable(&proto, a_hook.proto) || (!proto.IsObject() && !proto.IsDisplayObject())) {
				return false;
			}
			LogStageOnce(a_view, a_hook.menu);

			RE::GFxValue installed;
			if (proto.GetMember(kInstalledMember, &installed) && installed.IsBool() && installed.GetBool()) {
				return true;
			}

			static auto formatHandler = RE::make_gptr<FormatItemIconWrapper>();
			static auto setEntryHandler = RE::make_gptr<SetEntryWrapper>();
			static auto loadInitHandler = RE::make_gptr<OnLoadInitWrapper>();
			const bool favorites = a_hook.kind == RowKind::kSetEntry;
			Wrap(a_view, proto, a_hook, a_hook.method, favorites ? kOriginalSetEntry : kOriginalMember,
				favorites ? static_cast<RE::GFxFunctionHandler*>(setEntryHandler.get()) : formatHandler.get());
			Wrap(a_view, proto, a_hook, "onLoadInit", kOriginalOnLoadInit, loadInitHandler.get());
			proto.SetMember(kInstalledMember, RE::GFxValue(true));

			RE::GFxValue list;
			if (a_view.GetVariable(&list, a_hook.listPath) && list.IsDisplayObject()) {
				list.Invoke("UpdateList");
			}
			return true;
		}

		void ScheduleInstall(const MenuHook& a_hook, std::uint32_t a_attemptsRemaining)
		{
			SKSE::GetTaskInterface()->AddUITask([a_hook, a_attemptsRemaining]() {
				auto* ui = RE::UI::GetSingleton();
				auto  view = ui ? ui->GetMovieView(a_hook.menu) : nullptr;
				if (view && InstallInto(*view, a_hook)) {
					return;
				}
				if (a_attemptsRemaining == 0) {
					logger::warn("{}: {} prototype never became available", a_hook.menu, a_hook.className);
					return;
				}
				ScheduleInstall(a_hook, a_attemptsRemaining - 1);
			});
		}

		class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
				RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				const auto* menu = a_event ? FindMenu(a_event->menuName) : nullptr;
				if (!menu) {
					return RE::BSEventNotifyControl::kContinue;
				}
				if (a_event->opening) {
					ScheduleInstall(*menu, 120);
				} else {
					logger::info("{} closed", menu->menu);
					IconRules::LogStats();
					if (g_logTiming) {
						CloseBurst();
						const auto resolve = IconRules::TakeResolveTiming();
						const auto averageUs = [](const IconRules::Timer& timer) {
							return timer.count ? static_cast<double>(timer.totalNs) / timer.count / 1000.0 : 0.0;
						};
						logger::info("{} timing: resolve n={} avg={:.1f}us max={:.1f}us | row n={} avg={:.1f}us max={:.1f}us slowest={:08X} | worst burst ours={:.2f}ms wall={:.2f}ms rows={} bursts={}",
							menu->menu, resolve.count, averageUs(resolve), resolve.maxNs / 1000.0,
							g_rowTimer.count, averageUs(g_rowTimer), g_rowTimer.maxNs / 1000.0, g_slowestRowForm,
							g_worstBurst.oursNs / 1000000.0,
							std::chrono::duration<double, std::milli>(g_worstBurst.end - g_worstBurst.start).count(),
							g_worstBurst.rows, g_bursts);
						g_rowTimer = {};
						g_slowestRowForm = 0;
						g_burst = {};
						g_worstBurst = {};
						g_bursts = 0;
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void PrewarmIcons()
	{
		auto state = std::make_shared<Prewarm>();
		state->paths = IconRules::IconPaths();
		if (state->paths.empty()) {
			return;
		}
		state->px = g_lastDrawnPx ? g_lastDrawnPx : TextureManager::StageToScreenPx(kFallbackIconSize);
		const auto favoritesPx = g_lastFavoritesPx ? g_lastFavoritesPx : TextureManager::StageToScreenPx(kFavoritesFallbackBox);
		state->favoritesPx = favoritesPx != state->px ? favoritesPx : 0;
		g_prewarmPx = state->px;
		RunPrewarm(state);
	}

	void SetTiming(bool a_enabled)
	{
		g_logTiming = a_enabled;
	}

	void Install()
	{
		static MenuSink sink;
		RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&sink);
		logger::info("menu sink registered");
	}
}
