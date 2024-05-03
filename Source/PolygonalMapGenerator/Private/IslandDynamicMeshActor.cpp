// Fill out your copyright notice in the Description page of Project Settings.


#include "IslandDynamicMeshActor.h"

#include "Clipper2Helper.h"
#include "Coastline/IslandCoastline.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshDeformFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshSubdivideFunctions.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "GeometryScript/MeshVoxelFunctions.h"

void AIslandDynamicMeshActor::BeginPlay()
{
	Super::BeginPlay();
}

void AIslandDynamicMeshActor::SetMapData(UIslandMapData* InMapData)
{
	MapData = InMapData;
}

UIslandMapData* AIslandDynamicMeshActor::GetMapData()
{
	return MapData;
}

void AIslandDynamicMeshActor::GenerateIsland()
{
	GenerateIslandMesh();
	GenerateIslandTexture();
	if (!IsValid(IslandMaterial))
	{
		return;
	}
	UMaterialInstanceDynamic* MaterialInstance = UMaterialInstanceDynamic::Create(IslandMaterial, nullptr);
	MaterialInstance->SetTextureParameterValue(FName(TEXT("District ID 01")), IslandTexture->DistrictIDTexture01);
	MaterialInstance->SetTextureParameterValue(FName(TEXT("District ID 02")), IslandTexture->DistrictIDTexture02);
	GetDynamicMeshComponent()->SetMaterial(0, MaterialInstance);
}

void AIslandDynamicMeshActor::GetDistrictTextures(UTexture2D*& Texture1, UTexture2D*& Texture2)
{
	if (!IsValid(IslandTexture))
	{
		return;
	}
	Texture1 = IslandTexture->DistrictIDTexture01;
	Texture2 = IslandTexture->DistrictIDTexture02;
}

void AIslandDynamicMeshActor::GenerateIslandMesh()
{
	if (!IsValid(MapData))
	{
		return;
	}
	UDynamicMesh* DynamicMesh = GetDynamicMeshComponent()->GetDynamicMesh();

	const TArray<FCoastlinePolygon>& Coastlines = MapData->GetCoastLines();
	FVector2D MapSize = MapData->Mesh->GetSize();
	FVector2D VerticeBias = -FVector2D(MapSize / 2);

	// Build island meshs
	for (const FCoastlinePolygon& Coastline : Coastlines)
	{
		FGeometryScriptSimpleMeshBuffers Buffers;
		int32 VertexNum = Coastline.Positions.Num();
		int32 TriangleNum = Coastline.Triangles.Num();
		Buffers.Vertices.Empty(VertexNum * 2);
		Buffers.Triangles.Empty(TriangleNum + VertexNum * 2);
		TMap<int32, int32> IndexMap;
		for (int32 Index = 0; Index < VertexNum; ++Index)
		{
			IndexMap.Emplace(Coastline.Indices[Index], Index);
			Buffers.Vertices.Emplace(FVector(Coastline.Positions[Index] + VerticeBias, 0));
		}
		for (const FPolyTriangle2D& Tri : Coastline.Triangles)
		{
			Buffers.Triangles.Emplace(FIntVector(IndexMap[Tri.V2Index], IndexMap[Tri.V1Index], IndexMap[Tri.V0Index]));
		}

		// Island expand border
		TArray<FVector2D> ExpandPoints;
		UClipper2Helper::Offset(ExpandPoints, Coastline.Positions, BorderOffset * 2, 0);
		int32 ExpandPointNum = ExpandPoints.Num();
		TArray<int32> ExpandPointIds;
		ExpandPointIds.Empty(ExpandPointNum);
		for (int32 Index = 0; Index < ExpandPointNum; ++Index)
		{
			Buffers.Vertices.Emplace(FVector(ExpandPoints[Index] + VerticeBias, -BorderDepth * 2));
		}
		TArray<FPolyTriangle2D> ExpandTriangles;
		UPolyPartitionHelper::Triangulate(
			ExpandPoints, ExpandPointIds,
			ExpandTriangles
		);
		for (const FPolyTriangle2D& Tri : ExpandTriangles)
		{
			Buffers.Triangles.Emplace(
				FIntVector(Tri.V0Index, Tri.V1Index, Tri.V2Index) + FIntVector(VertexNum, VertexNum, VertexNum));
		}
		TArray<FIntVector> OuterTriangles;
		TriangulateRing(OuterTriangles, ExpandPoints, Coastline.Positions);
		Buffers.Triangles.Append(OuterTriangles);

		FGeometryScriptIndexList TriangleIndices;
		UGeometryScriptLibrary_MeshBasicEditFunctions::AppendBuffersToMesh(DynamicMesh, Buffers, TriangleIndices);
	}

	// Add base box to slight bend border
	{
		FTransform BaseBoxTransform;
		const float BaseHeight = BorderDepth;
		BaseBoxTransform.SetLocation(FVector(0, 0, -BorderDepth - BaseHeight - 100));
		UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
			DynamicMesh,
			FGeometryScriptPrimitiveOptions(),
			BaseBoxTransform,
			MapSize.X + BorderOffset * 2 + 100,
			MapSize.Y + BorderOffset * 2 + 100,
			BaseHeight
		);
	}

	// Voxelization
	UGeometryScriptLibrary_MeshVoxelFunctions::ApplyMeshSolidify(DynamicMesh, SolidifyOptions);

	UGeometryScriptLibrary_MeshNormalsFunctions::SetPerVertexNormals(DynamicMesh);

	FGeometryScriptIterativeMeshSmoothingOptions SmoothingOptions;
	SmoothingOptions.NumIterations = 3;
	UGeometryScriptLibrary_MeshDeformFunctions::ApplyIterativeSmoothingToMesh(
		DynamicMesh,
		FGeometryScriptMeshSelection(),
		SmoothingOptions
	);

	UGeometryScriptLibrary_MeshSubdivideFunctions::ApplyPNTessellation(
		DynamicMesh,
		FGeometryScriptPNTessellateOptions(),
		1
	);

	// Cut the section of mesh that under ocean
	{
		FTransform CutFrame;
		CutFrame.SetRotation(FRotator(180.0, 0, 0).Quaternion());
		CutFrame.SetLocation(FVector(0, 0, -BorderDepth));
		FGeometryScriptMeshPlaneCutOptions CutOptions;
		CutOptions.bFillHoles = false;
		CutOptions.bFillSpans = false;
		UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshPlaneCut(DynamicMesh, CutFrame, CutOptions);
	}

	FDynamicMesh3& Mesh = DynamicMesh->GetMeshRef();
	// UV
	DynamicMesh->EditMesh([&](FDynamicMesh3& EditMesh)
	{
		UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = EditMesh.Attributes()->GetUVLayer(0);
		for (int TriIndex : Mesh.TriangleIndicesItr())
		{
			FVector3d V0Pos, V1Pos, V2Pos;
			Mesh.GetTriVertices(TriIndex, V0Pos, V1Pos, V2Pos);
			if (UVOverlay != nullptr)
			{
				int32 Elem0 = UVOverlay->AppendElement(FVector2f(V0Pos.X / MapSize.X - 0.5f, V0Pos.Y / MapSize.Y - 0.5f));
				int32 Elem1 = UVOverlay->AppendElement(FVector2f(V1Pos.X / MapSize.X - 0.5f, V1Pos.Y / MapSize.Y - 0.5f));
				int32 Elem2 = UVOverlay->AppendElement(FVector2f(V2Pos.X / MapSize.X - 0.5f, V2Pos.Y / MapSize.Y - 0.5f));
				// int32 Elem0 = UVOverlay->AppendElement(
				// 	FVector2f(V0Pos.X / MapSize.X, V0Pos.Y / MapSize.Y));
				// int32 Elem1 = UVOverlay->AppendElement(
				// 	FVector2f(V1Pos.X / MapSize.X, V1Pos.Y / MapSize.Y));
				// int32 Elem2 = UVOverlay->AppendElement(
				// 	FVector2f(V2Pos.X / MapSize.X, V2Pos.Y / MapSize.Y));
				UVOverlay->SetTriangle(TriIndex, UE::Geometry::FIndex3i(Elem0, Elem1, Elem2), true);
			}
		}
	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);
}

void AIslandDynamicMeshActor::GenerateIslandTexture()
{
	IslandTexture = NewObject<UIslandTexture>();
	IslandTexture->MapData = MapData;
	UTexture2D *A, *B;
	IslandTexture->DrawDistrictIDTexture(A, B, TextureWidth, TextureHeight);
}

void AIslandDynamicMeshActor::TriangulateRing(TArray<FIntVector>& Triangles, const TArray<FVector2D>& OuterPoly,
                                              const TArray<FVector2D>& InnerPoly)
{
	TArray<int32> OuterLinkedInner;
	const int32 OuterNum = OuterPoly.Num();
	const int32 InnerNum = InnerPoly.Num();
	OuterLinkedInner.SetNumZeroed(OuterNum);
	for (int32 OuterIndex = 0; OuterIndex < OuterNum; ++OuterIndex)
	{
		const FVector2D& OuterPos = OuterPoly[OuterIndex];
		double ClosedInnerPointDistSquared = DBL_MAX;
		int32 ClosedInnerPointIndex = 0;
		for (int32 InnerIndex = 0; InnerIndex < InnerNum; ++InnerIndex)
		{
			double Dist = FVector2D::DistSquared(OuterPos, InnerPoly[InnerIndex]);
			if (Dist < ClosedInnerPointDistSquared)
			{
				ClosedInnerPointIndex = InnerIndex;
				ClosedInnerPointDistSquared = Dist;
			}
		}
		OuterLinkedInner[OuterIndex] = ClosedInnerPointIndex;
	}
	Triangles.Empty(FMath::Max(OuterNum, InnerNum) * 2);
	for (int32 OuterIndex = 0, LinkedInnerIndex = OuterLinkedInner[OuterIndex]; OuterIndex < OuterNum;)
	{
		const int32 NextOuterIndex = (OuterIndex + 1) % OuterNum;
		const int32 NextOuterLinkedInnerIndex = OuterLinkedInner[NextOuterIndex];
		const int32 LinkedNextInnerIndex = (LinkedInnerIndex + 1) % InnerNum;
		if (LinkedInnerIndex == NextOuterLinkedInnerIndex)
		{
			Triangles.Emplace(FIntVector(
				NextOuterIndex + InnerNum,
				OuterIndex + InnerNum,
				LinkedInnerIndex
			));
			++OuterIndex;
		}
		else if (LinkedNextInnerIndex == NextOuterLinkedInnerIndex)
		{
			Triangles.Emplace(FIntVector(
				NextOuterIndex + InnerNum,
				OuterIndex + InnerNum,
				LinkedInnerIndex
			));
			Triangles.Emplace(FIntVector(
				NextOuterLinkedInnerIndex,
				NextOuterIndex + InnerNum,
				LinkedInnerIndex
			));
			++OuterIndex;
			LinkedInnerIndex = NextOuterLinkedInnerIndex;
		}
		else
		{
			Triangles.Emplace(FIntVector(
				LinkedNextInnerIndex,
				OuterIndex + InnerNum,
				LinkedInnerIndex
			));
			LinkedInnerIndex = LinkedNextInnerIndex;
		}
	}
}
