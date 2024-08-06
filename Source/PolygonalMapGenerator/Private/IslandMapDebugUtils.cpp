// Fill out your copyright notice in the Description page of Project Settings.


#include "IslandMapDebugUtils.h"

#include "Coastline/IslandCoastline.h"
#include "Engine/Canvas.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"

void UIslandMapDebugUtils::DrawWater(UCanvasRenderTarget2D* RenderTarget2D, const UIslandMapData* MapData)
{
	if (MapData == nullptr)
		return;
	UTriangleDualMesh* Mesh = MapData->Mesh;
	if (Mesh == nullptr)
		return;
	Mesh->GetTriangleCentroids();
	UCanvas* Canvas;
	FVector2D Size;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(MapData->GetWorld(), RenderTarget2D, Canvas, Size,
	                                                       Context);
	if (Canvas == nullptr)
		return;
	const FVector2D Scale = Size / MapData->GetMapSize();

	for (int32 PointIndex = 0; PointIndex < Mesh->NumSolidRegions; ++PointIndex)
	{
		const TArray<FTriangleIndex>& TriangleIndexs = Mesh->r_circulate_t(PointIndex);
		TArray<FVector2D> TrianglePos;
		TrianglePos.Empty(TriangleIndexs.Num());
		for (const FTriangleIndex& TriangleIndex : TriangleIndexs)
		{
			TrianglePos.Add(Mesh->t_pos(TriangleIndex));
		}
		TArray<FCanvasUVTri> CanvasTris;
		CanvasTris.Empty(TrianglePos.Num() - 2);
		FVector2D FirstPos = TrianglePos[0];
		FVector2D SecondPos = TrianglePos[1];
		FLinearColor Color = FLinearColor(0.937, 0.647, 0.451);
		if (MapData->IsPointWater(PointIndex))
		{
			Color = FLinearColor(0.94, 0.29, 0.612);
			if (MapData->IsPointOcean(PointIndex))
			{
				Color = FLinearColor(0.341, 0.549, 0.898);
			}
		}
		for (int32 i = 2; i < TrianglePos.Num(); i++)
		{
			const FVector2D& NextPos = TrianglePos[i % TrianglePos.Num()];
			FCanvasUVTri Tri;
			Tri.V0_Color = Color;
			Tri.V1_Color = Color;
			Tri.V2_Color = Color;
			Tri.V0_Pos = FirstPos * Scale;
			Tri.V1_Pos = SecondPos * Scale;
			Tri.V2_Pos = NextPos * Scale;
			CanvasTris.Add(Tri);
			SecondPos = NextPos;
		}
		Canvas->K2_DrawTriangle(nullptr, CanvasTris);
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(MapData->GetWorld(), Context);
}

void UIslandMapDebugUtils::DrawRegion(UCanvasRenderTarget2D* RenderTarget2D, const UIslandMapData* MapData)
{
	if (MapData == nullptr)
		return;
	UTriangleDualMesh* Mesh = MapData->Mesh;
	if (Mesh == nullptr)
		return;
	Mesh->GetTriangleCentroids();
	UCanvas* Canvas;
	FVector2D Size;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(MapData->GetWorld(), RenderTarget2D, Canvas, Size,
	                                                       Context);
	if (Canvas == nullptr)
		return;
	const FVector2D Scale = Size / MapData->GetMapSize();

	for (int32 PointIndex = 0; PointIndex < Mesh->NumSolidRegions; ++PointIndex)
	{
		const TArray<FTriangleIndex>& TriangleIndexs = Mesh->r_circulate_t(PointIndex);
		TArray<FVector2D> TrianglePos;
		TrianglePos.Empty(TriangleIndexs.Num());
		for (const FTriangleIndex& TriangleIndex : TriangleIndexs)
		{
			TrianglePos.Add(Mesh->t_pos(TriangleIndex));
		}
		TArray<FCanvasUVTri> CanvasTris;
		CanvasTris.Empty(TrianglePos.Num() - 2);
		FVector2D FirstPos = TrianglePos[0];
		FVector2D SecondPos = TrianglePos[1];
		FLinearColor Color = FLinearColor::Gray;
		if (MapData->IsPointWater(PointIndex))
		{
			Color = FLinearColor::Blue;
			if (MapData->IsPointOcean(PointIndex))
			{
				Color = FLinearColor::Black;
			}
		}
		else
		{
			FBiomeData Biome = MapData->GetPointBiome(PointIndex);
			Color = Biome.DebugColor;
		}
		for (int32 i = 2; i < TrianglePos.Num(); i++)
		{
			const FVector2D& NextPos = TrianglePos[i % TrianglePos.Num()];
			FCanvasUVTri Tri;
			Tri.V0_Color = Color;
			Tri.V1_Color = Color;
			Tri.V2_Color = Color;
			Tri.V0_Pos = FirstPos * Scale;
			Tri.V1_Pos = SecondPos * Scale;
			Tri.V2_Pos = NextPos * Scale;
			CanvasTris.Add(Tri);
			SecondPos = NextPos;
		}
		Canvas->K2_DrawTriangle(nullptr, CanvasTris);
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(MapData->GetWorld(), Context);
}

void UIslandMapDebugUtils::DrawCoastline(UCanvasRenderTarget2D* RenderTarget2D, const UIslandMapData* MapData)
{
	if (MapData == nullptr)
		return;

	UTriangleDualMesh* Mesh = MapData->Mesh;
	Mesh->GetTriangleCentroids();
	UCanvas* Canvas;
	FVector2D Size;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(MapData->GetWorld(), RenderTarget2D, Canvas, Size,
	                                                       Context);
	const FVector2D Scale = Size / MapData->GetMapSize();

	for (int32 PointIndex = 0; PointIndex < Mesh->NumSolidRegions; ++PointIndex)
	{
		if (!MapData->IsPointCoast(PointIndex))
		{
			continue;
		}
		TArray<FSideIndex> Sides = Mesh->r_circulate_s(PointIndex);
		for (const FSideIndex& Side : Sides)
		{
			FPointIndex OuterRegion = Mesh->s_end_r(Side);
			if (!MapData->IsPointOcean(OuterRegion))
			{
				continue;
			}
			FTriangleIndex T1 = Mesh->s_inner_t(Side);
			FTriangleIndex T2 = Mesh->s_outer_t(Side);
			FVector2D TPos1 = Mesh->t_pos(T1) * Scale;
			FVector2D TPos2 = Mesh->t_pos(T2) * Scale;
			FVector2D CPos = (TPos1 + TPos2) / 2;
			Canvas->K2_DrawLine(TPos1, CPos, 3, FLinearColor::Green);
			Canvas->K2_DrawLine(CPos, TPos2, 3, FLinearColor::Red);
		}
	}

	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(MapData->GetWorld(), Context);
}

void UIslandMapDebugUtils::DrawTriangulationIsland(UCanvasRenderTarget2D* RenderTarget2D,
                                                   const UIslandMapData* MapData)
{
	if (MapData == nullptr)
		return;

	UTriangleDualMesh* Mesh = MapData->Mesh;
	Mesh->GetTriangleCentroids();
	UCanvas* Canvas;
	FVector2D Size;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(MapData->GetWorld(), RenderTarget2D, Canvas, Size,
	                                                       Context);
	const FVector2D Scale = Size / MapData->GetMapSize();
	TArray<FCanvasUVTri> CanvasTris;

	for (const FCoastlinePolygon& Coastline : MapData->GetCoastLines())
	{
		const FLinearColor Color = FLinearColor::MakeRandomSeededColor(Coastline.Begin.Pin()->AIndex);
		for (const FPolyTriangle2D& Tri : Coastline.Triangles)
		{
			CanvasTris.Add(FCanvasUVTri());
			FCanvasUVTri& CanvasTri = CanvasTris.Last();
			CanvasTri.V0_Color = Color;
			CanvasTri.V1_Color = Color;
			CanvasTri.V2_Color = Color;
			CanvasTri.V0_Pos = Tri.V0 * Scale;
			CanvasTri.V1_Pos = Tri.V1 * Scale;
			CanvasTri.V2_Pos = Tri.V2 * Scale;
		}
	}
	Canvas->K2_DrawTriangle(nullptr, CanvasTris);
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(MapData->GetWorld(), Context);
}

void UIslandMapDebugUtils::DrawDistrict(UCanvasRenderTarget2D* RenderTarget2D, const UIslandMapData* MapData)
{
	if (MapData == nullptr)
		return;

	UCanvas* Canvas;
	FVector2D Size;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(MapData->GetWorld(), RenderTarget2D, Canvas, Size,
	                                                       Context);

	const FVector2D Scale = Size / MapData->GetMapSize();
	TArray<FCanvasUVTri> CanvasTris;
	for (const FDistrictRegion& DistrictRegion : MapData->GetDistrictRegions())
	{
		FLinearColor Color = FLinearColor::MakeRandomSeededColor(DistrictRegion.District);

		for (const FPolyTriangle2D& Tri : DistrictRegion.Triangles)
		{
			FCanvasUVTri CanvasTri;
			CanvasTri.V0_Color = Color;
			CanvasTri.V1_Color = Color;
			CanvasTri.V2_Color = Color;
			CanvasTri.V0_Pos = Tri.V0 * Scale;
			CanvasTri.V1_Pos = Tri.V1 * Scale;
			CanvasTri.V2_Pos = Tri.V2 * Scale;
			CanvasTris.Add(CanvasTri);
		}
	}
	Canvas->K2_DrawTriangle(nullptr, CanvasTris);

	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(MapData->GetWorld(), Context);
}

void UIslandMapDebugUtils::DrawQuadTree(UCanvasRenderTarget2D* RenderTarget2D, const UIslandMapData* MapData,
	float Opacity)
{
	if (MapData == nullptr)
		return;

	UCanvas* Canvas;
	FVector2D Size;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(MapData->GetWorld(), RenderTarget2D, Canvas, Size,
														   Context);

	const FVector2D Scale = Size / MapData->GetMapSize();
	TArray<FCanvasUVTri> CanvasTris;
	for (const TSharedPtr<FIslandQuadTreeNode>& Area : MapData->GetQuadIslandAreas())
	{
		if(!Area->bCoverCoastline)
		{
			continue;
		}
		FLinearColor Color = FLinearColor::MakeRandomSeededColor(Area->Level);
		Color.A = Opacity;
		FCanvasUVTri CanvasTri;
		CanvasTri.V0_Color = Color;
		CanvasTri.V1_Color = Color;
		CanvasTri.V2_Color = Color;
		CanvasTri.V0_Pos = Area->LeftBottom * Scale;
		CanvasTri.V1_Pos = Area->RightBottom * Scale;
		CanvasTri.V2_Pos = Area->RightTop * Scale;
		CanvasTris.Add(CanvasTri);
		CanvasTri.V0_Pos = Area->LeftBottom * Scale;
		CanvasTri.V1_Pos = Area->RightTop * Scale;
		CanvasTri.V2_Pos = Area->LeftTop * Scale;
		CanvasTris.Add(CanvasTri);
	}
	Canvas->K2_DrawTriangle(nullptr, CanvasTris);

	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(MapData->GetWorld(), Context);
}
