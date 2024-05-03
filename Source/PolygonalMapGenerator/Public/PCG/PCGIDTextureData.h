// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGIDTextureSampler.h"
#include "Data/PCGSurfaceData.h"

#include "PCGIDTextureData.generated.h"

/**
 * 
 */
UCLASS()
class POLYGONALMAPGENERATOR_API UPCGIDTextureData : public UPCGSurfaceData
{
	GENERATED_BODY()

public:
	// ~Begin UPCGData interface
	// virtual EPCGDataType GetDataType() const override { return EPCGDataType::BaseTexture; }
	// ~End UPCGData interface

	//~Begin UPCGSpatialData interface
	virtual FBox GetBounds() const override;
	virtual FBox GetStrictBounds() const override;
	virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint,
	                         UPCGMetadata* OutMetadata) const override;
	//~End UPCGSpatialData interface

	//~Begin UPCGSpatialDataWithPointCache interface
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context) const override;
	//~End UPCGSpatialDataWithPointCache interface

	virtual bool IsValid() const;

public:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = SpatialData)
	int32 PrimaryID;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = SpatialData)
	EPCGIDTextureDensityFunction DensityFunction = EPCGIDTextureDensityFunction::Multiply;

	/** The size of one texel in cm, used when calling ToPointData. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (UIMin = "1.0", ClampMin = "1.0"))
	float TexelSize = 50.0f;

protected:

	TSharedPtr<FIDTextueData, ESPMode::ThreadSafe> TextureData;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = SpatialData)
	FBox Bounds = FBox(EForceInit::ForceInit);

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = SpatialData)
	int32 Height = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = SpatialData)
	int32 Width = 0;

	void CopyBaseTextureData(UPCGIDTextureData* NewTextureData) const;
	
public:
	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Texture; }
	// ~End UPCGData interface

	void Initialize(const TSharedPtr<FIDTextueData, ESPMode::ThreadSafe>& InTextureData, const FTransform& InTransform);

	/** Returns true if the format of InTexture is compatible and can be loaded. Will load texture if not already loaded. */
	static bool IsSupported(UTexture2D* InTexture);

	//~Begin UPCGSpatialData interface
protected:
	virtual UPCGSpatialData* CopyInternal() const override;
	//~End UPCGSpatialData interface

};
