// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshActor.h"
#include "IslandMapData.h"
#include "IslandTexture.h"
#include "GeometryScript/MeshVoxelFunctions.h"
#include "IslandDynamicMeshActor.generated.h"

UCLASS(BlueprintType, Blueprintable)
class POLYGONALMAPGENERATOR_API AIslandDynamicMeshActor : public ADynamicMeshActor
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	UIslandMapData* MapData;

	UPROPERTY()
	UIslandTexture* IslandTexture;

	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Island Mesh|Border")
	float BorderOffset = 500;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Island Mesh|Border")
	float BorderDepth = 500;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Island Mesh|Material")
	int32 TextureWidth = 512;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Island Mesh|Material")
	int32 TextureHeight = 512;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Island Mesh|Material")
	UMaterial* IslandMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FGeometryScriptSolidifyOptions SolidifyOptions;

	UFUNCTION(BlueprintCallable)
	void SetMapData(UIslandMapData* InMapData);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	UIslandMapData* GetMapData();

	UFUNCTION(BlueprintCallable)
	void GenerateIsland();

	UFUNCTION(BlueprintCallable)
	void GetDistrictTextures(UTexture2D*& Texture1, UTexture2D*& Texture2);

protected:
	void GenerateIslandMesh();

	void GenerateIslandTexture();

	static void TriangulateRing(TArray<FIntVector>& Triangles, const TArray<FVector2D>& OuterPoly,
	                            const TArray<FVector2D>& InnerPoly);
};
