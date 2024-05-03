// Fill out your copyright notice in the Description page of Project Settings.


#include "District/IslandPoissonDistrict.h"
#include "TriangleDualMesh.h"
#include "DelaunayHelper.h"
#include "RandomSampling/PoissonDiscUtilities.h"

void UIslandPoissonDistrict::ScatterDistrictStarts(TArray<FPointIndex>& DistrictStarts,
                                                   UTriangleDualMesh* Mesh,
                                                   const TArray<bool>& OceanRegions,
                                                   FRandomStream& Rng) const
{
	TArray<FVector2D> RegionPositions = Mesh->GetPoints();
	FVector4 Border(DBL_MAX, DBL_MAX, DBL_MIN, DBL_MIN);
	for (int32 RegionIndex = 0; RegionIndex < RegionPositions.Num(); ++RegionIndex)
	{
		if (Mesh->r_ghost(RegionIndex) || OceanRegions[RegionIndex])
			continue;
		const FVector2D& Pos = RegionPositions[RegionIndex];
		if (Border.X > Pos.X)
		{
			Border.X = Pos.X;
		}
		if (Border.Y > Pos.Y)
		{
			Border.Y = Pos.Y;
		}
		if (Border.Z < Pos.X)
		{
			Border.Z = Pos.X;
		}
		if (Border.W < Pos.X)
		{
			Border.W = Pos.X;
		}
	}
	const FVector2D ValidMapSize = FVector2D(Border.Z - Border.X, Border.W - Border.Y);
	TArray<FVector2D> Points;
	UPoissonDiscUtilities::Distribute2D(
		Points,
		Rng.GetCurrentSeed(),
		ValidMapSize,
		FVector2D(ValidMapSize.X, ValidMapSize.Y),
		FMath::Min(ValidMapSize.X, ValidMapSize.Y) * DistrictDistanceRate,
		3
	);
	TSet<FPointIndex> RegionFilter;
	for (const FVector2D& Point : Points)
	{
		FPointIndex PointIndex = Mesh->ClosestRegion(Point);
		if (Mesh->r_ghost(PointIndex) || OceanRegions[PointIndex])
		{
			continue;
		}
		RegionFilter.Add(PointIndex);
	}
	DistrictStarts.Append(RegionFilter.Array());
}
