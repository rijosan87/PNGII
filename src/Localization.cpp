#include "Localization.h"

#include <iterator>

namespace Localization
{
	namespace
	{
		constexpr auto kFolder = "Data/SKSE/Plugins/Translations/";
		constexpr std::string_view kPrefix = "PNGIconInjector_";

		using Map = std::unordered_map<std::string, std::string>;
		Map g_labels;
		Map g_english;
		std::unordered_set<std::string> g_warned;
		std::vector<Language> g_available;
		std::string g_code = "en";
		std::uint32_t g_generation = 0;

		// Rejects a malformed file (GBK, UTF-16) before its bytes reach ImGui.
		bool IsValidUTF8(std::string_view a_text)
		{
			const auto* bytes = reinterpret_cast<const unsigned char*>(a_text.data());
			const std::size_t len = a_text.size();
			for (std::size_t i = 0; i < len;) {
				std::size_t extra = bytes[i] <= 0x7F ? 0 : (bytes[i] & 0xE0) == 0xC0 ? 1 : (bytes[i] & 0xF0) == 0xE0 ? 2 : (bytes[i] & 0xF8) == 0xF0 ? 3 : 4;
				if (extra == 4 || (extra != 0 && i + extra >= len)) {
					return false;
				}
				for (std::size_t j = 1; j <= extra; ++j) {
					if ((bytes[i + j] & 0xC0) != 0x80) {
						return false;
					}
				}
				i += extra + 1;
			}
			return true;
		}

		std::string_view Trim(std::string_view a_text)
		{
			const auto first = a_text.find_first_not_of(" \t\r\n");
			if (first == std::string_view::npos) {
				return {};
			}
			return a_text.substr(first, a_text.find_last_not_of(" \t\r\n") - first + 1);
		}

		bool LoadFile(const std::filesystem::path& a_path, Map& a_target)
		{
			std::ifstream file(a_path, std::ios::binary);
			if (!file) {
				return false;
			}
			std::string content{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
			if (content.starts_with("\xEF\xBB\xBF")) {
				content.erase(0, 3);
			}
			if (!IsValidUTF8(content)) {
				logger::error("translation file is not valid UTF-8, not loaded: {}", a_path.string());
				return false;
			}
			std::string section;
			std::size_t start = 0;
			while (start < content.size()) {
				auto end = content.find('\n', start);
				if (end == std::string::npos) {
					end = content.size();
				}
				const auto line = Trim(std::string_view(content).substr(start, end - start));
				start = end + 1;
				if (line.empty() || line.front() == ';' || line.front() == '#') {
					continue;
				}
				if (line.front() == '[' && line.back() == ']') {
					section = Trim(line.substr(1, line.size() - 2));
					continue;
				}
				const auto equals = line.find('=');
				if (equals == std::string_view::npos || section.empty()) {
					continue;
				}
				a_target[section + "." + std::string(Trim(line.substr(0, equals)))] = std::string(Trim(line.substr(equals + 1)));
			}
			return !a_target.empty();
		}

		std::filesystem::path PathFor(std::string_view a_code)
		{
			return std::filesystem::path(kFolder) / std::format("{}{}.txt", kPrefix, a_code);
		}

		void ScanAvailable()
		{
			g_available.clear();
			std::error_code ec;
			for (std::filesystem::directory_iterator it{ kFolder, ec }, end; !ec && it != end; it.increment(ec)) {
				const auto stem = it->path().stem().string();
				if (!stem.starts_with(kPrefix) || it->path().extension() != ".txt" || stem.size() == kPrefix.size()) {
					continue;
				}
				const auto code = stem.substr(kPrefix.size());
				Map meta;
				LoadFile(it->path(), meta);
				const auto name = meta.find("Meta.Name");
				g_available.push_back({ code, name != meta.end() && !name->second.empty() ? name->second : code });
			}
			if (std::ranges::none_of(g_available, [](const Language& a_language) { return a_language.code == "en"; })) {
				g_available.push_back({ "en", "English" });
			}
			std::ranges::sort(g_available, {}, &Language::name);
		}
	}

	void Load(std::string_view a_code)
	{
		g_code = a_code.empty() ? "en" : std::string(a_code);
		g_labels.clear();
		g_english.clear();
		g_warned.clear();
		if (LoadFile(PathFor("en"), g_english)) {
			logger::info("translations: en loaded ({} keys)", g_english.size());
		} else {
			logger::warn("translations: {} not loaded, the settings pages show keys", PathFor("en").string());
		}
		if (g_code != "en") {
			if (LoadFile(PathFor(g_code), g_labels)) {
				logger::info("translations: {} loaded ({} keys)", g_code, g_labels.size());
			} else {
				logger::warn("translations: {} not loaded, using English", PathFor(g_code).string());
			}
		}
		ScanAvailable();
		++g_generation;
	}

	const std::string& Code() { return g_code; }

	const std::string& Get(std::string_view a_key)
	{
		const std::string key(a_key);
		for (const Map* map : { &g_labels, &g_english }) {
			if (const auto it = map->find(key); it != map->end()) {
				return it->second;
			}
		}
		// Once per key: the page draws every frame.
		const auto [it, inserted] = g_warned.insert(key);
		if (inserted) {
			logger::warn("translations: missing key {}", key);
		}
		return *it;
	}

	std::uint32_t Generation() { return g_generation; }

	const std::vector<Language>& Available() { return g_available; }
}
