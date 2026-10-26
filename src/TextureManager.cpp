#include "TextureManager.h"

#include <DirectXTex.h>
#include <d3d11.h>

namespace TextureManager
{
	namespace
	{
		// 4x4 white DDS shipped in Data/, loaded once through the engine to get a real NiSourceTexture to clone.
		constexpr auto kTemplatePath = "Textures\\Interface\\PNGIconInjector\\template.dds";
		constexpr auto kNamePrefix = "PNGIconInjector:";

		struct Entry
		{
			std::string url;
			std::unique_ptr<RE::BSScaleformExternalTexture> external;
		};

		std::unordered_map<std::string, Entry> g_entries;
		std::unordered_set<std::string>        g_misses;
		std::unordered_map<std::string, bool>  g_exists;
		std::unordered_map<std::string, std::pair<std::uint32_t, std::uint32_t>> g_sourceSizes;
		Stats                                   g_stats{};

		std::size_t PickLevel(std::size_t a_width, std::size_t a_height, std::uint32_t a_targetPx)
		{
			std::size_t level = 0;
			if (a_targetPx == 0) {
				return level;
			}
			while (true) {
				const auto w = (std::max<std::size_t>)(1, a_width >> level);
				const auto h = (std::max<std::size_t>)(1, a_height >> level);
				if (w == 1 && h == 1) {
					break;
				}
				const auto nw = (std::max<std::size_t>)(1, a_width >> (level + 1));
				const auto nh = (std::max<std::size_t>)(1, a_height >> (level + 1));
				if ((std::max)(nw, nh) < a_targetPx) {
					break;
				}
				++level;
			}
			return level;
		}

		std::string LevelKey(const std::string& a_path, std::size_t a_levelWidth)
		{
			return std::format("level|{}@{}", a_path, a_levelWidth);
		}

		bool ReadGameFile(const std::string& a_path, std::vector<std::uint8_t>& a_out)
		{
			std::string path = a_path;
			std::ranges::replace(path, '/', '\\');
			RE::BSResourceNiBinaryStream file(path);
			if (!file.good() || !file.stream) {
				return false;
			}
			const std::uint32_t size = file.stream->totalSize;
			if (size == 0) {
				return false;
			}
			a_out.resize(size);
			return file.read(a_out.data(), size);
		}

		std::size_t ImageBytes(const DirectX::TexMetadata& a_md)
		{
			const std::size_t bpp = DirectX::BitsPerPixel(a_md.format);
			std::size_t       total = 0;
			for (std::size_t mip = 0; mip < a_md.mipLevels; ++mip) {
				const std::size_t w = (std::max<std::size_t>)(1, a_md.width >> mip);
				const std::size_t h = (std::max<std::size_t>)(1, a_md.height >> mip);
				total += w * h * bpp / 8;
			}
			return total;
		}

		RE::NiSourceTexture* GetTemplate()
		{
			static RE::NiPointer<RE::NiTexture> holder;
			static bool                         tried = false;
			if (!tried) {
				tried = true;
				RE::BSShaderManager::GetTexture(kTemplatePath, true, holder, false);
			}
			auto* source = holder ? netimmerse_cast<RE::NiSourceTexture*>(holder.get()) : nullptr;
			if (!source || !source->rendererTexture) {
				return nullptr;
			}
			return source;
		}

		bool DecodeImage(const std::vector<std::uint8_t>& a_bytes, DirectX::ScratchImage& a_out)
		{
			HRESULT hr = DirectX::LoadFromWICMemory(a_bytes.data(), a_bytes.size(), DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, a_out);
			if (hr == CO_E_NOTINITIALIZED) {
				CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
				hr = DirectX::LoadFromWICMemory(a_bytes.data(), a_bytes.size(), DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, a_out);
			}
			return SUCCEEDED(hr);
		}

		const std::string* TryScaled(const std::string& a_path, std::uint32_t a_targetPx)
		{
			std::vector<std::uint8_t> bytes;
			if (!ReadGameFile(a_path, bytes)) {
				return nullptr;
			}

			auto* templ = GetTemplate();
			auto* device = reinterpret_cast<ID3D11Device*>(RE::BSGraphics::Renderer::GetDevice());
			if (!templ || !device) {
				logger::warn("pre-scaled back end unavailable (template={}, device={})",
					static_cast<void*>(templ), static_cast<void*>(device));
				return nullptr;
			}

			DirectX::ScratchImage image;
			if (!DecodeImage(bytes, image)) {
				logger::warn("DirectXTex could not decode {}", a_path);
				return nullptr;
			}

			DirectX::ScratchImage converted;
			const DirectX::ScratchImage* source = &image;
			if (image.GetMetadata().format != DXGI_FORMAT_R8G8B8A8_UNORM) {
				if (FAILED(DirectX::Convert(*image.GetImage(0, 0, 0), DXGI_FORMAT_R8G8B8A8_UNORM,
						DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, converted))) {
					return nullptr;
				}
				source = &converted;
			}

			// Scaleform samples only mip 0, so build the mip chain on the CPU (2x linear steps) and upload the smallest level >= the drawn size. Downscale in premultiplied alpha so black RGB from transparent pixels does not fringe the edges.
			const auto& srcMd = source->GetMetadata();
			DirectX::ScratchImage premul;
			if (FAILED(DirectX::PremultiplyAlpha(*source->GetImage(0, 0, 0), DirectX::TEX_PMALPHA_IGNORE_SRGB, premul))) {
				return nullptr;
			}
			DirectX::ScratchImage mips;
			if (FAILED(DirectX::GenerateMipMaps(*premul.GetImage(0, 0, 0), DirectX::TEX_FILTER_LINEAR, 0, mips))) {
				return nullptr;
			}
			const auto level = PickLevel(srcMd.width, srcMd.height, a_targetPx);
			// Uploaded premultiplied because Scaleform blends it that way; a straight upload drew a bright rim on grey icons.
			const auto* chosen = mips.GetImage(level, 0, 0);
			auto        md = mips.GetMetadata();
			md.width = chosen->width;
			md.height = chosen->height;
			md.mipLevels = 1;

			ID3D11Resource* resource = nullptr;
			if (FAILED(DirectX::CreateTexture(device, chosen, 1, md, &resource))) {
				return nullptr;
			}
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
			srvDesc.Format = md.format;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			ID3D11ShaderResourceView* srv = nullptr;
			if (FAILED(device->CreateShaderResourceView(resource, &srvDesc, &srv))) {
				resource->Release();
				return nullptr;
			}

			auto* renderer = new RE::NiTexture::RendererData(static_cast<std::uint16_t>(md.width),
				static_cast<std::uint16_t>(md.height));
			renderer->texture = reinterpret_cast<REX::W32::ID3D11Texture2D*>(resource);
			renderer->resourceView = reinterpret_cast<REX::W32::ID3D11ShaderResourceView*>(srv);
			renderer->unk1C = static_cast<std::uint8_t>(md.mipLevels);
			renderer->unk1D = static_cast<std::uint8_t>(md.format);

			auto* clone = static_cast<RE::NiSourceTexture*>(RE::malloc(sizeof(RE::NiSourceTexture)));
			if (!clone) {
				return nullptr;
			}
			std::memcpy(static_cast<void*>(clone), static_cast<const void*>(templ), sizeof(RE::NiSourceTexture));
			std::construct_at(&clone->name,
				RE::BSFixedString(std::format("{}{}@{}", kNamePrefix, a_path, md.width).c_str()));
			clone->prev = nullptr;
			clone->next = nullptr;
			clone->resourceStream = nullptr;
			clone->rendererTexture = reinterpret_cast<RE::BSGraphics::Texture*>(renderer);

			// SetTexture registers with the image loader under the NiTexture name. Its bool is not a reliable success test.
			auto external = std::make_unique<RE::BSScaleformExternalTexture>();
			external->SetTexture(clone);

			// BSFixedString pools case-insensitively, so build the URL from the stored name.
			std::string url = "img://"s + clone->name.c_str();
			g_stats.scaled++;
			g_sourceSizes[a_path] = { static_cast<std::uint32_t>(srcMd.width), static_cast<std::uint32_t>(srcMd.height) };
			g_stats.vramBytes += ImageBytes(md);
			logger::debug("scaled {} ({}x{} source, target {} px) -> {}x{} -> {}", a_path,
				srcMd.width, srcMd.height, a_targetPx, md.width, md.height, url);

			const auto [it, _] = g_entries.emplace(LevelKey(a_path, md.width), Entry{ std::move(url), std::move(external) });
			return &it->second.url;
		}

		const std::string* TryLoadPNG(const std::string& a_path)
		{
			const std::string key = "loadpng|" + a_path;
			if (const auto it = g_entries.find(key); it != g_entries.end()) {
				return &it->second.url;
			}
			auto external = std::make_unique<RE::BSScaleformExternalTexture>();
			if (!external->LoadPNG(RE::BSFixedString(a_path.c_str()))) {
				return nullptr;
			}
			// Lookup hashes the exact stored bytes, so build the URL from filePath as stored.
			std::string url = "img://"s + external->filePath.c_str();
			g_stats.loadPNG++;
			logger::debug("LoadPNG {} -> {}", a_path, url);
			const auto [it, _] = g_entries.emplace(key, Entry{ std::move(url), std::move(external) });
			return &it->second.url;
		}
	}

	bool Exists(const std::string& a_path)
	{
		if (const auto it = g_exists.find(a_path); it != g_exists.end()) {
			return it->second;
		}
		std::string path = a_path;
		std::ranges::replace(path, '/', '\\');
		RE::BSResourceNiBinaryStream file(path);
		const bool found = file.good() && file.stream && file.stream->totalSize > 0;
		g_exists.emplace(a_path, found);
		if (!found) {
			g_misses.insert(a_path);
		}
		return found;
	}

	const std::string* GetURL(const std::string& a_path, std::uint32_t a_targetPx)
	{
		const std::string key = std::format("{}@{}", a_path, a_targetPx);
		if (const auto it = g_entries.find(key); it != g_entries.end()) {
			return &it->second.url;
		}
		if (g_misses.contains(a_path)) {
			return nullptr;
		}
		// A size that picks an already-registered level reuses it; the name would register twice.
		if (const auto size = g_sourceSizes.find(a_path); size != g_sourceSizes.end()) {
			const auto [w, h] = size->second;
			const auto levelWidth = (std::max<std::size_t>)(1, w >> PickLevel(w, h, a_targetPx));
			if (const auto owner = g_entries.find(LevelKey(a_path, levelWidth)); owner != g_entries.end()) {
				logger::debug("shared {} (target {} px) -> {}", a_path, a_targetPx, owner->second.url);
				const auto [it, _] = g_entries.emplace(key, Entry{ owner->second.url, nullptr });
				return &it->second.url;
			}
		}
		if (const auto* url = TryScaled(a_path, a_targetPx)) {
			const auto [it, _] = g_entries.emplace(key, Entry{ *url, nullptr });
			return &it->second.url;
		}
		if (const auto* url = TryLoadPNG(a_path)) {
			const auto [it, _] = g_entries.emplace(key, Entry{ *url, nullptr });
			return &it->second.url;
		}
		logger::warn("no texture for {}", a_path);
		g_misses.insert(a_path);
		g_stats.missed++;
		return nullptr;
	}

	Stats GetStats()
	{
		return g_stats;
	}

	std::uint32_t StageToScreenPx(double a_stageUnits)
	{
		constexpr double kStageHeight = 720.0;
		const auto screen = RE::BSGraphics::Renderer::GetScreenSize();
		const double scale = screen.height > 0 ? screen.height / kStageHeight : 1.0;
		return static_cast<std::uint32_t>(std::ceil(a_stageUnits * scale));
	}
}
