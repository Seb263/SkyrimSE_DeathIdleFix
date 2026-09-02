#include "Events.h"
#include "SettingsIni.hpp"

static inline const std::string_view MOD_NAME = "Death Idle Fix";

static inline bool postLoadEventsLoaded = false;

static void PostLoadEvents() {
	if (postLoadEventsLoaded) return;
	postLoadEventsLoaded = true;

	Events::ModEventSink::LoadEventsPostLoad();
};

static void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
	auto postLoadEventsAlternate = []() {
		std::jthread([]() {
			while (!postLoadEventsLoaded) {
				static std::atomic_bool taskRunning = false;
				if (!taskRunning.exchange(true)) {
					SKSE::GetTaskInterface()->AddTask([]() {
						auto player = RE::PlayerCharacter::GetSingleton();
						if (player && player->Is3DLoaded() && player->GetParentCell() && player->GetParentCell()->IsAttached()) {
							PostLoadEvents();
						}
						taskRunning = false;
					});
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}).detach();
	};

	switch (a_msg->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		Events::ModEventSink::LoadEvents();
		postLoadEventsAlternate();
		break;
	
	case SKSE::MessagingInterface::kPostLoadGame:
	case SKSE::MessagingInterface::kNewGame:
		PostLoadEvents();
		break;
	}
}

static void InitializeLog(std::string_view pluginName, spdlog::level::level_enum a_level = spdlog::level::info)
{
	auto path = logger::log_directory();
	if (!path) REPORT_AND_FAIL("Failed to find standard logging directory.");

	*path /= std::format("{}.log", pluginName);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

	const auto level = a_level;

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
	log->set_level(level);
	log->flush_on(spdlog::level::info);

	spdlog::set_default_logger(std::move(log));
	if (level == spdlog::level::trace) spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
	else spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	const auto plugin{ SKSE::PluginDeclaration::GetSingleton() };
	const auto name{ plugin->GetName() };
	const auto version{ plugin->GetVersion() };

	SKSE::Init(a_skse);

	if (!SettingsIni::ReadSettings()) {
		InitializeLog(name, spdlog::level::info);
		logger::warn("Failed to load settings file. Default settings will be used.");
	} else {
		if (SettingsIni::iVerboseMode <= 0) {
			InitializeLog(name, spdlog::level::err);
		} else if (SettingsIni::iVerboseMode >= 2) {
			InitializeLog(name, spdlog::level::trace);
		} else {
			InitializeLog(name, spdlog::level::info);
		}
	}

	logger::info("{} v{} by Seb263 : Loaded - Game version : {}", MOD_NAME, version.string("."), REL::Module::get().version().string("."));

	auto g_message = SKSE::GetMessagingInterface();
	if (!g_message) REPORT_AND_FAIL("Messaging Interface not found.");
	else if (!g_message->RegisterListener(MessageHandler)) REPORT_AND_FAIL("Failed to register MessageHandler listener.");
	else logger::info("Successfully registered MessageHandler listener.");

	return true;
}
