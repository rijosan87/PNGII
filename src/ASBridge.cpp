#include "ASBridge.h"
#include "IconAPI.h"
#include "IconLabels.h"
#include "TextureManager.h"

#include <cmath>

namespace ASBridge
{
	namespace
	{
		constexpr auto kPluginName = "PNGIconInjector";
		// LoadImage reports every img:// image as 64x64, so an unscaled clip draws
		// 64 stage units; that is the size a caller gets when it passes none.
		constexpr double        kDefaultSize = 64.0;
		constexpr double        kMaxSize = 1024.0;
		// StageToScreenPx assumes InventoryMenu's 720-unit stage; other movies differ (LootMenu 1080).
		constexpr double        kInventoryStageHeight = 720.0;
		constexpr std::uint32_t kLoggedCalls = 8;

		// AS2 bit ops return signed 32-bit numbers, so FE/FF IDs built with `|` or `&` arrive negative.
		RE::FormID FormArg(const RE::GFxValue& a_value)
		{
			if (!a_value.IsNumber()) {
				return 0;
			}
			const double v = a_value.GetNumber();
			if (!std::isfinite(v) || v < -2147483648.0 || v >= 4294967296.0) {
				return 0;
			}
			return v < 0.0 ? static_cast<RE::FormID>(static_cast<std::int32_t>(v)) : static_cast<RE::FormID>(v);
		}

		double SizeArg(const RE::GFxFunctionHandler::Params& a_params)
		{
			if (a_params.argCount < 3 || !a_params.args[2].IsNumber()) {
				return kDefaultSize;
			}
			const double v = a_params.args[2].GetNumber();
			return std::isfinite(v) && v > 0.0 ? (std::min)(v, kMaxSize) : kDefaultSize;
		}

		std::int32_t SoulArg(const RE::GFxFunctionHandler::Params& a_params)
		{
			if (a_params.argCount < 4 || !a_params.args[3].IsNumber()) {
				return -1;
			}
			const double v = a_params.args[3].GetNumber();
			return v == 0.0 || v == 1.0 || v == 2.0 ? static_cast<std::int32_t>(v) : -1;
		}

		// Child SWFs share the host's movie def, which is why size is in top-level stage units.
		double StageHeight(const RE::GFxMovie* a_movie)
		{
			const auto* def = a_movie ? a_movie->GetMovieDef() : nullptr;
			const float h = def ? def->GetHeight() : 0.0f;
			return h > 0.0f ? h : kInventoryStageHeight;
		}

		class GetIconHandler final : public RE::GFxFunctionHandler
		{
		public:
			void Call(Params& a_params) override
			{
				if (!a_params.retVal) {
					return;
				}
				// undefined, not "": one check covers PNGII missing (the chained call is undefined) and no icon.
				a_params.retVal->SetUndefined();

				const RE::FormID form = a_params.argCount >= 1 ? FormArg(a_params.args[0]) : 0;
				std::string_view label = a_params.argCount >= 2 && a_params.args[1].IsString() ?
				                                   std::string_view(a_params.args[1].GetString()) :
				                                   std::string_view{};
				if (!form && label.empty()) {
					return;
				}
				// Same gate as the V1 C API: null until keywords are prepared after a save or new game loads.
				const auto resolver = IconAPI::GetResolver();
				if (!resolver) {
					static bool loggedNotReady = false;
					if (!loggedNotReady) {
						loggedNotReady = true;
						logger::debug("AS GetIcon before a save loaded: form {:08X} -> undefined", form);
					}
					return;
				}
				try {
					const double       size = SizeArg(a_params);
					const std::int32_t soul = SoulArg(a_params);
					// No label means SkyUI's own label for the form, after the gate so KID keywords exist.
					const bool derived = label.empty() && form;
					if (derived) {
						if (const char* d = IconLabels::ForFormID(form, soul)) {
							label = d;
						}
					}
					const auto* path = resolver(form, label, soul);
					const auto         px = TextureManager::StageToScreenPx(size * kInventoryStageHeight / StageHeight(a_params.movie));
					const std::string* url = path && !path->empty() ? TextureManager::GetURL(*path, px) : nullptr;

					static std::uint32_t logged = 0;
					if (logged < kLoggedCalls) {
						++logged;
						logger::debug("AS GetIcon form {:08X} label '{}'{} size {} soul {} -> {} px {}", form, label, derived ? " (derived)" : "", size,
							soul, px, url ? url->c_str() : (path ? "texture failed" : "no match"));
					}
					// CreateString copies into the AS string table; an unmanaged SetString keeps the raw pointer.
					if (url && a_params.movie) {
						a_params.movie->CreateString(a_params.retVal, url->c_str());
					}
				} catch (...) {
					a_params.retVal->SetUndefined();
				}
			}
		};

		class GetAPIVersionHandler final : public RE::GFxFunctionHandler
		{
		public:
			void Call(Params& a_params) override
			{
				if (a_params.retVal) {
					a_params.retVal->SetNumber(kAPIVersion);
				}
			}
		};

		// SKSE runs this for every new movie, possibly from a task thread; touch no PNGII state here.
		bool RegisterFuncs(RE::GFxMovieView* a_view, RE::GFxValue* a_root)
		{
			if (!a_view || !a_root) {
				return false;
			}
			static auto getIcon = RE::make_gptr<GetIconHandler>();
			static auto getVersion = RE::make_gptr<GetAPIVersionHandler>();
			RE::GFxValue fn;
			a_view->CreateFunction(&fn, getIcon.get());
			a_root->SetMember("GetIcon", fn);
			a_view->CreateFunction(&fn, getVersion.get());
			a_root->SetMember("GetAPIVersion", fn);
			return true;
		}
	}

	void Register()
	{
		const auto* scaleform = SKSE::GetScaleformInterface();
		if (!scaleform || !scaleform->Register(RegisterFuncs, kPluginName)) {
			logger::error("Scaleform: could not register skse.plugins.{}; AS GetIcon unavailable", kPluginName);
			return;
		}
		logger::info("Scaleform: registered skse.plugins.{} (GetIcon, API version {})", kPluginName, kAPIVersion);
	}
}
