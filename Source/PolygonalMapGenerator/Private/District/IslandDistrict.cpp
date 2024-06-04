// Fill out your copyright notice in the Description page of Project Settings.

#include "District/IslandDistrict.h"
#include "TriangleDualMesh.h"
#include "DelaunayHelper.h"

void UIslandDistrict::AssignDistrict(TArray<FDistrictRegion>& DistrictRegions, UTriangleDualMesh* Mesh,
                                     const TArray<bool>& OceanRegions, FRandomStream& Rng) const
{
	TArray<FPointIndex> DistrictStarts;
	ScatterDistrictStarts(DistrictStarts, Mesh, OceanRegions, Rng);
	TArray<int32> RegionDistricts;
	FillDistricts(RegionDistricts, Mesh, DistrictStarts, OceanRegions);
	TMap<int32, TMap<FTriangleIndex, TSharedPtr<FRegionEdge>>> DistrictInfos;
	for (int32 RegionIndex = 0; RegionIndex < RegionDistricts.Num(); ++RegionIndex)
	{
		int32 District = RegionDistricts[RegionIndex];
		if (District == -1)
		{
			continue;
		}
		TMap<FTriangleIndex, TSharedPtr<FRegionEdge>>* DistrictEdges = DistrictInfos.Find(District);
		if (DistrictEdges == nullptr)
		{
			DistrictInfos.Add(District, TMap<FTriangleIndex, TSharedPtr<FRegionEdge>>());
			DistrictEdges = DistrictInfos.Find(District);
		}
		TArray<FSideIndex> Sides = Mesh->r_circulate_s(RegionIndex);
		for (const FSideIndex& Side : Sides)
		{
			FPointIndex OuterRegion = Mesh->s_end_r(Side);
			int32 OuterDistrict = RegionDistricts[OuterRegion];
			if (OuterDistrict == District)
			{
				continue;
			}
			const FTriangleIndex AIndex = Mesh->s_inner_t(Side);
			const FTriangleIndex BIndex = Mesh->s_outer_t(Side);
			DistrictEdges->Add(
				BIndex,
				MakeShared<FRegionEdge>(
					AIndex, Mesh->t_pos(AIndex), BIndex, Mesh->t_pos(BIndex)
				)
			);
		}
	}

	DistrictRegions.Empty(DistrictInfos.Num());
	for (TPair<int32, TMap<FTriangleIndex, TSharedPtr<FRegionEdge>>>& DistrictInfo : DistrictInfos)
	{
		TMap<FTriangleIndex, TSharedPtr<FRegionEdge>>& BMap = DistrictInfo.Value;
		int32 EdgeNum = BMap.Num();
		DistrictRegions.Add(FDistrictRegion());
		FDistrictRegion& DistrictRegion = DistrictRegions.Last();
		DistrictRegion.District = DistrictInfo.Key;
		DistrictRegion.Edges.SetNum(EdgeNum);
		for (TPair<FTriangleIndex, TSharedPtr<FRegionEdge>>& EdgeInfo : BMap)
		{
			DistrictRegion.Edges.Add(EdgeInfo.Value);
			if (!DistrictRegion.Begin.IsValid())
				DistrictRegion.Begin = EdgeInfo.Value;
			TSharedPtr<FRegionEdge>& PrevEdge = BMap.FindChecked(EdgeInfo.Value.Get()->AIndex);
			if (PrevEdge == DistrictRegion.Begin)
				DistrictRegion.End = EdgeInfo.Value;
			if (!PrevEdge.Get()->Next.IsValid())
			{
				PrevEdge.Get()->Next = EdgeInfo.Value;
			}
		}
		TWeakPtr<FRegionEdge> NowEdge = DistrictRegion.Begin;
		DistrictRegion.Indices.Empty(EdgeNum);
		DistrictRegion.Positions.Empty(EdgeNum);
		while (true)
		{
			DistrictRegion.Indices.Add(NowEdge.Pin()->AIndex);
			DistrictRegion.Positions.Add(NowEdge.Pin()->APosition);
			TWeakPtr<FRegionEdge> NextEdge = NowEdge.Pin()->Next;
			if (NextEdge == DistrictRegion.Begin)
				break;
			NowEdge = NextEdge;
		}
	}

	// Triangulate
	for (FDistrictRegion& DistrictRegion : DistrictRegions)
	{
		UIslandMapUtils::TriangulateContour(DistrictRegion, DistrictRegion.Triangles);
	}
}

void UIslandDistrict::ScatterDistrictStarts(TArray<FPointIndex>& DistrictStarts,
                                            UTriangleDualMesh* Mesh,
                                            const TArray<bool>& OceanRegions, FRandomStream& Rng) const
{
}

void UIslandDistrict::FillDistricts(TArray<int32>& DistrictRegions, UTriangleDualMesh* Mesh,
                                    const TArray<FPointIndex>& DistrictStarts,
                                    const TArray<bool>& OceanRegions) const
{
	const int32 DistrictNum = DistrictStarts.Num();
	struct FRegionDistrict
	{
		int32 DistrictIndex;
		FPointIndex RegionIndex;
		FRegionDistrict() = default;

		FRegionDistrict(const int32& DI, const FPointIndex& RI) : DistrictIndex(DI), RegionIndex(RI)
		{
		}
	};
	DistrictRegions.Init(-1, OceanRegions.Num());
	TQueue<FRegionDistrict> Regions;
	for (int32 DistrictIndex = 0; DistrictIndex < DistrictNum; ++DistrictIndex)
	{
		DistrictRegions[DistrictStarts[DistrictIndex]] = DistrictIndex;
		Regions.Enqueue(FRegionDistrict(DistrictIndex, DistrictStarts[DistrictIndex]));
	}

	while (!Regions.IsEmpty())
	{
		FRegionDistrict Region;
		Regions.Dequeue(Region);
		TArray<FPointIndex> RoundRegions = Mesh->r_circulate_r(Region.RegionIndex);
		for (const FPointIndex& RegionIndex : RoundRegions)
		{
			if (Mesh->r_ghost(RegionIndex) || OceanRegions[RegionIndex] || DistrictRegions[RegionIndex] != -1)
			{
				continue;
			}
			DistrictRegions[RegionIndex] = Region.DistrictIndex;
			Regions.Enqueue(FRegionDistrict(Region.DistrictIndex, RegionIndex));
		}
	}
}
