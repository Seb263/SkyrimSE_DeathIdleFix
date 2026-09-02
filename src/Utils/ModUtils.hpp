#pragma once

#include "SettingsIni.hpp"

#define FRAME_DELAY_MS() std::chrono::milliseconds(static_cast<int>(std::lround(ModUtils::GetFrameDelay() * 1000.0f)))

class ModUtils
{
public:

	static bool SendAnimationEvent(RE::Actor* a_actor, const char* AnimationString)
	{
		if (const auto animGraphHolder = static_cast<RE::IAnimationGraphManagerHolder*>(a_actor)) {
			return animGraphHolder->NotifyAnimationGraph(AnimationString);
		} else {
			logger::warn("Failed to get animGraphHolder for Actor: '{}'", a_actor->GetName());
		}
		return false;
	}

	template <typename TInterval, typename TDuration, typename TCallback, typename TEndCallback = std::function<void()>>
	static void DoFor(TInterval interval, TDuration duration, std::function<bool()> stopCondition = []() { return false; },
					  TCallback&& callback = []() {}, TEndCallback&& endCallback = []() {}, const bool pausable = true, const bool secureFrame = true)
	{
		std::jthread([=]() mutable {
			WaitForGameReady();
			auto failure = std::make_shared<std::atomic_bool>(false);
			auto conditionMet = std::make_shared<std::atomic_bool>(false);
			auto deadline = (std::chrono::steady_clock::now() + duration);

			while (true) {
				if (pausable) {
					auto beforePause = std::chrono::steady_clock::now();
					if (WaitForGameReady(true)) deadline += (std::chrono::steady_clock::now() - beforePause);
				}

				const bool last = (std::chrono::steady_clock::now() + interval >= deadline);
				SKSE::GetTaskInterface()->AddTask([=, lastInner = last, taskStart = std::chrono::steady_clock::now()]() {
					if (secureFrame && std::chrono::steady_clock::now() - taskStart > 300ms) {
						if (!*failure) {
							*failure = true;
							TRACE("DoFor: Task was delayed and invalidated due to frame timing (>{}ms)", 300);
						}
					}
					if (*failure || *conditionMet) return;

					callback();
					*conditionMet = (stopCondition && stopCondition());
					if (lastInner || *conditionMet) {
						auto remaining = (*conditionMet ? 0ns : deadline - std::chrono::steady_clock::now());
						ModUtils::WaitAndCall(remaining > 0ns ? remaining : 0ns, [endCallback]() { endCallback(); }, secureFrame);
					}
				});

				std::this_thread::sleep_for(interval);
				if (last || *conditionMet) break;
			}
		}).detach();
	}

	template <typename TDuration, typename TCallback>
	static void WaitAndCall(TDuration delay, TCallback&& callback, const bool secureFrame = true)
	{
		std::jthread([delay, callback = std::forward<TCallback>(callback), secureFrame]() mutable {
			WaitForGameReady();
			auto failure = std::make_shared<std::atomic_bool>(false);
			const auto deadline = (std::chrono::steady_clock::now() + delay);

			while (true) {
				auto remaining = (deadline - std::chrono::steady_clock::now());
				std::this_thread::sleep_for(remaining > 100ms ? 100ms : (remaining > 0ns ? remaining : FRAME_DELAY_MS()));

				const bool last = (std::chrono::steady_clock::now() >= deadline);
				SKSE::GetTaskInterface()->AddTask([callback, secureFrame, failure, last, taskStart = std::chrono::steady_clock::now()]() {
					if (secureFrame && std::chrono::steady_clock::now() - taskStart > 300ms) *failure = true;
					if (last) {
						if (*failure) TRACE("WaitAndCall: Task was delayed and invalidated due to frame timing (>{}ms)", 300);
						else callback();
					}
				});
				if (last) break;
			}

		}).detach();
	}

	static RE::bhkRigidBody* GetRigidBody(RE::NiAVObject* a_object)
	{
		if (auto collisionObject = a_object->GetCollisionObject()) {
			return collisionObject->GetRigidBody();
		}
		return nullptr;
	}

	static bool WaitForGameReady(bool ignoreLoadingMenu = false)
	{
		bool wasPaused = false;

		while (true) {
			if (auto ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) {
				static auto loadingMenu = ui->GetMenu("Loading Menu");
				if (ignoreLoadingMenu && ui->numPausesGame == 1 && loadingMenu && loadingMenu->OnStack()) break;

				std::this_thread::sleep_for(FRAME_DELAY_MS());
				wasPaused = true;
				continue;
			}

			std::promise<void> p;
			auto f = p.get_future();

			SKSE::GetTaskInterface()->AddTask([&p]() { p.set_value(); });
        
			auto start = std::chrono::high_resolution_clock::now();
			f.get();

			if ((std::chrono::high_resolution_clock::now() - start) > 100ms) {
				wasPaused = true;
				continue;
			}

			break;
		}

		return wasPaused;
	}

	static float GetFrameDelay()
	{
		RE::BSTimer* bsTimer = RE::BSTimer::GetSingleton();
		if (!bsTimer) return 0.00694444f; // 144Hz

		float frame_delay = bsTimer->realTimeDelta / bsTimer->QGlobalTimeMultiplier();
		frame_delay = std::clamp(frame_delay, 0.004f, 0.1f);

		return frame_delay;
	}
};
