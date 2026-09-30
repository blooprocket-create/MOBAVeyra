// Copyright © 2026 Wayfinder Studios. All rights reserved.

// The developer commands that act on the typist's own Vanguard: its Health, its life, its
// cooldowns and where it stands. Each goes through the owning system's verbs.

#include "DevCommands/VeyraDevCommands.h"

#include "AbilitySystemComponent.h"
#include "Absorption/VeyraAbsorptionLedger.h"
#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attributes/VeyraResourceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Damage/VeyraDamageTypes.h"
#include "Engine/World.h"
#include "Misc/OutputDevice.h"
#include "NavigationSystem.h"
#include "Recall/VeyraRecallComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Tools/VeyraVisionToolComponent.h"
#include "UObject/ObjectKey.h"
#include "VeyraCombatVerbs.h"
#include "VeyraGameMode.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardCharacter.h"
#include "VeyraVanguardController.h"

namespace VeyraDevVanguardCommands
{
	const TCHAR* const Category = TEXT("Vanguard");

	const TCHAR* const NoVanguard = TEXT("You have no Vanguard in this match yet.");
	const TCHAR* const DeadVanguard = TEXT("Your Vanguard is dead; Veyra.Dev.Respawn brings it back.");

	/** The Vanguards God made invulnerable, each holding one of Combat's counted grants. */
	TSet<TObjectKey<UAbilitySystemComponent>>& GodGrants()
	{
		static TSet<TObjectKey<UAbilitySystemComponent>> Grants;
		return Grants;
	}

	double HealthOf(const UAbilitySystemComponent& AbilitySystem)
	{
		return AbilitySystem.GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
	}

	/** Deals Amount of true damage to the Vanguard from itself, through Combat's whole pipeline. */
	void HitSelf(UAbilitySystemComponent& AbilitySystem, double Amount)
	{
		FVeyraRawDamageEvent Damage;
		Damage.Components.Add({ EVeyraDamageType::TrueDamage, Amount });
		Damage.Delivery = EVeyraDamageDelivery::Developer;
		VeyraCombat::DealDamage(AbilitySystem, AbilitySystem, Damage);
	}

	FString RunHeal(AVeyraPlayerController& Requester, TConstArrayView<FString> /*Args*/)
	{
		UAbilitySystemComponent* AbilitySystem = VeyraDevCommands::AbilitySystemOf(Requester);
		if (!AbilitySystem)
		{
			return NoVanguard;
		}
		if (!VeyraTargeting::IsAlive(VeyraDevCommands::ParticipantOf(Requester)))
		{
			return DeadVanguard;
		}
		const double MissingHealth = VeyraCombat::GetMissingHealth(*AbilitySystem);
		if (MissingHealth > 0.0)
		{
			VeyraCombat::RestoreHealth(*AbilitySystem, MissingHealth);
		}
		// A Vanguard without a resource has no resource set.
		double MissingResource = 0.0;
		if (AbilitySystem->HasAttributeSetForAttribute(UVeyraResourceSet::GetMaxResourceAttribute()))
		{
			MissingResource = FMath::Max(0.0, AbilitySystem->GetNumericAttribute(UVeyraResourceSet::GetMaxResourceAttribute())
					- AbilitySystem->GetNumericAttribute(UVeyraResourceSet::GetResourceAttribute()));
			if (MissingResource > 0.0)
			{
				VeyraCombat::RestoreResource(*AbilitySystem, MissingResource);
			}
		}
		return FString::Printf(TEXT("Restored %.0f Health and %.0f resource."), MissingHealth, MissingResource);
	}

	FString RunGod(AVeyraPlayerController& Requester, TConstArrayView<FString> Args)
	{
		UAbilitySystemComponent* AbilitySystem = VeyraDevCommands::AbilitySystemOf(Requester);
		if (!AbilitySystem)
		{
			return NoVanguard;
		}
		const bool bOn = GodGrants().Contains(TObjectKey<UAbilitySystemComponent>(AbilitySystem));
		const TOptional<bool> bWanted = VeyraDevCommands::ParseSwitch(Args, bOn);
		if (!bWanted.IsSet())
		{
			return VeyraDevCommands::UsageReply(TEXT("God"));
		}
		if (bWanted.GetValue() && !bOn)
		{
			VeyraCombat::GrantInvulnerability(*AbilitySystem);
			GodGrants().Add(TObjectKey<UAbilitySystemComponent>(AbilitySystem));
		}
		else if (!bWanted.GetValue() && bOn)
		{
			VeyraCombat::RevokeInvulnerability(*AbilitySystem);
			GodGrants().Remove(TObjectKey<UAbilitySystemComponent>(AbilitySystem));
		}
		return bWanted.GetValue() ? TEXT("God is on: your Vanguard takes no damage, through death and respawn. Crowd control still lands.")
								  : TEXT("God is off.");
	}

	FString RunDamage(AVeyraPlayerController& Requester, TConstArrayView<FString> Args)
	{
		const TOptional<double> Amount = VeyraDevCommands::ParseNumber(Args, 0);
		if (!Amount.IsSet() || Amount.GetValue() <= 0.0 || Args.Num() != 1)
		{
			return VeyraDevCommands::UsageReply(TEXT("Damage"));
		}
		UAbilitySystemComponent* AbilitySystem = VeyraDevCommands::AbilitySystemOf(Requester);
		if (!AbilitySystem)
		{
			return NoVanguard;
		}
		if (!VeyraTargeting::IsAlive(VeyraDevCommands::ParticipantOf(Requester)))
		{
			return DeadVanguard;
		}
		const double Before = HealthOf(*AbilitySystem);
		HitSelf(*AbilitySystem, Amount.GetValue());
		return FString::Printf(TEXT("Hit your Vanguard for %.0f true damage: Health %.0f -> %.0f."), Amount.GetValue(), Before, HealthOf(*AbilitySystem));
	}

	FString RunKill(AVeyraPlayerController& Requester, TConstArrayView<FString> /*Args*/)
	{
		AVeyraPlayerState* Participant = VeyraDevCommands::ParticipantOf(Requester);
		UAbilitySystemComponent* AbilitySystem = VeyraDevCommands::AbilitySystemOf(Requester);
		if (!Participant || !AbilitySystem)
		{
			return NoVanguard;
		}
		if (!VeyraTargeting::IsAlive(Participant))
		{
			return TEXT("Your Vanguard is dead already.");
		}
		if (VeyraCombat::IsInvulnerable(*AbilitySystem))
		{
			return TEXT("Your Vanguard is invulnerable now; if God made it so, Veyra.Dev.God off first.");
		}
		// Enough to go through every shield and all Temporary Health (Combat Bible §25 steps 7–9).
		double Lethal = HealthOf(*AbilitySystem);
		if (const UVeyraDamageAbsorptionComponent* Absorption = Participant->FindComponentByClass<UVeyraDamageAbsorptionComponent>())
		{
			for (const FVeyraShieldEntry& Shield : Absorption->GetLedger().Shields)
			{
				Lethal += Shield.Remaining;
			}
			for (const FVeyraTemporaryHealthGrant& Grant : Absorption->GetLedger().TemporaryHealth)
			{
				Lethal += Grant.Remaining;
			}
		}
		HitSelf(*AbilitySystem, Lethal);
		return VeyraTargeting::IsAlive(Participant)
			? FString::Printf(TEXT("Your Vanguard survived with %.0f Health: something reduced the hit."), HealthOf(*AbilitySystem))
			: FString(TEXT("Your Vanguard died."));
	}

	FString RunRespawn(AVeyraPlayerController& Requester, TConstArrayView<FString> /*Args*/)
	{
		AVeyraPlayerState* Participant = VeyraDevCommands::ParticipantOf(Requester);
		if (!Participant)
		{
			return NoVanguard;
		}
		if (VeyraTargeting::IsAlive(Participant))
		{
			return TEXT("Your Vanguard is alive.");
		}
		AVeyraGameMode* GameMode = Requester.GetWorld()->GetAuthGameMode<AVeyraGameMode>();
		return GameMode && GameMode->HandleDeveloperRespawn(Requester) ? TEXT("Your Vanguard respawned at its fountain.")
																		 : TEXT("No respawn: the match must be live and not paused.");
	}

	FString RunResetCooldowns(AVeyraPlayerController& Requester, TConstArrayView<FString> /*Args*/)
	{
		AVeyraPlayerState* Participant = VeyraDevCommands::ParticipantOf(Requester);
		UVeyraCooldownComponent* Cooldowns = Participant ? Participant->FindComponentByClass<UVeyraCooldownComponent>() : nullptr;
		if (!Cooldowns)
		{
			return NoVanguard;
		}
		const int32 Cleared = Cooldowns->ClearAllCooldowns();
		if (UVeyraVisionToolComponent* Tool = Participant->FindComponentByClass<UVeyraVisionToolComponent>())
		{
			Tool->RefillWardCharges();
		}
		return FString::Printf(TEXT("%d cooldown(s) cleared, and your ward charges refilled."), Cleared);
	}

	/** Without coordinates, the teleport goes where the cursor points. */
	TOptional<FString> AimTeleport(AVeyraPlayerController& Typist, TArray<FString>& Args)
	{
		if (!Args.IsEmpty())
		{
			return {};
		}
		FHitResult Ground;
		if (!Typist.GetHitResultUnderCursor(ECC_Visibility, /*bTraceComplex*/ false, Ground))
		{
			return FString(TEXT("Point the cursor at the ground, or give coordinates. ")) + VeyraDevCommands::UsageReply(TEXT("Teleport"));
		}
		Args = { FString::SanitizeFloat(Ground.Location.X), FString::SanitizeFloat(Ground.Location.Y), FString::SanitizeFloat(Ground.Location.Z) };
		return {};
	}

	FString RunTeleport(AVeyraPlayerController& Requester, TConstArrayView<FString> Args)
	{
		const TOptional<double> X = VeyraDevCommands::ParseNumber(Args, 0);
		const TOptional<double> Y = VeyraDevCommands::ParseNumber(Args, 1);
		const TOptional<double> Z = VeyraDevCommands::ParseNumber(Args, 2);
		if (!X.IsSet() || !Y.IsSet() || (Args.Num() == 3 && !Z.IsSet()) || Args.Num() > 3)
		{
			return VeyraDevCommands::UsageReply(TEXT("Teleport"));
		}
		AVeyraPlayerState* Participant = VeyraDevCommands::ParticipantOf(Requester);
		UAbilitySystemComponent* AbilitySystem = VeyraDevCommands::AbilitySystemOf(Requester);
		AVeyraVanguardCharacter* Body = Requester.GetVanguard();
		if (!Participant || !AbilitySystem || !Body || !VeyraTargeting::IsAlive(Participant))
		{
			return DeadVanguard;
		}
		if (VeyraCombat::IsRiding(*AbilitySystem) || VeyraCombat::GetAttachHost(*AbilitySystem))
		{
			return TEXT("Your Vanguard rides, or holds on to another body; teleport once it lets go.");
		}

		// Onto ground the navigation knows, so orders move it on from there.
		const FVector Wanted(X.GetValue(), Y.GetValue(), Z.IsSet() ? Z.GetValue() : Body->GetActorLocation().Z);
		const UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Requester.GetWorld());
		FNavLocation Ground;
		if (!Navigation || !Navigation->ProjectPointToNavigation(Wanted, Ground))
		{
			return FString::Printf(TEXT("(%.0f, %.0f) is not ground your Vanguard can walk on."), Wanted.X, Wanted.Y);
		}
		if (AVeyraVanguardController* Orders = Participant->GetVanguardController())
		{
			Orders->StopOrders();
		}
		if (UVeyraRecallComponent* Recall = Participant->FindComponentByClass<UVeyraRecallComponent>())
		{
			Recall->Interrupt();
		}
		const FVector Destination = Ground.Location + FVector(0.0, 0.0, Body->GetSimpleCollisionHalfHeight());
		if (!Body->TeleportTo(Destination, Body->GetActorRotation()))
		{
			return TEXT("Something stands there; try a little further away.");
		}
		const FVector Arrived = Body->GetActorLocation();
		return FString::Printf(TEXT("Teleported to (%.0f, %.0f, %.0f)."), Arrived.X, Arrived.Y, Arrived.Z);
	}

	/** Where the typist's Vanguard and cursor are, as this machine sees them, in Teleport's argument form. */
	void RunWhere(UWorld* World, TConstArrayView<FString> /*Args*/, FOutputDevice& Output)
	{
		const AVeyraPlayerController* Typist = VeyraDevCommands::LocalTypist(World);
		const AVeyraVanguardCharacter* Body = Typist ? Typist->GetVanguard() : nullptr;
		if (!Body)
		{
			Output.Log(TEXT("You have no living Vanguard here."));
			return;
		}
		const FVector At = Body->GetActorLocation();
		Output.Logf(TEXT("Your Vanguard stands at %.0f %.0f %.0f."), At.X, At.Y, At.Z);
		FHitResult Ground;
		if (Typist->GetHitResultUnderCursor(ECC_Visibility, /*bTraceComplex*/ false, Ground))
		{
			Output.Logf(TEXT("The cursor points at %.0f %.0f %.0f."), Ground.Location.X, Ground.Location.Y, Ground.Location.Z);
		}
	}
}

void VeyraDevCommands::AddVanguardCommands(TArray<FVeyraDevCommand>& Out)
{
	using namespace VeyraDevVanguardCommands;
	Out.Add(FVeyraDevCommand::Server(TEXT("Heal"), Category, TEXT(""), TEXT("Fills your Vanguard's Health and resource."), &RunHeal));
	Out.Add(FVeyraDevCommand::Server(TEXT("God"), Category, TEXT("[on|off]"),
		TEXT("Your Vanguard takes no damage; no argument turns it over."), &RunGod));
	Out.Add(FVeyraDevCommand::Server(TEXT("Damage"), Category, TEXT("<amount>"),
		TEXT("Hits your Vanguard for this much true damage, through shields and the whole damage pipeline."), &RunDamage));
	Out.Add(FVeyraDevCommand::Server(TEXT("Kill"), Category, TEXT(""), TEXT("Kills your Vanguard, through the damage pipeline."), &RunKill));
	Out.Add(FVeyraDevCommand::Server(TEXT("Respawn"), Category, TEXT(""),
		TEXT("Your dead Vanguard respawns at its fountain now, for free."), &RunRespawn));
	Out.Add(FVeyraDevCommand::Server(TEXT("ResetCooldowns"), Category, TEXT(""),
		TEXT("Every ability, spell and item Active is ready now, and your ward charges are full."), &RunResetCooldowns));
	Out.Add(FVeyraDevCommand::Server(TEXT("Teleport"), Category, TEXT("[x y [z]]"),
		TEXT("Moves your Vanguard to the point, or to the ground under the cursor."), &RunTeleport, &AimTeleport));
	Out.Add(FVeyraDevCommand::Local(TEXT("Where"), Category, TEXT(""),
		TEXT("Prints where your Vanguard stands and the cursor points, as Teleport takes them."), &RunWhere));
}
