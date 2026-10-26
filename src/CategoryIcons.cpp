#include "CategoryIcons.h"
#include "IconRules.h"
#include "TextureManager.h"

namespace CategoryIcons
{
	namespace
	{
		constexpr auto kOriginalSetEntry = "pngii_catOrigSetEntry";
		constexpr auto kOriginalOnLoadInit = "pngii_catOrigOnLoadInit";
		constexpr auto kInstalledMember = "pngii_catInstalled";
		constexpr std::int32_t kHolderDepth = 5000;
		constexpr double kFallbackIconSize = 32.0;
		constexpr double kLoadedImageSize = 64.0;
		constexpr std::uint32_t kLoggedCalls = 8;

		struct Target
		{
			std::string_view menu;
			const char*      proto;
			const char*      className;
			const char*      list;
			bool             hasOnLoadInit;
			bool             alchemyColumn;
		};
		constexpr auto kInvProto = "_global.CategoryListEntry.prototype";
		constexpr auto kInvList = "_root.Menu_mc.inventoryLists.categoryList";
		constexpr std::array<Target, 7> kTargets{ {
			{ RE::InventoryMenu::MENU_NAME, kInvProto, "CategoryListEntry", kInvList, true, false },
			{ RE::ContainerMenu::MENU_NAME, kInvProto, "CategoryListEntry", kInvList, true, false },
			{ RE::BarterMenu::MENU_NAME, kInvProto, "CategoryListEntry", kInvList, true, false },
			{ RE::GiftMenu::MENU_NAME, kInvProto, "CategoryListEntry", kInvList, true, false },
			{ RE::MagicMenu::MENU_NAME, kInvProto, "CategoryListEntry", kInvList, true, false },
			{ RE::CraftingMenu::MENU_NAME, "_global.IconTabListEntry.prototype", "IconTabListEntry",
				"_root.Menu.InventoryLists.CategoriesList", false, false },
			{ RE::CraftingMenu::MENU_NAME, "_global.skyui.components.list.ButtonListEntry.prototype", "ButtonListEntry",
				"_root.Menu.InventoryLists.CategoriesList", false, true },
		} };
		constexpr double kDisabledAlpha = 35.0;
		constexpr double kFallbackAlchemySize = 16.0;

		double Num(const RE::GFxValue& a_obj, const char* a_member, double a_default)
		{
			RE::GFxValue v;
			return a_obj.GetMember(a_member, &v) && v.IsNumber() ? v.GetNumber() : a_default;
		}

		std::string ResolvePath(std::string_view a_label)
		{
			if (const auto* mapped = IconRules::FindCategoryIcon(a_label)) {
				return *mapped;
			}
			auto path = std::format("Interface/PNGIconInjector/categories/{}.png", a_label);
			return TextureManager::Exists(path) ? path : std::string{};
		}

		void Restore(RE::GFxValue& a_entry, RE::GFxValue& a_icon, RE::GFxValue& a_holder)
		{
			if (a_holder.IsDisplayObject()) {
				a_holder.SetMember("_visible", RE::GFxValue(false));
			}
			RE::GFxValue hid;
			if (a_entry.GetMember("pngii_catHid", &hid) && hid.IsBool() && hid.GetBool()) {
				a_icon.SetMember("_visible", RE::GFxValue(true));
				a_entry.SetMember("pngii_catHid", RE::GFxValue(false));
			}
		}

		void Apply(RE::GFxMovie& a_movie, RE::GFxValue& a_entry, const RE::GFxValue* a_entryObject, bool a_alchemy, bool a_log)
		{
			RE::GFxValue icon;
			RE::GFxValue holder;
			RE::GFxValue label;
			if (!a_entry.GetMember("icon", &icon) || !icon.IsDisplayObject()) {
				return;
			}
			a_entry.GetMember("pngii_catHolder", &holder);

			bool   hasLabel = false;
			double size = 0.0;
			if (a_alchemy) {
				if (!a_entryObject || !a_entryObject->IsObject()) {
					return;
				}
				hasLabel = a_entryObject->GetMember("iconLabel", &label) && label.IsString() && *label.GetString() != '\0';
				if (!hasLabel) {
					return;
				}
				size = Num(icon, "_height", 0.0);
				if (size <= 0.0) {
					size = kFallbackAlchemySize;
				}
			} else {
				hasLabel = a_entry.GetMember("_iconLabel", &label) && label.IsString();
				size = Num(a_entry, "_iconSize", kFallbackIconSize);
			}
			const std::string path = hasLabel ? ResolvePath(label.GetString()) : std::string{};
			const std::string* url = path.empty() ? nullptr : TextureManager::GetURL(path, TextureManager::StageToScreenPx(size));
			if (a_log) {
				logger::debug("category {}: {}", hasLabel ? label.GetString() : "<none>", url ? path.c_str() : "vanilla");
			}
			if (!url) {
				return;
			}
			if (!holder.IsDisplayObject() && !a_entry.CreateEmptyMovieClip(&holder, "pngii_catHolder", kHolderDepth)) {
				return;
			}

			const double scale = size / kLoadedImageSize * 100.0;
			holder.SetMember("_x", RE::GFxValue(Num(icon, "_x", 0.0)));
			holder.SetMember("_y", RE::GFxValue(Num(icon, "_y", 0.0)));
			holder.SetMember("_xscale", RE::GFxValue(scale));
			holder.SetMember("_yscale", RE::GFxValue(scale));
			if (a_alchemy) {
				RE::GFxValue enabled;
				const bool on = a_entryObject->GetMember("enabled", &enabled) && enabled.IsBool() && enabled.GetBool();
				holder.SetMember("_alpha", RE::GFxValue(on ? 100.0 : kDisabledAlpha));
			}

			RE::GFxValue current;
			if (!a_entry.GetMember("pngii_catUrl", &current) || !current.IsString() || *url != current.GetString()) {
				RE::GFxValue loader;
				a_movie.CreateObject(&loader, "MovieClipLoader");
				a_entry.SetMember("pngii_catLoader", loader);
				a_entry.SetMember("pngii_catUrl", RE::GFxValue(url->c_str()));
				std::array<RE::GFxValue, 2> args{ RE::GFxValue(url->c_str()), holder };
				loader.Invoke("loadClip", args);
			}
			holder.SetMember("_visible", RE::GFxValue(true));
			icon.SetMember("_visible", RE::GFxValue(false));
			a_entry.SetMember("pngii_catHid", RE::GFxValue(true));
		}

		class SetEntryWrapper final : public RE::GFxFunctionHandler
		{
		public:
			explicit SetEntryWrapper(bool a_alchemy) :
				_alchemy(a_alchemy)
			{}

			void Call(Params& a_params) override
			{
				if (!a_params.thisPtr) {
					return;
				}
				// Undo our hide before the original: COCKS hides `icon` on empty-label sub rows, and re-showing it afterwards restored a stale icon.
				RE::GFxValue icon;
				RE::GFxValue holder;
				if (a_params.thisPtr->GetMember("icon", &icon) && icon.IsDisplayObject()) {
					a_params.thisPtr->GetMember("pngii_catHolder", &holder);
					Restore(*a_params.thisPtr, icon, holder);
				}
				a_params.thisPtr->Invoke(kOriginalSetEntry, a_params.retVal, a_params.args, a_params.argCount);

				static std::uint32_t logged = 0;
				const bool log = logged < kLoggedCalls && ++logged;
				Apply(*a_params.movie, *a_params.thisPtr, a_params.argCount > 0 ? &a_params.args[0] : nullptr, _alchemy, log);
			}

		private:
			bool _alchemy;
		};

		// The category SWF's loadClip finishing re-shows `icon` after our hide (same trap as RowIcons).
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
				if (a_params.argCount < 1 || !a_params.thisPtr->GetMember("pngii_catHid", &hid) || !hid.IsBool() || !hid.GetBool()) {
					return;
				}
				a_params.args[0].SetMember("_visible", RE::GFxValue(false));

				static std::uint32_t logged = 0;
				if (logged < kLoggedCalls) {
					++logged;
					logger::debug("category onLoadInit re-hid icon");
				}
			}
		};

		bool IsCategoryMenu(std::string_view a_name)
		{
			return std::ranges::any_of(kTargets, [&](const Target& a_target) { return a_name == a_target.menu; });
		}

		void Wrap(RE::GFxMovieView& a_view, RE::GFxValue& a_proto, const Target& a_target, const char* a_method,
			const char* a_originalMember, RE::GFxFunctionHandler* a_handler)
		{
			RE::GFxValue original;
			if (!a_proto.GetMember(a_method, &original) || !original.IsObject()) {
				logger::warn("{}: {}.prototype.{} missing", a_target.menu, a_target.className, a_method);
				return;
			}
			RE::GFxValue wrapper;
			a_view.CreateFunction(&wrapper, a_handler);
			a_proto.SetMember(a_originalMember, original);
			a_proto.SetMember(a_method, wrapper);
			logger::info("{}: wrapped {}.prototype.{}", a_target.menu, a_target.className, a_method);
		}

		bool InstallInto(RE::GFxMovieView& a_view, const Target& a_target)
		{
			RE::GFxValue proto;
			if (!a_view.GetVariable(&proto, a_target.proto) || (!proto.IsObject() && !proto.IsDisplayObject())) {
				return false;
			}

			RE::GFxValue installed;
			if (proto.GetMember(kInstalledMember, &installed) && installed.IsBool() && installed.GetBool()) {
				return true;
			}

			static auto setEntryHandler = RE::make_gptr<SetEntryWrapper>(false);
			static auto alchemySetEntryHandler = RE::make_gptr<SetEntryWrapper>(true);
			static auto loadInitHandler = RE::make_gptr<OnLoadInitWrapper>();
			Wrap(a_view, proto, a_target, "setEntry", kOriginalSetEntry,
				a_target.alchemyColumn ? alchemySetEntryHandler.get() : setEntryHandler.get());
			RE::GFxValue loadInit;
			if (a_target.hasOnLoadInit || (a_target.alchemyColumn && proto.GetMember("onLoadInit", &loadInit) && loadInit.IsObject())) {
				Wrap(a_view, proto, a_target, "onLoadInit", kOriginalOnLoadInit, loadInitHandler.get());
			}
			proto.SetMember(kInstalledMember, RE::GFxValue(true));

			RE::GFxValue list;
			if (a_view.GetVariable(&list, a_target.list) && list.IsDisplayObject()) {
				list.Invoke("UpdateList");
			}
			return true;
		}

		void ScheduleInstall(std::string menu, std::uint32_t a_attemptsRemaining)
		{
			SKSE::GetTaskInterface()->AddUITask([menu = std::move(menu), a_attemptsRemaining]() {
				auto* ui = RE::UI::GetSingleton();
				auto  view = ui ? ui->GetMovieView(menu) : nullptr;
				bool done = view != nullptr;
				if (view) {
					for (const auto& target : kTargets) {
						if (menu == target.menu && !InstallInto(*view, target)) {
							done = false;
						}
					}
				}
				if (done) {
					return;
				}
				if (a_attemptsRemaining == 0) {
					logger::warn("{}: category entry prototype never became available", menu);
					return;
				}
				ScheduleInstall(menu, a_attemptsRemaining - 1);
			});
		}

		class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
				RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (a_event && a_event->opening && IsCategoryMenu(a_event->menuName.c_str())) {
					ScheduleInstall(a_event->menuName.c_str(), 120);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Install()
	{
		static MenuSink sink;
		RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&sink);
		logger::info("category icons: menu sink registered");
	}
}
