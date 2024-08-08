#include "DynamicMesh/IslandDynamicGridMeshActor.h"
#include "Coastline/IslandCoastline.h"
#include "GeometryScript/MeshNormalsFunctions.h"

void AIslandDynamicGridMeshActor::GenerateIslandMesh(UDynamicMesh* DynamicMesh, const FTransform& Transform)
{
	if (!IsValid(MapData))
	{
		return;
	}
	const int32 GridAmount = (GridDivisions + 1) * (GridDivisions + 1);
	GridMeshBuffers.SetNum(GridAmount);
	FGraphEventArray CalcGridTasks;
	CalcGridTasks.Empty(GridAmount);
	for (int32 Index = 0; Index < GridAmount; Index++)
	{
		CalcGridTasks.Add(FFunctionGraphTask::CreateAndDispatchWhenReady([&, Index, Transform]()
		{
			CalcGridMeshBuffer(Index, Transform);
		}, TStatId()));
	}
	FFunctionGraphTask::CreateAndDispatchWhenReady([this, DynamicMesh]()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(AIslandDynamicGridMeshActor::AppendBuffersToMesh);
		for (const FGeometryScriptSimpleMeshBuffers& GridMeshBuffer : GridMeshBuffers)
		{
			FGeometryScriptIndexList TriangleIndices;
			UGeometryScriptLibrary_MeshBasicEditFunctions::AppendBuffersToMesh(
				DynamicMesh, GridMeshBuffer, TriangleIndices, 0, true);
		}
		UGeometryScriptLibrary_MeshNormalsFunctions::SetPerVertexNormals(DynamicMesh);
	}, TStatId(), &CalcGridTasks, ENamedThreads::GameThread);
}

void AIslandDynamicGridMeshActor::CalcGridMeshBuffer(const int32 GridIndex, const FTransform Transform)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AIslandDynamicGridMeshActor::CalcGridMeshBuffer);
	FGeometryScriptSimpleMeshBuffers& Buffers = GridMeshBuffers[GridIndex];
	int32 GridRow = GridIndex / (GridDivisions + 1);
	int32 GridCol = GridIndex % (GridDivisions + 1);
	FVector2D MapSize = MapData->GetMapSize();
	FVector2D GridSize = MapSize / (GridDivisions + 1);
	FVector2D BoundaryMin(GridCol * GridSize.X, GridRow * GridSize.Y);
	FVector2D SubgridSize = GridSize / GridResolution;
	int32 VerticesNum = (GridResolution + 1) * (GridResolution + 1);
	Buffers.Vertices.SetNumUninitialized(VerticesNum);
	double MaxUnitDepth = 0.;
	double MinUnitDepth = TNumericLimits<double>::Max();
	for (int32 VIndex = 0; VIndex < VerticesNum; VIndex++)
	{
		FVector2D Point(
			BoundaryMin.X + VIndex / (GridResolution + 1) * SubgridSize.X,
			BoundaryMin.Y + VIndex % (GridResolution + 1) * SubgridSize.Y
		);
		double UnitDepth = 0.;
		double MinDistance = TNumericLimits<double>::Max();
		bool bPointInPolygon2D = false;
		for (const FCoastlinePolygon& CoastLine : MapData->GetCoastLines())
		{
			bPointInPolygon2D = UIslandMapUtils::PointInPolygon2D(Point, CoastLine.Positions);
			if (bPointInPolygon2D)
			{
				break;
			}
			MinDistance = FMath::Min(MinDistance, UIslandMapUtils::DistanceToPolygon2D(Point, CoastLine.Positions));
		}
		if (bPointInPolygon2D)
		{
			UnitDepth = 1.;
		}
		else if (MinDistance <= BorderOffset)
		{
			UnitDepth = (BorderOffset - MinDistance) / BorderOffset;
		}
		MaxUnitDepth = FMath::Max(MaxUnitDepth, UnitDepth);
		MinUnitDepth = FMath::Min(MinUnitDepth, UnitDepth);
		Buffers.Vertices[VIndex].Z = UnitDepth;
		Buffers.Vertices[VIndex].X = Point.X;
		Buffers.Vertices[VIndex].Y = Point.Y;
	}
	if (FMath::IsNearlyEqual(MaxUnitDepth, MinUnitDepth))
	{
		// If the Grid has no height differences, the grid uses quadrilaterals without subdivisions.
		TArray<FVector> FourCorners;
		FourCorners.Emplace(Buffers.Vertices[0]);
		FourCorners.Emplace(Buffers.Vertices[GridResolution]);
		FourCorners.Emplace(Buffers.Vertices[GridResolution * (GridResolution + 1)]);
		FourCorners.Emplace(Buffers.Vertices[VerticesNum - 1]);
		Buffers.Vertices = FourCorners;
		Buffers.Triangles.SetNumUninitialized(2);
		Buffers.Triangles[0].X = 0;
		Buffers.Triangles[0].Y = 1;
		Buffers.Triangles[0].Z = 2;
		Buffers.Triangles[1].X = 1;
		Buffers.Triangles[1].Y = 3;
		Buffers.Triangles[1].Z = 2;
		VerticesNum = 4;
	}
	else
	{
		int32 GridResolutionSquared = GridResolution * GridResolution;
		int32 TrianglesNum = GridResolutionSquared * 2;
		Buffers.Triangles.SetNum(TrianglesNum);
		for (int32 GIndex = 0; GIndex < GridResolutionSquared; GIndex++)
		{
			int32 Row = GIndex / GridResolution;
			int32 Col = GIndex % GridResolution;
			int32 TriAIndex = GIndex * 2;
			int32 TriBIndex = TriAIndex + 1;
			Buffers.Triangles[TriAIndex].X = (GridResolution + 1) * Row + Col;
			Buffers.Triangles[TriAIndex].Y = Buffers.Triangles[TriAIndex].X + 1;
			Buffers.Triangles[TriAIndex].Z = (GridResolution + 1) * (Row + 1) + Col;
			Buffers.Triangles[TriBIndex].X = Buffers.Triangles[TriAIndex].Y;
			Buffers.Triangles[TriBIndex].Y = Buffers.Triangles[TriAIndex].Z + 1;
			Buffers.Triangles[TriBIndex].Z = Buffers.Triangles[TriAIndex].Z;
		}
	}
	Buffers.UV0.SetNumUninitialized(VerticesNum);
	// Calculate Positions and UVs
	for (int32 VIndex = 0; VIndex < VerticesNum; VIndex++)
	{
		Buffers.UV0[VIndex] = FVector2D(Buffers.Vertices[VIndex].X, Buffers.Vertices[VIndex].Y) / MapSize;
		// TODO: 
		double UnitDepth = UIslandMapUtils::Remap(Buffers.Vertices[VIndex].Z, BorderDepthRemapMethod);
		Buffers.Vertices[VIndex].Z = (UnitDepth - 1) * BorderDepth;
		Buffers.Vertices[VIndex] = Transform.TransformPosition(Buffers.Vertices[VIndex]);
	}
}
