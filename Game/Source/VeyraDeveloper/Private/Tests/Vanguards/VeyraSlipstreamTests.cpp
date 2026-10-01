// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Delivery/VeyraLingeringArea.h"
#include "EngineUtils.h"
#include "Passives/VeyraSlipstreamPassive.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraVanguards.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	// Veyra.Vanguards.Slipstream.*: Aurelisse's passive and the kit around it (Roster Bible §24; ADR-027
	// §4–§8), from the committed tuning.
	TEST_CLASS(Slipstream, "Veyra.Vanguards")
	{
		// Fixture values: an ally within Windward's reach.
		static constexpr double Near = 300.0;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Aurelisse = nullptr;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Aurelisse = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Aurelisse->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), Id(TEXT("aurelisse")));
			ASSERT_THAT(IsTrue(Prepared.bPrepared && Cast<UVeyraSlipstreamPassive>(Prepared.Passive) != nullptr));
			Participant->SetPassive(Prepared.Passive);
			ASSERT_THAT(IsTrue(World.Learn(*Aurelisse, EVeyraAbilitySlot::E, Id(TEXT("aurelisse_windward")))));
		}

		static FVeyraContentId Id(const TCHAR* Text)
		{
			return FVeyraContentId::FromText(Text).GetValue();
		}

		static const FVeyraSlipstreamTuning& Tuning()
		{
			return *UVeyraVanguardsTuningSubsystem::FindSlipstream(Id(TEXT("aurelisse_slipstream")));
		}

		EVeyraCastRejection Windward(AActor* Named) const
		{
			FVeyraCastTarget Target;
			Target.Actor = Named;
			return VeyraAbilities::TryCast(*Aurelisse->GetAbilitySystemComponent(), EVeyraAbilitySlot::E, Target);
		}

		const AVeyraLingeringArea* Current()
		{
			for (TActorIterator<AVeyraLingeringArea> It(&Spawner.GetWorld()); It; ++It)
			{
				if (It->GetAbility() == Id(TEXT("aurelisse_slipstream")))
				{
					return *It;
				}
			}
			return nullptr;
		}

		TEST_METHOD(WindwardOnAnAllyLeavesACurrentThatSpeedsThem)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Ally = World.Spawn(EVeyraTeam::A, FVector(Near, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Windward(&Ally) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Current() != nullptr, TEXT("a current from her to the ally")));
			for (const FVeyraContentId& Status : Tuning().Statuses)
			{
				ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(Ally, *Status.ToString()), TEXT("speeding the ally inside it")));
			}
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(Ally, TEXT("aurelisse_windward_haste")), TEXT("and Windward itself buffs the ally")));
		}

		TEST_METHOD(WindwardOnHerselfLeavesNoCurrent)
		{
			ASSERT_THAT(IsTrue(Windward(nullptr) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Current() == nullptr));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(*Aurelisse, TEXT("aurelisse_windward_haste"))));
		}

		TEST_METHOD(TheKitMatchesItsCanon)
		{
			const FVeyraAbilitiesTuning& Abilities = UVeyraAbilitiesTuningSubsystem::Get();
			const FVeyraSelfBuffAbilityTuning& Guard = Abilities.SelfBuff.FindChecked(Id(TEXT("aurelisse_windward")));
			ASSERT_THAT(IsTrue(Guard.Recipient == EVeyraBuffRecipient::CasterOrAlly && Guard.Shields.Num() == 1 && Guard.Shields[0].AbsorbedReward.Num() == 1,
				TEXT("Windward: herself or an ally, a shield, and a second burst once it absorbs enough (§24)")));
			const FVeyraSelfBuffAbilityTuning& Room = Abilities.SelfBuff.FindChecked(Id(TEXT("aurelisse_room_to_breathe")));
			ASSERT_THAT(IsTrue(Room.Recipient == EVeyraBuffRecipient::CasterOrAlly && !Room.RecipientZones.IsEmpty() && !Room.Aura.IsEmpty(),
				TEXT("ROOM TO BREATHE: pushes once from the recipient, and its aura follows them")));
			const FVeyraAreaAbilityTuning& Rising = Abilities.Area.FindChecked(Id(TEXT("aurelisse_rising_current")));
			ASSERT_THAT(IsTrue(Rising.DelaySeconds > 0.0 && !Rising.Linger.IsEmpty(), TEXT("Rising Current: a warned knockup that lingers as an updraft")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
