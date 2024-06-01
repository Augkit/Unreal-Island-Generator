// Fill out your copyright notice in the Description page of Project Settings.


#include "PCG/PCGIDTextureSampler.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGCrc.h"
#include "PCGPin.h"
#include "Helpers/PCGBlueprintHelpers.h"
#include "Helpers/PCGHelpers.h"
#include "Helpers/PCGSettingsHelpers.h"

#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "PCG/PCGIDTextureData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGIDTextureSampler)

#define LOCTEXT_NAMESPACE "PCGIDTextureSamplerElement"

using namespace IDTextureFixedName;

TArray<FPCGPinProperties> UPCGIDTextureSamplerSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(OutNameDistrict1, EPCGDataType::Texture);
	Properties.Emplace(OutNameDistrict2, EPCGDataType::Texture);
	Properties.Emplace(OutNameDistrict3, EPCGDataType::Texture);
	Properties.Emplace(OutNameDistrict4, EPCGDataType::Texture);
	Properties.Emplace(OutNameDistrict5, EPCGDataType::Texture);
	Properties.Emplace(OutNameDistrict6, EPCGDataType::Texture);
	Properties.Emplace(OutNameDistrict7, EPCGDataType::Texture);
	Properties.Emplace(OutNameDistrict8, EPCGDataType::Texture);
	Properties.Emplace(OutNameDistrict9, EPCGDataType::Texture);
	Properties.Emplace(OutNameDistrict10, EPCGDataType::Texture);
	Properties.Emplace(OutNameDistrict11, EPCGDataType::Texture);
	Properties.Emplace(OutNameDistrict12, EPCGDataType::Texture);
	Properties.Emplace(OutNameDistrict13, EPCGDataType::Texture);
	Properties.Emplace(OutNameDistrict14, EPCGDataType::Texture);
	Properties.Emplace(OutNameDistrict15, EPCGDataType::Texture);
	Properties.Emplace(OutNameDistrict16, EPCGDataType::Texture);
	return Properties;
}

FPCGElementPtr UPCGIDTextureSamplerSettings::CreateElement() const
{
	return MakeShared<FPCGIDTextureSamplerElement>();
}

bool FPCGIDTextureSamplerElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGIDTextureSamplerElement::Execute);

	const UPCGIDTextureSamplerSettings* Settings = Context->GetInputSettings<UPCGIDTextureSamplerSettings>();
	check(Settings);

	if (!Settings->IDTexture1)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("IDTexture1IsNull", "IDTexture1 is Null"));
		return true;
	}
	if (!Settings->IDTexture2)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("IDTexture2IsNull", "IDTexture2 is Null"));
		return true;
	}

	if (!UPCGIDTextureData::IsSupported(Settings->IDTexture1) || !UPCGIDTextureData::IsSupported(Settings->IDTexture2))
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("UnsupportedTextureFormat",
			         "Texture has unsupported texture format, currently supported formats are FloatRGBA (Half float)."
		         ));
		return true;
	}

	const FTransform& Transform = Settings->Transform;
	const bool bUseAbsoluteTransform = Settings->bUseAbsoluteTransform;
	const EPCGIDTextureDensityFunction DensityFunction = Settings->DensityFunction;
	const float TexelSize = Settings->TexelSize;

	AActor* OriginalActor = UPCGBlueprintHelpers::GetOriginalComponent(*Context)->GetOwner();
	FTransform FinalTransform = Transform;
	if (!bUseAbsoluteTransform)
	{
		FTransform OriginalActorTransform = OriginalActor->GetTransform();
		FinalTransform = Transform * OriginalActorTransform;

		FBox OriginalActorLocalBounds = PCGHelpers::GetActorLocalBounds(OriginalActor);
		FinalTransform.SetScale3D(
			FinalTransform.GetScale3D() * 0.5 * (OriginalActorLocalBounds.Max - OriginalActorLocalBounds.Min));
	}

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	TSharedRef<FIDTextueData> OriginalIDTextueData = CreateOriginalIDTextureData(
		Settings->IDTexture1, Settings->IDTexture2);
	for (int32 ID = 1; ID <= 16; ++ID)
	{
		FPCGTaggedData& Output = Outputs.Emplace_GetRef();
		Output.Pin = FName(FString::Printf(TEXT("District%d"), ID));
		UPCGIDTextureData* TextureData = NewObject<UPCGIDTextureData>();
		Output.Data = TextureData;
		// Initialize & set properties
		TextureData->Initialize(OriginalIDTextueData, FinalTransform);
		TextureData->PrimaryID = ID;
		TextureData->DensityFunction = DensityFunction;
		TextureData->TexelSize = TexelSize;
		UPCGMetadata* Metadata = TextureData->MutableMetadata();
		Metadata->CreateInteger32Attribute(DataAttrPrimaryID, 0, false, true);
		Metadata->CreateInteger32Attribute(DataAttrDistrictID1, 0, false, true);
		Metadata->CreateFloatAttribute(DataAttrProportion1, 0.f, false, true);
		Metadata->CreateInteger32Attribute(DataAttrDistrictID2, 0, false, true);
		Metadata->CreateFloatAttribute(DataAttrProportion2, 0.f, false, true);
		Metadata->CreateInteger32Attribute(DataAttrDistrictID3, 0, false, true);
		Metadata->CreateFloatAttribute(DataAttrProportion3, 0.f, false, true);
		Metadata->CreateInteger32Attribute(DataAttrDistrictID4, 0, false, true);
		Metadata->CreateFloatAttribute(DataAttrProportion4, 0.f, false, true);
		if (!TextureData->IsValid())
		{
			PCGE_LOG(Error, GraphAndLog,
			         LOCTEXT("TextureDataInitFailed",
				         "Texture data failed to initialize, check log for more information"));
		}
	}

	return true;
}

TSharedRef<FIDTextueData> FPCGIDTextureSamplerElement::CreateOriginalIDTextureData(const UTexture2D* Texture1,
	const UTexture2D* Texture2)
{
	const int32 PixelCount = Texture1->GetSizeX() * Texture1->GetSizeY();
	TSharedRef<FIDTextueData, ESPMode::ThreadSafe> Result = MakeShared<FIDTextueData>();
	FIDTextueData& TextureData = Result.Get();
	TextureData.Width = Texture1->GetSizeX();
	TextureData.Height = Texture1->GetSizeY();
	TextureData.Data.SetNum(PixelCount);
	const FTexturePlatformData* PlatformData1 = Texture1->GetPlatformData();
	if (PlatformData1)
	{
		if (const FFloat16* BulkData = static_cast<const FFloat16*>(PlatformData1->Mips[0].BulkData.LockReadOnly()))
		{
			for (int32 D = 0; D < PixelCount; ++D)
			{
				TextureData.Data[D].DistrictID1 = FMath::RoundHalfToEven((BulkData + D * 4 + 0)->GetFloat() * 16);
				TextureData.Data[D].Proportion1 = (BulkData + D * 4 + 1)->GetFloat();
				TextureData.Data[D].DistrictID2 = FMath::RoundHalfToEven((BulkData + D * 4 + 2)->GetFloat() * 16);
				TextureData.Data[D].Proportion2 = (BulkData + D * 4 + 3)->GetFloat();
			}
		}
	}
	PlatformData1->Mips[0].BulkData.Unlock();
	const FTexturePlatformData* PlatformData2 = Texture2->GetPlatformData();
	if (PlatformData2)
	{
		if (const FFloat16* BulkData = static_cast<const FFloat16*>(PlatformData2->Mips[0].BulkData.LockReadOnly()))
		{
			for (int32 D = 0; D < PixelCount; ++D)
			{
				TextureData.Data[D].DistrictID3 = FMath::RoundHalfToEven((BulkData + D * 4 + 0)->GetFloat() * 16);
				TextureData.Data[D].Proportion3 = (BulkData + D * 4 + 1)->GetFloat();
				TextureData.Data[D].DistrictID4 = FMath::RoundHalfToEven((BulkData + D * 4 + 2)->GetFloat() * 16);
				TextureData.Data[D].Proportion4 = (BulkData + D * 4 + 3)->GetFloat();
			}
		}
	}
	PlatformData2->Mips[0].BulkData.Unlock();
	
	return Result;
}

void FPCGIDTextureSamplerElement::GetDependenciesCrc(const FPCGDataCollection& InInput, const UPCGSettings* InSettings,
                                                     UPCGComponent* InComponent, FPCGCrc& OutCrc) const
{
	FPCGCrc Crc;
	IPCGElement::GetDependenciesCrc(InInput, InSettings, InComponent, Crc);

	if (const UPCGIDTextureSamplerSettings* Settings = Cast<UPCGIDTextureSamplerSettings>(InSettings))
	{
		// If not using absolute transform, depend on actor transform and bounds, and therefore take dependency on actor data.
		bool bUseAbsoluteTransform;
		PCGSettingsHelpers::GetOverrideValue(InInput, Settings,
		                                     GET_MEMBER_NAME_CHECKED(UPCGIDTextureSamplerSettings,
		                                                             bUseAbsoluteTransform),
		                                     Settings->bUseAbsoluteTransform, bUseAbsoluteTransform);
		if (!bUseAbsoluteTransform && InComponent)
		{
			if (const UPCGData* Data = InComponent->GetActorPCGData())
			{
				Crc.Combine(Data->GetOrComputeCrc(/*bFullDataCrc=*/false));
			}
		}
	}

	OutCrc = Crc;
}

#undef LOCTEXT_NAMESPACE
