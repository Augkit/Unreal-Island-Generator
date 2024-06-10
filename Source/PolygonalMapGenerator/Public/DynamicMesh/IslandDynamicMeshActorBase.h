// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshActor.h"
#include "IslandDynamicMeshActorBase.generated.h"

DECLARE_STATS_GROUP(TEXT("Island Dynamic Mesh"), STATGROUP_IslandDynamicMesh, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Generate Dynamic Mesh"), STAT_GenerateDynamicMesh, STATGROUP_IslandDynamicMesh);
DECLARE_CYCLE_STAT(TEXT("Generate Island Texture"), STAT_GenerateIslandTexture, STATGROUP_IslandDynamicMesh);

UCLASS(BlueprintType, Blueprintable)
class POLYGONALMAPGENERATOR_API AIslandDynamicMeshActorBase : public ADynamicMeshActor
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	UIslandMapData* MapData;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	UMaterial* IslandMaterial;

	UFUNCTION(BlueprintCallable)
	void SetMapData(UIslandMapData* InMapData);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	UIslandMapData* GetMapData();

	UFUNCTION(BlueprintCallable)
	virtual void GenerateIsland(UIslandMapData* InMapData);

protected:
	virtual void GenerateIslandTexture();
	virtual void GenerateIslandMesh();
	virtual void SetMaterialParameters(UMaterialInstanceDynamic* MaterialInstance);
};
