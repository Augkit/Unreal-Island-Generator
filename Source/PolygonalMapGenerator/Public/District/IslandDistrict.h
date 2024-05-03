// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "IslandMapUtils.h"
#include "PolyPartitionHelper.h"
#include "Engine/DataAsset.h"
#include "IslandDistrict.generated.h"

USTRUCT()
struct POLYGONALMAPGENERATOR_API FDistrictRegion : public FAreaContour
{
	GENERATED_BODY()
	int32 District;
	TArray<FPolyTriangle2D> Triangles;
};

UCLASS(Blueprintable)
class POLYGONALMAPGENERATOR_API UIslandDistrict : public UDataAsset
{
	GENERATED_BODY()

public:
	virtual void AssignDistrict(TArray<FDistrictRegion>& DistrictRegions, UTriangleDualMesh* Mesh,
	                            const TArray<bool>& OceanRegions, FRandomStream& Rng) const;

protected:
	virtual void ScatterDistrictStarts(TArray<FPointIndex>& DistrictStarts, UTriangleDualMesh* Mesh,
	                                   const TArray<bool>& OceanRegions, FRandomStream& Rng) const;

	virtual void FillDistricts(TArray<int32>& DistrictRegions, UTriangleDualMesh* Mesh,
	                           const TArray<FPointIndex>& DistrictStarts,
	                           const TArray<bool>& OceanRegions) const;
};
