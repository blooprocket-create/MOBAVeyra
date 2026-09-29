// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraShellArt.h"

#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "UObject/SoftObjectPath.h"

namespace VeyraShellArt
{
FString HeroPackageName(const FString& VanguardId)
{
	return FString::Printf(TEXT("%s/T_%s_Hero"), *GetDefault<UVeyraShellStyleSettings>()->VanguardArtFolder, *VanguardId);
}

UTexture2D* HeroOf(const FString& VanguardId)
{
	if (VanguardId.IsEmpty())
	{
		return nullptr;
	}
	const FString PackageName = HeroPackageName(VanguardId);
	// A Vanguard whose art is not imported yet is shown without it, so only a package that exists is loaded.
	if (!FPackageName::DoesPackageExist(PackageName))
	{
		return nullptr;
	}
	const FSoftObjectPath Path(FString::Printf(TEXT("%s.%s"), *PackageName, *FPackageName::GetShortName(PackageName)));
	return Cast<UTexture2D>(Path.TryLoad());
}

FBox2f Crop(const FString& VanguardId, int32 Width, int32 Height, float Aspect, bool bPortrait)
{
	if (Width <= 0 || Height <= 0 || Aspect <= 0.0f)
	{
		return FBox2f(FVector2f::ZeroVector, FVector2f::UnitVector);
	}
	const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
	const FVeyraVanguardPortrait* Found = Style.VanguardPortraits.FindByPredicate([&VanguardId](const FVeyraVanguardPortrait& Portrait) {
		return Portrait.Vanguard == VanguardId;
	});
	const FVeyraVanguardPortrait& Portrait = Found ? *Found : Style.DefaultPortrait;
	// In pixels: as tall as asked, then no wider or taller than the illustration.
	float CropHeight = bPortrait ? Portrait.CropHeight * Height : static_cast<float>(Height);
	float CropWidth = CropHeight * Aspect;
	if (CropWidth > Width)
	{
		CropWidth = static_cast<float>(Width);
		CropHeight = CropWidth / Aspect;
	}
	const float Left = FMath::Clamp(static_cast<float>(Portrait.Focus.X) * Width - CropWidth / 2.0f, 0.0f, Width - CropWidth);
	const float Top = FMath::Clamp(static_cast<float>(Portrait.Focus.Y) * Height - CropHeight / 2.0f, 0.0f, Height - CropHeight);
	return FBox2f(FVector2f(Left / Width, Top / Height), FVector2f((Left + CropWidth) / Width, (Top + CropHeight) / Height));
}

FSlateBrush Brush(UTexture2D* Hero, const FBox2f& Region, const FVector2D& Size, float CornerRadius, const FLinearColor& Fill, const FLinearColor& Outline,
	float OutlineWidth)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.ImageSize = Size;
	if (Hero)
	{
		Brush.SetResourceObject(Hero);
		Brush.SetUVRegion(Region);
	}
	else
	{
		Brush.TintColor = FSlateColor(Fill);
	}
	const bool bCircle = CornerRadius < 0.0f;
	const float Radius = bCircle ? 0.0f : CornerRadius;
	Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(Radius, Radius, Radius, Radius), FSlateColor(Outline), OutlineWidth);
	Brush.OutlineSettings.RoundingType = bCircle ? ESlateBrushRoundingType::HalfHeightRadius : ESlateBrushRoundingType::FixedRadius;
	return Brush;
}
}
