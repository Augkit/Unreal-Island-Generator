// Fill out your copyright notice in the Description page of Project Settings.


#include "DynamicMesh/IslandDynamicMeshActorBase.h"

struct FCoastlinePolygon;

void AIslandDynamicMeshActorBase::SetMapData(UIslandMapData* InMapData)
{
	MapData = InMapData;
}

UIslandMapData* AIslandDynamicMeshActorBase::GetMapData()
{
	return MapData;
}

void AIslandDynamicMeshActorBase::GenerateIsland(UIslandMapData* InMapData)
{
	if (InMapData != nullptr)
		SetMapData(InMapData);
	if (MapData == nullptr)
		return;

	{
		SCOPE_CYCLE_COUNTER(STAT_GenerateIslandTexture)
		GenerateIslandTexture();
	}
	{
		SCOPE_CYCLE_COUNTER(STAT_GenerateDynamicMesh)
		GenerateIslandMesh();
	}
	if (!IsValid(IslandMaterial))
	{
		return;
	}
	UMaterialInstanceDynamic* MaterialInstance = UMaterialInstanceDynamic::Create(IslandMaterial, this);
	SetMaterialParameters(MaterialInstance);
	GetDynamicMeshComponent()->SetMaterial(0, MaterialInstance);
}

void AIslandDynamicMeshActorBase::GenerateIslandMesh()
{
	// Empty
}

void AIslandDynamicMeshActorBase::GenerateIslandTexture()
{
	// Empty
}

void AIslandDynamicMeshActorBase::SetMaterialParameters(UMaterialInstanceDynamic* MaterialInstance)
{
	// Empty
}
