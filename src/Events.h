#pragma once

#include "Main.hpp"
#include "SettingsIni.hpp"

#include "Utils/ModUtils.hpp"

namespace Events
{
	class ModEventSink :
		public RE::BSTEventSink<RE::TESLoadGameEvent>,
		public RE::BSTEventSink<RE::BGSActorCellEvent>
	{
		ModEventSink() = default;
		ModEventSink(const ModEventSink&) = delete;
		ModEventSink(ModEventSink&&) = delete;
		ModEventSink& operator=(const ModEventSink&) = delete;
		ModEventSink& operator=(ModEventSink&&) = delete;

	public:
#define continueEvent RE::BSEventNotifyControl::kContinue

		static inline bool postLoadEventsLoaded = false;

		static ModEventSink* GetSingleton()
		{
			static ModEventSink singleton;
			return &singleton;
		}

		static void LoadEvents()
		{
			auto* eventSink = GetSingleton();
			auto* eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
			eventSourceHolder->AddEventSink<RE::TESLoadGameEvent>(eventSink);
		}

		static void TimeBasedCellChange();

		static void LoadEventsPostLoad()
		{
			if (postLoadEventsLoaded)
				return;

			if (!REL::Module::IsVR()) {
				auto*      eventSink = GetSingleton();
				const auto player = RE::PlayerCharacter::GetSingleton();
				if (!player)
					REPORT_AND_FAIL("PlayerCharacter not found!");
				player->AsBGSActorCellEventSource()->AddEventSink(eventSink);
			} else {
				ModEventSink::TimeBasedCellChange();
			}
			postLoadEventsLoaded = true;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::TESLoadGameEvent* event, RE::BSTEventSource<RE::TESLoadGameEvent>*);
		RE::BSEventNotifyControl ProcessEvent(const RE::BGSActorCellEvent* event, RE::BSTEventSource<RE::BGSActorCellEvent>*);
	};
};
