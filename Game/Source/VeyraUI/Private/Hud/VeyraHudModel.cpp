// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Hud/VeyraHudModel.h"

#include "Entities/VeyraPlacedMarker.h"
#include "GameFramework/PlayerState.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attributes/VeyraResourceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Echoes/VeyraEcho.h"
#include "EngineUtils.h"
#include "Gold/VeyraGoldComponent.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Ledger/VeyraFluxLedger.h"
#include "Life/VeyraLifeComponent.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Progression/VeyraProgressionRules.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "Recall/VeyraRecallComponent.h"
#include "State/VeyraTeamFluxState.h"
#include "State/VeyraVisionTeamState.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Structures/VeyraStructure.h"
#include "Tools/VeyraVisionToolComponent.h"
#include "Wildlife/VeyraWildlife.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraFluxTuningSubsystem.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "Tuning/VeyraVisionTuningSubsystem.h"
#include "VeyraPlayerState.h"

namespace
{
	/** A component beside Unit's Ability System Component: on a Vanguard, its participant's PlayerState. */
	template <typename ComponentType>
	const ComponentType* FindBesideHudAbilitySystem(const AActor& Unit)
	{
		const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
		const AActor* Owner = AbilitySystem ? AbilitySystem->GetOwner() : nullptr;
		return Owner ? Owner->FindComponentByClass<ComponentType>() : nullptr;
	}
}

const AActor& VeyraHud::PresentedUnitOf(const AActor& Unit, EVeyraTeam Viewer)
{
	const AVeyraPlacedMarker* Marker = Cast<AVeyraPlacedMarker>(&Unit);
	const APlayerState* Owner = Marker ? Marker->GetPresentedAs() : nullptr;
	// Only its owner's enemies are deceived; its owner's side sees its owner's illusion (ADR-030 §5).
	const bool bDeceives = Owner && Viewer != EVeyraTeam::None && Viewer != Marker->GetVeyraTeam();
	return bDeceives ? static_cast<const AActor&>(*Owner) : Unit;
}

TOptional<FVeyraHudVitals> VeyraHud::VitalsOf(const AActor& Unit, EVeyraTeam Viewer)
{
	const AActor& Shown = PresentedUnitOf(Unit, Viewer);
	const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Shown);
	if (!AbilitySystem || !AbilitySystem->HasAttributeSetForAttribute(UVeyraVitalsSet::GetHealthAttribute()))
	{
		return {};
	}
	FVeyraHudVitals Vitals;
	Vitals.Health = AbilitySystem->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
	Vitals.MaxHealth = AbilitySystem->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
	if (AbilitySystem->HasAttributeSetForAttribute(UVeyraResourceSet::GetResourceAttribute()))
	{
		Vitals.Resource = AbilitySystem->GetNumericAttribute(UVeyraResourceSet::GetResourceAttribute());
		Vitals.MaxResource = AbilitySystem->GetNumericAttribute(UVeyraResourceSet::GetMaxResourceAttribute());
		const AVeyraPlayerState* Participant = Cast<AVeyraPlayerState>(AbilitySystem->GetOwner());
		const FVeyraVanguardDefinition* Definition = Participant ? UVeyraVanguardsTuningSubsystem::FindVanguard(Participant->GetVanguardId()) : nullptr;
		Vitals.Family = Definition ? Definition->Resource : EVeyraResourceFamily::Mana;
	}
	if (const UVeyraDamageAbsorptionComponent* Absorption = FindBesideHudAbilitySystem<UVeyraDamageAbsorptionComponent>(Shown))
	{
		for (const FVeyraShieldEntry& Shield : Absorption->GetLedger().Shields)
		{
			Vitals.Shield += Shield.Remaining;
		}
	}
	return Vitals;
}

TArray<FVeyraHudStatus> VeyraHud::StatusesOf(const AActor& Unit, double ServerNow, EVeyraTeam Viewer)
{
	TArray<FVeyraHudStatus> Statuses;
	// A decoy shows its owner's statuses to its owner's enemies, but never the stealth its owner hides in.
	const AActor& Shown = PresentedUnitOf(Unit, Viewer);
	const bool bDecoy = &Shown != &Unit;
	if (const UVeyraStatusComponent* Ledger = FindBesideHudAbilitySystem<UVeyraStatusComponent>(Shown))
	{
		for (const FVeyraStatusEntry& Entry : Ledger->GetLedger().Entries)
		{
			if (bDecoy && (Entry.Kind == EVeyraStatusKind::Invisible || Entry.Kind == EVeyraStatusKind::Camouflage))
			{
				continue;
			}
			Statuses.Add(FVeyraHudStatus{ Entry.Id, Entry.Kind, FMath::Max(0.0, Entry.EndsAt - ServerNow), Entry.Stacks });
		}
	}
	return Statuses;
}

TOptional<FVeyraHudStructure> VeyraHud::StructureOf(const AActor& Unit, double ServerNow)
{
	const AVeyraStructure* Structure = Cast<AVeyraStructure>(&Unit);
	if (!Structure)
	{
		return {};
	}
	FVeyraHudStructure Shown;
	Shown.Kind = Structure->GetStructureKind();
	Shown.bInvulnerable = Structure->IsInvulnerable();
	if (Structure->GetRebuildsAt() > 0.0)
	{
		Shown.RebuildSeconds = FMath::Max(0.0, Structure->GetRebuildsAt() - ServerNow);
	}
	return Shown;
}

TOptional<FVeyraHudFluxWell> VeyraHud::FluxWellOf(const AActor& Unit, double ServerNow)
{
	const AVeyraFluxWell* Well = Cast<AVeyraFluxWell>(&Unit);
	if (!Well)
	{
		return {};
	}
	FVeyraHudFluxWell Shown;
	Shown.State = Well->GetState();
	if (Well->GetState() != EVeyraFluxWellState::Open)
	{
		Shown.OpensInSeconds = FMath::Max(0.0, Well->GetOpensAt() - ServerNow);
	}
	return Shown;
}

TOptional<FVeyraContentId> VeyraHud::SpeciesOf(const AActor& Unit)
{
	const AVeyraWildlife* Creature = Cast<AVeyraWildlife>(&Unit);
	return Creature && Creature->GetSpecies().IsValid() ? TOptional<FVeyraContentId>(Creature->GetSpecies()) : TOptional<FVeyraContentId>();
}

TOptional<double> VeyraHud::AttackReachOf(const AActor& Unit)
{
	const UVeyraBasicAttackComponent* Attacks = FindBesideHudAbilitySystem<UVeyraBasicAttackComponent>(Unit);
	if (!Attacks)
	{
		return {};
	}
	// Reach runs edge to edge (Combat Bible §40), so a target whose edge touches the ring is in reach.
	return Attacks->GetRange(nullptr) + Unit.GetSimpleCollisionRadius();
}

bool VeyraHud::IsFighting(const AActor& Unit)
{
	const UVeyraBasicAttackComponent* Attacks = FindBesideHudAbilitySystem<UVeyraBasicAttackComponent>(Unit);
	return Attacks && Attacks->GetState().Phase != EVeyraAttackPhase::None;
}

TOptional<FVeyraHudMasteryEmote> VeyraHud::MasteryEmoteOf(const AActor& Unit, double ServerNow)
{
	const APawn* Pawn = Cast<APawn>(&Unit);
	const AVeyraPlayerState* Participant = Pawn ? Pawn->GetPlayerState<AVeyraPlayerState>() : nullptr;
	if (!Participant || Participant->GetMasteryLevel() <= 0 || !(ServerNow < Participant->GetMasteryEmoteUntil()))
	{
		return {};
	}
	return FVeyraHudMasteryEmote{ Participant->GetMasteryLevel(), Participant->GetEmoteTier() };
}

FVeyraHudPlayer VeyraHud::DescribePlayer(const AVeyraPlayerState& Participant, double ServerNow)
{
	FVeyraHudPlayer Player;
	Player.Vanguard = Participant.GetVanguardId();
	if (const APawn* Vanguard = Participant.GetPawn())
	{
		Player.Vitals = VitalsOf(*Vanguard, VeyraTeams::TeamOf(&Participant)).Get(FVeyraHudVitals());
	}

	const FVeyraProgressionTuning& Tuning = UVeyraProgressionTuningSubsystem::Get();
	const UVeyraProgressionComponent* Progression = Participant.FindComponentByClass<UVeyraProgressionComponent>();
	if (Progression && Progression->IsInitialized())
	{
		Player.Level = Progression->GetLevel();
		// Rounded down for display only: it never shows a level's XP before it is earned.
		Player.Experience = FMath::FloorToInt32(Progression->GetExperience());
		Player.ExperienceToNextLevel = VeyraProgression::ExperienceToNextLevel(Player.Level, Tuning);
		Player.UnspentSkillPoints = Progression->GetUnspentSkillPoints();
	}
	if (const UVeyraGoldComponent* Gold = Participant.FindComponentByClass<UVeyraGoldComponent>())
	{
		Player.Gold = FMath::FloorToInt32(Gold->GetGold());
	}
	if (const UVeyraLifeComponent* Life = Participant.FindComponentByClass<UVeyraLifeComponent>(); Life && !Life->IsAlive())
	{
		Player.bDead = true;
		Player.RespawnSeconds = FMath::Max(0.0, Participant.GetRespawnAt() - ServerNow);
	}
	if (const UVeyraRecallComponent* Recall = Participant.FindComponentByClass<UVeyraRecallComponent>(); Recall && Recall->IsRecalling())
	{
		const FVeyraRecallChannel& Channel = Recall->GetChannel();
		const double Length = Channel.EndsAt - Channel.StartedAt;
		Player.bRecalling = true;
		Player.RecallSeconds = FMath::Max(0.0, Channel.EndsAt - ServerNow);
		Player.RecallProgress = Length > 0.0 ? FMath::Clamp((ServerNow - Channel.StartedAt) / Length, 0.0, 1.0) : 1.0;
	}

	// Its projected Echo, which it commands while it stands (ADR-050 §7).
	for (TActorIterator<AVeyraEcho> It(Participant.GetWorld()); It; ++It)
	{
		const AVeyraEcho& Echo = **It;
		const FVeyraEchoAbilityTuning* EchoTuning = UVeyraAbilitiesTuningSubsystem::FindEcho(Echo.GetAbility());
		if (Echo.GetHolderState() != &Participant || Echo.IsWithdrawn() || !EchoTuning || EchoTuning->Projection.IsEmpty())
		{
			continue;
		}
		const UAbilitySystemComponent* Abilities = Echo.GetAbilitySystemComponent();
		const double Max = Abilities ? Abilities->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) : 0.0;
		FVeyraHudEcho& Shown = Player.Echo.Emplace();
		Shown.IntegrityShare = Max > 0.0 ? FMath::Clamp(Abilities->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute()) / Max, 0.0, 1.0) : 0.0;
		Shown.FormingSeconds = FMath::Max(0.0, Echo.GetControlAt() - ServerNow);
		Shown.ImmuneSeconds = FMath::Max(0.0, Echo.GetImmuneUntil() - ServerNow);
		Shown.RepeatsLeft = Echo.GetRepeatsLeft();
		Shown.Slots = EchoTuning->Slots;
		break;
	}

	if (const FVeyraVanguardDefinition* Definition = UVeyraVanguardsTuningSubsystem::FindVanguard(Player.Vanguard); Definition && !Definition->Passive.IsEmpty())
	{
		Player.Passive = Definition->Passive[0];
	}

	const UVeyraAbilityLoadoutComponent* Loadout = Participant.FindComponentByClass<UVeyraAbilityLoadoutComponent>();
	const UVeyraCooldownComponent* Cooldowns = Participant.FindComponentByClass<UVeyraCooldownComponent>();
	const UVeyraBasicAttackComponent* Attacks = Participant.FindComponentByClass<UVeyraBasicAttackComponent>();
	for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::All)
	{
		FVeyraHudSlot& Shown = Player.Slots.AddDefaulted_GetRef();
		Shown.Slot = Slot;
		// The unit's own rank shape, once it has one (ADR-031 §2).
		Shown.MaxRank = Progression && Progression->IsInitialized() ? Progression->GetMaxRank(Slot) : VeyraProgression::MaxRank(Slot, Tuning);
		if (const FVeyraLoadoutEntry* Entry = Loadout ? Loadout->FindSlot(Slot) : nullptr)
		{
			Shown.Ability = Entry->Ability;
			Shown.CooldownSeconds = Cooldowns ? Cooldowns->GetRemainingSeconds(Loadout->CooldownIdOf(Entry->Ability), ServerNow) : 0.0;
			if (Attacks && Attacks->GetEmpowermentView().Ability == Entry->Ability)
			{
				Shown.EmpoweredSeconds = FMath::Max(0.0, Attacks->GetEmpowermentView().ExpiresAt - ServerNow);
			}
		}
		if (const FVeyraLoadoutEntry* Own = Loadout ? Loadout->FindOwnSlot(Slot) : nullptr)
		{
			Shown.OwnAbility = Own->Ability;
		}
		if (Progression && Progression->IsInitialized())
		{
			Shown.Rank = Progression->GetRank(Slot);
			Shown.bCanRankUp = VeyraProgression::CheckRankUp(Slot, Shown.Rank, Player.Level, Player.UnspentSkillPoints, Tuning) == EVeyraRankRefusal::None;
		}
	}
	if (const UVeyraInventoryComponent* Inventory = Participant.FindComponentByClass<UVeyraInventoryComponent>())
	{
		const TArray<FVeyraInventorySlot>& Held = Inventory->GetSlots();
		for (int32 Index = 0; Index < Held.Num() && Index < static_cast<int32>(UE_ARRAY_COUNT(VeyraAbilitySlots::Items)); ++Index)
		{
			FVeyraHudItemSlot& Shown = Player.Items.AddDefaulted_GetRef();
			Shown.Slot = VeyraAbilitySlots::Items[Index];
			if (Held[Index].IsEmpty())
			{
				continue;
			}
			Shown.Item = Held[Index].Item;
			Shown.Count = Held[Index].Count;
			if (const FVeyraConsumableTuning* Consumable = UVeyraItemsTuningSubsystem::Get().Consumables.Find(Held[Index].Item); Consumable && Consumable->Charges > 0)
			{
				Shown.Charges = Held[Index].Charges;
			}
			if (const FVeyraQuestTuning* Quest = UVeyraItemsTuningSubsystem::Get().Quests.Find(Held[Index].Item))
			{
				Shown.Quest = FIntPoint(Held[Index].QuestProgress, Quest->Threshold);
			}
			if (VeyraInventory::Stores(UVeyraItemsTuningSubsystem::Get(), Held[Index].Item, EVeyraItemStore::Current))
			{
				Shown.Current = FMath::FloorToInt32(Held[Index].Current);
			}
			if (VeyraInventory::Stores(UVeyraItemsTuningSubsystem::Get(), Held[Index].Item, EVeyraItemStore::Reserve))
			{
				Shown.Reserve = FMath::FloorToInt32(Held[Index].Reserve);
			}
			// An item's Active sits in its slot's loadout entry, and cools down under its own ID.
			if (const FVeyraLoadoutEntry* Entry = Loadout ? Loadout->FindSlot(Shown.Slot) : nullptr; Entry && Cooldowns)
			{
				Shown.CooldownSeconds = Cooldowns->GetRemainingSeconds(Entry->Ability, ServerNow);
			}
		}
		Player.PendingPurchases = Inventory->GetQueue().Num();
	}
	if (Loadout)
	{
		const TArray<double>& Thresholds = UVeyraFluxTuningSubsystem::Get().SpellSlots.Thresholds;
		for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(VeyraAbilitySlots::Spells)); ++Index)
		{
			FVeyraHudSpellSlot& Shown = Player.Spells.AddDefaulted_GetRef();
			Shown.Slot = VeyraAbilitySlots::Spells[Index];
			Shown.bLocked = Loadout->IsLocked(Shown.Slot);
			Shown.UnlockFlux = Thresholds.IsValidIndex(Index) ? Thresholds[Index] : 0.0;
			if (const FVeyraLoadoutEntry* Entry = Loadout->FindSlot(Shown.Slot))
			{
				Shown.Spell = Entry->Ability;
				Shown.CooldownSeconds = Cooldowns ? Cooldowns->GetRemainingSeconds(Entry->Ability, ServerNow) : 0.0;
			}
		}
	}
	if (const UVeyraVisionToolComponent* Tool = Participant.FindComponentByClass<UVeyraVisionToolComponent>())
	{
		Player.VisionTool.bPresent = true;
		Player.VisionTool.Tool = Tool->GetEquipped();
		if (Tool->GetEquipped() != EVeyraVisionTool::PersistentWard)
		{
			Player.VisionTool.CooldownSeconds = FMath::Max(0.0, Tool->GetReadyAt(Tool->GetEquipped()) - ServerNow);
		}
		Player.VisionTool.WardCharges = Tool->GetWardCharges();
		Player.VisionTool.MaxWardCharges = UVeyraVisionTuningSubsystem::Get().WardCharges.Max;
		Player.VisionTool.NextChargeSeconds = Tool->GetNextChargeAt() < 0.0 ? 0.0 : FMath::Max(0.0, Tool->GetNextChargeAt() - ServerNow);
	}
	return Player;
}

FVeyraHudVision VeyraHud::DescribeVision(const UWorld* World, EVeyraTeam Viewer, double ServerNow)
{
	FVeyraHudVision Vision;
	const AVeyraVisionTeamState* State = AVeyraVisionTeamState::Find(World, Viewer);
	if (!State)
	{
		return Vision;
	}
	const double Cadence = UVeyraVisionTuningSubsystem::Get().Presence.PingEverySeconds;
	for (const FVeyraPresencePing& Ping : State->GetPings())
	{
		const double Fade = Cadence > 0.0 ? 1.0 - (ServerNow - Ping.At) / Cadence : 0.0;
		if (Fade > 0.0)
		{
			Vision.Pings.Add(FVeyraHudPing{ Ping.Centre, Ping.Radius, FMath::Min(Fade, 1.0) });
		}
	}
	for (const FVeyraOutline& Outline : State->GetOutlines())
	{
		if (Outline.Until > ServerNow)
		{
			Vision.Outlines.Add(Outline.Location);
		}
	}
	return Vision;
}

TArray<FVeyraHudTeamFlux> VeyraHud::DescribeTeamFlux(const UWorld* World, double ServerNow)
{
	TArray<FVeyraHudTeamFlux> Teams;
	const AVeyraTeamFluxState* State = AVeyraTeamFluxState::Find(World);
	if (!State)
	{
		return Teams;
	}
	for (const FVeyraTeamFluxView& View : State->GetTeams())
	{
		FVeyraHudTeamFlux& Shown = Teams.AddDefaulted_GetRef();
		Shown.Team = View.Team;
		Shown.Active = View.ActiveAt(ServerNow);
		Shown.Permanent = View.Permanent;
		// Flux's own rule says what the Flux gives; the HUD only shows it.
		Shown.FluxbornBonus = VeyraFlux::StrengthFor(Shown.Active, UVeyraFluxTuningSubsystem::Get().FluxbornScaling).HealthMultiplier - 1.0;
		for (const FVeyraTemporaryFluxView& Grant : View.Temporary)
		{
			if (Grant.ExpiresAt > ServerNow)
			{
				Shown.TemporarySeconds.Add(Grant.ExpiresAt - ServerNow);
			}
		}
		Shown.TemporarySeconds.Sort();
	}
	return Teams;
}
