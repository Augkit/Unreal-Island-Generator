/*
* From http://www.redblobgames.com/maps/mapgen2/
* Original work copyright 2017 Red Blob Games <redblobgames@gmail.com>
* Unreal Engine 4 implementation copyright 2018 Jay Stevens <jaystevens42@gmail.com>
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IslandMap.h"
#include "DualMesh/Public/TriangleDualMesh.h"
#include "IslandMapUtils.h"
#include "Mesh/IslandMeshBuilder.h"
#include "Biomes/IslandBiome.h"
#include "District/IslandDistrict.h"
#include "Elevation/IslandElevation.h"
#include "Moisture/IslandMoisture.h"
#include "Rivers/IslandRivers.h"
#include "Water/IslandWater.h"

#include "IslandMapData.generated.h"

USTRUCT()
struct FIslandQuadTreeNode
{
	GENERATED_BODY()
	FIslandQuadTreeNode()
	{
	}

	FIslandQuadTreeNode(const double& InLeft, const double& InRight, const double& InTop, const double& InBottom,
	                    const int32& InLevel):
		Left(InLeft), Right(InRight), Top(InTop), Bottom(InBottom), Level(InLevel)
	{
		Center = FVector2D(Right - Left, Top - Bottom) * 0.5f;
	}

	FIslandQuadTreeNode(const FVector2D& Min, const FVector2D& Max, const int32 InLevel): Level(InLevel)
	{
		LeftBottom = Min;
		RightBottom = FVector2D(Max.X, Min.Y);
		LeftTop = FVector2D(Min.X, Max.Y);
		RightTop = Max;
		Center = (Min + Max) * 0.5f;
	}

	FIslandQuadTreeNode(const TWeakPtr<FIslandQuadTreeNode>& InParent, const int32 Pos): Parent(InParent),
		PosInParent(Pos)
	{
		const FIslandQuadTreeNode* ParentPtr = InParent.Pin().Get();
		Level = ParentPtr->Level + 1;
		switch (PosInParent)
		{
		case 0: // 0 = left-bottom
			Left = ParentPtr->Left;
			Right = ParentPtr->Center.X;
			Top = ParentPtr->Center.Y;
			Bottom = ParentPtr->Bottom;
			bLeftBottomCovered = ParentPtr->bLeftBottomCovered;
			bRightTopCovered = ParentPtr->bCenterCovered;
			break;
		}
		// switch (PosInParent)
		// {
		// case 0: // 0 = left-bottom
		// 	LeftBottom = ParentPtr->LeftBottom;
		// 	RightBottom = FVector2D(ParentPtr->Center.X, ParentPtr->LeftBottom.Y);
		// 	RightTop = ParentPtr->Center;
		// 	LeftTop = FVector2D(ParentPtr->LeftBottom.X, ParentPtr->Center.Y);
		// 	bLeftBottomCovered = ParentPtr->bLeftBottomCovered;
		// 	bRightTopCovered = ParentPtr->bCenterCovered;
		// 	break;
		// case 1: // 1 = right-bottom
		// 	LeftBottom = FVector2D(ParentPtr->Center.X, ParentPtr->LeftBottom.Y);
		// 	RightBottom = ParentPtr->RightBottom;
		// 	RightTop = FVector2D(ParentPtr->RightBottom.X, ParentPtr->Center.Y);
		// 	LeftTop = ParentPtr->Center;
		// 	bLeftBottomCovered = ParentPtr->bCenterCovered;
		// 	bRightTopCovered = ParentPtr->bRightBottomCovered;
		// 	break;
		// case 2: // 2 = right-top
		// 	LeftBottom = ParentPtr->Center;
		// 	RightBottom = FVector2D(ParentPtr->RightTop.X, ParentPtr->Center.Y);
		// 	RightTop = ParentPtr->RightTop;
		// 	LeftTop = FVector2D(ParentPtr->Center.X, ParentPtr->RightTop.Y);
		// 	bLeftBottomCovered = ParentPtr->bCenterCovered;
		// 	bRightTopCovered = ParentPtr->bRightTopCovered;
		// 	break;
		// case 3: // 3 = left-top
		// 	LeftBottom = FVector2D(ParentPtr->LeftTop.X, ParentPtr->Center.Y);
		// 	RightBottom = ParentPtr->Center;
		// 	RightTop = FVector2D(ParentPtr->Center.X, ParentPtr->RightTop.Y);
		// 	LeftTop = ParentPtr->LeftTop;
		// 	bLeftBottomCovered = ParentPtr->bLeftTopCovered;
		// 	bRightTopCovered = ParentPtr->bCenterCovered;
		// 	break;
		// }
		// Center = (RightTop - LeftBottom) / 2.0f;
	}

	TWeakPtr<FIslandQuadTreeNode> Parent = nullptr;
	int32 Level;
	/** 0 = left-bottom, 1 = right-bottom, 2 = right-top, 3 = left-top */
	int32 PosInParent = 0;

	TSharedPtr<FIslandQuadTreeNode> Children[4]{nullptr, nullptr, nullptr, nullptr};

	double Left = 0.0;
	double Bottom = 0.0;
	double Right = 0.0;
	double Top = 0.0;

	FVector2D LeftBottom;
	FVector2D RightBottom;
	FVector2D RightTop;
	FVector2D LeftTop;
	FVector2D Center;
	bool bLeftBottomCovered = false;
	bool bRightBottomCovered = false;
	bool bRightTopCovered = false;
	bool bLeftTopCovered = false;
	bool bCenterCovered = false;
	bool bCoverCoastline = false;
};

UCLASS(BlueprintType, Blueprintable)
class POLYGONALMAPGENERATOR_API UIslandMapData : public UObject
{
	GENERATED_BODY()
	friend class UIslandMapUtils;
	friend class UIslandCoastline;

#if !UE_BUILD_SHIPPING

private:
	FDateTime LastRegenerationTime;
#endif

protected:
	UPROPERTY()
	TArray<bool> r_water;
	UPROPERTY()
	TArray<bool> r_ocean;
	UPROPERTY()
	TArray<bool> r_coast;
	UPROPERTY()
	TArray<float> r_elevation;
	UPROPERTY()
	TArray<int32> r_waterdistance;
	UPROPERTY()
	TArray<float> r_moisture;
	UPROPERTY()
	TArray<float> r_temperature;
	UPROPERTY()
	TArray<FBiomeData> r_biome;

	UPROPERTY()
	TArray<int32> t_coastdistance;
	UPROPERTY()
	TArray<float> t_elevation;
	UPROPERTY()
	TArray<FSideIndex> t_downslope_s;
	UPROPERTY()
	TArray<int32> s_flow;
	UPROPERTY()
	TArray<FTriangleIndex> spring_t;
	UPROPERTY()
	TArray<FTriangleIndex> river_t;

	// Note -- will be compiled when GetVoronoiPolygons is first called.
	// This will take a long time to compile and use a lot of memory. Use with caution!
	UPROPERTY()
	TArray<FIslandPolygon> VoronoiPolygons;

	UPROPERTY()
	UIslandCoastline* IslandCoastline;

	UPROPERTY()
	TArray<FDistrictRegion> DistrictRegions;

public:
	// The random seed to use for the island.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RNG", meta = (NoSpinbox))
	int32 Seed;
	// Modifies how we calculate drainage.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RNG", meta = (NoSpinbox))
	int32 DrainageSeed;
	// Modifies how we calculate drainage.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RNG", meta = (NoSpinbox))
	int32 RiverSeed;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RNG", meta = (NoSpinbox))
	int32 DistrictSeed;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RNG")
	bool bDetermineRandomSeedAtRuntime;
	// Modifies the types of biomes we produce.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map")
	FBiomeBias BiomeBias;
	// Modifies the shape of the island we generate.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map")
	FIslandShape Shape;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Edges", meta = (ClampMin = "0"))
	int32 NumRivers;
	// How "smooth" the island is.
	// Higher values are more smooth and create fewer bays and lakes.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float Smoothing;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Map")
	float Persistence;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Mesh")
	UTriangleDualMesh* Mesh;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "RNG")
	FRandomStream Rng;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "RNG")
	FRandomStream RiverRng;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "RNG")
	FRandomStream DrainageRng;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "RNG")
	FRandomStream DistrictRng;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map")
	const UIslandMeshBuilder* PointGenerator;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map")
	const UIslandBiome* Biomes;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map")
	const UIslandElevation* Elevation;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map")
	const UIslandMoisture* Moisture;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map")
	const UIslandRivers* Rivers;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map")
	const UIslandWater* Water;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map")
	UIslandDistrict* District;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Map")
	TArray<URiver*> CreatedRivers;

	TSharedPtr<FIslandQuadTreeNode> QuadTreeRoot;
	TArray<TSharedPtr<FIslandQuadTreeNode>> QuadAreas;

	UPROPERTY(BlueprintAssignable)
	FOnIslandGenerationComplete OnIslandPointGenerationComplete;
	UPROPERTY(BlueprintAssignable)
	FOnIslandGenerationComplete OnIslandWaterGenerationComplete;
	UPROPERTY(BlueprintAssignable)
	FOnIslandGenerationComplete OnIslandElevationGenerationComplete;
	UPROPERTY(BlueprintAssignable)
	FOnIslandGenerationComplete OnIslandRiverGenerationComplete;
	UPROPERTY(BlueprintAssignable)
	FOnIslandGenerationComplete OnIslandMoistureGenerationComplete;
	UPROPERTY(BlueprintAssignable)
	FOnIslandGenerationComplete OnIslandBiomeGenerationComplete;
	UPROPERTY(BlueprintAssignable)
	FOnIslandGenerationComplete OnIslandGenerationComplete;

public:
	UIslandMapData();

protected:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Procedural Generation|Island Generation")
	void OnPointGenerationComplete();
	virtual void OnPointGenerationComplete_Implementation();
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Procedural Generation|Island Generation")
	void OnWaterGenerationComplete();
	virtual void OnWaterGenerationComplete_Implementation();
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Procedural Generation|Island Generation")
	void OnElevationGenerationComplete();
	virtual void OnElevationGenerationComplete_Implementation();
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Procedural Generation|Island Generation")
	void OnRiverGenerationComplete();
	virtual void OnRiverGenerationComplete_Implementation();
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Procedural Generation|Island Generation")
	void OnMoistureGenerationComplete();
	virtual void OnMoistureGenerationComplete_Implementation();
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Procedural Generation|Island Generation")
	void OnBiomeGenerationComplete();
	virtual void OnBiomeGenerationComplete_Implementation();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Procedural Generation|Island Generation")
	void OnIslandGenComplete();
	virtual void OnIslandGenComplete_Implementation();

public:
	// Creates the island using all the current parameters.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Procedural Generation|Island Generation")
	void GenerateIsland();
	virtual void GenerateIsland_Implementation();

	UFUNCTION(BlueprintCallable, BlueprintPure)
	FVector2D GetMapSize() const;

	// WARNING: This will take a long time to compile and will use a lot of memory.
	// Use with caution!
	UFUNCTION()
	TArray<FIslandPolygon>& GetVoronoiPolygons();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Water")
	TArray<bool>& GetWaterRegions();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Water")
	bool IsPointWater(FPointIndex Region) const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Ocean")
	TArray<bool>& GetOceanRegions();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Ocean")
	bool IsPointOcean(FPointIndex Region) const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Ocean")
	TArray<bool>& GetCoastalRegions();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Ocean")
	bool IsPointCoast(FPointIndex Region) const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Elevation")
	TArray<float>& GetRegionElevations();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Elevation")
	float GetPointElevation(FPointIndex Region) const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Moisture")
	TArray<int32>& GetRegionWaterDistance();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Moisture")
	int32 GetPointWaterDistance(FPointIndex Region) const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Moisture")
	TArray<float>& GetRegionMoisture();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Moisture")
	float GetPointMoisture(FPointIndex Region) const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Temperature")
	TArray<float>& GetRegionTemperature();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Temperature")
	float GetPointTemperature(FPointIndex Region) const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Biomes")
	TArray<FBiomeData>& GetRegionBiomes();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Moisture")
	FBiomeData GetPointBiome(FPointIndex Region) const;

	const TArray<FDistrictRegion>& GetDistrictRegions() const;

	const TArray<TSharedPtr<FIslandQuadTreeNode>>& GetQuadIslandAreas() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Ocean")
	TArray<int32>& GetTriangleCoastDistances();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Ocean")
	int32 GetTriangleCoastDistance(FTriangleIndex Triangle) const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Elevation")
	TArray<float>& GetTriangleElevations();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Elevation")
	float GetTriangleElevation(FTriangleIndex Triangle) const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Rivers")
	TArray<FSideIndex>& GetTriangleDownslopes();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Rivers")
	TArray<int32>& GetSideFlow();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Rivers")
	TArray<FTriangleIndex>& GetSpringTriangles();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Rivers")
	bool IsTriangleSpring(FTriangleIndex Triangle) const;
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Rivers")
	TArray<FTriangleIndex>& GetRiverTriangles();
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Procedural Generation|Island Generation|Rivers")
	bool IsTriangleRiver(FTriangleIndex Triangle) const;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	const TArray<FCoastlinePolygon>& GetCoastLines() const;

	UFUNCTION(BlueprintCallable)
	void GenerateIslandQuadTree();
};
