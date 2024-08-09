#include "IslandDynamicAssets.h"

#include "Coastline/IslandCoastline.h"
#include "canvas_ity.hpp"
#include "GeometryScript/MeshBasicEditFunctions.h"

void UIslandDynamicAssets::AsyncGenerateAssets()
{
	GenerateMapDataTask = FFunctionGraphTask::CreateAndDispatchWhenReady([this]
	{
		if (MapData)
			MapData->GenerateIsland();
	});

	FGraphEventArray GenDistrictTexPrerequisites;
	GenDistrictTexPrerequisites.Emplace(GenerateMapDataTask);
	GenDistrictIDTextureTask = AsyncGenerateDistrictIDTexture(GenDistrictTexPrerequisites);

	const int32 TileAmount = GetTileAmount();
	TileInfo.Empty(TileAmount);
	FGraphEventArray CalcTilePrerequisites;
	CalcTilePrerequisites.Emplace(GenerateMapDataTask);
	for (int32 Index = 0; Index < TileAmount; Index++)
	{
		FGraphEventRef CalcTileMeshBuffersTask = FFunctionGraphTask::CreateAndDispatchWhenReady(
			[&, Index] { CalcTileMeshBuffer(Index); }, TStatId(), &CalcTilePrerequisites);
		TileInfo.Emplace(CalcTileMeshBuffersTask);
	}
}

FGraphEventRef UIslandDynamicAssets::AsyncGenerateDistrictIDTexture(const FGraphEventArray& Prerequisites)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AIslandDynamicMeshActor::GenerateDistrictIDTexture)
	FGraphEventRef GenTextureDataTask = FFunctionGraphTask::CreateAndDispatchWhenReady([this]
	{
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

		// TODO: This part would spend 5s, using task graph to optimization
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
		}
	}, TStatId(), &Prerequisites);
	FGraphEventArray UpdateResourcePrerequisites;
	UpdateResourcePrerequisites.Emplace(GenTextureDataTask);
	return FFunctionGraphTask::CreateAndDispatchWhenReady([this]
	{
		DistrictIDTexture01->UpdateResource();
		DistrictIDTexture02->UpdateResource();
	}, TStatId(), &UpdateResourcePrerequisites, ENamedThreads::GameThread);
}

void UIslandDynamicAssets::CalcTileMeshBuffer(const int32 TileIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AIslandDynamicTileMeshActor::CalcTileMeshBuffer);
	FDynamicTileInfo& Info = TileInfo[TileIndex];
	FGeometryScriptSimpleMeshBuffers& Buffers = Info.Buffers;
	Info.TileRow = TileIndex / (TileDivisions + 1);
	Info.TileCol = TileIndex % (TileDivisions + 1);
	FVector2D MapSize = MapData->GetMapSize();
	FVector2D TileSize = MapSize / (TileDivisions + 1);
	FVector2D BoundaryMin(Info.TileCol * TileSize.X, Info.TileRow * TileSize.Y);
	Info.TileCenter = BoundaryMin + TileSize / 2;
	FVector2D SubgridSize = TileSize / TileResolution;
	int32 VerticesNum = (TileResolution + 1) * (TileResolution + 1);
	Buffers.Vertices.SetNumUninitialized(VerticesNum);
	double MaxUnitDepth = 0.;
	double MinUnitDepth = TNumericLimits<double>::Max();
	for (int32 VIndex = 0; VIndex < VerticesNum; VIndex++)
	{
		FVector2D RelativeLocation(VIndex / (TileResolution + 1) * SubgridSize.X, VIndex % (TileResolution + 1) * SubgridSize.Y);
		FVector2D AbsoluteLocation = BoundaryMin + RelativeLocation;
		double UnitDepth = 0.;
		double MinDistance = TNumericLimits<double>::Max();
		bool bPointInPolygon2D = false;
		for (const FCoastlinePolygon& CoastLine : MapData->GetCoastLines())
		{
			bPointInPolygon2D = UIslandMapUtils::PointInPolygon2D(AbsoluteLocation, CoastLine.Positions);
			if (bPointInPolygon2D)
			{
				break;
			}
			MinDistance = FMath::Min(MinDistance, UIslandMapUtils::DistanceToPolygon2D(AbsoluteLocation, CoastLine.Positions));
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
		Buffers.Vertices[VIndex].X = AbsoluteLocation.X;
		Buffers.Vertices[VIndex].Y = AbsoluteLocation.Y;
	}
	if (FMath::IsNearlyEqual(MaxUnitDepth, MinUnitDepth))
	{
		// If the Tile has no height differences, the grid uses quadrilaterals without subdivisions.
		TArray<FVector> FourCorners;
		FourCorners.Emplace(Buffers.Vertices[0]);
		FourCorners.Emplace(Buffers.Vertices[TileResolution]);
		FourCorners.Emplace(Buffers.Vertices[TileResolution * (TileResolution + 1)]);
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
		int32 TileResolutionSquared = TileResolution * TileResolution;
		int32 TrianglesNum = TileResolutionSquared * 2;
		Buffers.Triangles.SetNum(TrianglesNum);
		for (int32 GIndex = 0; GIndex < TileResolutionSquared; GIndex++)
		{
			int32 Row = GIndex / TileResolution;
			int32 Col = GIndex % TileResolution;
			int32 TriAIndex = GIndex * 2;
			int32 TriBIndex = TriAIndex + 1;
			Buffers.Triangles[TriAIndex].X = (TileResolution + 1) * Row + Col;
			Buffers.Triangles[TriAIndex].Y = Buffers.Triangles[TriAIndex].X + 1;
			Buffers.Triangles[TriAIndex].Z = (TileResolution + 1) * (Row + 1) + Col;
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
		Buffers.Vertices[VIndex].X -= Info.TileCenter.X;
		Buffers.Vertices[VIndex].Y -= Info.TileCenter.Y;
		Buffers.Vertices[VIndex].Z = (BorderDepthRemapCurve
			                              ? BorderDepthRemapCurve->GetFloatValue(Buffers.Vertices[VIndex].Z)
			                              : Buffers.Vertices[VIndex].Z - 1) * BorderDepth;
	}
}

int32 UIslandDynamicAssets::GetTileAmount() const
{
	return (TileDivisions + 1) * (TileDivisions + 1);
}