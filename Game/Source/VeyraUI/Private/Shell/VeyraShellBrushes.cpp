// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraShellBrushes.h"

#include "Brushes/SlateNoResource.h"
#include "Engine/Texture2D.h"

namespace VeyraShellBrushes
{
namespace
{
	// How a shape is rasterised, not how it looks: texels per slate unit (crisp at twice the standard DPI), samples
	// along each axis of a texel (smooth diagonals), and the room left for a fill's gradient to stretch over.
	constexpr int32 TexelsPerUnit = 2;
	constexpr int32 Samples = 4;
	constexpr float GradientUnits = 32.0f;
	constexpr float ShineUnits = 1.0f;

	/** A made texture, rooted for the life of the process: a handful of small textures every screen shares. */
	struct FMade
	{
		UTexture2D* Texture = nullptr;
		FVeyraFaceLook Look;
		bool bFace = false;
	};

	TMap<FString, FMade>& Made()
	{
		static TMap<FString, FMade> Textures;
		return Textures;
	}

	FString KeyOf(const TCHAR* Kind, const FVeyraFaceLook& Look)
	{
		return FString::Printf(TEXT("%s|%s|%s|%s|%s|%s|%.3f|%.3f"), Kind, *Look.Top.ToString(), *Look.Bottom.ToString(), *Look.Edge.ToString(), *Look.Tick.ToString(),
			*Look.Shine.ToString(), Look.EdgeWidth, Look.Chamfer);
	}

	/** Width by Height texels of Texels, as a texture for the UI. */
	UTexture2D* MakeTexture(int32 Width, int32 Height, const TArray<FColor>& Texels)
	{
		UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8, NAME_None);
		if (!Texture)
		{
			return nullptr;
		}
		Texture->Filter = TF_Bilinear;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		Texture->LODGroup = TEXTUREGROUP_UI;
		Texture->SRGB = true;
		FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
		void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Data, Texels.GetData(), Texels.Num() * sizeof(FColor));
		Mip.BulkData.Unlock();
		Texture->UpdateResource();
		Texture->AddToRoot();
		return Texture;
	}

	/** A box brush of Texture, Size slate units, whose Margin (in slate units) keeps its size whatever it is drawn over. */
	FSlateBrush BoxOf(UTexture2D* Texture, const FVector2D& Size, const FMargin& Margin)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = Size;
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.Margin = FMargin(Margin.Left / Size.X, Margin.Top / Size.Y, Margin.Right / Size.X, Margin.Bottom / Size.Y);
		Brush.TintColor = FSlateColor(FLinearColor::White);
		return Brush;
	}

	/** The room Look's corners keep: the cut, the edge and a unit more. */
	float RimOf(const FVeyraFaceLook& Look)
	{
		const bool bEdged = Look.Edge.A > 0.0f || Look.Tick.A > 0.0f;
		return FMath::Max(Look.Chamfer, 0.0f) + (bEdged ? FMath::Max(Look.EdgeWidth, 0.0f) : 0.0f) + 1.0f;
	}

	/** Premultiplied colour, so samples average without dark fringes. */
	FLinearColor Premultiplied(const FLinearColor& Color)
	{
		return FLinearColor(Color.R * Color.A, Color.G * Color.A, Color.B * Color.A, Color.A);
	}

	/** Look's face, made once and shared. */
	FSlateBrush FaceBrush(const TCHAR* Kind, const FVeyraFaceLook& Look, bool bFace)
	{
		const FString Key = KeyOf(Kind, Look);
		FMade* Found = Made().Find(Key);
		if (!Found)
		{
			const FVeyraFaceTexels Face = Rasterise(Look);
			FMade Fresh;
			Fresh.Texture = MakeTexture(Face.Width, Face.Height, Face.Texels);
			Fresh.Look = Look;
			Fresh.bFace = bFace;
			Found = &Made().Add(Key, MoveTemp(Fresh));
		}
		UTexture2D* Texture = Found->Texture;
		if (!Texture)
		{
			return None();
		}
		const float Rim = RimOf(Look);
		return BoxOf(Texture, FVector2D(static_cast<float>(Texture->GetSizeX()) / TexelsPerUnit, static_cast<float>(Texture->GetSizeY()) / TexelsPerUnit), FMargin(Rim));
	}
}

FVeyraFaceTexels Rasterise(const FVeyraFaceLook& Look)
{
	// The fill under the edge where the edge is clear, the edge over the shape's border, the ticks over its cut corners,
	// and the shine along its head inside the edge.
	const bool bEdged = Look.Edge.A > 0.0f || Look.Tick.A > 0.0f;
	const float Edge = bEdged ? FMath::Max(Look.EdgeWidth, 0.0f) : 0.0f;
	const float Cut = FMath::Max(Look.Chamfer, 0.0f);
	// The corners keep their size; the middle stretches.
	const float Rim = RimOf(Look);
	const float Width = Rim * 2.0f + 2.0f;
	const float Height = Rim * 2.0f + GradientUnits;
	FVeyraFaceTexels Out;
	Out.Width = FMath::CeilToInt32(Width * TexelsPerUnit);
	Out.Height = FMath::CeilToInt32(Height * TexelsPerUnit);
	Out.Rim = Rim;
	Out.TexelsPerUnit = TexelsPerUnit;
	const float Diagonal = UE_SQRT_2;
	const auto Inside = [&](float X, float Y, float Inset) {
		return X >= Inset && X <= Width - Inset && Y >= Inset && Y <= Height - Inset && X + Y >= Cut + Inset * Diagonal
			&& (Width - X) + (Height - Y) >= Cut + Inset * Diagonal;
	};
	Out.Texels.SetNumUninitialized(Out.Width * Out.Height);
	for (int32 Row = 0; Row < Out.Height; ++Row)
	{
		for (int32 Column = 0; Column < Out.Width; ++Column)
		{
			FLinearColor Sum(0.0f, 0.0f, 0.0f, 0.0f);
			for (int32 SampleY = 0; SampleY < Samples; ++SampleY)
			{
				for (int32 SampleX = 0; SampleX < Samples; ++SampleX)
				{
					const float X = (Column + (SampleX + 0.5f) / Samples) / TexelsPerUnit;
					const float Y = (Row + (SampleY + 0.5f) / Samples) / TexelsPerUnit;
					if (!Inside(X, Y, 0.0f))
					{
						continue;
					}
					if (!Inside(X, Y, Edge))
					{
						// On the border: a tick where the cut is nearer than a straight side.
						const float ToCut = FMath::Min(X + Y - Cut, (Width - X) + (Height - Y) - Cut) / Diagonal;
						const float ToSide = FMath::Min(FMath::Min(X, Width - X), FMath::Min(Y, Height - Y));
						Sum += Premultiplied(Cut > 0.0f && ToCut < ToSide ? Look.Tick : Look.Edge);
						continue;
					}
					const float Along = FMath::Clamp((Y - Rim) / (Height - Rim * 2.0f), 0.0f, 1.0f);
					FLinearColor Fill = Premultiplied(FMath::Lerp(Look.Top, Look.Bottom, Along));
					if (Look.Shine.A > 0.0f && Y < Edge + ShineUnits)
					{
						Fill = Premultiplied(Look.Shine) + Fill * (1.0f - Look.Shine.A);
					}
					Sum += Fill;
				}
			}
			Sum /= static_cast<float>(Samples * Samples);
			const FLinearColor Straight = Sum.A > 0.0f ? FLinearColor(Sum.R / Sum.A, Sum.G / Sum.A, Sum.B / Sum.A, Sum.A) : FLinearColor::Transparent;
			Out.Texels[Row * Out.Width + Column] = Straight.ToFColor(/*bSRGB*/ true);
		}
	}
	return Out;
}

FSlateBrush Mask(float Chamfer, float Shade)
{
	FVeyraFaceLook Look;
	Look.Top = FLinearColor::White;
	const float Foot = 1.0f - FMath::Clamp(Shade, 0.0f, 1.0f);
	Look.Bottom = FLinearColor(Foot, Foot, Foot, 1.0f);
	Look.Chamfer = Chamfer;
	return FaceBrush(TEXT("Mask"), Look, /*bFace*/ false);
}

FSlateBrush Outline(float Chamfer, const FLinearColor& Edge, const FLinearColor& Tick, float EdgeWidth)
{
	FVeyraFaceLook Look;
	Look.Edge = Edge;
	Look.Tick = Tick;
	Look.EdgeWidth = EdgeWidth;
	Look.Chamfer = Chamfer;
	return FaceBrush(TEXT("Outline"), Look, /*bFace*/ false);
}

FSlateBrush Face(const FVeyraFaceLook& Look)
{
	return FaceBrush(TEXT("Face"), Look, /*bFace*/ true);
}

const FVeyraFaceLook* LookOf(const FSlateBrush& Brush)
{
	const UObject* Resource = Brush.GetResourceObject();
	if (!Resource)
	{
		return nullptr;
	}
	for (const TPair<FString, FMade>& Each : Made())
	{
		if (Each.Value.bFace && Each.Value.Texture == Resource)
		{
			return &Each.Value.Look;
		}
	}
	return nullptr;
}

FSlateBrush Underline(float Height, const FLinearColor& Color)
{
	const FString Key = FString::Printf(TEXT("Underline|%s|%.3f"), *Color.ToString(), Height);
	FMade* Found = Made().Find(Key);
	// One unit clear above the line, which stretches; the line keeps its height.
	const float Line = FMath::Max(Height, 0.0f);
	const float Total = Line + 1.0f;
	if (!Found)
	{
		const int32 TexelsHigh = FMath::CeilToInt32(Total * TexelsPerUnit);
		const int32 LineTexels = FMath::RoundToInt32(Line * TexelsPerUnit);
		TArray<FColor> Texels;
		Texels.Init(FColor(0, 0, 0, 0), TexelsHigh);
		for (int32 Row = TexelsHigh - LineTexels; Row < TexelsHigh; ++Row)
		{
			Texels[Row] = Color.ToFColor(/*bSRGB*/ true);
		}
		FMade Fresh;
		Fresh.Texture = MakeTexture(1, TexelsHigh, Texels);
		Found = &Made().Add(Key, MoveTemp(Fresh));
	}
	UTexture2D* Texture = Found->Texture;
	if (!Texture)
	{
		return None();
	}
	return BoxOf(Texture, FVector2D(1.0f / TexelsPerUnit, static_cast<float>(Texture->GetSizeY()) / TexelsPerUnit), FMargin(0.0f, 0.0f, 0.0f, Line));
}

FSlateBrush Diamond(float Size, const FLinearColor& Fill, const FLinearColor& Edge)
{
	const FString Key = FString::Printf(TEXT("Diamond|%s|%s|%.3f"), *Fill.ToString(), *Edge.ToString(), Size);
	FMade* Found = Made().Find(Key);
	const float Tall = FMath::Max(Size, 1.0f);
	// Two thirds as wide as tall, as the mark is drawn.
	const float Wide = Tall * 2.0f / 3.0f;
	if (!Found)
	{
		const int32 TexelsWide = FMath::CeilToInt32(Wide * TexelsPerUnit);
		const int32 TexelsHigh = FMath::CeilToInt32(Tall * TexelsPerUnit);
		// The edge is a fixed width: a unit and a half, or the whole mark when it is smaller.
		const float EdgeUnits = Edge.A > 0.0f ? 1.5f : 0.0f;
		TArray<FColor> Texels;
		Texels.SetNumUninitialized(TexelsWide * TexelsHigh);
		for (int32 Row = 0; Row < TexelsHigh; ++Row)
		{
			for (int32 Column = 0; Column < TexelsWide; ++Column)
			{
				FLinearColor Sum(0.0f, 0.0f, 0.0f, 0.0f);
				for (int32 SampleY = 0; SampleY < Samples; ++SampleY)
				{
					for (int32 SampleX = 0; SampleX < Samples; ++SampleX)
					{
						const float X = (Column + (SampleX + 0.5f) / Samples) / TexelsPerUnit;
						const float Y = (Row + (SampleY + 0.5f) / Samples) / TexelsPerUnit;
						// How far inside the diamond, 1 at its centre and 0 on its edge, and in units from its edge.
						const float Inside = 1.0f - (FMath::Abs(X - Wide / 2.0f) / (Wide / 2.0f) + FMath::Abs(Y - Tall / 2.0f) / (Tall / 2.0f));
						if (Inside < 0.0f)
						{
							continue;
						}
						const float FromEdge = Inside * Wide / 2.0f;
						Sum += Premultiplied(FromEdge < EdgeUnits ? Edge : Fill);
					}
				}
				Sum /= static_cast<float>(Samples * Samples);
				const FLinearColor Straight = Sum.A > 0.0f ? FLinearColor(Sum.R / Sum.A, Sum.G / Sum.A, Sum.B / Sum.A, Sum.A) : FLinearColor::Transparent;
				Texels[Row * TexelsWide + Column] = Straight.ToFColor(/*bSRGB*/ true);
			}
		}
		FMade Fresh;
		Fresh.Texture = MakeTexture(TexelsWide, TexelsHigh, Texels);
		Found = &Made().Add(Key, MoveTemp(Fresh));
	}
	if (!Found->Texture)
	{
		return None();
	}
	FSlateBrush Brush;
	Brush.SetResourceObject(Found->Texture);
	Brush.ImageSize = FVector2D(Wide, Tall);
	Brush.DrawAs = ESlateBrushDrawType::Image;
	return Brush;
}

FSlateBrush None()
{
	return FSlateNoResource();
}

int32 TextureCount()
{
	return Made().Num();
}
}
