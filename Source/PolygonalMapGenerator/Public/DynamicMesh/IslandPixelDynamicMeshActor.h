// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "IslandDynamicMeshActor.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "IslandPixelDynamicMeshActor.generated.h"

DECLARE_CYCLE_STAT(TEXT("Generate Heightmap Texture"), STAT_GenerateHeightmapTexture, STATGROUP_IslandDynamicMesh);

UCLASS(BlueprintType, Blueprintable)
class POLYGONALMAPGENERATOR_API AIslandPixelDynamicMeshActor : public AIslandDynamicMeshActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resolution", meta = (ClampMin = 1))
	int32 MeshPixelWidth = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resolution", meta = (ClampMin = 1))
	int32 MeshPixelHeight = 10;

	virtual void GenerateIslandMesh() override;
};
