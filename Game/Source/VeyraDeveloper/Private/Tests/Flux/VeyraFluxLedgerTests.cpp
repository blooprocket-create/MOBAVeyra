// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Components/ActorTestSpawner.h"
#include "CQTest.h"
#include "State/VeyraTeamFluxState.h"
#include "Tuning/VeyraFluxTuningSubsystem.h"
#include "VeyraTeamFluxSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraFluxTests
{
	FVeyraFluxGrantTuning FluxGrantOf(EVeyraFluxDuration Duration, double Amount, double Seconds)
	{
		FVeyraFluxGrantTuning Grant;
		Grant.Duration = Duration;
		Grant.Amount = Amount;
		Grant.DurationSeconds = Seconds;
		return Grant;
	}

	// Veyra.Flux.Ledger.*: permanent and temporary Team Flux, each temporary grant on its own timer,
	// and the Fluxborn strength it gives (Battleground Bible §4, §10; ADR-011 §10).
	TEST_CLASS(Ledger, "Veyra.Flux")
	{
		// Fixture values, not tuning.
		static constexpr double Step = 25.0;
		static constexpr double Tolerance = 1e-9;

		TEST_METHOD(PermanentFluxStaysAndTemporaryFluxExpiresOnItsOwnTimer)
		{
			FVeyraFluxLedger Ledger;
			Ledger.Add(FluxGrantOf(EVeyraFluxDuration::Permanent, Step, 0.0), 0.0);
			Ledger.Add(FluxGrantOf(EVeyraFluxDuration::Temporary, Step, 100.0), 10.0);
			Ledger.Add(FluxGrantOf(EVeyraFluxDuration::Temporary, Step, 100.0), 50.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Ledger.Active(60.0), Step * 3.0, Tolerance)));
			ASSERT_THAT(IsTrue(Ledger.NextExpiry() == 110.0, TEXT("the older grant falls away first")));

			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Ledger.Active(110.0), Step * 2.0, Tolerance), TEXT("a newer grant does not refresh an older one")));
			ASSERT_THAT(IsTrue(Ledger.Expire(110.0)));
			ASSERT_THAT(IsFalse(Ledger.Expire(110.0), TEXT("nothing more is due")));
			ASSERT_THAT(IsTrue(Ledger.NextExpiry() == 150.0));
			ASSERT_THAT(IsTrue(Ledger.Expire(150.0) && FMath::IsNearlyEqual(Ledger.Active(150.0), Step, Tolerance)));
			ASSERT_THAT(IsFalse(Ledger.NextExpiry().IsSet(), TEXT("only permanent Flux is left")));
		}

		TEST_METHOD(EachFullStepStrengthensFluxborn)
		{
			FVeyraFluxbornScalingTuning Scaling;
			Scaling.StepFlux = Step;
			Scaling.HealthPerStep = 0.05;
			Scaling.DamagePerStep = 0.04;
			ASSERT_THAT(IsTrue(VeyraFlux::StrengthFor(0.0, Scaling) == FVeyraFluxbornStrength()));
			ASSERT_THAT(IsTrue(VeyraFlux::StrengthFor(Step - 1.0, Scaling) == FVeyraFluxbornStrength(), TEXT("a partial step adds nothing")));
			const FVeyraFluxbornStrength TwoSteps = VeyraFlux::StrengthFor(Step * 2.5, Scaling);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(TwoSteps.HealthMultiplier, 1.10, Tolerance) && FMath::IsNearlyEqual(TwoSteps.DamageMultiplier, 1.08, Tolerance)));
		}
	};

	// Veyra.Flux.TeamFluxSubsystem.*: the server's Team Flux, its announcements and its published state.
	TEST_CLASS(TeamFluxSubsystem, "Veyra.Flux")
	{
		FActorTestSpawner Spawner;

		TEST_METHOD(AGrantFollowsTheTuningAndIsAnnouncedAndPublished)
		{
			UVeyraTeamFluxSubsystem* Flux = Spawner.GetWorld().GetSubsystem<UVeyraTeamFluxSubsystem>();
			ASSERT_THAT(IsNotNull(Flux));
			Flux->OnWorldBeginPlay(Spawner.GetWorld());
			TArray<EVeyraTeam> Changed;
			Flux->OnTeamFluxChanged.AddLambda([&Changed](EVeyraTeam Team) { Changed.Add(Team); });

			const FVeyraFluxTuning& Tuning = UVeyraFluxTuningSubsystem::Get();
			Flux->Grant(EVeyraTeam::A, EVeyraFluxSource::LaneSpire);
			Flux->Grant(EVeyraTeam::A, EVeyraFluxSource::Inhibitor);
			ASSERT_THAT(IsTrue(Changed.Num() == 2 && Changed[0] == EVeyraTeam::A));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Flux->GetActive(EVeyraTeam::A), Tuning.Grants.LaneSpire.Amount + Tuning.Grants.Inhibitor.Amount)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Flux->GetPermanent(EVeyraTeam::A), Tuning.Grants.LaneSpire.Amount), TEXT("the inhibitor's grant is temporary")));
			ASSERT_THAT(IsTrue(Flux->GetActive(EVeyraTeam::B) == 0.0));
			ASSERT_THAT(IsTrue(Flux->GetFluxbornStrength(EVeyraTeam::A) == VeyraFlux::StrengthFor(Flux->GetActive(EVeyraTeam::A), Tuning.FluxbornScaling)));

			const AVeyraTeamFluxState* State = Flux->GetState();
			ASSERT_THAT(IsNotNull(State));
			const FVeyraTeamFluxView* TeamA = State->Find(EVeyraTeam::A);
			ASSERT_THAT(IsTrue(TeamA && TeamA->Permanent == Tuning.Grants.LaneSpire.Amount && TeamA->Temporary.Num() == 1));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(TeamA->Temporary[0].ExpiresAt, Spawner.GetWorld().GetTimeSeconds() + Tuning.Grants.Inhibitor.DurationSeconds)));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
