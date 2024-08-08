// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshActor.h"
#include "GeometryScript/CollisionFunctions.h"
#include "IslandMapData.h"
#include "IslandDynamicMeshActorBase.generated.h"

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generate Mesh")
	bool bGenerateCollision = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generate Mesh", meta = ( EditCondition = "bGenerateCollision" ))
	FGeometryScriptCollisionFromMeshOptions GenerateCollisionOptions;

	UFUNCTION(BlueprintCallable)
	UIslandMapData* SetMapData(UIslandMapData* InMapData);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	UIslandMapData* GetMapData();

	UFUNCTION(BlueprintCallable, Category = "Generate Mesh")
	virtual bool GenerateIsland(UIslandMapData* InMapData, const FTransform& Transform);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Generate Mesh")
	void PostGenerateIsland(bool bSucceed);

protected:
	virtual void GenerateIslandTexture();
	virtual void GenerateIslandMesh(UDynamicMesh* DynamicMesh, const FTransform& Transform);
	virtual void SetMaterialParameters(UMaterialInstanceDynamic* MaterialInstance);

	virtual void PostGenerateIsland_Implementation(bool bSucceed);
};
