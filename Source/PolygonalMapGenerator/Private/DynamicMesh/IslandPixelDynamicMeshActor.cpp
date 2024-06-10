// Fill out your copyright notice in the Description page of Project Settings.


#include "DynamicMesh/IslandPixelDynamicMeshActor.h"

#include "IslandMapData.h"
#include "Coastline/IslandCoastline.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"

void AIslandPixelDynamicMeshActor::GenerateIslandMesh()
{
	if (!IsValid(MapData))
	{
		return;
	}
	if (MeshPixelHeight <= 1 || MeshPixelWidth <= 1)
	{
		// UE_LOG(LogMapGen, Error, FString::Printf(TEXT("MeshPixelWidth: %d and MeshPixelHeight: %d cannot be smaller than 1"), MeshPixelWidth, MeshPixelHeight));
		return;
	}
	UDynamicMesh* DynamicMesh = GetDynamicMeshComponent()->GetDynamicMesh();
	FVector2D MapSize = MapData->GetMapSize();
	FGeometryScriptPrimitiveOptions PrimitiveOptions;
	FTransform PanelTransform;
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendRectangleXY(
		DynamicMesh, PrimitiveOptions, PanelTransform,
		MapSize.X, MapSize.Y, FMath::Max<int32>(MapSize.X / MeshPixelWidth, 1),
		FMath::Max<int32>(MapSize.Y / MeshPixelHeight, 1)
	);

	FVector2D HalfMapSize = MapSize / 2;
	DynamicMesh->EditMesh([&](FDynamicMesh3& EditMesh)
	{
		int NumVertices = EditMesh.MaxVertexID();
		for (int Index = 0; Index < NumVertices; ++Index)
		{
			if (!EditMesh.IsVertex(Index))
				continue;
			FVector3d Position = EditMesh.GetVertex(Index);
			FVector2D Point = {Position.X + HalfMapSize.X, Position.Y + HalfMapSize.Y};
			bool bPointInPolygon2D = false;
			double MinDistance = std::numeric_limits<double>::max();
			for (const FCoastlinePolygon& CoastLine : MapData->GetCoastLines())
			{
				bPointInPolygon2D = UIslandMapUtils::PointInPolygon2D(Point, CoastLine.Positions);
				if (bPointInPolygon2D)
					break;
				MinDistance = FMath::Min(MinDistance, UIslandMapUtils::DistanceToPolygon2D(Point, CoastLine.Positions));
			}
			if (bPointInPolygon2D)
			{
				Position.Z += BorderDepth;
				EditMesh.SetVertex(Index, Position);
			}
			else if (MinDistance <= BorderOffset)
			{
				float UnitDepth = (BorderOffset - MinDistance) / BorderOffset;
				// Remap distance https://www.geogebra.org/m/cttp6cqd
				UnitDepth = (FMath::Cos((UnitDepth + 1) * PI) + 1) / 2;
				Position.Z += UnitDepth * BorderDepth;
				EditMesh.SetVertex(Index, Position);
			}
		}
	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);
}
