// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DelaunayHelper.h"
#include "PolyPartitionHelper.h"
#include "IslandMapUtils.h"
#include "IslandCoastline.generated.h"

USTRUCT(BlueprintType, Blueprintable)
struct POLYGONALMAPGENERATOR_API FCoastlinePolygon : public FAreaContour
{
	GENERATED_BODY()
	SIZE_T IslandId;
	TArray<FPolyTriangle2D> Triangles;
};

UCLASS()
class POLYGONALMAPGENERATOR_API UIslandCoastline : public UObject
{
	GENERATED_BODY()
protected:
	TArray<FCoastlinePolygon> Coastlines;
	TArray<TSharedPtr<FRegionEdge>> Edges;

public:
	void Initialize(const UTriangleDualMesh* Mesh, TArray<bool> OceanRegions, TArray<bool> CoastRegions);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	const TArray<FCoastlinePolygon>& GetCoastlines() const;
};
