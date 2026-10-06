// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Hud/VeyraTargetFrameModel.h"

#include "Companions/VeyraCompanion.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Entities/VeyraPlacedMarker.h"
#include "Fluxborn/VeyraFluxborn.h"
#include "GameFramework/Pawn.h"
#include "Hud/VeyraKillFeedModel.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Slots/VeyraAbilitySlot.h"
#include "Structures/VeyraStructure.h"
#include "Targeting/VeyraTargeting.h"
#include "Text/VeyraContentText.h"
#include "VeyraPlayerState.h"
#include "Wildlife/VeyraWildlife.h"

namespace VeyraTargetFrame
{
namespace
{
	/** The participant behind a Vanguard, or a participant itself; null for any other unit. */
	const AVeyraPlayerState* ParticipantOf(const AActor& Shown)
	{
		if (const AVeyraPlayerState* Participant = Cast<AVeyraPlayerState>(&Shown))
		{
			return Participant;
		}
		const APawn* Body = Cast<APawn>(&Shown);
		return Body ? Body->GetPlayerState<AVeyraPlayerState>() : nullptr;
	}

	/** A Vanguard's own part of the frame: its face, name, player, Level, items and Flux Spells (ADR-066 §3–§4). */
	void DescribeVanguard(const AVeyraPlayerState& Participant, double ServerNow, FVeyraTargetFrame& Frame)
	{
		Frame.Vanguard = Participant.GetVanguardId();
		Frame.Name = Frame.Vanguard.IsValid() ? VeyraContentText::VanguardName(Frame.Vanguard).ToString() : Participant.GetPlayerName();
		Frame.Detail = Participant.GetPlayerName();
		if (const UVeyraProgressionComponent* Progression = Participant.FindComponentByClass<UVeyraProgressionComponent>())
		{
			Frame.Level = Progression->GetLevel();
		}
		if (const UVeyraInventoryComponent* Inventory = Participant.FindComponentByClass<UVeyraInventoryComponent>())
		{
			for (const FVeyraInventorySlot& Slot : Inventory->GetSlots())
			{
				Frame.Items.Add(Slot.IsEmpty() ? FVeyraContentId() : Slot.Item);
			}
		}
		const UVeyraAbilityLoadoutComponent* Loadout = Participant.FindComponentByClass<UVeyraAbilityLoadoutComponent>();
		const UVeyraCooldownComponent* Cooldowns = Participant.FindComponentByClass<UVeyraCooldownComponent>();
		if (!Loadout)
		{
			return;
		}
		const TArray<FVeyraContentId>& Shared = Loadout->GetSharedSpells();
		for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::Spells)
		{
			const int32 Index = VeyraAbilitySlots::SpellIndexOf(Slot);
			FVeyraTargetFrameSpell& Spell = Frame.Spells.AddDefaulted_GetRef();
			Spell.Spell = Shared.IsValidIndex(Index) ? Shared[Index] : FVeyraContentId();
			Spell.bLocked = Loadout->IsLocked(Slot);
			if (Cooldowns && Spell.Spell.IsValid())
			{
				Spell.CooldownSeconds = Cooldowns->GetSharedRemainingSeconds(Spell.Spell, ServerNow);
				Spell.CooldownTotal = Spell.CooldownSeconds > 0.0 ? Cooldowns->GetSharedDurationSeconds(Spell.Spell) : 0.0;
			}
		}
	}

	/** Any other unit's name: a structure's place, a Fluxborn's, creature's or companion's kind, else its kind of unit. */
	FString NameOf(const AActor& Shown, EVeyraUnitKind Kind)
	{
		if (const AVeyraStructure* Structure = Cast<AVeyraStructure>(&Shown))
		{
			return VeyraKillFeedView::StructureName(Structure->GetStructureKind(), Structure->GetLane(), Structure->GetOrder());
		}
		if (const AVeyraFluxborn* Fluxborn = Cast<AVeyraFluxborn>(&Shown); Fluxborn && Fluxborn->GetKind().IsValid())
		{
			return Words(Fluxborn->GetKind().ToString());
		}
		if (const AVeyraWildlife* Creature = Cast<AVeyraWildlife>(&Shown); Creature && Creature->GetSpecies().IsValid())
		{
			return Words(Creature->GetSpecies().ToString());
		}
		if (const AVeyraCompanion* Companion = Cast<AVeyraCompanion>(&Shown); Companion && Companion->GetDefinitionId().IsValid())
		{
			return Words(Companion->GetDefinitionId().ToString());
		}
		return UEnum::GetDisplayValueAsText(Kind).ToString();
	}
}

TOptional<FVeyraTargetFrame> Describe(const AActor& Unit, EVeyraTeam Viewer, double ServerNow)
{
	// A wall's marker is drawn as the terrain it holds, and shows nothing of its own (ADR-032 §4).
	const AVeyraPlacedMarker* Marker = Cast<AVeyraPlacedMarker>(&Unit);
	const AActor& Shown = VeyraHud::PresentedUnitOf(Unit, Viewer);
	const TOptional<EVeyraUnitKind> Kind = VeyraUnits::KindOf(&Shown);
	const TOptional<FVeyraHudVitals> Vitals = VeyraHud::VitalsOf(Unit, Viewer);
	if (!Kind || !Vitals || (Marker && Marker->IsWall()))
	{
		return {};
	}
	FVeyraTargetFrame Frame;
	Frame.Kind = Kind.GetValue();
	Frame.Vitals = Vitals.GetValue();
	Frame.Side = VeyraTeams::TeamOf(&Shown);
	Frame.bAlive = VeyraTargeting::IsAlive(&Shown);
	const AVeyraPlayerState* Participant = Frame.Kind == EVeyraUnitKind::Vanguard ? ParticipantOf(Shown) : nullptr;
	if (Participant)
	{
		DescribeVanguard(*Participant, ServerNow, Frame);
	}
	else
	{
		Frame.Name = NameOf(Shown, Frame.Kind);
	}
	return Frame;
}

FString Words(const FString& Id)
{
	TArray<FString> Parts;
	Id.ParseIntoArray(Parts, TEXT("_"));
	for (FString& Part : Parts)
	{
		Part = Part.Left(1).ToUpper() + Part.Mid(1);
	}
	return FString::Join(Parts, TEXT(" "));
}
}
