#include "LootIcons.h"
#include "IconRules.h"
#include "TextureManager.h"

#include <mutex>

namespace LootIcons
{
	namespace
	{
		constexpr auto kMenu = "TrueHUD"sv;
		constexpr auto kProto = "_global.Widgets.TrueHUD_RecentLootEntry.prototype";
		constexpr auto kOriginalAddMessage = "pngii_lootOrigAddMessage";
		constexpr auto kOriginalOnLoadInit = "pngii_lootOrigOnLoadInit";
		constexpr auto kInstalledMember = "pngii_lootInstalled";
		constexpr std::int32_t kHolderDepth = 5000;
		// Both TrueHUD 1.1.9 and Norden UI set ItemIcon._width/_height = 24 in onLoadInit.
		constexpr double kCardIconSize = 24.0;
		constexpr double kLoadedImageSize = 64.0;
		constexpr RE::FormID kPlayer = 0x14;
		constexpr std::size_t kMaxPending = 32;
		constexpr auto kPendingLife = std::chrono::seconds(3);
		constexpr std::uint32_t kLoggedCalls = 8;

		struct Pending
		{
			RE::FormID                            form;
			std::chrono::steady_clock::time_point time;
		};

		bool                g_enabled = true;
		std::mutex          g_lock;
		std::deque<Pending> g_pending;

		double Num(const RE::GFxValue& a_obj, const char* a_member, double a_default)
		{
			RE::GFxValue v;
			return a_obj.GetMember(a_member, &v) && v.IsNumber() ? v.GetNumber() : a_default;
		}

		class ContainerSink final : public RE::BSTEventSink<RE::TESContainerChangedEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* a_event,
				RE::BSTEventSource<RE::TESContainerChangedEvent>*) override
			{
				if (!a_event || a_event->newContainer != kPlayer || a_event->oldContainer == kPlayer ||
					a_event->itemCount <= 0 || !a_event->baseObj) {
					return RE::BSEventNotifyControl::kContinue;
				}
				std::scoped_lock lock(g_lock);
				if (g_pending.size() >= kMaxPending) {
					g_pending.pop_front();
				}
				g_pending.push_back({ a_event->baseObj, std::chrono::steady_clock::now() });
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// Newest first; the matched entry is consumed so two cards of the same name pick two events.
		RE::FormID TakeForm(std::string_view a_name)
		{
			const auto now = std::chrono::steady_clock::now();
			std::scoped_lock lock(g_lock);
			while (!g_pending.empty() && now - g_pending.front().time > kPendingLife) {
				g_pending.pop_front();
			}
			for (auto it = g_pending.rbegin(); it != g_pending.rend(); ++it) {
				const auto* form = RE::TESForm::LookupByID(it->form);
				const char* name = form ? form->GetName() : nullptr;
				if (name && a_name == name) {
					const auto id = it->form;
					g_pending.erase(std::next(it).base());
					return id;
				}
			}
			return 0;
		}

		void Restore(RE::GFxValue& a_card, RE::GFxValue& a_icon, RE::GFxValue& a_holder)
		{
			if (a_holder.IsDisplayObject()) {
				a_holder.SetMember("_visible", RE::GFxValue(false));
			}
			RE::GFxValue hid;
			if (a_card.GetMember("pngii_lootHid", &hid) && hid.IsBool() && hid.GetBool()) {
				a_icon.SetMember("_visible", RE::GFxValue(true));
				a_card.SetMember("pngii_lootHid", RE::GFxValue(false));
			}
		}

		void Apply(RE::GFxMovie& a_movie, RE::GFxValue& a_card, const RE::GFxValue* a_args, std::uint32_t a_argCount, bool a_log)
		{
			RE::GFxValue icon;
			RE::GFxValue holder;
			if (!a_card.GetMember("ItemIcon", &icon) || !icon.IsDisplayObject()) {
				return;
			}
			a_card.GetMember("pngii_lootHolder", &holder);

			const std::string_view name = a_argCount > 0 && a_args[0].IsString() ? a_args[0].GetString() : ""sv;
			const std::string_view label = a_argCount > 2 && a_args[2].IsString() ? a_args[2].GetString() : ""sv;
			const RE::FormID       form = name.empty() ? 0 : TakeForm(name);
			const std::string* path = IconRules::Find(form, label, -1);

			RE::GFxValue list;
			const double listScale = a_card.GetMember("_parent", &list) && list.IsDisplayObject() ?
			                             Num(list, "_yscale", 100.0) / 100.0 :
			                             1.0;
			const std::string* url = path ? TextureManager::GetURL(*path, TextureManager::StageToScreenPx(kCardIconSize * listScale)) : nullptr;
			if (a_log) {
				logger::debug("loot card \"{}\" label {}: form {:08X} -> {}", name, label, form, url ? path->c_str() : "vanilla");
			}
			if (!url) {
				Restore(a_card, icon, holder);
				return;
			}
			if (!holder.IsDisplayObject() && !a_card.CreateEmptyMovieClip(&holder, "pngii_lootHolder", kHolderDepth)) {
				return;
			}

			const double scale = kCardIconSize / kLoadedImageSize * 100.0;
			holder.SetMember("_x", RE::GFxValue(Num(icon, "_x", 0.0)));
			holder.SetMember("_y", RE::GFxValue(Num(icon, "_y", 0.0)));
			holder.SetMember("_xscale", RE::GFxValue(scale));
			holder.SetMember("_yscale", RE::GFxValue(scale));

			RE::GFxValue current;
			if (!a_card.GetMember("pngii_lootUrl", &current) || !current.IsString() || *url != current.GetString()) {
				RE::GFxValue loader;
				a_movie.CreateObject(&loader, "MovieClipLoader");
				a_card.SetMember("pngii_lootLoader", loader);
				a_card.SetMember("pngii_lootUrl", RE::GFxValue(url->c_str()));
				std::array<RE::GFxValue, 2> args{ RE::GFxValue(url->c_str()), holder };
				loader.Invoke("loadClip", args);
			}
			holder.SetMember("_visible", RE::GFxValue(true));
			icon.SetMember("_visible", RE::GFxValue(false));
			a_card.SetMember("pngii_lootHid", RE::GFxValue(true));
		}

		class AddMessageWrapper final : public RE::GFxFunctionHandler
		{
		public:
			void Call(Params& a_params) override
			{
				if (!a_params.thisPtr) {
					return;
				}
				// Original first: it sets the label/tint (1.1.9) or starts setIcon's reload (Norden).
				a_params.thisPtr->Invoke(kOriginalAddMessage, a_params.retVal, a_params.args, a_params.argCount);

				static std::uint32_t logged = 0;
				const bool log = logged < kLoggedCalls && ++logged;
				Apply(*a_params.movie, *a_params.thisPtr, a_params.args, a_params.argCount, log);
			}
		};

		// onLoadInit sets ItemIcon._visible = true after our hide (both SWF variants).
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
				if (a_params.argCount < 1 || !a_params.thisPtr->GetMember("pngii_lootHid", &hid) || !hid.IsBool() || !hid.GetBool()) {
					return;
				}
				a_params.args[0].SetMember("_visible", RE::GFxValue(false));

				static std::uint32_t logged = 0;
				if (logged < kLoggedCalls) {
					++logged;
					logger::debug("loot card onLoadInit re-hid icon");
				}
			}
		};

		void Wrap(RE::GFxMovieView& a_view, RE::GFxValue& a_proto, const char* a_method,
			const char* a_originalMember, RE::GFxFunctionHandler* a_handler)
		{
			RE::GFxValue original;
			if (!a_proto.GetMember(a_method, &original) || !original.IsObject()) {
				logger::warn("{}: TrueHUD_RecentLootEntry.prototype.{} missing", kMenu, a_method);
				return;
			}
			RE::GFxValue wrapper;
			a_view.CreateFunction(&wrapper, a_handler);
			a_proto.SetMember(a_originalMember, original);
			a_proto.SetMember(a_method, wrapper);
			logger::info("{}: wrapped TrueHUD_RecentLootEntry.prototype.{}", kMenu, a_method);
		}

		bool InstallInto(RE::GFxMovieView& a_view)
		{
			RE::GFxValue proto;
			if (!a_view.GetVariable(&proto, kProto) || (!proto.IsObject() && !proto.IsDisplayObject())) {
				return false;
			}

			RE::GFxValue installed;
			if (proto.GetMember(kInstalledMember, &installed) && installed.IsBool() && installed.GetBool()) {
				return true;
			}

			static auto addMessageHandler = RE::make_gptr<AddMessageWrapper>();
			static auto loadInitHandler = RE::make_gptr<OnLoadInitWrapper>();
			Wrap(a_view, proto, "addMessage", kOriginalAddMessage, addMessageHandler.get());
			Wrap(a_view, proto, "onLoadInit", kOriginalOnLoadInit, loadInitHandler.get());
			proto.SetMember(kInstalledMember, RE::GFxValue(true));

			static bool stageLogged = false;
			if (!stageLogged) {
				stageLogged = true;
				if (const auto* def = a_view.GetMovieDef()) {
					logger::debug("{}: stage {} x {}", kMenu, def->GetWidth(), def->GetHeight());
				}
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
					logger::warn("{}: TrueHUD_RecentLootEntry prototype never became available", kMenu);
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
			logger::info("loot icons: off (INI bRecentLootIcons=0)");
			return;
		}
		static ContainerSink containerSink;
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESContainerChangedEvent>(&containerSink);
		static MenuSink menuSink;
		RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&menuSink);
		logger::info("loot icons: sinks registered");
	}
}
