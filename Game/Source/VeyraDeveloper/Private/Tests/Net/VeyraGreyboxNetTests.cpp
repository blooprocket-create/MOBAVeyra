// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST && WITH_VEYRA_UI

#include "GameFramework/PlayerState.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Greybox/VeyraGreyboxSubsystem.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraPlayerState.h"

namespace VeyraNetTests
{
	// Veyra.Net.GreyboxPresentation.*: the grey-box presentation in a match (ADR-008 §1). The dedicated
	// server draws nothing; each client draws both Vanguards in their colours as it sees them, and
	// telegraphs a windup and a delayed area where the server lands them. Both players are Cairn.
	NETWORK_TEST_CLASS(GreyboxPresentation, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		TUniquePtr<FScopedAbilitiesTuning> Abilities;
		FVeyraGreyboxLayout Layout;

		// Fixture values: a short preparation, the level that opens the first ultimate rank, a windup
		// and a delay long enough for every client to see them, and an allowance for float positions.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr int32 UltimateLevel = 6;
		static constexpr double LongSeconds = 30.0;
		static constexpr double PositionSlack = 5.0;

		int32 CasterId = INDEX_NONE;
		int32 TargetId = INDEX_NONE;

		BEFORE_EACH()
		{
			IgnoreLoginViewTargetRpc(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Tuning->Tuning.DeveloperMatch.Vanguards = { Id(TEXT("cairn")) };
			Tuning->Tuning.DeveloperMatch.StartingRank = EVeyraDeveloperStartingRank::None;
			// Cairn's committed kit, with his ultimate's windup and his slam's delay held long.
			const FVeyraAbilitiesTuning Committed = UVeyraAbilitiesTuningSubsystem::Get();
			Abilities = MakeUnique<FScopedAbilitiesTuning>();
			Abilities->Tuning = Committed;
			Abilities->Tuning.Area.FindChecked(Id(TEXT("cairn_burden_of_the_depths"))).Cast.WindupSeconds = LongSeconds;
			FVeyraAreaAbilityTuning& Slam = Abilities->Tuning.Area.FindChecked(Id(TEXT("cairn_crushing_hold")));
			Slam.Cast.WindupSeconds = 0.0;
			Slam.DelaySeconds = LongSeconds;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Abilities.Reset();
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		static FVeyraContentId Id(const TCHAR* Text)
		{
			return FVeyraContentId::FromText(Text).GetValue();
		}

		static AVeyraPlayerState* ParticipantOf(FState& State, int32 ClientIndex)
		{
			const AVeyraPlayerController* Controller = ServerControllerOf(State, ClientIndex);
			return Controller ? Controller->GetPlayerState<AVeyraPlayerState>() : nullptr;
		}

		/** This machine's presentation, brought up to date; null where there is none. */
		static UVeyraGreyboxSubsystem* PresentationOf(FState& State)
		{
			UVeyraGreyboxSubsystem* Greybox = State.World->GetSubsystem<UVeyraGreyboxSubsystem>();
			if (Greybox)
			{
				Greybox->Refresh();
			}
			return Greybox;
		}

		FPIENetworkComponent<FState>& Identify(FPIENetworkComponent<FState>& Chain)
		{
			return Chain.ThenServer(TEXT("Identify the players"), [this](FState& State) {
				CasterId = ParticipantOf(State, 0)->GetPlayerId();
				TargetId = ParticipantOf(State, 1)->GetPlayerId();
			});
		}

		/** On the server: the first player reaches the ultimate's level and ranks Slot. */
		void Prepare(FState& State, EVeyraAbilitySlot Slot)
		{
			AVeyraPlayerState* Caster = ParticipantOf(State, 0);
			ASSERT_THAT(IsTrue(Caster && Caster->GetVanguardId() == Id(TEXT("cairn"))));
			const TArray<int32>& ToNextLevel = UVeyraProgressionTuningSubsystem::Get().Experience.ToNextLevel;
			int32 Experience = 0;
			for (int32 Level = 1; Level < UltimateLevel; ++Level)
			{
				Experience += ToNextLevel[Level - 1];
			}
			UVeyraProgressionComponent* Progression = Caster->FindComponentByClass<UVeyraProgressionComponent>();
			Progression->AddExperience(Experience);
			ASSERT_THAT(IsTrue(Progression->AllocateRank(Slot) == EVeyraRankRefusal::None));
		}

		void CastAtTheTarget(FState& State, EVeyraAbilitySlot Slot) const
		{
			const AVeyraVanguardCharacter* Enemy = FindVanguard(State.World, TargetId);
			FVeyraCastTarget Target;
			Target.bHasLocation = true;
			Target.Location = Enemy ? Enemy->GetActorLocation() : FVector::ZeroVector;
			LocalControllerOf(State.World)->IssueCastOrder(Slot, Target);
		}

		/** Whether this machine telegraphs Ability's zones from Source, innermost first, where the caster stands. */
		bool SeesTelegraph(FState& State, const TCHAR* Ability, EVeyraTelegraphSource Source) const
		{
			const UVeyraGreyboxSubsystem* Greybox = PresentationOf(State);
			const AVeyraVanguardCharacter* Caster = FindVanguard(State.World, CasterId);
			const FVeyraAreaAbilityTuning* Area = UVeyraAbilitiesTuningSubsystem::FindArea(Id(Ability));
			if (!Greybox || !Caster || !Area)
			{
				return false;
			}
			const TArray<FVeyraTelegraph> Shown = Greybox->GetTelegraphs().FilterByPredicate([Source](const FVeyraTelegraph& Each) { return Each.Source == Source; });
			if (Shown.Num() != Area->Zones.Num())
			{
				return false;
			}
			for (int32 Index = 0; Index < Shown.Num(); ++Index)
			{
				const FVeyraShape& Drawn = Shown[Index].Placed.Shape;
				const FVeyraShape& Tuned = Area->Zones[Index].Shape;
				const bool bSameShape = Drawn.Kind == Tuned.Kind && Drawn.Radius == Tuned.Radius && Drawn.ArcDegrees == Tuned.ArcDegrees && Drawn.Length == Tuned.Length
					&& Drawn.Width == Tuned.Width;
				if (!bSameShape || FVector::Dist2D(Shown[Index].Placed.Origin, Caster->GetActorLocation()) > PositionSlack)
				{
					return false;
				}
			}
			return true;
		}

		TEST_METHOD(OnlyClientsDrawTheMatch)
		{
			Identify(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenServer(TEXT("The server has no presentation"), [this](FState& State) {
					ASSERT_THAT(IsNull(State.World->GetSubsystem<UVeyraGreyboxSubsystem>()));
				})
				.UntilClients(TEXT("Each client draws its own Vanguard and its enemy"), [this](FState& State) {
					const UVeyraGreyboxSubsystem* Greybox = PresentationOf(State);
					const AVeyraVanguardCharacter* Own = LocalControllerOf(State.World)->GetVanguard();
					const AVeyraVanguardCharacter* First = FindVanguard(State.World, CasterId);
					const AVeyraVanguardCharacter* Second = FindVanguard(State.World, TargetId);
					if (!Greybox || !Own || !First || !Second || !Greybox->FindBody(*First) || !Greybox->FindBody(*Second))
					{
						return false;
					}
					const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
					const AVeyraVanguardCharacter& Enemy = Own == First ? *Second : *First;
					return Greybox->BodyColorOf(*Own).Equals(Settings.OwnColor) && Greybox->BodyColorOf(Enemy).Equals(Settings.EnemyColor);
				});
		}

		TEST_METHOD(EachClientTelegraphsAWindupWhereTheServerWillLandIt)
		{
			Identify(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenServer(TEXT("Cairn takes his ultimate"), [this](FState& State) { Prepare(State, EVeyraAbilitySlot::R); })
				.ThenClient(TEXT("Cast it"), 0, [this](FState& State) { CastAtTheTarget(State, EVeyraAbilitySlot::R); })
				.UntilClients(TEXT("Each client telegraphs its zones at Cairn"), [this](FState& State) {
					return SeesTelegraph(State, TEXT("cairn_burden_of_the_depths"), EVeyraTelegraphSource::Windup);
				});
		}

		TEST_METHOD(EachClientTelegraphsADelayedArea)
		{
			Identify(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenServer(TEXT("Cairn ranks his slam"), [this](FState& State) { Prepare(State, EVeyraAbilitySlot::W); })
				.ThenClient(TEXT("Cast it"), 0, [this](FState& State) { CastAtTheTarget(State, EVeyraAbilitySlot::W); })
				.UntilClients(TEXT("Each client telegraphs the waiting area where Cairn cast it"), [this](FState& State) {
					return SeesTelegraph(State, TEXT("cairn_crushing_hold"), EVeyraTelegraphSource::DelayedArea);
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST && WITH_VEYRA_UI
