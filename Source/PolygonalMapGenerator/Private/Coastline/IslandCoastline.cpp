// Fill out your copyright notice in the Description page of Project Settings.


#include "Coastline/IslandCoastline.h"
#include "IslandMapData.h"
#include "PolyPartitionHelper.h"

void UIslandCoastline::Initialize(const UTriangleDualMesh* Mesh, TArray<bool> OceanRegions, TArray<bool> CoastRegions)
{
	TMap<FTriangleIndex, FRegionEdge*> BMap;
	for (FPointIndex PointIndex(0); PointIndex < Mesh->NumSolidRegions; ++PointIndex)
	{
		if (!CoastRegions[PointIndex])
		{
			continue;
		}
		TArray<FSideIndex> Sides = Mesh->r_circulate_s(PointIndex);
		for (const FSideIndex& Side : Sides)
		{
			FPointIndex OuterRegion = Mesh->s_end_r(Side);
			if (!OceanRegions[OuterRegion])
			{
				continue;
			}
			const FTriangleIndex AIndex = Mesh->s_inner_t(Side);
			const FTriangleIndex BIndex = Mesh->s_outer_t(Side);
			TSharedPtr<FRegionEdge> Edge = MakeShared<FRegionEdge>(
				AIndex, Mesh->t_pos(AIndex), BIndex, Mesh->t_pos(BIndex));
			Edges.Add(Edge);
			BMap.Add(BIndex, Edge.Get());
		}
	}

	for (const TSharedPtr<FRegionEdge>& Edge : Edges)
	{
		FRegionEdge* PrevEdge = BMap.FindChecked(Edge->AIndex);
		if (!PrevEdge->Next.IsValid())
		{
			PrevEdge->Next = Edge;
		}
	}
	TMap<FRegionEdge*, int32> IslandIdMap;
	IslandIdMap.Empty(Edges.Num());
	for (const TSharedPtr<FRegionEdge>& Edge : Edges)
	{
		if (IslandIdMap.Contains(Edge.Get()))
			continue;
		Coastlines.Add(FCoastlinePolygon());
		FCoastlinePolygon& Coastline = Coastlines.Last();
		Coastline.IslandId = Edge->AIndex;
		Coastline.Begin = Edge;
		TWeakPtr<FRegionEdge> NowEdge = Edge;
		int32 EdgeNum = 0;
		while (true)
		{
			++EdgeNum;
			IslandIdMap.Add(NowEdge.Pin().Get(), Edge->AIndex);
			TWeakPtr<FRegionEdge> NextEdge = NowEdge.Pin()->Next;
			if (NextEdge == Edge)
			{
				Coastline.End = NowEdge;
				break;
			}
			NowEdge = NextEdge;
		}
		NowEdge = Coastline.Begin;
		Coastline.Indices.Empty(EdgeNum);
		Coastline.Positions.Empty(EdgeNum);
		while (true)
		{
			Coastline.Indices.Add(NowEdge.Pin()->AIndex);
			Coastline.Positions.Add(NowEdge.Pin()->APosition);
			TWeakPtr<FRegionEdge> NextEdge = NowEdge.Pin()->Next;
			if (NextEdge == Coastline.Begin)
				break;
			NowEdge = NextEdge;
		}
	}

	// Triangulate
	for (FCoastlinePolygon& Coastline : Coastlines)
	{
		UIslandMapUtils::TriangulateContour(Coastline, Coastline.Triangles);
	}
}

const TArray<FCoastlinePolygon>& UIslandCoastline::GetCoastlines() const
{
	return Coastlines;
}
