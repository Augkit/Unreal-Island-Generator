// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "District/IslandDistrict.h"
#include "IslandScatterDistrict.generated.h"

UCLASS()
class POLYGONALMAPGENERATOR_API UIslandScatterDistrict : public UIslandDistrict
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Point", meta = (ClampMin = "0"))
	int32 DistrictAmount = 10;

protected:
	virtual void ScatterDistrictStarts(TArray<FPointIndex>& DistrictStarts, UTriangleDualMesh* Mesh,
	                                                  const TArray<bool>& OceanRegions,
	                                                  FRandomStream& Rng) const override;
};
