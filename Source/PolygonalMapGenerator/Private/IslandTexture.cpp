// Fill out your copyright notice in the Description page of Project Settings.


#include "IslandTexture.h"
#include "canvas_ity.hpp"

void UIslandTexture::DrawDistrictIDTexture(UTexture2D*& IDTexture01, UTexture2D*& IDTexture02, int32 Width,
                                           int32 Height)
{
	DrawDistrictIDTexture_Internal(Width, Height);
	IDTexture01 = DistrictIDTexture01;
	IDTexture02 = DistrictIDTexture02;
}

void UIslandTexture::DrawDistrictIDTexture_Internal(int32 Width, int32 Height)
{
		if (!MapData->IsValidLowLevel())
	{
		return;
	}
	const FVector2D Scale = FVector2D(Width, Height) / MapData->GetMapSize();
	canvas_ity::canvas_20 Canvas(Width, Height);
	for (const FDistrictRegion& DistrictRegion : MapData->GetDistrictRegions())
	{
		Canvas.set_line_width(0.133333f);
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

	int32 FloatImageBufferLength = Width * Height * 4;
	TArray<FFloat16> FloatIDImageBuffer1;
	FloatIDImageBuffer1.Empty(FloatImageBufferLength);
	TArray<FFloat16> FloatIDImageBuffer2;
	FloatIDImageBuffer2.Empty(FloatImageBufferLength);

	for (int32 Row = 0; Row < Height; ++Row)
	{
		for (int32 Col = 0; Col < Width; ++Col)
		{
			const canvas_ity::rgba_20& ColorData = *(Bitmap + static_cast<int>(Row * Width + Col));
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
		DistrictIDTexture01 = UTexture2D::CreateTransient(Width, Height, EPixelFormat::PF_FloatRGBA);
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
		DistrictIDTexture02 = UTexture2D::CreateTransient(Width, Height, EPixelFormat::PF_FloatRGBA);
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
