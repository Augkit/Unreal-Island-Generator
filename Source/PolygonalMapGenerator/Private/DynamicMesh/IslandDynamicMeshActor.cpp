// Fill out your copyright notice in the Description page of Project Settings.


#include "DynamicMesh/IslandDynamicMeshActor.h"

#include "canvas_ity.hpp"
#include "Clipper2Helper.h"
#include "IslandMapData.h"
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

void AIslandDynamicMeshActor::GenerateIslandTexture()
{
	SCOPE_CYCLE_COUNTER(STAT_GenerateDistrictIDTexture)
	if (!MapData->IsValidLowLevel())
	{
		return;
	}
	const FVector2D Scale = FVector2D(DistrictIDTextureWidth, DistrictIDTextureHeight) / MapData->GetMapSize();
	canvas_ity::canvas_20 Canvas(DistrictIDTextureWidth, DistrictIDTextureHeight);
	for (const FDistrictRegion& DistrictRegion : MapData->GetDistrictRegions())
	{
		canvas_ity::rgba_20 Data;
		Data.a = 1.f;
		switch (DistrictRegion.District)
		{
		case 0:
			Data.d_a = 1.f;
			Canvas.set_data_color(canvas_ity::fill_style, Data);
			break;
		case 1:
			Data.d_b = 1.f;
			Canvas.set_data_color(canvas_ity::fill_style, Data);
			break;
		case 2:
			Data.d_c = 1.f;
			Canvas.set_data_color(canvas_ity::fill_style, Data);
			break;
		case 3:
			Data.d_d = 1.f;
			Canvas.set_data_color(canvas_ity::fill_style, Data);
			break;
		case 4:
			Data.d_e = 1.f;
			Canvas.set_data_color(canvas_ity::fill_style, Data);
			break;
		case 5:
			Data.d_f = 1.f;
			Canvas.set_data_color(canvas_ity::fill_style, Data);
			break;
		case 6:
			Data.d_g = 1.f;
			Canvas.set_data_color(canvas_ity::fill_style, Data);
			break;
		case 7:
			Data.d_h = 1.f;
			Canvas.set_data_color(canvas_ity::fill_style, Data);
			break;
		case 8:
			Data.d_i = 1.f;
			Canvas.set_data_color(canvas_ity::fill_style, Data);
			break;
		case 9:
			Data.d_j = 1.f;
			Canvas.set_data_color(canvas_ity::fill_style, Data);
			break;
		case 10:
			Data.d_k = 1.f;
			Canvas.set_data_color(canvas_ity::fill_style, Data);
			break;
		case 11:
			Data.d_l = 1.f;
			Canvas.set_data_color(canvas_ity::fill_style, Data);
			break;
		case 12:
			Data.d_m = 1.f;
			Canvas.set_data_color(canvas_ity::fill_style, Data);
			break;
		case 13:
			Data.d_n = 1.f;
			Canvas.set_data_color(canvas_ity::fill_style, Data);
			break;
		case 14:
			Data.d_o = 1.f;
			Canvas.set_data_color(canvas_ity::fill_style, Data);
			break;
		case 15:
			Data.d_p = 1.f;
			Canvas.set_data_color(canvas_ity::fill_style, Data);
			break;
		default:
			Canvas.set_data_color(canvas_ity::fill_style, Data);
			break;
		}

		Canvas.begin_path();
		const FVector2d FirstPos = DistrictRegion.Positions[0] * Scale;
		Canvas.move_to(FirstPos.X, FirstPos.Y);
		for (int32 PositionIndex = 1; PositionIndex < DistrictRegion.Positions.Num(); ++PositionIndex)
		{
			const FVector2d Pos = DistrictRegion.Positions[PositionIndex] * Scale;
			Canvas.line_to(Pos.X, Pos.Y);
		}
		Canvas.close_path();
		Canvas.fill();
	}
	canvas_ity::rgba_20* Bitmap = Canvas.get_bitmap();

	int32 FloatImageBufferLength = DistrictIDTextureWidth * DistrictIDTextureHeight * 4;
	TArray<FFloat16> FloatIDImageBuffer1;
	FloatIDImageBuffer1.Empty(FloatImageBufferLength);
	TArray<FFloat16> FloatIDImageBuffer2;
	FloatIDImageBuffer2.Empty(FloatImageBufferLength);

	for (int32 Row = 0; Row < DistrictIDTextureHeight; ++Row)
	{
		for (int32 Col = 0; Col < DistrictIDTextureWidth; ++Col)
		{
			const canvas_ity::rgba_20& ColorData = *(Bitmap + static_cast<int>(Row * DistrictIDTextureWidth + Col));
			struct
			{
				int32 District;
				float Proportion;
			} Proportions[16];
			Proportions[0].District = 1;
			Proportions[1].District = 2;
			Proportions[2].District = 3;
			Proportions[3].District = 4;
			Proportions[4].District = 5;
			Proportions[5].District = 6;
			Proportions[6].District = 7;
			Proportions[7].District = 8;
			Proportions[8].District = 9;
			Proportions[9].District = 10;
			Proportions[10].District = 11;
			Proportions[11].District = 12;
			Proportions[12].District = 13;
			Proportions[13].District = 14;
			Proportions[14].District = 15;
			Proportions[15].District = 16;
			Proportions[0].Proportion = ColorData.d_a;
			Proportions[1].Proportion = ColorData.d_b;
			Proportions[2].Proportion = ColorData.d_c;
			Proportions[3].Proportion = ColorData.d_d;
			Proportions[4].Proportion = ColorData.d_e;
			Proportions[5].Proportion = ColorData.d_f;
			Proportions[6].Proportion = ColorData.d_g;
			Proportions[7].Proportion = ColorData.d_h;
			Proportions[8].Proportion = ColorData.d_i;
			Proportions[9].Proportion = ColorData.d_j;
			Proportions[10].Proportion = ColorData.d_k;
			Proportions[11].Proportion = ColorData.d_l;
			Proportions[12].Proportion = ColorData.d_m;
			Proportions[13].Proportion = ColorData.d_n;
			Proportions[14].Proportion = ColorData.d_o;
			Proportions[15].Proportion = ColorData.d_p;
			for (int32 i = 0; i < 15; i++)
				for (int32 j = 0; j < 15 - i; j++)
					if (Proportions[j].Proportion < Proportions[j + 1].Proportion)
						std::swap(Proportions[j], Proportions[j + 1]);

			if (Proportions[0].Proportion > 0)
			{
				FloatIDImageBuffer1.Emplace(Proportions[0].District / 16.f - 0.01f);
				FloatIDImageBuffer1.Emplace(Proportions[0].Proportion);
				FloatIDImageBuffer1.Emplace(Proportions[1].District / 16.f - 0.01f);
				FloatIDImageBuffer1.Emplace(Proportions[1].Proportion);
				FloatIDImageBuffer2.Emplace(Proportions[2].District / 16.f - 0.01f);
				FloatIDImageBuffer2.Emplace(Proportions[2].Proportion);
				FloatIDImageBuffer2.Emplace(Proportions[3].District / 16.f - 0.01f);
				FloatIDImageBuffer2.Emplace(Proportions[3].Proportion);
			}
			else
			{
				FloatIDImageBuffer1.Emplace(0.f);
				FloatIDImageBuffer1.Emplace(0.f);
				FloatIDImageBuffer1.Emplace(0.f);
				FloatIDImageBuffer1.Emplace(0.f);
				FloatIDImageBuffer2.Emplace(0.f);
				FloatIDImageBuffer2.Emplace(0.f);
				FloatIDImageBuffer2.Emplace(0.f);
				FloatIDImageBuffer2.Emplace(0.f);
			}
		}
	}
	{
		DistrictIDTexture01 = UTexture2D::CreateTransient(DistrictIDTextureWidth, DistrictIDTextureHeight,
		                                                  EPixelFormat::PF_FloatRGBA);
		DistrictIDTexture01->bNotOfflineProcessed = true;
		DistrictIDTexture01->SRGB = false;
		DistrictIDTexture01->LODGroup = TEXTUREGROUP_16BitData;
		DistrictIDTexture01->CompressionSettings = TC_HDR;
		uint8* MipData = static_cast<uint8*>(DistrictIDTexture01->GetPlatformData()->Mips[0].BulkData.Lock(
			LOCK_READ_WRITE));
		check(MipData != nullptr);
		FMemory::Memmove(MipData, FloatIDImageBuffer1.GetData(), FloatImageBufferLength * sizeof(FFloat16));
		DistrictIDTexture01->GetPlatformData()->Mips[0].BulkData.Unlock();
		DistrictIDTexture01->UpdateResource();
	}
	{
		DistrictIDTexture02 = UTexture2D::CreateTransient(DistrictIDTextureWidth, DistrictIDTextureHeight,
		                                                  EPixelFormat::PF_FloatRGBA);
		DistrictIDTexture02->bNotOfflineProcessed = true;
		DistrictIDTexture02->SRGB = false;
		DistrictIDTexture02->LODGroup = TEXTUREGROUP_16BitData;
		DistrictIDTexture02->CompressionSettings = TC_HDR;
		uint8* MipData = static_cast<uint8*>(DistrictIDTexture02->GetPlatformData()->Mips[0].BulkData.Lock(
			LOCK_READ_WRITE));
		check(MipData != nullptr);
		FMemory::Memmove(MipData, FloatIDImageBuffer2.GetData(), FloatImageBufferLength * sizeof(FFloat16));
		DistrictIDTexture02->GetPlatformData()->Mips[0].BulkData.Unlock();
		DistrictIDTexture02->UpdateResource();
	}
}

void AIslandDynamicMeshActor::GenerateIslandMesh()
{
	if (!IsValid(MapData))
	{
		return;
	}
	UDynamicMesh* DynamicMesh = GetDynamicMeshComponent()->GetDynamicMesh();
	switch (GenerateMeshMethod)
	{
	case EGenerateMeshType::GMT_Delaunator:
		GenerateMeshDelaunator(DynamicMesh);
		break;
	case EGenerateMeshType::GMT_Voxelization:
		GenerateMeshVoxelization(DynamicMesh);
		break;
	case EGenerateMeshType::GMT_PixelMesh:
		GenerateMeshPixel(DynamicMesh);
		break;
	}
}

void AIslandDynamicMeshActor::GenerateMeshDelaunator(UDynamicMesh* DynamicMesh)
{
	FVector2D MapSize = MapData->Mesh->GetSize();

	UTriangleDualMesh* Mesh = MapData->Mesh;
	FGeometryScriptSimpleMeshBuffers Buffers;

	TMap<FTriangleIndex, int32> VertexIndicesMap;
	for (int32 PointIndex = 0; PointIndex < Mesh->NumSolidRegions; ++PointIndex)
	{
		if (MapData->IsPointOcean(PointIndex))
			continue;
		const TArray<FTriangleIndex>& TriangleIndices = Mesh->r_circulate_t(PointIndex);
		if (TriangleIndices.Num() < 3)
		{
			continue;
		}
		int32 FirstVertexIndex = -1;
		int32 PrevVertexIndex = -1;
		for (int32 Index = 0; Index < TriangleIndices.Num(); ++Index)
		{
			const FTriangleIndex& TriangleIndex = TriangleIndices[Index];
			int32 VertexIndex;
			if (VertexIndicesMap.Contains(TriangleIndex))
			{
				VertexIndex = VertexIndicesMap.FindRef(TriangleIndex);
			}
			else
			{
				FVector2D Position2D = Mesh->t_pos(TriangleIndex);
				Buffers.UV0.Emplace(Position2D / MapSize);
				FVector Position(Position2D, 0);
				VertexIndex = VertexIndicesMap.Emplace(TriangleIndex, Buffers.Vertices.Emplace(Position));
			}
			if (Index == 0)
			{
				FirstVertexIndex = VertexIndex;
			}
			else if (Index == 1)
			{
				PrevVertexIndex = VertexIndex;
			}
			else
			{
				Buffers.Triangles.Emplace(FIntVector(VertexIndex, PrevVertexIndex, FirstVertexIndex));
				PrevVertexIndex = VertexIndex;
			}
		}
	}

	// Island expand border
	const TArray<FCoastlinePolygon>& Coastlines = MapData->GetCoastLines();
	if (DelaunatorBorderProcessMethod == EDelaunatorBorderProcess::DBP_StepDiffusion)
	{
		struct FBorderStepPoly
		{
			TArray<FVector2D> Points;
			TArray<int32> IDs;
		};
		TArray<FBorderStepPoly> BorderPolys;
		for (const FCoastlinePolygon& Coastline : Coastlines)
		{
			BorderPolys.SetNumZeroed(BorderTessellationTimes + 1);
			BorderPolys[0].Points = Coastline.Positions;
			BorderPolys[0].IDs.Empty(Coastline.Indices.Num());
			for (const FTriangleIndex& TriangleIndex : Coastline.Indices)
			{
				BorderPolys[0].IDs.Emplace(VertexIndicesMap.FindChecked(TriangleIndex));
			}
			int32 BiasIndex = FMath::Clamp(BorderTessellationStartStep, 0, BorderTessellationTimes - 1);
			int32 Step = BiasIndex + 1;
			int32 PrevStep = 0;
			for (; Step <= BorderTessellationTimes; ++Step)
			{
				TArray<FVector2D>& ExpandPoints = BorderPolys[Step].Points;
				TArray<int32>& ExpandPointIDs = BorderPolys[Step].IDs;
				TArray<FVector2D>& InnerPoints = BorderPolys[PrevStep].Points;
				TArray<int32>& InnerPointIDs = BorderPolys[PrevStep].IDs;
				float Scale = static_cast<float>(Step - PrevStep) / BorderTessellationTimes;
				if (PrevStep == 0)
				{
					TArray<FVector2D> OffsetPoints;
					UClipper2Helper::Offset(OffsetPoints, InnerPoints, BorderOffset * Scale, 0);
					SubdivisionPolygon(ExpandPoints, OffsetPoints);
				}
				else
				{
					UClipper2Helper::Offset(ExpandPoints, InnerPoints, BorderOffset * Scale, 0);
				}
				int32 ExpandPointNum = ExpandPoints.Num();
				ExpandPointIDs.Empty(ExpandPointNum);
				for (int32 Index = 0; Index < ExpandPointNum; ++Index)
				{
					ExpandPointIDs.Emplace(Buffers.Vertices.Emplace(FVector(ExpandPoints[Index], 0)));
					Buffers.UV0.Emplace(ExpandPoints[Index] / MapSize);
				}
				if (PrevStep != 0)
				{
					TArray<FIntVector> OuterTriangles;
					TriangulateRing(OuterTriangles, ExpandPoints, ExpandPointIDs, InnerPoints, InnerPointIDs);
					Buffers.Triangles.Append(OuterTriangles);
				}
				PrevStep = Step;
			}
			Step = BiasIndex;
			PrevStep = BiasIndex + 1;
			for (; Step >= 0; --Step)
			{
				TArray<FVector2D>& ExpandPoints = BorderPolys[PrevStep].Points;
				TArray<int32>& ExpandPointIDs = BorderPolys[PrevStep].IDs;
				TArray<FVector2D>& InnerPoints = BorderPolys[Step].Points;
				TArray<int32>& InnerPointIDs = BorderPolys[Step].IDs;
				if (Step != 0)
				{
					UClipper2Helper::Offset(InnerPoints, ExpandPoints, -BorderOffset / BorderTessellationTimes, 0);
					int32 InnerPointNum = InnerPoints.Num();
					InnerPointIDs.Empty(InnerPointNum);
					for (int32 Index = 0; Index < InnerPointNum; ++Index)
					{
						InnerPointIDs.Emplace(Buffers.Vertices.Emplace(FVector(InnerPoints[Index], 0)));
						Buffers.UV0.Emplace(InnerPoints[Index] / MapSize);
					}
				}
				TArray<FIntVector> OuterTriangles;
				TriangulateRing(OuterTriangles, ExpandPoints, ExpandPointIDs, InnerPoints, InnerPointIDs);
				Buffers.Triangles.Append(OuterTriangles);
				PrevStep = Step;
			}
		}
	}
	else if (DelaunatorBorderProcessMethod == EDelaunatorBorderProcess::DBP_StepTwoWay)
	{
		struct FBorderStepTwoWayPoly
		{
			TArray<FVector2D> InnerToOuterPoints;
			TArray<FVector2D> OuterToInnerPoints;
			TArray<FVector2D> UnionPoints;
			TArray<int32> UnionPointIDs;
		};
		TArray<FBorderStepTwoWayPoly> BorderStepPolys;
		float StepBorderOffset = BorderOffset / BorderTessellationTimes;
		for (const FCoastlinePolygon& Coastline : Coastlines)
		{
			const TArray<FVector2D>& InnermostPoints = Coastline.Positions;
			TArray<FVector2D> OutermostPoints;
			UClipper2Helper::Offset(OutermostPoints, InnermostPoints,
			                        BorderOffset + StepBorderOffset, 0);
			BorderStepPolys.SetNumZeroed(BorderTessellationTimes);
			for (int32 Step = 0; Step < BorderTessellationTimes; ++Step)
			{
				UClipper2Helper::Offset(
					BorderStepPolys[Step].InnerToOuterPoints,
					Step == 0 ? InnermostPoints : BorderStepPolys[Step - 1].InnerToOuterPoints,
					StepBorderOffset, 0
				);
				int32 ReverseStep = BorderTessellationTimes - Step - 1;
				UClipper2Helper::Offset(
					BorderStepPolys[ReverseStep].OuterToInnerPoints,
					Step == 0 ? OutermostPoints : BorderStepPolys[ReverseStep + 1].OuterToInnerPoints,
					-StepBorderOffset, 0
				);
			}
			TArray<int32> InnermostPointIDs;
			InnermostPointIDs.Empty(InnermostPoints.Num());
			for (const FTriangleIndex& TriangleIndex : Coastline.Indices)
			{
				InnermostPointIDs.Emplace(VertexIndicesMap.FindChecked(TriangleIndex));
			}
			for (int32 Step = 0; Step < BorderTessellationTimes; ++Step)
			{
				FBorderStepTwoWayPoly& BorderStepPoly = BorderStepPolys[Step];
				TArray<FVector2D> UnionPoints;
				UClipper2Helper::Union(UnionPoints, BorderStepPoly.InnerToOuterPoints,
				                       BorderStepPoly.OuterToInnerPoints);
				SubdivisionPolygon(BorderStepPoly.UnionPoints, UnionPoints);
				int32 StepBorderPointNum = BorderStepPoly.UnionPoints.Num();
				BorderStepPoly.UnionPointIDs.Empty(StepBorderPointNum);
				for (int32 Index = 0; Index < StepBorderPointNum; ++Index)
				{
					BorderStepPoly.UnionPointIDs.Emplace(
						Buffers.Vertices.Emplace(FVector(BorderStepPoly.UnionPoints[Index], 0)));
					Buffers.UV0.Emplace(BorderStepPoly.UnionPoints[Index] / MapSize);
				}
				const TArray<FVector2D>& InnerPoints =
					Step == 0
						? InnermostPoints
						: BorderStepPolys[Step - 1].UnionPoints;
				const TArray<int32>& InnerPointIDs =
					Step == 0 ? InnermostPointIDs : BorderStepPolys[Step - 1].UnionPointIDs;
				TArray<FIntVector> SpanTriangles;
				TriangulateRing(
					SpanTriangles,
					BorderStepPoly.UnionPoints, BorderStepPoly.UnionPointIDs,
					InnerPoints, InnerPointIDs
				);
				Buffers.Triangles.Append(SpanTriangles);
			}
		}
	}
	FGeometryScriptIndexList TriangleIndices;
	UGeometryScriptLibrary_MeshBasicEditFunctions::AppendBuffersToMesh(DynamicMesh, Buffers, TriangleIndices);

	UGeometryScriptLibrary_MeshSubdivideFunctions::ApplyPNTessellation(
		DynamicMesh,
		FGeometryScriptPNTessellateOptions(),
		TessellationLevel
	);

	FVector2D VerticesBias = -FVector2D(MapSize / 2);
	DynamicMesh->EditMesh([&](FDynamicMesh3& EditMesh)
	{
		int NumVertices = EditMesh.MaxVertexID();
		for (int Index = 0; Index < NumVertices; ++Index)
		{
			if (!EditMesh.IsVertex(Index))
				continue;
			FVector3d Position = EditMesh.GetVertex(Index);
			FVector2D Point = {Position.X, Position.Y};
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
			}
			else if (MinDistance <= BorderOffset)
			{
				float UnitDepth = (BorderOffset - MinDistance) / BorderOffset;
				UnitDepth = UIslandMapUtils::Remap(UnitDepth, BorderDepthRemapMethod);
				Position.Z += UnitDepth * BorderDepth;
			}
			// Offset mesh to center
			Position.X += VerticesBias.X;
			Position.Y += VerticesBias.Y;
			EditMesh.SetVertex(Index, Position);
		}
	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);

	UGeometryScriptLibrary_MeshNormalsFunctions::SetPerVertexNormals(DynamicMesh);
}

void AIslandDynamicMeshActor::GenerateMeshVoxelization(UDynamicMesh* DynamicMesh)
{
	const TArray<FCoastlinePolygon>& Coastlines = MapData->GetCoastLines();
	FVector2D MapSize = MapData->Mesh->GetSize();
	FVector2D VerticesBias = -FVector2D(MapSize / 2);

	// Build island meshes
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
			Buffers.Vertices.Emplace(FVector(Coastline.Positions[Index] + VerticesBias, 0));
		}
		for (const FPolyTriangle2D& Tri : Coastline.Triangles)
		{
			Buffers.Triangles.Emplace(FIntVector(IndexMap[Tri.V2Index], IndexMap[Tri.V1Index], IndexMap[Tri.V0Index]));
		}

		// Island expand border
		TArray<FVector2D> ExpandPoints;
		UClipper2Helper::Offset(ExpandPoints, Coastline.Positions, BorderOffset, 0);
		int32 ExpandPointNum = ExpandPoints.Num();
		TArray<int32> ExpandPointIds;
		ExpandPointIds.Empty(ExpandPointNum);
		for (int32 Index = 0; Index < ExpandPointNum; ++Index)
		{
			Buffers.Vertices.Emplace(FVector(ExpandPoints[Index] + VerticesBias, -BorderDepth));
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
		TessellationLevel
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
	UGeometryScriptLibrary_MeshNormalsFunctions::SetPerVertexNormals(DynamicMesh);

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
				int32 Elem0 = UVOverlay->AppendElement(
					FVector2f(V0Pos.X / MapSize.X - 0.5f, V0Pos.Y / MapSize.Y - 0.5f));
				int32 Elem1 = UVOverlay->AppendElement(
					FVector2f(V1Pos.X / MapSize.X - 0.5f, V1Pos.Y / MapSize.Y - 0.5f));
				int32 Elem2 = UVOverlay->AppendElement(
					FVector2f(V2Pos.X / MapSize.X - 0.5f, V2Pos.Y / MapSize.Y - 0.5f));
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

void AIslandDynamicMeshActor::GenerateMeshPixel(UDynamicMesh* DynamicMesh)
{
	if (MeshPixelHeight <= 1 || MeshPixelWidth <= 1)
	{
		// UE_LOG(LogMapGen, Error, FString::Printf(TEXT("MeshPixelWidth: %d and MeshPixelHeight: %d cannot be smaller than 1"), MeshPixelWidth, MeshPixelHeight));
		return;
	}
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
				UnitDepth = UIslandMapUtils::Remap(UnitDepth, BorderDepthRemapMethod);
				Position.Z += UnitDepth * BorderDepth;
				EditMesh.SetVertex(Index, Position);
			}
		}
	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);

	UGeometryScriptLibrary_MeshNormalsFunctions::SetPerVertexNormals(DynamicMesh);
}

void AIslandDynamicMeshActor::SetMaterialParameters(UMaterialInstanceDynamic* MaterialInstance)
{
	MaterialInstance->SetTextureParameterValue(FName(TEXT("District ID 01")), DistrictIDTexture01);
	MaterialInstance->SetTextureParameterValue(FName(TEXT("District ID 02")), DistrictIDTexture02);
}

void AIslandDynamicMeshActor::TriangulateRing(TArray<FIntVector>& Triangles,
                                              const TArray<FVector2D>& OuterPoly, const TArray<int32>& OuterPolyIds,
                                              const TArray<FVector2D>& InnerPoly, const TArray<int32>& InnerPolyIds)
{
	const int32 OuterNum = OuterPoly.Num();
	const int32 InnerNum = InnerPoly.Num();
	if (OuterNum != OuterPolyIds.Num() || InnerNum != InnerPolyIds.Num())
	{
		return;
	}
	TriangulateRing(Triangles, OuterPoly, InnerPoly);
	for (FIntVector& Triangle : Triangles)
	{
		Triangle.X = Triangle.X < InnerNum ? InnerPolyIds[Triangle.X] : OuterPolyIds[Triangle.X - InnerNum];
		Triangle.Y = Triangle.Y < InnerNum ? InnerPolyIds[Triangle.Y] : OuterPolyIds[Triangle.Y - InnerNum];
		Triangle.Z = Triangle.Z < InnerNum ? InnerPolyIds[Triangle.Z] : OuterPolyIds[Triangle.Z - InnerNum];
	}
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
		double ClosestInnerPointDistSquared = DBL_MAX;
		int32 ClosestInnerPointIndex = 0;
		for (int32 InnerIndex = 0; InnerIndex < InnerNum; ++InnerIndex)
		{
			double Dist = FVector2D::DistSquared(OuterPos, InnerPoly[InnerIndex]);
			if (Dist < ClosestInnerPointDistSquared)
			{
				ClosestInnerPointIndex = InnerIndex;
				ClosestInnerPointDistSquared = Dist;
			}
		}
		OuterLinkedInner[OuterIndex] = ClosestInnerPointIndex;
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
				NextOuterIndex + InnerNum,
				LinkedInnerIndex,
				NextOuterLinkedInnerIndex
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

void AIslandDynamicMeshActor::SubdivisionPolygon(TArray<FVector2D>& OutResult, const TArray<FVector2D>& Polygon)
{
	double PolygonLength = 0;
	int32 PolygonNum = Polygon.Num();
	TArray<float> PolygonDist;
	PolygonDist.SetNumZeroed(PolygonNum);
	for (int32 Index = 0; Index < PolygonNum; ++Index)
	{
		double Dist = FVector2D::Distance(Polygon[Index], Polygon[(Index + 1) % PolygonNum]);
		PolygonDist[Index] = Dist;
		PolygonLength += Dist;
	}
	double AverageDist = PolygonLength / PolygonNum;
	OutResult.Empty(PolygonNum);
	for (int32 Index = 0; Index < PolygonNum; ++Index)
	{
		OutResult.Emplace(Polygon[Index]);
		float Dist = PolygonDist[Index];
		if (Dist > AverageDist)
		{
			int32 SegmentsNum = FMath::CeilToInt32(Dist / AverageDist);
			FVector2D SegmentVector = (Polygon[(Index + 1) % PolygonNum] - Polygon[Index]) / SegmentsNum;
			for (int32 SegmentIndex = 1; SegmentIndex < SegmentsNum; ++SegmentIndex)
			{
				OutResult.Emplace(OutResult.Last() + SegmentVector);
			}
		}
	}
}
