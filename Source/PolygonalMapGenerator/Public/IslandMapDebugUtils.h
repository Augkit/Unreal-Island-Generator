// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "IslandMapData.h"
#include "IslandMapDebugUtils.generated.h"

UCLASS()
class POLYGONALMAPGENERATOR_API UIslandMapDebugUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Island Overview|Debug")
	static void DrawWater(UCanvasRenderTarget2D* RenderTarget2D, const UIslandMapData* MapData);

	UFUNCTION(BlueprintCallable, Category = "Island Overview|Debug")
	static void DrawRegion(UCanvasRenderTarget2D* RenderTarget2D, const UIslandMapData* MapData);

	UFUNCTION(BlueprintCallable, Category = "Island Overview|Debug")
	static void DrawCoastline(UCanvasRenderTarget2D* RenderTarget2D, const UIslandMapData* MapData);

	UFUNCTION(BlueprintCallable, Category = "Island Overview|Debug")
	static void DrawTriangulationIsland(UCanvasRenderTarget2D* RenderTarget2D, const UIslandMapData* MapData);

	UFUNCTION(BlueprintCallable, Category = "Island Overview|Debug")
	static void DrawDistrict(UCanvasRenderTarget2D* RenderTarget2D, const UIslandMapData* MapData);
};
