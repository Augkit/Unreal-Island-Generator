// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "IslandMapData.h"
#include "UObject/NoExportTypes.h"
#include "IslandTexture.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, Blueprintable)
class POLYGONALMAPGENERATOR_API UIslandTexture : public UObject
{
	GENERATED_BODY()

public:

	UIslandTexture() = default;

	UIslandTexture(UIslandMapData* InMapData) : MapData(InMapData)
	{
		
	};
	
	UPROPERTY(BlueprintReadWrite)
	UIslandMapData* MapData;

	UPROPERTY(BlueprintReadOnly)
	UTexture2D* DistrictIDTexture01;
	UPROPERTY(BlueprintReadOnly)
	UTexture2D* DistrictIDTexture02;

	UPROPERTY(BlueprintReadWrite)
	UCanvasRenderTarget2D* MeshTexture;

	UFUNCTION(BlueprintCallable)
	void DrawDistrictIDTexture(UTexture2D*& IDTexture01, UTexture2D*& IDTexture02, int32 Width = 512, int32 Height = 512);

protected:
	void DrawDistrictIDTexture_Internal(int32 Width = 512, int32 Height = 512);
};
