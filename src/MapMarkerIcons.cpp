#include "MapMarkerIcons.h"
#include "IconRules.h"
#include "StatusIcons.h"
#include "TextureManager.h"

#include <atomic>

namespace MapMarkerIcons
{
	namespace
	{
		constexpr auto kMenuProto = "_global.Map.MapMenu.prototype";
		constexpr auto kMarkerProto = "_global.Map.MapMarker.prototype";
		constexpr auto kOrigCreate = "pngii_mmOrigCreate";
		constexpr auto kOrigConfig = "pngii_mmOrigConfig";
		constexpr auto kInstalledMember = "pngii_mmInstalled";
		constexpr auto kPathMember = "pngii_mmPath";
		constexpr auto kDoneMember = "pngii_mmDone";
		// configUI draws without knowing which Map.MapMenu instance owns the clip.
		constexpr auto kLocalMember = "pngii_mmLocal";
		constexpr std::int32_t kHolderDepth = 5000;
		// BSScaleformImageLoader::LoadImage reports every registered image as 64x64.
		constexpr double kLoadedImageSize = 64.0;
		constexpr double kHoverScale = 1.5;
		constexpr double kInventoryStageHeight = 720.0;
		constexpr std::uint32_t kLoggedCalls = 8;

		std::array<std::atomic<bool>, 2> g_mapEnabled{ true, true };
		std::array<std::atomic<std::uint32_t>, 2> g_tints{};
		double g_stageHeight = kInventoryStageHeight;
		bool g_warnedMismatch = false;
		bool g_coMAPMissingLogged = false;
		std::array<bool, 2> g_loggedCounts{};

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

		bool IsClip(const RE::GFxValue& a_value)
		{
			return a_value.IsObject() || a_value.IsDisplayObject();
		}

		bool IsLocal(const RE::GFxValue& a_instance)
		{
			RE::GFxValue v;
			return !a_instance.GetMember("LocalMapMenu", &v) || v.IsUndefined() || v.IsNull();
		}

		const RE::BSTArray<RE::MapMenuMarker>* Markers(RE::MapMenu& a_menu, bool a_local)
		{
			if (a_local) {
				auto* data = a_menu.GetRuntimeData();
				return data ? &data->localMapMenu.mapMarkers : nullptr;
			}
			auto* data2 = a_menu.GetRuntimeData2();
			return data2 ? &data2->mapMarkers : nullptr;
		}

		void LogCounts(RE::MapMenu& a_menu, bool a_local, std::uint32_t a_flashCount)
		{
			auto& logged = g_loggedCounts[a_local ? 1 : 0];
			if (logged) {
				return;
			}
			logged = true;
			if (const auto* markers = Markers(a_menu, a_local)) {
				logger::debug("MapMenu: {} markers size {} capacity {} _markerList.length {}", a_local ? "local" : "world",
					markers->size(), markers->capacity(), a_flashCount);
			}
		}

		void Resolve(RE::MapMenu& a_menu, bool a_local, std::uint32_t a_idx, std::uint32_t a_flashCount, RE::GFxValue& a_clip)
		{
			// One MapMenu movie holds both marker lists, so this is checked per map.
			if (!g_mapEnabled[a_local ? 1 : 0]) {
				return;
			}
			const auto* markers = Markers(a_menu, a_local);
			// Engine count and clip count can differ; use the smaller.
			if (!markers || a_idx >= (std::min)(markers->size(), a_flashCount)) {
				return;
			}
			const auto& entry = (*markers)[a_idx];
			if (entry.type != 0) {
				return;
			}
			RE::NiPointer<RE::TESObjectREFR> ref;
			if (!RE::LookupReferenceByHandle(entry.ref, ref) || !ref) {
				return;
			}

			IconRules::MapMarkerQuery query{ ref->GetFormID(), nullptr, nullptr, 0, a_local, false, ref.get() };
			RE::NiPointer<RE::TESObjectREFR> linked;
			if (const auto* marker = ref->extraList.GetByType<RE::ExtraMapMarker>(); marker && marker->mapData) {
				query.markerType = marker->mapData->type.underlying();
				const auto clipType = Num(a_clip, "markerType", -1.0);
				if (clipType != static_cast<double>(query.markerType)) {
					if (!g_warnedMismatch) {
						g_warnedMismatch = true;
						logger::warn("MapMarkerIcons: marker index/type mismatch at {}: clip {} engine {}", a_idx, clipType, query.markerType);
					}
					return;
				}
				query.location = ref->GetEditorLocation();
			} else if (a_local && entry.door) {
				const auto* teleport = ref->extraList.GetByType<RE::ExtraTeleport>();
				if (!teleport) {
					return;
				}
				// CoMAP does not guard a null linked door; we do.
				if (teleport->teleportData) {
					linked = teleport->teleportData->linkedDoor.get();
				}
				query.isDoor = true;
				query.markerType = static_cast<std::uint16_t>(RE::MARKER_TYPE::kDoor);
				query.location = linked ? linked->GetEditorLocation() : nullptr;
			} else {
				return;
			}
			query.plugin = ref->GetFile(0);

			const bool undiscovered = Bool(a_clip, "isUndiscovered", false);
			if (const auto* path = IconRules::FindMapMarkerIcon(query, undiscovered)) {
				a_clip.SetMember(kPathMember, RE::GFxValue(path->c_str()));
				a_clip.SetMember(kLocalMember, RE::GFxValue(a_local));
			}
		}

		void Draw(RE::GFxMovie& a_movie, RE::GFxValue& a_clip)
		{
			RE::GFxValue pathValue;
			if (Bool(a_clip, kDoneMember, false) || !a_clip.GetMember(kPathMember, &pathValue) || !pathValue.IsString()) {
				return;
			}
			a_clip.SetMember(kDoneMember, RE::GFxValue(true));
			const std::string path = pathValue.GetString();

			RE::GFxValue iconClip;
			RE::GFxValue iconHolder;
			RE::GFxValue icon;
			if (!a_clip.GetMember("IconClip", &iconClip) || !iconClip.IsDisplayObject() ||
				!iconClip.GetMember("iconHolder", &iconHolder) || !iconHolder.IsDisplayObject() ||
				!iconHolder.GetMember("icon", &icon) || !icon.IsDisplayObject()) {
				static bool logged = false;
				if (!logged) {
					logged = true;
					logger::debug("MapMenu: marker clip has no IconClip.iconHolder.icon; markers keep vanilla");
				}
				return;
			}

			RE::GFxValue local;
			std::array<RE::GFxValue, 1> localArgs{ iconHolder };
			if (!icon.Invoke("getBounds", &local, localArgs) || !local.IsObject()) {
				return;
			}
			const double xMin = Num(local, "xMin", 0.0);
			const double yMin = Num(local, "yMin", 0.0);
			const double width = Num(local, "xMax", 0.0) - xMin;
			const double height = Num(local, "yMax", 0.0) - yMin;
			const double side = (std::max)(width, height);
			if (!(side > 0.0)) {
				return;
			}

			RE::GFxValue root;
			RE::GFxValue global;
			double stageSide = side;
			if (a_movie.GetVariable(&root, "_root") && IsClip(root)) {
				std::array<RE::GFxValue, 1> rootArgs{ root };
				if (icon.Invoke("getBounds", &global, rootArgs) && global.IsObject()) {
					stageSide = (std::max)(Num(global, "xMax", 0.0) - Num(global, "xMin", 0.0),
						Num(global, "yMax", 0.0) - Num(global, "yMin", 0.0));
				}
			}
			const auto px = TextureManager::StageToScreenPx(stageSide * kHoverScale * kInventoryStageHeight / g_stageHeight);
			const auto* url = TextureManager::GetURL(path, px);
			static std::uint32_t loggedDraws = 0;
			if (loggedDraws < kLoggedCalls) {
				++loggedDraws;
				logger::debug("map marker {} bounds {:.1f} x {:.1f}, stage side {:.1f} -> {} px {}", path, width, height, stageSide, px,
					url ? "drawn" : "vanilla (no texture)");
			}
			if (!url) {
				return;
			}

			RE::GFxValue holder;
			if (!iconHolder.CreateEmptyMovieClip(&holder, "pngii_mmHolder", kHolderDepth)) {
				return;
			}
			const double scale = side / kLoadedImageSize * 100.0;
			holder.SetMember("_x", RE::GFxValue(xMin + (width - side) / 2.0));
			holder.SetMember("_y", RE::GFxValue(yMin + (height - side) / 2.0));
			holder.SetMember("_xscale", RE::GFxValue(scale));
			holder.SetMember("_yscale", RE::GFxValue(scale));
			// Tint the outer holder and load into a child, so loadClip never replaces the tinted clip.
			RE::GFxValue image;
			if (!holder.CreateEmptyMovieClip(&image, "pngii_mmImage", 0)) {
				return;
			}
			if (const auto tint = g_tints[Bool(a_clip, kLocalMember, false) ? 1 : 0].load()) {
				StatusIcons::SetTint(a_movie, holder, tint);
			}

			RE::GFxValue loader;
			a_movie.CreateObject(&loader, "MovieClipLoader");
			a_clip.SetMember("pngii_mmLoader", loader);
			a_clip.SetMember("pngii_mmHolder", holder);
			std::array<RE::GFxValue, 2> args{ RE::GFxValue(url->c_str()), image };
			loader.Invoke("loadClip", args);
			icon.SetMember("_visible", RE::GFxValue(false));
		}

		void ResolveAndDraw(RE::GFxMovie& a_movie, RE::MapMenu& a_menu, bool a_local, std::uint32_t a_idx,
			std::uint32_t a_flashCount, RE::GFxValue& a_clip)
		{
			Resolve(a_menu, a_local, a_idx, a_flashCount, a_clip);
			if (Bool(a_clip, "initialized", false)) {
				Draw(a_movie, a_clip);
			}
		}

		void Walk(RE::GFxMovie& a_movie, RE::MapMenu& a_menu, const RE::GFxValue& a_instance, bool a_local)
		{
			RE::GFxValue list;
			if (!a_instance.GetMember("_markerList", &list) || !list.IsArray()) {
				return;
			}
			const auto length = list.GetArraySize();
			const auto next = Num(a_instance, "_nextCreateIndex", -1.0);
			const auto created = next < 0.0 ? length : (std::min)(static_cast<std::uint32_t>(next), length);
			if (created == 0) {
				return;
			}
			LogCounts(a_menu, a_local, length);
			for (std::uint32_t i = 0; i < created; ++i) {
				RE::GFxValue clip;
				RE::GFxValue path;
				if (!list.GetElement(i, &clip) || !clip.IsDisplayObject() || Bool(clip, kDoneMember, false) ||
					(clip.GetMember(kPathMember, &path) && path.IsString())) {
					continue;
				}
				ResolveAndDraw(a_movie, a_menu, a_local, i, length, clip);
			}
		}

		class CreateMarkersWrapper final : public RE::GFxFunctionHandler
		{
		public:
			void Call(Params& a_params) override
			{
				if (!a_params.thisPtr) {
					return;
				}
				auto& self = *a_params.thisPtr;
				const auto before = Num(self, "_nextCreateIndex", -1.0);
				self.Invoke(kOrigCreate, a_params.retVal, a_params.args, a_params.argCount);
				if (before < 0.0) {
					return;
				}
				RE::GFxValue list;
				if (!self.GetMember("_markerList", &list) || !list.IsArray()) {
					return;
				}
				const auto length = list.GetArraySize();
				const auto after = Num(self, "_nextCreateIndex", -1.0);
				const auto end = after < 0.0 ? length : (std::min)(static_cast<std::uint32_t>(after), length);
				auto menu = RE::UI::GetSingleton()->GetMenu<RE::MapMenu>();
				if (!menu) {
					return;
				}
				const bool local = IsLocal(self);
				LogCounts(*menu, local, length);
				for (auto i = static_cast<std::uint32_t>(before); i < end; ++i) {
					RE::GFxValue clip;
					if (list.GetElement(i, &clip) && clip.IsDisplayObject()) {
						ResolveAndDraw(*a_params.movie, *menu, local, i, length, clip);
					}
				}
			}
		};

		class ConfigUIWrapper final : public RE::GFxFunctionHandler
		{
		public:
			void Call(Params& a_params) override
			{
				if (!a_params.thisPtr) {
					return;
				}
				a_params.thisPtr->Invoke(kOrigConfig, a_params.retVal, a_params.args, a_params.argCount);
				Draw(*a_params.movie, *a_params.thisPtr);
			}
		};

		bool Wrap(RE::GFxMovieView& a_view, RE::GFxValue& a_proto, const char* a_class, const char* a_method,
			const char* a_originalMember, RE::GFxFunctionHandler* a_handler)
		{
			RE::GFxValue original;
			if (!a_proto.GetMember(a_method, &original) || !original.IsObject()) {
				logger::warn("MapMenu: {}.prototype.{} missing", a_class, a_method);
				return false;
			}
			RE::GFxValue wrapper;
			a_view.CreateFunction(&wrapper, a_handler);
			a_proto.SetMember(a_originalMember, original);
			a_proto.SetMember(a_method, wrapper);
			logger::info("MapMenu: wrapped {}.prototype.{}", a_class, a_method);
			return true;
		}

		bool InstallInto(RE::GFxMovieView& a_view)
		{
			RE::GFxValue menuProto;
			RE::GFxValue markerProto;
			if (!a_view.GetVariable(&menuProto, kMenuProto) || !IsClip(menuProto) ||
				!a_view.GetVariable(&markerProto, kMarkerProto) || !IsClip(markerProto)) {
				return false;
			}

			if (const auto* def = a_view.GetMovieDef(); def && def->GetHeight() > 0.0f) {
				g_stageHeight = def->GetHeight();
				static bool stageLogged = false;
				if (!stageLogged) {
					stageLogged = true;
					logger::debug("MapMenu: stage {} x {}", def->GetWidth(), def->GetHeight());
				}
			}

			if (!Bool(menuProto, kInstalledMember, false)) {
				static auto createHandler = RE::make_gptr<CreateMarkersWrapper>();
				static auto configHandler = RE::make_gptr<ConfigUIWrapper>();
				Wrap(a_view, menuProto, "Map.MapMenu", "CreateMarkers", kOrigCreate, createHandler.get());
				Wrap(a_view, markerProto, "Map.MapMarker", "configUI", kOrigConfig, configHandler.get());
				menuProto.SetMember(kInstalledMember, RE::GFxValue(true));
			}
			g_loggedCounts = {};

			if (!g_coMAPMissingLogged && !IconRules::HasCoMAP()) {
				g_coMAPMissingLogged = true;
				logger::info("MapMenu: CoMAP interface not received; comapIcon rules never match");
			}

			auto menu = RE::UI::GetSingleton()->GetMenu<RE::MapMenu>();
			auto* data2 = menu ? menu->GetRuntimeData2() : nullptr;
			if (!data2 || !IsClip(data2->mapMovie)) {
				return true;
			}
			const auto& world = data2->mapMovie;
			Walk(a_view, *menu, world, false);
			RE::GFxValue localMap;
			RE::GFxValue iconDisplay;
			if (world.GetMember("LocalMapMenu", &localMap) && IsClip(localMap) &&
				localMap.GetMember("IconDisplay", &iconDisplay) && IsClip(iconDisplay)) {
				Walk(a_view, *menu, iconDisplay, true);
			}
			return true;
		}

		void ScheduleInstall(std::uint32_t a_attemptsRemaining)
		{
			SKSE::GetTaskInterface()->AddUITask([a_attemptsRemaining]() {
				auto* ui = RE::UI::GetSingleton();
				auto  view = ui ? ui->GetMovieView(RE::MapMenu::MENU_NAME) : nullptr;
				if (view && InstallInto(*view)) {
					return;
				}
				if (a_attemptsRemaining == 0) {
					logger::warn("MapMenu: Map.MapMenu / Map.MapMarker prototype never became available; markers keep vanilla");
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
				if ((g_mapEnabled[0] || g_mapEnabled[1]) && a_event && a_event->opening && a_event->menuName == RE::MapMenu::MENU_NAME) {
					ScheduleInstall(120);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void SetMapsEnabled(bool a_world, bool a_local)
	{
		g_mapEnabled[0] = a_world;
		g_mapEnabled[1] = a_local;
	}

	void SetTints(std::uint32_t a_world, std::uint32_t a_local)
	{
		g_tints[0] = a_world;
		g_tints[1] = a_local;
	}

	void Install()
	{
		if (REL::Module::IsVR()) {
			logger::info("Map marker icons: off (VR)");
			return;
		}
		if (!IconRules::HasMapMarkerRules()) {
			logger::info("Map marker icons: off (no mapMarkers rules loaded)");
			return;
		}
		static MenuSink menuSink;
		RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&menuSink);
		logger::info("Map marker icons: sink registered (world {}, local {})", g_mapEnabled[0].load() ? "on" : "off", g_mapEnabled[1].load() ? "on" : "off");
	}
}
