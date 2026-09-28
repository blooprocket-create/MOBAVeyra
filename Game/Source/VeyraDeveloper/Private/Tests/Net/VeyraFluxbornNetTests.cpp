// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "EngineUtils.h"
#include "Fluxborn/VeyraFluxborn.h"
#include "Tests/Net/VeyraBattlegroundNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraCombatVerbs.h"

namespace VeyraNetTests
{
	// Veyra.Net.FluxbornLanes.*: Fluxborn march their lane under their server controllers, fight the
	// enemy's where the waves meet, and fall; every client sees it (ADR-011 §7).
	NETWORK_TEST_CLASS(FluxbornLanes, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Greybox;
		TWeakObjectPtr<AVeyraFluxborn> Ours;
		TWeakObjectPtr<AVeyraFluxborn> Theirs;
		FVector2D OursStart = FVector2D::ZeroVector;

		// Fixture values: a short preparation, and how far along its lane a Fluxborn must be seen to walk.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double MarchProof = 300.0;

		BEFORE_EACH()
		{
			IgnoreLoginViewTargetRpc(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Greybox).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		static FVeyraContentId Strider()
		{
			return FVeyraContentId::FromText(TEXT("strider")).GetValue();
		}

		/** The Fluxborn of Team this machine sees, in no particular order. */
		static TArray<const AVeyraFluxborn*> SeenFluxborn(const UWorld* World, EVeyraTeam Team)
		{
			TArray<const AVeyraFluxborn*> Seen;
			for (TActorIterator<AVeyraFluxborn> It(World); It; ++It)
			{
				if (It->GetVeyraTeam() == Team && !It->IsActorBeingDestroyed())
				{
					Seen.Add(*It);
				}
			}
			return Seen;
		}

		static bool IsHurt(const AVeyraFluxborn* Unit)
		{
			return Unit && VeyraCombat::GetMissingHealth(*Unit->GetAbilitySystemComponent()) > 0.0;
		}

		TEST_METHOD(OpposingFluxbornMarchMeetAndFight)
		{
			StartBattleground(Network, Greybox, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Each team sends a Strider down the mid lane"), [this](FState& State) {
					UVeyraBattlegroundSubsystem* Battleground = State.World->GetSubsystem<UVeyraBattlegroundSubsystem>();
					Ours = Battleground->SpawnFluxborn(Strider(), EVeyraTeam::A, EVeyraLane::Mid);
					Theirs = Battleground->SpawnFluxborn(Strider(), EVeyraTeam::B, EVeyraLane::Mid);
					ASSERT_THAT(IsTrue(Ours.IsValid() && Theirs.IsValid()));
					OursStart = FVector2D(Ours->GetActorLocation());
				})
				.UntilClients(TEXT("Every client sees team A's Strider march toward team B"), [this](FState& State) {
					// The compact mid lane runs up the diagonal, toward team B.
					const FVector2D Toward = FVector2D(1.0, 1.0).GetSafeNormal();
					for (const AVeyraFluxborn* Unit : SeenFluxborn(State.World, EVeyraTeam::A))
					{
						if (FVector2D::DotProduct(FVector2D(Unit->GetActorLocation()) - OursStart, Toward) >= MarchProof)
						{
							return true;
						}
					}
					return false;
				})
				.UntilClients(TEXT("Every client sees them hurt each other where they meet"), [](FState& State) {
					const TArray<const AVeyraFluxborn*> A = SeenFluxborn(State.World, EVeyraTeam::A);
					const TArray<const AVeyraFluxborn*> B = SeenFluxborn(State.World, EVeyraTeam::B);
					return A.Num() == 1 && B.Num() == 1 && IsHurt(A[0]) && IsHurt(B[0]);
				})
				.ThenServer(TEXT("Team A's Strider falls"), [this](FState& /*State*/) {
					AVeyraFluxborn* Unit = Ours.Get();
					ASSERT_THAT(IsNotNull(Unit));
					UAbilitySystemComponent& Victim = *Unit->GetAbilitySystemComponent();
					FVeyraRawDamageEvent Lethal;
					Lethal.Components.Add({ EVeyraDamageType::TrueDamage, Victim.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) });
					Lethal.Delivery = EVeyraDamageDelivery::Developer;
					ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Theirs->GetAbilitySystemComponent(), Victim, Lethal) && !Unit->IsAlive()));
				})
				.UntilClients(TEXT("Every client sees its body removed"), [](FState& State) { return SeenFluxborn(State.World, EVeyraTeam::A).IsEmpty(); });
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
