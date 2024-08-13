#pragma once

#include "CoreMinimal.h"
#include "IslandMapData.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "UObject/Object.h"
#include "IslandDynamicAssets.generated.h"

USTRUCT()
struct FDynamicTileInfo
{
	GENERATED_BODY()

	FDynamicTileInfo()
	{
	}

	FDynamicTileInfo(const FGraphEventRef& InTask) : Task(InTask)
	{
	}

	FGraphEventRef Task;
	int32 TileRow = 0;
	int32 TileCol = 0;
	FVector2D TileCenter;
	FGeometryScriptSimpleMeshBuffers Buffers;
};

UCLASS(Blueprintable, BlueprintType, EditInlineNew)
class POLYGONALMAPGENERATOR_API UIslandDynamicAssets : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category="MapData")
	UIslandMapData* MapData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="District")
	int32 DistrictIDTextureWidth = 4096;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="District")
	int32 DistrictIDTextureHeight = 4096;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grid")
	int32 TileDivisions = 9;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grid")
	int32 TileResolution = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Border")
	float BorderOffset = 500;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Border")
	float BorderDepth = 500;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Border")
	TObjectPtr<UCurveFloat> BorderDepthRemapCurve;

	FGraphEventRef GenerateMapDataTask;

	FGraphEventRef GenDistrictIDTextureTask;

	TArray<FDynamicTileInfo> TileInfo;

	void AsyncGenerateAssets();

protected:
	UPROPERTY(BlueprintReadWrite)
	UTexture2D* DistrictIDTexture01;
	UPROPERTY(BlueprintReadWrite)
	UTexture2D* DistrictIDTexture02;

	FGraphEventRef AsyncGenerateDistrictIDTexture(const FGraphEventArray& Prerequisites);

	void CalcTileMeshBuffer(const int32 GridIndex);

public:
	UFUNCTION(BlueprintCallable, Category="MapData")
	FORCEINLINE int32 GetTileAmount() const;

	FORCEINLINE FGraphEventArray GetTileTasks() const;
	
	UFUNCTION(BlueprintCallable, Category="Texture")
	FORCEINLINE UTexture2D* GetDistrictIDTexture01() const
	{
		return DistrictIDTexture01;
	}

	UFUNCTION(BlueprintCallable, Category="Texture")
	FORCEINLINE UTexture2D* GetDistrictIDTexture02() const
	{
		return DistrictIDTexture02;
	}

};
