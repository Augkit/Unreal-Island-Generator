// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGPin.h"

#include "PCGIDTextureSampler.generated.h"

UENUM(BlueprintType)
enum class EPCGIDTextureDensityFunction : uint8
{
	Ignore,
	Multiply
};
struct FPixelData
{
	int32 DistrictID1;
	float Proportion1;
	int32 DistrictID2;
	float Proportion2;
	int32 DistrictID3;
	float Proportion3;
	int32 DistrictID4;
	float Proportion4;
};

struct FIDTextueData
{
	int32 Width;
	int32 Height;
	TArray<FPixelData> Data;
};

namespace IDTextureFixedName
{
const FName OutNameDistrict1 = FName(TEXT("District1"));
const FName OutNameDistrict2 = FName(TEXT("District2"));
const FName OutNameDistrict3 = FName(TEXT("District3"));
const FName OutNameDistrict4 = FName(TEXT("District4"));
const FName OutNameDistrict5 = FName(TEXT("District5"));
const FName OutNameDistrict6 = FName(TEXT("District6"));
const FName OutNameDistrict7 = FName(TEXT("District7"));
const FName OutNameDistrict8 = FName(TEXT("District8"));
const FName OutNameDistrict9 = FName(TEXT("District9"));
const FName OutNameDistrict10 = FName(TEXT("District10"));
const FName OutNameDistrict11 = FName(TEXT("District11"));
const FName OutNameDistrict12 = FName(TEXT("District12"));
const FName OutNameDistrict13 = FName(TEXT("District13"));
const FName OutNameDistrict14 = FName(TEXT("District14"));
const FName OutNameDistrict15 = FName(TEXT("District15"));
const FName OutNameDistrict16 = FName(TEXT("District16"));

const FName DataAttrPrimaryID = FName(TEXT("PrimaryID"));
const FName DataAttrDistrictID1 = FName(TEXT("DistrictID1"));
const FName DataAttrDistrictID2 = FName(TEXT("DistrictID2"));
const FName DataAttrDistrictID3 = FName(TEXT("DistrictID3"));
const FName DataAttrDistrictID4 = FName(TEXT("DistrictID4"));
const FName DataAttrProportion1 = FName(TEXT("Proportion1"));
const FName DataAttrProportion2 = FName(TEXT("Proportion2"));
const FName DataAttrProportion3 = FName(TEXT("Proportion3"));
const FName DataAttrProportion4 = FName(TEXT("Proportion4"));
}

UCLASS(BlueprintType, ClassGroup = (Procedural))
class POLYGONALMAPGENERATOR_API UPCGIDTextureSamplerSettings : public UPCGSettings
{
	GENERATED_BODY()

public:


	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("GetIDTextureData")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGIDTextureSamplerSettings", "NodeTitle", "Get ID Texture Data"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return TArray<FPCGPinProperties>(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	// Surface transform
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FTransform Transform = FTransform::Identity;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bUseAbsoluteTransform = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	UTexture2D* IDTexture1 = nullptr;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	UTexture2D* IDTexture2 = nullptr;
	
	// Common members in BaseTextureData
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = SpatialData, meta = (PCG_Overridable))
	EPCGIDTextureDensityFunction DensityFunction = EPCGIDTextureDensityFunction::Multiply;

	/** The size of one texel in cm, used when calling ToPointData. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (UIMin = "1.0", ClampMin = "1.0", PCG_Overridable))
	float TexelSize = 50.0f;
};

class FPCGIDTextureSamplerElement : public IPCGElement
{
public:
	virtual void GetDependenciesCrc(const FPCGDataCollection& InInput, const UPCGSettings* InSettings, UPCGComponent* InComponent, FPCGCrc& OutCrc) const override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

public:
	static TSharedRef<FIDTextueData> CreateOriginalIDTextureData(const UTexture2D* Texture1, const UTexture2D* Texture2);
};
