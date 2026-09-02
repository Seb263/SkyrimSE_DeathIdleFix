#pragma once

#include "SettingsIni.hpp"

#include "Utils\ModUtils.hpp"

namespace Events
{
	class MainEvent
	{
		public:

			static void ProceedDeathIdleFix()
			{
				std::jthread([]() {
					ModUtils::WaitForGameReady();
					
					ModUtils::WaitAndCall(std::chrono::milliseconds(FRAME_DELAY_MS()), []() {
						CellsMaintenanceTask();
					}, false);
				}).detach();
			}

		private:

			static void CellsMaintenanceTask()
			{
				const auto player = RE::PlayerCharacter::GetSingleton();
				if (!player) return;

				TRACE("Process Cells Maintenance Task.");

				const auto& playerPosition = RE::NiPoint3(player->GetPositionX(), player->GetPositionY(), 0.0f);
				const auto processActor = [](RE::Actor* ref) {
                    if (!ref || !ref->IsDead() || !ref->IsInRagdollState() || !ref->GetParentCell() || !ref->GetParentCell()->IsAttached()) return;

					FixActor(ref);
				};

				const auto processCell = [&](RE::TESObjectCELL* cell) {
					if (!cell || !cell->IsAttached() ||
						(cell->IsExteriorCell() && playerPosition.GetDistance({ cell->GetCoordinates()->worldX, cell->GetCoordinates()->worldY, 0.0f }) > 8192.0f)) return;
				
					TRACE("  -> Maintenance on cell [{:08X}]", cell->formID);

					cell->ForEachReference([&](RE::TESObjectREFR* ref) -> RE::BSContainer::ForEachResult {
						processActor(ref ? ref->As<RE::Actor>() : nullptr);
						return RE::BSContainer::ForEachResult::kContinue;
					});
				};

				auto* gridCells = RE::TES::GetSingleton()->gridCells;
				if (gridCells && gridCells->cells) {
					for (std::size_t i = 0; i < gridCells->length * gridCells->length; ++i) {
						processCell(gridCells->cells[i]);
					}
				}
				processCell(RE::TES::GetSingleton()->interiorCell);

				TRACE("Ended Cells Maintenance Task.");
			}

			static void FixActor(RE::Actor* actor)
			{
				if (!actor) return;

				if (ModUtils::SendAnimationEvent(actor, "RagdollInstant")) {
					if (SettingsIni::bRagdollStabilization) TweakPhysic(actor);
                    
					RE::FormID baseFormID = actor->GetBaseObject() ? actor->GetBaseObject()->formID : 0x0;
					TRACE("    -> Death Idle Fixed on actor <\"{}\" [REF:{:08X}] [BASE:{:08X}]>", actor->GetName(), actor->formID, baseFormID);
				}
			}

			static void TweakPhysic(RE::Actor* actor)
			{
				if (!actor) return;

				auto hkpRigidBody = GetValidRagdollRigidBody(actor);
				if (!hkpRigidBody) return;

				auto& motionState = hkpRigidBody->motion.motionState;

				const std::uint8_t originalLinear = motionState.maxLinearVelocity.value;
				const std::uint8_t originalAngular = motionState.maxAngularVelocity.value;

				constexpr std::uint8_t initialLinearValue = 10;
				constexpr std::uint8_t initialAngularValue = 5;
				constexpr auto duration = 1500ms;

				motionState.maxLinearVelocity.value = initialLinearValue;
				motionState.maxAngularVelocity.value = initialAngularValue;

				ModUtils::WaitAndCall(500ms, [actorFormID = actor->formID, originalLinear, originalAngular, duration]() {
					ModUtils::DoFor(FRAME_DELAY_MS(), duration, []() { return false; }, [actorFormID, originalLinear, originalAngular, start = std::chrono::steady_clock::now(), duration]() {
						RE::Actor* actor = RE::TESForm::LookupByID<RE::Actor>(actorFormID);
						if (!actor) return;

						auto hkpRigidBody = GetValidRagdollRigidBody(actor);
						if (!hkpRigidBody) return;

						auto& motionState = hkpRigidBody->motion.motionState;

						auto now = std::chrono::steady_clock::now();
						auto elapsed = now - start;
						auto progress = std::clamp(
							static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()) /
							static_cast<float>(duration.count()), 0.0f, 1.0f
						);

						motionState.maxLinearVelocity.value = static_cast<std::uint8_t>(initialLinearValue + (originalLinear - initialLinearValue) * progress);
						motionState.maxAngularVelocity.value = static_cast<std::uint8_t>(initialAngularValue + (originalAngular - initialAngularValue) * progress);
					},
					[actorFormID, originalLinear, originalAngular]() {
						RE::Actor* actor = RE::TESForm::LookupByID<RE::Actor>(actorFormID);
						if (!actor) return;

						auto hkpRigidBody = GetValidRagdollRigidBody(actor);
						if (!hkpRigidBody) return;

						auto& motionState = hkpRigidBody->motion.motionState;
						motionState.maxLinearVelocity.value = originalLinear;
						motionState.maxAngularVelocity.value = originalAngular;
					});
				}, false);
			}

			static RE::hkpRigidBody* GetValidRagdollRigidBody(RE::Actor* actor)
			{
				if (!actor || !actor->Is3DLoaded() || !actor->IsInRagdollState()) return nullptr;

				RE::NiAVObject* niAVObject = actor->Get3D(false);
				if (!niAVObject) return nullptr;

				RE::NiPointer<RE::NiAVObject> root(niAVObject);
				if (!root) return nullptr;

				if (auto rb = ModUtils::GetRigidBody(root.get())) {
					if (auto hkpRigidBody = static_cast<RE::hkpRigidBody*>(rb->referencedObject.get())) {
						if (hkpRigidBody->world && hkpRigidBody->motion.GetMass() > 0.0f) return hkpRigidBody;
					}
				}
				return nullptr;
			}
	};
};
