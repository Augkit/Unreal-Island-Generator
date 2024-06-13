// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "IslandDynamicMeshActorBase.h"
#include "IslandMapUtils.h"
#include "GeometryScript/MeshVoxelFunctions.h"
#include "IslandDynamicMeshActor.generated.h"

DECLARE_CYCLE_STAT(TEXT("Generate District ID Texture"), STAT_GenerateDistrictIDTexture, STATGROUP_IslandDynamicMesh);

UENUM(BlueprintType)
enum class EGenerateMeshType : uint8
{
	GMT_Delaunator UMETA(DisplayName="Delaunator"),
	GMT_Voxelization UMETA(DisplayName="Voxelization"),
	GMT_PixelMesh UMETA(DisplayName="Pixel Mesh"),
};

UENUM(BlueprintType)
enum class EDelaunatorBorderProcess : uint8
{
	DBP_StepDiffusion UMETA(DisplayName="Step Diffusion"),
	DBP_StepTwoWay UMETA(DisplayName="Step Two-way"),
};

UCLASS(BlueprintType, Blueprintable)
class POLYGONALMAPGENERATOR_API AIslandDynamicMeshActor : public AIslandDynamicMeshActorBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "District ID Texture")
	int32 DistrictIDTextureWidth = 512;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "District ID Texture")
	int32 DistrictIDTextureHeight = 512;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generate Mesh")
	EGenerateMeshType GenerateMeshMethod = EGenerateMeshType::GMT_Delaunator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generate Mesh",
		meta = (ClampMin = 1, EditCondition = "GenerateMeshMethod == EGenerateMeshType::GMT_PixelMesh"))
	int32 MeshPixelWidth = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generate Mesh",
		meta = (ClampMin = 1, EditCondition = "GenerateMeshMethod == EGenerateMeshType::GMT_PixelMesh"))
	int32 MeshPixelHeight = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generate Mesh|Border")
	float BorderOffset = 500;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generate Mesh|Border")
	float BorderDepth = 500;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Generate Mesh|Border",
		meta = (
			ClampMin = 1, EditCondition =
			"GenerateMeshMethod == EGenerateMeshType::GMT_Delaunator || GenerateMeshMethod == EGenerateMeshType::GMT_PixelMesh"
		)
	)
	ERemapType BorderDepthRemapMethod = ERemapType::RT_Linear;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Generate Mesh|Border",
		meta = (
			ClampMin = 1,
			EditCondition = "GenerateMeshMethod == EGenerateMeshType::GMT_Delaunator"
		)
	)
	int32 BorderTessellationTimes = 5;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Generate Mesh|Border",
		meta = (
			ClampMin = 1,
			EditCondition = "GenerateMeshMethod == EGenerateMeshType::GMT_Delaunator"
		)
	)
	EDelaunatorBorderProcess DelaunatorBorderProcessMethod = EDelaunatorBorderProcess::DBP_StepDiffusion;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Generate Mesh|Border",
		meta = (
			ClampMin = 0,
			EditCondition =
			"GenerateMeshMethod == EGenerateMeshType::GMT_Delaunator && DelaunatorBorderProcessMethod == EDelaunatorBorderProcess::DBP_StepDiffusion"
		)
	)
	int32 BorderTessellationStartStep = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generate Mesh",
		meta = (
			ClampMin = 0, EditCondition =
			"GenerateMeshMethod == EGenerateMeshType::GMT_Delaunator || GenerateMeshMethod == EGenerateMeshType::GMT_Voxelization"
		)
	)
	int32 TessellationLevel = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FGeometryScriptSolidifyOptions SolidifyOptions;

	UPROPERTY(BlueprintReadOnly)
	UTexture2D* DistrictIDTexture01;
	UPROPERTY(BlueprintReadOnly)
	UTexture2D* DistrictIDTexture02;

protected:
	virtual void GenerateIslandTexture() override;
	virtual void GenerateIslandMesh() override;
	virtual void GenerateMeshDelaunator(UDynamicMesh* DynamicMesh);
	virtual void GenerateMeshVoxelization(UDynamicMesh* DynamicMesh);
	virtual void GenerateMeshPixel(UDynamicMesh* DynamicMesh);

	virtual void SetMaterialParameters(UMaterialInstanceDynamic* MaterialInstance) override;

	static void TriangulateRing(TArray<FIntVector>& Triangles,
	                            const TArray<FVector2D>& OuterPoly, const TArray<int32>& OuterPolyIds,
	                            const TArray<FVector2D>& InnerPoly, const TArray<int32>& InnerPolyIds
	);
	static void TriangulateRing(TArray<FIntVector>& Triangles, const TArray<FVector2D>& OuterPoly,
	                            const TArray<FVector2D>& InnerPoly);

	static void SubdivisionPolygon(TArray<FVector2D>& OutResult, const TArray<FVector2D>& Polygon);
};
