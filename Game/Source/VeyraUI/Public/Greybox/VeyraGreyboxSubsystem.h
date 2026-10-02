// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Hud/VeyraCombatTextModel.h"
#include "Shapes/VeyraShapes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Teams/VeyraTeam.h"

#include "VeyraGreyboxSubsystem.generated.h"

class AHUD;
class AVeyraProjectile;
class UFont;
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
	/** A lingering area, until it ends (ADR-018 §5). */
	LingeringArea,
	/** A lingering area whose end is near and hits (ADR-026 §4). */
	LingeringAreaEnding,
	/** The local player's indicator: where an ability would land, before it is cast (ADR-041 §2). */
	Indicator,
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
 * - the battleground's lanes, river and bases are drawn on its floor from its layout;
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

	/** A structure's art, once drawn; null for any other unit. */
	UStaticMeshComponent* FindArt(const AActor& Unit) const;

	/** The sphere drawn for Projectile, once it has one. */
	UStaticMeshComponent* FindProjectileVisual(const AVeyraProjectile& Projectile) const;

	/** The actor holding the battleground's drawn ground, once the world shows the battleground. */
	AActor* GetGround() const { return GroundMarkings.Get(); }

	/** What the last refresh telegraphed. */
	const TArray<FVeyraTelegraph>& GetTelegraphs() const { return Telegraphs; }

	/** The server's gameplay time as this machine knows it; it stands still while the match is paused. */
	double GetServerNow() const;

	/**
	 * The HUD's font: the engine's Roboto family, every weight of it, as a font asset, which canvas text
	 * needs (a canvas draws no text from a font that is not an asset).
	 */
	UFont* GetHudFont() const { return HudFont; }

	/** The side of this machine's player; None for a spectator or a world without one. */
	EVeyraTeam GetViewerTeam() const;

	/** The combat text this machine's player received and still shows, oldest first, by this machine's clock (ADR-052 §1). */
	const TArray<FVeyraCombatTextArrival>& GetCombatText() const { return CombatText; }

	/** The colour of Team as the viewer sees it: ally, enemy or neutral. */
	FLinearColor ColorOfSide(EVeyraTeam Team) const;

	/** The colour of Unit as the viewer sees it: the viewer's own Vanguard, or its side's. */
	FLinearColor SideColorOf(const AActor& Unit) const;

	/** The colour Unit's body shows: its side's, tinted while it is stunned or slowed. */
	FLinearColor BodyColorOf(const AActor& Unit) const;

private:
	/** Listens to the local player's combat text once its controller exists, and forgets the numbers done showing. */
	void RefreshCombatText();
	void OnCombatText(const FVeyraCombatTextLine& Line);

	TArray<FVeyraCombatTextArrival> CombatText;
	TWeakObjectPtr<class AVeyraPlayerController> CombatTextSource;
	FDelegateHandle CombatTextHandle;

	struct FBody
	{
		TWeakObjectPtr<UStaticMeshComponent> Mesh;
		TWeakObjectPtr<UMaterialInstanceDynamic> Material;
		FLinearColor Shown = FLinearColor::Transparent;

		/** A unit's art, which stands in for its body: the component, the mesh it shows, and its Flux's colour. */
		TWeakObjectPtr<UStaticMeshComponent> Art;
		TWeakObjectPtr<UStaticMesh> ArtMesh;
		TWeakObjectPtr<UMaterialInstanceDynamic> ArtFlux;
		FLinearColor ArtShown = FLinearColor::Transparent;
	};

	struct FProjectileVisual
	{
		TWeakObjectPtr<UStaticMeshComponent> Mesh;

		/** A homing projectile's drawn position, stepped toward its target in server time. */
		FVector Position = FVector::ZeroVector;
		double PresentedAt = 0.0;
	};

	void RefreshBodies();

	/** Dresses Structure in its kind's art, standing or wrecked as it is, its Flux in its side's colour; its body hides behind it. */
	void RefreshStructureArt(const class AVeyraStructure& Structure, FBody& Body);

	/** Dresses Unit in its kind's art, active or collapsed as it is, its Flux in its body's colour, once its kind is known and has art. */
	void RefreshFluxbornArt(const class AVeyraFluxborn& Unit, FBody& Body);

	/**
	 * Draws Mesh over Unit in place of its body, its pivot at the capsule's foot, and colours the art set's Flux
	 * slot Color. Visual only, as a body is: it blocks nothing and shapes no navigation.
	 */
	void ShowArt(const APawn& Unit, FBody& Body, UStaticMesh& Mesh, const class UVeyraUnitArtSet& Set, const FLinearColor& Color);

	/**
	 * Once the world shows the battleground (its structures have arrived), draws its ground from
	 * World.json's layout: the river, the lanes' road and each base's pad. It decides nothing:
	 * nothing collides with it or shapes navigation.
	 */
	void RefreshBattleground();

	/**
	 * A flat marking of Mesh in Color, centred on Centre, turned by Yaw, with half extents HalfSize on
	 * the ground, Layer lifts up. Returns its material instance, or null.
	 */
	UMaterialInstanceDynamic* AddGroundMarking(AActor& Owner, UStaticMesh& Mesh, const FLinearColor& Color, const FVector2D& Centre, double Yaw,
		const FVector2D& HalfSize, int32 Layer) const;

	/** The colour of Team's base as the viewer sees it. */
	FLinearColor BaseColorOf(EVeyraTeam Team) const;

	void RefreshProjectiles();
	void RefreshTelegraphs();

	/** Adds the local player's indicator, while it holds a cast ready or previews one, aimed at the ground under its cursor. */
	void AddIndicator(const class AVeyraPlayerController& Local, const struct FVeyraAbilitiesTuning& Tuning);

	/** Adds the ring of the local player's basic attack reach around the body it commands, while Show Attack Range is held. */
	void AddAttackRange(const class AVeyraPlayerController& Local);
	void DrawTelegraphs();

	/** The viewer's side's presence pings and outlines, drawn on the ground with the telegraphs (ADR-016 §8). */
	void DrawVisionMarks();

	/** The chains between companions and their owners, joining the telegraphs' lines (ADR-034 §7). */
	void DrawChains();

	/** A projected Echo's tether circle and stream (ADR-050 §7). */
	void DrawEchoTethers();

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
	TObjectPtr<UStaticMesh> GroundMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> PadMesh;

	/** The structure kit's and the Fluxborn kit's art sets, loaded with the settings. */
	UPROPERTY(Transient)
	TObjectPtr<class UVeyraUnitArtSet> StructureArt;

	UPROPERTY(Transient)
	TObjectPtr<class UVeyraUnitArtSet> FluxbornArt;

	/** The actor holding the battleground's ground markings, once drawn. */
	TWeakObjectPtr<AActor> GroundMarkings;

	/** The fog banks drawn on the ground already: each carries its markings, and takes them as it goes (ADR-036 §1). */
	TSet<TWeakObjectPtr<const AActor>> DrawnFogBanks;

	/** Each base's pad, and the viewer's side they were coloured for. */
	TMap<EVeyraTeam, TWeakObjectPtr<UMaterialInstanceDynamic>> PadMaterials;
	EVeyraTeam PadsDrawnFor = EVeyraTeam::None;

	UPROPERTY(Transient)
	TObjectPtr<ULineBatchComponent> TelegraphLines;

	UPROPERTY(Transient)
	TObjectPtr<UFont> HudFont;

	TMap<TWeakObjectPtr<const AActor>, FBody> Bodies;
	TMap<TWeakObjectPtr<const AVeyraProjectile>, FProjectileVisual> Projectiles;
	TArray<FVeyraTelegraph> Telegraphs;

	/** The overlay actor, which the world owns, and the HUD it draws for. */
	TWeakObjectPtr<AActor> HudOverlay;
	TWeakObjectPtr<AHUD> OverlayHud;

	/** Whether the settings and their assets are usable; nothing is drawn otherwise. */
	bool bReady = false;
};
