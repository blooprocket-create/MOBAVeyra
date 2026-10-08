// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Components/ActorTestSpawner.h"
#include "Engine/World.h"
#include "Kit/VeyraKitPresentation.h"
#include "Kit/VeyraKitPresentationSettings.h"
#include "Kit/VeyraKitPresentationSubsystem.h"
#include "Statuses/VeyraStatusTypes.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuning.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraVanguardCharacter.h"

namespace VeyraKitPresentationTests
{
	// Veyra.UI.KitPresentation.*: what the kit presentation shows of a self-buff's aura and end payload, read from fixture
	// tuning, and how a strand's beads flow (ADR-071).
	TEST_CLASS(KitPresentation, "Veyra.UI")
	{
		static constexpr double AuraRadius = 350.0;
		static constexpr double AuraSeconds = 8.0;
		static constexpr double BurstAfter = 1.5;
		static constexpr double BurstRadius = 300.0;
		static constexpr double StartedAt = 10.0;
		static constexpr double Slack = 1e-6;

		static FVeyraContentId Id(const TCHAR* Name)
		{
			return FVeyraContentId::FromText(Name).GetValue();
		}

		/** A buff with an aura and a payload, marked by its first status. */
		static FVeyraAbilitiesTuning Tuning()
		{
			FVeyraAbilitiesTuning Tuning;
			FVeyraSelfBuffAbilityTuning& Buff = Tuning.SelfBuff.Add(Id(TEXT("test_giant")));
			Buff.Statuses = { Id(TEXT("test_giant_body")), Id(TEXT("test_giant_footing")) };
			FVeyraAuraTuning& Aura = Buff.Aura.AddDefaulted_GetRef();
			Aura.Radius = AuraRadius;
			Aura.DurationSeconds = AuraSeconds;
			FVeyraEndPayloadTuning& Payload = Buff.EndPayload.AddDefaulted_GetRef();
			Payload.AfterSeconds = BurstAfter;
			Payload.Radius = BurstRadius;
			return Tuning;
		}

		static FVeyraStatusEntry Entry(const TCHAR* Name)
		{
			FVeyraStatusEntry Entry;
			Entry.Id = Id(Name);
			Entry.StartedAt = StartedAt;
			return Entry;
		}

		TEST_METHOD(ABuffsAuraShowsWhileItLastsByItsFirstStatus)
		{
			const FVeyraKitPresentationIndex Index = FVeyraKitPresentationIndex::Build(Tuning());
			const TArray<FVeyraShownAura> Shown = Index.AurasOf(Entry(TEXT("test_giant_body")), StartedAt + 1.0);
			ASSERT_THAT(AreEqual(1, Shown.Num()));
			ASSERT_THAT(IsNear(Shown[0].Radius, AuraRadius, Slack, TEXT("its reach is Abilities.json's, not a second copy")));
			ASSERT_THAT(IsTrue(Index.AurasOf(Entry(TEXT("test_giant_body")), StartedAt + AuraSeconds).IsEmpty(), TEXT("gone as it ends")));
			ASSERT_THAT(IsTrue(Index.AurasOf(Entry(TEXT("test_giant_footing")), StartedAt + 1.0).IsEmpty(), TEXT("its other statuses do not draw it twice")));
		}

		TEST_METHOD(APayloadBurstsAtItsTimeAfterTheCastAndSpreadsToItsReach)
		{
			const FVeyraKitPresentationIndex Index = FVeyraKitPresentationIndex::Build(Tuning());
			const TArray<FVeyraShownBurst> Bursts = Index.BurstsOf(Entry(TEXT("test_giant_body")));
			ASSERT_THAT(AreEqual(1, Bursts.Num()));
			ASSERT_THAT(IsNear(Bursts[0].At, StartedAt + BurstAfter, Slack));
			ASSERT_THAT(IsNear(Bursts[0].Radius, BurstRadius, Slack));
			constexpr double Spread = 0.4;
			ASSERT_THAT(IsFalse(VeyraKitPresentation::BurstShareAt(Bursts[0], Bursts[0].At - 0.01, Spread).IsSet(), TEXT("not before it comes")));
			ASSERT_THAT(IsNear(VeyraKitPresentation::BurstShareAt(Bursts[0], Bursts[0].At + Spread / 2.0, Spread).Get(0.0), 0.5, Slack));
			ASSERT_THAT(IsFalse(VeyraKitPresentation::BurstShareAt(Bursts[0], Bursts[0].At + Spread * 2.0, Spread).IsSet(), TEXT("nor once spread")));
		}

		TEST_METHOD(OnlyAPayloadThatAlwaysComesIsShown)
		{
			// A buff fires its first payload alone, and one that needs hits may never come: neither shows a burst.
			FVeyraAbilitiesTuning Second = Tuning();
			FVeyraEndPayloadTuning& Later = Second.SelfBuff.FindChecked(Id(TEXT("test_giant"))).EndPayload.AddDefaulted_GetRef();
			Later.AfterSeconds = BurstAfter * 2.0;
			Later.Radius = BurstRadius * 2.0;
			ASSERT_THAT(AreEqual(1, FVeyraKitPresentationIndex::Build(Second).BurstsOf(Entry(TEXT("test_giant_body"))).Num(), TEXT("only the first")));
			FVeyraAbilitiesTuning Countered = Tuning();
			Countered.SelfBuff.FindChecked(Id(TEXT("test_giant"))).EndPayload[0].MinHits = 1;
			ASSERT_THAT(IsTrue(FVeyraKitPresentationIndex::Build(Countered).BurstsOf(Entry(TEXT("test_giant_body"))).IsEmpty(),
				TEXT("a payload that needs hits a client cannot count shows nothing")));
			ASSERT_THAT(AreEqual(1, FVeyraKitPresentationIndex::Build(Countered).AurasOf(Entry(TEXT("test_giant_body")), StartedAt).Num(),
				TEXT("its aura still shows")));
		}

		TEST_METHOD(AStrandsBeadsFlowEvenlyTowardItsSource)
		{
			constexpr int32 Count = 4;
			constexpr double Flow = 1.0;
			ASSERT_THAT(IsNear(VeyraKitPresentation::BeadShareAt(0, Count, 0.0, Flow), 0.0, Slack));
			ASSERT_THAT(IsNear(VeyraKitPresentation::BeadShareAt(1, Count, 0.0, Flow), 0.25, Slack, TEXT("evenly spaced")));
			ASSERT_THAT(IsNear(VeyraKitPresentation::BeadShareAt(0, Count, 0.5, Flow), 0.5, Slack, TEXT("halfway to the source in half its time")));
		}

		TEST_METHOD(TheShippedKitPresentationIsValid)
		{
			const TArray<FString> Problems = GetDefault<UVeyraKitPresentationSettings>()->Validate();
			ASSERT_THAT(IsTrue(Problems.IsEmpty(), *FString::Join(Problems, TEXT("; "))));
			ASSERT_THAT(IsTrue(GetDefault<UVeyraKitPresentationSettings>()->IsStrand(TEXT("PATCH_THREADED")), TEXT("statuses compare ignoring case")));
		}
	};

	// Veyra.UI.KitPresentationWorld.*: the kit presentation drawing from two Vanguards' status ledgers in a world, with
	// Patch's committed tuning and presentation settings (ADR-071 §2-§3).
	TEST_CLASS(KitPresentationWorld, "Veyra.UI")
	{
		static constexpr double Apart = 300.0;
		static constexpr double LongSeconds = 60.0;
		static constexpr float Step = 0.05f;
		static constexpr double Slack = 1e-6;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Patch = nullptr;
		AVeyraVanguardCharacter* Enemy = nullptr;
		AVeyraVanguardCharacter* Ally = nullptr;
		UVeyraKitPresentationSubsystem* Kit = nullptr;

		BEFORE_EACH()
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			Patch = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Enemy = &World.Spawn(EVeyraTeam::B, FVector(Apart, 0.0, 0.0));
			Ally = &World.Spawn(EVeyraTeam::A, FVector(-Apart, 0.0, 0.0));
			Kit =Spawner.GetWorld().GetSubsystem<UVeyraKitPresentationSubsystem>();
			ASSERT_THAT(IsNotNull(Kit));
		}

		/** Puts status Name on To from From, for LongSeconds; its kind does not matter to the presentation. */
		static void Apply(AVeyraVanguardCharacter& From, AVeyraVanguardCharacter& To, const TCHAR* Name)
		{
			FVeyraStatusSpec Spec;
			Spec.Id = FVeyraContentId::FromText(Name).GetValue();
			Spec.Kind = EVeyraStatusKind::Counter;
			Spec.DurationSeconds = LongSeconds;
			VeyraCombat::ApplyStatus(*From.GetAbilitySystemComponent(), *To.GetAbilitySystemComponent(), Spec);
		}

		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, Step);
				++GFrameCounter;
				World.GetTimerManager().Tick(Step);
			}
		}

		TEST_METHOD(AThreadedUnitIsJoinedToTheBodyHoldingTheOtherEnd)
		{
			Apply(*Patch, *Enemy, TEXT("patch_threaded"));
			Kit->Refresh();
			const TArray<UVeyraKitPresentationSubsystem::FStrandShown>& Strands = Kit->GetStrands();
			ASSERT_THAT(AreEqual(1, Strands.Num()));
			ASSERT_THAT(IsTrue(Strands[0].Source.Get() == Patch && Strands[0].Holder.Get() == Enemy, TEXT("from Patch to the unit he threaded")));
		}

		TEST_METHOD(TheThingInsideRingsHimAtItsAuraAndItsShoveSpreadsAfter)
		{
			// The ability's own statuses and reach, as Abilities.json has them.
			const FVeyraSelfBuffAbilityTuning& Inside = UVeyraAbilitiesTuningSubsystem::Get().SelfBuff.FindChecked(
				FVeyraContentId::FromText(TEXT("patch_the_thing_inside")).GetValue());
			Apply(*Patch, *Patch, *Inside.Statuses[0].ToString());
			Kit->Refresh();
			const auto IsAura = [Patch = Patch, &Inside](const UVeyraKitPresentationSubsystem::FRingShown& Ring) {
				return !Ring.bBurst && Ring.Holder.Get() == Patch && FMath::IsNearlyEqual(Ring.Radius, Inside.Aura[0].Radius, Slack);
			};
			ASSERT_THAT(IsTrue(Kit->GetRings().ContainsByPredicate(IsAura), TEXT("its aura's ring, at its reach")));
			Wait(Inside.EndPayload[0].AfterSeconds + Step);
			Kit->Refresh();
			ASSERT_THAT(IsTrue(Kit->GetRings().ContainsByPredicate([Patch = Patch, &Inside](const UVeyraKitPresentationSubsystem::FRingShown& Ring) {
				return Ring.bBurst && Ring.Holder.Get() == Patch && FMath::IsNearlyEqual(Ring.Radius, Inside.EndPayload[0].Radius, Slack);
			}), TEXT("its shove spreads as it comes")));
		}

		TEST_METHOD(ABuffCastOnAnAllyRingsTheAllyAndItsPayloadComesFromItsCaster)
		{
			// An aura follows the buff's holder, an ally it was cast on; a payload comes from the caster's own body.
			const FVeyraSelfBuffAbilityTuning& Inside = UVeyraAbilitiesTuningSubsystem::Get().SelfBuff.FindChecked(
				FVeyraContentId::FromText(TEXT("patch_the_thing_inside")).GetValue());
			Apply(*Patch, *Ally, *Inside.Statuses[0].ToString());
			Kit->Refresh();
			ASSERT_THAT(IsTrue(Kit->GetRings().ContainsByPredicate([Ally = Ally](const UVeyraKitPresentationSubsystem::FRingShown& Ring) {
				return !Ring.bBurst && Ring.Holder.Get() == Ally;
			}), TEXT("the aura rings the ally holding it")));
			Wait(Inside.EndPayload[0].AfterSeconds + Step);
			Kit->Refresh();
			ASSERT_THAT(IsTrue(Kit->GetRings().ContainsByPredicate([Patch = Patch](const UVeyraKitPresentationSubsystem::FRingShown& Ring) {
				return Ring.bBurst && Ring.Holder.Get() == Patch;
			}), TEXT("the payload spreads from its caster")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
