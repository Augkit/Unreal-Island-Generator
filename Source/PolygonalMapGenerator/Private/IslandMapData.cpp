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

#include "IslandMapData.h"
#include "DualMeshBuilder.h"
#include "DelaunayHelper.h"
#include "IslandMapUtils.h"
#include "PolyPartitionHelper.h"
#include "TimerManager.h"
#include "Coastline/IslandCoastline.h"
#include "Engine/Canvas.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "RandomSampling/PoissonDiscUtilities.h"

// Sets default values
UIslandMapData::UIslandMapData()
{
	bDetermineRandomSeedAtRuntime = false;
	Seed = 0;
	DrainageSeed = 1;
	RiverSeed = 2;
	NumRivers = 30;

#if !UE_BUILD_SHIPPING
	LastRegenerationTime = FDateTime::MinValue();
#endif

	OnIslandPointGenerationComplete.AddDynamic(this, &UIslandMapData::OnPointGenerationComplete);
	OnIslandWaterGenerationComplete.AddDynamic(this, &UIslandMapData::OnWaterGenerationComplete);
	OnIslandElevationGenerationComplete.AddDynamic(this, &UIslandMapData::OnElevationGenerationComplete);
	OnIslandRiverGenerationComplete.AddDynamic(this, &UIslandMapData::OnRiverGenerationComplete);
	OnIslandMoistureGenerationComplete.AddDynamic(this, &UIslandMapData::OnMoistureGenerationComplete);
	OnIslandBiomeGenerationComplete.AddDynamic(this, &UIslandMapData::OnBiomeGenerationComplete);
	OnIslandGenerationComplete.AddDynamic(this, &UIslandMapData::OnIslandGenComplete);
}

/*void UIslandMapData::OnConstruction(const FTransform& NewTransform)
{
	Super::OnConstruction(NewTransform);

#if !UE_BUILD_SHIPPING
	FDateTime now = FDateTime::UtcNow();
	FTimespan lengthBetweenRegeneration = now - LastRegenerationTime;
	if (lengthBetweenRegeneration.GetTotalSeconds() < 1 || Rng.GetInitialSeed() == Seed && RiverRng.GetInitialSeed() == RiverSeed && DrainageRng.GetInitialSeed() == DrainageSeed)
	{
		OnIslandGenComplete();
		return;
	}
#endif

	GenerateIsland();
}*/

void UIslandMapData::OnPointGenerationComplete_Implementation()
{
	// Do nothing by default
}

void UIslandMapData::OnWaterGenerationComplete_Implementation()
{
	// Do nothing by default
}

void UIslandMapData::OnElevationGenerationComplete_Implementation()
{
	// Do nothing by default
}

void UIslandMapData::OnRiverGenerationComplete_Implementation()
{
	// Do nothing by default
}

void UIslandMapData::OnMoistureGenerationComplete_Implementation()
{
	// Do nothing by default
}

void UIslandMapData::OnBiomeGenerationComplete_Implementation()
{
	// Do nothing by default
}

void UIslandMapData::OnIslandGenComplete_Implementation()
{
	// Do nothing by default
}

void UIslandMapData::GenerateIsland_Implementation()
{
	if (PointGenerator == nullptr || Water == nullptr || Elevation == nullptr || Rivers == nullptr
		|| Moisture == nullptr || Biomes == nullptr || District == nullptr)
	{
		UE_LOG(LogMapGen, Error, TEXT("IslandMap not properly set up!"));
		return;
	}
	FDateTime startTime;
#if !UE_BUILD_SHIPPING
	startTime = FDateTime::UtcNow();
	LastRegenerationTime = startTime;
#endif

	if (bDetermineRandomSeedAtRuntime)
	{
		startTime = FDateTime::UtcNow();
		int multiplier = startTime.GetSecond() % 2 == 0 ? 1 : -1;
		Seed = ((startTime.GetMillisecond() * startTime.GetMinute()) + (startTime.GetHour() * startTime.GetDayOfYear()))
			* multiplier;
	}

	Rng = FRandomStream();
	Rng.Initialize(Seed);
	if (bDetermineRandomSeedAtRuntime)
	{
		RiverSeed = Rng.RandRange(INT32_MIN, INT32_MAX);
		DrainageSeed = Rng.RandRange(INT32_MIN, INT32_MAX);
		DistrictRng = Rng.RandRange(INT32_MIN, INT32_MAX);
	}
	RiverRng = FRandomStream();
	RiverRng.Initialize(RiverSeed);
	DrainageRng = FRandomStream();
	DrainageRng.Initialize(DrainageSeed);
	DistrictRng = FRandomStream();
	DistrictRng.Initialize(DistrictSeed);

	Persistence = FMath::Pow(0.5f, 1.0 + Smoothing);
	Shape.Amplitudes.SetNum(Shape.Octaves);
	for (int i = 0; i < Shape.Amplitudes.Num(); i++)
	{
		Shape.Amplitudes[i] = FMath::Pow(Persistence, i);
	}

#if !UE_BUILD_SHIPPING
	FDateTime finishedTime = FDateTime::UtcNow();
	FTimespan difference = finishedTime - startTime;
	startTime = finishedTime;
	UE_LOG(LogMapGen, Log, TEXT("Initialization took %f seconds."), difference.GetTotalSeconds());
#endif

	// Generate map points
	Mesh = PointGenerator->GenerateDualMesh(Rng);
	OnIslandPointGenerationComplete.Broadcast();

#if !UE_BUILD_SHIPPING
	finishedTime = FDateTime::UtcNow();
	difference = finishedTime - startTime;
	startTime = finishedTime;
	UE_LOG(LogMapGen, Log, TEXT("Generating points took %f seconds."), difference.GetTotalSeconds());
#endif

	// Reset all arrays
	CreatedRivers.Empty(NumRivers);
	spring_t.Empty();
	river_t.Empty(NumRivers);

	r_water.Empty(Mesh->NumRegions);
	r_water.SetNumZeroed(Mesh->NumRegions);
	r_ocean.Empty(Mesh->NumRegions);
	r_ocean.SetNumZeroed(Mesh->NumRegions);

	t_elevation.Empty(Mesh->NumTriangles);
	t_elevation.SetNumZeroed(Mesh->NumTriangles);
	t_downslope_s.Empty(Mesh->NumTriangles);
	t_downslope_s.SetNum(Mesh->NumTriangles);
	t_coastdistance.Empty(Mesh->NumTriangles);
	t_coastdistance.SetNumZeroed(Mesh->NumTriangles);
	r_elevation.Empty(Mesh->NumRegions);
	r_elevation.SetNumZeroed(Mesh->NumRegions);

	s_flow.Empty(Mesh->NumSides);
	s_flow.SetNumZeroed(Mesh->NumSides);

	r_moisture.Empty(Mesh->NumRegions);
	r_moisture.SetNumZeroed(Mesh->NumRegions);
	r_waterdistance.Empty(Mesh->NumRegions);
	r_waterdistance.SetNumZeroed(Mesh->NumRegions);

	r_coast.Empty(Mesh->NumRegions);
	r_coast.SetNumZeroed(Mesh->NumRegions);
	r_temperature.Empty(Mesh->NumRegions);
	r_temperature.SetNumZeroed(Mesh->NumRegions);
	r_biome.Empty(Mesh->NumRegions);
	r_biome.SetNumZeroed(Mesh->NumRegions);

#if !UE_BUILD_SHIPPING
	finishedTime = FDateTime::UtcNow();
	difference = finishedTime - startTime;
	startTime = finishedTime;
	UE_LOG(LogMapGen, Log, TEXT("Resetting arrays took %f seconds."), difference.GetTotalSeconds());
#endif

	// Water
	Water->assign_r_water(r_water, Rng, Mesh, Shape);
	Water->assign_r_ocean(r_ocean, Mesh, r_water);
	OnIslandWaterGenerationComplete.Broadcast();

#if !UE_BUILD_SHIPPING
	finishedTime = FDateTime::UtcNow();
	difference = finishedTime - startTime;
	startTime = finishedTime;
	UE_LOG(LogMapGen, Log, TEXT("Generated map water in %f seconds."), difference.GetTotalSeconds());
#endif

	// Elevation
	Elevation->assign_t_elevation(t_elevation, t_coastdistance, t_downslope_s, Mesh, r_ocean, r_water, DrainageRng);
	Elevation->redistribute_t_elevation(t_elevation, Mesh, r_ocean);
	Elevation->assign_r_elevation(r_elevation, Mesh, t_elevation, r_ocean);
	OnIslandElevationGenerationComplete.Broadcast();

#if !UE_BUILD_SHIPPING
	finishedTime = FDateTime::UtcNow();
	difference = finishedTime - startTime;
	startTime = finishedTime;
	UE_LOG(LogMapGen, Log, TEXT("Generated map elevation in %f seconds."), difference.GetTotalSeconds());
#endif

	// Rivers
	spring_t = Rivers->find_spring_t(Mesh, r_water, t_elevation, t_downslope_s);
	UIslandMapUtils::RandomShuffle(spring_t, RiverRng);
	river_t.SetNum(NumRivers < spring_t.Num() ? NumRivers : spring_t.Num());
	for (int i = 0; i < river_t.Num(); i++)
	{
		river_t[i] = spring_t[i];
	}
	Rivers->assign_s_flow(s_flow, CreatedRivers, Mesh, t_downslope_s, river_t, RiverRng);
	OnIslandRiverGenerationComplete.Broadcast();

#if !UE_BUILD_SHIPPING
	finishedTime = FDateTime::UtcNow();
	difference = finishedTime - startTime;
	startTime = finishedTime;
	UE_LOG(LogMapGen, Log, TEXT("Generated %d map rivers in %f seconds."), CreatedRivers.Num(),
	       difference.GetTotalSeconds());
#endif

	// Moisture
	Moisture->assign_r_moisture(r_moisture, r_waterdistance, Mesh, r_water,
	                            Moisture->find_moisture_seeds_r(Mesh, s_flow, r_ocean, r_water));
	Moisture->redistribute_r_moisture(r_moisture, Mesh, r_water, BiomeBias.Rainfall, 1.0f + BiomeBias.Rainfall);
	OnIslandMoistureGenerationComplete.Broadcast();

#if !UE_BUILD_SHIPPING
	finishedTime = FDateTime::UtcNow();
	difference = finishedTime - startTime;
	startTime = finishedTime;
	UE_LOG(LogMapGen, Log, TEXT("Generated map moisture in %f seconds."), difference.GetTotalSeconds());
#endif

	// Biomes
	Biomes->assign_r_coast(r_coast, Mesh, r_ocean);
	Biomes->assign_r_temperature(r_temperature, Mesh, r_ocean, r_water, r_elevation, r_moisture,
	                             BiomeBias.NorthernTemperature, BiomeBias.SouthernTemperature);
	Biomes->assign_r_biome(r_biome, Mesh, r_ocean, r_water, r_coast, r_temperature, r_moisture);
	OnIslandBiomeGenerationComplete.Broadcast();

#if !UE_BUILD_SHIPPING
	finishedTime = FDateTime::UtcNow();
	difference = finishedTime - startTime;
	startTime = finishedTime;
	UE_LOG(LogMapGen, Log, TEXT("Generated map biomes in %f seconds."), difference.GetTotalSeconds());
	FTimespan completedTime = finishedTime - LastRegenerationTime;
	UE_LOG(LogMapGen, Log, TEXT("Total map generation time: %f seconds."), completedTime.GetTotalSeconds());
#endif
	District->AssignDistrict(DistrictRegions, Mesh, r_ocean, Rng);

	IslandCoastline = NewObject<UIslandCoastline>();
	IslandCoastline->Initialize(Mesh, r_ocean, r_coast);

	// Do whatever we need to do when the island generation is done
	OnIslandGenerationComplete.Broadcast();
}

FVector2D UIslandMapData::GetMapSize() const
{
	if(Mesh == nullptr)
		return FVector2D::ZeroVector;
	return Mesh->GetSize();
}

TArray<FIslandPolygon>& UIslandMapData::GetVoronoiPolygons()
{
	if (VoronoiPolygons.Num() == 0)
	{
		VoronoiPolygons.SetNumZeroed(Mesh->NumSolidRegions);
		for (FPointIndex r = 0; r < Mesh->NumSolidRegions; r++)
		{
			VoronoiPolygons[r].Biome = r_biome[r];
			VoronoiPolygons[r].Vertices = Mesh->r_circulate_t(r);
			for (FTriangleIndex t : VoronoiPolygons[r].Vertices)
			{
				if (!t.IsValid())
				{
					continue;
				}
				FVector2D point2D = Mesh->t_pos(t);
				float z = t_elevation.IsValidIndex(t) ? t_elevation[t] : -1000.0f;
				VoronoiPolygons[r].VertexPoints.Add(FVector(point2D.X, point2D.Y, z * 10000));
			}
		}
	}
	return VoronoiPolygons;
}

TArray<bool>& UIslandMapData::GetWaterRegions()
{
	return r_water;
}

bool UIslandMapData::IsPointWater(FPointIndex Region) const
{
	if (r_water.IsValidIndex(Region))
	{
		return r_water[Region];
	}
	else
	{
		return false;
	}
}

TArray<bool>& UIslandMapData::GetOceanRegions()
{
	return r_ocean;
}

bool UIslandMapData::IsPointOcean(FPointIndex Region) const
{
	if (r_ocean.IsValidIndex(Region))
	{
		return r_ocean[Region];
	}
	else
	{
		return false;
	}
}

TArray<bool>& UIslandMapData::GetCoastalRegions()
{
	return r_coast;
}

bool UIslandMapData::IsPointCoast(FPointIndex Region) const
{
	if (r_coast.IsValidIndex(Region))
	{
		return r_coast[Region];
	}
	else
	{
		return false;
	}
}

TArray<float>& UIslandMapData::GetRegionElevations()
{
	return r_elevation;
}

float UIslandMapData::GetPointElevation(FPointIndex Region) const
{
	if (r_elevation.IsValidIndex(Region))
	{
		return r_elevation[Region];
	}
	else
	{
		return -1.0f;
	}
}

TArray<int32>& UIslandMapData::GetRegionWaterDistance()
{
	return r_waterdistance;
}

int32 UIslandMapData::GetPointWaterDistance(FPointIndex Region) const
{
	if (r_waterdistance.IsValidIndex(Region))
	{
		return r_waterdistance[Region];
	}
	else
	{
		return -1;
	}
}

TArray<float>& UIslandMapData::GetRegionMoisture()
{
	return r_moisture;
}

float UIslandMapData::GetPointMoisture(FPointIndex Region) const
{
	if (r_moisture.IsValidIndex(Region))
	{
		return r_moisture[Region];
	}
	else
	{
		return -1.0f;
	}
}

TArray<float>& UIslandMapData::GetRegionTemperature()
{
	return r_temperature;
}

float UIslandMapData::GetPointTemperature(FPointIndex Region) const
{
	if (r_temperature.IsValidIndex(Region))
	{
		return r_temperature[Region];
	}
	else
	{
		return -1.0f;
	}
}

TArray<FBiomeData>& UIslandMapData::GetRegionBiomes()
{
	return r_biome;
}

FBiomeData UIslandMapData::GetPointBiome(FPointIndex Region) const
{
	if (r_biome.IsValidIndex(Region))
	{
		return r_biome[Region];
	}
	else
	{
		return FBiomeData();
	}
}

const TArray<FDistrictRegion>& UIslandMapData::GetDistrictRegions() const
{
	return DistrictRegions;
}

TArray<int32>& UIslandMapData::GetTriangleCoastDistances()
{
	return t_coastdistance;
}

int32 UIslandMapData::GetTriangleCoastDistance(FTriangleIndex Triangle) const
{
	if (t_coastdistance.IsValidIndex(Triangle))
	{
		return t_coastdistance[Triangle];
	}
	else
	{
		return -1;
	}
}

TArray<float>& UIslandMapData::GetTriangleElevations()
{
	return t_elevation;
}

float UIslandMapData::GetTriangleElevation(FTriangleIndex Triangle) const
{
	if (t_elevation.IsValidIndex(Triangle))
	{
		return t_elevation[Triangle];
	}
	else
	{
		return -1.0f;
	}
}

TArray<FSideIndex>& UIslandMapData::GetTriangleDownslopes()
{
	return t_downslope_s;
}

TArray<int32>& UIslandMapData::GetSideFlow()
{
	return s_flow;
}

TArray<FTriangleIndex>& UIslandMapData::GetSpringTriangles()
{
	return spring_t;
}

bool UIslandMapData::IsTriangleSpring(FTriangleIndex Triangle) const
{
	return spring_t.Contains(Triangle);
}

TArray<FTriangleIndex>& UIslandMapData::GetRiverTriangles()
{
	return river_t;
}

bool UIslandMapData::IsTriangleRiver(FTriangleIndex Triangle) const
{
	return river_t.Contains(Triangle);
}

const TArray<FCoastlinePolygon>& UIslandMapData::GetCoastLines() const
{
	return IslandCoastline->GetCoastlines();
}
