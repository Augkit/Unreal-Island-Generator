// Fill out your copyright notice in the Description page of Project Settings.


#include "District/IslandScatterDistrict.h"
#include "TriangleDualMesh.h"

void UIslandScatterDistrict::ScatterDistrictStarts(TArray<FPointIndex>& DistrictStarts, UTriangleDualMesh* Mesh,
                                                   const TArray<bool>& OceanRegions,
                                                   FRandomStream& Rng) const
{
	TSet<int32> IslandRegionSet;
	for (int32 RegionIndex = 0; RegionIndex < Mesh->NumRegions; ++RegionIndex)
	{
		if (!OceanRegions[RegionIndex])
		{
			IslandRegionSet.Add(RegionIndex);
		}
	}
	TArray<int32> IslandRegions = IslandRegionSet.Array();
	int32 RandomTimes = DistrictAmount;
	while (RandomTimes > 0)
	{
		--RandomTimes;
		DistrictStarts.Add(IslandRegions[Rng.RandRange(0, IslandRegionSet.Num() - 1)]);
	}
}
