// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Shapes/VeyraShapes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Teams/VeyraTeam.h"

#include "VeyraGreyboxSubsystem.generated.h"

class AHUD;
class AVeyraProjectile;
class ULineBatchComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/** What a telegraph warns of. */
UENUM()
enum class EVeyraTelegraphSource : uint8
{
	/** A cast's windup: where it lands at Commit. */
	Windup,
	/** A channel: where each tick lands. */
	Channel,
	/** A delayed area waiting to hit. */
	DelayedArea,
};

/** One telegraphed shape, as this machine draws it. */
struct FVeyraTelegraph
{
	FVeyraPlacedShape Placed;
	EVeyraTelegraphSource Source = EVeyraTelegraphSource::Windup;

	/** The side whose cast it is. */
	EVeyraTeam Team = EVeyraTeam::None;

	/** Seconds until it hits or its phase ends, in server gameplay time. */
	double RemainingSeconds = 0.0;
};

/**
 * The grey-box presentation of one client world (ADR-008 §1). Engine shapes show replicated state
 * and decide nothing:
 * - every unit gets a body in its side's colour, tinted while crowd controlled;
 * - projectiles are drawn from their launch data and the server's clock (ADR-009 §4);
 * - casts, channels and delayed areas are telegraphed from the ability tuning (VeyraCastTelegraphs);
 * - bars and the player's panel are drawn over whichever HUD the player has (VeyraGreyboxHud).
 * No gameplay class knows it. It is not created on a dedicated server, which never loads this module.
 */
UCLASS()
class VEYRAUI_API UVeyraGreyboxSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** A pause stops the server's clock, not the drawing. */
	virtual bool IsTickableWhenPaused() const override { return true; }

	/** Brings the presentation up to date with the world. Its tick calls this; tests call it directly. */
	void Refresh();

	/** The body drawn for Unit, once it has one. */
	UStaticMeshComponent* FindBody(const AActor& Unit) const;

	/** The sphere drawn for Projectile, once it has one. */
	UStaticMeshComponent* FindProjectileVisual(const AVeyraProjectile& Projectile) const;

	/** What the last refresh telegraphed. */
	const TArray<FVeyraTelegraph>& GetTelegraphs() const { return Telegraphs; }

	/** The server's gameplay time as this machine knows it; it stands still while the match is paused. */
	double GetServerNow() const;

	/** The side of this machine's player; None for a spectator or a world without one. */
	EVeyraTeam GetViewerTeam() const;

	/** The colour of Team as the viewer sees it: ally, enemy or neutral. */
	FLinearColor ColorOfSide(EVeyraTeam Team) const;

	/** The colour of Unit as the viewer sees it: the viewer's own Vanguard, or its side's. */
	FLinearColor SideColorOf(const AActor& Unit) const;

	/** The colour Unit's body shows: its side's, tinted while it is stunned or slowed. */
	FLinearColor BodyColorOf(const AActor& Unit) const;

private:
	struct FBody
	{
		TWeakObjectPtr<UStaticMeshComponent> Mesh;
		TWeakObjectPtr<UMaterialInstanceDynamic> Material;
		FLinearColor Shown = FLinearColor::Transparent;
	};

	struct FProjectileVisual
	{
		TWeakObjectPtr<UStaticMeshComponent> Mesh;

		/** A homing projectile's drawn position, stepped toward its target in server time. */
		FVector Position = FVector::ZeroVector;
		double PresentedAt = 0.0;
	};

	void RefreshBodies();
	void RefreshProjectiles();
	void RefreshTelegraphs();
	void DrawTelegraphs();

	/**
	 * Has the local player's HUD draw the grey-box HUD, through an overlay actor it renders for
	 * (AVeyraHudOverlay), once per HUD. That puts the HUD on the HUD's own canvas, under the menus.
	 */
	void AttachHudOverlay();

	/** A shape of Mesh attached to Owner, with its own material instance, or null. */
	UStaticMeshComponent* AddShape(AActor& Owner, UStaticMesh& Mesh, UMaterialInstanceDynamic*& OutMaterial) const;

	/** The ground under Location, lifted for drawing; Location itself where there is none. */
	FVector GroundUnder(const FVector& Location) const;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> BodyMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> ProjectileMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ShapeMaterial;

	UPROPERTY(Transient)
	TObjectPtr<ULineBatchComponent> TelegraphLines;

	TMap<TWeakObjectPtr<const AActor>, FBody> Bodies;
	TMap<TWeakObjectPtr<const AVeyraProjectile>, FProjectileVisual> Projectiles;
	TArray<FVeyraTelegraph> Telegraphs;

	/** The overlay actor, which the world owns, and the HUD it draws for. */
	TWeakObjectPtr<AActor> HudOverlay;
	TWeakObjectPtr<AHUD> OverlayHud;

	/** Whether the settings and their assets are usable; nothing is drawn otherwise. */
	bool bReady = false;
};
