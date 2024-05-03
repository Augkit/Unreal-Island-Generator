// Fill out your copyright notice in the Description page of Project Settings.


#include "PCG/PCGIDTextureData.h"

#include "Data/PCGPointData.h"
#include "Helpers/PCGAsync.h"
#include "Helpers/PCGHelpers.h"

#include "TextureResource.h"
#include "Engine/Texture2D.h"
#include "Metadata/PCGMetadataAccessor.h"
#include "PCG/PCGIDTextureSampler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGIDTextureData)

using namespace IDTextureFixedName;

FBox UPCGIDTextureData::GetBounds() const
{
	return Bounds;
}

FBox UPCGIDTextureData::GetStrictBounds() const
{
	return Bounds;
}

bool UPCGIDTextureData::SamplePoint(const FTransform& InTransform, const FBox& InBounds, FPCGPoint& OutPoint,
                                    UPCGMetadata* OutMetadata) const
{
	if (!IsValid())
	{
		return false;
	}

	OutPoint.Transform = InTransform;
	FVector PointPositionInLocalSpace = Transform.InverseTransformPosition(InTransform.GetLocation());
	PointPositionInLocalSpace.Z = 0;
	OutPoint.Transform.SetLocation(Transform.TransformPosition(PointPositionInLocalSpace));
	OutPoint.SetLocalBounds(InBounds);
	UPCGMetadataAccessorHelpers::SetInteger32Attribute(OutPoint, OutMetadata, DataAttrPrimaryID, PrimaryID);

	FVector2D Position2D(PointPositionInLocalSpace.X, PointPositionInLocalSpace.Y);

	FVector2D PointPositionInTextureSpace = (Position2D + 1) / 2;
	PointPositionInTextureSpace.X *= TextureData->Width - 1;
	PointPositionInTextureSpace.Y *= TextureData->Height - 1;
	// PointPositionInTextureSpace.X += 0.5f;
	// PointPositionInTextureSpace.Y += 0.5f;

	int32 Index = static_cast<int32>(PointPositionInTextureSpace.X)
		+ static_cast<int32>(PointPositionInTextureSpace.Y)
		* TextureData->Width;
	if (Index < 0 || Index >= TextureData->Data.Num())
	{
		return false;
	}
	FPixelData& PixelData = TextureData->Data[Index];
	OutPoint.Density = ((DensityFunction == EPCGIDTextureDensityFunction::Ignore)
		                    ? 1.0f
		                    : PixelData.DistrictID1 == PrimaryID
		                    ? PixelData.Proportion1
		                    : 0.f);
	UPCGMetadataAccessorHelpers::SetInteger32Attribute(OutPoint, OutMetadata, DataAttrPrimaryID, PrimaryID);
	UPCGMetadataAccessorHelpers::SetInteger32Attribute(OutPoint, OutMetadata, DataAttrDistrictID1,
	                                                   PixelData.DistrictID1);
	UPCGMetadataAccessorHelpers::SetInteger32Attribute(OutPoint, OutMetadata, DataAttrDistrictID2,
	                                                   PixelData.DistrictID2);
	UPCGMetadataAccessorHelpers::SetInteger32Attribute(OutPoint, OutMetadata, DataAttrDistrictID3,
	                                                   PixelData.DistrictID3);
	UPCGMetadataAccessorHelpers::SetInteger32Attribute(OutPoint, OutMetadata, DataAttrDistrictID4,
	                                                   PixelData.DistrictID4);
	UPCGMetadataAccessorHelpers::SetFloatAttribute(OutPoint, OutMetadata, DataAttrProportion1, PixelData.Proportion1);
	UPCGMetadataAccessorHelpers::SetFloatAttribute(OutPoint, OutMetadata, DataAttrProportion2, PixelData.Proportion2);
	UPCGMetadataAccessorHelpers::SetFloatAttribute(OutPoint, OutMetadata, DataAttrProportion3, PixelData.Proportion3);
	UPCGMetadataAccessorHelpers::SetFloatAttribute(OutPoint, OutMetadata, DataAttrProportion4, PixelData.Proportion4);
	return OutPoint.Density > 0;
}

const UPCGPointData* UPCGIDTextureData::CreatePointData(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGIDTextureData::CreatePointData);
	// TODO: this is a trivial implementation
	// A better sampler would allow to sample a fixed number of points in either direction
	// or based on a given texel size
	FBox2D LocalSurfaceBounds(FVector2D(-1.0f, -1.0f), FVector2D(1.0f, 1.0f));

	UPCGPointData* Data = NewObject<UPCGPointData>();
	Data->InitializeFromData(this);

	// Early out for invalid data
	if (!IsValid())
	{
		UE_LOG(LogPCG, Error, TEXT("Texture data does not have valid sizes - will return empty data"));
		return Data;
	}

	TArray<FPCGPoint>& Points = Data->GetMutablePoints();

	// Map target texel size to the current physical size of the texture data.
	const FVector::FReal XSize = 2.0 * Transform.GetScale3D().X;
	const FVector::FReal YSize = 2.0 * Transform.GetScale3D().Y;

	const int32 XCount = FMath::Floor(XSize / TexelSize);
	const int32 YCount = FMath::Floor(YSize / TexelSize);
	const int32 PointCount = XCount * YCount;

	if (PointCount <= 0)
	{
		UE_LOG(LogPCG, Warning, TEXT("Texture data has a texel size larger than its data - will return empty data"));
		return Data;
	}

	UPCGMetadata* OutMetadata = Data->MutableMetadata();
	TArray<FPixelData>& OriTextureData = TextureData.Get()->Data;
	FPCGAsync::AsyncPointProcessing(
		Context, PointCount, Points,
		[this, XCount, YCount, &OriTextureData, &OutMetadata](int32 Index, FPCGPoint& OutPoint)
		{
			const int LocalX = Index % XCount;
			const int LocalY = Index / YCount;
			const int X = static_cast<float>(LocalX) / XCount * Width;
			const int Y = static_cast<float>(LocalY) / YCount * Height;
			if (X >= Width || Y >= Height)
			{
				return false;
			}
			FPixelData& PixelData = OriTextureData[X + Y * Width];
			const float Density = (
				(DensityFunction == EPCGIDTextureDensityFunction::Ignore)
					? 1.0f
					: PixelData.DistrictID1 == PrimaryID
					? PixelData.Proportion1
					: 0.f);
#if WITH_EDITOR
			if (Density > 0 || bKeepZeroDensityPoints)
#else
			if (Density > 0)
#endif
			{
				FVector LocalPosition((2.0 * LocalX + 0.5) / XCount - 1.0,
				                      (2.0 * LocalY + 0.5) / YCount - 1.0, 0);
				OutPoint = FPCGPoint(
					FTransform(Transform.TransformPosition(LocalPosition)),
					Density,
					PCGHelpers::ComputeSeed(X, Y));
				UPCGMetadataAccessorHelpers::SetInteger32Attribute(OutPoint, OutMetadata, DataAttrPrimaryID, PrimaryID);
				UPCGMetadataAccessorHelpers::SetInteger32Attribute(OutPoint, OutMetadata, DataAttrDistrictID1,
				                                                   PixelData.DistrictID1);
				UPCGMetadataAccessorHelpers::SetInteger32Attribute(OutPoint, OutMetadata, DataAttrDistrictID2,
				                                                   PixelData.DistrictID2);
				UPCGMetadataAccessorHelpers::SetInteger32Attribute(OutPoint, OutMetadata, DataAttrDistrictID3,
				                                                   PixelData.DistrictID3);
				UPCGMetadataAccessorHelpers::SetInteger32Attribute(OutPoint, OutMetadata, DataAttrDistrictID4,
				                                                   PixelData.DistrictID4);
				UPCGMetadataAccessorHelpers::SetFloatAttribute(OutPoint, OutMetadata, DataAttrProportion1,
				                                               PixelData.Proportion1);
				UPCGMetadataAccessorHelpers::SetFloatAttribute(OutPoint, OutMetadata, DataAttrProportion2,
				                                               PixelData.Proportion2);
				UPCGMetadataAccessorHelpers::SetFloatAttribute(OutPoint, OutMetadata, DataAttrProportion3,
				                                               PixelData.Proportion3);
				UPCGMetadataAccessorHelpers::SetFloatAttribute(OutPoint, OutMetadata, DataAttrProportion4,
				                                               PixelData.Proportion4);

				OutPoint.SetExtents(FVector(TexelSize / 2.0));

				return true;
			}

			return false;
		});

	return Data;
}


bool UPCGIDTextureData::IsValid() const
{
	return Height > 0 && Width > 0;
}

void UPCGIDTextureData::CopyBaseTextureData(UPCGIDTextureData* NewTextureData) const
{
	CopyBaseSurfaceData(NewTextureData);
	NewTextureData->PrimaryID = PrimaryID;
	NewTextureData->DensityFunction = DensityFunction;
	NewTextureData->TexelSize = TexelSize;
	NewTextureData->Bounds = Bounds;
	NewTextureData->Height = Height;
	NewTextureData->Width = Width;
}

void UPCGIDTextureData::Initialize(const TSharedPtr<FIDTextueData, ESPMode::ThreadSafe>& InTextureData,
                                   const FTransform& InTransform)
{
	TextureData = InTextureData;
	Transform = InTransform;
	Width = InTextureData->Width;
	Height = InTextureData->Height;
	Bounds = FBox(EForceInit::ForceInit);
	Bounds += FVector(-1.0f, -1.0f, 0.0f);
	Bounds += FVector(1.0f, 1.0f, 0.0f);
	Bounds = Bounds.TransformBy(Transform);
}

bool UPCGIDTextureData::IsSupported(UTexture2D* InTexture)
{
	const FTexturePlatformData* PlatformData = InTexture ? InTexture->GetPlatformData() : nullptr;

	return PlatformData && PlatformData->Mips.Num() > 0 &&
		(PlatformData->PixelFormat == PF_FloatRGBA);
}

UPCGSpatialData* UPCGIDTextureData::CopyInternal() const
{
	UPCGIDTextureData* NewTextureData = NewObject<UPCGIDTextureData>();

	CopyBaseTextureData(NewTextureData);

	NewTextureData->TextureData = TextureData;

	return NewTextureData;
}
