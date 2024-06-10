// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "IslandDynamicMeshActorBase.h"
#include "GeometryScript/MeshVoxelFunctions.h"
#include "IslandDynamicMeshActor.generated.h"

DECLARE_CYCLE_STAT(TEXT("Generate District ID Texture"), STAT_GenerateDistrictIDTexture, STATGROUP_IslandDynamicMesh);

UCLASS(BlueprintType, Blueprintable)
class POLYGONALMAPGENERATOR_API AIslandDynamicMeshActor : public AIslandDynamicMeshActorBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resolution")
	int32 DistrictIDTextureWidth = 512;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resolution")
	int32 DistrictIDTextureHeight = 512;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Border")
	float BorderOffset = 500;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Border")
	float BorderDepth = 500;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FGeometryScriptSolidifyOptions SolidifyOptions;

	UPROPERTY(BlueprintReadOnly)
	UTexture2D* DistrictIDTexture01;
	UPROPERTY(BlueprintReadOnly)
	UTexture2D* DistrictIDTexture02;

protected:
	virtual void GenerateIslandTexture() override;
	virtual void GenerateIslandMesh() override;
	virtual void SetMaterialParameters(UMaterialInstanceDynamic* MaterialInstance) override;

	static void TriangulateRing(TArray<FIntVector>& Triangles, const TArray<FVector2D>& OuterPoly,
	                            const TArray<FVector2D>& InnerPoly);
};
