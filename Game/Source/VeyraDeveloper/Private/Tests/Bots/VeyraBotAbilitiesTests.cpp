// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Brain/VeyraBotAbilities.h"
#include "CQTest.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraBotAbilitiesTests
{
	inline FVeyraContentId ProfileId(const TCHAR* Text)
	{
		return FVeyraContentId::FromText(Text).GetValue();
	}

	// Veyra.Bots.BotAbilityProfiles.*: each archetype a released kit uses is one a bot can aim, reaching
	// as far as its tuning says (ADR-013 §4).
	TEST_CLASS(BotAbilityProfiles, "Veyra.Bots")
	{
		static constexpr double AnyAttackRange = 150.0;

		TEST_METHOD(EveryItemActiveABotUsesIsOneItCanAim)
		{
			// Bots cast the Actives Bots.json gives a use, so each of those must have a profile (ADR-051 §6).
			for (const TCHAR* Active : { TEXT("razorwheel_cleave"), TEXT("seize_momentum") })
			{
				const TOptional<FVeyraBotAbilityProfile> Profile = VeyraBotAbilities::ProfileOf(ProfileId(Active), AnyAttackRange);
				ASSERT_THAT(IsTrue(Profile.IsSet() && Profile->Reach > 0.0, Active));
			}
		}

		TEST_METHOD(ATetherOrALeapToHoldOnIsCastAtAUnitInItsCastRange)
		{
			const FVeyraContentId TetherId = ProfileId(TEXT("patch_dont_leave_me"));
			const FVeyraTetherAbilityTuning* Tether = UVeyraAbilitiesTuningSubsystem::FindTether(TetherId);
			ASSERT_THAT(IsNotNull(Tether));
			const TOptional<FVeyraBotAbilityProfile> Tethers = VeyraBotAbilities::ProfileOf(TetherId, AnyAttackRange);
			ASSERT_THAT(IsTrue(Tethers.IsSet()));
			ASSERT_THAT(IsTrue(Tethers->Targeting == EVeyraBotTargeting::Unit));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Tether->Cast.CastRange, Tethers->Reach)));
			ASSERT_THAT(IsTrue(Tethers->TargetKinds == Tether->TargetKinds));

			const FVeyraContentId AttachId = ProfileId(TEXT("patch_bear_hug"));
			const FVeyraAttachAbilityTuning* Attach = UVeyraAbilitiesTuningSubsystem::FindAttach(AttachId);
			ASSERT_THAT(IsNotNull(Attach));
			const TOptional<FVeyraBotAbilityProfile> Holds = VeyraBotAbilities::ProfileOf(AttachId, AnyAttackRange);
			ASSERT_THAT(IsTrue(Holds.IsSet()));
			ASSERT_THAT(IsTrue(Holds->Targeting == EVeyraBotTargeting::Unit));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Attach->Cast.CastRange, Holds->Reach)));
			ASSERT_THAT(IsTrue(Holds->CostByRank == Attach->Cast.ResourceCostByRank));
		}

		TEST_METHOD(ARideIsCastOnItselfAndReachesAsFarAsItCarriesItsRider)
		{
			const FVeyraContentId RideId = ProfileId(TEXT("raska_kickstart"));
			const FVeyraRideAbilityTuning* Ride = UVeyraAbilitiesTuningSubsystem::FindRide(RideId);
			ASSERT_THAT(IsNotNull(Ride));
			const TOptional<FVeyraBotAbilityProfile> Rides = VeyraBotAbilities::ProfileOf(RideId, AnyAttackRange);
			ASSERT_THAT(IsTrue(Rides.IsSet()));
			ASSERT_THAT(IsTrue(Rides->Targeting == EVeyraBotTargeting::Self));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Ride->SetSpeed * Ride->DurationSeconds, Rides->Reach)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Ride->Cast.WindupSeconds, Rides->LeadSeconds)));
		}

		TEST_METHOD(AnAreasFogCountsInItsReachAndAPointAreaIsMarked)
		{
			// A corridor of fog from its caster reaches as far as the corridor runs, past its small zone.
			const FVeyraContentId CorridorId = ProfileId(TEXT("sylra_through_the_white"));
			const FVeyraAreaAbilityTuning* Corridor = UVeyraAbilitiesTuningSubsystem::FindArea(CorridorId);
			ASSERT_THAT(IsTrue(Corridor && Corridor->Fog.Num() == 1 && Corridor->Origin == EVeyraAreaOrigin::Caster));
			const TOptional<FVeyraBotAbilityProfile> Runs = VeyraBotAbilities::ProfileOf(CorridorId, AnyAttackRange);
			ASSERT_THAT(IsTrue(Runs.IsSet() && FMath::IsNearlyEqual(Runs->Reach, Corridor->Fog[0].Length), FString::Printf(TEXT("%g"), Runs.IsSet() ? Runs->Reach : -1.0)));
			ASSERT_THAT(IsFalse(Runs->bAreaAtPoint));
			// A circle of fog at its point reaches its cast range and the circle.
			const FVeyraContentId MistId = ProfileId(TEXT("sylra_lay_the_mist"));
			const FVeyraAreaAbilityTuning* Mist = UVeyraAbilitiesTuningSubsystem::FindArea(MistId);
			ASSERT_THAT(IsTrue(Mist && Mist->Fog.Num() == 1 && Mist->Origin == EVeyraAreaOrigin::TargetPoint));
			const TOptional<FVeyraBotAbilityProfile> Lays = VeyraBotAbilities::ProfileOf(MistId, AnyAttackRange);
			ASSERT_THAT(IsTrue(Lays.IsSet() && Lays->bAreaAtPoint));
			ASSERT_THAT(IsTrue(Lays->Reach >= Mist->Cast.CastRange + Mist->Fog[0].Radius));
		}

		TEST_METHOD(ASelfBuffReachesAsFarAsItsCastersBasicAttack)
		{
			// Fixture value: any basic attack's reach.
			constexpr double AttackRange = 425.0;
			const TOptional<FVeyraBotAbilityProfile> Profile = VeyraBotAbilities::ProfileOf(ProfileId(TEXT("relay_full_grid")), AttackRange);
			ASSERT_THAT(IsTrue(Profile.IsSet()));
			ASSERT_THAT(IsTrue(Profile->Targeting == EVeyraBotTargeting::Self));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Profile->Reach, AttackRange), FString::Printf(TEXT("reach %g"), Profile->Reach)));
		}

		TEST_METHOD(ADashReachesAsFarAsItsLandingsAreasReach)
		{
			const FVeyraContentId DashId = ProfileId(TEXT("raska_last_exit"));
			const FVeyraDashAbilityTuning* Dash = UVeyraAbilitiesTuningSubsystem::FindDash(DashId);
			ASSERT_THAT(IsNotNull(Dash));
			ASSERT_THAT(IsFalse(Dash->EndZones.IsEmpty()));
			const TOptional<FVeyraBotAbilityProfile> Lands = VeyraBotAbilities::ProfileOf(DashId, AnyAttackRange);
			ASSERT_THAT(IsTrue(Lands.IsSet()));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Dash->Distance + Dash->EndZones[0].Shape.Radius, Lands->Reach)));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
