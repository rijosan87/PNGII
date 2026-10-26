#include "NewItems.h"
#include "IconRules.h"

#include <mutex>

namespace NewItems
{
	namespace
	{
		constexpr RE::FormID kPlayer = 0x14;
		constexpr std::uint32_t kUniqueID = 'PNGI';
		constexpr std::uint32_t kRecord = 'NEWI';
		constexpr std::uint32_t kVersion = 1;
		constexpr std::size_t kMaxTracked = 4096;

		// The event may arrive off the UI thread; Apply and menu close run on the UI thread, save/load/revert on the main thread.
		std::mutex g_lock;
		std::unordered_set<RE::FormID> g_tracked;
		std::unordered_set<RE::FormID> g_pending;
		bool g_enabled = false;
		bool g_capWarned = false;

		class ContainerSink final : public RE::BSTEventSink<RE::TESContainerChangedEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* a_event,
				RE::BSTEventSource<RE::TESContainerChangedEvent>*) override
			{
				// Removals ignored: a form the player no longer holds is never drawn and shows new again if re-acquired.
				if (!a_event || a_event->newContainer != kPlayer || a_event->oldContainer == kPlayer ||
					a_event->itemCount <= 0 || !a_event->baseObj) {
					return RE::BSEventNotifyControl::kContinue;
				}
				std::scoped_lock lock(g_lock);
				if (g_tracked.size() >= kMaxTracked && !g_tracked.contains(a_event->baseObj)) {
					if (!g_capWarned) {
						g_capWarned = true;
						logger::warn("new items: tracking cap {} reached, further additions ignored", kMaxTracked);
					}
					return RE::BSEventNotifyControl::kContinue;
				}
				g_tracked.insert(a_event->baseObj);
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		void Save(SKSE::SerializationInterface* a_intfc)
		{
			if (!g_enabled) {
				return;
			}
			std::scoped_lock lock(g_lock);
			if (!a_intfc->OpenRecord(kRecord, kVersion)) {
				logger::error("new items: could not open NEWI record");
				return;
			}
			const auto count = static_cast<std::uint32_t>(g_tracked.size());
			a_intfc->WriteRecordData(count);
			for (const auto id : g_tracked) {
				a_intfc->WriteRecordData(id);
			}
			logger::debug("new items: saved {} tracked forms", count);
		}

		void Load(SKSE::SerializationInterface* a_intfc)
		{
			std::uint32_t type = 0;
			std::uint32_t version = 0;
			std::uint32_t length = 0;
			while (a_intfc->GetNextRecordInfo(type, version, length)) {
				if (!g_enabled || type != kRecord) {
					continue;
				}
				if (version != kVersion) {
					logger::warn("new items: unsupported NEWI version {}", version);
					continue;
				}
				std::uint32_t count = 0;
				if (a_intfc->ReadRecordData(count) != sizeof(count) || count > kMaxTracked ||
					length != sizeof(count) + count * sizeof(RE::FormID)) {
					logger::error("new items: rejected malformed NEWI record ({} bytes)", length);
					continue;
				}
				std::scoped_lock lock(g_lock);
				std::uint32_t dropped = 0;
				for (std::uint32_t i = 0; i < count; ++i) {
					RE::FormID oldID = 0;
					RE::FormID newID = 0;
					if (a_intfc->ReadRecordData(oldID) != sizeof(oldID) || !a_intfc->ResolveFormID(oldID, newID) ||
						!RE::TESForm::LookupByID(newID)) {
						++dropped;
						continue;
					}
					g_tracked.insert(newID);
				}
				logger::info("new items: loaded {} tracked forms ({} dropped)", g_tracked.size(), dropped);
			}
		}

		void Revert(SKSE::SerializationInterface*)
		{
			std::scoped_lock lock(g_lock);
			g_tracked.clear();
			g_pending.clear();
		}
	}

	void RegisterSerialization()
	{
		const auto* serde = SKSE::GetSerializationInterface();
		if (!serde) {
			logger::error("new items: no serialization interface");
			return;
		}
		serde->SetUniqueID(kUniqueID);
		serde->SetSaveCallback(Save);
		serde->SetLoadCallback(Load);
		serde->SetRevertCallback(Revert);
	}

	void Install()
	{
		g_enabled = IconRules::HasNewRules();
		if (!g_enabled) {
			logger::info("new items: no newItem rules, tracking off");
			return;
		}
		static ContainerSink sink;
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESContainerChangedEvent>(&sink);
		logger::info("new items: container sink registered");
	}

	bool IsNew(RE::FormID a_formID)
	{
		std::scoped_lock lock(g_lock);
		return g_tracked.contains(a_formID);
	}

	void MarkSeen(RE::FormID a_formID)
	{
		std::scoped_lock lock(g_lock);
		g_pending.insert(a_formID);
	}

	void CommitSeen()
	{
		std::scoped_lock lock(g_lock);
		for (const auto id : g_pending) {
			g_tracked.erase(id);
		}
		g_pending.clear();
	}
}
