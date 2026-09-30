// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Math/Box2D.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "Styling/SlateBrush.h"

class UTexture2D;

/**
 * The Vanguards' art on the shell's screens: each playable Vanguard's hero illustration, imported as
 * a UI texture by UVeyraVanguardArtCommandlet (Game/Scripts/BuildVanguardArt.ps1) into the style's
 * VanguardArtFolder. Champion select shows it large and crops portraits from it around the face the
 * style's VanguardPortraits name. Each item's icon is imported the same way into the style's
 * ItemArtFolder (Game/Scripts/BuildIconArt.ps1 -Kind Items), for the shop's tiles and the HUD's item bar.
 */
namespace VeyraShellArt
{
	/** The package VanguardId's hero texture is saved in: "<VanguardArtFolder>/T_<id>_Hero". */
	VEYRAUI_API FString HeroPackageName(const FString& VanguardId);

	/** VanguardId's hero texture, loaded; null when there is no ID or its art has not been imported. */
	VEYRAUI_API UTexture2D* HeroOf(const FString& VanguardId);

	/** The package ItemId's icon is saved in: "<ItemArtFolder>/T_<id>_Icon". */
	VEYRAUI_API FString ItemIconPackageName(const FString& ItemId);

	/**
	 * ItemId's icon, loaded; null when there is no ID or its icon has not been imported, which the UI
	 * shows as the item's initials. Remembered, since the HUD asks every frame.
	 */
	VEYRAUI_API UTexture2D* ItemIconOf(const FString& ItemId);

	/** The package AbilityId's icon is saved in: "<AbilityArtFolder>/T_<id>_Icon". Flux Spells are abilities. */
	VEYRAUI_API FString AbilityIconPackageName(const FString& AbilityId);

	/** AbilityId's icon, a passive's or a Flux Spell's too, loaded and remembered; null until one is imported. */
	VEYRAUI_API UTexture2D* AbilityIconOf(const FString& AbilityId);

	/**
	 * The part of a Width x Height hero illustration to show at Aspect (width over height), as UVs.
	 * A portrait crops the style's CropHeight of the height around VanguardId's face; otherwise the
	 * crop is as large as the illustration allows. Either way it is centred on the face as far as the
	 * illustration's edges let it be.
	 */
	VEYRAUI_API FBox2f Crop(const FString& VanguardId, int32 Width, int32 Height, float Aspect, bool bPortrait);

	/**
	 * A brush drawing Region of Hero at Size, with rounded corners of CornerRadius, or a circle when
	 * CornerRadius is negative, outlined by Outline at OutlineWidth. With no Hero it is a disc or
	 * rounded box of Fill, which marks an empty seat.
	 */
	VEYRAUI_API FSlateBrush Brush(UTexture2D* Hero, const FBox2f& Region, const FVector2D& Size, float CornerRadius, const FLinearColor& Fill,
		const FLinearColor& Outline, float OutlineWidth);
}
