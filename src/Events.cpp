#include "Events.h"

namespace Events
{
	void ModEventSink::TimeBasedCellChange()
	{
		std::thread([]() {
			const auto player = RE::PlayerCharacter::GetSingleton();
			if (!player) REPORT_AND_FAIL("PlayerCharacter not found!");
			RE::FormID previousCell = 0x0;

			while (true) {
				SKSE::GetTaskInterface()->AddTask([player, &previousCell]() {
					const auto currentCell = player->GetParentCell() ? player->GetParentCell()->formID : 0x0;
					if (player->Is3DLoaded() && currentCell != previousCell) {
						previousCell = currentCell;
						TRACE("Player moved to new cell: [{:08X}].", currentCell);
						Events::MainEvent::ProceedDeathIdleFix();
					}
				});
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
		}).detach();
	}

	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESLoadGameEvent* event, RE::BSTEventSource<RE::TESLoadGameEvent>*)
	{
		Events::MainEvent::ProceedDeathIdleFix();

		return continueEvent;
	}

	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::BGSActorCellEvent* event, RE::BSTEventSource<RE::BGSActorCellEvent>*)
	{
		if (event->flags.all(RE::BGSActorCellEvent::CellFlag::kEnter) && event->flags.none(RE::BGSActorCellEvent::CellFlag::kLeave)) {
			TRACE("Player moved to new cell: [{:08X}].", event->cellID);

			Events::MainEvent::ProceedDeathIdleFix();
		}

		return continueEvent;
	}
}
