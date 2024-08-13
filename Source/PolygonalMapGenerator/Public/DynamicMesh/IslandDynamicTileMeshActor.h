#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshActor.h"
#include "IslandDynamicAssets.h"
#include "GameFramework/Actor.h"
#include "GeometryScript/CollisionFunctions.h"
#include "IslandDynamicTileMeshActor.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogIslandDynamicActor, Log, All);

UCLASS(Blueprintable, BlueprintType)
class POLYGONALMAPGENERATOR_API AIslandDynamicTileMeshActor : public AActor
{
	GENERATED_BODY()

public:
	AIslandDynamicTileMeshActor();

protected:
	UPROPERTY(BlueprintReadWrite, Category="Assets")
	TObjectPtr<UIslandDynamicAssets> Assets;

	UPROPERTY(BlueprintReadWrite, Category="Tiles")
	TArray<TObjectPtr<ADynamicMeshActor>> TileActors;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transform")
	float MaxSpawnTileTickTime = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transform")
	FVector2D Pivot = FVector2D(.5f, .5f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	TObjectPtr<UMaterial> IslandMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	FName DistrictIDTexture01ParamName = FName(TEXT("District ID 01"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	FName DistrictIDTexture02ParamName = FName(TEXT("District ID 02"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generate Mesh")
	bool bGenerateCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generate Mesh",
		meta = ( EditCondition = "bGenerateCollision" ))
	FGeometryScriptCollisionFromMeshOptions GenerateCollisionOptions;

	void AsyncGenerateDynamicMesh(UIslandDynamicAssets* InAssets);

protected:
	int32 CompletedTilesCount = 0;
	virtual void CheckIfAllTilesAreCompleted();

	int32 SpawnedTileActorsCount = 0;
	TQueue<int32> TileToSpawnQueue;

public:
	virtual void Tick(float DeltaSeconds) override;

protected:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Generate Mesh")
	void PostGenerateIsland(bool bSucceed);
};
